// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2025 Ben

#include <gtest/gtest.h>

#include "ui/repeater_refresh_policy.h"

namespace {

using sigurdos::ui::repeater_refresh_allowed;
using sigurdos::ui::RepeaterManagementRequest;

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

TEST(RepeaterManagementPolicy, RequestLabelsStayStable)
{
    EXPECT_STREQ(sigurdos::ui::repeater_management_request_label(
                    RepeaterManagementRequest::Status),
                "Status");
    EXPECT_STREQ(sigurdos::ui::repeater_management_request_label(
                    RepeaterManagementRequest::Telemetry),
                "Telemetry");
    EXPECT_STREQ(sigurdos::ui::repeater_management_request_label(
                    RepeaterManagementRequest::Neighbours),
                "Neighbours");
}

TEST(RepeaterManagementPolicy, RequestFailureMessagesAreUserVisible)
{
    EXPECT_STREQ(sigurdos::ui::repeater_management_request_failed_message(
                    RepeaterManagementRequest::Status),
                "! Status request failed");
    EXPECT_STREQ(sigurdos::ui::repeater_management_request_failed_message(
                    RepeaterManagementRequest::Telemetry),
                "! Telemetry request failed");
    EXPECT_STREQ(sigurdos::ui::repeater_management_request_failed_message(
                    RepeaterManagementRequest::Neighbours),
                "! Neighbours request failed");
}

TEST(LoginRefreshPolicy, SkipsImmediateDetailRefreshForSentBlankRoomGuestLogin)
{
    EXPECT_FALSE(sigurdos::ui::login_detail_refresh_after_submit(true, true));
}

TEST(LoginRefreshPolicy, SkipsDetailRefreshForFailedBlankRoomGuestLogin)
{
    EXPECT_FALSE(sigurdos::ui::login_detail_refresh_after_submit(false, true));
}

TEST(LoginRefreshPolicy, RefreshesAfterFailedAdminOrRepeaterLoginSubmit)
{
    EXPECT_TRUE(sigurdos::ui::login_detail_refresh_after_submit(false, false));
}

TEST(LoginRefreshPolicy, RefreshesAfterSentAdminOrRepeaterLogin)
{
    EXPECT_TRUE(sigurdos::ui::login_detail_refresh_after_submit(true, false));
}

TEST(LoginRefreshPolicy, StartsPollForSentAdminOrRepeaterLogin)
{
    EXPECT_TRUE(sigurdos::ui::login_submit_starts_poll_timer(true, false));
    EXPECT_FALSE(sigurdos::ui::login_submit_starts_poll_timer(false, false));
}

TEST(LoginRefreshPolicy, DoesNotPollForBlankRoomGuestOpen)
{
    EXPECT_FALSE(sigurdos::ui::login_submit_starts_poll_timer(true, true));
    EXPECT_FALSE(sigurdos::ui::login_submit_starts_poll_timer(false, true));
}

TEST(LoginRefreshPolicy, AllowsDelayedRefreshOnlyForCurrentOpenDetail)
{
    EXPECT_TRUE(sigurdos::ui::login_detail_refresh_allowed(true, true));
    EXPECT_FALSE(sigurdos::ui::login_detail_refresh_allowed(false, true));
    EXPECT_FALSE(sigurdos::ui::login_detail_refresh_allowed(true, false));
}

TEST(LoginPollPolicy, DoesNotTimeoutBeforeLastPendingPoll)
{
    EXPECT_FALSE(sigurdos::ui::login_poll_timed_out(
        sigurdos::ui::REPEATER_LOGIN_POLL_MAX_PENDING_POLLS - 1));
}

TEST(LoginPollPolicy, TimesOutAtPendingPollLimit)
{
    EXPECT_TRUE(sigurdos::ui::login_poll_timed_out(
        sigurdos::ui::REPEATER_LOGIN_POLL_MAX_PENDING_POLLS));
    EXPECT_TRUE(sigurdos::ui::login_poll_timed_out(
        sigurdos::ui::REPEATER_LOGIN_POLL_MAX_PENDING_POLLS + 1));
}

TEST(LoginPollPolicy, TimeoutDurationMatchesIntervalAndPollLimit)
{
    EXPECT_EQ(sigurdos::ui::login_poll_timeout_ms(),
              sigurdos::ui::REPEATER_LOGIN_POLL_INTERVAL_MS *
              static_cast<uint32_t>(sigurdos::ui::REPEATER_LOGIN_POLL_MAX_PENDING_POLLS));
}

TEST(RepeaterAdminPolicy, ShowsManagementRowsOnlyForAdmins)
{
    EXPECT_TRUE(sigurdos::ui::repeater_show_admin_management_rows(true));
    EXPECT_FALSE(sigurdos::ui::repeater_show_admin_management_rows(false));
}

TEST(RepeaterAdminPolicy, KeepsHighRiskRowsHidden)
{
    EXPECT_FALSE(sigurdos::ui::repeater_show_admin_radio_rows(true));
    EXPECT_FALSE(sigurdos::ui::repeater_show_admin_radio_rows(false));
    EXPECT_FALSE(sigurdos::ui::repeater_show_admin_password_rows(true));
    EXPECT_FALSE(sigurdos::ui::repeater_show_admin_password_rows(false));
}

} // namespace
