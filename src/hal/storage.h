// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Ben

#pragma once

#include <cstddef>
#include <cstdint>

namespace sigurdos {
namespace hal {

enum class StorageMountResult {
    Mounted,
    Formatted,
    Unavailable,
};

/// Mount SPIFFS for normal boot. This never formats existing data. If the
/// SPIFFS partition is completely erased (fresh full-flash install), it formats
/// once and remounts so first boot has persistent storage.
StorageMountResult storage_begin();

/// Ensure SPIFFS is mounted for subsystems that are called after boot. If boot
/// already proved storage unavailable, this returns false without retry spam.
bool storage_ensure_mounted();

/// Last known storage availability. False until storage_begin() succeeds.
bool storage_available();

/// Reset cached mount state after an explicit SPIFFS.end()/format path.
void storage_reset_state();

namespace detail {

bool buffer_is_erased(const uint8_t* data, size_t len);

} // namespace detail
} // namespace hal
} // namespace sigurdos
