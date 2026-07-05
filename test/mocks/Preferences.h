#pragma once
// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Ben
//
// Native mock for ESP32 Preferences NVS used by persistence_store tests.

#include <cstddef>
#include <cstdint>
#include <cstring>
#include <map>
#include <string>
#include <vector>

class Preferences {
public:
    bool begin(const char*, bool read_only = false) {
        _read_only = read_only;
        return mock_begin_result;
    }

    void end() {}

    size_t putUChar(const char* key, uint8_t value) {
        if (!canWrite(key)) return 0;
        mock_values[key] = std::vector<uint8_t>{value};
        return 1;
    }

    uint8_t getUChar(const char* key, uint8_t default_value = 0) const {
        auto it = mock_values.find(key ? key : "");
        if (it == mock_values.end() || it->second.empty()) return default_value;
        return it->second[0];
    }

    size_t putString(const char* key, const char* value) {
        if (!canWrite(key) || !value) return 0;
        size_t len = std::strlen(value);
        mock_values[key] = std::vector<uint8_t>(value, value + len + 1);
        return len;
    }

    size_t getString(const char* key, char* out, size_t out_len) const {
        if (!key || !out || out_len == 0) return 0;
        auto it = mock_values.find(key);
        if (it == mock_values.end() || it->second.empty()) return 0;
        size_t len = 0;
        while (len < it->second.size() && it->second[len] != 0) len++;
        size_t copy_len = len < out_len - 1 ? len : out_len - 1;
        std::memcpy(out, it->second.data(), copy_len);
        out[copy_len] = '\0';
        return len;
    }

    size_t putBytes(const char* key, const void* value, size_t len) {
        if (!canWrite(key) || (!value && len > 0)) return 0;
        const uint8_t* bytes = static_cast<const uint8_t*>(value);
        mock_values[key] = std::vector<uint8_t>(bytes, bytes + len);
        return len;
    }

    size_t getBytes(const char* key, void* out, size_t max_len) const {
        if (!key || (!out && max_len > 0)) return 0;
        auto it = mock_values.find(key);
        if (it == mock_values.end()) return 0;
        size_t copy_len = it->second.size() < max_len ? it->second.size() : max_len;
        if (copy_len > 0) std::memcpy(out, it->second.data(), copy_len);
        return it->second.size();
    }

    bool isKey(const char* key) const {
        return key && mock_values.find(key) != mock_values.end();
    }

    bool remove(const char* key) {
        if (_read_only || !key) return false;
        if (mock_remove_fail_key == key) return false;
        mock_values.erase(key);
        return true;
    }

    static void mockReset() {
        mock_values.clear();
        mock_begin_result = true;
        mock_put_fail_key.clear();
        mock_remove_fail_key.clear();
    }

    static void mockSetBeginResult(bool ok) { mock_begin_result = ok; }
    static void mockFailPutKey(const char* key) { mock_put_fail_key = key ? key : ""; }
    static void mockFailRemoveKey(const char* key) { mock_remove_fail_key = key ? key : ""; }

    static bool mockHasKey(const char* key) {
        return key && mock_values.find(key) != mock_values.end();
    }

    static std::vector<uint8_t> mockBytes(const char* key) {
        auto it = mock_values.find(key ? key : "");
        return it == mock_values.end() ? std::vector<uint8_t>{} : it->second;
    }

private:
    bool canWrite(const char* key) const {
        return !_read_only && key && mock_put_fail_key != key;
    }

    bool _read_only = false;

    inline static std::map<std::string, std::vector<uint8_t>> mock_values;
    inline static bool mock_begin_result = true;
    inline static std::string mock_put_fail_key;
    inline static std::string mock_remove_fail_key;
};
