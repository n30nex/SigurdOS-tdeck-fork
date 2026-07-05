// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2025 Ben

#pragma once

#include <cstdint>

namespace sigurdos::ui {

static constexpr uint32_t REPEATER_LOGIN_POLL_INTERVAL_MS = 2000;
static constexpr uint16_t REPEATER_LOGIN_POLL_MAX_PENDING_POLLS = 8;
static constexpr uint32_t REPEATER_MANAGEMENT_REQUEST_POLL_MS = 1000;
static constexpr uint32_t REPEATER_MANAGEMENT_REQUEST_TIMEOUT_MS = 120000;

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

inline bool room_admin_password_login_supported()
{
    return false;
}

inline const char* room_admin_password_login_unsupported_message()
{
    return "! Room admin login is not supported yet";
}

inline uint8_t login_contact_type_from_hint(uint8_t contact_type_hint,
                                            uint8_t live_contact_type)
{
    return contact_type_hint != 0 ? contact_type_hint : live_contact_type;
}

inline bool login_contact_type_is_room(uint8_t contact_type)
{
    // MeshCore advert type 3 is room-server. Keep this helper local so the UI
    // state policy does not need to pull in the full mesh wrapper.
    return contact_type == 3;
}

inline bool login_submit_is_blank_room_guest(uint8_t contact_type,
                                             const char* password)
{
    return login_contact_type_is_room(contact_type) && password &&
           password[0] == '\0';
}

inline bool login_submit_room_admin_fails_closed(uint8_t contact_type,
                                                 const char* password)
{
    return login_contact_type_is_room(contact_type) && password &&
           password[0] != '\0' &&
           !room_admin_password_login_supported();
}

inline bool login_submit_sends_network_login(uint8_t contact_type,
                                             const char* password)
{
    if (login_submit_is_blank_room_guest(contact_type, password)) return false;
    if (login_submit_room_admin_fails_closed(contact_type, password)) return false;
    return true;
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

inline bool repeater_detail_pending_refresh_should_keep_polling(uint8_t login_status)
{
    return login_status == 1;
}

inline uint32_t login_poll_timeout_ms()
{
    return REPEATER_LOGIN_POLL_INTERVAL_MS *
           static_cast<uint32_t>(REPEATER_LOGIN_POLL_MAX_PENDING_POLLS);
}

inline bool repeater_management_request_timed_out(uint32_t now_ms,
                                                  uint32_t started_at_ms)
{
    return (uint32_t)(now_ms - started_at_ms) >=
           REPEATER_MANAGEMENT_REQUEST_TIMEOUT_MS;
}

inline uint32_t repeater_management_request_remaining_secs(uint32_t now_ms,
                                                           uint32_t started_at_ms)
{
    const uint32_t elapsed = now_ms - started_at_ms;
    if (elapsed >= REPEATER_MANAGEMENT_REQUEST_TIMEOUT_MS) return 0;
    const uint32_t remaining_ms =
        REPEATER_MANAGEMENT_REQUEST_TIMEOUT_MS - elapsed;
    return (remaining_ms + 999u) / 1000u;
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
