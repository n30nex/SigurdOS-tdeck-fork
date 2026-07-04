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


#include "ui.h"
#include "home_screen.h"
#include "chat_screen.h"
#include "screens.h"
#include "navigation.h"
#include "notifications.h"
#include "theme.h"
#include "responsive.h"
using namespace sigurdos::responsive;
#include "../mesh/mesh_wrapper.h"
#include "../hal/battery.h"
#include "../hal/prefs.h"
#include "../hal/buzzer.h"
#include "../fonts/emoji_font.h"
#include <Arduino.h>
#include <lvgl.h>

namespace sigurdos {
namespace ui {

static lv_obj_t* splash_scr = nullptr;
static lv_obj_t* splash_status = nullptr;
static uint32_t splash_start = 0;
static bool home_shown = false;
static bool persisted_state_loaded = false;

struct ActivityFlashCtx {
    lv_obj_t* flash;
    uint8_t step;
};

static ActivityFlashCtx* activity_flash_ctx = nullptr;
static lv_timer_t* activity_flash_timer = nullptr;

static uint32_t activity_flash_color(uint8_t step)
{
    return (step % 2 == 0) ? 0xffffff : theme::ACCENT;
}

static void activity_flash_cleanup(bool delete_flash)
{
    if (activity_flash_timer) {
        lv_timer_t* timer = activity_flash_timer;
        activity_flash_timer = nullptr;
        lv_timer_del(timer);
    }
    if (activity_flash_ctx) {
        if (delete_flash && activity_flash_ctx->flash &&
            lv_obj_is_valid(activity_flash_ctx->flash)) {
            lv_obj_del_async(activity_flash_ctx->flash);
        }
        delete activity_flash_ctx;
        activity_flash_ctx = nullptr;
    }
}

static void activity_flash_timer_cb(lv_timer_t* timer)
{
    auto* ctx = (ActivityFlashCtx*)lv_timer_get_user_data(timer);
    if (!ctx || !ctx->flash || !lv_obj_is_valid(ctx->flash)) {
        if (activity_flash_timer == timer) activity_flash_timer = nullptr;
        if (activity_flash_ctx == ctx) activity_flash_ctx = nullptr;
        delete ctx;
        lv_timer_del(timer);
        return;
    }

    if (ctx->step >= 4) {
        lv_obj_del_async(ctx->flash);
        if (activity_flash_timer == timer) activity_flash_timer = nullptr;
        if (activity_flash_ctx == ctx) activity_flash_ctx = nullptr;
        delete ctx;
        lv_timer_del(timer);
        return;
    }

    lv_obj_set_style_bg_color(ctx->flash, lv_color_hex(activity_flash_color(ctx->step)), 0);
    ctx->step++;
}

static void show_activity_flash()
{
    lv_obj_t* active = lv_scr_act();
    if (!active) return;

    activity_flash_cleanup(true);

    lv_obj_t* flash = lv_obj_create(active);
    lv_obj_set_size(flash, LV_PCT(100), 3);
    lv_obj_align(flash, LV_ALIGN_TOP_MID, 0, 0);
    lv_obj_set_style_bg_color(flash, lv_color_hex(activity_flash_color(0)), 0);
    lv_obj_set_style_bg_opa(flash, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(flash, 0, 0);
    lv_obj_set_style_pad_all(flash, 0, 0);
    lv_obj_remove_flag(flash, (lv_obj_flag_t)(LV_OBJ_FLAG_CLICKABLE | LV_OBJ_FLAG_SCROLLABLE));
    lv_obj_move_foreground(flash);

    activity_flash_ctx = new ActivityFlashCtx{flash, 1};
    activity_flash_timer = lv_timer_create(activity_flash_timer_cb, 90, activity_flash_ctx);
    if (!activity_flash_timer) {
        lv_obj_delete_delayed(flash, 360);
        delete activity_flash_ctx;
        activity_flash_ctx = nullptr;
    }
}

void init()
{
    // Register emoji font as fallback for all Montserrat fonts
    emoji_font_register_fallback();

    // ── Splash Screen ─────────────────────────────────
    splash_scr = lv_obj_create(nullptr);
    theme::apply_dark_bg(splash_scr);

    lv_obj_t* logo = lv_label_create(splash_scr);
    lv_label_set_text(logo, "SigurdOS");
    lv_obj_set_style_text_color(logo, lv_color_hex(theme::ACCENT), 0);
    lv_obj_set_style_text_font(logo, emoji_wrapped_montserrat_24, 0);
    lv_obj_align(logo, LV_ALIGN_CENTER, 0, -16);

    lv_obj_t* sub = lv_label_create(splash_scr);
    lv_label_set_text(sub, "T-Deck");
    lv_obj_set_style_text_color(sub, lv_color_hex(theme::TEXT_SECONDARY), 0);
    lv_obj_set_style_text_font(sub, emoji_wrapped_montserrat_14, 0);
    lv_obj_align(sub, LV_ALIGN_CENTER, 0, 16);

    splash_status = lv_label_create(splash_scr);
    lv_label_set_text(splash_status, "Starting display...");
    lv_obj_set_width(splash_status, DISPLAY_W - 32);
    lv_obj_set_style_text_align(splash_status, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_set_style_text_color(splash_status, lv_color_hex(theme::TEXT_SECONDARY), 0);
    lv_obj_set_style_text_font(splash_status, emoji_wrapped_montserrat_12, 0);
    lv_obj_align(splash_status, LV_ALIGN_BOTTOM_MID, 0, -56);

    // Loading bar
    lv_obj_t* bar = lv_obj_create(splash_scr);
    lv_obj_set_size(bar, DISPLAY_W * 2 / 3, 4);
    lv_obj_set_style_bg_color(bar, lv_color_hex(theme::BG_TERTIARY), 0);
    lv_obj_set_style_bg_opa(bar, LV_OPA_COVER, 0);
    lv_obj_set_style_radius(bar, 2, 0);
    lv_obj_set_style_border_width(bar, 0, 0);
    lv_obj_align(bar, LV_ALIGN_BOTTOM_MID, 0, -40);

    lv_obj_t* fill = lv_obj_create(bar);
    lv_obj_set_size(fill, DISPLAY_W / 5, 4);
    lv_obj_set_style_bg_color(fill, lv_color_hex(theme::ACCENT), 0);
    lv_obj_set_style_bg_opa(fill, LV_OPA_COVER, 0);
    lv_obj_set_style_radius(fill, 2, 0);
    lv_obj_set_style_border_width(fill, 0, 0);
    lv_obj_align(fill, LV_ALIGN_LEFT_MID, 0, 0);

    lv_scr_load(splash_scr);
    splash_start = millis();
    home_shown = false;
    persisted_state_loaded = false;
}

void set_boot_status(const char* status)
{
    if (!splash_status || !lv_obj_is_valid(splash_status) || !status) return;
    lv_label_set_text(splash_status, status);
}

void load_persisted_state()
{
    if (persisted_state_loaded) return;
    chat_load_messages();
    persisted_state_loaded = true;
}

void loop()
{
    // Transition from splash to home or onboarding after 2 seconds
    if (!home_shown && ui_splash_transition_elapsed(millis(), splash_start)) {
        const sigurdos::NodePrefs& p = sigurdos::prefs_get();
        // Show onboarding if: never saved prefs (fresh device) OR not yet configured
        if (!sigurdos::prefs_exists() || !p.configured) {
            navigate_to(Screen::Onboarding);
        } else {
            home_screen_create();
        }
        home_shown = true;
        splash_scr = nullptr;
        splash_status = nullptr;
    }

    // Periodic status bar updates (every 30s)
    static uint32_t last_update = 0;
    if (home_shown && (millis() - last_update > 30000)) {
        last_update = millis();
        home_screen_update_battery(sigurdos_battery_pct());
        home_screen_update_badges();
        {
            uint32_t epoch = sigurdos::mesh::getCurrentTime();
            char tbuf[8];
            if (epoch == 0) {
                snprintf(tbuf, sizeof(tbuf), "--:--");
            } else {
                uint32_t sec = epoch % 86400;
                snprintf(tbuf, sizeof(tbuf), "%02d:%02d", (sec/3600)%24, (sec/60)%60);
            }
            home_screen_update_time(tbuf);
        }
        // Persist state every 5 min (catches unexpected power loss)
        // modulo avoids the 22-day gap after uint16_t overflow (follow-up to #637)
        static uint16_t save_counter = 0;
        if (++save_counter % 10 == 0) {
            sigurdos::mesh::saveState();
            sigurdos::mesh::saveChannels();
            sigurdos::mesh::saveContacts();
            chat_save_messages();
        }
    }

    // Poll for new mesh messages and feed to chat
    if (home_shown) {
        static uint32_t last_msg_poll = 0;
        if (millis() - last_msg_poll > 1000) {
            last_msg_poll = millis();
            static sigurdos::mesh::MeshMessage msgs[4];  // static to avoid ~1300B stack in loop()
            int n = sigurdos::mesh::pollMessages(msgs, 4);
            bool incoming_msg = false;
            bool incoming_channel_msg = false;
            for (int i = 0; i < n; i++) {
                chat_screen_add_msg(msgs[i].channel, msgs[i].sender, msgs[i].text,
                                    msgs[i].is_self, msgs[i].txt_type);
                if (!msgs[i].is_self) {
                    incoming_msg = true;
                    if (msgs[i].channel[0]) incoming_channel_msg = true;
                }
            }
            static uint32_t last_activity_seq = 0;
            const uint32_t activity_seq = sigurdos::mesh::getMeshActivitySeq();
            const bool got_new_activity = (activity_seq != last_activity_seq);
            const auto notification = activity_notification_plan(
                got_new_activity, sigurdos::prefs_get().buzzer_quiet,
                incoming_msg, incoming_channel_msg);
            if (notification.flash) {
                last_activity_seq = activity_seq;
                show_activity_flash();
                home_screen_update_badges();
            }
            if (notification.buzz) {
                if (notification.buzz_pattern == sigurdos::hal::BuzzerPatternKind::Double) {
                    sigurdos::hal::buzzer_beep_double();
                } else {
                    sigurdos::hal::buzzer_beep_short();
                }
            }
            // Refresh ACK status on the current chat screen
            chat_screen_refresh_acks();
        }
    }
}

bool handle_trackball_event(SigurdOSTrackballEvent event)
{
    if (!home_shown) return false;
    // Block all trackball events while PIN entry screen is displayed
    // Prevents back-swipe bypass of PIN authentication (#541)
    if (is_pin_entry_active()) return true;
    if (current_screen() == Screen::Home) {
        home_screen_handle_trackball(event);
        return true;
    }
    if (current_screen() == Screen::Chat) {
        // Chat handles its own Left (channel list toggle); fall through
        // for non-messaging states where chat returns false
        if (chat_screen_handle_trackball(event)) return true;
    }
    if (current_screen() == Screen::Map) {
        if (map_screen_handle_trackball(event)) return true;
        // Allow Left to still work for back navigation
        return handle_back_swipe(event);
    }
    // Universal back-swipe: two-swipe commit for all other screens
    return handle_back_swipe(event);
}

} // namespace ui
} // namespace sigurdos
