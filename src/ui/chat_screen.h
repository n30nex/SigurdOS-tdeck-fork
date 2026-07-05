#pragma once

// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2025 Ben
//
// This file is part of SigurdOS.
//
// SigurdOS is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.
//
// SigurdOS is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with SigurdOS.  If not, see <https://www.gnu.org/licenses/>.


#include "../hal/trackball.h"
#include "../mesh/public_channel.h"
#include <lvgl.h>
#include <cstdint>
#include <cstdio>
#include <cstring>

namespace sigurdos::ui {

// Chat message history cap bounds, per channel.
static constexpr uint16_t CHAT_SCREEN_MESSAGE_CAP_MAX     = 200;
static constexpr uint16_t CHAT_SCREEN_MESSAGE_CAP_DEFAULT = 200;
static constexpr uint16_t CHAT_SCREEN_MESSAGE_CAP_MIN     = 8;
// Keep the visible tail small on T-Deck hardware. Each chat bubble is several
// LVGL objects, so rendering too many at once can starve input or trip WDT.
static constexpr uint16_t CHAT_SCREEN_RENDER_MAX          = 24;
static constexpr uint16_t CHAT_SCREEN_PUBLIC_RENDER_MAX   = 8;
static constexpr int CHAT_EMOJI_PICKER_PAGE_SIZE = 16;
static constexpr uint32_t CHAT_SCREEN_LIST_LOAD_ANIM_MS = 200;
static constexpr uint32_t CHAT_SCREEN_CHANNEL_OPEN_DELAY_MS = 260;
static constexpr uint32_t CHAT_SCREEN_CHANNEL_SELECT_DELAY_MS = 40;
static constexpr int CHAT_SCREEN_CHANNEL_NAME_CAP = 37;
static constexpr size_t CHAT_SCREEN_PERSIST_SENDER_BYTES = 32;
static constexpr size_t CHAT_SCREEN_PERSIST_TEXT_BYTES = 160;
static constexpr size_t CHAT_SCREEN_PERSIST_RECORD_BYTES_V2 =
    CHAT_SCREEN_PERSIST_SENDER_BYTES + CHAT_SCREEN_PERSIST_TEXT_BYTES + 4 + 1;
static constexpr size_t CHAT_SCREEN_PERSIST_RECORD_BYTES =
    CHAT_SCREEN_PERSIST_RECORD_BYTES_V2 + 1;
static constexpr uint8_t CHAT_SCREEN_TEXT_PLAIN = 0;
static constexpr uint8_t CHAT_SCREEN_TEXT_CLI_DATA = 1;
static constexpr uint8_t CHAT_SCREEN_TEXT_SIGNED_PLAIN = 2;

inline uint16_t chat_screen_normalize_message_cap(uint16_t cap)
{
    if (cap == 0) return CHAT_SCREEN_MESSAGE_CAP_DEFAULT;
    if (cap < CHAT_SCREEN_MESSAGE_CAP_MIN) return CHAT_SCREEN_MESSAGE_CAP_MIN;
    if (cap > CHAT_SCREEN_MESSAGE_CAP_MAX) return CHAT_SCREEN_MESSAGE_CAP_MAX;
    return cap;
}

inline bool chat_screen_is_dm_name(const char* name)
{
    return name && std::strncmp(name, "DM:", 3) == 0;
}

inline bool chat_screen_is_room_name(const char* name)
{
    return name && std::strncmp(name, "Room:", 5) == 0;
}

inline const char* chat_screen_room_contact_name(const char* name)
{
    return chat_screen_is_room_name(name) ? (name + 5) : "";
}

inline void chat_screen_format_room_name(const char* contact_name,
                                         char* out,
                                         size_t out_sz)
{
    if (!out || out_sz == 0) return;
    if (!contact_name || !contact_name[0]) {
        out[0] = '\0';
        return;
    }
    std::snprintf(out, out_sz, "Room:%s", contact_name);
    out[out_sz - 1] = '\0';
}

inline bool chat_screen_filter_accepts_channel(int mode, const char* name)
{
    if (mode == 2) return chat_screen_is_dm_name(name);
    return name && name[0] && !chat_screen_is_dm_name(name);
}

inline bool chat_screen_direct_open_should_skip_channel_list(bool opened_from_chat,
                                                             bool target_ready)
{
    return target_ready && !opened_from_chat;
}

inline int chat_screen_emoji_page_count(int emoji_count)
{
    if (emoji_count <= 0) return 0;
    return (emoji_count + CHAT_EMOJI_PICKER_PAGE_SIZE - 1) / CHAT_EMOJI_PICKER_PAGE_SIZE;
}

inline int chat_screen_emoji_page_start(int page, int emoji_count)
{
    const int pages = chat_screen_emoji_page_count(emoji_count);
    if (pages == 0) return 0;
    if (page < 0) page = 0;
    if (page >= pages) page = pages - 1;
    return page * CHAT_EMOJI_PICKER_PAGE_SIZE;
}

inline int chat_screen_emoji_page_end(int page, int emoji_count)
{
    int end = chat_screen_emoji_page_start(page, emoji_count) + CHAT_EMOJI_PICKER_PAGE_SIZE;
    if (end > emoji_count) end = emoji_count;
    return end;
}

inline uint16_t chat_screen_visible_message_start(uint16_t count,
                                                  uint16_t max_render)
{
    if (max_render == 0 || count <= max_render) return 0;
    return (uint16_t)(count - max_render);
}

inline uint16_t chat_screen_render_limit_for_channel(const char* channel)
{
    return sigurdos::mesh::isPublicChannelName(channel)
        ? CHAT_SCREEN_PUBLIC_RENDER_MAX
        : CHAT_SCREEN_RENDER_MAX;
}

inline bool chat_screen_live_append_within_visible_budget(const char* channel,
                                                          uint16_t child_count)
{
    return child_count <= chat_screen_render_limit_for_channel(channel);
}

inline uint8_t chat_screen_normalize_text_type(uint8_t txt_type)
{
    return txt_type <= CHAT_SCREEN_TEXT_SIGNED_PLAIN ? txt_type : CHAT_SCREEN_TEXT_PLAIN;
}

inline bool chat_screen_message_is_command(uint8_t txt_type)
{
    return chat_screen_normalize_text_type(txt_type) == CHAT_SCREEN_TEXT_CLI_DATA;
}

inline bool chat_screen_public_message_actions_available(const char* channel,
                                                         const char* sender,
                                                         bool is_self)
{
    return !is_self && sender && sender[0] &&
           sigurdos::mesh::isPublicChannelName(channel);
}

inline bool chat_screen_public_message_dm_available(bool contact_known)
{
    return contact_known;
}

inline void chat_screen_format_public_reply_prefix(const char* sender,
                                                   char* out,
                                                   size_t out_sz)
{
    if (!out || out_sz == 0) return;
    if (!sender || !sender[0]) {
        out[0] = '\0';
        return;
    }
    std::snprintf(out, out_sz, "@%s ", sender);
    out[out_sz - 1] = '\0';
}

// Create and show the chat screen
void chat_screen_show();

// Open a direct message conversation with a contact (creates if needed)
void chat_screen_open_dm(const char* contact_name);

// Open a channel conversation directly (creates the Public channel if needed)
void chat_screen_open_channel(const char* channel_name);

// Open a room-server chat conversation without routing normal Public traffic.
void chat_screen_open_room(const char* room_name);

// Set which conversations to show: 1=channels only, 2=DMs only.
// Other values fall back to channels only.
void chat_screen_set_filter(int mode);

// Add a message to the chat display. Returns true when the message was rendered
// directly into the currently visible conversation.
bool chat_screen_add_msg(const char* channel, const char* sender, const char* text,
                         bool is_self, uint8_t txt_type = CHAT_SCREEN_TEXT_PLAIN);

// Handle trackball events for the chat screen. Returns true if consumed.
bool chat_screen_handle_trackball(SigurdOSTrackballEvent event);

// Return the chat input textarea object if the messaging view is active, else nullptr.
lv_obj_t* chat_screen_get_input_field();

// Return the name of the currently active channel (e.g. "#eng-nw"), or "" if none.
const char* chat_screen_get_active_channel_name();

// Open the per-channel quick-action menu over the messaging
// view: private per-chat scope controls plus channel actions. No-op
// unless the messaging view of a real channel is currently shown.
void chat_screen_show_channel_menu();

// True while a chat overlay (channel menu or scope picker) is open. The
// keyboard callback uses this to deliver keys straight to the focused
// overlay widget instead of forcing focus back to the message input.
bool chat_screen_overlay_active();

// Chat message history cap (per-channel): get/set and persistence-backed config.
uint16_t chat_screen_get_message_cap();
void     chat_screen_set_message_cap(uint16_t cap);

// Persist/restore per-channel message history to/from SPIFFS
void chat_save_messages();
void chat_load_messages();

// Periodically check for newly arrived ACKs and re-render if needed.
// Call from the main UI loop (~1s interval).
void chat_screen_refresh_acks();

} // namespace sigurdos::ui
