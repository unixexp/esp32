#include <stdint.h>
#include <stdio.h>
#include "esp_err.h"
#include "esp_log.h"
#include "fir.h"
#include "fir_tables.h"

static const char *FIR_TAG = "FIR";

void init_fir_processor(void) {
	ESP_LOGI(FIR_TAG, "FIR Init...");
	
}
