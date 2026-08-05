/**
 * @file pwr_ctrl.c
 * @brief 
 *
 * @author Mikael Bengtsson
 * @date 2025-05-18
 */

#include "pwr_ctrl.h"
#include "driver/gpio.h"
#include "esp_log.h"
#include "esp_adc/adc_oneshot.h"
#include "esp_adc/adc_cali.h"
#include "esp_adc/adc_cali_scheme.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

/******************************************************************************
 * Private Definitions and Types
 ******************************************************************************/
#define PWR_CTRL_24V_PIN GPIO_NUM_8
#define FLIP_BOARD_ON_PIN GPIO_NUM_7
#define BATTERY_VOLTAGE_ADC_CH ADC_CHANNEL_7
#define ADC_ATTEN ADC_ATTEN_DB_12

const static char *TAG = "PWR_CTRL";
adc_oneshot_unit_handle_t adc1_handle;
adc_cali_handle_t adc1_cali_handle = NULL;

bool do_calibration1_chan0 = false;
/******************************************************************************
 * Private Function Declarations
 ******************************************************************************/
static bool _adc_calibration_init(adc_unit_t unit, adc_channel_t channel, adc_atten_t atten, adc_cali_handle_t *out_handle);
static void _adc_calibration_deinit(adc_cali_handle_t handle);

/******************************************************************************
 * Private Function Implementations
 ******************************************************************************/
static bool _adc_calibration_init(adc_unit_t unit, adc_channel_t channel, adc_atten_t atten, adc_cali_handle_t *out_handle)
{
    adc_cali_handle_t handle = NULL;
    esp_err_t ret = ESP_FAIL;
    bool calibrated = false;

#if ADC_CALI_SCHEME_CURVE_FITTING_SUPPORTED
    if (!calibrated) {
        ESP_LOGI(TAG, "calibration scheme version is %s", "Curve Fitting");
        adc_cali_curve_fitting_config_t cali_config = {
            .unit_id = unit,
            .chan = channel,
            .atten = atten,
            .bitwidth = ADC_BITWIDTH_DEFAULT,
        };
        ret = adc_cali_create_scheme_curve_fitting(&cali_config, &handle);
        if (ret == ESP_OK) {
            calibrated = true;
        }
    }
#endif

#if ADC_CALI_SCHEME_LINE_FITTING_SUPPORTED
    if (!calibrated) {
        ESP_LOGI(TAG, "calibration scheme version is %s", "Line Fitting");
        adc_cali_line_fitting_config_t cali_config = {
            .unit_id = unit,
            .atten = atten,
            .bitwidth = ADC_BITWIDTH_DEFAULT,
        };
        ret = adc_cali_create_scheme_line_fitting(&cali_config, &handle);
        if (ret == ESP_OK) {
            calibrated = true;
        }
    }
#endif

    *out_handle = handle;
    if (ret == ESP_OK) {
        ESP_LOGI(TAG, "Calibration Success");
    } else if (ret == ESP_ERR_NOT_SUPPORTED || !calibrated) {
        ESP_LOGW(TAG, "eFuse not burnt, skip software calibration");
    } else {
        ESP_LOGE(TAG, "Invalid arg or no memory");
    }

    return calibrated;
}

static void _adc_calibration_deinit(adc_cali_handle_t handle)
{
#if ADC_CALI_SCHEME_CURVE_FITTING_SUPPORTED
    ESP_LOGI(TAG, "deregister %s calibration scheme", "Curve Fitting");
    ESP_ERROR_CHECK(adc_cali_delete_scheme_curve_fitting(handle));

#elif ADC_CALI_SCHEME_LINE_FITTING_SUPPORTED
    ESP_LOGI(TAG, "deregister %s calibration scheme", "Line Fitting");
    ESP_ERROR_CHECK(adc_cali_delete_scheme_line_fitting(handle));
#endif
}

/******************************************************************************
 * Public Function Implementations
 ******************************************************************************/
esp_err_t init_power_control(void)
{
    // Initialize power control

    //Initiaze GPIOs
    gpio_config_t io_config = {   
        .pin_bit_mask = (1ULL << PWR_CTRL_24V_PIN) | (1ULL << FLIP_BOARD_ON_PIN),
        .mode = GPIO_MODE_OUTPUT,
        .pull_up_en = GPIO_PULLUP_DISABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE
    };
    gpio_config(&io_config);

    disable_flip_board();

    //Initialize ADC
    //-------------ADC1 Init---------------//
    adc_oneshot_unit_init_cfg_t init_config1 = {
        .unit_id = ADC_UNIT_1,
    };
    ESP_ERROR_CHECK(adc_oneshot_new_unit(&init_config1, &adc1_handle));

    //-------------ADC1 Config---------------//
    adc_oneshot_chan_cfg_t config = {
        .bitwidth = ADC_BITWIDTH_DEFAULT,
        .atten = ADC_ATTEN,
    };
    ESP_ERROR_CHECK(adc_oneshot_config_channel(adc1_handle, BATTERY_VOLTAGE_ADC_CH, &config));

    //-------------ADC1 Calibration Init---------------//
    do_calibration1_chan0 = _adc_calibration_init(ADC_UNIT_1, BATTERY_VOLTAGE_ADC_CH, ADC_ATTEN, &adc1_cali_handle);

    return ESP_OK;
}


esp_err_t enable_24V_supply(void)
{
    // Enable 24V supply
    //Set GPIO 8 to high
    gpio_set_level(PWR_CTRL_24V_PIN, 1);
    ESP_LOGI(TAG, "24V supply enabled");
    return ESP_OK;
}

esp_err_t disable_24V_supply(void)
{
    // Disable 24V supply
    //Set GPIO 8 to low
    gpio_set_level(PWR_CTRL_24V_PIN, 0);
    ESP_LOGI(TAG, "24V supply disabled");
    return ESP_OK;
}

esp_err_t enable_flip_board_logic_supply(void)
{
    // Enable flip board logic supply
    //Set GPIO 7 to high
    gpio_set_level(FLIP_BOARD_ON_PIN, 1);
    ESP_LOGI(TAG, "Flip board logic supply enabled");
    return ESP_OK;
}

esp_err_t disable_flip_board_logic_supply(void)
{
    // Disable flip board logic supply
    //Set GPIO 7 to low
    gpio_set_level(FLIP_BOARD_ON_PIN, 0);
    ESP_LOGI(TAG, "Flip board logic supply disabled");
    return ESP_OK;
}

esp_err_t enable_flip_board(void)
{
    //Enable 24V supply
    //enable_24V_supply(); //We don't enable the 24V supply since we are supplying with 24V.
    
    //Enable flip board logic supply
    enable_flip_board_logic_supply();

    vTaskDelay(250 / portTICK_PERIOD_MS);
    return ESP_OK;
}

esp_err_t disable_flip_board(void)
{
    //Disable 24V supply
    disable_24V_supply();
    //Disable flip board logic supply
    disable_flip_board_logic_supply();
    return ESP_OK;
}

esp_err_t get_battery_voltage(int* voltage_mv)
{

    int adc_raw;
    int voltage;

    // Get battery voltage
    //Read ADC channel 0
    ESP_ERROR_CHECK(adc_oneshot_read(adc1_handle, BATTERY_VOLTAGE_ADC_CH, &adc_raw));
    ESP_LOGD(TAG, "ADC%d Channel[%d] Raw Data: %d", ADC_UNIT_1 + 1, BATTERY_VOLTAGE_ADC_CH, adc_raw);
    if (do_calibration1_chan0) {
        ESP_ERROR_CHECK(adc_cali_raw_to_voltage(adc1_cali_handle, adc_raw, &voltage));
        ESP_LOGD(TAG, "ADC%d Channel[%d] Cali Voltage: %d mV", ADC_UNIT_1 + 1, BATTERY_VOLTAGE_ADC_CH, voltage);
    }
    *voltage_mv = voltage;
    return ESP_OK;
}

