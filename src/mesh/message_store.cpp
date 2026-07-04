// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Ben

#include "message_store.h"

#include <cstdlib>
#include <cstring>

#if defined(ESP32_PLATFORM)
#include <SPIFFS.h>
#include "hal/storage.h"
#else
#include <cstdio>
#endif

namespace sigurdos {
namespace mesh {

namespace {

#if defined(ESP32_PLATFORM)
static constexpr const char* STORE_PATH = "/companion_msgs";
static constexpr const char* STORE_TMP_PATH = "/companion_msgs.tmp";
static constexpr const char* STORE_BAK_PATH = "/companion_msgs.bak";
#endif

static constexpr uint32_t MESSAGE_STORE_MAX_RECORDS = 64;

#if !defined(ESP32_PLATFORM)
static char g_native_path[160] = "/tmp/sigurdos_companion_msgs.bin";
static bool g_native_fail_next_replace_rename = false;

static void copyZ(char* dest, size_t dest_sz, const char* src)
{
    if (!dest || dest_sz == 0) return;
    if (!src) src = "";
    std::strncpy(dest, src, dest_sz - 1);
    dest[dest_sz - 1] = '\0';
}

static bool siblingPath(char* out, size_t out_sz, const char* suffix)
{
    if (!out || out_sz == 0 || !suffix) return false;
    int n = std::snprintf(out, out_sz, "%s%s", g_native_path, suffix);
    return n > 0 && (size_t)n < out_sz;
}
#endif

static uint8_t flagsFor(const StoredMessage& msg)
{
    uint8_t flags = 0;
    if (msg.is_self) flags |= 0x01;
    if (msg.is_channel) flags |= 0x02;
    if (msg.acked) flags |= 0x04;
    if (msg.companion_sent) flags |= 0x08;
    return flags;
}

static void applyFlags(StoredMessage& msg, uint8_t flags)
{
    msg.is_self = (flags & 0x01) != 0;
    msg.is_channel = (flags & 0x02) != 0;
    msg.acked = (flags & 0x04) != 0;
    msg.companion_sent = (flags & 0x08) != 0;
}

static bool readHeader(uint32_t* out_count);
static bool writeHeaderIfNeeded();

#if defined(ESP32_PLATFORM)
static bool ensureFs()
{
    if (!sigurdos::storage_available()) return false;
    static bool mounted = false;
    if (!mounted) {
        if (!SPIFFS.begin(false)) return false;
        mounted = true;
    }
    return true;
}

static bool pathExists(const char* path)
{
    return path && SPIFFS.exists(path);
}

static bool removePath(const char* path)
{
    if (!path || !SPIFFS.exists(path)) return true;
    return SPIFFS.remove(path);
}

static bool renamePath(const char* from, const char* to)
{
    return from && to && SPIFFS.rename(from, to);
}

static bool existsStore()
{
    return pathExists(STORE_PATH);
}

static bool removeStore()
{
    return removePath(STORE_PATH);
}
#else
static bool ensureFs()
{
    return true;
}

static bool pathExists(const char* path)
{
    if (!path) return false;
    FILE* f = std::fopen(path, "rb");
    if (!f) return false;
    std::fclose(f);
    return true;
}

static bool removePath(const char* path)
{
    return path && (std::remove(path) == 0 || !pathExists(path));
}

static bool renamePath(const char* from, const char* to)
{
    return from && to && std::rename(from, to) == 0;
}

static bool existsStore()
{
    return pathExists(g_native_path);
}

static bool removeStore()
{
    return removePath(g_native_path);
}
#endif

static bool readRecordRaw(StoredMessage& msg, const uint8_t* rec, size_t len)
{
    if (!rec || len < detail::MESSAGE_STORE_RECORD_SIZE) return false;

    size_t pos = 0;
    std::memset(&msg, 0, sizeof(msg));
    std::memcpy(&msg.store_id, rec + pos, 4); pos += 4;

    std::memcpy(msg.conversation, rec + pos, SIGURDOS_MSG_CONVERSATION_LEN);
    msg.conversation[SIGURDOS_MSG_CONVERSATION_LEN - 1] = '\0';
    pos += SIGURDOS_MSG_CONVERSATION_LEN;

    std::memcpy(msg.sender, rec + pos, SIGURDOS_MSG_SENDER_LEN);
    msg.sender[SIGURDOS_MSG_SENDER_LEN - 1] = '\0';
    pos += SIGURDOS_MSG_SENDER_LEN;

    std::memcpy(msg.text, rec + pos, SIGURDOS_MSG_TEXT_LEN);
    msg.text[SIGURDOS_MSG_TEXT_LEN - 1] = '\0';
    pos += SIGURDOS_MSG_TEXT_LEN;

    std::memcpy(&msg.timestamp, rec + pos, 4);
    pos += 4;

    std::memcpy(msg.sender_prefix, rec + pos, SIGURDOS_MSG_PREFIX_LEN);
    pos += SIGURDOS_MSG_PREFIX_LEN;

    std::memcpy(&msg.rssi, rec + pos, 2);
    pos += 2;

    msg.snr_quarters = (int8_t)rec[pos++];
    msg.path_len = rec[pos++];
    msg.txt_type = rec[pos++];
    msg.extra_len = rec[pos++];
    std::memcpy(msg.extra, rec + pos, 8); pos += 8;
    applyFlags(msg, rec[pos++]);
    return true;
}

static bool readRecordAt(uint32_t index, StoredMessage& msg)
{
    uint32_t count = 0;
    if (!readHeader(&count) || index >= count) return false;
    const long offset = 9L + (long)index * (long)detail::MESSAGE_STORE_RECORD_SIZE;
    uint8_t rec[detail::MESSAGE_STORE_RECORD_SIZE];

#if defined(ESP32_PLATFORM)
    File f = SPIFFS.open(STORE_PATH, "r");
    if (!f) return false;
    if (!f.seek(offset, SeekSet)) { f.close(); return false; }
    bool ok = f.read(rec, sizeof(rec)) == sizeof(rec);
    f.close();
#else
    FILE* f = std::fopen(g_native_path, "rb");
    if (!f) return false;
    if (std::fseek(f, offset, SEEK_SET) != 0) { std::fclose(f); return false; }
    bool ok = std::fread(rec, 1, sizeof(rec), f) == sizeof(rec);
    std::fclose(f);
#endif
    return ok && readRecordRaw(msg, rec, sizeof(rec));
}

static void writeRecordRaw(const StoredMessage& msg, uint8_t* rec, size_t len)
{
    if (!rec || len < detail::MESSAGE_STORE_RECORD_SIZE) return;

    StoredMessage norm = msg;
    detail::storedMessageNormalize(norm);

    size_t pos = 0;
    std::memset(rec, 0, len);
    std::memcpy(rec + pos, &norm.store_id, 4); pos += 4;
    std::memcpy(rec + pos, norm.conversation,
                strnlen(norm.conversation, SIGURDOS_MSG_CONVERSATION_LEN - 1));
    pos += SIGURDOS_MSG_CONVERSATION_LEN;

    std::memcpy(rec + pos, norm.sender,
                strnlen(norm.sender, SIGURDOS_MSG_SENDER_LEN - 1));
    pos += SIGURDOS_MSG_SENDER_LEN;

    std::memcpy(rec + pos, norm.text,
                strnlen(norm.text, SIGURDOS_MSG_TEXT_LEN - 1));
    pos += SIGURDOS_MSG_TEXT_LEN;

    std::memcpy(rec + pos, &norm.timestamp, 4);
    pos += 4;

    std::memcpy(rec + pos, norm.sender_prefix, SIGURDOS_MSG_PREFIX_LEN);
    pos += SIGURDOS_MSG_PREFIX_LEN;

    std::memcpy(rec + pos, &norm.rssi, 2);
    pos += 2;

    rec[pos++] = (uint8_t)norm.snr_quarters;
    rec[pos++] = norm.path_len;
    rec[pos++] = norm.txt_type;
    rec[pos++] = norm.extra_len;
    std::memcpy(rec + pos, norm.extra, 8); pos += 8;
    rec[pos++] = flagsFor(norm);
}

static bool readHeader(uint32_t* out_count)
{
    if (!ensureFs() || !existsStore()) return false;

#if defined(ESP32_PLATFORM)
    File f = SPIFFS.open(STORE_PATH, "r");
    if (!f) return false;
    uint32_t magic = 0;
    uint8_t version = 0;
    uint32_t count = 0;
    bool ok = f.read((uint8_t*)&magic, 4) == 4 &&
              f.read(&version, 1) == 1 &&
              f.read((uint8_t*)&count, 4) == 4;
    f.close();
#else
    FILE* f = std::fopen(g_native_path, "rb");
    if (!f) return false;
    uint32_t magic = 0;
    uint8_t version = 0;
    uint32_t count = 0;
    bool ok = std::fread(&magic, 1, 4, f) == 4 &&
              std::fread(&version, 1, 1, f) == 1 &&
              std::fread(&count, 1, 4, f) == 4;
    std::fclose(f);
#endif

    if (!ok || magic != detail::MESSAGE_STORE_MAGIC ||
        version != detail::MESSAGE_STORE_VERSION) {
        return false;
    }
    if (out_count) *out_count = count;
    return true;
}

static bool writeHeaderCount(uint32_t count)
{
    if (!ensureFs()) return false;

#if defined(ESP32_PLATFORM)
    File f = SPIFFS.open(STORE_PATH, existsStore() ? "r+" : "w");
    if (!f) return false;
    uint32_t magic = detail::MESSAGE_STORE_MAGIC;
    uint8_t version = detail::MESSAGE_STORE_VERSION;
    bool ok = f.write((const uint8_t*)&magic, 4) == 4 &&
              f.write(&version, 1) == 1 &&
              f.write((const uint8_t*)&count, 4) == 4;
    f.close();
    return ok;
#else
    FILE* f = std::fopen(g_native_path, existsStore() ? "r+b" : "w+b");
    if (!f) return false;
    uint32_t magic = detail::MESSAGE_STORE_MAGIC;
    uint8_t version = detail::MESSAGE_STORE_VERSION;
    bool ok = std::fwrite(&magic, 1, 4, f) == 4 &&
              std::fwrite(&version, 1, 1, f) == 1 &&
              std::fwrite(&count, 1, 4, f) == 4;
    std::fclose(f);
    return ok;
#endif
}

static bool writeHeaderIfNeeded()
{
    uint32_t count = 0;
    if (readHeader(&count)) return true;
    removeStore();
    return writeHeaderCount(0);
}

static int loadAllInternal(StoredMessage* out, int max)
{
    if (!out || max <= 0) return 0;

    uint32_t count = 0;
    if (!readHeader(&count)) return 0;

#if defined(ESP32_PLATFORM)
    File f = SPIFFS.open(STORE_PATH, "r");
    if (!f) return 0;
    f.seek(9, SeekSet);
#else
    FILE* f = std::fopen(g_native_path, "rb");
    if (!f) return 0;
    std::fseek(f, 9, SEEK_SET);
#endif

    int n = 0;
    uint8_t rec[detail::MESSAGE_STORE_RECORD_SIZE];
    for (uint32_t i = 0; i < count; i++) {
#if defined(ESP32_PLATFORM)
        if (f.read(rec, sizeof(rec)) != sizeof(rec)) break;
#else
        if (std::fread(rec, 1, sizeof(rec), f) != sizeof(rec)) break;
#endif
        StoredMessage msg;
        if (!readRecordRaw(msg, rec, sizeof(rec))) continue;
        if (n < max) out[n++] = msg;
    }

#if defined(ESP32_PLATFORM)
    f.close();
#else
    std::fclose(f);
#endif
    return n;
}

static bool nextStoreId(uint32_t* out_id)
{
    if (!out_id) return false;
    uint32_t count = 0;
    if (!readHeader(&count)) return false;

    uint32_t max_id = 0;
    for (uint32_t i = 0; i < count; i++) {
        StoredMessage msg;
        if (!readRecordAt(i, msg)) continue;
        if (msg.store_id > max_id) max_id = msg.store_id;
    }

    if (max_id == 0xFFFFFFFFu) return false;
    *out_id = max_id + 1;
    return *out_id != 0;
}

static bool findExistingMessage(const StoredMessage& msg, StoredMessage* out)
{
    uint32_t count = 0;
    if (!readHeader(&count)) return false;
    StoredMessage existing;
    for (uint32_t i = 0; i < count; i++) {
        if (readRecordAt(i, existing) && detail::storedMessageSameIdentity(existing, msg)) {
            if (out) *out = existing;
            return true;
        }
    }
    return false;
}

static bool finishAtomicReplace(const char* tmp_path, const char* store_path, const char* bak_path)
{
    if (!tmp_path || !store_path || !bak_path) return false;

    removePath(bak_path);
    bool backed_up = false;
    if (pathExists(store_path)) {
        if (!renamePath(store_path, bak_path)) {
            removePath(tmp_path);
            return false;
        }
        backed_up = true;
    }

    bool promoted = false;
#if !defined(ESP32_PLATFORM)
    if (g_native_fail_next_replace_rename) {
        g_native_fail_next_replace_rename = false;
    } else {
        promoted = renamePath(tmp_path, store_path);
    }
#else
    promoted = renamePath(tmp_path, store_path);
#endif

    if (!promoted) {
        removePath(store_path);
        if (backed_up) renamePath(bak_path, store_path);
        removePath(tmp_path);
        return false;
    }

    removePath(bak_path);
    return true;
}

static bool recoverPendingReplace()
{
    if (!ensureFs()) return false;

#if defined(ESP32_PLATFORM)
    const char* store_path = STORE_PATH;
    const char* tmp_path = STORE_TMP_PATH;
    const char* bak_path = STORE_BAK_PATH;
#else
    const char* store_path = g_native_path;
    char tmp_path_buf[180];
    char bak_path_buf[180];
    if (!siblingPath(tmp_path_buf, sizeof(tmp_path_buf), ".tmp") ||
        !siblingPath(bak_path_buf, sizeof(bak_path_buf), ".bak")) {
        return false;
    }
    const char* tmp_path = tmp_path_buf;
    const char* bak_path = bak_path_buf;
#endif

    if (pathExists(store_path)) {
        removePath(tmp_path);
        removePath(bak_path);
        return true;
    }

    if (pathExists(bak_path)) {
        removePath(store_path);
        if (!renamePath(bak_path, store_path)) return false;
        removePath(tmp_path);
        return true;
    }

    if (pathExists(tmp_path)) {
        if (!renamePath(tmp_path, store_path)) {
            removePath(tmp_path);
            return true;
        }
        uint32_t count = 0;
        if (!readHeader(&count)) {
            removeStore();
        }
    }

    return true;
}

// Atomically replace the entire message store with the given records.
// Writes to a temp file first, backs up the live file, then promotes the temp.
// If promotion fails, the backup is restored and startup recovery can finish it.
static bool atomicReplaceStore(const StoredMessage* msgs, uint32_t count)
{
    if (!ensureFs()) return false;
    if (!msgs && count > 0) return false;

#if defined(ESP32_PLATFORM)
    removePath(STORE_TMP_PATH);
    File f = SPIFFS.open(STORE_TMP_PATH, "w");
    if (!f) return false;
    uint32_t magic = detail::MESSAGE_STORE_MAGIC;
    uint8_t version = detail::MESSAGE_STORE_VERSION;
    bool ok = f.write((const uint8_t*)&magic, 4) == 4 &&
              f.write(&version, 1) == 1 &&
              f.write((const uint8_t*)&count, 4) == 4;
    if (ok) {
        uint8_t rec[detail::MESSAGE_STORE_RECORD_SIZE];
        for (uint32_t i = 0; ok && i < count; i++) {
            writeRecordRaw(msgs[i], rec, sizeof(rec));
            ok = f.write(rec, sizeof(rec)) == sizeof(rec);
        }
    }
    f.close();
    if (!ok) {
        removePath(STORE_TMP_PATH);
        return false;
    }
    return finishAtomicReplace(STORE_TMP_PATH, STORE_PATH, STORE_BAK_PATH);
#else
    char tmp_path[180];
    char bak_path[180];
    if (!siblingPath(tmp_path, sizeof(tmp_path), ".tmp") ||
        !siblingPath(bak_path, sizeof(bak_path), ".bak")) {
        return false;
    }
    removePath(tmp_path);
    FILE* f = std::fopen(tmp_path, "wb");
    if (!f) return false;
    uint32_t magic = detail::MESSAGE_STORE_MAGIC;
    uint8_t version = detail::MESSAGE_STORE_VERSION;
    bool ok = std::fwrite(&magic, 1, 4, f) == 4 &&
              std::fwrite(&version, 1, 1, f) == 1 &&
              std::fwrite(&count, 1, 4, f) == 4;
    if (ok) {
        uint8_t rec[detail::MESSAGE_STORE_RECORD_SIZE];
        for (uint32_t i = 0; ok && i < count; i++) {
            writeRecordRaw(msgs[i], rec, sizeof(rec));
            ok = std::fwrite(rec, 1, sizeof(rec), f) == sizeof(rec);
        }
    }
    std::fclose(f);
    if (!ok) {
        removePath(tmp_path);
        return false;
    }
    return finishAtomicReplace(tmp_path, g_native_path, bak_path);
#endif
}

static bool repairStoreIdsIfNeeded()
{
    uint32_t count = 0;
    if (!readHeader(&count)) return false;
    if (count == 0) return true;
    if (count > 256) return false;

    StoredMessage* msgs = (StoredMessage*)std::malloc(sizeof(StoredMessage) * count);
    if (!msgs) return false;
    int n = loadAllInternal(msgs, (int)count);
    if (n <= 0) {
        std::free(msgs);
        return false;
    }

    bool needs_repair = false;
    uint32_t prev_id = 0;
    for (int i = 0; i < n; i++) {
        if (msgs[i].store_id == 0 || msgs[i].store_id <= prev_id) {
            needs_repair = true;
            break;
        }
        prev_id = msgs[i].store_id;
    }

    if (!needs_repair) {
        std::free(msgs);
        return true;
    }

    for (int i = 0; i < n; i++) {
        msgs[i].store_id = (uint32_t)i + 1u;
    }
    bool ok = atomicReplaceStore(msgs, (uint32_t)n);
    std::free(msgs);
    return ok;
}

static bool trimStoreToRecent(uint32_t max_records)
{
    if (max_records == 0) return messageStoreClear();
    StoredMessage* recent = (StoredMessage*)std::malloc(sizeof(StoredMessage) * max_records);
    if (!recent) return false;
    int n = messageStoreLoadRecent(nullptr, recent, (int)max_records);
    if (!atomicReplaceStore(recent, (uint32_t)n)) {
        std::free(recent);
        return false;
    }
    std::free(recent);
    return true;
}

} // namespace

namespace detail {

static bool storedMessageHasSenderPrefix(const StoredMessage& msg)
{
    for (size_t i = 0; i < SIGURDOS_MSG_PREFIX_LEN; i++) {
        if (msg.sender_prefix[i] != 0) return true;
    }
    return false;
}

bool storedMessageSameIdentity(const StoredMessage& a, const StoredMessage& b)
{
    const bool a_has_prefix = storedMessageHasSenderPrefix(a);
    const bool b_has_prefix = storedMessageHasSenderPrefix(b);
    bool same_sender = false;
    if (a_has_prefix || b_has_prefix) {
        same_sender = a_has_prefix && b_has_prefix &&
                      std::memcmp(a.sender_prefix, b.sender_prefix,
                                  SIGURDOS_MSG_PREFIX_LEN) == 0;
    } else {
        same_sender = std::strncmp(a.sender, b.sender, SIGURDOS_MSG_SENDER_LEN) == 0;
    }

    return std::strncmp(a.conversation, b.conversation, SIGURDOS_MSG_CONVERSATION_LEN) == 0 &&
           same_sender &&
           a.timestamp == b.timestamp &&
           a.is_self == b.is_self &&
           a.is_channel == b.is_channel;
}

void storedMessageNormalize(StoredMessage& msg)
{
    msg.conversation[SIGURDOS_MSG_CONVERSATION_LEN - 1] = '\0';
    msg.sender[SIGURDOS_MSG_SENDER_LEN - 1] = '\0';
    msg.text[SIGURDOS_MSG_TEXT_LEN - 1] = '\0';
    if (msg.timestamp == 0) msg.timestamp = 1;
}

} // namespace detail

bool messageStoreBegin()
{
    if (!recoverPendingReplace()) return false;
    if (!writeHeaderIfNeeded()) return false;
    uint32_t count = 0;
    if (readHeader(&count) && count > MESSAGE_STORE_MAX_RECORDS) {
        if (!trimStoreToRecent(MESSAGE_STORE_MAX_RECORDS)) return false;
    }
    return repairStoreIdsIfNeeded();
}

bool messageStoreClear()
{
    removeStore();
    return writeHeaderCount(0);
}

static bool appendStoredMessage(StoredMessage& msg)
{
    if (!writeHeaderIfNeeded()) return false;

    StoredMessage norm = msg;
    detail::storedMessageNormalize(norm);

    StoredMessage existing{};
    if (findExistingMessage(norm, &existing)) {
        msg.store_id = existing.store_id;
        return true;
    }

    uint32_t store_id = 0;
    if (!nextStoreId(&store_id)) return false;
    norm.store_id = store_id;

    uint8_t rec[detail::MESSAGE_STORE_RECORD_SIZE];
    writeRecordRaw(norm, rec, sizeof(rec));

#if defined(ESP32_PLATFORM)
    File f = SPIFFS.open(STORE_PATH, "a");
    if (!f) return false;
    bool ok = f.write(rec, sizeof(rec)) == sizeof(rec);
    f.close();
#else
    FILE* f = std::fopen(g_native_path, "ab");
    if (!f) return false;
    bool ok = std::fwrite(rec, 1, sizeof(rec), f) == sizeof(rec);
    std::fclose(f);
#endif
    if (!ok) return false;

    uint32_t count = 0;
    readHeader(&count);
    uint32_t new_count = count + 1;
    if (!writeHeaderCount(new_count)) return false;
    if (new_count > MESSAGE_STORE_MAX_RECORDS) {
        if (!trimStoreToRecent(MESSAGE_STORE_MAX_RECORDS)) return false;
    }
    msg.store_id = norm.store_id;
    return true;
}

bool messageStoreAppend(StoredMessage& msg)
{
    return appendStoredMessage(msg);
}

bool messageStoreAppend(const StoredMessage& msg)
{
    StoredMessage copy = msg;
    return appendStoredMessage(copy);
}

int messageStoreLoadRecent(const char* conversation, StoredMessage* out, int max)
{
    if (!out || max <= 0) return 0;
    uint32_t count = 0;
    if (!readHeader(&count)) return 0;
    int n = 0;
    for (int i = (int)count - 1; i >= 0 && n < max; i--) {
        StoredMessage msg;
        if (!readRecordAt((uint32_t)i, msg)) continue;
        if (!conversation || !conversation[0] ||
            std::strncmp(msg.conversation, conversation, SIGURDOS_MSG_CONVERSATION_LEN) == 0) {
            out[n++] = msg;
        }
    }
    for (int i = 0; i < n / 2; i++) {
        StoredMessage swap = out[i];
        out[i] = out[n - 1 - i];
        out[n - 1 - i] = swap;
    }
    return n;
}

int messageStoreLoadAll(StoredMessage* out, int max)
{
    return loadAllInternal(out, max);
}

bool messageStoreMarkAcked(const char* conversation, uint32_t timestamp)
{
    if (!conversation || !conversation[0] || timestamp == 0) return false;
    uint32_t count = 0;
    if (!readHeader(&count)) return false;
    if (count > 256) return false;
    StoredMessage* msgs = (StoredMessage*)std::malloc(sizeof(StoredMessage) * count);
    if (!msgs) return false;
    int n = messageStoreLoadAll(msgs, (int)count);
    bool changed = false;
    for (int i = 0; i < n; i++) {
        if (msgs[i].timestamp == timestamp &&
            std::strncmp(msgs[i].conversation, conversation, SIGURDOS_MSG_CONVERSATION_LEN) == 0) {
            msgs[i].acked = true;
            changed = true;
        }
    }
    if (!changed) {
        std::free(msgs);
        return false;
    }

    bool ok = atomicReplaceStore(msgs, (uint32_t)n);
    std::free(msgs);
    return ok;
}

bool messageStoreMarkCompanionSent(uint32_t store_id)
{
    uint32_t count = 0;
    if (!readHeader(&count)) return false;
    if (count == 0) return false;
    if (count > 256) return false;
    StoredMessage* msgs = (StoredMessage*)std::malloc(sizeof(StoredMessage) * count);
    if (!msgs) return false;
    int n = messageStoreLoadAll(msgs, (int)count);
    bool found = false;
    for (int i = 0; i < n; i++) {
        if (msgs[i].store_id == store_id) {
            msgs[i].companion_sent = true;
            found = true;
            break;
        }
    }
    if (!found) {
        std::free(msgs);
        return false;
    }
    bool ok = atomicReplaceStore(msgs, (uint32_t)n);
    std::free(msgs);
    return ok;
}

int messageStoreLoadUnsent(StoredMessage* out, int max)
{
    if (!out || max <= 0) return 0;
    uint32_t count = 0;
    if (!readHeader(&count)) return 0;
    int out_idx = 0;
    for (int i = (int)count - 1; i >= 0 && out_idx < max; i--) {
        StoredMessage msg;
        if (!readRecordAt((uint32_t)i, msg)) continue;
        if (!msg.companion_sent) {
            out[out_idx++] = msg;
        }
    }
    // Reverse to chronological order (we loaded newest-first)
    for (int i = 0; i < out_idx / 2; i++) {
        StoredMessage swap = out[i];
        out[i] = out[out_idx - 1 - i];
        out[out_idx - 1 - i] = swap;
    }
    return out_idx;
}

int messageStoreCount()
{
    uint32_t count = 0;
    return readHeader(&count) ? (int)count : 0;
}

#if !defined(ESP32_PLATFORM)
void messageStoreSetNativePath(const char* path)
{
    if (!path || !path[0]) return;
    copyZ(g_native_path, sizeof(g_native_path), path);
}

void messageStoreSetNativeFailNextReplaceRename(bool fail)
{
    g_native_fail_next_replace_rename = fail;
}
#endif

} // namespace mesh
} // namespace sigurdos
