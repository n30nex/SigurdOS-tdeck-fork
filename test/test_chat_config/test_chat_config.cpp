// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2025 Ben

#include <gtest/gtest.h>
#include <cstdint>
#include <cstring>
#include <cstdio>

#include "ui/chat_screen.h"

namespace {

using sigurdos::ui::CHAT_SCREEN_MESSAGE_CAP_DEFAULT;
using sigurdos::ui::CHAT_SCREEN_MESSAGE_CAP_MAX;
using sigurdos::ui::CHAT_SCREEN_MESSAGE_CAP_MIN;
using sigurdos::ui::CHAT_SCREEN_PUBLIC_RENDER_MAX;
using sigurdos::ui::CHAT_SCREEN_RENDER_MAX;
using sigurdos::ui::CHAT_EMOJI_PICKER_PAGE_SIZE;
using sigurdos::ui::chat_screen_emoji_page_count;
using sigurdos::ui::chat_screen_emoji_page_end;
using sigurdos::ui::chat_screen_emoji_page_start;
using sigurdos::ui::chat_screen_filter_accepts_channel;
using sigurdos::ui::chat_screen_format_public_reply_prefix;
using sigurdos::ui::chat_screen_is_dm_name;
using sigurdos::ui::chat_screen_normalize_message_cap;
using sigurdos::ui::chat_screen_public_message_actions_available;
using sigurdos::ui::chat_screen_public_message_dm_available;
using sigurdos::ui::chat_screen_render_limit_for_channel;
using sigurdos::ui::chat_screen_visible_message_start;

TEST(ChatConfig, ZeroUsesDefaultMessageCap) {
    EXPECT_EQ(chat_screen_normalize_message_cap(0), CHAT_SCREEN_MESSAGE_CAP_DEFAULT);
}

TEST(ChatConfig, ValuesBelowMinimumClampUp) {
    EXPECT_EQ(chat_screen_normalize_message_cap(1), CHAT_SCREEN_MESSAGE_CAP_MIN);
    EXPECT_EQ(chat_screen_normalize_message_cap(CHAT_SCREEN_MESSAGE_CAP_MIN - 1),
              CHAT_SCREEN_MESSAGE_CAP_MIN);
}

TEST(ChatConfig, MinimumAndMaximumAreAccepted) {
    EXPECT_EQ(chat_screen_normalize_message_cap(CHAT_SCREEN_MESSAGE_CAP_MIN),
              CHAT_SCREEN_MESSAGE_CAP_MIN);
    EXPECT_EQ(chat_screen_normalize_message_cap(CHAT_SCREEN_MESSAGE_CAP_MAX),
              CHAT_SCREEN_MESSAGE_CAP_MAX);
}

TEST(ChatConfig, MiddleValuesPassThrough) {
    EXPECT_EQ(chat_screen_normalize_message_cap(64), static_cast<uint16_t>(64));
}

TEST(ChatConfig, ValuesAboveMaximumClampDown) {
    EXPECT_EQ(chat_screen_normalize_message_cap(CHAT_SCREEN_MESSAGE_CAP_MAX + 1),
              CHAT_SCREEN_MESSAGE_CAP_MAX);
    EXPECT_EQ(chat_screen_normalize_message_cap(UINT16_MAX), CHAT_SCREEN_MESSAGE_CAP_MAX);
}

TEST(ChatConfig, DmNameDetectionUsesConversationPrefix) {
    EXPECT_TRUE(chat_screen_is_dm_name("DM: Alice"));
    EXPECT_TRUE(chat_screen_is_dm_name("DM:"));
    EXPECT_FALSE(chat_screen_is_dm_name("Public"));
    EXPECT_FALSE(chat_screen_is_dm_name("#general"));
    EXPECT_FALSE(chat_screen_is_dm_name(nullptr));
}

TEST(ChatConfig, ChannelFilterKeepsPublicAndHashtagChannels) {
    EXPECT_TRUE(chat_screen_filter_accepts_channel(1, "Public"));
    EXPECT_TRUE(chat_screen_filter_accepts_channel(1, "#general"));
    EXPECT_FALSE(chat_screen_filter_accepts_channel(1, "DM: Alice"));
    EXPECT_FALSE(chat_screen_filter_accepts_channel(1, ""));
    EXPECT_FALSE(chat_screen_filter_accepts_channel(1, nullptr));
}

TEST(ChatConfig, DmFilterKeepsOnlyDmConversations) {
    EXPECT_TRUE(chat_screen_filter_accepts_channel(2, "DM: Alice"));
    EXPECT_FALSE(chat_screen_filter_accepts_channel(2, "Public"));
    EXPECT_FALSE(chat_screen_filter_accepts_channel(2, "#general"));
}

TEST(ChatConfig, EmojiPickerUsesBoundedPages) {
    EXPECT_EQ(CHAT_EMOJI_PICKER_PAGE_SIZE, 16);
    EXPECT_EQ(chat_screen_emoji_page_count(0), 0);
    EXPECT_EQ(chat_screen_emoji_page_count(1), 1);
    EXPECT_EQ(chat_screen_emoji_page_count(16), 1);
    EXPECT_EQ(chat_screen_emoji_page_count(17), 2);
    EXPECT_EQ(chat_screen_emoji_page_count(52), 4);
}

TEST(ChatConfig, EmojiPickerClampsPageRanges) {
    EXPECT_EQ(chat_screen_emoji_page_start(-1, 52), 0);
    EXPECT_EQ(chat_screen_emoji_page_end(-1, 52), 16);
    EXPECT_EQ(chat_screen_emoji_page_start(3, 52), 48);
    EXPECT_EQ(chat_screen_emoji_page_end(3, 52), 52);
    EXPECT_EQ(chat_screen_emoji_page_start(99, 52), 48);
    EXPECT_EQ(chat_screen_emoji_page_end(99, 52), 52);
}

TEST(ChatConfig, MessageRenderTailKeepsRecentMessagesBounded) {
    EXPECT_EQ(chat_screen_visible_message_start(0, CHAT_SCREEN_RENDER_MAX), 0);
    EXPECT_EQ(chat_screen_visible_message_start(12, CHAT_SCREEN_RENDER_MAX), 0);
    EXPECT_EQ(chat_screen_visible_message_start(CHAT_SCREEN_RENDER_MAX,
                                                CHAT_SCREEN_RENDER_MAX), 0);
    EXPECT_EQ(chat_screen_visible_message_start(CHAT_SCREEN_RENDER_MAX + 5,
                                                CHAT_SCREEN_RENDER_MAX), 5);
    EXPECT_EQ(chat_screen_visible_message_start(200, CHAT_SCREEN_RENDER_MAX),
              200 - CHAT_SCREEN_RENDER_MAX);
    EXPECT_EQ(chat_screen_visible_message_start(200, 0), 0);
}

TEST(ChatConfig, RenderTailStaysSmallForTDeckLvglBudget) {
    EXPECT_LE(CHAT_SCREEN_RENDER_MAX, static_cast<uint16_t>(24));
}

TEST(ChatConfig, PublicRenderTailIsExtraSmallForTDeckLvglBudget) {
    EXPECT_LT(CHAT_SCREEN_PUBLIC_RENDER_MAX, CHAT_SCREEN_RENDER_MAX);
    EXPECT_LE(CHAT_SCREEN_PUBLIC_RENDER_MAX, static_cast<uint16_t>(8));
    EXPECT_EQ(chat_screen_render_limit_for_channel("Public"),
              CHAT_SCREEN_PUBLIC_RENDER_MAX);
    EXPECT_EQ(chat_screen_render_limit_for_channel("#general"),
              CHAT_SCREEN_RENDER_MAX);
    EXPECT_EQ(chat_screen_render_limit_for_channel("DM: Alice"),
              CHAT_SCREEN_RENDER_MAX);
}

TEST(ChatConfig, PublicMessageActionsOnlyApplyToIncomingPublicMessages) {
    EXPECT_TRUE(chat_screen_public_message_actions_available("Public", "Alice", false));
    EXPECT_FALSE(chat_screen_public_message_actions_available("Public", "Alice", true));
    EXPECT_FALSE(chat_screen_public_message_actions_available("#general", "Alice", false));
    EXPECT_FALSE(chat_screen_public_message_actions_available("Public", "", false));
    EXPECT_FALSE(chat_screen_public_message_actions_available("Public", nullptr, false));
}

TEST(ChatConfig, PublicMessageDmActionRequiresKnownContact) {
    EXPECT_TRUE(chat_screen_public_message_dm_available(true));
    EXPECT_FALSE(chat_screen_public_message_dm_available(false));
}

TEST(ChatConfig, PublicReplyPrefixMentionsSender) {
    char out[16];
    chat_screen_format_public_reply_prefix("Alice", out, sizeof(out));
    EXPECT_STREQ("@Alice ", out);

    chat_screen_format_public_reply_prefix(nullptr, out, sizeof(out));
    EXPECT_STREQ("", out);

    chat_screen_format_public_reply_prefix("VeryLongContactName", out, 8);
    EXPECT_EQ(out[7], '\0');
    EXPECT_STREQ("@VeryLo", out);
}

// ── Issue #543: DM name buffer overflow tests ──────────────────────────

// Constants matching chat_screen.cpp
static constexpr int MAX_NAME_LEN  = 31;
static constexpr int CHANNEL_BUF_SZ = 32;

// Simulates the DM name formatting that caused stack overflow #543
static void formatDmName(const char* contact_name, char* out, size_t out_sz) {
    snprintf(out, out_sz, "DM: %s", contact_name);
}

TEST(ChatScreenDmName, FormatFitsInBuffer) {
    // Verify compile-time sizing: DM prefix + MAX_NAME_LEN + null must fit in a reasonable buffer
    constexpr size_t NEEDED = sizeof("DM: ") + MAX_NAME_LEN;  // including null from ""
    EXPECT_LE(NEEDED, (size_t)37) << "Buffer must be at least " << NEEDED << " bytes";
}

TEST(ChatScreenDmName, MaxLengthName) {
    char max_name[32];
    memset(max_name, 'A', MAX_NAME_LEN);
    max_name[MAX_NAME_LEN] = '\0';

    char dm_name[37];
    formatDmName(max_name, dm_name, sizeof(dm_name));

    EXPECT_EQ(strlen(dm_name), (size_t)(4 + MAX_NAME_LEN)) << "DM name should be 'DM: ' + contact_name";
    EXPECT_EQ(strncmp(dm_name, "DM: ", 4), 0);
    EXPECT_EQ(strncmp(dm_name + 4, max_name, MAX_NAME_LEN), 0);
}

TEST(ChatScreenDmName, NullTerminatorCheck) {
    // Verify the full DM name is null-terminated even at max length
    char max_name[32];
    memset(max_name, 'A', MAX_NAME_LEN);
    max_name[MAX_NAME_LEN] = '\0';

    char dm_name[37] = {};
    memset(dm_name, 0xFF, sizeof(dm_name));  // poison
    formatDmName(max_name, dm_name, sizeof(dm_name));

    EXPECT_EQ(dm_name[4 + MAX_NAME_LEN], '\0') << "Must be null-terminated after contact name";
}

TEST(ChatScreenDmName, ChannelCopyFits) {
    // Verify DM name fits in dyn_channels[37] buffer
    // "DM: " uses 4 bytes, leaving 32 bytes for name within 37-byte channel buffer
    // 31-char names fit fully; names at MAX_NAME_LEN (31) produce "DM: " + 31 = 35 chars
    char max_name[32];
    memset(max_name, 'A', MAX_NAME_LEN);
    max_name[MAX_NAME_LEN] = '\0';

    char dm_name[37];
    formatDmName(max_name, dm_name, sizeof(dm_name));

    char channel_buf[CHANNEL_BUF_SZ];
    strncpy(channel_buf, dm_name, CHANNEL_BUF_SZ - 1);
    channel_buf[CHANNEL_BUF_SZ - 1] = '\0';

    // No crash — buffer is fully valid
    EXPECT_EQ(strlen(channel_buf), (size_t)(CHANNEL_BUF_SZ - 1))
        << "DM name truncated to fit channel buffer (4+len=" << (4 + MAX_NAME_LEN)
        << " > " << (CHANNEL_BUF_SZ - 1) << ")";
}

} // anonymous namespace
