// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2025 Ben

#pragma once

namespace sigurdos::ui {

inline bool repeater_refresh_allowed(bool has_state,
                                     bool screen_valid,
                                     bool screen_current,
                                     bool screen_active,
                                     bool detail_open)
{
    return has_state && screen_valid && screen_current && screen_active && !detail_open;
}

} // namespace sigurdos::ui
