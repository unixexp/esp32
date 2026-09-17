#include <stdio.h>
#include <stdbool.h>
#include <string.h>
#include <sys/types.h>
#include <unistd.h>
#include "driver/gpio.h"
#include "freertos/idf_additions.h"
#include "freertos/projdefs.h"
#include "hal/gpio_types.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "portmacro.h"

#define CONTROL_TASK_SUF "_control_task"

typedef struct {
	char *led_name;
	u_int8_t gpio_pin;
	u_int16_t delay_ms;
	u_int16_t last_delay_ms;
} blinked_params_t;

static blinked_params_t led_params = {
	.led_name = "my led",
	.gpio_pin = 2,
	.delay_ms = 1000,
	.last_delay_ms = 0
};

static SemaphoreHandle_t mutex;

void control_task(void *pvParameters) {
	blinked_params_t *params = (blinked_params_t *) pvParameters;
	
	while(true) {
		vTaskDelay(pdMS_TO_TICKS(3000));
		
		if (mutex != NULL) {
			if (xSemaphoreTake(mutex, portMAX_DELAY) == pdTRUE) {
				if (params->last_delay_ms == 0) {
					params->last_delay_ms = params->delay_ms;
				}
				
				if (params->delay_ms == params->last_delay_ms && params->last_delay_ms >= 100) {
					params->delay_ms = params->delay_ms / 4;
				} else {
					params->delay_ms = params->last_delay_ms;
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
