/**
 * @file pwr_ctrl.h
 * @brief 
 *
 * @author Mikael Bengtsson
 * @date 2025-05-18
 */

#ifndef PWR_CTRL_H
#define PWR_CTRL_H

#include <stdint.h>
#include <stdbool.h>
#include "esp_err.h"

/******************************************************************************
 * Public Definitions and Types
 ******************************************************************************/


/******************************************************************************
 * Public Constants
 ******************************************************************************/


/******************************************************************************
 * Public Function Declarations
 ******************************************************************************/
esp_err_t enable_24V_supply(void);
esp_err_t disable_24V_supply(void);
esp_err_t enable_flip_board_logic_supply(void);
esp_err_t disable_flip_board_logic_supply(void);
esp_err_t enable_flip_board(void);
esp_err_t disable_flip_board(void);
esp_err_t get_battery_voltage(int* voltage_mv);
esp_err_t init_power_control(void);

#endif /* PWR_CTRL_H */
