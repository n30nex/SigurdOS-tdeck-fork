#include <gtest/gtest.h>
#include <cstdio>
#include <cstring>

#include "mesh/message_store.h"

namespace {

sigurdos::mesh::StoredMessage makeMsg(const char* conversation,
                                      const char* sender,
                                      const char* text,
                                      uint32_t ts,
                                      bool self,
                                      bool channel)
{
    sigurdos::mesh::StoredMessage msg{};
    std::strncpy(msg.conversation, conversation, sizeof(msg.conversation) - 1);
    std::strncpy(msg.sender, sender, sizeof(msg.sender) - 1);
    std::strncpy(msg.text, text, sizeof(msg.text) - 1);
    msg.timestamp = ts;
    msg.is_self = self;
    msg.is_channel = channel;
    msg.rssi = -70;
    msg.snr_quarters = 12;
    for (int i = 0; i < 6; i++) msg.sender_prefix[i] = (uint8_t)(0xA0 + i);
    return msg;
}

void setPrefix(sigurdos::mesh::StoredMessage& msg, uint8_t base)
{
    for (int i = 0; i < 6; i++) msg.sender_prefix[i] = (uint8_t)(base + i);
}

void writeRawStoreWithIds(const char* path, const uint32_t* ids, int count)
{
    FILE* f = std::fopen(path, "wb");
    ASSERT_NE(f, nullptr);

    uint32_t magic = sigurdos::mesh::detail::MESSAGE_STORE_MAGIC;
    uint8_t version = sigurdos::mesh::detail::MESSAGE_STORE_VERSION;
    uint32_t stored_count = (uint32_t)count;
    ASSERT_EQ(std::fwrite(&magic, 1, 4, f), 4u);
    ASSERT_EQ(std::fwrite(&version, 1, 1, f), 1u);
    ASSERT_EQ(std::fwrite(&stored_count, 1, 4, f), 4u);

    for (int i = 0; i < count; i++) {
        char text[24];
        std::snprintf(text, sizeof(text), "legacy%d", i + 1);
        auto msg = makeMsg("DM: Alice", "Alice", text, (uint32_t)i + 1u, false, false);
        msg.store_id = ids[i];

        uint8_t rec[sigurdos::mesh::detail::MESSAGE_STORE_RECORD_SIZE]{};
        size_t pos = 0;
        std::memcpy(rec + pos, &msg.store_id, 4); pos += 4;
        std::memcpy(rec + pos, msg.conversation, std::strlen(msg.conversation));
        pos += sigurdos::mesh::SIGURDOS_MSG_CONVERSATION_LEN;
        std::memcpy(rec + pos, msg.sender, std::strlen(msg.sender));
        pos += sigurdos::mesh::SIGURDOS_MSG_SENDER_LEN;
        std::memcpy(rec + pos, msg.text, std::strlen(msg.text));
        pos += sigurdos::mesh::SIGURDOS_MSG_TEXT_LEN;
        std::memcpy(rec + pos, &msg.timestamp, 4); pos += 4;
        std::memcpy(rec + pos, msg.sender_prefix, sigurdos::mesh::SIGURDOS_MSG_PREFIX_LEN);
        pos += sigurdos::mesh::SIGURDOS_MSG_PREFIX_LEN;
        std::memcpy(rec + pos, &msg.rssi, 2); pos += 2;
        rec[pos++] = (uint8_t)msg.snr_quarters;
        rec[pos++] = msg.path_len;
        rec[pos++] = msg.txt_type;
        rec[pos++] = msg.extra_len;
        std::memcpy(rec + pos, msg.extra, 8); pos += 8;
        uint8_t flags = 0;
        if (msg.is_self) flags |= 0x01;
        if (msg.is_channel) flags |= 0x02;
        if (msg.acked) flags |= 0x04;
        if (msg.companion_sent) flags |= 0x08;
        rec[pos++] = flags;
        ASSERT_EQ(pos, sigurdos::mesh::detail::MESSAGE_STORE_RECORD_SIZE);
        ASSERT_EQ(std::fwrite(rec, 1, sizeof(rec), f), sizeof(rec));
    }

    std::fclose(f);
}

bool fileExists(const char* path)
{
    FILE* f = std::fopen(path, "rb");
    if (!f) return false;
    std::fclose(f);
    return true;
}

void siblingPath(const char* path, const char* suffix, char* out, size_t out_sz)
{
    std::snprintf(out, out_sz, "%s%s", path, suffix);
}

class MessageStoreTest : public ::testing::Test {
protected:
    char path[128]{};
    char tmp_path[140]{};
    char bak_path[140]{};

    void SetUp() override {
        std::snprintf(path, sizeof(path), "/tmp/sigurdos_msg_store_%d.bin",
                      ::testing::UnitTest::GetInstance()->random_seed());
        siblingPath(path, ".tmp", tmp_path, sizeof(tmp_path));
        siblingPath(path, ".bak", bak_path, sizeof(bak_path));
        sigurdos::mesh::messageStoreSetNativeFailNextReplaceRename(false);
        sigurdos::mesh::messageStoreSetNativePath(path);
        std::remove(path);
        std::remove(tmp_path);
        std::remove(bak_path);
        ASSERT_TRUE(sigurdos::mesh::messageStoreBegin());
        ASSERT_TRUE(sigurdos::mesh::messageStoreClear());
    }

    void TearDown() override {
        sigurdos::mesh::messageStoreSetNativeFailNextReplaceRename(false);
        std::remove(path);
        std::remove(tmp_path);
        std::remove(bak_path);
    }
};

TEST_F(MessageStoreTest, AppendLoadAndDedup) {
    auto msg = makeMsg("DM: Alice", "Alice", "hello", 42, false, false);

    EXPECT_TRUE(sigurdos::mesh::messageStoreAppend(msg));
    EXPECT_EQ(msg.store_id, 1u);
    EXPECT_TRUE(sigurdos::mesh::messageStoreAppend(msg));
    EXPECT_EQ(msg.store_id, 1u);
    EXPECT_EQ(sigurdos::mesh::messageStoreCount(), 1);

    sigurdos::mesh::StoredMessage out[4]{};
    int n = sigurdos::mesh::messageStoreLoadAll(out, 4);
    ASSERT_EQ(n, 1);
    EXPECT_STREQ(out[0].conversation, "DM: Alice");
    EXPECT_STREQ(out[0].sender, "Alice");
    EXPECT_STREQ(out[0].text, "hello");
    EXPECT_EQ(out[0].timestamp, 42u);
    EXPECT_FALSE(out[0].is_self);
}

TEST_F(MessageStoreTest, DedupeUsesSenderPrefixForSameNameContacts) {
    auto first = makeMsg("Public", "Alex", "Alex: one", 100, false, true);
    auto second = makeMsg("Public", "Alex", "Alex: two", 100, false, true);
    setPrefix(first, 0xA0);
    setPrefix(second, 0xB0);

    EXPECT_TRUE(sigurdos::mesh::messageStoreAppend(first));
    EXPECT_TRUE(sigurdos::mesh::messageStoreAppend(second));
    EXPECT_EQ(sigurdos::mesh::messageStoreCount(), 2);
}

TEST_F(MessageStoreTest, DedupeSurvivesSenderRenameWhenPrefixMatches) {
    auto first = makeMsg("DM: Alice", "Alice", "hello", 101, false, false);
    auto renamed = makeMsg("DM: Alice", "Alice New", "hello again", 101, false, false);
    setPrefix(first, 0xC0);
    setPrefix(renamed, 0xC0);

    EXPECT_TRUE(sigurdos::mesh::messageStoreAppend(first));
    EXPECT_TRUE(sigurdos::mesh::messageStoreAppend(renamed));
    EXPECT_EQ(sigurdos::mesh::messageStoreCount(), 1);
}

TEST_F(MessageStoreTest, RepeatedPacketDeliveryDedupesByPrefixAndTimestamp) {
    auto first = makeMsg("Public", "Bob", "Bob: replay", 102, false, true);
    auto replay = makeMsg("Public", "Bob", "Bob: replay", 102, false, true);
    setPrefix(first, 0xD0);
    setPrefix(replay, 0xD0);

    EXPECT_TRUE(sigurdos::mesh::messageStoreAppend(first));
    EXPECT_TRUE(sigurdos::mesh::messageStoreAppend(replay));
    EXPECT_EQ(sigurdos::mesh::messageStoreCount(), 1);
}

TEST_F(MessageStoreTest, DedupeFallsBackToSenderNameWhenNoPrefixExists) {
    auto first = makeMsg("DM: Legacy", "Legacy", "hello", 103, false, false);
    auto replay = makeMsg("DM: Legacy", "Legacy", "hello", 103, false, false);
    std::memset(first.sender_prefix, 0, sizeof(first.sender_prefix));
    std::memset(replay.sender_prefix, 0, sizeof(replay.sender_prefix));

    EXPECT_TRUE(sigurdos::mesh::messageStoreAppend(first));
    EXPECT_TRUE(sigurdos::mesh::messageStoreAppend(replay));
    EXPECT_EQ(sigurdos::mesh::messageStoreCount(), 1);
}

TEST_F(MessageStoreTest, PathLenRoundTrips) {
    auto msg = makeMsg("Public", "Alice", "Alice: hi", 100, false, true);
    msg.path_len = 0x83;  // distinct from the zero-init default
    EXPECT_TRUE(sigurdos::mesh::messageStoreAppend(msg));

    sigurdos::mesh::StoredMessage out[2]{};
    int n = sigurdos::mesh::messageStoreLoadAll(out, 2);
    ASSERT_EQ(n, 1);
    EXPECT_EQ(out[0].path_len, 0x83);
}

TEST_F(MessageStoreTest, LoadRecentFiltersAndPreservesOrder) {
    EXPECT_TRUE(sigurdos::mesh::messageStoreAppend(
        makeMsg("DM: Alice", "Alice", "one", 1, false, false)));
    EXPECT_TRUE(sigurdos::mesh::messageStoreAppend(
        makeMsg("#test", "Bob", "two", 2, false, true)));
    EXPECT_TRUE(sigurdos::mesh::messageStoreAppend(
        makeMsg("DM: Alice", "self", "three", 3, true, false)));

    sigurdos::mesh::StoredMessage out[4]{};
    int n = sigurdos::mesh::messageStoreLoadRecent("DM: Alice", out, 4);
    ASSERT_EQ(n, 2);
    EXPECT_STREQ(out[0].text, "one");
    EXPECT_STREQ(out[1].text, "three");
}

TEST_F(MessageStoreTest, MaxLengthDmConversationRoundTrips) {
    const char* convo = "DM: ABCDEFGHIJKLMNOPQRSTUVWXYZ12345";
    static_assert(sizeof("DM: ABCDEFGHIJKLMNOPQRSTUVWXYZ12345") <=
                  sigurdos::mesh::SIGURDOS_MSG_CONVERSATION_LEN,
                  "max-length DM conversation must fit message store");

    EXPECT_TRUE(sigurdos::mesh::messageStoreAppend(
        makeMsg(convo, "ABCDEFGHIJKLMNOPQRSTUVWXYZ12345", "hello", 42, false, false)));

    sigurdos::mesh::StoredMessage out[2]{};
    int n = sigurdos::mesh::messageStoreLoadRecent(convo, out, 2);
    ASSERT_EQ(n, 1);
    EXPECT_STREQ(out[0].conversation, convo);
    EXPECT_STREQ(out[0].text, "hello");
}

TEST_F(MessageStoreTest, StoreRotatesToNewestRecords) {
    for (uint32_t i = 1; i <= 70; i++) {
        char text[24];
        std::snprintf(text, sizeof(text), "msg%lu", (unsigned long)i);
        EXPECT_TRUE(sigurdos::mesh::messageStoreAppend(
            makeMsg("DM: Alice", "Alice", text, i, false, false)));
    }

    EXPECT_EQ(sigurdos::mesh::messageStoreCount(), 64);
    sigurdos::mesh::StoredMessage out[64]{};
    int n = sigurdos::mesh::messageStoreLoadAll(out, 64);
    ASSERT_EQ(n, 64);
    EXPECT_EQ(out[0].timestamp, 7u);
    EXPECT_EQ(out[63].timestamp, 70u);
}

TEST_F(MessageStoreTest, StoreIdStaysUniqueAfterRotation) {
    for (uint32_t i = 1; i <= 70; i++) {
        char text[24];
        std::snprintf(text, sizeof(text), "msg%lu", (unsigned long)i);
        EXPECT_TRUE(sigurdos::mesh::messageStoreAppend(
            makeMsg("DM: Alice", "Alice", text, i, false, false)));
    }

    sigurdos::mesh::StoredMessage before[64]{};
    int n = sigurdos::mesh::messageStoreLoadAll(before, 64);
    ASSERT_EQ(n, 64);
    for (int i = 1; i < n; i++) {
        EXPECT_GT(before[i].store_id, before[i - 1].store_id);
    }

    uint32_t last_id_before = before[63].store_id;
    EXPECT_TRUE(sigurdos::mesh::messageStoreAppend(
        makeMsg("DM: Alice", "Alice", "after-trim", 71, false, false)));

    sigurdos::mesh::StoredMessage out[64]{};
    n = sigurdos::mesh::messageStoreLoadAll(out, 64);
    ASSERT_EQ(n, 64);
    EXPECT_EQ(out[0].timestamp, 8u);
    EXPECT_EQ(out[63].timestamp, 71u);
    EXPECT_GT(out[63].store_id, last_id_before);
    for (int i = 1; i < n; i++) {
        EXPECT_GT(out[i].store_id, out[i - 1].store_id);
    }

    EXPECT_TRUE(sigurdos::mesh::messageStoreMarkCompanionSent(out[63].store_id));
    sigurdos::mesh::StoredMessage verify[64]{};
    n = sigurdos::mesh::messageStoreLoadAll(verify, 64);
    ASSERT_EQ(n, 64);
    for (int i = 0; i < n - 1; i++) {
        EXPECT_FALSE(verify[i].companion_sent);
    }
    EXPECT_TRUE(verify[n - 1].companion_sent);
}

TEST_F(MessageStoreTest, MarkAckedUpdatesStoredMessage) {
    EXPECT_TRUE(sigurdos::mesh::messageStoreAppend(
        makeMsg("DM: Alice", "self", "sent", 77, true, false)));
    EXPECT_TRUE(sigurdos::mesh::messageStoreMarkAcked("DM: Alice", 77));

    sigurdos::mesh::StoredMessage out[2]{};
    int n = sigurdos::mesh::messageStoreLoadAll(out, 2);
    ASSERT_EQ(n, 1);
    EXPECT_TRUE(out[0].acked);
}

TEST_F(MessageStoreTest, StoreIdIsMonotonic) {
    EXPECT_TRUE(sigurdos::mesh::messageStoreAppend(
        makeMsg("DM: Alice", "Alice", "first", 1, false, false)));
    EXPECT_TRUE(sigurdos::mesh::messageStoreAppend(
        makeMsg("DM: Alice", "Alice", "second", 2, false, false)));
    EXPECT_TRUE(sigurdos::mesh::messageStoreAppend(
        makeMsg("DM: Bob", "Bob", "third", 3, false, false)));

    sigurdos::mesh::StoredMessage out[4]{};
    int n = sigurdos::mesh::messageStoreLoadAll(out, 4);
    ASSERT_EQ(n, 3);
    EXPECT_EQ(out[0].store_id, 1u);
    EXPECT_EQ(out[1].store_id, 2u);
    EXPECT_EQ(out[2].store_id, 3u);
}

TEST_F(MessageStoreTest, BeginRepairsLegacyZeroAndDuplicateStoreIds) {
    const uint32_t ids[] = {0u, 1u, 1u};
    writeRawStoreWithIds(path, ids, 3);

    ASSERT_TRUE(sigurdos::mesh::messageStoreBegin());

    sigurdos::mesh::StoredMessage out[4]{};
    int n = sigurdos::mesh::messageStoreLoadAll(out, 4);
    ASSERT_EQ(n, 3);
    EXPECT_EQ(out[0].store_id, 1u);
    EXPECT_EQ(out[1].store_id, 2u);
    EXPECT_EQ(out[2].store_id, 3u);

    EXPECT_TRUE(sigurdos::mesh::messageStoreMarkCompanionSent(1));
    n = sigurdos::mesh::messageStoreLoadAll(out, 4);
    ASSERT_EQ(n, 3);
    EXPECT_TRUE(out[0].companion_sent);
    EXPECT_FALSE(out[1].companion_sent);
    EXPECT_FALSE(out[2].companion_sent);
}

TEST_F(MessageStoreTest, BeginRestoresBackupWhenLiveMissing) {
    EXPECT_TRUE(sigurdos::mesh::messageStoreAppend(
        makeMsg("DM: Alice", "Alice", "before-backup", 1, false, false)));

    ASSERT_EQ(std::rename(path, bak_path), 0);
    ASSERT_FALSE(fileExists(path));
    ASSERT_TRUE(fileExists(bak_path));

    ASSERT_TRUE(sigurdos::mesh::messageStoreBegin());

    sigurdos::mesh::StoredMessage out[2]{};
    int n = sigurdos::mesh::messageStoreLoadAll(out, 2);
    ASSERT_EQ(n, 1);
    EXPECT_STREQ(out[0].text, "before-backup");
    EXPECT_TRUE(fileExists(path));
    EXPECT_FALSE(fileExists(bak_path));
}

TEST_F(MessageStoreTest, BeginKeepsLiveStoreWhenTempLeftBehind) {
    EXPECT_TRUE(sigurdos::mesh::messageStoreAppend(
        makeMsg("DM: Alice", "Alice", "live-store", 1, false, false)));
    const uint32_t ids[] = {1u};
    writeRawStoreWithIds(tmp_path, ids, 1);
    ASSERT_TRUE(fileExists(path));
    ASSERT_TRUE(fileExists(tmp_path));

    ASSERT_TRUE(sigurdos::mesh::messageStoreBegin());

    sigurdos::mesh::StoredMessage out[2]{};
    int n = sigurdos::mesh::messageStoreLoadAll(out, 2);
    ASSERT_EQ(n, 1);
    EXPECT_STREQ(out[0].text, "live-store");
    EXPECT_FALSE(fileExists(tmp_path));
}

TEST_F(MessageStoreTest, CompactionFailureLeavesOldStoreReadable) {
    EXPECT_TRUE(sigurdos::mesh::messageStoreAppend(
        makeMsg("DM: Alice", "self", "sent", 77, true, false)));

    sigurdos::mesh::messageStoreSetNativeFailNextReplaceRename(true);
    EXPECT_FALSE(sigurdos::mesh::messageStoreMarkAcked("DM: Alice", 77));

    sigurdos::mesh::StoredMessage out[2]{};
    int n = sigurdos::mesh::messageStoreLoadAll(out, 2);
    ASSERT_EQ(n, 1);
    EXPECT_STREQ(out[0].text, "sent");
    EXPECT_FALSE(out[0].acked);

    ASSERT_TRUE(sigurdos::mesh::messageStoreBegin());
    n = sigurdos::mesh::messageStoreLoadAll(out, 2);
    ASSERT_EQ(n, 1);
    EXPECT_STREQ(out[0].text, "sent");
    EXPECT_FALSE(out[0].acked);
}

TEST_F(MessageStoreTest, MarkCompanionSentMarksOnlyOneRecord) {
    EXPECT_TRUE(sigurdos::mesh::messageStoreAppend(
        makeMsg("DM: Alice", "Alice", "one", 1, false, false)));
    EXPECT_TRUE(sigurdos::mesh::messageStoreAppend(
        makeMsg("DM: Alice", "Alice", "two", 2, false, false)));
    EXPECT_TRUE(sigurdos::mesh::messageStoreAppend(
        makeMsg("DM: Bob", "Bob", "three", 3, false, false)));

    // Mark only record with store_id=2
    EXPECT_TRUE(sigurdos::mesh::messageStoreMarkCompanionSent(2));

    sigurdos::mesh::StoredMessage out[4]{};
    int n = sigurdos::mesh::messageStoreLoadAll(out, 4);
    ASSERT_EQ(n, 3);
    // Record 0 (store_id=1): NOT marked
    EXPECT_FALSE(out[0].companion_sent);
    // Record 1 (store_id=2): marked
    EXPECT_TRUE(out[1].companion_sent);
    // Record 2 (store_id=3): NOT marked
    EXPECT_FALSE(out[2].companion_sent);
}

TEST_F(MessageStoreTest, MarkCompanionSentNotFoundReturnsFalse) {
    EXPECT_TRUE(sigurdos::mesh::messageStoreAppend(
        makeMsg("DM: Alice", "Alice", "one", 1, false, false)));
    EXPECT_FALSE(sigurdos::mesh::messageStoreMarkCompanionSent(999));
}

TEST_F(MessageStoreTest, LoadUnsentOnlyReturnsUnmarkedRecords) {
    auto m1 = makeMsg("DM: Alice", "Alice", "one", 1, false, false);
    auto m2 = makeMsg("DM: Alice", "Alice", "two", 2, false, false);
    auto m3 = makeMsg("DM: Bob", "Bob", "three", 3, false, false);

    EXPECT_TRUE(sigurdos::mesh::messageStoreAppend(m1));
    EXPECT_TRUE(sigurdos::mesh::messageStoreAppend(m2));
    EXPECT_TRUE(sigurdos::mesh::messageStoreAppend(m3));

    // Mark record 2 as sent
    EXPECT_TRUE(sigurdos::mesh::messageStoreMarkCompanionSent(2));

    sigurdos::mesh::StoredMessage out[4]{};
    int n = sigurdos::mesh::messageStoreLoadUnsent(out, 4);
    ASSERT_EQ(n, 2);
    EXPECT_EQ(out[0].store_id, 1u);
    EXPECT_EQ(out[1].store_id, 3u);
}

TEST_F(MessageStoreTest, MetadataRoundTrips) {
    auto msg = makeMsg("DM: Alice", "Alice", "hello", 42, false, false);
    msg.txt_type = 2;  // COMPANION_TXT_SIGNED_PLAIN
    msg.extra_len = 4;
    msg.extra[0] = 0xDE;
    msg.extra[1] = 0xAD;
    msg.extra[2] = 0xBE;
    msg.extra[3] = 0xEF;
    msg.sender_prefix[0] = 0x11;
    msg.sender_prefix[1] = 0x22;
    msg.sender_prefix[2] = 0x33;
    msg.sender_prefix[3] = 0x44;
    msg.sender_prefix[4] = 0x55;
    msg.sender_prefix[5] = 0x66;

    EXPECT_TRUE(sigurdos::mesh::messageStoreAppend(msg));

    sigurdos::mesh::StoredMessage out[2]{};
    int n = sigurdos::mesh::messageStoreLoadAll(out, 2);
    ASSERT_EQ(n, 1);
    EXPECT_EQ(out[0].txt_type, 2u);
    EXPECT_EQ(out[0].extra_len, 4u);
    EXPECT_EQ(out[0].extra[0], 0xDE);
    EXPECT_EQ(out[0].extra[1], 0xAD);
    EXPECT_EQ(out[0].extra[2], 0xBE);
    EXPECT_EQ(out[0].extra[3], 0xEF);
    EXPECT_EQ(out[0].sender_prefix[0], 0x11);
    EXPECT_EQ(out[0].sender_prefix[1], 0x22);
    EXPECT_EQ(out[0].sender_prefix[2], 0x33);
}

} // namespace
