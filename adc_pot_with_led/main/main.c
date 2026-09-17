#include <stdio.h>
#include <stdbool.h>
#include <string.h>
#include <unistd.h>
#include "driver/gpio.h"
#include "freertos/FreeRTOS.h"
#include "potentiometer.h"

// ADC Module number
#define POT_ADC_UNIT ADC_UNIT_1
// GPIO36 'VP' pin => channel mapping
#define POT_ADC_CH ADC_CHANNEL_0

#define CONTROL_TASK_SUF "_control_task"

typedef struct {
	char *led_name;
	u_int8_t gpio_pin;
	u_int16_t delay_ms;
} blinked_params_t;

static blinked_params_t led_params = {
	.led_name = "my led",
	.gpio_pin = 2,
	.delay_ms = 1000
};

static SemaphoreHandle_t mutex;

void control_task(void *pvParameters) {
	blinked_params_t *params = (blinked_params_t *) pvParameters;
	
	int p_value = 0;
	potentiometer_handler_t p_handler = { .handler = NULL, .channel = POT_ADC_CH, .unit = POT_ADC_UNIT };
	init_potentiometer_handler(&p_handler);

	while(true) {
		p_value = read_potentiometer_value(&p_handler);
		vTaskDelay(pdMS_TO_TICKS(100));
		
		if (mutex != NULL) {
			if (xSemaphoreTake(mutex, portMAX_DELAY) == pdTRUE) {
				if (p_value > 100) {
					params->delay_ms = p_value;
				}
				xSemaphoreGive(mutex);
			}
		}
	}
}

void blink_task(void *pvParameters) {
	blinked_params_t *params = (blinked_params_t *) pvParameters;
	u_int8_t led_state = 0;
	u_int16_t local_delay_ms = params->delay_ms;
	
	gpio_reset_pin(params->gpio_pin);
	gpio_set_direction(params->gpio_pin, GPIO_MODE_OUTPUT);

	while(true) {
		if (xSemaphoreTake(mutex, 10) == pdTRUE) {
			local_delay_ms = params->delay_ms;
			xSemaphoreGive(mutex);
		}
		led_state = !led_state;
		gpio_set_level(params->gpio_pin, led_state);
		vTaskDelay(pdMS_TO_TICKS(local_delay_ms));
	}
}

void app_main(void)
{
	mutex = xSemaphoreCreateMutex();
		 
	int ctl_task_name_size = strlen(led_params.led_name) + sizeof(CONTROL_TASK_SUF);
	char ctl_task_name[ctl_task_name_size];
	snprintf(ctl_task_name, ctl_task_name_size, "%s%s", led_params.led_name, CONTROL_TASK_SUF);
	
	if (mutex != NULL) {
		xTaskCreate(blink_task, led_params.led_name, 2048, (void*)&led_params, 1, NULL);
		xTaskCreate(control_task, ctl_task_name, 2048, (void*)&led_params, 1, NULL);
	}

}
