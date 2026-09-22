#ifndef _I2S_UTIL_H_
#define _I2S_UTIL_H_

#include <stdio.h>
#include "driver/i2s_std.h"
#include "esp_a2dp_api.h"

void start_i2s_channel(void);
void stop_i2s_channel(void);
void open_i2s_channel(void);
void close_i2s_channel(void);
void update_i2s_channel_config(esp_a2d_mcc_t *mcc);

#endif /* _I2S_UTIL_H_ */