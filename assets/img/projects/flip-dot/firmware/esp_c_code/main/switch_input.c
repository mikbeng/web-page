/**
 * @file switch_input.c
 * @brief 
 *
 * @author
 * @date 2025-01-22
 */

#include "switch_input.h"
#include "driver/gpio.h"
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"

/******************************************************************************
 * Private Definitions and Types
 ******************************************************************************/
#define DEBOUNCE_TIME_MS 50  // Adjust this value based on your switch characteristics

/******************************************************************************
 * Private Function Declarations
 ******************************************************************************/

/******************************************************************************
 * Private Variables
 ******************************************************************************/
static uint32_t last_press_time = 0;
static bool last_state = false;
static QueueHandle_t switch_event_queue = NULL;

/******************************************************************************
 * Private Function Implementations
 ******************************************************************************/
static void IRAM_ATTR gpio_isr_handler(void* arg)
{
    uint32_t current_time = xTaskGetTickCountFromISR() * portTICK_PERIOD_MS;
    
    // Basic debounce in ISR
    if ((current_time - last_press_time) >= DEBOUNCE_TIME_MS) {
        bool current_state = (gpio_get_level(SWITCH_PIN) == 0);
        
        if (current_state != last_state) {
            switch_event_t event = current_state ? 
                SWITCH_EVENT_PRESSED : SWITCH_EVENT_RELEASED;
            
            // Send event to queue from ISR
            BaseType_t xHigherPriorityTaskWoken = pdFALSE;
            xQueueSendFromISR(switch_event_queue, &event, &xHigherPriorityTaskWoken);
            if (xHigherPriorityTaskWoken) {
                portYIELD_FROM_ISR();
            }
            
            last_state = current_state;
            last_press_time = current_time;
        }
    }
}

/******************************************************************************
 * Public Function Implementations
 ******************************************************************************/
void switch_input_init(void)
{
    // Create event queue
    switch_event_queue = xQueueCreate(10, sizeof(switch_event_t));
    
    gpio_config_t input_config = {
        .pin_bit_mask = (1ULL << SWITCH_PIN),
        .mode = GPIO_MODE_INPUT,
        .pull_up_en = GPIO_PULLUP_DISABLE,    // Using internal pullup
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_ANYEDGE        // Interrupt on both edges
    };
    gpio_config(&input_config);

    // Install GPIO ISR service
    gpio_install_isr_service(0);
    
    // Add ISR handler
    gpio_isr_handler_add(SWITCH_PIN, gpio_isr_handler, NULL);
}

bool switch_input_get_event(switch_event_t* event, TickType_t timeout)
{
    return xQueueReceive(switch_event_queue, event, timeout) == pdTRUE;
}
