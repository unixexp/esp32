#include <stdio.h>
#include "a2dp_util.h"
#include "esp_a2dp_api.h"

static void a2dp_state_cb(esp_a2d_cb_event_t event, esp_a2d_cb_param_t *param) {
	
}

static void a2dp_data_cb(const uint8_t *data, uint32_t len) {
	
}

void init_a2dp(void) {
	esp_a2d_register_callback(&a2dp_state_cb);
	esp_a2d_sink_init();
}
