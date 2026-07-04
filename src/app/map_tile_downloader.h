#pragma once

// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2025 Ben
//
// Current-view map tile downloader. This deliberately caches only the
// visible map neighborhood, not arbitrary offline areas.

#include <cstdint>

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
const char* sigurdos_map_tile_download_provider();
