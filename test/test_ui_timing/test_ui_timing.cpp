// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2025 Ben

#include <gtest/gtest.h>
#include <cstdint>

#include "ui/ui.h"
#include "ui/notifications.h"

namespace {

using sigurdos::ui::UI_SPLASH_DURATION_MS;
using sigurdos::ui::activity_notification_plan;
using sigurdos::ui::ui_splash_transition_elapsed;

TEST(UITiming, SplashDoesNotTransitionBeforeDelay) {
    EXPECT_FALSE(ui_splash_transition_elapsed(2999, 1000));
}

TEST(UITiming, SplashDoesNotTransitionAtExactDelay) {
    EXPECT_FALSE(ui_splash_transition_elapsed(1000 + UI_SPLASH_DURATION_MS, 1000));
}

TEST(UITiming, SplashTransitionsAfterDelay) {
    EXPECT_TRUE(ui_splash_transition_elapsed(1000 + UI_SPLASH_DURATION_MS + 1, 1000));
}

TEST(UITiming, CustomDurationUsesSameStrictBoundary) {
    EXPECT_FALSE(ui_splash_transition_elapsed(150, 100, 50));
    EXPECT_TRUE(ui_splash_transition_elapsed(151, 100, 50));
}

TEST(UITiming, SplashTimingHandlesMillisRollover) {
    const uint32_t start = UINT32_MAX - 100u;
    EXPECT_FALSE(ui_splash_transition_elapsed(99, start, 200));
    EXPECT_TRUE(ui_splash_transition_elapsed(100, start, 200));
}

TEST(UINotifications, NoActivityDoesNotFlashOrBuzz) {
    const auto plan = activity_notification_plan(false, false, false, false);
    EXPECT_FALSE(plan.flash);
    EXPECT_FALSE(plan.buzz);
}

TEST(UINotifications, QuietModeSuppressesBuzzButKeepsFlash) {
    const auto plan = activity_notification_plan(true, true, true, false);
    EXPECT_TRUE(plan.flash);
    EXPECT_FALSE(plan.buzz);
}

TEST(UINotifications, ActivityWithoutMessageFlashesWithoutBuzz) {
    const auto plan = activity_notification_plan(true, false, false, false);
    EXPECT_TRUE(plan.flash);
    EXPECT_FALSE(plan.buzz);
}

TEST(UINotifications, IncomingMessageWithoutActivityStillBuzzesWithoutFlash) {
    const auto plan = activity_notification_plan(false, false, true, false);
    EXPECT_FALSE(plan.flash);
    EXPECT_TRUE(plan.buzz);
    EXPECT_EQ(plan.buzz_pattern, sigurdos::hal::BuzzerPatternKind::Short);
}

TEST(UINotifications, DirectMessagesUseShortBuzzPattern) {
    const auto plan = activity_notification_plan(true, false, true, false);
    EXPECT_TRUE(plan.flash);
    EXPECT_TRUE(plan.buzz);
    EXPECT_EQ(plan.buzz_pattern, sigurdos::hal::BuzzerPatternKind::Short);
}

TEST(UINotifications, ChannelMessagesUseDoubleBuzzPattern) {
    const auto plan = activity_notification_plan(true, false, true, true);
    EXPECT_TRUE(plan.flash);
    EXPECT_TRUE(plan.buzz);
    EXPECT_EQ(plan.buzz_pattern, sigurdos::hal::BuzzerPatternKind::Double);
}

TEST(UINotifications, VisibleIncomingMessageSuppressesGlobalFlashOnly) {
    const auto plan = activity_notification_plan(true, false, true, true, true);
    EXPECT_FALSE(plan.flash);
    EXPECT_TRUE(plan.buzz);
    EXPECT_EQ(plan.buzz_pattern, sigurdos::hal::BuzzerPatternKind::Double);
}

TEST(UINotifications, MixedVisibleAndOffscreenMessagesStillFlash) {
    const auto plan = activity_notification_plan(true, false, true, true, false);
    EXPECT_TRUE(plan.flash);
    EXPECT_TRUE(plan.buzz);
}

} // anonymous namespace
