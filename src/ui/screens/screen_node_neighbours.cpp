// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2025 Ben

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

static lv_timer_t* s_neighbours_wait_timer = nullptr;
static bool s_neighbours_wait_active = false;
static uint32_t s_neighbours_wait_started_ms = 0;
static char s_neighbours_wait_contact[32] = "";

static void clear_neighbours_wait_timer()
{
    if (!s_neighbours_wait_timer) return;
    lv_timer_t* timer = s_neighbours_wait_timer;
    s_neighbours_wait_timer = nullptr;
    lv_timer_del(timer);
}

void node_neighbours_request_started(const char* contact_name)
{
    clear_neighbours_wait_timer();
    s_neighbours_wait_active = true;
    s_neighbours_wait_started_ms = millis();
    snprintf(s_neighbours_wait_contact, sizeof(s_neighbours_wait_contact), "%s",
             contact_name ? contact_name : "");
}

static void neighbours_wait_timer_cb(lv_timer_t* timer)
{
    if (timer) lv_timer_del(timer);
    if (s_neighbours_wait_timer == timer) s_neighbours_wait_timer = nullptr;
    if (current_screen() != Screen::NodeNeighbours) {
        s_neighbours_wait_active = false;
        return;
    }
    node_neighbours_screen_show();
}

static void schedule_neighbours_wait_refresh()
{
    if (!s_neighbours_wait_timer) {
        s_neighbours_wait_timer = lv_timer_create(neighbours_wait_timer_cb,
                                                  REPEATER_MANAGEMENT_REQUEST_POLL_MS,
                                                  nullptr);
    }
}

void node_neighbours_screen_show()
{
    static constexpr int ROW_H = 22;
    lv_obj_t* scr = make_screen_full("Neighbours");

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
        lv_obj_set_width(lbl, LV_PCT(46));
        lv_label_set_long_mode(lbl, LV_LABEL_LONG_CLIP);
        lv_obj_set_style_text_color(lbl, lv_color_hex(TEXT_SECONDARY), 0);
        lv_obj_set_style_text_font(lbl, emoji_wrapped_montserrat_10, 0);

        lv_obj_t* val = lv_label_create(row);
        lv_label_set_text(val, value);
        lv_obj_set_width(val, LV_PCT(54));
        lv_label_set_long_mode(val, LV_LABEL_LONG_CLIP);
        lv_obj_set_style_text_color(val, lv_color_hex(color), 0);
        lv_obj_set_style_text_font(val, emoji_wrapped_montserrat_12, 0);
        lv_obj_set_style_text_align(val, LV_TEXT_ALIGN_RIGHT, 0);
    };

    if (sigurdos::mesh::hasNeighboursResponse()) {
        sigurdos::mesh::NodeNeighboursResult nr;
        sigurdos::mesh::getNeighboursResult(&nr);

        char buf[64];
        snprintf(buf, sizeof(buf), "%u total / %u returned", nr.total, nr.returned);
        add_row("Neighbours", buf, ACCENT);

        if (nr.n_items == 0) {
            lv_obj_t* empty = lv_label_create(list);
            lv_label_set_text(empty, "No neighbour entries returned");
            lv_obj_set_style_text_color(empty, lv_color_hex(TEXT_SECONDARY), 0);
            lv_obj_set_style_text_font(empty, emoji_wrapped_montserrat_12, 0);
            lv_obj_align(empty, LV_ALIGN_CENTER, 0, 0);
        } else {
            for (int i = 0; i < nr.n_items; i++) {
                const auto& item = nr.items[i];
                snprintf(buf, sizeof(buf), "%lus ago  %.1f dB",
                         static_cast<unsigned long>(item.heard_secs_ago),
                         static_cast<float>(item.snr_quarters) / 4.0f);
                add_row(item.pubkey_prefix, buf, TEXT_PRIMARY);
            }
        }

        clear_neighbours_wait_timer();
        s_neighbours_wait_active = false;
        s_neighbours_wait_contact[0] = '\0';
        sigurdos::mesh::clearResponses();
    } else {
        if (!s_neighbours_wait_active) {
            node_neighbours_request_started(nullptr);
        }
        const uint32_t now = millis();
        const bool timed_out =
            repeater_management_request_timed_out(now, s_neighbours_wait_started_ms);
        char wait_buf[128];
        if (timed_out) {
            snprintf(wait_buf, sizeof(wait_buf),
                     "Neighbours request timed out.\nUse Back or Retry.");
        } else {
            snprintf(wait_buf, sizeof(wait_buf),
                     "Requesting neighbours...\nWaiting for response...\nTimeout in %lus",
                     static_cast<unsigned long>(
                         repeater_management_request_remaining_secs(
                             now, s_neighbours_wait_started_ms)));
        }
        lv_obj_t* waiting = lv_label_create(list);
        lv_label_set_text(waiting, wait_buf);
        lv_obj_set_width(waiting, CONTENT_W - 16);
        lv_obj_set_style_text_align(waiting, LV_TEXT_ALIGN_CENTER, 0);
        lv_obj_set_style_text_color(waiting, lv_color_hex(TEXT_SECONDARY), 0);
        lv_obj_set_style_text_font(waiting, emoji_wrapped_montserrat_12, 0);
        lv_obj_align(waiting, LV_ALIGN_CENTER, 0, 0);

        if (timed_out && s_neighbours_wait_contact[0]) {
            char* retry_name = strdup(s_neighbours_wait_contact);
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
                    if (sigurdos::mesh::requestNeighbours(name)) {
                        node_neighbours_request_started(name);
                        node_neighbours_screen_show();
                    } else {
                        sigurdos::mesh::mesh_v2_queue_push(
                            "System", "", "! Neighbours request failed", 0, 0.0f);
                    }
                }, LV_EVENT_CLICKED, nullptr);
                lv_obj_add_event_cb(retry, [](lv_event_t* e) {
                    free(lv_obj_get_user_data(
                        static_cast<lv_obj_t*>(lv_event_get_current_target(e))));
                }, LV_EVENT_DELETE, nullptr);
            }
        } else if (!timed_out) {
            schedule_neighbours_wait_refresh();
        }
    }

    show_screen(scr);
}

} // namespace sigurdos::ui
