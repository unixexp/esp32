#include <stdio.h>
#include <stdbool.h>
#include <unistd.h>
#include "driver/gpio.h"
#include "esp_err.h"
#include "freertos/FreeRTOS.h"
#include "freertos/projdefs.h"
#include "hal/adc_types.h"
#include "hal/gpio_types.h"
#include "esp_adc/adc_oneshot.h"

/*
	Potentiometer connection pin out (front view)

	pin1 (left) -> GND
	pin2 --------> VP
	pin3 --------> 3V3
*/

// ADC Module number
#define POT_ADC_UNIT ADC_UNIT_1
// GPIO36 'VP' pin => channel mapping
#define POT_ADC_CH ADC_CHANNEL_0

void init_adc_pot_handler(adc_oneshot_unit_handle_t *out_handler) {

	adc_oneshot_unit_init_cfg_t adc_handler_config = {
		.unit_id = POT_ADC_UNIT,
		.ulp_mode = ADC_ULP_MODE_DISABLE
	};
	
	ESP_ERROR_CHECK(adc_oneshot_new_unit(&adc_handler_config, out_handler));
	
	adc_oneshot_chan_cfg_t adc_channel_config = {
		.bitwidth = ADC_BITWIDTH_DEFAULT,
		.atten = ADC_ATTEN_DB_12
	};
	
	ESP_ERROR_CHECK(adc_oneshot_config_channel(*out_handler, POT_ADC_CH, &adc_channel_config));
}

void app_main(void)
{
	adc_oneshot_unit_handle_t adc_pot_handler = NULL;
	init_adc_pot_handler(&adc_pot_handler);
	
	int pot_raw_value = 0;
	
    while (true) {
		adc_oneshot_read(adc_pot_handler, POT_ADC_CH, &pot_raw_value);
		printf("Potentiometer RAW Value: %d\n", pot_raw_value);
        vTaskDelay(pdMS_TO_TICKS(200));
    }
}
