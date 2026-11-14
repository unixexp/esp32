#ifndef _I2S_H_
#define _I2S_H_

#include <stdint.h>
#include <stdio.h>
#include "esp_a2dp_api.h"
#include "driver/i2s_std.h"
#include "freertos/FreeRTOS.h"
#include "freertos/idf_additions.h"
#include "freertos/task.h"
#include "freertos/semphr.h"
#include "freertos/ringbuf.h"

/*
	One sample (16-bit stereo PCM) = 4 bytes
	Let's take highest buffer size as 64KB: 65536 / 4 = 16384 samples.
	Time of sound will calculates by formula: 16384 samples / 44100 =~ 371 ms
*/
#define RINGBUF_HIGHEST_WATER_LEVEL    (80 * 1024)
#define RINGBUF_PREFETCH_WATER_LEVEL   (4096 * 10)
#define READ_CHUNK_SIZE 4096

#define I2S_CH0_PINOUT_PCM5102 { \
	.bclk_pin = GPIO_NUM_26, \
	.ws_pin = GPIO_NUM_25, \
	.dout_pin = GPIO_NUM_27, \
}

#define I2S_CH1_PINOUT_PCM5102 { \
	.bclk_pin = GPIO_NUM_26, \
	.ws_pin = GPIO_NUM_25, \
	.dout_pin = GPIO_NUM_27, \
}

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
	int bclk_pin;
	int ws_pin;
	int dout_pin;
} i2s_channel_pinout_t;

typedef struct {
	int num;
	i2s_chan_handle_t tx_handler;
	audio_sink_chan_st_t state;
	i2s_channel_pinout_t pinout;
} i2s_channel_t;

typedef struct {
	i2s_channel_t *chan0;
	i2s_channel_t *chan1;
	RingbufHandle_t ring_buf;
	TaskHandle_t write_task_handle;
	SemaphoreHandle_t write_semaphore;
	uint16_t ring_buf_mode;
} audio_sink_srv_i2s_cb_t;

void open_i2s(void);
void close_i2s(void);
void start_i2s(void);
void stop_i2s(void);
void update_i2s_config(esp_a2d_mcc_t *mcc);

void open_i2s_channel(i2s_channel_t *chan);
void start_i2s_channel(i2s_channel_t *chan);
void stop_i2s_channel(i2s_channel_t *chan);
void update_i2s_config(esp_a2d_mcc_t *mcc);
size_t output_i2s_data(const uint8_t *data, size_t size);

#endif /* _I2S_H_ */