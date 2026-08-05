/**
 * @file flip_dot.c
 * @brief Flip dot display driver implementation
 *
 * @author Mikael Bengtsson
 * @date 2025-05-18
 */

#include "flip_dot.h"
#include <string.h>  
#include "driver/gpio.h" 
#include "freertos/FreeRTOS.h" 
#include "freertos/task.h"
#include "esp_log.h"
#include <math.h>
#include <stdlib.h>

/******************************************************************************
 * Private Definitions and Types
 ******************************************************************************/

// Pin definitions - replace with your actual pin numbers
#define PIN_ROW_A0 13
#define PIN_ROW_A1 12
#define PIN_ROW_A2 27
#define PIN_COL_A0 26
#define PIN_COL_A1 25
#define PIN_COL_A2 5
#define PIN_COL_A3 19   //2A/2B header pin 18 Inverted!
#define PIN_ENABLE_1A0 33
#define PIN_ENABLE_1A1 15
#define PIN_ENABLE_2A0 32
#define PIN_ENABLE_2A1 14
#define PIN_ENABLE_1E 21
#define PIN_ENABLE_2E 4

static const char *TAG = "flip_dot";

/******************************************************************************
 * Private Function Declarations
 ******************************************************************************/
// GPIO functions
void gpio_write(uint8_t pin, bool value, bool is_inverted);

// Demux functions
void demux_74HC4514_init(demux_74HC4514_t *demux, 
                          gpio_pin_t pin_A0, 
                          gpio_pin_t pin_A1, 
                          gpio_pin_t pin_A2, 
                          gpio_pin_t pin_A3);
                          
void demux_74HC4514_set_output(demux_74HC4514_t *demux, uint8_t output_pos);

void demux_74HC139_init(demux_74HC139_t *demux,
                         gpio_pin_t pin_1A0,
                         gpio_pin_t pin_1A1,
                         gpio_pin_t pin_2A0,
                         gpio_pin_t pin_2A1,
                         gpio_pin_t pin_1E,
                         gpio_pin_t pin_2E);
                         
void demux_74HC139_set_row_output(demux_74HC139_t *demux, 
                                  uint8_t row_grp, 
                                  uint8_t output_pos, 
                                  demux_74HC4514_t *row_demux);
                                  
void demux_74HC139_set_col_output(demux_74HC139_t *demux, 
                                  uint8_t col_grp, 
                                  uint8_t output_pos, 
                                  demux_74HC4514_t *col_demux);
                                  
void demux_74HC139_enable_output(demux_74HC139_t *demux, uint8_t channel);
void demux_74HC139_disable_output(demux_74HC139_t *demux, uint8_t channel);

/******************************************************************************
 * Private Function Implementations
 ******************************************************************************/
// Delay function (microseconds)
static void delay_us(uint32_t us) {
    // Replace with your platform's microsecond delay
    uint32_t delay_ms = us / 1000; // Convert to milliseconds
    //ESP_LOGI(TAG, "Delaying for %ld milliseconds", delay_ms);
    vTaskDelay(delay_ms / portTICK_PERIOD_MS);
}

// Delay function (milliseconds)
static void delay_ms(uint32_t ms) {
    // Replace with your platform's millisecond delay
    vTaskDelay(ms / portTICK_PERIOD_MS);
}

void flip_dot_set_pixel(flip_dot_t *display, uint8_t row, uint8_t col, bool value);

// Converts a decimal number to binary array
static void decimal_to_bin(uint8_t number, uint8_t bits, uint8_t *binary_arr) {
    for (int i = bits-1; i >= 0; i--) {
        uint8_t mask = 1 << i;
        binary_arr[bits-1-i] = (mask & number) ? 1 : 0;
    }
}

/******************************************************************************
 * Public Function Implementations
 ******************************************************************************/

/******************************************************************************
 * GPIO Functions
 ******************************************************************************/

void gpio_write(uint8_t pin, bool value, bool is_inverted) {
    // Write value to GPIO pin, considering inversion
    gpio_set_level(pin, is_inverted ? !value : value);
}

/******************************************************************************
 * Demux Functions
 ******************************************************************************/

void demux_74HC4514_init(demux_74HC4514_t *demux, 
                          gpio_pin_t pin_A0, 
                          gpio_pin_t pin_A1, 
                          gpio_pin_t pin_A2, 
                          gpio_pin_t pin_A3) {
    demux->pin_A0 = pin_A0;
    demux->pin_A1 = pin_A1;
    demux->pin_A2 = pin_A2;
    demux->pin_A3 = pin_A3;

    //Initiaze GPIOs
    gpio_config_t io_config = {   
        .pin_bit_mask = (1ULL << pin_A0.pin) | (1ULL << pin_A1.pin) | (1ULL << pin_A2.pin),
        .mode = GPIO_MODE_OUTPUT,
        .pull_up_en = GPIO_PULLUP_DISABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE
    };
    
    if (pin_A3.pin != 0xFF) { // 0xFF indicates unused pin
        io_config.pin_bit_mask |= (1ULL << pin_A3.pin);
    }

    gpio_config(&io_config);

}

void demux_74HC4514_set_output(demux_74HC4514_t *demux, uint8_t output_pos) {
    uint8_t binary[4] = {0};
    
    // Convert output position to binary
    decimal_to_bin(output_pos, 4, binary);
    
    ESP_LOGD(TAG, "74HC4514 output_pos=%d -> binary=[%d,%d,%d,%d]", output_pos, binary[0], binary[1], binary[2], binary[3]);
    
    // Set output pins
    gpio_write(demux->pin_A0.pin, binary[3], demux->pin_A0.is_inverted);
    gpio_write(demux->pin_A1.pin, binary[2], demux->pin_A1.is_inverted);
    gpio_write(demux->pin_A2.pin, binary[1], demux->pin_A2.is_inverted);
    
    ESP_LOGD(TAG, "Setting pins: A0=%d, A1=%d, A2=%d", binary[3], binary[2], binary[1]);
    
    if (demux->pin_A3.pin != 0xFF) {
        gpio_write(demux->pin_A3.pin, !binary[0], demux->pin_A3.is_inverted); // A3 is inverted in the original code
        ESP_LOGD(TAG, "Setting pin A3=%d (inverted from %d)", !binary[0], binary[0]);
    }
}

void demux_74HC139_init(demux_74HC139_t *demux,
                         gpio_pin_t pin_1A0,
                         gpio_pin_t pin_1A1,
                         gpio_pin_t pin_2A0,
                         gpio_pin_t pin_2A1,
                         gpio_pin_t pin_1E,
                         gpio_pin_t pin_2E) {
    demux->pin_1A0 = pin_1A0;
    demux->pin_1A1 = pin_1A1;
    demux->pin_2A0 = pin_2A0;
    demux->pin_2A1 = pin_2A1;
    demux->pin_1E = pin_1E;
    demux->pin_2E = pin_2E;
    
    //Initiaze GPIOs
    gpio_config_t io_config = {   
        .pin_bit_mask = (1ULL << pin_1A0.pin) | (1ULL << pin_1A1.pin) | (1ULL << pin_2A0.pin) | (1ULL << pin_2A1.pin) | (1ULL << pin_1E.pin) | (1ULL << pin_2E.pin),
        .mode = GPIO_MODE_OUTPUT,
        .pull_up_en = GPIO_PULLUP_DISABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE
    };
    gpio_config(&io_config);
}

void demux_74HC139_set_row_output(demux_74HC139_t *demux, 
                                  uint8_t row_grp, 
                                  uint8_t output_pos, 
                                  demux_74HC4514_t *row_demux) {
    uint8_t binary[2] = {0};
    
    // Convert row group to binary
    decimal_to_bin(row_grp, 2, binary);
    
    ESP_LOGD(TAG, "74HC139 ROW: row_grp=%d -> binary=[%d,%d]", row_grp, binary[0], binary[1]);
    
    // Set row group pins
    gpio_write(demux->pin_1A0.pin, binary[1], demux->pin_1A0.is_inverted);
    gpio_write(demux->pin_1A1.pin, binary[0], demux->pin_1A1.is_inverted);
    
    ESP_LOGD(TAG, "Setting row group pins: 1A0=%d, 1A1=%d", binary[1], binary[0]);
    
    // Set row output position
    ESP_LOGD(TAG, "Setting ROW demux for output_pos=%d", output_pos);
    demux_74HC4514_set_output(row_demux, output_pos);
}

void demux_74HC139_set_col_output(demux_74HC139_t *demux, 
                                  uint8_t col_grp, 
                                  uint8_t output_pos, 
                                  demux_74HC4514_t *col_demux) {
    uint8_t binary[2] = {0};
    
    // Convert column group to binary
    decimal_to_bin(col_grp, 2, binary);
    
    ESP_LOGD(TAG, "74HC139 COL: col_grp=%d -> binary=[%d,%d]", col_grp, binary[0], binary[1]);
    
    // Set column group pins
    gpio_write(demux->pin_2A0.pin, binary[1], demux->pin_2A0.is_inverted);
    gpio_write(demux->pin_2A1.pin, binary[0], demux->pin_2A1.is_inverted);
    
    ESP_LOGD(TAG, "Setting col group pins: 2A0=%d, 2A1=%d", binary[1], binary[0]);
    
    // Set column output position
    ESP_LOGD(TAG, "Setting COL demux for output_pos=%d", output_pos);
    demux_74HC4514_set_output(col_demux, output_pos);
}



/******************************************************************************
 * Flip Dot Functions
 ******************************************************************************/
void demux_74HC139_enable_output(demux_74HC139_t *demux, uint8_t channel) {
    if (channel == 1) {
        gpio_write(demux->pin_1E.pin, true, demux->pin_1E.is_inverted);
    } else if (channel == 2) {
        gpio_write(demux->pin_2E.pin, true, demux->pin_2E.is_inverted);
    }
}

void demux_74HC139_disable_output(demux_74HC139_t *demux, uint8_t channel) {
    if (channel == 1) {
        gpio_write(demux->pin_1E.pin, false, demux->pin_1E.is_inverted);
    } else if (channel == 2) {
        gpio_write(demux->pin_2E.pin, false, demux->pin_2E.is_inverted);
    }
}


void flip_dot_init(flip_dot_t *display, uint32_t flip_time_us, sweep_mode_t sweep_mode) {
    
    ESP_LOGI(TAG, "Initializing flip dot");
    // Setup pin configurations
    gpio_pin_t row_A0 = {PIN_ROW_A0, false};
    gpio_pin_t row_A1 = {PIN_ROW_A1, false};
    gpio_pin_t row_A2 = {PIN_ROW_A2, false};
    gpio_pin_t row_A3 = {0xFF, false};  // Not used for rows
    
    gpio_pin_t col_A0 = {PIN_COL_A0, false};
    gpio_pin_t col_A1 = {PIN_COL_A1, false};
    gpio_pin_t col_A2 = {PIN_COL_A2, false};
    gpio_pin_t col_A3 = {PIN_COL_A3, true};
    
    gpio_pin_t enable_1A0 = {PIN_ENABLE_1A0, false};
    gpio_pin_t enable_1A1 = {PIN_ENABLE_1A1, false};
    gpio_pin_t enable_2A0 = {PIN_ENABLE_2A0, false};
    gpio_pin_t enable_2A1 = {PIN_ENABLE_2A1, false};
    gpio_pin_t enable_1E = {PIN_ENABLE_1E, false};
    gpio_pin_t enable_2E = {PIN_ENABLE_2E, false};  
    
    // Initialize demuxes
    demux_74HC4514_init(&display->row_demux, row_A0, row_A1, row_A2, row_A3);
    demux_74HC4514_init(&display->col_demux, col_A0, col_A1, col_A2, col_A3);
    demux_74HC139_init(&display->enable_demux, enable_1A0, enable_1A1, enable_2A0, enable_2A1, enable_1E, enable_2E);
    
    // gpio_pin_t pins_to_flip[] = {enable_1E, enable_1A0, enable_1A1, enable_2A0, enable_2A1, col_A0, col_A1, col_A2, col_A3, row_A0, row_A1, row_A2, row_A3};

    // //Create an array with the pin names
    // const char *pin_names[] = {"enable_1E", "enable_1A0", "enable_1A1", "enable_2A0", "enable_2A1", "col_A0", "col_A1", "col_A2", "col_A3", "row_A0", "row_A1", "row_A2", "row_A3"};

    // while(1) {
    //     //Flip all gpios 20 times each in a while loop
    //     for (int j = 0; j < sizeof(pins_to_flip) / sizeof(pins_to_flip[0]); j++) {
    //         ESP_LOGI(TAG, "Flipping pin %s", pin_names[j]);
    //         for (int i = 0; i < 20; i++) {
    //             gpio_write(pins_to_flip[j].pin, true, pins_to_flip[j].is_inverted);
    //             delay_ms(1000);
    //             gpio_write(pins_to_flip[j].pin, false, pins_to_flip[j].is_inverted);
    //             delay_ms(1000);
    //             }
    //         }
    // }

    // Set parameters
    display->flip_time_us = flip_time_us;
    display->sweep_mode = sweep_mode;
    display->pixel_delay_min_ms = 0;
    display->pixel_delay_max_ms = 0;
    display->noise_effect.enabled = false;

    // Initialize pixel state to all zeros
    memset(display->pixel_state, 0, sizeof(display->pixel_state));
    
    // Enable row output
    demux_74HC139_enable_output(&display->enable_demux, 1);
    gpio_write(enable_2E.pin, false, enable_2E.is_inverted);
    delay_ms(200);
    
    delay_ms(200);
}

void flip_dot_set_update_effect(flip_dot_t *display, sweep_mode_t sweep_mode,
                                uint16_t pixel_delay_min_ms, uint16_t pixel_delay_max_ms)
{
    display->sweep_mode = sweep_mode;
    display->pixel_delay_min_ms = pixel_delay_min_ms;
    display->pixel_delay_max_ms = pixel_delay_max_ms;
}

void flip_dot_set_noise_effect(flip_dot_t *display, const flip_dot_noise_effect_t *effect)
{
    if (effect) {
        display->noise_effect = *effect;
    } else {
        display->noise_effect.enabled = false;
    }
}

static uint16_t random_pixel_delay_ms(uint16_t min_ms, uint16_t max_ms)
{
    if (max_ms <= min_ms) {
        return min_ms;
    }
    return min_ms + (rand() % (max_ms - min_ms + 1));
}

static uint8_t random_noise_flip_count(const flip_dot_noise_effect_t *fx)
{
    if (fx->noise_flips_max <= fx->noise_flips_min) {
        return fx->noise_flips_min;
    }
    return fx->noise_flips_min + (rand() % (fx->noise_flips_max - fx->noise_flips_min + 1));
}

static bool find_nth_active_pixel(const uint8_t remaining[DISPLAY_HEIGHT][DISPLAY_WIDTH],
                                  uint16_t n, uint8_t *row, uint8_t *col)
{
    uint16_t idx = 0;
    for (uint8_t r = 0; r < DISPLAY_HEIGHT; r++) {
        for (uint8_t c = 0; c < DISPLAY_WIDTH; c++) {
            if (remaining[r][c] == 0) {
                continue;
            }
            if (idx == n) {
                *row = r;
                *col = c;
                return true;
            }
            idx++;
        }
    }
    return false;
}

static void flip_dot_run_noise_phase(flip_dot_t *display,
                                     const uint8_t data[DISPLAY_HEIGHT][DISPLAY_WIDTH],
                                     const uint8_t flip_list[DISPLAY_HEIGHT * DISPLAY_WIDTH][2],
                                     uint16_t flip_count)
{
    const flip_dot_noise_effect_t *fx = &display->noise_effect;
    if (!fx->enabled || fx->noise_flips_max == 0 || flip_count == 0) {
        return;
    }

    uint8_t min_r = DISPLAY_HEIGHT - 1;
    uint8_t max_r = 0;
    uint8_t min_c = DISPLAY_WIDTH - 1;
    uint8_t max_c = 0;

    for (uint16_t i = 0; i < flip_count; i++) {
        uint8_t r = flip_list[i][0];
        uint8_t c = flip_list[i][1];
        if (r < min_r) min_r = r;
        if (r > max_r) max_r = r;
        if (c < min_c) min_c = c;
        if (c > max_c) max_c = c;
    }

    if (fx->vicinity_margin > 0) {
        min_r = (min_r > fx->vicinity_margin) ? min_r - fx->vicinity_margin : 0;
        max_r = (max_r + fx->vicinity_margin < DISPLAY_HEIGHT) ? max_r + fx->vicinity_margin : DISPLAY_HEIGHT - 1;
        min_c = (min_c > fx->vicinity_margin) ? min_c - fx->vicinity_margin : 0;
        max_c = (max_c + fx->vicinity_margin < DISPLAY_WIDTH) ? max_c + fx->vicinity_margin : DISPLAY_WIDTH - 1;
    }

    uint8_t remaining[DISPLAY_HEIGHT][DISPLAY_WIDTH] = {0};
    uint16_t active_count = 0;

    for (uint8_t r = min_r; r <= max_r; r++) {
        for (uint8_t c = min_c; c <= max_c; c++) {
            bool is_target = data[r][c] != display->pixel_state[r][c];
            bool include = is_target;

            if (!include && fx->include_probability_pct > 0) {
                include = (rand() % 100) < fx->include_probability_pct;
            }
            if (!include) {
                continue;
            }

            uint8_t flips = random_noise_flip_count(fx);
            if (flips == 0) {
                continue;
            }

            remaining[r][c] = flips;
            active_count++;
        }
    }

    while (active_count > 0) {
        uint8_t r;
        uint8_t c;
        if (!find_nth_active_pixel(remaining, rand() % active_count, &r, &c)) {
            break;
        }

        flip_dot_set_pixel(display, r, c, !display->pixel_state[r][c]);
        remaining[r][c]--;
        if (remaining[r][c] == 0) {
            active_count--;
        }

        if (fx->noise_flip_delay_ms > 0) {
            delay_ms(fx->noise_flip_delay_ms);
        }
    }
}

static void flip_dot_apply_settle_phase(flip_dot_t *display,
                                        const uint8_t data[DISPLAY_HEIGHT][DISPLAY_WIDTH],
                                        uint8_t flip_list[DISPLAY_HEIGHT * DISPLAY_WIDTH][2],
                                        uint16_t flip_count)
{
    switch (display->sweep_mode) {
        case SWEEP_ROW:
            break;

        case SWEEP_COL:
            for (uint16_t i = 0; i < flip_count; i++) {
                for (uint16_t j = i + 1; j < flip_count; j++) {
                    if (flip_list[i][1] > flip_list[j][1]) {
                        uint8_t temp_r = flip_list[i][0];
                        uint8_t temp_c = flip_list[i][1];
                        flip_list[i][0] = flip_list[j][0];
                        flip_list[i][1] = flip_list[j][1];
                        flip_list[j][0] = temp_r;
                        flip_list[j][1] = temp_c;
                    }
                }
            }
            break;

        case SWEEP_RANDOM:
            for (uint16_t i = flip_count - 1; i > 0; i--) {
                uint16_t j = rand() % (i + 1);
                uint8_t temp_r = flip_list[i][0];
                uint8_t temp_c = flip_list[i][1];
                flip_list[i][0] = flip_list[j][0];
                flip_list[i][1] = flip_list[j][1];
                flip_list[j][0] = temp_r;
                flip_list[j][1] = temp_c;
            }
            break;

        case SWEEP_DIAG:
            break;
    }

    for (uint16_t i = 0; i < flip_count; i++) {
        uint8_t r = flip_list[i][0];
        uint8_t c = flip_list[i][1];
        flip_dot_set_pixel(display, r, c, data[r][c]);

        if (i + 1 < flip_count && display->pixel_delay_max_ms > 0) {
            delay_ms(random_pixel_delay_ms(display->pixel_delay_min_ms,
                                           display->pixel_delay_max_ms));
        }
    }

    memcpy(display->pixel_state, data, sizeof(display->pixel_state));
}

void flip_dot_set_pixel(flip_dot_t *display, uint8_t row, uint8_t col, bool value) {
    uint8_t row_grp = row / 7;
    uint8_t row_grp_pixel = row % 7;
    
    uint8_t col_grp = col / 7;
    uint8_t col_grp_pixel = col % 7;
    
    ESP_LOGD(TAG, "Setting pixel (%d,%d) = %d", row, col, value);
    ESP_LOGD(TAG, "Row: grp=%d, pixel=%d | Col: grp=%d, pixel=%d", row_grp, row_grp_pixel, col_grp, col_grp_pixel);
    
    // Set row output
    uint8_t row_output_pos = row_grp_pixel + 1 + ((!value) * 8);
    ESP_LOGD(TAG, "Row output position: %d", row_output_pos);
    demux_74HC139_set_row_output(&display->enable_demux, row_grp, row_output_pos, &display->row_demux);
    
    // Set column output
    uint8_t col_output_pos = col_grp_pixel + 1 + ((!value) * 8);
    ESP_LOGD(TAG, "Column output position: %d", col_output_pos);
    demux_74HC139_set_col_output(&display->enable_demux, col_grp, col_output_pos, &display->col_demux);
    
    // Send column enable pulse
    ESP_LOGD(TAG, "Sending enable pulse (pin=%d, inverted=%d)", display->enable_demux.pin_2E.pin, display->enable_demux.pin_2E.is_inverted);
    gpio_write(display->enable_demux.pin_2E.pin, true, display->enable_demux.pin_2E.is_inverted);
    delay_us(display->flip_time_us);
    gpio_write(display->enable_demux.pin_2E.pin, false, display->enable_demux.pin_2E.is_inverted);
    delay_us(1000); //Some recover delay for the capacitor to discharge completely. This can maybe be lower than 1ms, but since freertos is configured to 1000hz, smaller delays are not possible.
    
    // Update pixel state in memory
    display->pixel_state[row][col] = value;
}

void flip_dot_clear_display(flip_dot_t *display) {
    ESP_LOGI(TAG, "Clearing display");
    for (uint8_t r = 0; r < DISPLAY_HEIGHT; r++) {
        for (uint8_t c = 0; c < DISPLAY_WIDTH; c++) {
            //ESP_LOGI(TAG, "Clearing pixel (%d,%d)", r, c);
            flip_dot_set_pixel(display, r, c, false);
        }
    }
}

void flip_dot_update_display(flip_dot_t *display, const uint8_t data[DISPLAY_HEIGHT][DISPLAY_WIDTH]) {
    uint8_t flip_list[DISPLAY_HEIGHT * DISPLAY_WIDTH][2];
    uint16_t flip_count = 0;

    for (uint8_t r = 0; r < DISPLAY_HEIGHT; r++) {
        for (uint8_t c = 0; c < DISPLAY_WIDTH; c++) {
            if (data[r][c] != display->pixel_state[r][c]) {
                flip_list[flip_count][0] = r;
                flip_list[flip_count][1] = c;
                flip_count++;
            }
        }
    }

    if (flip_count == 0) {
        return;
    }

    flip_dot_run_noise_phase(display, data, flip_list, flip_count);

    /* Rebuild flip list — noise may have moved some pixels onto their target. */
    flip_count = 0;
    for (uint8_t r = 0; r < DISPLAY_HEIGHT; r++) {
        for (uint8_t c = 0; c < DISPLAY_WIDTH; c++) {
            if (data[r][c] != display->pixel_state[r][c]) {
                flip_list[flip_count][0] = r;
                flip_list[flip_count][1] = c;
                flip_count++;
            }
        }
    }

    if (flip_count == 0) {
        memcpy(display->pixel_state, data, sizeof(display->pixel_state));
        return;
    }

    flip_dot_apply_settle_phase(display, data, flip_list, flip_count);
}

void flip_dot_set_rows_cols(flip_dot_t *display, uint8_t row_start, uint8_t row_end, uint8_t col_start, uint8_t col_end, bool pixel_value) {
    for (uint8_t r = row_start; r <= row_end; r++) {
        for (uint8_t c = col_start; c <= col_end; c++) {
            flip_dot_set_pixel(display, r, c, pixel_value);
        }
    }
}

void flip_dot_debug_pixel_calc(uint8_t row, uint8_t col, bool value) {
    uint8_t row_grp = row / 7;
    uint8_t row_grp_pixel = row % 7;
    uint8_t col_grp = col / 7;
    uint8_t col_grp_pixel = col % 7;
    
    uint8_t row_output_pos = row_grp_pixel + 1 + (value * 8);
    uint8_t col_output_pos = col_grp_pixel + 1 + (value * 8);
    
    ESP_LOGI(TAG, "DEBUG CALC for pixel (%d,%d) = %d:", row, col, value);
    ESP_LOGI(TAG, "  Row: grp=%d, pixel_in_grp=%d, output_pos=%d", row_grp, row_grp_pixel, row_output_pos);
    ESP_LOGI(TAG, "  Col: grp=%d, pixel_in_grp=%d, output_pos=%d", col_grp, col_grp_pixel, col_output_pos);
    
    // Show binary representation
    uint8_t row_binary[4], col_binary[4];
    decimal_to_bin(row_output_pos, 4, row_binary);
    decimal_to_bin(col_output_pos, 4, col_binary);
    
    ESP_LOGI(TAG, "  Row binary: [%d,%d,%d,%d] -> A0=%d, A1=%d, A2=%d", 
             row_binary[0], row_binary[1], row_binary[2], row_binary[3],
             row_binary[3], row_binary[2], row_binary[1]);
             
    ESP_LOGI(TAG, "  Col binary: [%d,%d,%d,%d] -> A0=%d, A1=%d, A2=%d, A3=%d", 
             col_binary[0], col_binary[1], col_binary[2], col_binary[3],
             col_binary[3], col_binary[2], col_binary[1], !col_binary[0]);
}

/******************************************************************************
 * Demo Functions
 ******************************************************************************/

static bool demo_frame_delay(uint32_t delay_ms, flip_dot_demo_abort_cb_t should_abort)
{
    const uint32_t step_ms = 10;
    uint32_t elapsed_ms = 0;

    while (elapsed_ms < delay_ms) {
        if (should_abort && should_abort()) {
            return true;
        }
        vTaskDelay(step_ms / portTICK_PERIOD_MS);
        elapsed_ms += step_ms;
    }
    return false;
}

static bool demo_should_stop(flip_dot_demo_abort_cb_t should_abort)
{
    return should_abort && should_abort();
}

void flip_dot_demo_sine_wave(flip_dot_t *display, uint32_t delay_ms, flip_dot_demo_abort_cb_t should_abort) {
    ESP_LOGI(TAG, "Starting sine wave demo");
    
    uint8_t display_buffer[DISPLAY_HEIGHT][DISPLAY_WIDTH];
    
    for (uint32_t frame = 0; frame < 200; frame++) {
        if (demo_should_stop(should_abort)) {
            return;
        }

        // Clear the buffer
        memset(display_buffer, 0, sizeof(display_buffer));
        
        for (uint8_t col = 0; col < DISPLAY_WIDTH; col++) {
            // Create a sine wave that moves across the display
            float angle = (float)(col + frame) * 0.3f;
            float sine_val = sinf(angle);
            
            // Map sine wave (-1 to 1) to display height
            int row = (int)(((sine_val + 1.0f) / 2.0f) * (DISPLAY_HEIGHT - 1));
            
            if (row >= 0 && row < DISPLAY_HEIGHT) {
                display_buffer[row][col] = 1;
                
                // Add a second wave offset by 90 degrees
                float angle2 = angle + (M_PI / 2.0f);
                float sine_val2 = sinf(angle2);
                int row2 = (int)(((sine_val2 + 1.0f) / 2.0f) * (DISPLAY_HEIGHT - 1));
                
                if (row2 >= 0 && row2 < DISPLAY_HEIGHT && row2 != row) {
                    display_buffer[row2][col] = 1;
                }
            }
        }
        
        // Update only changed pixels
        flip_dot_update_display(display, display_buffer);
        if (demo_frame_delay(delay_ms, should_abort)) {
            return;
        }
    }
}

void flip_dot_demo_bouncing_ball(flip_dot_t *display, uint32_t delay_ms, flip_dot_demo_abort_cb_t should_abort) {
    ESP_LOGI(TAG, "Starting bouncing ball demo");
    
    uint8_t display_buffer[DISPLAY_HEIGHT][DISPLAY_WIDTH];
    float ball_x = 5.0f;
    float ball_y = 5.0f;
    float vel_x = 0.8f;
    float vel_y = 0.6f;
    
    for (uint32_t frame = 0; frame < 300; frame++) {
        if (demo_should_stop(should_abort)) {
            return;
        }

        // Clear the buffer
        memset(display_buffer, 0, sizeof(display_buffer));
        
        // Update ball position
        ball_x += vel_x;
        ball_y += vel_y;
        
        // Bounce off walls
        if (ball_x <= 1 || ball_x >= DISPLAY_WIDTH - 2) {
            vel_x = -vel_x;
            ball_x = (ball_x <= 1) ? 1 : DISPLAY_WIDTH - 2;
        }
        if (ball_y <= 1 || ball_y >= DISPLAY_HEIGHT - 2) {
            vel_y = -vel_y;
            ball_y = (ball_y <= 1) ? 1 : DISPLAY_HEIGHT - 2;
        }
        
        // Draw ball (3x3 square) in buffer
        int center_x = (int)ball_x;
        int center_y = (int)ball_y;
        
        for (int dy = -1; dy <= 1; dy++) {
            for (int dx = -1; dx <= 1; dx++) {
                int x = center_x + dx;
                int y = center_y + dy;
                if (x >= 0 && x < DISPLAY_WIDTH && y >= 0 && y < DISPLAY_HEIGHT) {
                    display_buffer[y][x] = 1;
                }
            }
        }
        
        // Update only changed pixels
        flip_dot_update_display(display, display_buffer);
        if (demo_frame_delay(delay_ms, should_abort)) {
            return;
        }
    }
}

void flip_dot_demo_matrix_rain(flip_dot_t *display, uint32_t delay_ms, flip_dot_demo_abort_cb_t should_abort) {
    ESP_LOGI(TAG, "Starting matrix rain demo");
    
    uint8_t display_buffer[DISPLAY_HEIGHT][DISPLAY_WIDTH];
    // Array to track rain drops for each column
    int8_t drop_pos[DISPLAY_WIDTH];
    uint8_t drop_length[DISPLAY_WIDTH];
    
    // Initialize rain drops
    for (uint8_t col = 0; col < DISPLAY_WIDTH; col++) {
        drop_pos[col] = -(rand() % 10);  // Start at different times
        drop_length[col] = 3 + (rand() % 5);  // Random length 3-7
    }
    
    for (uint32_t frame = 0; frame < 500; frame++) {
        if (demo_should_stop(should_abort)) {
            return;
        }

        // Clear the buffer
        memset(display_buffer, 0, sizeof(display_buffer));
        
        for (uint8_t col = 0; col < DISPLAY_WIDTH; col++) {
            // Draw the rain drop trail
            for (uint8_t i = 0; i < drop_length[col]; i++) {
                int row = drop_pos[col] - i;
                if (row >= 0 && row < DISPLAY_HEIGHT) {
                    // Fade effect: head of drop is bright, tail gets dimmer
                    bool pixel_on = (i < drop_length[col] / 2) || (rand() % 3 == 0);
                    if (pixel_on) {
                        display_buffer[row][col] = 1;
                    }
                }
            }
            
            // Move drop down
            drop_pos[col]++;
            
            // Reset drop when it goes off screen
            if (drop_pos[col] > DISPLAY_HEIGHT + drop_length[col]) {
                drop_pos[col] = -(rand() % 10);
                drop_length[col] = 3 + (rand() % 5);
            }
        }
        
        // Update only changed pixels
        flip_dot_update_display(display, display_buffer);
        if (demo_frame_delay(delay_ms, should_abort)) {
            return;
        }
    }
}

void flip_dot_demo_ripple_effect(flip_dot_t *display, uint32_t delay_ms, flip_dot_demo_abort_cb_t should_abort) {
    ESP_LOGI(TAG, "Starting ripple effect demo");
    
    uint8_t display_buffer[DISPLAY_HEIGHT][DISPLAY_WIDTH];
    float center_x = DISPLAY_WIDTH / 2.0f;
    float center_y = DISPLAY_HEIGHT / 2.0f;
    
    for (uint32_t frame = 0; frame < 100; frame++) {
        if (demo_should_stop(should_abort)) {
            return;
        }

        // Clear the buffer
        memset(display_buffer, 0, sizeof(display_buffer));
        
        float radius1 = frame * 0.5f;
        float radius2 = (frame > 20) ? (frame - 20) * 0.5f : 0;
        float radius3 = (frame > 40) ? (frame - 40) * 0.5f : 0;
        
        for (uint8_t row = 0; row < DISPLAY_HEIGHT; row++) {
            for (uint8_t col = 0; col < DISPLAY_WIDTH; col++) {
                float dist = sqrtf((col - center_x) * (col - center_x) + 
                                 (row - center_y) * (row - center_y));
                
                // Check if pixel is on any of the ripple rings
                bool on_ripple = false;
                if (fabs(dist - radius1) < 0.8f && radius1 > 0) on_ripple = true;
                if (fabs(dist - radius2) < 0.8f && radius2 > 0) on_ripple = true;
                if (fabs(dist - radius3) < 0.8f && radius3 > 0) on_ripple = true;
                
                if (on_ripple) {
                    display_buffer[row][col] = 1;
                }
            }
        }
        
        // Update only changed pixels
        flip_dot_update_display(display, display_buffer);
        if (demo_frame_delay(delay_ms, should_abort)) {
            return;
        }
    }
}

void flip_dot_demo_scrolling_text(flip_dot_t *display, const char* text, uint32_t delay_ms, flip_dot_demo_abort_cb_t should_abort) {
    ESP_LOGI(TAG, "Starting scrolling text demo: %s", text);
    
    uint8_t display_buffer[DISPLAY_HEIGHT][DISPLAY_WIDTH];
    
    // Simple 5x7 font for basic characters
    const uint8_t font_A[7] = {0x1F, 0x24, 0x24, 0x24, 0x1F, 0x00, 0x00};
    const uint8_t font_space[7] = {0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00};
    
    int text_len = strlen(text);
    int total_width = text_len * 6;  // 5 pixels + 1 space per character
    
    for (int offset = DISPLAY_WIDTH; offset > -total_width; offset--) {
        if (demo_should_stop(should_abort)) {
            return;
        }

        // Clear the buffer
        memset(display_buffer, 0, sizeof(display_buffer));
        
        for (int char_idx = 0; char_idx < text_len; char_idx++) {
            int char_x = offset + char_idx * 6;
            
            const uint8_t* font_data;
            if (text[char_idx] == 'A' || text[char_idx] == 'a') {
                font_data = font_A;
            } else {
                font_data = font_space;  // Default to space for unsupported chars
            }
            
            // Draw character in buffer
            for (int col = 0; col < 5; col++) {
                if (char_x + col >= 0 && char_x + col < DISPLAY_WIDTH) {
                    for (int row = 0; row < 7; row++) {
                        if (row + 3 < DISPLAY_HEIGHT) {  // Center vertically
                            bool pixel = (font_data[col] >> row) & 1;
                            if (pixel) {
                                display_buffer[row + 3][char_x + col] = 1;
                            }
                        }
                    }
                }
            }
        }
        
        // Update only changed pixels
        flip_dot_update_display(display, display_buffer);
        if (demo_frame_delay(delay_ms, should_abort)) {
            return;
        }
    }
}

void flip_dot_demo_game_of_life(flip_dot_t *display, uint32_t delay_ms, uint32_t generations, flip_dot_demo_abort_cb_t should_abort) {
    ESP_LOGI(TAG, "Starting Conway's Game of Life demo");
    
    uint8_t current_gen[DISPLAY_HEIGHT][DISPLAY_WIDTH];
    uint8_t next_gen[DISPLAY_HEIGHT][DISPLAY_WIDTH];
    
    // Initialize with random pattern
    for (uint8_t row = 0; row < DISPLAY_HEIGHT; row++) {
        for (uint8_t col = 0; col < DISPLAY_WIDTH; col++) {
            current_gen[row][col] = (rand() % 3 == 0) ? 1 : 0;  // ~33% chance of being alive
        }
    }
    
    for (uint32_t gen = 0; gen < generations; gen++) {
        if (demo_should_stop(should_abort)) {
            return;
        }

        // Calculate next generation
        for (uint8_t row = 0; row < DISPLAY_HEIGHT; row++) {
            for (uint8_t col = 0; col < DISPLAY_WIDTH; col++) {
                // Count living neighbors
                uint8_t neighbors = 0;
                for (int dr = -1; dr <= 1; dr++) {
                    for (int dc = -1; dc <= 1; dc++) {
                        if (dr == 0 && dc == 0) continue;  // Skip center cell
                        
                        int nr = row + dr;
                        int nc = col + dc;
                        
                        // Wrap around edges (toroidal topology)
                        if (nr < 0) nr = DISPLAY_HEIGHT - 1;
                        if (nr >= DISPLAY_HEIGHT) nr = 0;
                        if (nc < 0) nc = DISPLAY_WIDTH - 1;
                        if (nc >= DISPLAY_WIDTH) nc = 0;
                        
                        if (current_gen[nr][nc]) neighbors++;
                    }
                }
                
                // Apply Conway's rules
                if (current_gen[row][col]) {
                    // Cell is alive
                    next_gen[row][col] = (neighbors == 2 || neighbors == 3) ? 1 : 0;
                } else {
                    // Cell is dead
                    next_gen[row][col] = (neighbors == 3) ? 1 : 0;
                }
            }
        }
        
        // Update display with new generation (only changed pixels)
        flip_dot_update_display(display, next_gen);
        
        // Copy next generation to current
        memcpy(current_gen, next_gen, sizeof(current_gen));

        if (demo_frame_delay(delay_ms, should_abort)) {
            return;
        }
    }
}
