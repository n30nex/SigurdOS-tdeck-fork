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
#include "../theme.h"
#include "../responsive.h"
#include "../../mesh/mesh_wrapper.h"
#include "../../app/map_renderer.h"
#include "../../app/map_tile_downloader.h"
#include "../../fonts/emoji_font.h"
#include "../../hal/gps.h"
#include "../../hal/prefs.h"
#include <Arduino.h>
#include <lvgl.h>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <new>

namespace sigurdos::ui {

using namespace theme;
using namespace responsive;

// ── Forward declarations for trackball handler ──
static void render_map_with_contacts();
static void update_map_status(const char* text);

static lv_obj_t* g_map_status_label = nullptr;
static lv_obj_t* g_map_gps_label = nullptr;
static lv_obj_t* g_map_screen = nullptr;
static lv_timer_t* g_download_status_timer = nullptr;
static bool g_download_was_running = false;
static bool g_prompted_manual_location = false;
static constexpr int MANUAL_LOCATION_ZOOM = 12;
static constexpr int GPS_CENTER_ZOOM = 14;

// ════════════════════════════════════════════════════════
// Map — trackball pan navigation
// ════════════════════════════════════════════════════════

bool map_screen_handle_trackball(SigurdOSTrackballEvent event) {
    const int PAN_PX = 12;  // pixels per trackball tick
    switch (event) {
        case SigurdOSTrackballEvent::Up:
            sigurdos_map_pan(0, -PAN_PX);
            render_map_with_contacts();
            return true;
        case SigurdOSTrackballEvent::Down:
            sigurdos_map_pan(0, PAN_PX);
            render_map_with_contacts();
            return true;
        case SigurdOSTrackballEvent::Left:
            sigurdos_map_pan(-PAN_PX, 0);
            render_map_with_contacts();
            return true;
        case SigurdOSTrackballEvent::Right:
            sigurdos_map_pan(PAN_PX, 0);
            render_map_with_contacts();
            return true;
        case SigurdOSTrackballEvent::Click:
            sigurdos_map_zoom_in();
            render_map_with_contacts();
            return true;
        default:
            return false;
    }
}

// ════════════════════════════════════════════════════════
// Map — offline tile maps
// ════════════════════════════════════════════════════════

// Helper: render map tiles then overlay contact markers
static void render_map_with_contacts() {
    sigurdos_map_render();
    sigurdos::mesh::ContactInfo* contacts =
        new(std::nothrow) sigurdos::mesh::ContactInfo[MAX_CONTACTS];
    if (!contacts) {
        sigurdos_map_contact_render(nullptr, 0);
        return;
    }
    int n = sigurdos::mesh::exportContactsFull(contacts, MAX_CONTACTS);
    if (n < 0) n = 0;
    if (n > MAX_CONTACTS) n = MAX_CONTACTS;
    sigurdos_map_contact_render(contacts, n);
    delete[] contacts;
}

static void update_map_status(const char* text)
{
    if (!g_map_status_label || !lv_obj_is_valid(g_map_status_label)) return;
    lv_label_set_text(g_map_status_label, text ? text : "");
}

static void update_map_gps_status()
{
    if (!g_map_gps_label || !lv_obj_is_valid(g_map_gps_label)) return;
    const bool has_fix = sigurdos_gps_has_fix();
    const uint8_t sats_view = sigurdos_gps_satellites_in_view();
    const uint8_t sats_fix = sigurdos_gps_satellites();
    const unsigned sats = sats_view ? sats_view : sats_fix;
    char buf[20];
    snprintf(buf, sizeof(buf), "%s %u", LV_SYMBOL_GPS, sats);
    lv_label_set_text(g_map_gps_label, buf);
    lv_obj_set_style_text_color(g_map_gps_label,
        lv_color_hex(has_fix ? ACCENT_GREEN : ACCENT_RED), 0);
}

static void create_map_gps_topbar_status(lv_obj_t* scr)
{
    lv_obj_t* top = scr ? lv_obj_get_child(scr, 0) : nullptr;
    if (!top) return;
    g_map_gps_label = lv_label_create(top);
    lv_obj_set_width(g_map_gps_label, 62);
    lv_label_set_long_mode(g_map_gps_label, LV_LABEL_LONG_DOT);
    lv_obj_set_style_text_font(g_map_gps_label, emoji_wrapped_montserrat_10, 0);
    lv_obj_align(g_map_gps_label, LV_ALIGN_LEFT_MID, 42, 0);
    update_map_gps_status();
}

static bool parse_coordinate(const char* text, double min_val, double max_val,
                             double* out)
{
    if (!text || !out) return false;
    char* end = nullptr;
    double value = std::strtod(text, &end);
    if (end == text) return false;
    while (*end == ' ' || *end == '\t') ++end;
    if (*end != '\0') return false;
    if (!std::isfinite(value) || value < min_val || value > max_val) return false;
    *out = value;
    return true;
}

static void poll_tile_download_status()
{
    update_map_gps_status();
    SigurdosMapTileDownloadStatus status;
    sigurdos_map_tile_download_get_status(&status);
    if (!status.running && !status.complete && !status.message[0]) return;
    update_map_status(status.message);
    if (g_download_was_running && !status.running && status.complete) {
        sigurdos_map_discover_tiles();
        render_map_with_contacts();
    }
    g_download_was_running = status.running;
}

static void start_visible_tile_download()
{
    if (sigurdos_map_tile_download_start_current_view()) {
        g_download_was_running = true;
        poll_tile_download_status();
        return;
    }

    SigurdosMapTileDownloadStatus status;
    sigurdos_map_tile_download_get_status(&status);
    update_map_status(status.message[0] ? status.message : "Tile download busy");
}

static void center_on_gps()
{
    if (!sigurdos_gps_has_fix()) {
        update_map_status("GPS waiting for fix");
        return;
    }

    sigurdos_map_set_view(sigurdos_gps_latitude(), sigurdos_gps_longitude(),
                          GPS_CENTER_ZOOM);
    update_map_status("Centered on GPS");
    render_map_with_contacts();
}

struct ManualLocationDialogCtx {
    lv_obj_t* lat_input;
    lv_obj_t* lon_input;
};

static void close_dialog_from_child(lv_obj_t* child)
{
    lv_obj_t* dlg = child ? lv_obj_get_parent(child) : nullptr;
    if (dlg) lv_obj_del_async(dlg);
}

static bool apply_manual_location(ManualLocationDialogCtx* ctx)
{
    if (!ctx || !ctx->lat_input || !ctx->lon_input) return false;

    double lat = 0.0;
    double lon = 0.0;
    if (!parse_coordinate(lv_textarea_get_text(ctx->lat_input),
                          SIGURDOS_MAP_MIN_LAT, SIGURDOS_MAP_MAX_LAT, &lat) ||
        !parse_coordinate(lv_textarea_get_text(ctx->lon_input),
                          SIGURDOS_MAP_MIN_LON, SIGURDOS_MAP_MAX_LON, &lon)) {
        update_map_status("Enter valid lat/lon");
        return false;
    }

    sigurdos::NodePrefs p = sigurdos::prefs_get();
    p.map_location_valid = true;
    p.map_lat = (int32_t)std::lround(lat * 1000000.0);
    p.map_lon = (int32_t)std::lround(lon * 1000000.0);
    sigurdos::prefs_set(p);
    sigurdos::prefs_save(p);

    sigurdos_map_set_view(lat, lon, MANUAL_LOCATION_ZOOM);
    update_map_status("Map center saved");
    render_map_with_contacts();
    return true;
}

static void show_manual_location_dialog(lv_obj_t* parent)
{
    if (!parent) return;
    g_prompted_manual_location = true;

    auto dlg_sz = dialog_size(260, 146);
    lv_obj_t* dlg = lv_obj_create(parent);
    lv_obj_set_size(dlg, dlg_sz.w, dlg_sz.h);
    lv_obj_center(dlg);
    lv_obj_set_style_bg_color(dlg, lv_color_hex(BG_SECONDARY), 0);
    lv_obj_set_style_bg_opa(dlg, LV_OPA_COVER, 0);
    lv_obj_set_style_radius(dlg, 0, 0);
    lv_obj_set_style_border_width(dlg, 2, 0);
    lv_obj_set_style_border_color(dlg, lv_color_hex(ACCENT), 0);
    lv_obj_set_style_pad_all(dlg, 8, 0);

    auto* ctx = new(std::nothrow) ManualLocationDialogCtx{nullptr, nullptr};
    if (!ctx) {
        lv_obj_del_async(dlg);
        return;
    }
    lv_obj_add_event_cb(dlg, [](lv_event_t* e) {
        delete (ManualLocationDialogCtx*)lv_event_get_user_data(e);
    }, LV_EVENT_DELETE, ctx);

    lv_obj_t* title = lv_label_create(dlg);
    lv_label_set_text(title, "Map Center");
    lv_obj_set_style_text_color(title, lv_color_hex(TEXT_PRIMARY), 0);
    lv_obj_set_style_text_font(title, emoji_wrapped_montserrat_12, 0);
    lv_obj_align(title, LV_ALIGN_TOP_MID, 0, 0);

    lv_obj_t* lat_lbl = lv_label_create(dlg);
    lv_label_set_text(lat_lbl, "Lat");
    lv_obj_set_style_text_color(lat_lbl, lv_color_hex(TEXT_SECONDARY), 0);
    lv_obj_set_style_text_font(lat_lbl, emoji_wrapped_montserrat_10, 0);
    lv_obj_align(lat_lbl, LV_ALIGN_TOP_LEFT, 0, 28);

    ctx->lat_input = lv_textarea_create(dlg);
    lv_obj_set_size(ctx->lat_input, dlg_sz.w - 62, 24);
    lv_obj_align(ctx->lat_input, LV_ALIGN_TOP_RIGHT, 0, 22);
    lv_textarea_set_one_line(ctx->lat_input, true);
    lv_textarea_set_max_length(ctx->lat_input, 12);
    lv_textarea_set_placeholder_text(ctx->lat_input, "43.6532");
    apply_pixel_input(ctx->lat_input);
    apply_focus_style(ctx->lat_input);

    lv_obj_t* lon_lbl = lv_label_create(dlg);
    lv_label_set_text(lon_lbl, "Lon");
    lv_obj_set_style_text_color(lon_lbl, lv_color_hex(TEXT_SECONDARY), 0);
    lv_obj_set_style_text_font(lon_lbl, emoji_wrapped_montserrat_10, 0);
    lv_obj_align(lon_lbl, LV_ALIGN_TOP_LEFT, 0, 60);

    ctx->lon_input = lv_textarea_create(dlg);
    lv_obj_set_size(ctx->lon_input, dlg_sz.w - 62, 24);
    lv_obj_align(ctx->lon_input, LV_ALIGN_TOP_RIGHT, 0, 54);
    lv_textarea_set_one_line(ctx->lon_input, true);
    lv_textarea_set_max_length(ctx->lon_input, 13);
    lv_textarea_set_placeholder_text(ctx->lon_input, "-79.3832");
    apply_pixel_input(ctx->lon_input);
    apply_focus_style(ctx->lon_input);

    lv_obj_t* save_btn = lv_btn_create(dlg);
    lv_obj_set_size(save_btn, 86, 26);
    lv_obj_align(save_btn, LV_ALIGN_BOTTOM_LEFT, 8, 0);
    apply_pixel_btn(save_btn);
    lv_obj_t* save_lbl = lv_label_create(save_btn);
    lv_label_set_text(save_lbl, "Save");
    lv_obj_set_style_text_font(save_lbl, emoji_wrapped_montserrat_10, 0);
    lv_obj_center(save_lbl);
    lv_obj_add_event_cb(save_btn, [](lv_event_t* e) {
        auto* c = (ManualLocationDialogCtx*)lv_event_get_user_data(e);
        if (apply_manual_location(c)) {
            close_dialog_from_child((lv_obj_t*)lv_event_get_target(e));
            start_visible_tile_download();
        }
    }, LV_EVENT_CLICKED, ctx);

    lv_obj_t* skip_btn = lv_btn_create(dlg);
    lv_obj_set_size(skip_btn, 86, 26);
    lv_obj_align(skip_btn, LV_ALIGN_BOTTOM_RIGHT, -8, 0);
    lv_obj_set_style_bg_color(skip_btn, lv_color_hex(BG_INPUT), 0);
    lv_obj_set_style_radius(skip_btn, 0, 0);
    lv_obj_set_style_border_width(skip_btn, 0, 0);
    lv_obj_t* skip_lbl = lv_label_create(skip_btn);
    lv_label_set_text(skip_lbl, "Skip");
    lv_obj_set_style_text_font(skip_lbl, emoji_wrapped_montserrat_10, 0);
    lv_obj_center(skip_lbl);
    lv_obj_add_event_cb(skip_btn, [](lv_event_t* e) {
        close_dialog_from_child((lv_obj_t*)lv_event_get_target(e));
    }, LV_EVENT_CLICKED, nullptr);

    lv_group_t* g = lv_group_get_default();
    if (g) {
        lv_group_add_obj(g, ctx->lat_input);
        lv_group_add_obj(g, ctx->lon_input);
        lv_group_add_obj(g, save_btn);
        lv_group_add_obj(g, skip_btn);
        lv_group_focus_obj(ctx->lat_input);
    }
}

static void apply_best_map_start_location()
{
    if (sigurdos_gps_has_fix()) {
        sigurdos_map_set_view(sigurdos_gps_latitude(), sigurdos_gps_longitude(),
                              MANUAL_LOCATION_ZOOM);
        return;
    }

    const sigurdos::NodePrefs& p = sigurdos::prefs_get();
    if (p.map_location_valid) {
        sigurdos_map_set_view((double)p.map_lat / 1000000.0,
                              (double)p.map_lon / 1000000.0,
                              MANUAL_LOCATION_ZOOM);
    }
}

void map_screen_show()
{
    lv_obj_t* scr = make_screen_full("Map");
    g_map_screen = scr;
    g_map_status_label = nullptr;
    g_map_gps_label = nullptr;
    g_download_was_running = false;
    create_map_gps_topbar_status(scr);
    lv_obj_add_event_cb(scr, [](lv_event_t* e) {
        lv_obj_t* deleting = (lv_obj_t*)lv_event_get_target(e);
        if (deleting != g_map_screen) return;
        g_map_screen = nullptr;
        g_map_status_label = nullptr;
        g_map_gps_label = nullptr;
        if (g_download_status_timer) {
            lv_timer_del(g_download_status_timer);
            g_download_status_timer = nullptr;
        }
        g_download_was_running = false;
    }, LV_EVENT_DELETE, nullptr);

    // Create the map overlay container before initializing contacts
    lv_obj_t* map = lv_obj_create(scr);
    lv_obj_set_size(map, DISPLAY_W, CONTENT_H);
    lv_obj_align(map, LV_ALIGN_TOP_MID, 0, CONTENT_Y);
    lv_obj_set_style_bg_opa(map, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(map, 0, 0);
    lv_obj_add_flag(map, LV_OBJ_FLAG_CLICKABLE);

    sigurdos_map_init();
    sigurdos_map_reparent(scr);
    apply_best_map_start_location();

    // Discover tiles on first map visit (deferred from boot to avoid blocking)
    sigurdos_map_discover_tiles();

    // Pre-allocate contact marker dots on top of map BEFORE rendering
    sigurdos_map_contact_init(map);
    sigurdos_map_contact_set_tap_cb(contact_detail_screen_show);

    render_map_with_contacts();

    static int drag_start_x = 0, drag_start_y = 0;
    static uint32_t map_last_render_ms = 0;

    // Reset drag state on every entry so stale values from a previous visit
    // don't cause a map jump or skip the first re-render.
    lv_obj_add_event_cb(scr, [](lv_event_t*) {
        drag_start_x = 0;
        drag_start_y = 0;
        map_last_render_ms = 0;
    }, LV_EVENT_SCREEN_LOADED, nullptr);

    lv_obj_add_event_cb(map, [](lv_event_t* e) {
        int code = lv_event_get_code(e);
        if (code != LV_EVENT_PRESSED && code != LV_EVENT_PRESSING && code != LV_EVENT_RELEASED) return;
        lv_indev_t* indev = lv_indev_get_act();
        lv_point_t pt;
        lv_indev_get_point(indev, &pt);
        if (code == LV_EVENT_PRESSED) {
            drag_start_x = pt.x; drag_start_y = pt.y;
        } else if (code == LV_EVENT_PRESSING) {
            int dx = drag_start_x - pt.x;
            int dy = drag_start_y - pt.y;
            drag_start_x = pt.x; drag_start_y = pt.y;
            if (dx != 0 || dy != 0) sigurdos_map_pan(dx, dy);
            uint32_t now = millis();
            if (now - map_last_render_ms >= 200) {
                render_map_with_contacts();
                map_last_render_ms = now;
            }
        } else if (code == LV_EVENT_RELEASED) {
            render_map_with_contacts();
            map_last_render_ms = millis();
        }
    }, LV_EVENT_ALL, nullptr);

    // Zoom buttons (above bottom bar)
    int zoom_y_base = DISPLAY_H - BOT_BAR_H - DIVIDER_H - 8;

    lv_obj_t* zoom_in = lv_btn_create(scr);
    lv_obj_set_size(zoom_in, 32, 32);
    lv_obj_align(zoom_in, LV_ALIGN_BOTTOM_RIGHT, -8, -(BOT_BAR_H + DIVIDER_H + 8));
    lv_obj_set_style_bg_color(zoom_in, lv_color_hex(ACCENT), 0);
    lv_obj_set_style_radius(zoom_in, 0, 0);
    lv_obj_t* zi = lv_label_create(zoom_in);
    lv_label_set_text(zi, "+"); lv_obj_center(zi);
    lv_obj_add_event_cb(zoom_in, [](lv_event_t*) { sigurdos_map_zoom_in(); render_map_with_contacts(); },
                        LV_EVENT_CLICKED, nullptr);

    lv_obj_t* zoom_out = lv_btn_create(scr);
    lv_obj_set_size(zoom_out, 32, 32);
    lv_obj_align(zoom_out, LV_ALIGN_BOTTOM_RIGHT, -8, -(BOT_BAR_H + DIVIDER_H + 48));
    lv_obj_set_style_bg_color(zoom_out, lv_color_hex(BG_TERTIARY), 0);
    lv_obj_set_style_radius(zoom_out, 0, 0);
    lv_obj_t* zo = lv_label_create(zoom_out);
    lv_label_set_text(zo, "-"); lv_obj_center(zo);
    lv_obj_add_event_cb(zoom_out, [](lv_event_t*) { sigurdos_map_zoom_out(); render_map_with_contacts(); },
                        LV_EVENT_CLICKED, nullptr);

    lv_obj_t* gps_btn = lv_btn_create(scr);
    lv_obj_set_size(gps_btn, 72, 28);
    lv_obj_align(gps_btn, LV_ALIGN_BOTTOM_LEFT, 8, -(BOT_BAR_H + DIVIDER_H + 8));
    lv_obj_set_style_bg_color(gps_btn, lv_color_hex(BG_TERTIARY), 0);
    lv_obj_set_style_radius(gps_btn, 0, 0);
    lv_obj_set_style_border_width(gps_btn, 1, 0);
    lv_obj_set_style_border_color(gps_btn, lv_color_hex(ACCENT), 0);
    lv_obj_t* gps = lv_label_create(gps_btn);
    lv_label_set_text(gps, "Use GPS");
    lv_obj_set_style_text_color(gps, lv_color_hex(ACCENT), 0);
    lv_obj_set_style_text_font(gps, emoji_wrapped_montserrat_10, 0);
    lv_obj_center(gps);
    lv_obj_add_event_cb(gps_btn, [](lv_event_t*) {
        center_on_gps();
    }, LV_EVENT_CLICKED, nullptr);

    lv_obj_t* dl_btn = lv_btn_create(scr);
    lv_obj_set_size(dl_btn, 36, 28);
    lv_obj_align(dl_btn, LV_ALIGN_BOTTOM_LEFT, 84, -(BOT_BAR_H + DIVIDER_H + 8));
    lv_obj_set_style_bg_color(dl_btn, lv_color_hex(BG_TERTIARY), 0);
    lv_obj_set_style_radius(dl_btn, 0, 0);
    lv_obj_set_style_border_width(dl_btn, 1, 0);
    lv_obj_set_style_border_color(dl_btn, lv_color_hex(ACCENT), 0);
    lv_obj_t* dl = lv_label_create(dl_btn);
    lv_label_set_text(dl, LV_SYMBOL_DOWNLOAD);
    lv_obj_set_style_text_color(dl, lv_color_hex(ACCENT), 0);
    lv_obj_set_style_text_font(dl, emoji_wrapped_montserrat_12, 0);
    lv_obj_center(dl);
    lv_obj_add_event_cb(dl_btn, [](lv_event_t*) {
        start_visible_tile_download();
    }, LV_EVENT_CLICKED, nullptr);

    g_map_status_label = lv_label_create(scr);
    lv_label_set_text(g_map_status_label, sigurdos_map_tiles_available()
        ? "Tiles ready"
        : "No local tiles");
    lv_obj_set_style_text_color(g_map_status_label, lv_color_hex(TEXT_SECONDARY), 0);
    lv_obj_set_style_text_font(g_map_status_label, emoji_wrapped_montserrat_10, 0);
    lv_label_set_long_mode(g_map_status_label, LV_LABEL_LONG_DOT);
    lv_obj_set_width(g_map_status_label, DISPLAY_W - 168);
    lv_obj_align(g_map_status_label, LV_ALIGN_BOTTOM_LEFT, 128,
                 -(BOT_BAR_H + DIVIDER_H + 14));

    if (!sigurdos_gps_has_fix() && !sigurdos::prefs_get().map_location_valid &&
        !g_prompted_manual_location) {
        show_manual_location_dialog(scr);
    }

    (void)zoom_y_base;
    show_screen(scr);
    if (g_download_status_timer) {
        lv_timer_del(g_download_status_timer);
        g_download_status_timer = nullptr;
    }
    g_download_status_timer = lv_timer_create([](lv_timer_t*) {
        poll_tile_download_status();
    }, 500, nullptr);
    lv_timer_create([](lv_timer_t* t) {
        sigurdos_map_render();
        lv_timer_del(t);
    }, 250, nullptr);
}

} // namespace sigurdos::ui
