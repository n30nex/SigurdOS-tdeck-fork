// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2025 Ben
//
// This file is part of SigurdOS.
//
// SigurdOS is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.
//
// SigurdOS is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with SigurdOS.  If not, see <https://www.gnu.org/licenses/>.


/**
 * Unit tests for NodePrefs defaults and native preference persistence.
 */
#include <cstdio>
#include <cstring>

#include <gtest/gtest.h>

#include "hal/prefs.h"

namespace {

class PrefsTest : public ::testing::Test {
protected:
    void SetUp() override {
        sigurdos::NodePrefs defaults;
        defaults.set_defaults();
        sigurdos::prefs_set(defaults);

        const char* names[] = {
            "Repeater A", "Repeater B", "Repeater C", "Repeater D",
            "Slot0", "Slot1", "Slot2", "Slot3",
            "Slot4", "Slot5", "Slot6", "Slot7", "Slot8"
        };
        for (const char* name : names) {
            sigurdos::removeRepeaterPassword(name);
        }
        sigurdos::clearMapTileProvider();
    }
};

TEST_F(PrefsTest, DefaultRxBoostedGainIsDisabled) {
    sigurdos::NodePrefs prefs;
    prefs.set_defaults();

    EXPECT_FALSE(prefs.rx_boosted_gain);
}

TEST_F(PrefsTest, RxBoostedGainRoundTripsThroughPrefsSetAndGet) {
    sigurdos::NodePrefs prefs;
    prefs.set_defaults();
    prefs.rx_boosted_gain = true;

    sigurdos::prefs_set(prefs);

    EXPECT_TRUE(sigurdos::prefs_get().rx_boosted_gain);

    prefs.rx_boosted_gain = false;
    sigurdos::prefs_set(prefs);

    EXPECT_FALSE(sigurdos::prefs_get().rx_boosted_gain);
}

TEST_F(PrefsTest, RxBoostedGainRoundTripsThroughPrefsSaveAndLoad) {
    sigurdos::NodePrefs saved;
    saved.set_defaults();
    saved.rx_boosted_gain = true;

    ASSERT_TRUE(sigurdos::prefs_save(saved));

    sigurdos::NodePrefs loaded;
    loaded.set_defaults();

    ASSERT_TRUE(sigurdos::prefs_load(loaded));
    EXPECT_TRUE(loaded.rx_boosted_gain);
}

TEST_F(PrefsTest, DefaultPathHashModeIsOneByte) {
    sigurdos::NodePrefs prefs;
    prefs.set_defaults();

    EXPECT_EQ(0, prefs.path_hash_mode);
}

TEST_F(PrefsTest, PathHashModeRoundTripsThroughPrefsSetAndGet) {
    sigurdos::NodePrefs prefs;
    prefs.set_defaults();
    prefs.path_hash_mode = 2;  // 3-byte path hash

    sigurdos::prefs_set(prefs);

    EXPECT_EQ(2, sigurdos::prefs_get().path_hash_mode);
}

TEST_F(PrefsTest, PathHashModeRoundTripsThroughPrefsSaveAndLoad) {
    sigurdos::NodePrefs saved;
    saved.set_defaults();
    saved.path_hash_mode = 1;  // 2-byte path hash

    ASSERT_TRUE(sigurdos::prefs_save(saved));

    sigurdos::NodePrefs loaded;
    loaded.set_defaults();

    ASSERT_TRUE(sigurdos::prefs_load(loaded));
    EXPECT_EQ(1, loaded.path_hash_mode);
}

TEST_F(PrefsTest, KeyboardLayoutRoundTripsThroughPrefs) {
    sigurdos::NodePrefs saved;
    saved.set_defaults();
    saved.kbd_layout = 9;

    ASSERT_TRUE(sigurdos::prefs_save(saved));

    sigurdos::NodePrefs loaded;
    loaded.set_defaults();
    ASSERT_TRUE(sigurdos::prefs_load(loaded));
    EXPECT_EQ(9, loaded.kbd_layout);
}

TEST_F(PrefsTest, KeyboardRawOverlayDefaultsOffAndRoundTrips) {
    sigurdos::NodePrefs defaults;
    defaults.set_defaults();
    EXPECT_FALSE(defaults.kbd_raw_overlay);

    sigurdos::NodePrefs saved;
    saved.set_defaults();
    saved.kbd_raw_overlay = true;
    ASSERT_TRUE(sigurdos::prefs_save(saved));

    sigurdos::NodePrefs loaded;
    loaded.set_defaults();
    ASSERT_TRUE(sigurdos::prefs_load(loaded));
    EXPECT_TRUE(loaded.kbd_raw_overlay);
}

TEST_F(PrefsTest, RadioProfileRoundTripsThroughPrefs) {
    sigurdos::NodePrefs saved;
    saved.set_defaults();
    std::strncpy(saved.radio_profile, "ca_902_928", sizeof(saved.radio_profile) - 1);
    saved.radio_profile[sizeof(saved.radio_profile) - 1] = '\0';

    ASSERT_TRUE(sigurdos::prefs_save(saved));

    sigurdos::NodePrefs loaded;
    loaded.set_defaults();
    ASSERT_TRUE(sigurdos::prefs_load(loaded));
    EXPECT_STREQ("ca_902_928", loaded.radio_profile);
}

TEST_F(PrefsTest, MapLocationRoundTripsThroughPrefs) {
    sigurdos::NodePrefs saved;
    saved.set_defaults();
    saved.map_location_valid = true;
    saved.map_lat = 43653200;
    saved.map_lon = -79383200;

    ASSERT_TRUE(sigurdos::prefs_save(saved));

    sigurdos::NodePrefs loaded;
    loaded.set_defaults();
    ASSERT_TRUE(sigurdos::prefs_load(loaded));
    EXPECT_TRUE(loaded.map_location_valid);
    EXPECT_EQ(43653200, loaded.map_lat);
    EXPECT_EQ(-79383200, loaded.map_lon);
}

TEST_F(PrefsTest, WifiCredentialReuseRequiresMatchingSsid) {
    sigurdos::NodePrefs prefs;
    prefs.set_defaults();
    std::strncpy(prefs.wifi_ssid, "Workshop", sizeof(prefs.wifi_ssid) - 1);
    std::strncpy(prefs.wifi_password, "correct horse battery staple",
                 sizeof(prefs.wifi_password) - 1);

    EXPECT_TRUE(sigurdos::prefs_wifi_ssid_matches(prefs, "Workshop"));
    EXPECT_FALSE(sigurdos::prefs_wifi_ssid_matches(prefs, "Guest"));
    EXPECT_FALSE(sigurdos::prefs_wifi_ssid_matches(prefs, ""));
    EXPECT_FALSE(sigurdos::prefs_wifi_ssid_matches(prefs, nullptr));
}

TEST_F(PrefsTest, WifiCredentialReuseHonorsEncryptedNetworks) {
    sigurdos::NodePrefs prefs;
    prefs.set_defaults();
    std::strncpy(prefs.wifi_ssid, "Workshop", sizeof(prefs.wifi_ssid) - 1);

    EXPECT_TRUE(sigurdos::prefs_wifi_credentials_reusable(prefs, "Workshop", false));
    EXPECT_FALSE(sigurdos::prefs_wifi_credentials_reusable(prefs, "Workshop", true));

    std::strncpy(prefs.wifi_password, "secret", sizeof(prefs.wifi_password) - 1);
    EXPECT_TRUE(sigurdos::prefs_wifi_credentials_reusable(prefs, "Workshop", true));
    EXPECT_FALSE(sigurdos::prefs_wifi_credentials_reusable(prefs, "Guest", false));
}

TEST_F(PrefsTest, MapTileProviderUrlValidationRejectsUnsafeValues) {
    EXPECT_TRUE(sigurdos::mapTileProviderUrlValid("https://tiles.example.test/osm"));
    EXPECT_TRUE(sigurdos::mapTileProviderUrlValid("http://localhost:8080/tiles"));

    EXPECT_FALSE(sigurdos::mapTileProviderUrlValid(nullptr));
    EXPECT_FALSE(sigurdos::mapTileProviderUrlValid(""));
    EXPECT_FALSE(sigurdos::mapTileProviderUrlValid("ftp://tiles.example.test"));
    EXPECT_FALSE(sigurdos::mapTileProviderUrlValid("https://tiles.example.test/with space"));
    EXPECT_FALSE(sigurdos::mapTileProviderUrlValid("https://tiles.example.test/<bad>"));
}

TEST_F(PrefsTest, MapTileProviderSaveLoadAndClearRoundTrips) {
    char loaded[sigurdos::MAP_TILE_PROVIDER_MAX_LEN] = {};

    EXPECT_FALSE(sigurdos::loadMapTileProvider(loaded, sizeof(loaded)));
    ASSERT_TRUE(sigurdos::saveMapTileProvider("  https://tiles.example.test/osm/  "));
    ASSERT_TRUE(sigurdos::loadMapTileProvider(loaded, sizeof(loaded)));
    EXPECT_STREQ("https://tiles.example.test/osm", loaded);

    ASSERT_TRUE(sigurdos::clearMapTileProvider());
    loaded[0] = '\0';
    EXPECT_FALSE(sigurdos::loadMapTileProvider(loaded, sizeof(loaded)));
    EXPECT_STREQ("", loaded);
}

TEST_F(PrefsTest, RepeaterPasswordSaveLoadAndForgetRoundTrips) {
    char loaded[64] = {0};

    EXPECT_FALSE(sigurdos::loadRepeaterPassword("Repeater A", loaded, sizeof(loaded)));
    ASSERT_TRUE(sigurdos::saveRepeaterPassword("Repeater A", "admin-secret"));
    ASSERT_TRUE(sigurdos::loadRepeaterPassword("Repeater A", loaded, sizeof(loaded)));
    EXPECT_STREQ("admin-secret", loaded);

    sigurdos::removeRepeaterPassword("Repeater A");
    loaded[0] = '\0';
    EXPECT_FALSE(sigurdos::loadRepeaterPassword("Repeater A", loaded, sizeof(loaded)));
    EXPECT_STREQ("", loaded);
}

TEST_F(PrefsTest, RepeaterPasswordSaveUpdatesExistingContactSlot) {
    char loaded[64] = {0};

    ASSERT_TRUE(sigurdos::saveRepeaterPassword("Repeater A", "old-secret"));
    ASSERT_TRUE(sigurdos::saveRepeaterPassword("Repeater A", "new-secret"));

    ASSERT_TRUE(sigurdos::loadRepeaterPassword("Repeater A", loaded, sizeof(loaded)));
    EXPECT_STREQ("new-secret", loaded);
}

TEST_F(PrefsTest, RepeaterPasswordForgetCompactsRemainingEntries) {
    char loaded[64] = {0};

    ASSERT_TRUE(sigurdos::saveRepeaterPassword("Repeater A", "alpha"));
    ASSERT_TRUE(sigurdos::saveRepeaterPassword("Repeater B", "bravo"));
    sigurdos::removeRepeaterPassword("Repeater A");

    EXPECT_FALSE(sigurdos::loadRepeaterPassword("Repeater A", loaded, sizeof(loaded)));
    loaded[0] = '\0';
    ASSERT_TRUE(sigurdos::loadRepeaterPassword("Repeater B", loaded, sizeof(loaded)));
    EXPECT_STREQ("bravo", loaded);
}

TEST_F(PrefsTest, RepeaterPasswordStoreRejectsNinthSavedContact) {
    char name[16];
    for (int i = 0; i < 8; i++) {
        std::snprintf(name, sizeof(name), "Slot%d", i);
        ASSERT_TRUE(sigurdos::saveRepeaterPassword(name, "secret"));
    }

    EXPECT_FALSE(sigurdos::saveRepeaterPassword("Slot8", "secret"));
    EXPECT_TRUE(sigurdos::saveRepeaterPassword("Slot7", "updated"));
}

} // namespace
