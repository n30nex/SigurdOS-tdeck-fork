// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include "../hal/buzzer.h"

namespace sigurdos::ui {

struct ActivityNotificationPlan {
    bool flash;
    bool buzz;
    sigurdos::hal::BuzzerPatternKind buzz_pattern;
};

inline ActivityNotificationPlan activity_notification_plan(bool got_new_activity,
                                                           bool buzzer_quiet,
                                                           bool incoming_message,
                                                           bool incoming_channel_msg)
{
    return {
        got_new_activity || incoming_message,
        incoming_message && !buzzer_quiet,
        incoming_channel_msg ? sigurdos::hal::BuzzerPatternKind::Double
                             : sigurdos::hal::BuzzerPatternKind::Short,
    };
}

} // namespace sigurdos::ui
