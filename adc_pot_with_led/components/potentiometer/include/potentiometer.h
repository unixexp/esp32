/*
	Potentiometer connection pin out (front view)

	pin1 (left) -> GND
	pin2 --------> VP
	pin3 --------> 3V3
*/

#ifndef _POTENTIOMETER_H_
#define _POTENTIOMETER_H_

#include "esp_adc/adc_oneshot.h"

typedef struct {
	adc_oneshot_unit_handle_t handler;
	int unit;
	int channel;
} potentiometer_handler_t;

void init_potentiometer_handler(potentiometer_handler_t *p_handler);
int read_potentiometer_value(potentiometer_handler_t *p_handler);

#endif /* _POTENTIOMETER_H_ */
