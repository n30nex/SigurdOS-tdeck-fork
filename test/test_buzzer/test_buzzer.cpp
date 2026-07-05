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

#include <gtest/gtest.h>

#include <Arduino.h>

#include "hal/buzzer.h"
#include "hal/tdeck_pins.h"

namespace {

using sigurdos::hal::BuzzerPatternKind;
using sigurdos::hal::BuzzerPatternStep;
using sigurdos::hal::SIGURDOS_BUZZER_DOUBLE_GAP_MS;
using sigurdos::hal::SIGURDOS_BUZZER_DOUBLE_ON_MS;
using sigurdos::hal::SIGURDOS_BUZZER_SHORT_ON_MS;
using sigurdos::hal::SIGURDOS_BUZZER_TONE_HZ;
using sigurdos::hal::sigurdos_buzzer_pattern;

class BuzzerPatternTest : public ::testing::Test {};

TEST_F(BuzzerPatternTest, ShortPatternMatchesNotificationContract) {
    std::size_t count = 0;
    const BuzzerPatternStep* pattern =
        sigurdos_buzzer_pattern(BuzzerPatternKind::Short, &count);

    ASSERT_NE(pattern, nullptr);
    ASSERT_EQ(count, 2U);
    EXPECT_TRUE(pattern[0].tone_on);
    EXPECT_EQ(pattern[0].duration_ms, SIGURDOS_BUZZER_SHORT_ON_MS);
    EXPECT_EQ(pattern[0].frequency_hz, SIGURDOS_BUZZER_TONE_HZ);
    EXPECT_FALSE(pattern[1].tone_on);
    EXPECT_EQ(pattern[1].duration_ms, 0U);
    EXPECT_EQ(pattern[1].frequency_hz, 0U);
}

TEST_F(BuzzerPatternTest, DoublePatternMatchesNotificationContract) {
    std::size_t count = 0;
    const BuzzerPatternStep* pattern =
        sigurdos_buzzer_pattern(BuzzerPatternKind::Double, &count);

    ASSERT_NE(pattern, nullptr);
    ASSERT_EQ(count, 4U);
    EXPECT_TRUE(pattern[0].tone_on);
    EXPECT_EQ(pattern[0].duration_ms, SIGURDOS_BUZZER_DOUBLE_ON_MS);
    EXPECT_EQ(pattern[0].frequency_hz, SIGURDOS_BUZZER_TONE_HZ);
    EXPECT_FALSE(pattern[1].tone_on);
    EXPECT_EQ(pattern[1].duration_ms, SIGURDOS_BUZZER_DOUBLE_GAP_MS);
    EXPECT_EQ(pattern[1].frequency_hz, 0U);
    EXPECT_TRUE(pattern[2].tone_on);
    EXPECT_EQ(pattern[2].duration_ms, SIGURDOS_BUZZER_DOUBLE_ON_MS);
    EXPECT_EQ(pattern[2].frequency_hz, SIGURDOS_BUZZER_TONE_HZ);
    EXPECT_FALSE(pattern[3].tone_on);
    EXPECT_EQ(pattern[3].duration_ms, 0U);
    EXPECT_EQ(pattern[3].frequency_hz, 0U);
}

TEST_F(BuzzerPatternTest, PatternLookupSupportsNullCount) {
    const BuzzerPatternStep* pattern =
        sigurdos_buzzer_pattern(BuzzerPatternKind::Short, nullptr);

    ASSERT_NE(pattern, nullptr);
    EXPECT_TRUE(pattern[0].tone_on);
    EXPECT_EQ(pattern[0].duration_ms, SIGURDOS_BUZZER_SHORT_ON_MS);
    EXPECT_EQ(pattern[0].frequency_hz, SIGURDOS_BUZZER_TONE_HZ);
}

TEST_F(BuzzerPatternTest, PatternDurationsStayWithinResponsiveBounds) {
    std::size_t count = 0;
    const BuzzerPatternStep* pattern =
        sigurdos_buzzer_pattern(BuzzerPatternKind::Double, &count);

    uint16_t total_ms = 0;
    for (std::size_t i = 0; i < count; ++i) {
        total_ms = static_cast<uint16_t>(total_ms + pattern[i].duration_ms);
        EXPECT_LE(pattern[i].duration_ms, 100U);
    }

    EXPECT_EQ(total_ms,
              static_cast<uint16_t>((2 * SIGURDOS_BUZZER_DOUBLE_ON_MS) +
                                    SIGURDOS_BUZZER_DOUBLE_GAP_MS));
    EXPECT_LE(total_ms, 250U);
}

class BuzzerPlaybackTest : public ::testing::Test {
protected:
    void SetUp() override {
        arduino_mock::reset();
        arduino_mock::current_millis = 1000;
        sigurdos::hal::buzzer_init();
        // Drain any pattern left active by a previous test: each loop call
        // can advance at most one step, so a handful of far-future ticks
        // always reaches the idle state.
        for (int i = 0; i < 8; ++i) {
            arduino_mock::current_millis += 1000;
            sigurdos::hal::buzzer_loop();
        }
        arduino_mock::current_millis = 1000;
        ASSERT_FALSE(output_active());
    }

    bool output_active() const {
        return sigurdos::hal::buzzer_output_active_for_test();
    }

    void loop_at(unsigned long ms) {
        arduino_mock::current_millis = ms;
        sigurdos::hal::buzzer_loop();
    }
};

TEST_F(BuzzerPlaybackTest, ShortBeepReturnsImmediatelyAndPlaysSequence) {
    sigurdos::hal::buzzer_beep_short();

    // The mock delay() advances current_millis, so an unchanged clock
    // proves the call no longer blocks (the old code burned 80 ms here).
    EXPECT_EQ(arduino_mock::current_millis, 1000UL);
    EXPECT_TRUE(output_active());
    EXPECT_EQ(arduino_mock::tone_calls, 0)
        << "T-Deck notification audio is generated through the I2S speaker path";
    EXPECT_EQ(arduino_mock::pin_states[PIN_KEYBOARD_INT], 0)
        << "GPIO46 is the keyboard interrupt and must not be driven as audio";

    loop_at(1079);  // 1 ms before the ON step ends
    EXPECT_TRUE(output_active());

    loop_at(1080);  // ON step elapsed — advance to terminal LOW
    EXPECT_FALSE(output_active());
    EXPECT_EQ(arduino_mock::no_tone_calls, 0);

    loop_at(1081);  // pattern finished — further loops are no-ops
    EXPECT_FALSE(output_active());
    loop_at(5000);
    EXPECT_FALSE(output_active());
}

TEST_F(BuzzerPlaybackTest, InitDoesNotDriveKeyboardInterruptPin) {
    EXPECT_EQ(PIN_BUZZER, SIGURDOS_GPIO_DISABLED);
    EXPECT_EQ(PIN_KEYBOARD_INT, 46);
    EXPECT_EQ(arduino_mock::pin_mode_calls[PIN_KEYBOARD_INT], 0);
    EXPECT_FALSE(output_active());
    EXPECT_EQ(arduino_mock::no_tone_calls, 0)
        << "init must not call noTone before LEDC is attached";
}

TEST_F(BuzzerPlaybackTest, DoubleBeepPlaysOnGapOnSequence) {
    sigurdos::hal::buzzer_beep_double();
    EXPECT_EQ(arduino_mock::current_millis, 1000UL);
    EXPECT_TRUE(output_active());
    EXPECT_EQ(arduino_mock::tone_calls, 0);

    loop_at(1059);
    EXPECT_TRUE(output_active());
    loop_at(1060);  // first ON elapsed → gap
    EXPECT_FALSE(output_active());
    EXPECT_EQ(arduino_mock::no_tone_calls, 0);

    loop_at(1119);
    EXPECT_FALSE(output_active());
    loop_at(1120);  // gap elapsed → second ON
    EXPECT_TRUE(output_active());
    EXPECT_EQ(arduino_mock::tone_calls, 0);

    loop_at(1180);  // second ON elapsed → terminal LOW
    EXPECT_FALSE(output_active());
    loop_at(1181);
    EXPECT_FALSE(output_active());
}

TEST_F(BuzzerPlaybackTest, NewBeepRestartsActivePattern) {
    sigurdos::hal::buzzer_beep_double();
    loop_at(1060);  // into the gap step
    ASSERT_FALSE(output_active());

    arduino_mock::current_millis = 1070;
    sigurdos::hal::buzzer_beep_short();  // replaces the double pattern
    EXPECT_TRUE(output_active());

    loop_at(1149);  // short ON runs from 1070 → 1150
    EXPECT_TRUE(output_active());
    loop_at(1150);
    EXPECT_FALSE(output_active());

    // No resurrected double-pattern step later on.
    loop_at(1300);
    EXPECT_FALSE(output_active());
}

TEST_F(BuzzerPlaybackTest, SelfTestUsesDoublePattern) {
    sigurdos::hal::buzzer_self_test();
    EXPECT_TRUE(output_active());
    EXPECT_EQ(arduino_mock::tone_calls, 0);

    loop_at(1060);
    EXPECT_FALSE(output_active());
    loop_at(1120);
    EXPECT_TRUE(output_active());
    EXPECT_EQ(arduino_mock::tone_calls, 0);
}

} // namespace
