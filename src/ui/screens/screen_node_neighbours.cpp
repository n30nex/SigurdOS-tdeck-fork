// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2025 Ben

#include "../screens.h"
#include "../screens_common.h"
#include "../theme.h"
#include "../responsive.h"
#include "../../mesh/mesh_wrapper.h"
#include "../../fonts/emoji_font.h"
#include <lvgl.h>
#include <cstdio>

namespace sigurdos::ui {

using namespace theme;
using namespace responsive;

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

        sigurdos::mesh::clearResponses();
    } else {
        lv_obj_t* waiting = lv_label_create(list);
        lv_label_set_text(waiting, "Requesting neighbours...\nWaiting for response...");
        lv_obj_set_style_text_color(waiting, lv_color_hex(TEXT_SECONDARY), 0);
        lv_obj_set_style_text_font(waiting, emoji_wrapped_montserrat_12, 0);
        lv_obj_align(waiting, LV_ALIGN_CENTER, 0, 0);
    }

    show_screen(scr);
}

} // namespace sigurdos::ui
