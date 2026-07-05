// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2025 Ben

#include <gtest/gtest.h>

#include "Arduino.h"
#include "SD.h"
#include "hal/sdcard.h"
#include "hal/tdeck_pins.h"

#include <cstring>
#include <vector>

namespace {

class SDCardTest : public ::testing::Test {
protected:
    void SetUp() override {
        arduino_mock::reset();
        SD.mockReset();
        SD.mockSetBeginResult(false);
        ASSERT_FALSE(sigurdos_sdcard_init());
    }

    void mount(uint64_t total = 32000000000ULL, uint64_t used = 1000000000ULL) {
        SD.mockSetBeginResult(true);
        SD.mockSetUsage(total, used);
        ASSERT_TRUE(sigurdos_sdcard_init());
    }
};

TEST_F(SDCardTest, CSPinIsValidGPIO) {
    EXPECT_GE(PIN_SD_CS, 0);
    EXPECT_LE(PIN_SD_CS, 48);
}

TEST_F(SDCardTest, CSPinDoesNotConflictWithDisplayOrLoRa) {
    EXPECT_NE(PIN_SD_CS, PIN_TFT_CS);
    EXPECT_NE(PIN_SD_CS, PIN_LORA_NSS);
}

TEST_F(SDCardTest, InitialStateIsNotMounted) {
    EXPECT_FALSE(sigurdos_sdcard_mounted());
}

TEST_F(SDCardTest, MountedStateHasCapacity) {
    mount(32000000000ULL, 15000000000ULL);
    EXPECT_TRUE(sigurdos_sdcard_mounted());
    EXPECT_EQ(sigurdos_sdcard_capacity_bytes(), 32000000000ULL);
    EXPECT_EQ(sigurdos_sdcard_free_bytes(), 17000000000ULL);
}

TEST_F(SDCardTest, ErrorStateHasZeroCapacity) {
    EXPECT_FALSE(sigurdos_sdcard_mounted());
    EXPECT_EQ(sigurdos_sdcard_capacity_bytes(), 0ULL);
    EXPECT_EQ(sigurdos_sdcard_free_bytes(), 0ULL);
}

TEST_F(SDCardTest, DiagnosticDefaultStateIsUnmounted) {
    SigurdosSdMountDiagnostic diag = sigurdos_sdcard_diagnostics();
    EXPECT_FALSE(diag.mounted);
    EXPECT_EQ(diag.attempt_count, 3);
    EXPECT_EQ(diag.last_source, SIGURDOS_SD_MOUNT_SOURCE_INIT);
    EXPECT_EQ(diag.last_error, SIGURDOS_SD_MOUNT_ERROR_BEGIN_FAILED);
    EXPECT_EQ(diag.last_backoff_ms, 300U);
}

TEST_F(SDCardTest, DiagnosticCanReportRetryFailure) {
    EXPECT_FALSE(sigurdos_sdcard_retry());
    SigurdosSdMountDiagnostic diag = sigurdos_sdcard_diagnostics();
    EXPECT_FALSE(diag.mounted);
    EXPECT_EQ(diag.last_source, SIGURDOS_SD_MOUNT_SOURCE_RETRY);
    EXPECT_EQ(diag.last_error, SIGURDOS_SD_MOUNT_ERROR_BEGIN_FAILED);
}

TEST_F(SDCardTest, ValidPathStartsWithSlash) {
    EXPECT_TRUE(sigurdos_sdcard_path_valid("/maps/london.mbtiles"));
    EXPECT_TRUE(sigurdos_sdcard_path_valid("/config.txt"));
    EXPECT_TRUE(sigurdos_sdcard_path_valid("/logs/mesh_2026_05_14.log"));
}

TEST_F(SDCardTest, PathWithoutSlashIsInvalid) {
    EXPECT_FALSE(sigurdos_sdcard_path_valid("maps/file.dat"));
    EXPECT_FALSE(sigurdos_sdcard_path_valid(""));
    EXPECT_FALSE(sigurdos_sdcard_path_valid(nullptr));
}

TEST_F(SDCardTest, PathWithDotDotIsInvalid) {
    EXPECT_FALSE(sigurdos_sdcard_path_valid("/etc/../passwd"));
    EXPECT_FALSE(sigurdos_sdcard_path_valid("/../../root"));
    EXPECT_FALSE(sigurdos_sdcard_path_valid("/tiles/10/..hidden.png"));
}

TEST_F(SDCardTest, PathTooLongIsInvalid) {
    char long_path[SIGURDOS_SD_MAX_PATH_LEN + 5];
    memset(long_path, 'a', sizeof(long_path));
    long_path[0] = '/';
    long_path[sizeof(long_path) - 1] = '\0';
    EXPECT_FALSE(sigurdos_sdcard_path_valid(long_path));
}

TEST_F(SDCardTest, PathAtMaxLengthIsValid) {
    char max_path[SIGURDOS_SD_MAX_PATH_LEN + 1];
    memset(max_path, 'x', SIGURDOS_SD_MAX_PATH_LEN);
    max_path[0] = '/';
    max_path[SIGURDOS_SD_MAX_PATH_LEN] = '\0';
    EXPECT_TRUE(sigurdos_sdcard_path_valid(max_path));
}

TEST_F(SDCardTest, FormatGB) {
    char buf[16];
    const char* s = sigurdos_sdcard_format_size(32ULL * 1024 * 1024 * 1024, buf, sizeof(buf));
    EXPECT_STREQ(s, "32.0 GB");
}

TEST_F(SDCardTest, FormatMB) {
    char buf[16];
    const char* s = sigurdos_sdcard_format_size(500 * 1024 * 1024ULL, buf, sizeof(buf));
    EXPECT_STREQ(s, "500.0 MB");
}

TEST_F(SDCardTest, FormatKB) {
    char buf[16];
    const char* s = sigurdos_sdcard_format_size(512 * 1024ULL, buf, sizeof(buf));
    EXPECT_STREQ(s, "512 KB");
}

TEST_F(SDCardTest, FormatZero) {
    char buf[16];
    const char* s = sigurdos_sdcard_format_size(0, buf, sizeof(buf));
    EXPECT_STREQ(s, "0 B");
}

TEST_F(SDCardTest, MapTilePathConvention) {
    EXPECT_TRUE(sigurdos_sdcard_path_valid("/sdcard/tiles/10/512/340.png"));
    EXPECT_TRUE(sigurdos_sdcard_path_valid("/sdcard/tiles/14/8137/5290.png"));
    EXPECT_TRUE(sigurdos_sdcard_path_valid("/sdcard/tiles/metadata.json"));
}

TEST_F(SDCardTest, ExistingFileSurvivesFailedReplacementOpen) {
    mount();
    const uint8_t old_data[] = {'o', 'l', 'd'};
    const uint8_t new_data[] = {'n', 'e', 'w'};
    SD.mockSetFile("/state.bin", old_data, sizeof(old_data));
    SD.mockFailOpenPath("/state.bin.tmp");

    EXPECT_FALSE(sigurdos_sdcard_write("/state.bin", new_data, sizeof(new_data)));
    EXPECT_EQ(SD.mockFile("/state.bin"), std::vector<uint8_t>({'o', 'l', 'd'}));
}

TEST_F(SDCardTest, ExistingFileSurvivesShortWrite) {
    mount();
    const uint8_t old_data[] = {'o', 'l', 'd'};
    const uint8_t new_data[] = {'n', 'e', 'w'};
    SD.mockSetFile("/state.bin", old_data, sizeof(old_data));
    SD.mockShortWriteOnce();

    EXPECT_FALSE(sigurdos_sdcard_write("/state.bin", new_data, sizeof(new_data)));
    EXPECT_EQ(SD.mockFile("/state.bin"), std::vector<uint8_t>({'o', 'l', 'd'}));
}

TEST_F(SDCardTest, ExistingFileSurvivesFailedFinalRename) {
    mount();
    const uint8_t old_data[] = {'o', 'l', 'd'};
    const uint8_t new_data[] = {'n', 'e', 'w'};
    SD.mockSetFile("/state.bin", old_data, sizeof(old_data));
    SD.mockFailRenameTo("/state.bin");

    EXPECT_FALSE(sigurdos_sdcard_write("/state.bin", new_data, sizeof(new_data)));
    EXPECT_EQ(SD.mockFile("/state.bin"), std::vector<uint8_t>({'o', 'l', 'd'}));
}

TEST_F(SDCardTest, SuccessfulReplacementUpdatesContent) {
    mount();
    const uint8_t old_data[] = {'o', 'l', 'd'};
    const uint8_t new_data[] = {'n', 'e', 'w'};
    SD.mockSetFile("/state.bin", old_data, sizeof(old_data));

    EXPECT_TRUE(sigurdos_sdcard_write("/state.bin", new_data, sizeof(new_data)));
    EXPECT_EQ(SD.mockFile("/state.bin"), std::vector<uint8_t>({'n', 'e', 'w'}));
    EXPECT_FALSE(SD.exists("/state.bin.tmp"));
    EXPECT_FALSE(SD.exists("/state.bin.bak"));
}

TEST_F(SDCardTest, ZeroLengthWriteTruncatesOrCreatesFile) {
    mount();
    const uint8_t old_data[] = {'o', 'l', 'd'};
    SD.mockSetFile("/state.bin", old_data, sizeof(old_data));

    EXPECT_TRUE(sigurdos_sdcard_write("/state.bin", nullptr, 0));
    EXPECT_TRUE(sigurdos_sdcard_exists("/state.bin"));
    EXPECT_TRUE(SD.mockFile("/state.bin").empty());
}

TEST_F(SDCardTest, FreeBytesRefreshesAfterMockUsageChanges) {
    mount(1000, 100);
    EXPECT_EQ(sigurdos_sdcard_free_bytes(), 900ULL);

    SD.mockSetUsage(1000, 300);
    EXPECT_EQ(sigurdos_sdcard_free_bytes(), 700ULL);
}

TEST_F(SDCardTest, UsedGreaterThanTotalReportsZeroFree) {
    mount(1000, 2000);
    EXPECT_EQ(sigurdos_sdcard_free_bytes(), 0ULL);
}

} // anonymous namespace
