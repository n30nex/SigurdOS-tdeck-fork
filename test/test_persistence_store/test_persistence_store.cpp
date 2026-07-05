#include <gtest/gtest.h>

#include <cstdint>
#include <cstring>
#include <vector>

#include "Preferences.h"
#include "mesh/persistence_store.h"

namespace {

struct ChannelFixture {
    const char* name;
    uint8_t secret_base;
    uint8_t hash_base;
};

struct SaveCtx {
    const ChannelFixture* channels;
    int count;
    int fail_index = -1;
};

bool readChannel(int index, char* name_out, size_t name_len,
                 uint8_t* secret_out, size_t secret_len,
                 uint8_t* hash_out, size_t hash_len, void* ctx)
{
    auto* save = static_cast<SaveCtx*>(ctx);
    if (!save || index < 0 || index >= save->count || index == save->fail_index) return false;
    const auto& ch = save->channels[index];
    std::strncpy(name_out, ch.name, name_len - 1);
    name_out[name_len - 1] = '\0';
    for (size_t i = 0; i < secret_len; i++) secret_out[i] = (uint8_t)(ch.secret_base + i);
    for (size_t i = 0; i < hash_len; i++) hash_out[i] = (uint8_t)(ch.hash_base + i);
    return true;
}

bool loadChannel(const uint8_t*, size_t, const uint8_t*, const char*, void* ctx)
{
    int* count = static_cast<int*>(ctx);
    (*count)++;
    return true;
}

void expectByteSequence(const char* key, uint8_t base)
{
    std::vector<uint8_t> bytes = Preferences::mockBytes(key);
    ASSERT_EQ(bytes.size(), 32u);
    for (size_t i = 0; i < bytes.size(); i++) {
        EXPECT_EQ(bytes[i], (uint8_t)(base + i));
    }
}

class PersistenceStoreTest : public ::testing::Test {
protected:
    void SetUp() override { Preferences::mockReset(); }
    void TearDown() override { Preferences::mockReset(); }
};

TEST_F(PersistenceStoreTest, SaveWritesCountNameSecretAndHash) {
    const ChannelFixture channels[] = {{"Public", 0x10, 0x80}};
    SaveCtx ctx{channels, 1};

    EXPECT_TRUE(sigurdos::mesh::channelStoreSave(1, readChannel, &ctx));

    EXPECT_EQ(Preferences::mockBytes("ch_cnt"), std::vector<uint8_t>{1});
    std::vector<uint8_t> name = Preferences::mockBytes("ch_0_name");
    ASSERT_GE(name.size(), 7u);
    EXPECT_STREQ(reinterpret_cast<const char*>(name.data()), "Public");
    expectByteSequence("ch_0_sec", 0x10);
    expectByteSequence("ch_0_hash", 0x80);
}

TEST_F(PersistenceStoreTest, FailedPutStringReturnsFalseAndKeepsOldCount) {
    Preferences seed;
    ASSERT_TRUE(seed.begin("sigurdos", false));
    EXPECT_EQ(seed.putUChar("ch_cnt", 2), 1u);
    seed.end();

    const ChannelFixture channels[] = {{"Public", 0x10, 0x80}};
    SaveCtx ctx{channels, 1};
    Preferences::mockFailPutKey("ch_0_name");

    EXPECT_FALSE(sigurdos::mesh::channelStoreSave(1, readChannel, &ctx));
    EXPECT_EQ(Preferences::mockBytes("ch_cnt"), std::vector<uint8_t>{2});
}

TEST_F(PersistenceStoreTest, FailedPutBytesReturnsFalse) {
    const ChannelFixture channels[] = {{"Public", 0x10, 0x80}};
    SaveCtx ctx{channels, 1};
    Preferences::mockFailPutKey("ch_0_sec");

    EXPECT_FALSE(sigurdos::mesh::channelStoreSave(1, readChannel, &ctx));
}

TEST_F(PersistenceStoreTest, FailedCountCommitReturnsFalse) {
    const ChannelFixture channels[] = {{"Public", 0x10, 0x80}};
    SaveCtx ctx{channels, 1};
    Preferences::mockFailPutKey("ch_cnt");

    EXPECT_FALSE(sigurdos::mesh::channelStoreSave(1, readChannel, &ctx));
}

TEST_F(PersistenceStoreTest, ShrinkingRemovesStaleChannelKeys) {
    const ChannelFixture initial[] = {
        {"Public", 0x10, 0x80},
        {"Ops", 0x20, 0x90},
    };
    SaveCtx initial_ctx{initial, 2};
    ASSERT_TRUE(sigurdos::mesh::channelStoreSave(2, readChannel, &initial_ctx));
    ASSERT_TRUE(Preferences::mockHasKey("ch_1_name"));
    ASSERT_TRUE(Preferences::mockHasKey("ch_1_sec"));
    ASSERT_TRUE(Preferences::mockHasKey("ch_1_hash"));

    const ChannelFixture shrunk[] = {{"Public", 0x10, 0x80}};
    SaveCtx shrunk_ctx{shrunk, 1};
    EXPECT_TRUE(sigurdos::mesh::channelStoreSave(1, readChannel, &shrunk_ctx));

    EXPECT_EQ(Preferences::mockBytes("ch_cnt"), std::vector<uint8_t>{1});
    EXPECT_FALSE(Preferences::mockHasKey("ch_1_name"));
    EXPECT_FALSE(Preferences::mockHasKey("ch_1_sec"));
    EXPECT_FALSE(Preferences::mockHasKey("ch_1_hash"));
}

TEST_F(PersistenceStoreTest, FailedStaleKeyRemovalReturnsFalseAndKeepsOldCount) {
    const ChannelFixture initial[] = {
        {"Public", 0x10, 0x80},
        {"Ops", 0x20, 0x90},
    };
    SaveCtx initial_ctx{initial, 2};
    ASSERT_TRUE(sigurdos::mesh::channelStoreSave(2, readChannel, &initial_ctx));

    const ChannelFixture shrunk[] = {{"Public", 0x10, 0x80}};
    SaveCtx shrunk_ctx{shrunk, 1};
    Preferences::mockFailRemoveKey("ch_1_sec");

    EXPECT_FALSE(sigurdos::mesh::channelStoreSave(1, readChannel, &shrunk_ctx));
    EXPECT_EQ(Preferences::mockBytes("ch_cnt"), std::vector<uint8_t>{2});
}

TEST_F(PersistenceStoreTest, ReadCallbackFailureReturnsFalse) {
    const ChannelFixture channels[] = {
        {"Public", 0x10, 0x80},
        {"Ops", 0x20, 0x90},
    };
    SaveCtx ctx{channels, 2, 1};

    EXPECT_FALSE(sigurdos::mesh::channelStoreSave(2, readChannel, &ctx));
}

TEST_F(PersistenceStoreTest, LoadReadsSavedChannels) {
    const ChannelFixture channels[] = {
        {"Public", 0x10, 0x80},
        {"Ops", 0x20, 0x90},
    };
    SaveCtx ctx{channels, 2};
    ASSERT_TRUE(sigurdos::mesh::channelStoreSave(2, readChannel, &ctx));

    int loaded = 0;
    EXPECT_EQ(sigurdos::mesh::channelStoreLoad(loadChannel, &loaded), 2);
    EXPECT_EQ(loaded, 2);
}

} // namespace
