// SPDX-License-Identifier: GPL-3.0-or-later

#include "buzzer.h"
#include "tdeck_pins.h"
#include <Arduino.h>

#if defined(ESP32_PLATFORM)
#include <driver/gpio.h>
#if __has_include(<driver/i2s_std.h>)
#define SIGURDOS_I2S_STD_DRIVER 1
#include <driver/i2s_std.h>
#else
#define SIGURDOS_I2S_LEGACY_DRIVER 1
#include <driver/i2s.h>
#endif
#include <esp_err.h>
#include <freertos/FreeRTOS.h>
#endif

namespace sigurdos {
namespace hal {

namespace {

const BuzzerPatternStep* s_pattern = nullptr;
std::size_t s_count = 0;
std::size_t s_idx = 0;
uint32_t s_step_started_ms = 0;
bool s_active = false;
bool s_output_on = false;
uint16_t s_frequency_hz = 0;

#if defined(ESP32_PLATFORM)
static constexpr uint32_t I2S_SAMPLE_RATE_HZ = 16000;
static constexpr size_t I2S_FRAMES_PER_CHUNK = 128;
static constexpr int16_t I2S_TONE_AMPLITUDE = 9000;

#if defined(SIGURDOS_I2S_STD_DRIVER)
i2s_chan_handle_t s_i2s_tx = nullptr;
#else
static constexpr i2s_port_t I2S_LEGACY_PORT = I2S_NUM_0;
#endif
bool s_i2s_ready = false;
uint32_t s_sample_cursor = 0;

bool speaker_init_i2s()
{
    if (s_i2s_ready) return true;

#if defined(SIGURDOS_I2S_STD_DRIVER)
    i2s_chan_config_t chan_cfg =
        I2S_CHANNEL_DEFAULT_CONFIG(I2S_NUM_AUTO, I2S_ROLE_MASTER);
    if (i2s_new_channel(&chan_cfg, &s_i2s_tx, nullptr) != ESP_OK || !s_i2s_tx) {
        s_i2s_tx = nullptr;
        return false;
    }

    i2s_std_config_t std_cfg = {};
    std_cfg.clk_cfg = I2S_STD_CLK_DEFAULT_CONFIG(I2S_SAMPLE_RATE_HZ);
    std_cfg.slot_cfg = I2S_STD_MSB_SLOT_DEFAULT_CONFIG(I2S_DATA_BIT_WIDTH_16BIT,
                                                       I2S_SLOT_MODE_STEREO);
    std_cfg.gpio_cfg.mclk = I2S_GPIO_UNUSED;
    std_cfg.gpio_cfg.bclk = static_cast<gpio_num_t>(PIN_I2S_BCK);
    std_cfg.gpio_cfg.ws = static_cast<gpio_num_t>(PIN_I2S_WS);
    std_cfg.gpio_cfg.dout = static_cast<gpio_num_t>(PIN_I2S_DOUT);
    std_cfg.gpio_cfg.din = I2S_GPIO_UNUSED;
    std_cfg.gpio_cfg.invert_flags.mclk_inv = false;
    std_cfg.gpio_cfg.invert_flags.bclk_inv = false;
    std_cfg.gpio_cfg.invert_flags.ws_inv = false;

    if (i2s_channel_init_std_mode(s_i2s_tx, &std_cfg) != ESP_OK ||
        i2s_channel_enable(s_i2s_tx) != ESP_OK) {
        i2s_del_channel(s_i2s_tx);
        s_i2s_tx = nullptr;
        return false;
    }

    s_i2s_ready = true;
    return true;
#else
    i2s_config_t i2s_cfg = {};
    i2s_cfg.mode = static_cast<i2s_mode_t>(I2S_MODE_MASTER | I2S_MODE_TX);
    i2s_cfg.sample_rate = I2S_SAMPLE_RATE_HZ;
    i2s_cfg.bits_per_sample = I2S_BITS_PER_SAMPLE_16BIT;
    i2s_cfg.channel_format = I2S_CHANNEL_FMT_RIGHT_LEFT;
    i2s_cfg.communication_format = I2S_COMM_FORMAT_STAND_I2S;
    i2s_cfg.intr_alloc_flags = 0;
    i2s_cfg.dma_buf_count = 4;
    i2s_cfg.dma_buf_len = I2S_FRAMES_PER_CHUNK;
    i2s_cfg.use_apll = false;
    i2s_cfg.tx_desc_auto_clear = true;
    i2s_cfg.fixed_mclk = 0;

    i2s_pin_config_t pin_cfg = {};
    pin_cfg.mck_io_num = I2S_PIN_NO_CHANGE;
    pin_cfg.bck_io_num = PIN_I2S_BCK;
    pin_cfg.ws_io_num = PIN_I2S_WS;
    pin_cfg.data_out_num = PIN_I2S_DOUT;
    pin_cfg.data_in_num = I2S_PIN_NO_CHANGE;

    if (i2s_driver_install(I2S_LEGACY_PORT, &i2s_cfg, 0, nullptr) != ESP_OK) {
        return false;
    }
    if (i2s_set_pin(I2S_LEGACY_PORT, &pin_cfg) != ESP_OK) {
        i2s_driver_uninstall(I2S_LEGACY_PORT);
        return false;
    }
    (void)i2s_zero_dma_buffer(I2S_LEGACY_PORT);
    s_i2s_ready = true;
    return true;
#endif
}

void speaker_write_silence()
{
#if defined(SIGURDOS_I2S_STD_DRIVER)
    if (!s_i2s_ready || !s_i2s_tx) return;
#else
    if (!s_i2s_ready) return;
#endif
    static int16_t silence[I2S_FRAMES_PER_CHUNK * 2] = {0};
    size_t written = 0;
#if defined(SIGURDOS_I2S_STD_DRIVER)
    (void)i2s_channel_write(s_i2s_tx, silence, sizeof(silence), &written, 0);
#else
    (void)i2s_write(I2S_LEGACY_PORT, silence, sizeof(silence), &written, 0);
#endif
}

void speaker_write_tone_chunk()
{
    if (!s_output_on || s_frequency_hz == 0) return;
    if (!speaker_init_i2s()) return;

    int16_t samples[I2S_FRAMES_PER_CHUNK * 2];
    const uint32_t period =
        (I2S_SAMPLE_RATE_HZ / s_frequency_hz) > 1
            ? (I2S_SAMPLE_RATE_HZ / s_frequency_hz)
            : 2;
    const uint32_t half_period = period / 2;

    for (size_t frame = 0; frame < I2S_FRAMES_PER_CHUNK; ++frame) {
        const int16_t v =
            ((s_sample_cursor % period) < half_period)
                ? I2S_TONE_AMPLITUDE
                : static_cast<int16_t>(-I2S_TONE_AMPLITUDE);
        samples[frame * 2] = v;
        samples[frame * 2 + 1] = v;
        s_sample_cursor++;
    }

    size_t written = 0;
#if defined(SIGURDOS_I2S_STD_DRIVER)
    (void)i2s_channel_write(s_i2s_tx, samples, sizeof(samples), &written, 0);
#else
    (void)i2s_write(I2S_LEGACY_PORT, samples, sizeof(samples), &written, 0);
#endif
}
#endif

void buzzer_stop_output()
{
    s_output_on = false;
    s_frequency_hz = 0;
#if defined(ESP32_PLATFORM)
    speaker_write_silence();
#endif
}

void buzzer_apply_step() {
    const BuzzerPatternStep& step = s_pattern[s_idx];
    if (step.tone_on) {
        s_output_on = true;
        s_frequency_hz = step.frequency_hz;
#if defined(ESP32_PLATFORM)
        speaker_write_tone_chunk();
#endif
    } else {
        buzzer_stop_output();
    }
    s_step_started_ms = millis();
    if (step.duration_ms == 0) {
        // Terminal marker: apply its output state, then idle LOW/silent.
        buzzer_stop_output();
        s_active = false;
    }
}

void buzzer_start_pattern(BuzzerPatternKind kind) {
    s_pattern = sigurdos_buzzer_pattern(kind, &s_count);
    s_idx = 0;
    s_active = s_pattern && s_count > 0;
    if (!s_active) {
        buzzer_stop_output();
        return;
    }
    buzzer_apply_step();
}

} // namespace

void buzzer_init() {
    s_pattern = nullptr;
    s_count = 0;
    s_idx = 0;
    s_active = false;
    s_step_started_ms = millis();
#if defined(ESP32_PLATFORM)
    s_sample_cursor = 0;
#endif
#if defined(ESP32_PLATFORM)
    (void)speaker_init_i2s();
#endif
    buzzer_stop_output();
}

void buzzer_loop() {
    if (!s_active) return;
    if (!s_pattern || s_idx >= s_count) {
        buzzer_stop_output();
        s_active = false;
        return;
    }
#if defined(ESP32_PLATFORM)
    speaker_write_tone_chunk();
#endif
    if (millis() - s_step_started_ms < s_pattern[s_idx].duration_ms) return;
    s_idx++;
    if (s_idx >= s_count) {
        buzzer_stop_output();
        s_active = false;
        return;
    }
    buzzer_apply_step();
}

void buzzer_beep_short() {
    buzzer_start_pattern(BuzzerPatternKind::Short);
}

void buzzer_beep_double() {
    buzzer_start_pattern(BuzzerPatternKind::Double);
}

void buzzer_self_test() {
    buzzer_start_pattern(BuzzerPatternKind::Double);
}

#if defined(SIGURDOS_NATIVE_PREFERENCES)
bool buzzer_output_active_for_test()
{
    return s_output_on;
}
#endif

} // namespace hal
} // namespace sigurdos
