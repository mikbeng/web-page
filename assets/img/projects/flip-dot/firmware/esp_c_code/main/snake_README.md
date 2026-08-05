# Snake Game Application

## Overview
This Snake game application is designed for the flip dot display system. It provides a classic Snake game experience with automatic demo functionality.

## Features
- Classic Snake gameplay with food collection
- Collision detection (walls and self-collision)
- Progressive difficulty with increasing speed
- Score tracking and level progression
- Automatic demo mode with AI-controlled snake
- Game over screen with cross pattern display

## Game Mechanics
- Snake starts with 3 segments in the center of the play area
- Food spawns randomly in the play area (avoiding snake body)
- Robust food generation with fallback positioning to prevent invalid positions
- Snake grows by one segment when food is eaten
- Score increases by 10 points per food item
- Level increases every 50 points
- Game speed increases with each level (minimum speed limit applies)
- Game uses full screen with collision detection at screen edges

## Display Layout
- 28x13 pixel flip dot display
- Full screen usage (no borders)
- Effective play area: 28x13 pixels
- Snake segments and food are represented as single pixels

## API Functions

### Game Control
- `snake_game_init()` - Initialize game with flip dot display
- `snake_game_start()` - Start the game
- `snake_game_pause()` - Pause the game
- `snake_game_resume()` - Resume paused game
- `snake_game_reset()` - Reset game to initial state

### Game Logic
- `snake_game_update()` - Update game state (call periodically)
- `snake_game_change_direction()` - Change snake direction
- `snake_game_render()` - Render game to display

### Demo Mode
- `snake_game_demo()` - Run automated demo for specified duration

## Usage Example
```c
#include "snake.h"
#include "flip_dot.h"

flip_dot_t display;
snake_game_t game;

// Initialize display and game
flip_dot_init(&display, 2000, SWEEP_ROW);
snake_game_init(&game, &display);

// Start game
snake_game_start(&game);

// Game loop
while (snake_game_is_running(&game)) {
    snake_game_update(&game);
    vTaskDelay(game.game_speed_ms / portTICK_PERIOD_MS);
}
```

## Demo Integration
The Snake game is integrated into the main demo loop and will run automatically as part of the system demonstrations. 