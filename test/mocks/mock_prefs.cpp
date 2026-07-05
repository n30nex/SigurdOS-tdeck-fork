// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2025 Ben
//
// Mock prefs implementation for native test environment.
// Provides stub implementations so keyboard.cpp (which includes prefs.h)
// can compile and link without real NVS (Preferences) hardware.

#include "hal/prefs.h"

#include <cstring>

namespace sigurdos {

static NodePrefs g_prefs;
static constexpr int MAX_SAVED_REPEATER_PWS = 8;

struct SavedRepeaterPassword {
    char name[32];
    char password[64];
};

static SavedRepeaterPassword g_repeater_passwords[MAX_SAVED_REPEATER_PWS];
static int g_repeater_password_count = 0;
static char g_map_tile_provider[MAP_TILE_PROVIDER_MAX_LEN] = {};

struct PrefDefaults {
    PrefDefaults() { g_prefs.set_defaults(); }
};

static PrefDefaults g_defaults;

bool prefs_load(NodePrefs& p) {
    p = g_prefs;
    return true;
}

bool prefs_save(const NodePrefs& p) {
    g_prefs = p;
    g_prefs.tx_power_dbm = prefs_normalize_tx_power_dbm(g_prefs.tx_power_dbm);
    return true;
}

bool prefs_exists() {
    return true;
}

const NodePrefs& prefs_get() {
    return g_prefs;
}

void prefs_set(const NodePrefs& p) {
    g_prefs = p;
    g_prefs.tx_power_dbm = prefs_normalize_tx_power_dbm(g_prefs.tx_power_dbm);
}

bool saveRepeaterPassword(const char* name, const char* password) {
    if (!name || !name[0] || !password) return false;

    int slot = -1;
    for (int i = 0; i < g_repeater_password_count; i++) {
        if (std::strncmp(g_repeater_passwords[i].name, name,
                         sizeof(g_repeater_passwords[i].name)) == 0) {
            slot = i;
            break;
        }
    }

    if (slot < 0) {
        if (g_repeater_password_count >= MAX_SAVED_REPEATER_PWS) return false;
        slot = g_repeater_password_count++;
    }

    std::strncpy(g_repeater_passwords[slot].name, name,
                 sizeof(g_repeater_passwords[slot].name) - 1);
    g_repeater_passwords[slot].name[sizeof(g_repeater_passwords[slot].name) - 1] = '\0';
    std::strncpy(g_repeater_passwords[slot].password, password,
                 sizeof(g_repeater_passwords[slot].password) - 1);
    g_repeater_passwords[slot].password[sizeof(g_repeater_passwords[slot].password) - 1] = '\0';
    return true;
}

bool loadRepeaterPassword(const char* name, char* password, size_t max_len) {
    if (!name || !name[0] || !password || max_len == 0) return false;
    for (int i = 0; i < g_repeater_password_count; i++) {
        if (std::strncmp(g_repeater_passwords[i].name, name,
                         sizeof(g_repeater_passwords[i].name)) == 0) {
            std::strncpy(password, g_repeater_passwords[i].password, max_len - 1);
            password[max_len - 1] = '\0';
            return password[0] != '\0';
        }
    }
    password[0] = '\0';
    return false;
}

void removeRepeaterPassword(const char* name) {
    if (!name || !name[0]) return;
    for (int i = 0; i < g_repeater_password_count; i++) {
        if (std::strncmp(g_repeater_passwords[i].name, name,
                         sizeof(g_repeater_passwords[i].name)) == 0) {
            for (int j = i; j + 1 < g_repeater_password_count; j++) {
                g_repeater_passwords[j] = g_repeater_passwords[j + 1];
            }
            g_repeater_password_count--;
            if (g_repeater_password_count >= 0) {
                g_repeater_passwords[g_repeater_password_count] = {};
            }
            return;
        }
    }
}

static bool normalizeMapTileProviderUrl(const char* provider_url,
                                        char* out,
                                        size_t out_size) {
    if (!provider_url || !out || out_size == 0) return false;
    out[0] = '\0';

    while (*provider_url == ' ' || *provider_url == '\t' ||
           *provider_url == '\r' || *provider_url == '\n') {
        provider_url++;
    }

    size_t len = std::strlen(provider_url);
    while (len > 0 &&
           (provider_url[len - 1] == ' ' || provider_url[len - 1] == '\t' ||
            provider_url[len - 1] == '\r' || provider_url[len - 1] == '\n')) {
        len--;
    }

    while (len > 8 && provider_url[len - 1] == '/') {
        len--;
    }

    if (len == 0 || len >= out_size) return false;
    std::memcpy(out, provider_url, len);
    out[len] = '\0';
    return true;
}

bool mapTileProviderUrlValid(const char* provider_url) {
    if (!provider_url || !provider_url[0]) return false;

    const size_t len = std::strlen(provider_url);
    if (len >= MAP_TILE_PROVIDER_MAX_LEN) return false;

    const bool http =
        std::strncmp(provider_url, "http://", 7) == 0 ||
        std::strncmp(provider_url, "https://", 8) == 0;
    if (!http) return false;

    for (const char* p = provider_url; *p; ++p) {
        const unsigned char c = static_cast<unsigned char>(*p);
        if (c <= ' ' || c == '"' || c == '\'' || c == '<' || c == '>') {
            return false;
        }
    }
    return true;
}

bool saveMapTileProvider(const char* provider_url) {
    char normalized[MAP_TILE_PROVIDER_MAX_LEN] = {};
    if (!normalizeMapTileProviderUrl(provider_url, normalized, sizeof(normalized)) ||
        !mapTileProviderUrlValid(normalized)) {
        return false;
    }
    std::strncpy(g_map_tile_provider, normalized, sizeof(g_map_tile_provider) - 1);
    g_map_tile_provider[sizeof(g_map_tile_provider) - 1] = '\0';
    return true;
}

bool loadMapTileProvider(char* provider_url, size_t max_len) {
    if (!provider_url || max_len == 0) return false;
    provider_url[0] = '\0';
    if (!g_map_tile_provider[0] ||
        !mapTileProviderUrlValid(g_map_tile_provider) ||
        std::strlen(g_map_tile_provider) >= max_len) {
        return false;
    }
    std::strncpy(provider_url, g_map_tile_provider, max_len - 1);
    provider_url[max_len - 1] = '\0';
    return true;
}

bool clearMapTileProvider() {
    g_map_tile_provider[0] = '\0';
    return true;
}

} // namespace sigurdos
