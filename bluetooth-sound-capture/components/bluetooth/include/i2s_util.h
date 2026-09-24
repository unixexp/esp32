#ifndef _I2S_UTIL_H_
#define _I2S_UTIL_H_

#include <stdint.h>
#include <stdio.h>
#include "esp_a2dp_api.h"
#include "driver/i2s_std.h"
#include "freertos/FreeRTOS.h"
#include "freertos/idf_additions.h"
#include "freertos/task.h"
#include "freertos/semphr.h"
#include "freertos/ringbuf.h"

#define RINGBUF_HIGHEST_WATER_LEVEL    (32 * 1024)
#define RINGBUF_PREFETCH_WATER_LEVEL   (20 * 1024)

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
	RingbufHandle_t ring_buf;
	TaskHandle_t write_task_handle;
	SemaphoreHandle_t write_semaphore;
	uint16_t ring_buf_mode;
} audio_sink_srv_i2s_cb_t;

static audio_sink_srv_i2s_cb_t s_i2s_cb;

void open_i2s_channel(void);
void close_i2s_channel(void);
void start_i2s_channel(void);
void stop_i2s_channel(void);
void update_i2s_channel_config(esp_a2d_mcc_t *mcc);
static void i2s_task_handler(void *args);
size_t i2s_data_output(const uint8_t *data, size_t size);

#endif /* _I2S_UTIL_H_ */