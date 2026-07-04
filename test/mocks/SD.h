#pragma once
// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Ben
//
// Native mock for the Arduino SD API used by hal/sdcard.cpp.

#include "Arduino.h"

#ifdef map
#undef map
#endif

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <map>
#include <string>
#include <vector>

#define FILE_READ  "r"
#define FILE_WRITE "w"

class File {
public:
    File() = default;
    File(std::vector<uint8_t>* data, bool writable, bool* short_write)
        : _data(data), _writable(writable), _short_write(short_write), _valid(data != nullptr) {}

    operator bool() const { return _valid; }

    size_t write(const uint8_t* buf, size_t len) {
        if (!_valid || !_writable || (!buf && len > 0)) return 0;
        size_t actual = len;
        if (_short_write && *_short_write && len > 0) {
            actual = len - 1;
            *_short_write = false;
        }
        _data->insert(_data->end(), buf, buf + actual);
        return actual;
    }

    size_t read(uint8_t* buf, size_t len) {
        if (!_valid || !buf || !_data) return 0;
        size_t remaining = _pos < _data->size() ? _data->size() - _pos : 0;
        size_t actual = remaining < len ? remaining : len;
        if (actual > 0) std::memcpy(buf, _data->data() + _pos, actual);
        _pos += actual;
        return actual;
    }

    size_t size() const { return _valid && _data ? _data->size() : 0; }
    void close() { _valid = false; }

private:
    std::vector<uint8_t>* _data = nullptr;
    bool _writable = false;
    bool* _short_write = nullptr;
    bool _valid = false;
    size_t _pos = 0;
};

class SDClass {
public:
    bool begin(int, SPIClass&, uint32_t, const char*) { return _begin_result; }
    void end() { _mounted = false; }

    bool exists(const char* path) const {
        return path && _files.find(path) != _files.end();
    }

    bool remove(const char* path) {
        if (!path) return false;
        return _files.erase(path) > 0;
    }

    bool rename(const char* from, const char* to) {
        if (!from || !to) return false;
        if (_fail_rename_to == to) {
            _fail_rename_to.clear();
            return false;
        }
        auto it = _files.find(from);
        if (it == _files.end() || _files.find(to) != _files.end()) return false;
        _files[to] = it->second;
        _files.erase(it);
        return true;
    }

    File open(const char* path, const char* mode) {
        if (!path || !mode || _fail_open_path == path) return File();
        bool write = std::strchr(mode, 'w') != nullptr || std::strchr(mode, 'a') != nullptr;
        if (write) {
            auto& data = _files[path];
            data.clear();
            return File(&data, true, &_short_write_once);
        }
        auto it = _files.find(path);
        if (it == _files.end()) return File();
        return File(&it->second, false, &_short_write_once);
    }

    uint64_t totalBytes() const { return _total_bytes; }
    uint64_t usedBytes() const { return _used_bytes; }

    void mockReset() {
        _files.clear();
        _begin_result = true;
        _mounted = false;
        _total_bytes = 0;
        _used_bytes = 0;
        _fail_open_path.clear();
        _fail_rename_to.clear();
        _short_write_once = false;
    }

    void mockSetBeginResult(bool ok) { _begin_result = ok; }
    void mockSetUsage(uint64_t total, uint64_t used) {
        _total_bytes = total;
        _used_bytes = used;
    }
    void mockFailOpenPath(const char* path) { _fail_open_path = path ? path : ""; }
    void mockFailRenameTo(const char* path) { _fail_rename_to = path ? path : ""; }
    void mockShortWriteOnce() { _short_write_once = true; }

    void mockSetFile(const char* path, const uint8_t* data, size_t len) {
        if (!path) return;
        const uint8_t* begin = data;
        const uint8_t* end = data ? data + len : data;
        _files[path] = (data && len > 0) ? std::vector<uint8_t>(begin, end)
                                         : std::vector<uint8_t>{};
    }

    std::vector<uint8_t> mockFile(const char* path) const {
        auto it = _files.find(path ? path : "");
        return it == _files.end() ? std::vector<uint8_t>{} : it->second;
    }

private:
    std::map<std::string, std::vector<uint8_t>> _files;
    bool _begin_result = true;
    bool _mounted = false;
    uint64_t _total_bytes = 0;
    uint64_t _used_bytes = 0;
    std::string _fail_open_path;
    std::string _fail_rename_to;
    bool _short_write_once = false;
};

extern SDClass SD;
