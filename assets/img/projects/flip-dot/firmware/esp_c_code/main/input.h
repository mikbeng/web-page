/**
 * @file input.h
 * @brief Input system for flip dot display applications
 *
 * @author Mikael Bengtsson
 * @date 2025-06-11
 */

#ifndef INPUT_H
#define INPUT_H

#include <stdint.h>
#include <stdbool.h>
#include "esp_err.h"

/******************************************************************************
 * Public Definitions and Types
 ******************************************************************************/

// Input types
typedef enum {
    INPUT_TYPE_NONE,
    INPUT_TYPE_SERIAL,
    INPUT_TYPE_BUTTON,
    INPUT_TYPE_ENCODER,
    INPUT_TYPE_TOUCH,
    INPUT_TYPE_ESPNOW  // Add ESP-NOW input type
} input_type_t;

// Input commands/keys
typedef enum {
    INPUT_CMD_NONE,
    INPUT_CMD_UP,
    INPUT_CMD_DOWN,
    INPUT_CMD_LEFT,
    INPUT_CMD_RIGHT,
    INPUT_CMD_SELECT,
    INPUT_CMD_BACK,
    INPUT_CMD_START,
    INPUT_CMD_PAUSE,
    INPUT_CMD_RESET
} input_command_t;

// Input event structure
typedef struct {
    input_type_t type;
    input_command_t command;
    uint32_t timestamp;
    bool is_pressed;  // For button states (pressed/released)
    int32_t value;    // For analog values (encoder, touch coordinates, etc.)
} input_event_t;

// Input callback function type
typedef void (*input_callback_t)(input_event_t *event);

// Serial input configuration
typedef struct {
    uint32_t baudrate;
    uint8_t uart_num;
    int tx_pin;
    int rx_pin;
    bool echo_enabled;
} serial_input_config_t;

// ESP-NOW input configuration
typedef struct {
    uint8_t channel;           // WiFi channel to use
    bool enable_encryption;    // Enable encryption for ESP-NOW
    uint8_t peer_mac[6];      // MAC address of the peer device
} espnow_input_config_t;

// Input system configuration
typedef struct {
    input_type_t enabled_types;
    serial_input_config_t serial_config;
    espnow_input_config_t espnow_config;  // Add ESP-NOW config
    input_callback_t callback;
} input_system_config_t;

// Input system handle
typedef struct {
    input_system_config_t config;
    bool initialized;
    bool serial_enabled;
    bool espnow_enabled;      // Add ESP-NOW enabled flag
    uint8_t rx_buffer[256];
    size_t rx_buffer_pos;
} input_system_t;

/******************************************************************************
 * Public Constants
 ******************************************************************************/

// Default serial configuration
#define INPUT_DEFAULT_BAUDRATE 115200
#define INPUT_DEFAULT_UART_NUM 0
#define INPUT_DEFAULT_TX_PIN 1
#define INPUT_DEFAULT_RX_PIN 3

// Key mappings for serial input
#define INPUT_KEY_UP_CHAR 'w'
#define INPUT_KEY_DOWN_CHAR 's'
#define INPUT_KEY_LEFT_CHAR 'a'
#define INPUT_KEY_RIGHT_CHAR 'd'
#define INPUT_KEY_SELECT_CHAR ' '  // Space bar
#define INPUT_KEY_START_CHAR 'p'   // P for play/pause
#define INPUT_KEY_RESET_CHAR 'r'   // R for reset
#define INPUT_KEY_BACK_CHAR 'b'    // B for back

// Alternative arrow key sequences (for terminals that support them)
#define INPUT_ARROW_UP_SEQ "\x1b[A"
#define INPUT_ARROW_DOWN_SEQ "\x1b[B"
#define INPUT_ARROW_RIGHT_SEQ "\x1b[C"
#define INPUT_ARROW_LEFT_SEQ "\x1b[D"

/******************************************************************************
 * Public Function Declarations
 ******************************************************************************/

// System initialization and control
esp_err_t input_system_init(input_system_t *input_sys, input_system_config_t *config);
esp_err_t input_system_start(input_system_t *input_sys);
esp_err_t input_system_stop(input_system_t *input_sys);
void input_system_deinit(input_system_t *input_sys);

// Input processing
void input_system_process(input_system_t *input_sys);
bool input_system_has_pending_input(input_system_t *input_sys);

// Event handling
void send_input_event(input_system_t *input_sys, input_command_t command, input_type_t type);

// Serial input specific functions
esp_err_t input_serial_init(input_system_t *input_sys);
void input_serial_process(input_system_t *input_sys);
void input_serial_send_prompt(input_system_t *input_sys, const char* prompt);

// ESP-NOW specific functions
esp_err_t input_espnow_init(input_system_t *input_sys);
void input_espnow_process(input_system_t *input_sys);
espnow_input_config_t input_get_default_espnow_config(void);

// Utility functions
const char* input_command_to_string(input_command_t command);
input_command_t input_char_to_command(char c);
void input_print_help(input_system_t *input_sys);

// Default configurations
input_system_config_t input_get_default_config(void);
serial_input_config_t input_get_default_serial_config(void);

#endif /* INPUT_H */ 