// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2025 Ben
//
// Mock prefs implementation for native test environment.
// Provides stub implementations so keyboard.cpp (which includes prefs.h)
// can compile and link without real NVS (Preferences) hardware.

#include "hal/prefs.h"

namespace sigurdos {

static NodePrefs g_prefs;
static constexpr int MAX_SAVED_REPEATER_PWS = 8;

struct SavedRepeaterPassword {
    char name[32];
    char password[64];
};

static SavedRepeaterPassword g_repeater_passwords[MAX_SAVED_REPEATER_PWS];
static int g_repeater_password_count = 0;

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

} // namespace sigurdos
