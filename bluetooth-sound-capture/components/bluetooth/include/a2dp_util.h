#ifndef _A2DP_UTIL_H_
#define _A2DP_UTIL_H_

#include <stdio.h>
#include "esp_a2dp_api.h"

static void a2dp_state_cb(esp_a2d_cb_event_t event, esp_a2d_cb_param_t *param);
static void a2dp_pcm_data_cb(const uint8_t *data, uint32_t len);
void init_a2dp(void);

#endif /* _A2DP_UTIL_H_ */
