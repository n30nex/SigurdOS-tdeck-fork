// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2025 Ben

#pragma once

#include <cstdint>

namespace sigurdos {

static constexpr uint32_t SIGURDOS_VALID_TIME_MIN_EPOCH = 1700000000UL;
static constexpr uint32_t SIGURDOS_NTP_RETRY_INTERVAL_MS = 60000UL;
static constexpr uint32_t SIGURDOS_NTP_RESYNC_INTERVAL_MS = 21600000UL;

inline bool time_epoch_is_sane(uint32_t epoch)
{
    return epoch >= SIGURDOS_VALID_TIME_MIN_EPOCH;
}

inline bool ntp_retry_due(bool ntp_started, uint32_t now_ms, uint32_t last_start_ms)
{
    return !ntp_started ||
           (uint32_t)(now_ms - last_start_ms) >= SIGURDOS_NTP_RETRY_INTERVAL_MS;
}

inline bool ntp_resync_due(uint32_t now_ms, uint32_t last_sync_ms)
{
    return last_sync_ms == 0 ||
           (uint32_t)(now_ms - last_sync_ms) >= SIGURDOS_NTP_RESYNC_INTERVAL_MS;
}

inline bool onboarding_manual_time_needed(uint32_t current_epoch)
{
    return !time_epoch_is_sane(current_epoch);
}

} // namespace sigurdos
