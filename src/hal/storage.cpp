// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Ben

#include "storage.h"

#if defined(ESP32_PLATFORM)
#include <Arduino.h>
#include <SPIFFS.h>
#include <esp_err.h>
#include <esp_partition.h>
#endif

namespace sigurdos {
namespace hal {

namespace {

bool g_mount_attempted = false;
bool g_storage_available = false;

#if defined(ESP32_PLATFORM)

bool spiffs_partition_is_erased()
{
    esp_partition_iterator_t it = esp_partition_find(
        ESP_PARTITION_TYPE_DATA,
        static_cast<esp_partition_subtype_t>(ESP_PARTITION_SUBTYPE_DATA_SPIFFS),
        nullptr);
    if (!it) return false;

    const esp_partition_t* partition = esp_partition_get(it);
    if (!partition || partition->size == 0) {
        esp_partition_iterator_release(it);
        return false;
    }

    uint8_t buf[512];
    for (uint32_t offset = 0; offset < partition->size; offset += sizeof(buf)) {
        const size_t to_read = (partition->size - offset < sizeof(buf))
            ? static_cast<size_t>(partition->size - offset)
            : sizeof(buf);
        if (esp_partition_read(partition, offset, buf, to_read) != ESP_OK) {
            esp_partition_iterator_release(it);
            return false;
        }
        if (!detail::buffer_is_erased(buf, to_read)) {
            esp_partition_iterator_release(it);
            return false;
        }
    }

    esp_partition_iterator_release(it);
    return true;
}

void record_mount(bool available)
{
    g_mount_attempted = true;
    g_storage_available = available;
}

#endif

} // namespace

namespace detail {

bool buffer_is_erased(const uint8_t* data, size_t len)
{
    if (!data && len > 0) return false;
    for (size_t i = 0; i < len; ++i) {
        if (data[i] != 0xFF) return false;
    }
    return true;
}

} // namespace detail

StorageMountResult storage_begin()
{
#if defined(ESP32_PLATFORM)
    if (SPIFFS.begin(false)) {
        record_mount(true);
        return StorageMountResult::Mounted;
    }

    if (spiffs_partition_is_erased()) {
        Serial.println("[boot] SPIFFS partition is erased — formatting for first boot");
        SPIFFS.end();
        if (SPIFFS.format() && SPIFFS.begin(false)) {
            record_mount(true);
            return StorageMountResult::Formatted;
        }
        Serial.println("[boot] WARNING: SPIFFS first-boot format failed");
    }

    record_mount(false);
    return StorageMountResult::Unavailable;
#else
    g_mount_attempted = true;
    g_storage_available = true;
    return StorageMountResult::Mounted;
#endif
}

bool storage_ensure_mounted()
{
#if defined(ESP32_PLATFORM)
    if (g_mount_attempted && !g_storage_available) return false;
    return storage_begin() != StorageMountResult::Unavailable;
#else
    g_mount_attempted = true;
    g_storage_available = true;
    return true;
#endif
}

bool storage_available()
{
    return g_storage_available;
}

void storage_reset_state()
{
    g_mount_attempted = false;
    g_storage_available = false;
}

} // namespace hal
} // namespace sigurdos
