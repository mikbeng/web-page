/**
 * @file clock.h
 * @brief HH:MM clock application for flip dot display
 */

#ifndef CLOCK_H
#define CLOCK_H

#include <stdint.h>
#include <stdbool.h>
#include "flip_dot.h"

typedef bool (*clock_app_abort_cb_t)(void);

/**
 * Run the clock until should_abort returns true (or NULL to run forever).
 * Renders HH:MM from SNTP wall time and refreshes once per minute during
 * active hours (06:00–22:00). Shows "Godnatt" during quiet hours.
 */
void clock_app_run(flip_dot_t *display, clock_app_abort_cb_t should_abort);

#endif /* CLOCK_H */
