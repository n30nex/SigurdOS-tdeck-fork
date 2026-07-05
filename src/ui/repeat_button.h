#pragma once

// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Ben

#include <cstdint>
#include <lvgl.h>

namespace sigurdos::ui {

using RepeatButtonAction = void (*)(void* user_data);

void attach_hold_repeat(lv_obj_t* button,
                        RepeatButtonAction action,
                        void* user_data,
                        uint16_t initial_delay_ms = 420,
                        uint16_t first_period_ms = 180,
                        uint16_t min_period_ms = 55);

} // namespace sigurdos::ui
