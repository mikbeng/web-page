#include <stdio.h>
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "pwr_ctrl.h"
#include "esp_log.h"
#include "flip_dot.h"
#include "snake.h"
#include "clock.h"
#include "input.h"
#include "driver/gpio.h"
#include "esp_wifi.h"
#include "esp_mac.h"
#include "switch_input.h"

const static char *TAG = "MAIN";

static flip_dot_t flip_dot;

typedef enum {
    APP_MODE_SNAKE,
    APP_MODE_DEMO,
    APP_MODE_CLOCK,
} app_mode_t;

static const char *mode_name(app_mode_t mode)
{
    switch (mode) {
    case APP_MODE_SNAKE:  return "snake";
    case APP_MODE_DEMO:   return "demo";
    case APP_MODE_CLOCK:  return "clock";
    default:              return "unknown";
    }
}

static app_mode_t advance_mode(app_mode_t mode)
{
    return (app_mode_t)(((int)mode + 1) % 3);
}

static bool poll_mode_switch(flip_dot_t *display, app_mode_t *mode)
{
    switch_event_t event;
    if (switch_input_get_event(&event, 10) && event == SWITCH_EVENT_PRESSED) {
        *mode = advance_mode(*mode);
        flip_dot_clear_display(display);
        ESP_LOGI(TAG, "Switched to %s mode", mode_name(*mode));
        return true;
    }
    return false;
}

typedef struct {
    flip_dot_t *display;
    app_mode_t *mode;
} demo_abort_ctx_t;

static demo_abort_ctx_t demo_abort_ctx;

static bool demo_should_abort(void)
{
    return poll_mode_switch(demo_abort_ctx.display, demo_abort_ctx.mode);
}

static bool delay_with_abort(uint32_t delay_ms)
{
    const uint32_t step_ms = 10;
    uint32_t elapsed_ms = 0;

    while (elapsed_ms < delay_ms) {
        if (demo_should_abort()) {
            return true;
        }
        vTaskDelay(step_ms / portTICK_PERIOD_MS);
        elapsed_ms += step_ms;
    }
    return false;
}

static snake_game_t *g_snake_game = NULL;

static void snake_input_callback(input_event_t *event)
{
    if (!g_snake_game || !event) {
        return;
    }

    if (g_snake_game->state == GAME_INIT) {
        if (event->type == INPUT_TYPE_ESPNOW && event->is_pressed) {
            snake_game_start(g_snake_game);
        }
        return;
    }

    if (g_snake_game->state != GAME_RUNNING) {
        return;
    }

    snake_direction_t new_direction = g_snake_game->snake.direction;

    switch (event->command) {
    case INPUT_CMD_UP:    new_direction = DIR_UP;    break;
    case INPUT_CMD_DOWN:  new_direction = DIR_DOWN;  break;
    case INPUT_CMD_LEFT:  new_direction = DIR_LEFT;  break;
    case INPUT_CMD_RIGHT: new_direction = DIR_RIGHT; break;
    case INPUT_CMD_PAUSE:
        if (g_snake_game->state == GAME_RUNNING) {
            snake_game_pause(g_snake_game);
        } else if (g_snake_game->state == GAME_PAUSED) {
            snake_game_resume(g_snake_game);
        }
        return;
    case INPUT_CMD_RESET:
        snake_game_reset(g_snake_game);
        return;
    default:
        return;
    }

    direction_buffer_push(&g_snake_game->snake.input_buffer, new_direction);
}

static void show_snake_start_screen(snake_game_t *game)
{
    memset(game->game_buffer, 0, sizeof(game->game_buffer));
    for (int y = 2; y <= 4; y++) {
        for (int x = 0; x < DISPLAY_WIDTH; x++) {
            game->game_buffer[y][x] = 1;
        }
    }
    snake_game_render(game);
}

static void run_snake_mode(flip_dot_t *display, input_system_t *input_sys, app_mode_t *mode)
{
    ESP_LOGI(TAG, "Entering snake mode");

    snake_game_t game;
    snake_game_init(&game, display);
    g_snake_game = &game;
    input_sys->config.callback = snake_input_callback;
    show_snake_start_screen(&game);

    uint32_t last_update = xTaskGetTickCount() * portTICK_PERIOD_MS;

    while (*mode == APP_MODE_SNAKE) {
        if (poll_mode_switch(display, mode)) {
            break;
        }

        input_system_process(input_sys);

        uint32_t current_time = xTaskGetTickCount() * portTICK_PERIOD_MS;
        if (game.state == GAME_RUNNING && (current_time - last_update >= game.game_speed_ms)) {
            snake_game_update(&game);
            last_update = current_time;

            if (snake_game_is_over(&game)) {
                snake_game_show_game_over(&game);
                vTaskDelay(2000 / portTICK_PERIOD_MS);
                snake_game_reset(&game);
                show_snake_start_screen(&game);
            }
        }

        vTaskDelay(10 / portTICK_PERIOD_MS);
    }

    g_snake_game = NULL;
    input_sys->config.callback = NULL;
}

static void run_demo_mode(flip_dot_t *display, app_mode_t *mode)
{
    ESP_LOGI(TAG, "Entering demo mode");

    demo_abort_ctx.display = display;
    demo_abort_ctx.mode = mode;

    while (*mode == APP_MODE_DEMO) {
        ESP_LOGI(TAG, "Running bouncing ball demo...");
        flip_dot_demo_bouncing_ball(display, 30, demo_should_abort);
        if (*mode != APP_MODE_DEMO) {
            break;
        }

        if (delay_with_abort(5000)) {
            break;
        }

        ESP_LOGI(TAG, "Running sine wave demo...");
        flip_dot_demo_sine_wave(display, 150, demo_should_abort);
    }
}

static bool clock_should_abort(void)
{
    return poll_mode_switch(demo_abort_ctx.display, demo_abort_ctx.mode);
}

static void run_clock_mode(flip_dot_t *display, app_mode_t *mode)
{
    ESP_LOGI(TAG, "Entering clock mode");

    demo_abort_ctx.display = display;
    demo_abort_ctx.mode = mode;

    if (*mode == APP_MODE_CLOCK) {
        clock_app_run(display, clock_should_abort);
    }
}

void app_main(void)
{
    uint8_t mac[6];
    esp_read_mac(mac, ESP_MAC_WIFI_STA);
    ESP_LOGI(TAG, "Device MAC address: %02X:%02X:%02X:%02X:%02X:%02X",
             mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);

    init_power_control();
    switch_input_init();

    flip_dot_init(&flip_dot, 2000, SWEEP_ROW);

    ESP_LOGI(TAG, "Enabling flip board");
    disable_24V_supply();
    enable_flip_board();

    vTaskDelay(1000 / portTICK_PERIOD_MS);
    flip_dot_clear_display(&flip_dot);

    input_system_config_t input_config = input_get_default_config();
    input_config.enabled_types = INPUT_TYPE_ESPNOW;
    input_config.espnow_config = input_get_default_espnow_config();

    input_system_t input_sys;
    esp_err_t ret = input_system_init(&input_sys, &input_config);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to initialize input system: %s", esp_err_to_name(ret));
        return;
    }

    ret = input_system_start(&input_sys);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to start input system: %s", esp_err_to_name(ret));
        input_system_deinit(&input_sys);
        return;
    }

    app_mode_t mode = APP_MODE_CLOCK;
    ESP_LOGI(TAG, "Starting in %s mode (press switch to change)", mode_name(mode));

    while (1) {
        switch (mode) {
        case APP_MODE_SNAKE:
            run_snake_mode(&flip_dot, &input_sys, &mode);
            break;
        case APP_MODE_DEMO:
            run_demo_mode(&flip_dot, &mode);
            break;
        case APP_MODE_CLOCK:
            run_clock_mode(&flip_dot, &mode);
            break;
        }
    }
}
