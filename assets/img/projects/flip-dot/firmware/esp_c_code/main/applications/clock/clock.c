/**
 * @file clock.c
 * @brief HH:MM clock application implementation
 */

#include "clock.h"
#include "esp_log.h"
#include "esp_sntp.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "sdkconfig.h"
#include <string.h>
#include <time.h>
#include <sys/time.h>

static const char *TAG = "clock";

/* Update effect tuning — sweep order and per-pixel delay between flips. */
#define CLOCK_SWEEP_MODE                 SWEEP_RANDOM

/* Hour rollover: pre-settle noise with shorter per-pixel delays. */
#define CLOCK_HOUR_PIXEL_DELAY_MIN_MS    10
#define CLOCK_HOUR_PIXEL_DELAY_MAX_MS    50

/* Minute tick: random sweep only, longer delays, no noise. */
#define CLOCK_MINUTE_PIXEL_DELAY_MIN_MS  100
#define CLOCK_MINUTE_PIXEL_DELAY_MAX_MS  400

/* Pre-settle noise around pixels that are about to change (hour updates only). */
#define CLOCK_NOISE_ENABLED              true
#define CLOCK_NOISE_VICINITY_MARGIN      2
#define CLOCK_NOISE_INCLUDE_PROBABILITY  35
#define CLOCK_NOISE_FLIPS_MIN            2
#define CLOCK_NOISE_FLIPS_MAX            4
#define CLOCK_NOISE_FLIP_DELAY_MS        10

#define SNTP_SYNC_TIMEOUT_MS  30000
#define SNTP_POLL_INTERVAL_MS 100

/* Quiet hours: no clock updates, show "Godnatt" instead (local time). */
#define CLOCK_INACTIVE_START_HOUR  22
#define CLOCK_INACTIVE_END_HOUR    6

/* Set to true to always show the quiet-hours message (for display testing). */
#define CLOCK_FORCE_QUIET_TEST     false

#define QUIET_FONT_WIDTH   4
#define QUIET_FONT_HEIGHT  7
#define QUIET_CHAR_GAP     0
#define QUIET_TEXT         "GODNATT"

#define FONT_WIDTH  5
#define FONT_HEIGHT 7
#define CHAR_GAP    1
#define COLON_WIDTH 2

/* 5x7 glyphs: each byte is one column, bits 0..6 are rows (top to bottom). */
static const uint8_t GLYPH_DIGITS[10][FONT_WIDTH] = {
    {0x3E, 0x51, 0x49, 0x45, 0x3E}, /* 0 */
    {0x00, 0x42, 0x7F, 0x40, 0x00}, /* 1 */
    {0x42, 0x61, 0x51, 0x49, 0x46}, /* 2 */
    {0x21, 0x41, 0x45, 0x4B, 0x31}, /* 3 */
    {0x18, 0x14, 0x12, 0x7F, 0x10}, /* 4 */
    {0x27, 0x45, 0x45, 0x45, 0x39}, /* 5 */
    {0x3C, 0x4A, 0x49, 0x49, 0x30}, /* 6 */
    {0x01, 0x71, 0x09, 0x05, 0x03}, /* 7 */
    {0x36, 0x49, 0x49, 0x49, 0x36}, /* 8 */
    {0x06, 0x49, 0x49, 0x29, 0x1E}, /* 9 */
};

static const uint8_t GLYPH_COLON[COLON_WIDTH] = {0x00, 0x24};

/* 4x7 uppercase glyphs — 7 chars × 4 px = 28 px (full display width). */
static const uint8_t QUIET_GLYPH_G[4] = {0x3E, 0x41, 0x49, 0x3A};
static const uint8_t QUIET_GLYPH_O[4] = {0x3E, 0x41, 0x41, 0x3E};
static const uint8_t QUIET_GLYPH_D[4] = {0x7F, 0x41, 0x41, 0x3E};
static const uint8_t QUIET_GLYPH_N[4] = {0x7F, 0x04, 0x18, 0x7F};
static const uint8_t QUIET_GLYPH_A[4] = {0x7E, 0x09, 0x09, 0x7E};
static const uint8_t QUIET_GLYPH_T[4] = {0x01, 0x01, 0x7F, 0x01};

typedef enum {
    CLOCK_VIEW_TIME,
    CLOCK_VIEW_QUIET,
} clock_view_t;

static bool s_sntp_started;

static bool time_is_valid(time_t t)
{
    return t > 1577836800; /* 2020-01-01 */
}

static void clock_sntp_start(void)
{
    if (s_sntp_started) {
        return;
    }

    setenv("TZ", CONFIG_CLOCK_TIMEZONE, 1);
    tzset();

    esp_sntp_setoperatingmode(SNTP_OPMODE_POLL);
    esp_sntp_setservername(0, "pool.ntp.org");
    esp_sntp_init();
    s_sntp_started = true;
    ESP_LOGI(TAG, "SNTP started (timezone: %s)", CONFIG_CLOCK_TIMEZONE);
}

static bool clock_wait_for_sync(clock_app_abort_cb_t should_abort)
{
    const uint32_t step_ms = 200;
    uint32_t waited_ms = 0;

    while (waited_ms < SNTP_SYNC_TIMEOUT_MS) {
        if (should_abort && should_abort()) {
            return false;
        }

        time_t now;
        time(&now);
        if (time_is_valid(now)) {
            ESP_LOGI(TAG, "Time synced");
            return true;
        }

        vTaskDelay(step_ms / portTICK_PERIOD_MS);
        waited_ms += step_ms;
    }

    ESP_LOGE(TAG, "SNTP sync timed out after %u ms", SNTP_SYNC_TIMEOUT_MS);
    return false;
}

static bool clock_get_local_time(struct tm *local)
{
    time_t now;
    time(&now);
    if (!time_is_valid(now)) {
        return false;
    }
    localtime_r(&now, local);
    return true;
}

static bool clock_is_inactive_hour(int hour)
{
    return hour >= CLOCK_INACTIVE_START_HOUR || hour < CLOCK_INACTIVE_END_HOUR;
}

static void draw_glyph(uint8_t buffer[DISPLAY_HEIGHT][DISPLAY_WIDTH],
                       int x, int y,
                       const uint8_t *cols, int width, int height)
{
    for (int col = 0; col < width; col++) {
        int px = x + col;
        if (px < 0 || px >= DISPLAY_WIDTH) {
            continue;
        }
        for (int row = 0; row < height; row++) {
            int py = y + row;
            if (py < 0 || py >= DISPLAY_HEIGHT) {
                continue;
            }
            if ((cols[col] >> row) & 1) {
                buffer[py][px] = 1;
            }
        }
    }
}

static const uint8_t *clock_get_quiet_glyph(char c)
{
    switch (c) {
    case 'G': return QUIET_GLYPH_G;
    case 'O': return QUIET_GLYPH_O;
    case 'D': return QUIET_GLYPH_D;
    case 'N': return QUIET_GLYPH_N;
    case 'A': return QUIET_GLYPH_A;
    case 'T': return QUIET_GLYPH_T;
    default:  return NULL;
    }
}

static int quiet_line_width(const char *text)
{
    const int len = (int)strlen(text);
    if (len <= 0) {
        return 0;
    }
    return len * QUIET_FONT_WIDTH + (len - 1) * QUIET_CHAR_GAP;
}

static void clock_render_quiet_line(uint8_t buffer[DISPLAY_HEIGHT][DISPLAY_WIDTH],
                                    const char *text, int origin_y)
{
    const int total_w = quiet_line_width(text);
    int x = (DISPLAY_WIDTH - total_w) / 2;

    for (const char *c = text; *c != '\0'; c++) {
        const uint8_t *glyph = clock_get_quiet_glyph(*c);
        if (glyph) {
            draw_glyph(buffer, x, origin_y, glyph, QUIET_FONT_WIDTH, QUIET_FONT_HEIGHT);
        }
        x += QUIET_FONT_WIDTH + QUIET_CHAR_GAP;
    }
}

static int clock_string_width(void)
{
    return (FONT_WIDTH + CHAR_GAP) * 2 + COLON_WIDTH + CHAR_GAP
         + (FONT_WIDTH + CHAR_GAP) * 2 - CHAR_GAP;
}

static void clock_render(uint8_t buffer[DISPLAY_HEIGHT][DISPLAY_WIDTH],
                         uint8_t hh, uint8_t mm)
{
    memset(buffer, 0, DISPLAY_HEIGHT * DISPLAY_WIDTH);

    const int total_w = clock_string_width();
    const int origin_x = (DISPLAY_WIDTH - total_w) / 2;
    const int origin_y = (DISPLAY_HEIGHT - FONT_HEIGHT) / 2;

    uint8_t digits[4] = {
        hh / 10, hh % 10,
        mm / 10, mm % 10,
    };

    int x = origin_x;
    for (int i = 0; i < 4; i++) {
        if (i == 2) {
            draw_glyph(buffer, x, origin_y, GLYPH_COLON, COLON_WIDTH, FONT_HEIGHT);
            x += COLON_WIDTH + CHAR_GAP;
        }
        draw_glyph(buffer, x, origin_y, GLYPH_DIGITS[digits[i]], FONT_WIDTH, FONT_HEIGHT);
        x += FONT_WIDTH;
        if (i < 3) {
            x += CHAR_GAP;
        }
    }
}

static void clock_render_quiet(uint8_t buffer[DISPLAY_HEIGHT][DISPLAY_WIDTH])
{
    memset(buffer, 0, DISPLAY_HEIGHT * DISPLAY_WIDTH);
    const int origin_y = (DISPLAY_HEIGHT - QUIET_FONT_HEIGHT) / 2;
    clock_render_quiet_line(buffer, QUIET_TEXT, origin_y);
}

static void clock_apply_quiet_update_effect(flip_dot_t *display,
                                            flip_dot_noise_effect_t *noise)
{
    flip_dot_set_update_effect(display, CLOCK_SWEEP_MODE,
                               CLOCK_MINUTE_PIXEL_DELAY_MIN_MS,
                               CLOCK_MINUTE_PIXEL_DELAY_MAX_MS);
    noise->enabled = false;
    flip_dot_set_noise_effect(display, noise);
}

void clock_app_run(flip_dot_t *display, clock_app_abort_cb_t should_abort)
{
    uint8_t buffer[DISPLAY_HEIGHT][DISPLAY_WIDTH];
    int last_display_key = -1;
    clock_view_t last_view = CLOCK_VIEW_TIME;

    const sweep_mode_t prev_sweep = display->sweep_mode;
    const uint16_t prev_delay_min = display->pixel_delay_min_ms;
    const uint16_t prev_delay_max = display->pixel_delay_max_ms;
    const flip_dot_noise_effect_t prev_noise = display->noise_effect;

    flip_dot_noise_effect_t clock_noise = {
        .enabled = CLOCK_NOISE_ENABLED,
        .vicinity_margin = CLOCK_NOISE_VICINITY_MARGIN,
        .include_probability_pct = CLOCK_NOISE_INCLUDE_PROBABILITY,
        .noise_flips_min = CLOCK_NOISE_FLIPS_MIN,
        .noise_flips_max = CLOCK_NOISE_FLIPS_MAX,
        .noise_flip_delay_ms = CLOCK_NOISE_FLIP_DELAY_MS,
    };

    clock_sntp_start();
    if (!clock_wait_for_sync(should_abort)) {
        flip_dot_set_update_effect(display, prev_sweep, prev_delay_min, prev_delay_max);
        flip_dot_set_noise_effect(display, &prev_noise);
        return;
    }

    while (1) {
        if (should_abort && should_abort()) {
            break;
        }

        struct tm local;
        if (!clock_get_local_time(&local)) {
            ESP_LOGW(TAG, "Lost time sync, waiting...");
            if (!clock_wait_for_sync(should_abort)) {
                break;
            }
            continue;
        }

        const bool inactive = CLOCK_FORCE_QUIET_TEST
            || clock_is_inactive_hour(local.tm_hour);

        if (inactive) {
            if (last_view != CLOCK_VIEW_QUIET) {
                clock_apply_quiet_update_effect(display, &clock_noise);
                clock_render_quiet(buffer);
                flip_dot_update_display(display, buffer);
                last_view = CLOCK_VIEW_QUIET;
                last_display_key = -1;
                ESP_LOGI(TAG, "Quiet hours — showing %s", QUIET_TEXT);
            }
        } else {
            const int display_key = local.tm_hour * 60 + local.tm_min;
            const bool entering_active = last_view != CLOCK_VIEW_TIME;

            if (entering_active || display_key != last_display_key) {
                const bool hour_changed = entering_active || last_display_key < 0
                    || (last_display_key / 60) != local.tm_hour;

                if (hour_changed) {
                    flip_dot_set_update_effect(display, CLOCK_SWEEP_MODE,
                                               CLOCK_HOUR_PIXEL_DELAY_MIN_MS,
                                               CLOCK_HOUR_PIXEL_DELAY_MAX_MS);
                    clock_noise.enabled = true;
                } else {
                    flip_dot_set_update_effect(display, CLOCK_SWEEP_MODE,
                                               CLOCK_MINUTE_PIXEL_DELAY_MIN_MS,
                                               CLOCK_MINUTE_PIXEL_DELAY_MAX_MS);
                    clock_noise.enabled = false;
                }
                flip_dot_set_noise_effect(display, &clock_noise);

                clock_render(buffer, (uint8_t)local.tm_hour, (uint8_t)local.tm_min);
                flip_dot_update_display(display, buffer);
                last_view = CLOCK_VIEW_TIME;
                last_display_key = display_key;
                ESP_LOGI(TAG, "Display updated: %02d:%02d (%s)",
                         local.tm_hour, local.tm_min,
                         hour_changed ? "hour" : "minute");
            }
        }

        vTaskDelay(SNTP_POLL_INTERVAL_MS / portTICK_PERIOD_MS);
    }

    flip_dot_set_update_effect(display, prev_sweep, prev_delay_min, prev_delay_max);
    flip_dot_set_noise_effect(display, &prev_noise);
}
