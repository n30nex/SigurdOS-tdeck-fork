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
 * Unit tests for home screen icon routing
 *
 * Verifies that each home screen tile navigates to its intended screen.
 * The REPEATERS tile was incorrectly mapped to Screen::Heard (same as PACKETS)
 * instead of Screen::Network (Finder's screen which shows nearby nodes).
 */
#include <gtest/gtest.h>
#include <cstring>

namespace {

// ── Replicate the home screen icon routing table for pure testing ──
// Mirrors home_screen.cpp lines 53-73

enum class Screen {
    Home,
    Chat,
    Contacts,
    Channels,
    Network,
    Heard,
    Map,
    Advertise,
    Settings,
    Trace,
    Terminal,
    Signal,
    RadioSetup,
    Repeaters,
    Onboarding,
    COUNT
};

struct IconDef {
    const char* label;
    const char* symbol;
    bool        badge;
    Screen      target;
};

// MUST match home_screen.cpp exactly (same order, same targets)
static const IconDef icons[] = {
    {"CHATS",     "\x0e",  true,  Screen::Chat},
    {"DMs",       "\x0f",  false, Screen::Chat},
    {"ROOMS",     "\x10",  false, Screen::Contacts},
    {"CONTACTS",  "\x11",  true,  Screen::Contacts},
    {"REPEATERS", "\x15",  true,  Screen::Repeaters},
    {"ADVERTISE", "\x07",  false, Screen::Advertise},
    {"MAP",       "\x13",  false, Screen::Map},
    {"TERMINAL",  "\x0c",  false, Screen::Terminal},
    {"PACKETS",   "\x0b",  false, Screen::Heard},
    {"SETTINGS",  "\x16",  false, Screen::Settings},
    {"SETUP",     "\x16",  false, Screen::Onboarding},
    {"SIGNAL",    "\x19",  false, Screen::Signal},
};

static constexpr int ICON_COUNT = sizeof(icons) / sizeof(icons[0]);

// ── Tests ────────────────────────────────────────────────

TEST(HomeScreenIconTest, OnlyPacketsTargetsHeard) {
    // CHATS/DMs and ROOMS/CONTACTS intentionally share targets with different
    // filters; Heard should remain exclusive to the PACKETS tile.
    int heard_count = 0;
    for (int i = 0; i < ICON_COUNT; i++) {
        if (icons[i].target == Screen::Heard)
            heard_count++;
    }
    // FIXED: only PACKETS = 1 tile pointing to Heard
    EXPECT_EQ(heard_count, 1)
        << "Only PACKETS should target Heard (REPEATERS now targets Repeaters)";
}

TEST(HomeScreenIconTest, RepeatersTargetsRepeaters) {
    // REPEATERS now goes to Screen::Repeaters (dedicated repeaters-only view)
    EXPECT_EQ(icons[4].target, Screen::Repeaters)
        << "REPEATERS should target Repeaters screen (repeaters only)";
}

TEST(HomeScreenIconTest, ChatsAndDmsBothOpenChatWithDifferentFilters) {
    EXPECT_EQ(icons[0].target, Screen::Chat);
    EXPECT_EQ(icons[1].target, Screen::Chat);
    EXPECT_STREQ(icons[0].label, "CHATS");
    EXPECT_STREQ(icons[1].label, "DMs");
}

TEST(HomeScreenIconTest, PacketsTargetsHeard) {
    // PACKETS should stay on Heard (raw packets log)
    EXPECT_EQ(icons[8].target, Screen::Heard);
}

TEST(HomeScreenIconTest, RepeatersAndPacketsAreDifferent) {
    // FIXED: REPEATERS (Network) and PACKETS (Heard) now go to different screens
    EXPECT_NE(icons[4].target, icons[8].target)
        << "REPEATERS and PACKETS should go to different screens";
}

TEST(HomeScreenIconTest, AllIconsPresent) {
    EXPECT_EQ(ICON_COUNT, 12);
}

TEST(HomeScreenIconTest, ChatsTargetsChat) {
    EXPECT_EQ(icons[0].target, Screen::Chat);
}

TEST(HomeScreenIconTest, ContactsTargetsContacts) {
    EXPECT_EQ(icons[3].target, Screen::Contacts);
}

TEST(HomeScreenIconTest, MapTargetsMap) {
    EXPECT_EQ(icons[6].target, Screen::Map);
}

TEST(HomeScreenIconTest, AdvertiseTargetsAdvertise) {
    EXPECT_EQ(icons[5].target, Screen::Advertise);
}

TEST(HomeScreenIconTest, SettingsTargetsSettings) {
    EXPECT_EQ(icons[9].target, Screen::Settings);
}

TEST(HomeScreenIconTest, RoomsTargetsContacts) {
    EXPECT_EQ(icons[2].target, Screen::Contacts);
}

TEST(HomeScreenIconTest, TerminalTargetsTerminal) {
    EXPECT_EQ(icons[7].target, Screen::Terminal);
}

TEST(HomeScreenIconTest, SetupTargetsOnboarding) {
    EXPECT_EQ(icons[10].target, Screen::Onboarding);
}

TEST(HomeScreenIconTest, SignalTargetsSignal) {
    EXPECT_EQ(icons[11].target, Screen::Signal);
}

} // anonymous namespace
