// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Ben

#include <cstring>

#include <gtest/gtest.h>

#include "hal/prefs.h"
#include "hal/radio_profiles.h"

namespace {

TEST(RadioProfilesTest, DefaultProfileIsUsaCanadaRecommended) {
    const auto* profile = sigurdos::radio_profile_default();

    ASSERT_NE(nullptr, profile);
    EXPECT_STREQ("na_rec", profile->id);
    EXPECT_STREQ("USA/Canada", profile->short_label);
    EXPECT_FLOAT_EQ(910.525f, profile->freq_mhz);
    EXPECT_FLOAT_EQ(62.5f, profile->bw_khz);
    EXPECT_EQ(7, profile->sf);
    EXPECT_EQ(5, profile->cr);
    EXPECT_EQ(22, profile->tx_power_dbm);
    EXPECT_EQ(2, profile->path_hash_mode);
}

TEST(RadioProfilesTest, LegacyUnitedStatesAndCanadaIdsResolveToMergedPreset) {
    const auto* us = sigurdos::radio_profile_find("us_902_928");
    const auto* ca = sigurdos::radio_profile_find("ca_902_928");

    ASSERT_NE(nullptr, us);
    ASSERT_NE(nullptr, ca);
    EXPECT_EQ(us, ca);
    EXPECT_STREQ("na_rec", ca->id);
    EXPECT_FLOAT_EQ(910.525f, ca->freq_mhz);
    EXPECT_FLOAT_EQ(62.5f, ca->bw_khz);
    EXPECT_EQ(7, ca->sf);
    EXPECT_EQ(5, ca->cr);
    EXPECT_EQ(22, ca->tx_power_dbm);
    EXPECT_EQ(2, ca->path_hash_mode);
}

TEST(RadioProfilesTest, ApplySetsPrefsAndPathHashMode) {
    sigurdos::NodePrefs prefs;
    prefs.set_defaults();
    ASSERT_FALSE(prefs.configured);

    const auto* na = sigurdos::radio_profile_find("na_rec");
    ASSERT_NE(nullptr, na);
    sigurdos::radio_profile_apply(*na, prefs);

    EXPECT_TRUE(prefs.configured);
    EXPECT_STREQ("na_rec", prefs.radio_profile);
    EXPECT_FLOAT_EQ(910.525f, prefs.freq);
    EXPECT_FLOAT_EQ(62.5f, prefs.bw);
    EXPECT_EQ(7, prefs.sf);
    EXPECT_EQ(5, prefs.cr);
    EXPECT_EQ(22, prefs.tx_power_dbm);
    EXPECT_EQ(2, prefs.path_hash_mode);
}

TEST(RadioProfilesTest, MatchUsesSavedMergedPreset) {
    sigurdos::NodePrefs prefs;
    prefs.set_defaults();
    const auto* na = sigurdos::radio_profile_find("na_rec");
    ASSERT_NE(nullptr, na);
    sigurdos::radio_profile_apply(*na, prefs);

    const auto* matched = sigurdos::radio_profile_match(prefs);

    ASSERT_NE(nullptr, matched);
    EXPECT_STREQ("na_rec", matched->id);
}

TEST(RadioProfilesTest, IncludesCommunityPresetMatrix) {
    EXPECT_NE(nullptr, sigurdos::radio_profile_find("au"));
    EXPECT_NE(nullptr, sigurdos::radio_profile_find("eu_uk_n"));
    EXPECT_NE(nullptr, sigurdos::radio_profile_find("nz_n"));
    EXPECT_NE(nullptr, sigurdos::radio_profile_find("pt868"));
    EXPECT_NE(nullptr, sigurdos::radio_profile_find("vn"));
}

TEST(RadioProfilesTest, CustomMarksManualSettings) {
    sigurdos::NodePrefs prefs;
    prefs.set_defaults();
    prefs.freq = 916.250f;
    prefs.bw = 125.0f;
    prefs.sf = 9;
    prefs.cr = 6;
    prefs.tx_power_dbm = 20;
    prefs.configured = true;

    EXPECT_EQ(nullptr, sigurdos::radio_profile_match(prefs));

    sigurdos::radio_profile_set_custom(prefs);
    EXPECT_STREQ("custom", prefs.radio_profile);
}

} // namespace
