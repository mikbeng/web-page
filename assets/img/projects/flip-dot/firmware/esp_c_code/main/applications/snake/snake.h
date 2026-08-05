/**
 * @file snake.h
 * @brief Snake game application for flip dot display
 *
 * @author Mikael Bengtsson
 * @date 2025-06-11
 */

#ifndef SNAKE_H
#define SNAKE_H

#include <stdint.h>
#include <stdbool.h>
#include "flip_dot.h"

/******************************************************************************
 * Public Definitions and Types
 ******************************************************************************/

// Game constants
#define SNAKE_MAX_LENGTH 50
#define SNAKE_INITIAL_LENGTH 3
#define FOOD_COUNT 1

// Direction definitions
typedef enum {
    DIR_UP,
    DIR_DOWN,
    DIR_LEFT,
    DIR_RIGHT
} snake_direction_t;

// Game state definitions
typedef enum {
    GAME_INIT,
    GAME_RUNNING,
    GAME_OVER,
    GAME_PAUSED
} game_state_t;

// Position structure
typedef struct {
    uint8_t x;
    uint8_t y;
} position_t;

// Input buffer for direction changes
#define DIRECTION_BUFFER_SIZE 8

typedef struct {
    snake_direction_t buffer[DIRECTION_BUFFER_SIZE];
    uint8_t head;  // Write position
    uint8_t tail;  // Read position
    uint8_t count; // Number of buffered directions
} direction_buffer_t;

// Snake segment structure
typedef struct {
    position_t segments[SNAKE_MAX_LENGTH];
    uint8_t length;
    snake_direction_t direction;
    direction_buffer_t input_buffer;  // Buffer for direction changes
} snake_t;

// Food structure
typedef struct {
    position_t position;
    bool active;
} food_t;

// Game structure
typedef struct {
    snake_t snake;
    food_t food[FOOD_COUNT];
    game_state_t state;
    uint32_t score;
    uint32_t level;
    uint32_t game_speed_ms;
    flip_dot_t *display;
    uint8_t game_buffer[DISPLAY_HEIGHT][DISPLAY_WIDTH];
} snake_game_t;

/******************************************************************************
 * Public Constants
 ******************************************************************************/

// Game timing constants
#define SNAKE_INITIAL_SPEED_MS 350
#define SNAKE_SPEED_DECREASE_PER_LEVEL 50
#define SNAKE_MIN_SPEED_MS 100

// Display boundaries (full screen)
#define GAME_MIN_X 0
#define GAME_MAX_X (DISPLAY_WIDTH - 1)
#define GAME_MIN_Y 0
#define GAME_MAX_Y (DISPLAY_HEIGHT - 1)

/******************************************************************************
 * Public Function Declarations
 ******************************************************************************/

// Game initialization and control
void snake_game_init(snake_game_t *game, flip_dot_t *display);
void snake_game_reset(snake_game_t *game);
void snake_game_start(snake_game_t *game);
void snake_game_pause(snake_game_t *game);
void snake_game_resume(snake_game_t *game);

// Game logic
void snake_game_update(snake_game_t *game);
void snake_game_change_direction(snake_game_t *game, snake_direction_t new_direction);
bool snake_game_is_running(snake_game_t *game);
bool snake_game_is_over(snake_game_t *game);

// Display functions
void snake_game_render(snake_game_t *game);
void snake_game_show_game_over(snake_game_t *game);
void snake_game_show_score(snake_game_t *game);

// Demo function
void snake_game_demo(flip_dot_t *display, uint32_t duration_ms);

// Interactive game function
void snake_game_run_interactive(flip_dot_t *display);

// Utility functions
void snake_game_generate_food(snake_game_t *game);
bool snake_game_check_collision(snake_game_t *game);
bool snake_game_check_food_collision(snake_game_t *game);

// Input buffer functions
void direction_buffer_init(direction_buffer_t *buffer);
bool direction_buffer_push(direction_buffer_t *buffer, snake_direction_t direction);
bool direction_buffer_pop(direction_buffer_t *buffer, snake_direction_t *direction);
bool direction_buffer_is_empty(direction_buffer_t *buffer);

#endif /* SNAKE_H */ 