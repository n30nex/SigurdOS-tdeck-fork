#pragma once

// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2025 Ben
//
// Current-view map tile downloader. This deliberately caches only the
// visible map neighborhood, not arbitrary offline areas.

#include <cstdint>

static constexpr int SIGURDOS_MAP_TILE_CURRENT_VIEW_RADIUS = 1;
static constexpr int SIGURDOS_MAP_TILE_MAX_CURRENT_VIEW_TILES =
    (SIGURDOS_MAP_TILE_CURRENT_VIEW_RADIUS * 2 + 1) *
    (SIGURDOS_MAP_TILE_CURRENT_VIEW_RADIUS * 2 + 1);
static constexpr const char* SIGURDOS_MAP_TILE_PROVIDER =
    "https://tile.openstreetmap.org";
static constexpr const char* SIGURDOS_MAP_TILE_USER_AGENT =
    "SigurdOS-TDeck/1.0 (+https://github.com/hermes-gadget/SigurdOS-tdeck)";
static constexpr const char* SIGURDOS_MAP_TILE_ATTRIBUTION =
    "\xc2\xa9 OpenStreetMap";

struct SigurdosMapTileDownloadStatus {
    int requested;
    int downloaded;
    int skipped;
    int failed;
    bool running;
    bool complete;
    char message[96];
};

bool sigurdos_map_download_current_view_tiles(SigurdosMapTileDownloadStatus* out);
bool sigurdos_map_tile_download_start_current_view();
void sigurdos_map_tile_download_get_status(SigurdosMapTileDownloadStatus* out);
bool sigurdos_map_tile_download_is_running();

inline const char* sigurdos_map_tile_download_provider()
{
    return SIGURDOS_MAP_TILE_PROVIDER;
}

inline const char* sigurdos_map_tile_download_user_agent()
{
    return SIGURDOS_MAP_TILE_USER_AGENT;
}

inline const char* sigurdos_map_tile_download_attribution()
{
    return SIGURDOS_MAP_TILE_ATTRIBUTION;
}

inline bool sigurdos_map_tile_download_prefetch_allowed()
{
    return false;
}
