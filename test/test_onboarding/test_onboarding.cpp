// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2025 Ben

#include <gtest/gtest.h>

#include "hal/time_sync_policy.h"
#include "ui/onboarding_screen.h"

namespace {

using sigurdos::ui::onboarding_date_valid;
using sigurdos::ui::onboarding_days_in_month;
using sigurdos::ui::onboarding_clamp_day;
using sigurdos::ui::onboarding_is_leap_year;
using sigurdos::ui::onboarding_time_valid;
using sigurdos::ui::onboarding_wrap_range;

TEST(OnboardingValidation, LeapYearRulesCoverCenturyCases) {
    EXPECT_TRUE(onboarding_is_leap_year(2024));
    EXPECT_FALSE(onboarding_is_leap_year(2025));
    EXPECT_FALSE(onboarding_is_leap_year(2100));
    EXPECT_TRUE(onboarding_is_leap_year(2400));
}

TEST(OnboardingValidation, DaysInMonthHandlesMonthBounds) {
    EXPECT_EQ(onboarding_days_in_month(2025, 0), 0);
    EXPECT_EQ(onboarding_days_in_month(2025, 1), 31);
    EXPECT_EQ(onboarding_days_in_month(2025, 2), 28);
    EXPECT_EQ(onboarding_days_in_month(2024, 2), 29);
    EXPECT_EQ(onboarding_days_in_month(2025, 4), 30);
    EXPECT_EQ(onboarding_days_in_month(2025, 12), 31);
    EXPECT_EQ(onboarding_days_in_month(2025, 13), 0);
}

TEST(OnboardingValidation, DateAcceptsValidBoundaries) {
    EXPECT_TRUE(onboarding_date_valid(2020, 1, 1));
    EXPECT_TRUE(onboarding_date_valid(2025, 12, 31));
    EXPECT_TRUE(onboarding_date_valid(2024, 2, 29));
}

TEST(OnboardingValidation, DateRejectsInvalidValues) {
    EXPECT_FALSE(onboarding_date_valid(2019, 12, 31));
    EXPECT_FALSE(onboarding_date_valid(2025, 0, 1));
    EXPECT_FALSE(onboarding_date_valid(2025, 13, 1));
    EXPECT_FALSE(onboarding_date_valid(2025, 1, 0));
    EXPECT_FALSE(onboarding_date_valid(2025, 4, 31));
    EXPECT_FALSE(onboarding_date_valid(2025, 2, 29));
}

TEST(OnboardingValidation, ClampDayHandlesMonthAndLeapChanges) {
    EXPECT_EQ(29, onboarding_clamp_day(2024, 2, 31));
    EXPECT_EQ(28, onboarding_clamp_day(2025, 2, 31));
    EXPECT_EQ(30, onboarding_clamp_day(2025, 4, 31));
    EXPECT_EQ(1, onboarding_clamp_day(2025, 13, 31));
    EXPECT_EQ(1, onboarding_clamp_day(2025, 1, 0));
}

TEST(OnboardingValidation, WrapRangeMovesAcrossBounds) {
    EXPECT_EQ(12, onboarding_wrap_range(0, 1, 12));
    EXPECT_EQ(1, onboarding_wrap_range(13, 1, 12));
    EXPECT_EQ(5, onboarding_wrap_range(5, 1, 12));
}

TEST(OnboardingValidation, TimeAcceptsValidBoundaries) {
    EXPECT_TRUE(onboarding_time_valid(0, 0));
    EXPECT_TRUE(onboarding_time_valid(12, 34));
    EXPECT_TRUE(onboarding_time_valid(23, 59));
}

TEST(OnboardingValidation, TimeRejectsInvalidValues) {
    EXPECT_FALSE(onboarding_time_valid(-1, 0));
    EXPECT_FALSE(onboarding_time_valid(24, 0));
    EXPECT_FALSE(onboarding_time_valid(12, -1));
    EXPECT_FALSE(onboarding_time_valid(12, 60));
}

TEST(OnboardingValidation, ManualTimeStepNeededOnlyForUnsaneEpoch) {
    EXPECT_TRUE(sigurdos::onboarding_manual_time_needed(0));
    EXPECT_TRUE(sigurdos::onboarding_manual_time_needed(1699999999UL));
    EXPECT_FALSE(sigurdos::onboarding_manual_time_needed(1700000000UL));
    EXPECT_FALSE(sigurdos::onboarding_manual_time_needed(1800000000UL));
}

TEST(TimeSyncPolicy, NtpRetryStartsInitiallyThenWaitsForBackoff) {
    EXPECT_TRUE(sigurdos::ntp_retry_due(false, 0, 0));
    EXPECT_FALSE(sigurdos::ntp_retry_due(true, 59999, 0));
    EXPECT_TRUE(sigurdos::ntp_retry_due(true, 60000, 0));
}

TEST(TimeSyncPolicy, NtpResyncUsesSixHourWindow) {
    EXPECT_TRUE(sigurdos::ntp_resync_due(0, 0));
    EXPECT_FALSE(sigurdos::ntp_resync_due(1000, 1000));
    EXPECT_FALSE(sigurdos::ntp_resync_due(
        1000 + sigurdos::SIGURDOS_NTP_RESYNC_INTERVAL_MS - 1, 1000));
    EXPECT_TRUE(sigurdos::ntp_resync_due(
        1000 + sigurdos::SIGURDOS_NTP_RESYNC_INTERVAL_MS, 1000));
}

} // anonymous namespace
