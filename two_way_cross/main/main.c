#include <stdio.h>
#include <stdbool.h>
#include <unistd.h>

#include "a2dp.h"
#include "bt.h"
#include "fir.h"

void app_main(void)
{
	uint8_t pin[4] = {'3', '8', '3', '8'};
	
    init_nvs();
	init_fir_processor();
	init_bluetooth_controller();
	init_bluedroid_host("YR Audio Module", true, pin, sizeof(pin));
	init_a2dp();
	
}
