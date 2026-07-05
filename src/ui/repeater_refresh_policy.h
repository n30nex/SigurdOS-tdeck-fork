// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2025 Ben

#pragma once

#include <cstdint>

namespace sigurdos::ui {

static constexpr uint32_t REPEATER_LOGIN_POLL_INTERVAL_MS = 2000;
static constexpr uint16_t REPEATER_LOGIN_POLL_MAX_PENDING_POLLS = 8;

enum class RepeaterManagementRequest : uint8_t {
    Status,
    Telemetry,
    Neighbours,
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
    case RepeaterManagementRequest::Neighbours: return "Neighbours";
    }
    return "";
}

inline const char* repeater_management_request_failed_message(RepeaterManagementRequest request)
{
    switch (request) {
    case RepeaterManagementRequest::Status:    return "! Status request failed";
    case RepeaterManagementRequest::Telemetry: return "! Telemetry request failed";
    case RepeaterManagementRequest::Neighbours: return "! Neighbours request failed";
    }
    return "! Request failed";
}

inline bool login_detail_refresh_after_submit(bool send_attempted,
                                              bool blank_room_guest_login)
{
    (void)send_attempted;
    return !blank_room_guest_login;
}

inline bool login_submit_starts_poll_timer(bool sent,
                                           bool blank_room_guest_login)
{
    return sent && !blank_room_guest_login;
}

inline bool login_detail_refresh_allowed(bool detail_open_for_contact,
                                         bool screen_still_current)
{
    return detail_open_for_contact && screen_still_current;
}

inline bool login_poll_timed_out(uint16_t pending_polls)
{
    return pending_polls >= REPEATER_LOGIN_POLL_MAX_PENDING_POLLS;
}

inline uint32_t login_poll_timeout_ms()
{
    return REPEATER_LOGIN_POLL_INTERVAL_MS *
           static_cast<uint32_t>(REPEATER_LOGIN_POLL_MAX_PENDING_POLLS);
}

inline bool repeater_show_admin_management_rows(bool is_admin)
{
    return is_admin;
}

inline bool repeater_show_admin_radio_rows(bool /*is_admin*/)
{
    return false;
}

inline bool repeater_show_admin_password_rows(bool /*is_admin*/)
{
    return false;
}

} // namespace sigurdos::ui
