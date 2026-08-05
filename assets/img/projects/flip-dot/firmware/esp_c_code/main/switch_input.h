/**
 * @file switch_input.h
 * @brief 
 *
 * @author
 * @date 2025-01-22
 */

#ifndef SWITCH_INPUT_H
#define SWITCH_INPUT_H

#include <stdint.h>
#include <stdbool.h>
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"

// Define callback function type
typedef void (*switch_callback_t)(void);

typedef enum {
    SWITCH_RISING_EDGE,
    SWITCH_FALLING_EDGE
} switch_edge_t;

typedef enum {
    SWITCH_EVENT_PRESSED,
    SWITCH_EVENT_RELEASED
} switch_event_t;

/******************************************************************************
 * Public Definitions and Types
 ******************************************************************************/
#define SWITCH_PIN 36

/******************************************************************************
 * Public Constants
 ******************************************************************************/


/******************************************************************************
 * Public Function Declarations
 ******************************************************************************/
/**
 * @brief Initialize the switch GPIO pin
 */
void switch_input_init(void);


bool switch_input_get_event(switch_event_t* event, TickType_t timeout);

#endif /* SWITCH_INPUT_H */
