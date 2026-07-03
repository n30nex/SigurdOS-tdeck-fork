// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Ben

#include "repeat_button.h"

namespace sigurdos::ui {
namespace {

struct RepeatButtonCtx {
    RepeatButtonAction action;
    void* user_data;
    lv_timer_t* timer;
    uint16_t initial_delay_ms;
    uint16_t first_period_ms;
    uint16_t min_period_ms;
    uint16_t current_period_ms;
    bool fired;
};

static uint16_t next_period(uint16_t current, uint16_t minimum)
{
    if (current <= minimum) return minimum;
    const uint16_t stepped = current > 30 ? (uint16_t)(current - 30) : minimum;
    return stepped < minimum ? minimum : stepped;
}

static void repeat_timer_cb(lv_timer_t* timer)
{
    auto* ctx = static_cast<RepeatButtonCtx*>(lv_timer_get_user_data(timer));
    if (!ctx || !ctx->action) return;

    ctx->fired = true;
    ctx->action(ctx->user_data);

    if (ctx->current_period_ms == ctx->initial_delay_ms) {
        ctx->current_period_ms = ctx->first_period_ms;
    } else {
        ctx->current_period_ms = next_period(ctx->current_period_ms,
                                             ctx->min_period_ms);
    }
    lv_timer_set_period(timer, ctx->current_period_ms);
}

static void stop_timer(RepeatButtonCtx* ctx)
{
    if (!ctx || !ctx->timer) return;
    lv_timer_del(ctx->timer);
    ctx->timer = nullptr;
}

static void repeat_event_cb(lv_event_t* e)
{
    auto* ctx = static_cast<RepeatButtonCtx*>(lv_event_get_user_data(e));
    if (!ctx) return;

    const lv_event_code_t code = lv_event_get_code(e);
    switch (code) {
    case LV_EVENT_PRESSED:
        stop_timer(ctx);
        ctx->fired = false;
        ctx->current_period_ms = ctx->initial_delay_ms;
        ctx->timer = lv_timer_create(repeat_timer_cb, ctx->current_period_ms, ctx);
        break;
    case LV_EVENT_CLICKED:
        if (!ctx->fired && ctx->action) {
            ctx->action(ctx->user_data);
        }
        break;
    case LV_EVENT_RELEASED:
    case LV_EVENT_PRESS_LOST:
        stop_timer(ctx);
        break;
    case LV_EVENT_DELETE:
        stop_timer(ctx);
        delete ctx;
        break;
    default:
        break;
    }
}

} // namespace

void attach_hold_repeat(lv_obj_t* button,
                        RepeatButtonAction action,
                        void* user_data,
                        uint16_t initial_delay_ms,
                        uint16_t first_period_ms,
                        uint16_t min_period_ms)
{
    if (!button || !action) return;
    auto* ctx = new RepeatButtonCtx{
        action,
        user_data,
        nullptr,
        initial_delay_ms,
        first_period_ms,
        min_period_ms,
        initial_delay_ms,
        false,
    };
    lv_obj_add_event_cb(button, repeat_event_cb, LV_EVENT_ALL, ctx);
}

} // namespace sigurdos::ui
