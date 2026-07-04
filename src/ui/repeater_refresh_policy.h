// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2025 Ben

#pragma once

#include <cstdint>

namespace sigurdos::ui {

enum class RepeaterManagementRequest : uint8_t {
    Status,
    Telemetry,
};

inline bool repeater_refresh_allowed(bool has_state,
                                     bool screen_valid,
                                     bool screen_current,
                                     bool screen_active,
                                     bool detail_open)
{
    return has_state && screen_valid && screen_current && screen_active && !detail_open;
}

inline const char* repeater_management_request_label(RepeaterManagementRequest request)
{
    switch (request) {
    case RepeaterManagementRequest::Status:    return "Status";
    case RepeaterManagementRequest::Telemetry: return "Telemetry";
    }
    return "";
}

inline const char* repeater_management_request_failed_message(RepeaterManagementRequest request)
{
    switch (request) {
    case RepeaterManagementRequest::Status:    return "! Status request failed";
    case RepeaterManagementRequest::Telemetry: return "! Telemetry request failed";
    }
    return "! Request failed";
}

} // namespace sigurdos::ui
