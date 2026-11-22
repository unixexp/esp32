#include <stdint.h>
#include <stdio.h>
#include "i2s.h"
#include "driver/i2s_common.h"
#include "esp_err.h"
#include "esp_log.h"
#include "driver/i2s_std.h"
#include "esp_a2dp_api.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/projdefs.h"
#include "freertos/ringbuf.h"
#include "freertos/task.h"
#include "freertos/semphr.h"
#include "freertos/ringbuf.h"
#include "portmacro.h"
#include "dsp.h"
#include "dsps_fir.h"
#include "fir_tables.h"

static const char *I2S_LOG_TAG = "I2S";
static uint32_t s_underflow_cnt = 0;

static audio_sink_srv_i2s_cb_t s_i2s_cb;
static i2s_channel_t chan0 = {
	.num = 0,
	.tx_handler = NULL,
	.state = CHANNEL_STATUS_IDLE,
	.pinout = I2S_CH0_PINOUT_PCM5102
};

static i2s_channel_t chan1 = {
	.num = 1,
	.tx_handler = NULL,
	.state = CHANNEL_STATUS_IDLE,
	.pinout = I2S_CH1_PINOUT_PCM5102
};

static float fdata_l[MAX_SAMPLES], fdata_r[MAX_SAMPLES];
static uint8_t data_wf_tw_l[MAX_SAMPLES * sizeof(int16_t) * 2];
static uint8_t data_wf_tw_r[MAX_SAMPLES * sizeof(int16_t) * 2];

static float fdata_wf[MAX_SAMPLES], fdata_tw[MAX_SAMPLES];
static uint8_t data_wf_tw_mono[MAX_SAMPLES * sizeof(int16_t) * 2];

static fir_f32_t fir_hpf_mono;
static float fir_delay_hpf_mono[FIR_TAPS];

static void i2s_task_handler(void *args) {

	uint8_t *data = NULL;
	size_t item_size = 0;
	
	ESP_LOGI(I2S_LOG_TAG, "Task handler started.");
	
	// Initialize FIR coefficients
	dsp_init_fir_ch(&fir_hpf_mono, fir_delay_hpf_mono, 0, &fir_hpf_table, FIR_TAPS);
	
	peak_filter_init_coeffs(44100.0f, 160.0f, 1.8f, -8.0f);
	
	// Variables to logging delta time between I2S writes
	uint32_t dbg_iter = 0;
	int64_t max_dt_ch0 = 0;
	int64_t max_dt_ch1 = 0;
	size_t last_item_size = 0;
	const uint32_t dbg_period = 200;
	// Variables to logging delta time between I2S writes END
	
	for (;;) {
		if (s_i2s_cb.ring_buf_mode == RINGBUFFER_MODE_PREFETCHING) {
			size_t free_size = xRingbufferGetCurFreeSize(s_i2s_cb.ring_buf);
            size_t filled_bytes = RINGBUF_HIGHEST_WATER_LEVEL - free_size;
            
            if (filled_bytes < RINGBUF_PREFETCH_WATER_LEVEL) {
                vTaskDelay(pdMS_TO_TICKS(10));
                continue;
            } else {
                s_i2s_cb.ring_buf_mode = RINGBUFFER_MODE_PROCESSING;
            }
		}
		
		item_size = 0;
		data = (uint8_t *)xRingbufferReceiveUpTo(s_i2s_cb.ring_buf, &item_size,
			(TickType_t)pdMS_TO_TICKS(400), READ_CHUNK_SIZE);
		
		if (data != NULL) {
			size_t len_bytes = item_size & ~((size_t)3);
			if (len_bytes > 0 &&
					s_i2s_cb.chan0->state == CHANNEL_STATUS_ENABLED &&
					s_i2s_cb.chan1->state == CHANNEL_STATUS_ENABLED) {
				
				// For mono (joined stereo) 2-way system
				size_t samples = dsp_mono_downmix((int16_t *)data, len_bytes);
				if (samples == 0) {
					ESP_LOGE(I2S_LOG_TAG, "dsp_mono_downmix -> samples = %d", samples);
					return;
				}
				
				// For stereo 2-way system
				// samples = dsp_unpack_dac_stereo((int16_t *)data, fdata_l, fdata_r, len_bytes);
				
				// For mono (joined stereo) 2-way system
				samples = dsp_unpack_dac_stereo((int16_t *)data, fdata_wf, fdata_tw, len_bytes);
				if (samples > 0) {
					// For mono (joined stereo) 2-way system
					dsp_pack_dac_stereo(fdata_wf, fdata_tw, (int16_t *)data_wf_tw_mono, len_bytes);
					
					// For stereo 2-way system
					// dsp_pack_dac_stereo(fdata_l, fdata_l, (int16_t *)data_wf_tw_l, len_bytes);
					// dsp_pack_dac_stereo(fdata_r, fdata_r, (int16_t *)data_wf_tw_r, len_bytes);
					
					size_t bytes_writen_ch0 = 0;
					
					// Remove comment for stereo 2-way system
					// size_t bytes_writen_ch1 = 0;
					
					// For mono (joined stereo) 2-way system
					i2s_channel_write(s_i2s_cb.chan0->tx_handler, data_wf_tw_mono, len_bytes, &bytes_writen_ch0, portMAX_DELAY);
					
					/*
					// For stereo 2-way system with logging. TODO: compensate timings
					int64_t t0 = esp_timer_get_time();
					i2s_channel_write(s_i2s_cb.chan0->tx_handler, data_wf_tw_l, len_bytes, &bytes_writen_ch0, portMAX_DELAY);
					int64_t t1 = esp_timer_get_time();
					i2s_channel_write(s_i2s_cb.chan1->tx_handler, data_wf_tw_r, len_bytes, &bytes_writen_ch1, portMAX_DELAY);
					int64_t t2 = esp_timer_get_time();
					
					int64_t dt_ch0 = t1 - t0;
					int64_t dt_ch1 = t2 - t1;
					
					if (dt_ch0 > max_dt_ch0) max_dt_ch0 = dt_ch0;
					if (dt_ch1 > max_dt_ch1) max_dt_ch1 = dt_ch1;
					last_item_size = item_size;
					
					dbg_iter++;
					if (dbg_iter >= dbg_period) {
						ESP_LOGI(I2S_LOG_TAG,
						                         "sz=%u, dt0=%lld us (max=%lld), dt1=%lld us (max=%lld)",
						                         (unsigned)last_item_size,
						                         (long long)dt_ch0, (long long)max_dt_ch0,
						                         (long long)dt_ch1, (long long)max_dt_ch1);
						                dbg_iter = 0;
						                max_dt_ch0 = 0;
						                max_dt_ch1 = 0;
										
						ESP_LOGI(I2S_LOG_TAG,
						                         "len=%u, w0=%u, w1=%u, match0=%d, match1=%d",
						                         (unsigned)len_bytes,
						                         (unsigned)bytes_writen_ch0,
						                         (unsigned)bytes_writen_ch1,
						                         (bytes_writen_ch0 == len_bytes) ? 1 : 0,
						                         (bytes_writen_ch1 == len_bytes) ? 1 : 0);
					}
					*/
				} else {
					ESP_LOGE(I2S_LOG_TAG, "dsp_unpack_dac_stereo -> samples = %d", samples);
				}
			} else {
				vTaskDelay(pdMS_TO_TICKS(10));
			}
			
			vRingbufferReturnItem(s_i2s_cb.ring_buf, (void *)data);
		} else {
			s_underflow_cnt++;
			size_t bytes_free = xRingbufferGetCurFreeSize(s_i2s_cb.ring_buf);
			ESP_LOGW(I2S_LOG_TAG, "[#%lu] Buffer is empty! Input data late. Free bytes in buffer: %d bytes",
				s_underflow_cnt, bytes_free);

			s_i2s_cb.ring_buf_mode = RINGBUFFER_MODE_PREFETCHING;
		}
	}

}

void open_i2s(void) {
	open_i2s_channel(&chan0);
	open_i2s_channel(&chan1);
	s_i2s_cb.chan0 = &chan0;
	s_i2s_cb.chan1 = &chan1;
}

void close_i2s(void) {
	stop_i2s_channel(s_i2s_cb.chan0);
	stop_i2s_channel(s_i2s_cb.chan1);
	
	if (s_i2s_cb.write_task_handle) {
		vTaskDelete(s_i2s_cb.write_task_handle);
		s_i2s_cb.write_task_handle = NULL;
	}
	
	if (s_i2s_cb.ring_buf) {
		vRingbufferDelete(s_i2s_cb.ring_buf);
		s_i2s_cb.ring_buf = NULL;
	}
	
	if (s_i2s_cb.write_semaphore) {
		vSemaphoreDelete(s_i2s_cb.write_semaphore);
		s_i2s_cb.write_semaphore = NULL;
	}
	
	if (s_i2s_cb.chan0->state == CHANNEL_STATUS_OPENED) {
		ESP_ERROR_CHECK(i2s_del_channel(s_i2s_cb.chan0->tx_handler));
		s_i2s_cb.chan0->state = CHANNEL_STATUS_IDLE;
	}
	
	if (s_i2s_cb.chan1->state == CHANNEL_STATUS_OPENED) {
			ESP_ERROR_CHECK(i2s_del_channel(s_i2s_cb.chan1->tx_handler));
			s_i2s_cb.chan1->state = CHANNEL_STATUS_IDLE;
		}
	
	memset(&s_i2s_cb, 0, sizeof(s_i2s_cb));

	ESP_LOGI(I2S_LOG_TAG, "Channel %d changed state: CHANNEL_STATUS_IDLE", s_i2s_cb.chan0->num);
	ESP_LOGI(I2S_LOG_TAG, "Channel %d changed state: CHANNEL_STATUS_IDLE", s_i2s_cb.chan1->num);
}

void start_i2s(void) {
	start_i2s_channel(s_i2s_cb.chan0);
	start_i2s_channel(s_i2s_cb.chan1);
	
	ESP_LOGI(I2S_LOG_TAG, "Mode changed: RINGBUFFER_MODE_PREFETCHING");
	    s_i2s_cb.ring_buf_mode = RINGBUFFER_MODE_PREFETCHING;
    if ((s_i2s_cb.write_semaphore == NULL) && (s_i2s_cb.write_semaphore = xSemaphoreCreateBinary()) == NULL) {
        ESP_LOGE(I2S_LOG_TAG, "%s, Semaphore create failed", __func__);
        
		if (s_i2s_cb.write_semaphore != NULL) {
			vSemaphoreDelete(s_i2s_cb.write_semaphore);
			s_i2s_cb.write_semaphore = NULL;	
		}
		
    }
    if ((s_i2s_cb.ring_buf == NULL) && (s_i2s_cb.ring_buf = xRingbufferCreate(RINGBUF_HIGHEST_WATER_LEVEL, RINGBUF_TYPE_BYTEBUF)) == NULL) {
        ESP_LOGE(I2S_LOG_TAG, "%s, Ringbuffer create failed", __func__);
        
		if (s_i2s_cb.ring_buf != NULL) {
			vRingbufferDelete(s_i2s_cb.ring_buf);
			s_i2s_cb.ring_buf = NULL;	
		}
    }
    if (s_i2s_cb.write_task_handle == NULL) {
        if (xTaskCreatePinnedToCore(i2s_task_handler, "BtI2STask", 4 * 1024, NULL, 5, &s_i2s_cb.write_task_handle, 1) != pdPASS) {
            ESP_LOGE(I2S_LOG_TAG, "%s, Task create failed", __func__);
            
			if (s_i2s_cb.write_task_handle != NULL) {
				vTaskDelete(s_i2s_cb.write_task_handle);
				s_i2s_cb.write_task_handle = NULL;	
			}

        }
    }
}

void open_i2s_channel(i2s_channel_t *chan) {
	if (chan->state != CHANNEL_STATUS_IDLE) {
        ESP_LOGW(I2S_LOG_TAG, "Channel %d already open, skipping initialization", chan->num);
        return;
    }
	i2s_chan_config_t chan_cfg = I2S_CHANNEL_DEFAULT_CONFIG(chan->num, I2S_ROLE_MASTER);
    chan_cfg.auto_clear = true;
	chan_cfg.dma_desc_num = 4;
	chan_cfg.dma_frame_num = 512;
    i2s_std_config_t std_cfg = {
		.clk_cfg = I2S_STD_CLK_DEFAULT_CONFIG(44100),
        .slot_cfg = I2S_STD_MSB_SLOT_DEFAULT_CONFIG(I2S_DATA_BIT_WIDTH_16BIT, I2S_SLOT_MODE_STEREO),
        .gpio_cfg = {
            .mclk = I2S_GPIO_UNUSED,
            .bclk = chan->pinout.bclk_pin,
            .ws = chan->pinout.ws_pin,
            .dout = chan->pinout.dout_pin,
            .din = I2S_GPIO_UNUSED,
            .invert_flags = {
                .mclk_inv = false,
                .bclk_inv = false,
                .ws_inv = false,
            },
        },
    };
	std_cfg.clk_cfg.clk_src = I2S_CLK_SRC_APLL;
    /* initialize I2S channel */
    ESP_ERROR_CHECK(i2s_new_channel(&chan_cfg, &chan->tx_handler, NULL));
    ESP_ERROR_CHECK(i2s_channel_init_std_mode(chan->tx_handler, &std_cfg));
    chan->state = CHANNEL_STATUS_OPENED;
	ESP_LOGI(I2S_LOG_TAG, "Channel %d changed state: CHANNEL_STATUS_OPENED", chan->num);
}

void start_i2s_channel(i2s_channel_t *chan) {
	if (chan->state != CHANNEL_STATUS_OPENED) {
		ESP_LOGE(I2S_LOG_TAG, "%s TX channel wrong state: %d", __func__, chan->state);
		return;
	}
	
	ESP_ERROR_CHECK(i2s_channel_enable(chan->tx_handler));
	ESP_LOGI(I2S_LOG_TAG, "Channel %d changed state: CHANNEL_STATUS_ENABLED", chan->num);
	
	chan->state = CHANNEL_STATUS_ENABLED;
}

void stop_i2s_channel(i2s_channel_t *chan) {
	if (chan->state == CHANNEL_STATUS_ENABLED) {
		ESP_ERROR_CHECK(i2s_channel_disable(chan->tx_handler));
		chan->state = CHANNEL_STATUS_OPENED;
		ESP_LOGI(I2S_LOG_TAG, "Channel %d changed state: CHANNEL_STATUS_OPENED", chan->num);
	}
}

void update_i2s_config(esp_a2d_mcc_t *mcc) {	
	int sample_rate = 0;
	int ch_count = 0;
	
	if (mcc->type == ESP_A2D_MCT_M24) {
		ESP_LOGI(I2S_LOG_TAG, "A2DP audio stream configuration, codec type: AAC (%d)", mcc->type);
		// AAC Codec
		if (mcc->cie.m24_info.samp_freq1 & ESP_A2D_M24_CIE_SF1_44K) {
			sample_rate = 44100;
		} else {
			ESP_LOGE(I2S_LOG_TAG, "Unsupported A2DP audio stream sample rate: %d", mcc->cie.m24_info.samp_freq1);
			ESP_LOGE(I2S_LOG_TAG, "Unsupported A2DP audio stream sample rate2: %d", mcc->cie.m24_info.samp_freq2);
			return;
		}
		
		if (mcc->cie.m24_info.ch & ESP_A2D_M24_CIE_CH_1) {
            ch_count = 1;
        } else if (mcc->cie.m24_info.ch & ESP_A2D_M24_CIE_CH_2) {
			ch_count = 2;
		} else {
			ESP_LOGE(I2S_LOG_TAG, "Unsupported A2DP audio stream channel mode: %d", mcc->cie.sbc_info.ch_mode);
			return;
		}
		
		i2s_std_clk_config_t clk_cfg = I2S_STD_CLK_DEFAULT_CONFIG(sample_rate);
        i2s_std_slot_config_t slot_cfg = I2S_STD_MSB_SLOT_DEFAULT_CONFIG(I2S_DATA_BIT_WIDTH_16BIT, ch_count);

        ESP_ERROR_CHECK(i2s_channel_reconfig_std_clock(s_i2s_cb.chan0->tx_handler, &clk_cfg));
        ESP_ERROR_CHECK(i2s_channel_reconfig_std_slot(s_i2s_cb.chan0->tx_handler, &slot_cfg));
		ESP_ERROR_CHECK(i2s_channel_reconfig_std_clock(s_i2s_cb.chan1->tx_handler, &clk_cfg));
		ESP_ERROR_CHECK(i2s_channel_reconfig_std_slot(s_i2s_cb.chan1->tx_handler, &slot_cfg));

        ESP_LOGI(I2S_LOG_TAG, "Configure audio player: 0x%x-0x%x-0x%x-0x%x-0x%x-0x%x-0x%x-0x%x-0x%x",
                 mcc->cie.m24_info.drc,
                 mcc->cie.m24_info.obj_type,
                 mcc->cie.m24_info.samp_freq1,
                 mcc->cie.m24_info.samp_freq2,
                 mcc->cie.m24_info.ch,
                 mcc->cie.m24_info.vbr,
                 mcc->cie.m24_info.br1,
                 mcc->cie.m24_info.br2,
                 mcc->cie.m24_info.br3);
        ESP_LOGI(I2S_LOG_TAG, "Audio player configured, sample rate: %d, %d channels", sample_rate, ch_count);
	} else if (mcc->type == ESP_A2D_MCT_SBC) {
		ESP_LOGI(I2S_LOG_TAG, "A2DP audio stream configuration, codec type: SBC (%d)", mcc->type);
		// SBC Codec
		if (mcc->cie.sbc_info.samp_freq & ESP_A2D_SBC_CIE_SF_44K) {
			sample_rate = 44100;
		} else {
			ESP_LOGE(I2S_LOG_TAG, "Unsupported A2DP audio stream sample rate: %d", mcc->cie.sbc_info.samp_freq);
			return;
		}
		
		if (mcc->cie.sbc_info.ch_mode & ESP_A2D_SBC_CIE_CH_MODE_MONO) {
            ch_count = 1;
        } else if (mcc->cie.sbc_info.ch_mode & (ESP_A2D_SBC_CIE_CH_MODE_STEREO |
												ESP_A2D_SBC_CIE_CH_MODE_JOINT_STEREO |
												ESP_A2D_SBC_CIE_CH_MODE_DUAL_CHANNEL)) {
			ch_count = 2;
		} else {
			ESP_LOGE(I2S_LOG_TAG, "Unsupported A2DP audio stream channel mode: %d", mcc->cie.sbc_info.ch_mode);
			return;
		}

        i2s_std_clk_config_t clk_cfg = I2S_STD_CLK_DEFAULT_CONFIG(sample_rate);
        i2s_std_slot_config_t slot_cfg = I2S_STD_MSB_SLOT_DEFAULT_CONFIG(I2S_DATA_BIT_WIDTH_16BIT, ch_count);

        ESP_ERROR_CHECK(i2s_channel_reconfig_std_clock(s_i2s_cb.chan0->tx_handler, &clk_cfg));
        ESP_ERROR_CHECK(i2s_channel_reconfig_std_slot(s_i2s_cb.chan0->tx_handler, &slot_cfg));
		ESP_ERROR_CHECK(i2s_channel_reconfig_std_clock(s_i2s_cb.chan1->tx_handler, &clk_cfg));
		ESP_ERROR_CHECK(i2s_channel_reconfig_std_slot(s_i2s_cb.chan1->tx_handler, &slot_cfg));

        ESP_LOGI(I2S_LOG_TAG, "Configure audio player: 0x%x-0x%x-0x%x-0x%x-0x%x-%d-%d",
                 mcc->cie.sbc_info.samp_freq,
                 mcc->cie.sbc_info.ch_mode,
                 mcc->cie.sbc_info.block_len,
                 mcc->cie.sbc_info.num_subbands,
                 mcc->cie.sbc_info.alloc_mthd,
                 mcc->cie.sbc_info.min_bitpool,
                 mcc->cie.sbc_info.max_bitpool);
		ESP_LOGI(I2S_LOG_TAG, "Audio player configured, sample rate: %d, %d channels", sample_rate, ch_count);
	} else {
		ESP_LOGE(I2S_LOG_TAG, "Unsupported A2DP audio stream configuration, codec type: %d", mcc->type);
		return;
	}
}

size_t output_i2s_data(const uint8_t *data, size_t size) {
	
	if (s_i2s_cb.ring_buf == NULL) {
		return 0;
	}
	
	// We guarantee exact multiples to 4 bytes (1 stereo-pair = 4 bytes)
    size &= ~3; 
    if (size == 0) {
        return 0;
    }
	
	size_t free_size = xRingbufferGetCurFreeSize(s_i2s_cb.ring_buf);
    if (free_size < size) {
        return 0; 
    }
	
	BaseType_t done = xRingbufferSend(s_i2s_cb.ring_buf, (void *)data, size, (TickType_t)0);
	
	return done ? size : 0;
}


