#include <stdio.h>
#include "esp_log.h"
#include "i2s_util.h"
#include "esp_a2dp_api.h"

static const char *I2S_LOG_TAG = "I2S";

void start_i2s_channel(void)
{
}

void stop_i2s_channel(void)
{
}

void open_i2s_channel(void)
{
}

void close_i2s_channel(void)
{
}

void update_i2s_channel_config(esp_a2d_mcc_t *mcc) {
	ESP_LOGI(I2S_LOG_TAG, "A2DP audio stream configuration, codec type: %d", mcc->type);
}
