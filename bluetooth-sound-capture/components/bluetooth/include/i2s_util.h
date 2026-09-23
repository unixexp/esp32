#ifndef _I2S_UTIL_H_
#define _I2S_UTIL_H_

#include <stdio.h>
#include "esp_a2dp_api.h"
#include "driver/i2s_std.h"

typedef enum {
    RINGBUFFER_MODE_PROCESSING,    /* ringbuffer is buffering incoming audio data */
    RINGBUFFER_MODE_PREFETCHING,   /* ringbuffer is buffering incoming audio data */
    RINGBUFFER_MODE_DROPPING       /* ringbuffer is not buffering (dropping) incoming audio data */
} audio_sink_ringbuffer_mode_t;

typedef enum {
    CHANNEL_STATUS_IDLE,
    CHANNEL_STATUS_OPENED,
    CHANNEL_STATUS_ENABLED
} audio_sink_chan_st_t;

typedef struct {                                 
    i2s_chan_handle_t tx_chan;
	audio_sink_chan_st_t chan_st;
	
} audio_sink_srv_i2s_cb_t;

static audio_sink_srv_i2s_cb_t s_i2s_cb;

void open_i2s_channel(void);
void close_i2s_channel(void);
void start_i2s_channel(void);
void stop_i2s_channel(void);
void update_i2s_channel_config(esp_a2d_mcc_t *mcc);

#endif /* _I2S_UTIL_H_ */