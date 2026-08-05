/**
 * @file input.c
 * @brief Input system implementation
 *
 * @author Mikael Bengtsson
 * @date 2025-06-11
 */

#include "input.h"
#include <string.h>
#include <stdio.h>
#include "esp_log.h"
#include "driver/uart.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

/******************************************************************************
 * Private Definitions and Types
 ******************************************************************************/

static const char *TAG = "input_system";

// Buffer sizes
#define UART_RX_BUF_SIZE 1024
#define UART_TX_BUF_SIZE 1024

/******************************************************************************
 * Private Function Declarations
 ******************************************************************************/

static void process_serial_char(input_system_t *input_sys, char c);
static void process_arrow_sequence(input_system_t *input_sys, const char *seq);
static uint32_t get_timestamp_ms(void);

/******************************************************************************
 * Private Function Implementations
 ******************************************************************************/

static void process_serial_char(input_system_t *input_sys, char c) {
    input_command_t command = input_char_to_command(c);
    
    if (command != INPUT_CMD_NONE) {
        send_input_event(input_sys, command, INPUT_TYPE_SERIAL);
        
        // Echo the command if echo is enabled
        if (input_sys->config.serial_config.echo_enabled) {
            const char* cmd_str = input_command_to_string(command);
            printf("Input: %s\n", cmd_str);
        }
    }
}

static void process_arrow_sequence(input_system_t *input_sys, const char *seq) {
    input_command_t command = INPUT_CMD_NONE;
    
    if (strcmp(seq, INPUT_ARROW_UP_SEQ) == 0) {
        command = INPUT_CMD_UP;
    } else if (strcmp(seq, INPUT_ARROW_DOWN_SEQ) == 0) {
        command = INPUT_CMD_DOWN;
    } else if (strcmp(seq, INPUT_ARROW_LEFT_SEQ) == 0) {
        command = INPUT_CMD_LEFT;
    } else if (strcmp(seq, INPUT_ARROW_RIGHT_SEQ) == 0) {
        command = INPUT_CMD_RIGHT;
    }
    
    if (command != INPUT_CMD_NONE) {
        send_input_event(input_sys, command, INPUT_TYPE_SERIAL);
        
        if (input_sys->config.serial_config.echo_enabled) {
            const char* cmd_str = input_command_to_string(command);
            printf("Arrow Input: %s\n", cmd_str);
        }
    }
}

void send_input_event(input_system_t *input_sys, input_command_t command, input_type_t type) {
    if (input_sys->config.callback) {
        input_event_t event = {
            .type = type,
            .command = command,
            .timestamp = get_timestamp_ms(),
            .is_pressed = true,
            .value = 0
        };
        
        input_sys->config.callback(&event);
    }
}

static uint32_t get_timestamp_ms(void) {
    return (uint32_t)(xTaskGetTickCount() * portTICK_PERIOD_MS);
}

/******************************************************************************
 * Public Function Implementations
 ******************************************************************************/

esp_err_t input_system_init(input_system_t *input_sys, input_system_config_t *config) {
    ESP_LOGI(TAG, "Initializing input system");
    
    if (!input_sys || !config) {
        ESP_LOGE(TAG, "Invalid parameters");
        return ESP_ERR_INVALID_ARG;
    }
    
    // Copy configuration
    memcpy(&input_sys->config, config, sizeof(input_system_config_t));
    
    // Initialize state
    input_sys->initialized = false;
    input_sys->serial_enabled = false;
    input_sys->espnow_enabled = false;
    input_sys->rx_buffer_pos = 0;
    memset(input_sys->rx_buffer, 0, sizeof(input_sys->rx_buffer));
    
    esp_err_t ret = ESP_OK;
    
    // Initialize the requested input type
    switch (config->enabled_types) {
        case INPUT_TYPE_SERIAL:
            ret = input_serial_init(input_sys);
            if (ret == ESP_OK) {
                input_sys->serial_enabled = true;
            }
            break;
            
        case INPUT_TYPE_ESPNOW:
            ret = input_espnow_init(input_sys);
            if (ret == ESP_OK) {
                input_sys->espnow_enabled = true;
            }
            break;
            
        default:
            ESP_LOGE(TAG, "Unsupported input type: %d", config->enabled_types);
            return ESP_ERR_INVALID_ARG;
    }
    
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to initialize input type %d: %s", 
                 config->enabled_types, esp_err_to_name(ret));
        return ret;
    }
    
    input_sys->initialized = true;
    ESP_LOGI(TAG, "Input system initialized successfully with type %d", config->enabled_types);
    
    return ESP_OK;
}

esp_err_t input_system_start(input_system_t *input_sys) {
    if (!input_sys || !input_sys->initialized) {
        ESP_LOGE(TAG, "Input system not initialized");
        return ESP_ERR_INVALID_STATE;
    }
    
    ESP_LOGI(TAG, "Starting input system");
    
    if (input_sys->serial_enabled) {
        input_serial_send_prompt(input_sys, "Snake Game Controls: W/A/S/D or Arrow Keys, P=Pause, R=Reset\n");
        input_print_help(input_sys);
    }
    
    return ESP_OK;
}

esp_err_t input_system_stop(input_system_t *input_sys) {
    if (!input_sys || !input_sys->initialized) {
        return ESP_ERR_INVALID_STATE;
    }
    
    ESP_LOGI(TAG, "Stopping input system");
    return ESP_OK;
}

void input_system_deinit(input_system_t *input_sys) {
    if (!input_sys) {
        return;
    }
    
    ESP_LOGI(TAG, "Deinitializing input system");
    
    if (input_sys->serial_enabled) {
        uart_driver_delete(input_sys->config.serial_config.uart_num);
    }
    
    input_sys->initialized = false;
    input_sys->serial_enabled = false;
}

void input_system_process(input_system_t *input_sys) {
    if (!input_sys || !input_sys->initialized) {
        return;
    }
    
    switch (input_sys->config.enabled_types) {
        case INPUT_TYPE_SERIAL:
            if (input_sys->serial_enabled) {
                input_serial_process(input_sys);
            }
            break;
            
        case INPUT_TYPE_ESPNOW:
            if (input_sys->espnow_enabled) {
                input_espnow_process(input_sys);
            }
            break;
            
        default:
            break;
    }
}

bool input_system_has_pending_input(input_system_t *input_sys) {
    if (!input_sys || !input_sys->initialized) {
        return false;
    }
    
    if (input_sys->serial_enabled) {
        size_t available = 0;
        uart_get_buffered_data_len(input_sys->config.serial_config.uart_num, &available);
        return available > 0;
    }
    
    return false;
}

esp_err_t input_serial_init(input_system_t *input_sys) {
    ESP_LOGI(TAG, "Initializing serial input");
    
    uart_config_t uart_config = {
        .baud_rate = input_sys->config.serial_config.baudrate,
        .data_bits = UART_DATA_8_BITS,
        .parity = UART_PARITY_DISABLE,
        .stop_bits = UART_STOP_BITS_1,
        .flow_ctrl = UART_HW_FLOWCTRL_DISABLE,
        .source_clk = UART_SCLK_DEFAULT,
    };
    
    esp_err_t ret = uart_driver_install(input_sys->config.serial_config.uart_num, 
                                        UART_RX_BUF_SIZE, UART_TX_BUF_SIZE, 0, NULL, 0);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to install UART driver: %s", esp_err_to_name(ret));
        return ret;
    }
    
    ret = uart_param_config(input_sys->config.serial_config.uart_num, &uart_config);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to configure UART: %s", esp_err_to_name(ret));
        uart_driver_delete(input_sys->config.serial_config.uart_num);
        return ret;
    }
    
    ret = uart_set_pin(input_sys->config.serial_config.uart_num,
                       input_sys->config.serial_config.tx_pin,
                       input_sys->config.serial_config.rx_pin,
                       UART_PIN_NO_CHANGE,
                       UART_PIN_NO_CHANGE);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to set UART pins: %s", esp_err_to_name(ret));
        uart_driver_delete(input_sys->config.serial_config.uart_num);
        return ret;
    }
    
    ESP_LOGI(TAG, "Serial input initialized on UART%d at %ld baud", 
             input_sys->config.serial_config.uart_num,
             input_sys->config.serial_config.baudrate);
    
    return ESP_OK;
}

void input_serial_process(input_system_t *input_sys) {
    uint8_t data[128];
    int length = uart_read_bytes(input_sys->config.serial_config.uart_num, 
                                data, sizeof(data), 0);
    
    if (length > 0) {
        for (int i = 0; i < length; i++) {
            char c = (char)data[i];
            
            // Handle escape sequences for arrow keys
            if (c == '\x1b') {  // ESC character
                if (i + 2 < length && data[i+1] == '[') {
                    char arrow_seq[4] = {'\x1b', '[', data[i+2], '\0'};
                    process_arrow_sequence(input_sys, arrow_seq);
                    i += 2;  // Skip the next two characters
                    continue;
                }
            }
            
            // Handle regular characters
            if (c >= 32 && c <= 126) {  // Printable ASCII
                process_serial_char(input_sys, c);
            }
        }
    }
}

void input_serial_send_prompt(input_system_t *input_sys, const char* prompt) {
    if (input_sys->serial_enabled && prompt) {
        uart_write_bytes(input_sys->config.serial_config.uart_num, prompt, strlen(prompt));
    }
}

const char* input_command_to_string(input_command_t command) {
    switch (command) {
        case INPUT_CMD_NONE: return "NONE";
        case INPUT_CMD_UP: return "UP";
        case INPUT_CMD_DOWN: return "DOWN";
        case INPUT_CMD_LEFT: return "LEFT";
        case INPUT_CMD_RIGHT: return "RIGHT";
        case INPUT_CMD_SELECT: return "SELECT";
        case INPUT_CMD_BACK: return "BACK";
        case INPUT_CMD_START: return "START";
        case INPUT_CMD_PAUSE: return "PAUSE";
        case INPUT_CMD_RESET: return "RESET";
        default: return "UNKNOWN";
    }
}

input_command_t input_char_to_command(char c) {
    switch (c) {
        case INPUT_KEY_UP_CHAR: return INPUT_CMD_UP;
        case INPUT_KEY_DOWN_CHAR: return INPUT_CMD_DOWN;
        case INPUT_KEY_LEFT_CHAR: return INPUT_CMD_LEFT;
        case INPUT_KEY_RIGHT_CHAR: return INPUT_CMD_RIGHT;
        case INPUT_KEY_SELECT_CHAR: return INPUT_CMD_SELECT;
        case INPUT_KEY_START_CHAR: return INPUT_CMD_PAUSE;  // P for pause/start
        case INPUT_KEY_RESET_CHAR: return INPUT_CMD_RESET;
        case INPUT_KEY_BACK_CHAR: return INPUT_CMD_BACK;
        default: return INPUT_CMD_NONE;
    }
}

void input_print_help(input_system_t *input_sys) {
    if (!input_sys->serial_enabled) {
        return;
    }
    
    const char* help_text = 
        "=== Snake Game Controls ===\n"
        "W / Up Arrow    - Move Up\n"
        "S / Down Arrow  - Move Down\n"
        "A / Left Arrow  - Move Left\n"
        "D / Right Arrow - Move Right\n"
        "P               - Pause/Resume\n"
        "R               - Reset Game\n"
        "B               - Back/Exit\n"
        "==========================\n\n";
    
    input_serial_send_prompt(input_sys, help_text);
}

input_system_config_t input_get_default_config(void) {
    input_system_config_t config = {
        .enabled_types = INPUT_TYPE_SERIAL,
        .serial_config = input_get_default_serial_config(),
        .callback = NULL
    };
    return config;
}

serial_input_config_t input_get_default_serial_config(void) {
    serial_input_config_t config = {
        .baudrate = INPUT_DEFAULT_BAUDRATE,
        .uart_num = INPUT_DEFAULT_UART_NUM,
        .tx_pin = INPUT_DEFAULT_TX_PIN,
        .rx_pin = INPUT_DEFAULT_RX_PIN,
        .echo_enabled = true
    };
    return config;
} 