// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Ben

#include "persistence_store.h"

#if defined(ESP32_PLATFORM) || defined(SIGURDOS_NATIVE_PREFERENCES)
#include <Preferences.h>
#endif

#if defined(ESP32_PLATFORM)
#include <SPIFFS.h>
#include <FS.h>
#else
#include <cstdio>
#include <cstring>
#endif

namespace sigurdos {
namespace mesh {

// ════════════════════════════════════════════════════
// Channel persistence (NVS-backed)
// ════════════════════════════════════════════════════

bool channelStoreSave(int count, ChannelReadFn read, void* ctx)
{
    if (!read || count < 0 || count > 255) return false;

#if defined(ESP32_PLATFORM) || defined(SIGURDOS_NATIVE_PREFERENCES)
    Preferences nvs;
    if (!nvs.begin("sigurdos", false)) return false;
    uint8_t old_count = nvs.getUChar("ch_cnt", 0);
    bool ok = true;
    for (int i = 0; i < count; i++) {
        char name[32] = {0};
        uint8_t secret[32] = {0};
        uint8_t hash[32] = {0};
        if (!read(i, name, sizeof(name), secret, sizeof(secret),
                  hash, sizeof(hash), ctx)) {
            ok = false;
            break;
        }
        char key[16];
        snprintf(key, sizeof(key), "ch_%d_name", i);
        if (nvs.putString(key, name) == 0) {
            ok = false;
            break;
        }
        snprintf(key, sizeof(key), "ch_%d_sec", i);
        if (nvs.putBytes(key, secret, sizeof(secret)) != sizeof(secret)) {
            ok = false;
            break;
        }
        snprintf(key, sizeof(key), "ch_%d_hash", i);
        if (nvs.putBytes(key, hash, sizeof(hash)) != sizeof(hash)) {
            ok = false;
            break;
        }
    }

    for (int i = count; ok && i < old_count; i++) {
        char key[16];
        snprintf(key, sizeof(key), "ch_%d_name", i);
        if (nvs.isKey(key) && !nvs.remove(key)) ok = false;
        snprintf(key, sizeof(key), "ch_%d_sec", i);
        if (nvs.isKey(key) && !nvs.remove(key)) ok = false;
        snprintf(key, sizeof(key), "ch_%d_hash", i);
        if (nvs.isKey(key) && !nvs.remove(key)) ok = false;
    }

    if (ok && nvs.putUChar("ch_cnt", (uint8_t)count) != 1) ok = false;
    nvs.end();
    return ok;
#else
    (void)count;
    return false;
#endif
}

int channelStoreLoad(ChannelLoadFn load, void* ctx)
{
    if (!load) return 0;

#if defined(ESP32_PLATFORM) || defined(SIGURDOS_NATIVE_PREFERENCES)
    Preferences nvs;
    if (!nvs.begin("sigurdos", true)) return 0;
    int n = nvs.getUChar("ch_cnt", 0);
    int loaded = 0;
    for (int i = 0; i < n; i++) {
        char key[16];
        char name[32] = {0};
        uint8_t secret[32] = {0};
        uint8_t hash[32] = {0};
        snprintf(key, sizeof(key), "ch_%d_name", i);
        if (nvs.getString(key, name, sizeof(name)) <= 0) continue;
        snprintf(key, sizeof(key), "ch_%d_sec", i);
        if (nvs.getBytes(key, secret, sizeof(secret)) <= 0) continue;
        snprintf(key, sizeof(key), "ch_%d_hash", i);
        if (nvs.getBytes(key, hash, sizeof(hash)) <= 0) continue;
        if (name[0] && load(secret, sizeof(secret), hash, name, ctx)) {
            loaded++;
        }
    }
    nvs.end();
    return loaded;
#else
    (void)load;
    return 0;
#endif
}

// ════════════════════════════════════════════════════
// Identity persistence (SPIFFS-backed)
// ════════════════════════════════════════════════════

bool identityStoreSave(const uint8_t* data, size_t len)
{
    if (!data || len == 0) return false;

#if defined(ESP32_PLATFORM)
    File f = SPIFFS.open("/mesh_id", "w");
    if (!f) return false;
    size_t written = f.write(data, len);
    f.close();
    if (written != len) {
        SPIFFS.remove("/mesh_id");
        return false;
    }
    return true;
#else
    (void)data;
    (void)len;
    return false;
#endif
}

bool identityStoreLoad(uint8_t* buf, size_t buf_len, size_t* out_len)
{
    if (!buf || buf_len == 0) return false;

#if defined(ESP32_PLATFORM)
    File f = SPIFFS.open("/mesh_id", "r");
    if (!f) return false;
    size_t sz = f.size();
    if (sz > buf_len) {
        f.close();
        return false;
    }
    size_t read = f.read(buf, sz);
    f.close();
    if (out_len) *out_len = read;
    return read == sz && sz > 0;
#else
    (void)buf;
    (void)buf_len;
    if (out_len) *out_len = 0;
    return false;
#endif
}

} // namespace mesh
} // namespace sigurdos
