#include <stdio.h>
#include <unistd.h>
#include "esp_err.h"
#include "hal/adc_types.h"
#include "esp_adc/adc_oneshot.h"
#include "potentiometer.h"

void init_potentiometer_handler(potentiometer_handler_t *p_handler) {

	adc_oneshot_unit_init_cfg_t adc_handler_config = {
		.unit_id = p_handler->unit,
		.ulp_mode = ADC_ULP_MODE_DISABLE
	};
	
	ESP_ERROR_CHECK(adc_oneshot_new_unit(&adc_handler_config, &p_handler->handler));
	
	adc_oneshot_chan_cfg_t adc_channel_config = {
		.bitwidth = ADC_BITWIDTH_DEFAULT,
		.atten = ADC_ATTEN_DB_12
	};
	
	ESP_ERROR_CHECK(adc_oneshot_config_channel(p_handler->handler, p_handler->channel,  &adc_channel_config));
}

int read_potentiometer_value(potentiometer_handler_t *p_handler)
{	
	int value = 0;
	adc_oneshot_read(p_handler->handler, p_handler->channel, &value);
	return value;
}
