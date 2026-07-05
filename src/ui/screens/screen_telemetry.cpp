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

#include "../screens.h"
#include "../screens_common.h"
#include "../navigation.h"
#include "../theme.h"
#include "../responsive.h"
#include "../repeater_refresh_policy.h"
#include "../../mesh/mesh_wrapper.h"
#include "../../fonts/emoji_font.h"
#include <Arduino.h>
#include <lvgl.h>
#include <cstdio>
#include <cstdlib>
#include <cstring>

namespace sigurdos::ui {

using namespace theme;
using namespace responsive;

static lv_timer_t* s_telemetry_wait_timer = nullptr;
static bool s_telemetry_wait_active = false;
static uint32_t s_telemetry_wait_started_ms = 0;
static char s_telemetry_wait_contact[32] = "";

static void clear_telemetry_wait_timer()
{
    if (!s_telemetry_wait_timer) return;
    lv_timer_t* timer = s_telemetry_wait_timer;
    s_telemetry_wait_timer = nullptr;
    lv_timer_del(timer);
}

void telemetry_request_started(const char* contact_name)
{
    clear_telemetry_wait_timer();
    s_telemetry_wait_active = true;
    s_telemetry_wait_started_ms = millis();
    snprintf(s_telemetry_wait_contact, sizeof(s_telemetry_wait_contact), "%s",
             contact_name ? contact_name : "");
}

static void telemetry_wait_timer_cb(lv_timer_t* timer)
{
    if (timer) lv_timer_del(timer);
    if (s_telemetry_wait_timer == timer) s_telemetry_wait_timer = nullptr;
    if (current_screen() != Screen::Telemetry) {
        s_telemetry_wait_active = false;
        return;
    }
    telemetry_screen_show();
}

static void schedule_telemetry_wait_refresh()
{
    if (!s_telemetry_wait_timer) {
        s_telemetry_wait_timer = lv_timer_create(telemetry_wait_timer_cb,
                                                 REPEATER_MANAGEMENT_REQUEST_POLL_MS,
                                                 nullptr);
    }
}

// ════════════════════════════════════════════════════════
// Telemetry screen (Phase 4.3)
// ════════════════════════════════════════════════════════
void telemetry_screen_show()
{
    static constexpr int ROW_H = 20;
    lv_obj_t* scr = make_screen_full("Telemetry");

    lv_obj_t* list = lv_obj_create(scr);
    lv_obj_set_size(list, LV_PCT(100), CONTENT_H - 24);
    lv_obj_align(list, LV_ALIGN_TOP_MID, 0, CONTENT_Y);
    lv_obj_set_style_bg_opa(list, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(list, 0, 0);
    lv_obj_set_style_pad_all(list, 4, 0);
    lv_obj_set_flex_flow(list, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_scrollbar_mode(list, LV_SCROLLBAR_MODE_OFF);

    auto add_row = [&](const char* label, const char* value, uint32_t color) {
        lv_obj_t* row = lv_obj_create(list);
        lv_obj_set_size(row, LV_PCT(100), ROW_H);
        lv_obj_set_style_bg_opa(row, LV_OPA_TRANSP, 0);
        lv_obj_set_style_border_width(row, 0, 0);
        lv_obj_set_flex_flow(row, LV_FLEX_FLOW_ROW);
        lv_obj_set_style_pad_left(row, 8, 0);
        lv_obj_set_style_pad_right(row, 8, 0);

        lv_obj_t* lbl = lv_label_create(row);
        lv_label_set_text(lbl, label);
        lv_obj_set_style_text_color(lbl, lv_color_hex(TEXT_SECONDARY), 0);
        lv_obj_set_style_text_font(lbl, emoji_wrapped_montserrat_10, 0);

        lv_obj_t* val = lv_label_create(row);
        lv_label_set_text(val, value);
        lv_obj_set_style_text_color(val, lv_color_hex(color), 0);
        lv_obj_set_style_text_font(val, emoji_wrapped_montserrat_12, 0);
        lv_obj_align(val, LV_ALIGN_RIGHT_MID, -8, 0);
    };

    if (sigurdos::mesh::hasTelemetryResponse()) {
        sigurdos::mesh::TelemetryResult tr;
        sigurdos::mesh::getTelemetryResult(&tr);

        if (tr.n_items == 0) {
            lv_obj_t* empty = lv_label_create(list);
            lv_label_set_text(empty, "No telemetry data");
            lv_obj_set_style_text_color(empty, lv_color_hex(TEXT_SECONDARY), 0);
            lv_obj_align(empty, LV_ALIGN_CENTER, 0, 0);
        } else {
            for (int i = 0; i < tr.n_items; i++) {
                auto& item = tr.items[i];
                uint32_t color = item.type == 116  // LPP_VOLTAGE
                    ? ACCENT_GREEN : item.type == 103 // LPP_TEMPERATURE
                    ? ACCENT : TEXT_PRIMARY;
                char label[32];
                snprintf(label, sizeof(label), "Ch.%d", item.channel);
                add_row(label, item.value_str, color);
            }
        }

        clear_telemetry_wait_timer();
        s_telemetry_wait_active = false;
        s_telemetry_wait_contact[0] = '\0';
        sigurdos::mesh::clearResponses();
    } else {
        if (!s_telemetry_wait_active) {
            telemetry_request_started(nullptr);
        }
        const uint32_t now = millis();
        const bool timed_out =
            repeater_management_request_timed_out(now, s_telemetry_wait_started_ms);
        char wait_buf[128];
        if (timed_out) {
            snprintf(wait_buf, sizeof(wait_buf),
                     "Telemetry request timed out.\nUse Back or Retry.");
        } else {
            snprintf(wait_buf, sizeof(wait_buf),
                     "Requesting telemetry...\nWaiting for response...\nTimeout in %lus",
                     static_cast<unsigned long>(
                         repeater_management_request_remaining_secs(
                             now, s_telemetry_wait_started_ms)));
        }
        lv_obj_t* waiting = lv_label_create(list);
        lv_label_set_text(waiting, wait_buf);
        lv_obj_set_width(waiting, CONTENT_W - 16);
        lv_obj_set_style_text_align(waiting, LV_TEXT_ALIGN_CENTER, 0);
        lv_obj_set_style_text_color(waiting, lv_color_hex(TEXT_SECONDARY), 0);
        lv_obj_set_style_text_font(waiting, emoji_wrapped_montserrat_12, 0);
        lv_obj_align(waiting, LV_ALIGN_CENTER, 0, 0);

        if (timed_out && s_telemetry_wait_contact[0]) {
            char* retry_name = strdup(s_telemetry_wait_contact);
            if (retry_name) {
                lv_obj_t* retry = lv_btn_create(list);
                lv_obj_set_size(retry, 110, 26);
                apply_pixel_btn_outline(retry);
                lv_obj_set_user_data(retry, retry_name);
                lv_obj_t* lbl = lv_label_create(retry);
                lv_label_set_text(lbl, LV_SYMBOL_REFRESH " Retry");
                lv_obj_center(lbl);
                lv_obj_add_event_cb(retry, [](lv_event_t* e) {
                    const char* name =
                        static_cast<const char*>(lv_obj_get_user_data(
                            static_cast<lv_obj_t*>(lv_event_get_current_target(e))));
                    if (!name || !name[0]) return;
                    if (sigurdos::mesh::requestTelemetry(name)) {
                        telemetry_request_started(name);
                        telemetry_screen_show();
                    } else {
                        sigurdos::mesh::mesh_v2_queue_push(
                            "System", "", "! Telemetry request failed", 0, 0.0f);
                    }
                }, LV_EVENT_CLICKED, nullptr);
                lv_obj_add_event_cb(retry, [](lv_event_t* e) {
                    free(lv_obj_get_user_data(
                        static_cast<lv_obj_t*>(lv_event_get_current_target(e))));
                }, LV_EVENT_DELETE, nullptr);
            }
        } else if (!timed_out) {
            schedule_telemetry_wait_refresh();
        }
    }

    show_screen(scr);
}

} // namespace sigurdos::ui
