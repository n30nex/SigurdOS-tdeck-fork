#pragma once

// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Ben

#include <cstring>

namespace sigurdos::mesh {

static constexpr char PUBLIC_CHANNEL_NAME[] = "Public";
static constexpr char PUBLIC_CHANNEL_PSK_BASE64[] = "izOH6cXN6mrJ5e26oRXNcg==";

inline bool isPublicChannelName(const char* name)
{
    return name && std::strcmp(name, PUBLIC_CHANNEL_NAME) == 0;
}

inline bool isReservedPublicHashtagName(const char* name)
{
    if (!name) return false;
    while (*name == ' ' || *name == '\t') ++name;
    if (*name == '#') ++name;
    const char* end = name + std::strlen(name);
    while (end > name && (end[-1] == ' ' || end[-1] == '\t' ||
                          end[-1] == '\r' || end[-1] == '\n')) {
        --end;
    }
    const size_t len = (size_t)(end - name);
    return len == 6 &&
           (name[0] == 'P' || name[0] == 'p') &&
           (name[1] == 'U' || name[1] == 'u') &&
           (name[2] == 'B' || name[2] == 'b') &&
           (name[3] == 'L' || name[3] == 'l') &&
           (name[4] == 'I' || name[4] == 'i') &&
           (name[5] == 'C' || name[5] == 'c');
}

} // namespace sigurdos::mesh
