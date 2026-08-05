/**
 * @file snake.c
 * @brief Snake game application implementation
 *
 * @author Mikael Bengtsson
 * @date 2025-06-11
 */

#include "snake.h"
#include "input.h"
#include <string.h>
#include <stdlib.h>
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

/******************************************************************************
 * Private Definitions and Types
 ******************************************************************************/

static const char *TAG = "snake_game";

/******************************************************************************
 * Private Function Declarations
 ******************************************************************************/

// Internal helper functions
static void init_snake(snake_t *snake);
static void move_snake(snake_t *snake);
static bool is_valid_position(position_t pos);
static bool is_position_in_snake(snake_t *snake, position_t pos);
static void clear_game_buffer(uint8_t buffer[DISPLAY_HEIGHT][DISPLAY_WIDTH]);
static uint32_t get_random_number(uint32_t max);

/******************************************************************************
 * Private Function Implementations
 ******************************************************************************/

static void init_snake(snake_t *snake) {
    snake->length = SNAKE_INITIAL_LENGTH;
    snake->direction = DIR_RIGHT;
    
    // Initialize input buffer
    direction_buffer_init(&snake->input_buffer);
    
    // Initialize snake in the center of the play area
    uint8_t start_x = (GAME_MIN_X + GAME_MAX_X) / 2;
    uint8_t start_y = (GAME_MIN_Y + GAME_MAX_Y) / 2;
    
    for (uint8_t i = 0; i < snake->length; i++) {
        snake->segments[i].x = start_x - i;
        snake->segments[i].y = start_y;
    }
}

static void move_snake(snake_t *snake) {
    // Process next direction from buffer if available
    snake_direction_t next_direction;
    if (direction_buffer_pop(&snake->input_buffer, &next_direction)) {
        // Validate direction change (prevent reversing into itself)
        bool valid_change = true;
        switch (snake->direction) {
            case DIR_UP:    valid_change = (next_direction != DIR_DOWN); break;
            case DIR_DOWN:  valid_change = (next_direction != DIR_UP); break;
            case DIR_LEFT:  valid_change = (next_direction != DIR_RIGHT); break;
            case DIR_RIGHT: valid_change = (next_direction != DIR_LEFT); break;
        }
        
        if (valid_change) {
            snake->direction = next_direction;
        } else {
            ESP_LOGD(TAG, "Invalid direction change blocked: %d -> %d", snake->direction, next_direction);
        }
    }
    
    // Calculate new head position
    position_t new_head = snake->segments[0];
    
    switch (snake->direction) {
        case DIR_UP:
            new_head.y--;
            break;
        case DIR_DOWN:
            new_head.y++;
            break;
        case DIR_LEFT:
            new_head.x--;
            break;
        case DIR_RIGHT:
            new_head.x++;
            break;
    }
    
    // Debug: check if new head would be at (0,0)
    if (new_head.x == 0 && new_head.y == 0) {
        ESP_LOGW(TAG, "WARNING: New snake head would be at (0,0)!");
    }
    
    // Move body segments
    for (int8_t i = snake->length - 1; i > 0; i--) {
        snake->segments[i] = snake->segments[i - 1];
    }
    
    // Set new head position
    snake->segments[0] = new_head;
}

static bool is_valid_position(position_t pos) {
    return (pos.x <= GAME_MAX_X && pos.y <= GAME_MAX_Y);
}

static bool is_position_in_snake(snake_t *snake, position_t pos) {
    for (uint8_t i = 0; i < snake->length; i++) {
        if (snake->segments[i].x == pos.x && snake->segments[i].y == pos.y) {
            return true;
        }
    }
    return false;
}

static void clear_game_buffer(uint8_t buffer[DISPLAY_HEIGHT][DISPLAY_WIDTH]) {
    memset(buffer, 0, DISPLAY_HEIGHT * DISPLAY_WIDTH);
}

static uint32_t get_random_number(uint32_t max) {
    return rand() % max;
}

/******************************************************************************
 * Input Buffer Functions
 ******************************************************************************/

void direction_buffer_init(direction_buffer_t *buffer) {
    buffer->head = 0;
    buffer->tail = 0;
    buffer->count = 0;
}

bool direction_buffer_push(direction_buffer_t *buffer, snake_direction_t direction) {
    if (buffer->count >= DIRECTION_BUFFER_SIZE) {
        ESP_LOGW(TAG, "Direction buffer full, dropping input");
        return false; // Buffer full
    }
    
    buffer->buffer[buffer->head] = direction;
    buffer->head = (buffer->head + 1) % DIRECTION_BUFFER_SIZE;
    buffer->count++;
    
    ESP_LOGD(TAG, "Direction %d buffered (count: %d)", direction, buffer->count);
    return true;
}

bool direction_buffer_pop(direction_buffer_t *buffer, snake_direction_t *direction) {
    if (buffer->count == 0) {
        return false; // Buffer empty
    }
    
    *direction = buffer->buffer[buffer->tail];
    buffer->tail = (buffer->tail + 1) % DIRECTION_BUFFER_SIZE;
    buffer->count--;
    
    ESP_LOGD(TAG, "Direction %d popped from buffer (count: %d)", *direction, buffer->count);
    return true;
}

bool direction_buffer_is_empty(direction_buffer_t *buffer) {
    return buffer->count == 0;
}

/******************************************************************************
 * Public Function Implementations
 ******************************************************************************/

void snake_game_init(snake_game_t *game, flip_dot_t *display) {
    ESP_LOGI(TAG, "Initializing Snake game");
    
    game->display = display;
    game->state = GAME_INIT;
    game->score = 0;
    game->level = 1;
    game->game_speed_ms = SNAKE_INITIAL_SPEED_MS;
    
    // Initialize game buffer
    clear_game_buffer(game->game_buffer);
    
    // Initialize snake
    init_snake(&game->snake);
    
    // Initialize food
    for (uint8_t i = 0; i < FOOD_COUNT; i++) {
        game->food[i].active = false;
        game->food[i].position.x = 255;  // Invalid position
        game->food[i].position.y = 255;  // Invalid position
    }
    
    // Generate initial food
    snake_game_generate_food(game);
    
    ESP_LOGI(TAG, "Snake game initialized");
}

void snake_game_reset(snake_game_t *game) {
    ESP_LOGI(TAG, "Resetting Snake game");
    
    game->score = 0;
    game->level = 1;
    game->game_speed_ms = SNAKE_INITIAL_SPEED_MS;
    game->state = GAME_INIT;
    
    // Reset snake
    init_snake(&game->snake);
    
    // Reset food
    for (uint8_t i = 0; i < FOOD_COUNT; i++) {
        game->food[i].active = false;
        game->food[i].position.x = 255;  // Invalid position
        game->food[i].position.y = 255;  // Invalid position
    }
    snake_game_generate_food(game);
    
    // Clear display
    clear_game_buffer(game->game_buffer);
    snake_game_render(game);
}

void snake_game_start(snake_game_t *game) {
    ESP_LOGI(TAG, "Starting Snake game from state: %d", game->state);
    game->state = GAME_RUNNING;
    ESP_LOGI(TAG, "Game state changed to: %d", game->state);
    
    // Clear the display and render initial state
    clear_game_buffer(game->game_buffer);
    snake_game_render(game);
}

void snake_game_pause(snake_game_t *game) {
    if (game->state == GAME_RUNNING) {
        ESP_LOGI(TAG, "Pausing Snake game");
        game->state = GAME_PAUSED;
    }
}

void snake_game_resume(snake_game_t *game) {
    if (game->state == GAME_PAUSED) {
        ESP_LOGI(TAG, "Resuming Snake game");
        game->state = GAME_RUNNING;
    }
}

void snake_game_update(snake_game_t *game) {
    if (game->state != GAME_RUNNING) {
        return;
    }
    
    // Store the tail position BEFORE moving, in case we need to grow
    position_t tail_pos_before_move = game->snake.segments[game->snake.length - 1];
    
    // Move snake
    move_snake(&game->snake);
    
    // Check for collisions
    if (snake_game_check_collision(game)) {
        ESP_LOGI(TAG, "Game Over! Final score: %ld", game->score);
        game->state = GAME_OVER;
        return;
    }
    
    // Check for food collision
    if (snake_game_check_food_collision(game)) {
        ESP_LOGI(TAG, "Food eaten! Score: %ld", game->score);
        
        // Increase snake length and properly initialize new tail
        if (game->snake.length < SNAKE_MAX_LENGTH) {
            uint8_t old_length = game->snake.length;
            
            game->snake.length++;
            
            // Set the new tail segment to where the tail was BEFORE the move
            // This ensures immediate visual growth
            game->snake.segments[game->snake.length - 1] = tail_pos_before_move;
            
            ESP_LOGI(TAG, "Snake length increased from %d to %d", old_length, game->snake.length);
            ESP_LOGI(TAG, "New tail segment at (%d, %d) (tail pos before move)", tail_pos_before_move.x, tail_pos_before_move.y);
        }
        
        // Increase score
        game->score += 10;
        
        // Check for level up (every 50 points)
        if (game->score % 50 == 0) {
            game->level++;
            if (game->game_speed_ms > SNAKE_MIN_SPEED_MS) {
                game->game_speed_ms -= SNAKE_SPEED_DECREASE_PER_LEVEL;
                if (game->game_speed_ms < SNAKE_MIN_SPEED_MS) {
                    game->game_speed_ms = SNAKE_MIN_SPEED_MS;
                }
            }
            ESP_LOGI(TAG, "Level up! Level: %ld, Speed: %ld ms", game->level, game->game_speed_ms);
        }
        
        // Generate new food - with debug logging
        ESP_LOGI(TAG, "Generating new food after eating...");
        snake_game_generate_food(game);
        ESP_LOGI(TAG, "New food generation completed");
    }
    
    // Render the game
    snake_game_render(game);
}

void snake_game_change_direction(snake_game_t *game, snake_direction_t new_direction) {
    // Add direction to buffer - validation will happen in move_snake()
    if (!direction_buffer_push(&game->snake.input_buffer, new_direction)) {
        ESP_LOGW(TAG, "Failed to buffer direction change: %d", new_direction);
    }
}

bool snake_game_is_running(snake_game_t *game) {
    return game->state == GAME_RUNNING;
}

bool snake_game_is_over(snake_game_t *game) {
    return game->state == GAME_OVER;
}

void snake_game_render(snake_game_t *game) {
    // Clear the game buffer
    clear_game_buffer(game->game_buffer);
    
    // Debug: check if (0,0) gets set during snake drawing
    bool pixel_0_0_set_by_snake = false;
    
    // Draw snake
    for (uint8_t i = 0; i < game->snake.length; i++) {
        position_t pos = game->snake.segments[i];
        if (pos.x < DISPLAY_WIDTH && pos.y < DISPLAY_HEIGHT) {
            if (pos.x == 0 && pos.y == 0) {
                ESP_LOGW(TAG, "WARNING: Snake segment %d is at (0,0)!", i);
                pixel_0_0_set_by_snake = true;
            }
            game->game_buffer[pos.y][pos.x] = 1;
        }
    }
    
    // Draw food
    for (uint8_t i = 0; i < FOOD_COUNT; i++) {
        if (game->food[i].active) {
            position_t pos = game->food[i].position;
            
            // Debug: log if we're about to draw at (0,0)
            if (pos.x == 0 && pos.y == 0) {
                ESP_LOGW(TAG, "WARNING: About to draw food at (0,0)! Deactivating food.");
                game->food[i].active = false;
                continue;
            }
            
            // Extra bounds checking to prevent invalid positions
            if (pos.x <= GAME_MAX_X && pos.y <= GAME_MAX_Y && 
                pos.x < DISPLAY_WIDTH && pos.y < DISPLAY_HEIGHT) {
                game->game_buffer[pos.y][pos.x] = 1;
            } else {
                ESP_LOGW(TAG, "Invalid food position (%d,%d), deactivating", pos.x, pos.y);
                game->food[i].active = false;
            }
        }
    }
    
    // Debug: check if (0,0) is set in the final buffer
    if (game->game_buffer[0][0] == 1) {
        ESP_LOGW(TAG, "WARNING: Pixel (0,0) is SET in game buffer! Snake: %s", 
                 pixel_0_0_set_by_snake ? "YES" : "NO");
    }
    
    // Update the display
    flip_dot_update_display(game->display, game->game_buffer);
}

void snake_game_show_game_over(snake_game_t *game) {
    ESP_LOGI(TAG, "Showing game over screen");
    
    // Clear display
    clear_game_buffer(game->game_buffer);
    
    // Simple game over pattern - create a cross pattern
    for (uint8_t i = 2; i < DISPLAY_WIDTH - 2; i++) {
        game->game_buffer[DISPLAY_HEIGHT / 2][i] = 1;  // Horizontal line
    }
    for (uint8_t i = 2; i < DISPLAY_HEIGHT - 2; i++) {
        game->game_buffer[i][DISPLAY_WIDTH / 2] = 1;   // Vertical line
    }
    
    // Update display
    flip_dot_update_display(game->display, game->game_buffer);
    
    // Hold for a few seconds
    vTaskDelay(3000 / portTICK_PERIOD_MS);
}

void snake_game_show_score(snake_game_t *game) {
    ESP_LOGI(TAG, "Final Score: %ld, Level: %ld", game->score, game->level);
    // Score display could be implemented with a simple pattern or number display
    // For now, just log the score
}

void snake_game_generate_food(snake_game_t *game) {
    // Find inactive food slot
    uint8_t food_index = 0;
    for (uint8_t i = 0; i < FOOD_COUNT; i++) {
        if (!game->food[i].active) {
            food_index = i;
            break;
        }
    }
    
    // Clear the position first to ensure no invalid data
    game->food[food_index].position.x = 255;
    game->food[food_index].position.y = 255;
    game->food[food_index].active = false;
    
    // Generate random position that's not in the snake
    position_t new_pos;
    uint8_t attempts = 0;
    const uint8_t max_attempts = 200;  // Increased attempts
    bool valid_pos_found = false;
    
    // Exclude (0,0) from valid positions to prevent the top-left pixel issue
    do {
        new_pos.x = GAME_MIN_X + get_random_number(GAME_MAX_X - GAME_MIN_X + 1);
        new_pos.y = GAME_MIN_Y + get_random_number(GAME_MAX_Y - GAME_MIN_Y + 1);
        attempts++;
        
        // Additional check: avoid (0,0) position temporarily for debugging
        if (new_pos.x == 0 && new_pos.y == 0) {
            continue;  // Skip (0,0) position
        }
        
        if (!is_position_in_snake(&game->snake, new_pos)) {
            valid_pos_found = true;
        }
    } while (!valid_pos_found && attempts < max_attempts);
    
    if (valid_pos_found) {
        game->food[food_index].position = new_pos;
        game->food[food_index].active = true;
        ESP_LOGI(TAG, "Food generated at (%d, %d)", new_pos.x, new_pos.y);
    } else {
        // Fallback: place food at first available position (excluding 0,0)
        ESP_LOGW(TAG, "Could not generate random food after %d attempts, using fallback", max_attempts);
        bool found = false;
        for (uint8_t y = GAME_MIN_Y; y <= GAME_MAX_Y && !found; y++) {
            for (uint8_t x = GAME_MIN_X; x <= GAME_MAX_X && !found; x++) {
                // Skip (0,0) position in fallback as well
                if (x == 0 && y == 0) {
                    continue;
                }
                
                position_t test_pos = {x, y};
                if (!is_position_in_snake(&game->snake, test_pos)) {
                    game->food[food_index].position = test_pos;
                    game->food[food_index].active = true;
                    found = true;
                    ESP_LOGI(TAG, "Fallback food generated at (%d, %d)", x, y);
                }
            }
        }
        
        if (!found) {
            ESP_LOGE(TAG, "Could not place food anywhere! Game area full?");
            game->food[food_index].active = false;
        }
    }
}

bool snake_game_check_collision(snake_game_t *game) {
    position_t head = game->snake.segments[0];
    
    // Check wall collision
    if (!is_valid_position(head)) {
        ESP_LOGD(TAG, "Wall collision at (%d, %d)", head.x, head.y);
        return true;
    }
    
    // Check self collision (skip head segment)
    for (uint8_t i = 1; i < game->snake.length; i++) {
        if (game->snake.segments[i].x == head.x && game->snake.segments[i].y == head.y) {
            ESP_LOGD(TAG, "Self collision at (%d, %d)", head.x, head.y);
            return true;
        }
    }
    
    return false;
}

bool snake_game_check_food_collision(snake_game_t *game) {
    position_t head = game->snake.segments[0];
    
    for (uint8_t i = 0; i < FOOD_COUNT; i++) {
        if (game->food[i].active &&
            game->food[i].position.x == head.x &&
            game->food[i].position.y == head.y) {
            
            // Deactivate the eaten food
            game->food[i].active = false;
            ESP_LOGD(TAG, "Food collision at (%d, %d)", head.x, head.y);
            return true;
        }
    }
    
    return false;
}

void snake_game_demo(flip_dot_t *display, uint32_t duration_ms) {
    ESP_LOGI(TAG, "Starting Snake game demo");
    
    snake_game_t game;
    snake_game_init(&game, display);
    snake_game_start(&game);
    
    uint32_t start_time = xTaskGetTickCount() * portTICK_PERIOD_MS;
    uint32_t last_update = start_time;
    uint32_t direction_change_time = start_time;
    
    // Simple AI: change direction every few seconds
    snake_direction_t directions[] = {DIR_RIGHT, DIR_DOWN, DIR_LEFT, DIR_UP};
    uint8_t current_dir_index = 0;
    
    while ((xTaskGetTickCount() * portTICK_PERIOD_MS - start_time) < duration_ms) {
        uint32_t current_time = xTaskGetTickCount() * portTICK_PERIOD_MS;
        
        // Change direction every 3 seconds for demo
        if (current_time - direction_change_time > 3000) {
            current_dir_index = (current_dir_index + 1) % 4;
            snake_game_change_direction(&game, directions[current_dir_index]);
            direction_change_time = current_time;
        }
        
        // Update game at the appropriate speed
        if (current_time - last_update >= game.game_speed_ms) {
            snake_game_update(&game);
            last_update = current_time;
            
            // Reset game if it's over
            if (snake_game_is_over(&game)) {
                snake_game_show_game_over(&game);
                snake_game_reset(&game);
                snake_game_start(&game);
            }
        }
        
        vTaskDelay(10 / portTICK_PERIOD_MS);  // Small delay to prevent busy waiting
    }
    
    ESP_LOGI(TAG, "Snake game demo completed");
}

/******************************************************************************
 * Interactive Game Functions
 ******************************************************************************/

// Global variables for game state
static snake_game_t *g_current_game = NULL;
static input_system_t g_input_system;

// Input callback function
static void snake_input_callback(input_event_t *event) {
    if (!g_current_game || !event) {
        return;
    }
    
    ESP_LOGI(TAG, "Input received: type=%d, cmd=%d, pressed=%d, game_state=%d", 
             event->type, event->command, event->is_pressed, g_current_game->state);
    
    // Handle game start from any button press
    if (g_current_game->state == GAME_INIT) {
        if (event->type == INPUT_TYPE_ESPNOW && event->is_pressed) {
            ESP_LOGI(TAG, "Starting game from button press");
            snake_game_start(g_current_game);
            return;
        }
    }
    
    // Only process direction changes if game is running
    if (g_current_game->state != GAME_RUNNING) {
        ESP_LOGD(TAG, "Ignoring input - game not running (state=%d)", g_current_game->state);
        return;
    }
    
    // Convert input command to snake direction
    snake_direction_t new_direction = g_current_game->snake.direction;  // Keep current direction by default
    
    switch (event->command) {
        case INPUT_CMD_UP:
            new_direction = DIR_UP;
            break;
        case INPUT_CMD_DOWN:
            new_direction = DIR_DOWN;
            break;
        case INPUT_CMD_LEFT:
            new_direction = DIR_LEFT;
            break;
        case INPUT_CMD_RIGHT:
            new_direction = DIR_RIGHT;
            break;
        case INPUT_CMD_PAUSE:
            if (g_current_game->state == GAME_RUNNING) {
                snake_game_pause(g_current_game);
            } else if (g_current_game->state == GAME_PAUSED) {
                snake_game_resume(g_current_game);
            }
            return;
        case INPUT_CMD_RESET:
            snake_game_reset(g_current_game);
            return;
        default:
            return;  // Ignore other commands
    }
    
    // Buffer the direction change
    if (direction_buffer_push(&g_current_game->snake.input_buffer, new_direction)) {
        ESP_LOGD(TAG, "Direction change buffered: %d", new_direction);
    }
}

void snake_game_run_interactive(flip_dot_t *display) {
    snake_game_t game;
    snake_game_init(&game, display);
    g_current_game = &game;
    
    ESP_LOGI(TAG, "Game initialized with state: %d", game.state);
    
    // Initialize input system with ESP-NOW enabled
    input_system_config_t input_config = input_get_default_config();
    input_config.enabled_types = INPUT_TYPE_ESPNOW;  // Only enable ESP-NOW
    input_config.callback = snake_input_callback;
    input_config.espnow_config = input_get_default_espnow_config();
    
    esp_err_t ret = input_system_init(&g_input_system, &input_config);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to initialize input system: %s", esp_err_to_name(ret));
        return;
    }
    
    ret = input_system_start(&g_input_system);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to start input system: %s", esp_err_to_name(ret));
        input_system_deinit(&g_input_system);
        return;
    }
    
    // Show start screen
    clear_game_buffer(game.game_buffer);
    // Draw "PRESS ANY BUTTON" text
    for (int y = 0; y < DISPLAY_HEIGHT; y++) {
        for (int x = 0; x < DISPLAY_WIDTH; x++) {
            if (y >= 2 && y <= 4) {  // Middle rows
                game.game_buffer[y][x] = 1;  // Draw a line
            }
        }
    }
    snake_game_render(&game);
    ESP_LOGI(TAG, "Start screen displayed, waiting for button press");
    
    // Main game loop
    uint32_t last_update = xTaskGetTickCount() * portTICK_PERIOD_MS;
    
    while (1) {
        uint32_t current_time = xTaskGetTickCount() * portTICK_PERIOD_MS;
        
        // Process input
        input_system_process(&g_input_system);
        
        // Update game state at the appropriate speed
        if (game.state == GAME_RUNNING && (current_time - last_update >= game.game_speed_ms)) {
            snake_game_update(&game);
            last_update = current_time;
            
            // Check for game over
            if (snake_game_is_over(&game)) {
                snake_game_show_game_over(&game);
                vTaskDelay(2000 / portTICK_PERIOD_MS);  // Show game over for 2 seconds
                snake_game_reset(&game);
                
                // Show start screen again
                clear_game_buffer(game.game_buffer);
                for (int y = 0; y < DISPLAY_HEIGHT; y++) {
                    for (int x = 0; x < DISPLAY_WIDTH; x++) {
                        if (y >= 2 && y <= 4) {  // Middle rows
                            game.game_buffer[y][x] = 1;  // Draw a line
                        }
                    }
                }
                snake_game_render(&game);
                ESP_LOGI(TAG, "Game reset, showing start screen");
            }
        }
        
        vTaskDelay(10 / portTICK_PERIOD_MS);  // Small delay to prevent busy waiting
    }
} 