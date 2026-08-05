# Input System Module

## Overview
The input system provides a flexible and extensible framework for handling various input types for flip dot display applications. Currently supports serial input with plans for future expansion to buttons, encoders, and touch inputs.

## Features
- **Serial Input Support**: WASD keys and arrow keys for directional control
- **Callback-based Architecture**: Event-driven input handling
- **Extensible Design**: Easy to add new input types (buttons, encoders, etc.)
- **ESP32 UART Integration**: Built-in support for ESP32 UART drivers
- **Command Mapping**: Configurable key-to-command mappings

## Key Mappings

### WASD Controls
- `W` - Move Up
- `S` - Move Down  
- `A` - Move Left
- `D` - Move Right

### Arrow Keys
- `↑` (Up Arrow) - Move Up
- `↓` (Down Arrow) - Move Down
- `←` (Left Arrow) - Move Left
- `→` (Right Arrow) - Move Right

### Game Controls
- `P` - Pause/Resume/Start
- `R` - Reset Game
- `B` - Back/Exit
- `Space` - Select/Action

## Serial Configuration
- **Default Baudrate**: 115200
- **Default UART**: UART0 (USB Serial)
- **TX Pin**: GPIO 1
- **RX Pin**: GPIO 3
- **Echo**: Enabled by default

## Usage Example

### Basic Setup
```c
#include "input.h"

// Input callback function
void my_input_callback(input_event_t *event) {
    switch (event->command) {
        case INPUT_CMD_UP:
            // Handle up command
            break;
        case INPUT_CMD_DOWN:
            // Handle down command
            break;
        // ... handle other commands
    }
}

// Initialize input system
input_system_t input_sys;
input_system_config_t config = input_get_default_config();
config.callback = my_input_callback;

input_system_init(&input_sys, &config);
input_system_start(&input_sys);

// Main loop
while (running) {
    input_system_process(&input_sys);
    // ... other processing
    vTaskDelay(10 / portTICK_PERIOD_MS);
}

// Cleanup
input_system_stop(&input_sys);
input_system_deinit(&input_sys);
```

### Integration with Snake Game
The input system is automatically integrated with the interactive Snake game:
```c
// Simply call the interactive game function
snake_game_run_interactive(&display);
```

## Terminal Usage
When connected via serial monitor:
1. Open serial monitor at 115200 baud
2. Help text will be displayed automatically
3. Use WASD keys or arrow keys to control
4. Press 'P' to start/pause the game
5. Press 'R' to reset the game
6. Press 'B' to exit back to demo mode

## Future Extensions
The input system is designed to easily support additional input types:

### Planned Input Types
- **Physical Buttons**: GPIO-based button inputs
- **Rotary Encoder**: For navigation and value adjustment
- **Touch Sensors**: Capacitive touch input
- **I2C Devices**: External input controllers

### Adding New Input Types
1. Add new `input_type_t` enum value
2. Extend configuration structures
3. Implement initialization and processing functions
4. Add to main processing loop

## API Reference

### Initialization Functions
- `input_system_init()` - Initialize the input system
- `input_system_start()` - Start input processing
- `input_system_stop()` - Stop input processing
- `input_system_deinit()` - Clean up resources

### Processing Functions
- `input_system_process()` - Process pending inputs (call in main loop)
- `input_system_has_pending_input()` - Check for pending input

### Utility Functions
- `input_command_to_string()` - Convert command to string for debugging
- `input_char_to_command()` - Map character to command
- `input_print_help()` - Display help text via serial

### Configuration Functions
- `input_get_default_config()` - Get default system configuration
- `input_get_default_serial_config()` - Get default serial configuration

## Error Handling
All initialization functions return `esp_err_t` values:
- `ESP_OK` - Success
- `ESP_ERR_INVALID_ARG` - Invalid parameters
- `ESP_ERR_INVALID_STATE` - System not properly initialized
- Other ESP-IDF UART error codes for serial setup issues 