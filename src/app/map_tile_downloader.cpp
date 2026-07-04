// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2025 Ben
//
// Current-view raster tile cache for the map screen.

#include "map_tile_downloader.h"

#include "map_renderer.h"
#include "../hal/prefs.h"
#include "../hal/sdcard.h"
#include "../hal/wifi_ota.h"

#include <cstdio>
#include <cstring>

#if defined(ESP32_PLATFORM)
#include <HTTPClient.h>
#include <WiFiClientSecure.h>
#include <cerrno>
#include <sys/stat.h>
#include <unistd.h>
#include <Arduino.h>
#endif

static constexpr const char* TILE_PROVIDER =
    "https://tile.openstreetmap.org";
static constexpr const char* TILE_USER_AGENT =
    "SigurdOS-TDeck/1.0 (+https://github.com/hermes-gadget/SigurdOS-tdeck)";
static constexpr int TILE_HTTP_TIMEOUT_MS = 15000;
static constexpr int TILE_MAX_BYTES = 196 * 1024;
static constexpr int TILE_READ_TIMEOUT_MS = 20000;

const char* sigurdos_map_tile_download_provider()
{
    return TILE_PROVIDER;
}

static void set_status(SigurdosMapTileDownloadStatus* out,
                       int requested, int downloaded, int skipped, int failed,
                       const char* message,
                       bool running = false,
                       bool complete = false)
{
    if (!out) return;
    out->requested = requested;
    out->downloaded = downloaded;
    out->skipped = skipped;
    out->failed = failed;
    out->running = running;
    out->complete = complete;
    if (message) {
        std::snprintf(out->message, sizeof(out->message), "%s", message);
    } else {
        out->message[0] = '\0';
    }
}

#if defined(ESP32_PLATFORM)

#include <freertos/FreeRTOS.h>
#include <freertos/task.h>

struct TilePlan {
    int z;
    int x[9];
    int y[9];
    int count;
};

static portMUX_TYPE s_status_mux = portMUX_INITIALIZER_UNLOCKED;
static SigurdosMapTileDownloadStatus s_async_status = {
    0, 0, 0, 0, false, false, ""
};
static TilePlan s_async_plan = {};
static TaskHandle_t s_async_task = nullptr;

static void publish_status(const SigurdosMapTileDownloadStatus& status)
{
    portENTER_CRITICAL(&s_status_mux);
    s_async_status = status;
    portEXIT_CRITICAL(&s_status_mux);
}

static void publish_message(SigurdosMapTileDownloadStatus& status,
                            const char* message,
                            bool running,
                            bool complete)
{
    status.running = running;
    status.complete = complete;
    if (message) {
        std::snprintf(status.message, sizeof(status.message), "%s", message);
    }
    publish_status(status);
}

static bool ensure_dir(const char* path)
{
    if (!path || !path[0]) return false;
    if (::mkdir(path, 0775) == 0) return true;
    return errno == EEXIST;
}

static bool ensure_tile_dirs(int z, int x)
{
    char path[80];
    if (!ensure_dir(SIGURDOS_SD_MOUNTPOINT "/tiles")) return false;

    std::snprintf(path, sizeof(path), SIGURDOS_SD_MOUNTPOINT "/tiles/%d", z);
    if (!ensure_dir(path)) return false;

    std::snprintf(path, sizeof(path), SIGURDOS_SD_MOUNTPOINT "/tiles/%d/%d", z, x);
    return ensure_dir(path);
}

static bool remove_partial(const char* path)
{
    if (!path || !path[0]) return false;
    return std::remove(path) == 0;
}

static bool download_one_tile(int z, int x, int y, SigurdosMapTileDownloadStatus* out)
{
    char path[96];
    std::snprintf(path, sizeof(path), SIGURDOS_SD_MOUNTPOINT "/tiles/%d/%d/%d.png",
                  z, x, y);
    char tmp_path[104];
    std::snprintf(tmp_path, sizeof(tmp_path), "%s.tmp", path);

    if (sigurdos_sdcard_exists(path)) {
        if (out) out->skipped++;
        return true;
    }

    if (!ensure_tile_dirs(z, x)) {
        if (out) out->failed++;
        return false;
    }

    char url[128];
    std::snprintf(url, sizeof(url), "%s/%d/%d/%d.png", TILE_PROVIDER, z, x, y);

    WiFiClientSecure client;
    client.setInsecure();

    HTTPClient http;
    http.setFollowRedirects(HTTPC_STRICT_FOLLOW_REDIRECTS);
    http.setTimeout(TILE_HTTP_TIMEOUT_MS);
    http.setUserAgent(TILE_USER_AGENT);

    if (!http.begin(client, url)) {
        if (out) out->failed++;
        return false;
    }

    int code = http.GET();
    if (code != HTTP_CODE_OK) {
        http.end();
        if (out) out->failed++;
        return false;
    }

    int total = http.getSize();
    if (total <= 0 || total > TILE_MAX_BYTES) {
        http.end();
        if (out) out->failed++;
        return false;
    }

    remove_partial(tmp_path);
    FILE* f = std::fopen(tmp_path, "wb");
    if (!f) {
        http.end();
        if (out) out->failed++;
        return false;
    }

    WiFiClient* stream = http.getStreamPtr();
    uint8_t buf[1024];
    int written_total = 0;
    uint32_t last_progress = millis();

    while (written_total < total) {
        int available = stream ? stream->available() : 0;
        if (available <= 0) {
            if (!stream || !stream->connected() ||
                (millis() - last_progress) > TILE_READ_TIMEOUT_MS) {
                break;
            }
            delay(10);
            continue;
        }

        int want = total - written_total;
        if (want > (int)sizeof(buf)) want = sizeof(buf);
        if (want > available) want = available;

        size_t got = stream->readBytes(buf, (size_t)want);
        if (got == 0) break;

        size_t wrote = std::fwrite(buf, 1, got, f);
        if (wrote != got) break;

        written_total += (int)got;
        last_progress = millis();
    }

    std::fclose(f);
    http.end();

    if (written_total != total) {
        remove_partial(tmp_path);
        if (out) out->failed++;
        return false;
    }

    remove_partial(path);
    if (std::rename(tmp_path, path) != 0) {
        remove_partial(tmp_path);
        if (out) out->failed++;
        return false;
    }

    if (out) out->downloaded++;
    return true;
}

static bool prepare_current_view_plan(TilePlan* plan, SigurdosMapTileDownloadStatus* out)
{
    if (!plan) {
        set_status(out, 0, 0, 0, 0, "Map plan unavailable");
        return false;
    }
    *plan = {};

    const int z = sigurdos_map_get_zoom();
    if (!sigurdos_map_zoom_valid(z)) {
        set_status(out, 0, 0, 0, 0, "Map zoom invalid");
        return false;
    }

    const int n = sigurdos_map_tiles_per_axis(z);
    const int center_x = (int)sigurdos_map_lon_to_tile_x(sigurdos_map_get_lon(), z);
    const int center_y = (int)sigurdos_map_lat_to_tile_y(sigurdos_map_get_lat(), z);
    plan->z = z;

    for (int y = center_y - 1; y <= center_y + 1; ++y) {
        if (y < 0 || y >= n) continue;
        for (int x = center_x - 1; x <= center_x + 1; ++x) {
            if (x < 0 || x >= n) continue;
            if (plan->count >= 9) continue;
            plan->x[plan->count] = x;
            plan->y[plan->count] = y;
            plan->count++;
        }
    }

    if (plan->count <= 0) {
        set_status(out, 0, 0, 0, 0, "No visible tiles");
        return false;
    }

    set_status(out, plan->count, 0, 0, 0, "Tiles queued");
    return true;
}

static bool ensure_download_prereqs(SigurdosMapTileDownloadStatus* out)
{
    if (!sigurdos_sdcard_mounted() && !sigurdos_sdcard_retry()) {
        set_status(out, 0, 0, 0, 0, "SD card not ready");
        return false;
    }

    if (!sigurdos::wifi_sta::isConnected()) {
        const sigurdos::NodePrefs& p = sigurdos::prefs_get();
        if (p.wifi_ssid[0] &&
            sigurdos::wifi_sta::getStatus() != sigurdos::wifi_sta::Status::Connecting) {
            sigurdos::wifi_sta::beginConnect(p.wifi_ssid, p.wifi_password);
        }
        set_status(out, 0, 0, 0, 0, "WiFi connecting; retry soon");
        return false;
    }
    return true;
}

bool sigurdos_map_download_current_view_tiles(SigurdosMapTileDownloadStatus* out)
{
    set_status(out, 0, 0, 0, 0, "");

    if (!ensure_download_prereqs(out)) {
        return false;
    }

    TilePlan plan;
    if (!prepare_current_view_plan(&plan, out)) {
        return false;
    }

    if (out) out->requested = plan.count;
    for (int i = 0; i < plan.count; ++i) {
        download_one_tile(plan.z, plan.x[i], plan.y[i], out);
    }

    char msg[96];
    std::snprintf(msg, sizeof(msg), "Tiles: %d new, %d cached, %d failed",
                  out ? out->downloaded : 0,
                  out ? out->skipped : 0,
                  out ? out->failed : 0);
    if (out) std::snprintf(out->message, sizeof(out->message), "%s", msg);
    return out ? (out->requested > 0 && out->failed == 0) : true;
}

static void tile_download_task(void*)
{
    TilePlan plan = s_async_plan;
    SigurdosMapTileDownloadStatus status;
    set_status(&status, plan.count, 0, 0, 0, "Downloading tiles...", true, false);
    publish_status(status);

    if (!ensure_download_prereqs(&status)) {
        status.running = false;
        status.complete = true;
        publish_status(status);
        s_async_task = nullptr;
        vTaskDelete(nullptr);
        return;
    }

    for (int i = 0; i < plan.count; ++i) {
        char msg[96];
        std::snprintf(msg, sizeof(msg), "Tile %d/%d...", i + 1, plan.count);
        publish_message(status, msg, true, false);
        download_one_tile(plan.z, plan.x[i], plan.y[i], &status);
        const int done = status.downloaded + status.skipped + status.failed;
        std::snprintf(msg, sizeof(msg), "Tiles %d/%d: %d new, %d cached, %d failed",
                      done, status.requested, status.downloaded,
                      status.skipped, status.failed);
        publish_message(status, msg, true, false);
        vTaskDelay(pdMS_TO_TICKS(60));
    }

    char msg[96];
    std::snprintf(msg, sizeof(msg), "Tiles: %d new, %d cached, %d failed",
                  status.downloaded, status.skipped, status.failed);
    publish_message(status, msg, false, true);
    s_async_task = nullptr;
    vTaskDelete(nullptr);
}

bool sigurdos_map_tile_download_start_current_view()
{
    if (sigurdos_map_tile_download_is_running()) {
        return false;
    }

    SigurdosMapTileDownloadStatus status;
    if (!prepare_current_view_plan(&s_async_plan, &status)) {
        status.running = false;
        status.complete = true;
        publish_status(status);
        return false;
    }

    status.running = true;
    status.complete = false;
    std::snprintf(status.message, sizeof(status.message), "Queued %d tiles",
                  status.requested);
    publish_status(status);

    BaseType_t ok = xTaskCreatePinnedToCore(tile_download_task, "map_tiles",
                                            8192, nullptr, 1,
                                            &s_async_task, 0);
    if (ok != pdPASS) {
        set_status(&status, s_async_plan.count, 0, 0, 0,
                   "Tile task failed", false, true);
        publish_status(status);
        s_async_task = nullptr;
        return false;
    }
    return true;
}

void sigurdos_map_tile_download_get_status(SigurdosMapTileDownloadStatus* out)
{
    if (!out) return;
    portENTER_CRITICAL(&s_status_mux);
    *out = s_async_status;
    portEXIT_CRITICAL(&s_status_mux);
}

bool sigurdos_map_tile_download_is_running()
{
    portENTER_CRITICAL(&s_status_mux);
    bool running = s_async_status.running;
    portEXIT_CRITICAL(&s_status_mux);
    return running;
}

#else

bool sigurdos_map_download_current_view_tiles(SigurdosMapTileDownloadStatus* out)
{
    set_status(out, 0, 0, 0, 0, "Tile download unavailable in native tests");
    return false;
}

bool sigurdos_map_tile_download_start_current_view()
{
    return false;
}

void sigurdos_map_tile_download_get_status(SigurdosMapTileDownloadStatus* out)
{
    set_status(out, 0, 0, 0, 0, "Tile download unavailable in native tests");
}

bool sigurdos_map_tile_download_is_running()
{
    return false;
}

#endif
