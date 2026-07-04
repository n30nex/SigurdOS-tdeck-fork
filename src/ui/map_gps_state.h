// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2025 Ben

#pragma once

#include <stdint.h>
#include "theme.h"

namespace sigurdos::ui {

enum class MapGpsButtonState : uint8_t {
    Off,
    Finding,
    Locked,
};

inline MapGpsButtonState map_gps_button_state(bool gps_enabled, bool has_fix)
{
    if (has_fix) return MapGpsButtonState::Locked;
    return gps_enabled ? MapGpsButtonState::Finding : MapGpsButtonState::Off;
}

inline const char* map_gps_button_text(MapGpsButtonState state)
{
    switch (state) {
    case MapGpsButtonState::Locked:  return "GPS Lock";
    case MapGpsButtonState::Finding: return "Finding Sats";
    case MapGpsButtonState::Off:
    default:                         return "Use GPS";
    }
}

inline uint32_t map_gps_button_color(MapGpsButtonState state)
{
    switch (state) {
    case MapGpsButtonState::Locked:  return theme::ACCENT_GREEN;
    case MapGpsButtonState::Finding: return theme::ACCENT_YELLOW;
    case MapGpsButtonState::Off:
    default:                         return theme::ACCENT;
    }
}

} // namespace sigurdos::ui
