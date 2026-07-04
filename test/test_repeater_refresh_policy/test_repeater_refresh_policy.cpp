// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2025 Ben

#include <gtest/gtest.h>

#include "ui/repeater_refresh_policy.h"

namespace {

using sigurdos::ui::repeater_refresh_allowed;

TEST(RepeaterRefreshPolicy, AllowsOnlyActiveRepeaterList)
{
    EXPECT_TRUE(repeater_refresh_allowed(true, true, true, true, false));
}

TEST(RepeaterRefreshPolicy, StopsWithoutTimerState)
{
    EXPECT_FALSE(repeater_refresh_allowed(false, true, true, true, false));
}

TEST(RepeaterRefreshPolicy, StopsWhenListScreenWasDeleted)
{
    EXPECT_FALSE(repeater_refresh_allowed(true, false, true, false, false));
}

TEST(RepeaterRefreshPolicy, StopsWhenNavigationLeftRepeaters)
{
    EXPECT_FALSE(repeater_refresh_allowed(true, true, false, true, false));
}

TEST(RepeaterRefreshPolicy, StopsWhenDetailPageIsOpen)
{
    EXPECT_FALSE(repeater_refresh_allowed(true, true, true, true, true));
}

TEST(RepeaterRefreshPolicy, StopsWhenRepeaterListIsNotActiveScreen)
{
    EXPECT_FALSE(repeater_refresh_allowed(true, true, true, false, false));
}

} // namespace
