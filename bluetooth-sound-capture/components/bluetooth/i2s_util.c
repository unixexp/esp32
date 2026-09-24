#include <stdio.h>
#include "driver/i2s_common.h"
#include "esp_err.h"
#include "esp_log.h"
#include "i2s_util.h"
#include "driver/i2s_std.h"
#include "esp_a2dp_api.h"
#include "freertos/FreeRTOS.h"
#include "freertos/ringbuf.h"
#include "freertos/task.h"
#include "freertos/semphr.h"
#include "freertos/ringbuf.h"

static const char *I2S_LOG_TAG = "I2S";

#define I2S_BCLK_PIN    GPIO_NUM_26
#define I2S_WS_PIN      GPIO_NUM_25
#define I2S_DOUT_PIN    GPIO_NUM_22

void open_i2s_channel(void) {
	if (s_i2s_cb.chan_st != CHANNEL_STATUS_IDLE) {
        ESP_LOGW(I2S_LOG_TAG, "Service already open, skipping initialization");
        return;
    }
	i2s_chan_config_t chan_cfg = I2S_CHANNEL_DEFAULT_CONFIG(I2S_NUM_0, I2S_ROLE_MASTER);
    chan_cfg.auto_clear = true;
    i2s_std_config_t std_cfg = {
        .clk_cfg = I2S_STD_CLK_DEFAULT_CONFIG(44100),
        .slot_cfg = I2S_STD_MSB_SLOT_DEFAULT_CONFIG(I2S_DATA_BIT_WIDTH_16BIT, I2S_SLOT_MODE_STEREO),
        .gpio_cfg = {
            .mclk = I2S_GPIO_UNUSED,
            .bclk = GPIO_NUM_26,
            .ws = GPIO_NUM_25,
            .dout = GPIO_NUM_22,
            .din = I2S_GPIO_UNUSED,
            .invert_flags = {
                .mclk_inv = false,
                .bclk_inv = false,
                .ws_inv = false,
            },
        },
    };
    /* initialize I2S channel */
    ESP_ERROR_CHECK(i2s_new_channel(&chan_cfg, &s_i2s_cb.tx_chan, NULL));
    ESP_ERROR_CHECK(i2s_channel_init_std_mode(s_i2s_cb.tx_chan, &std_cfg));
    s_i2s_cb.chan_st = CHANNEL_STATUS_OPENED;
	ESP_LOGI(I2S_LOG_TAG, "state: opened");
}

void close_i2s_channel(void) {
	stop_i2s_channel();
	
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
	
	if (s_i2s_cb.chan_st == CHANNEL_STATUS_OPENED) {
		ESP_ERROR_CHECK(i2s_del_channel(s_i2s_cb.tx_chan));
		s_i2s_cb.chan_st = CHANNEL_STATUS_IDLE;
	}
	memset(&s_i2s_cb, 0, sizeof(s_i2s_cb));
	ESP_LOGI(I2S_LOG_TAG, "state: closed");
}

void start_i2s_channel(void) {
	if (s_i2s_cb.chan_st != CHANNEL_STATUS_OPENED) {
		ESP_LOGE(I2S_LOG_TAG, "%s TX channel wrong state: %d", __func__, s_i2s_cb.chan_st);
		return;
	}
	
	ESP_ERROR_CHECK(i2s_channel_enable(s_i2s_cb.tx_chan));
	ESP_LOGI(I2S_LOG_TAG, "state: enabled");
	
	ESP_LOGI(I2S_LOG_TAG, "ringbuffer data empty! mode changed: RINGBUFFER_MODE_PREFETCHING");
    s_i2s_cb.ring_buf_mode = RINGBUFFER_MODE_PREFETCHING;
    if ((s_i2s_cb.write_semaphore == NULL) && (s_i2s_cb.write_semaphore = xSemaphoreCreateBinary()) == NULL) {
        ESP_LOGE(I2S_LOG_TAG, "%s, Semaphore create failed", __func__);
        
		vSemaphoreDelete(s_i2s_cb.write_semaphore);
		s_i2s_cb.write_semaphore = NULL;
    }
    if ((s_i2s_cb.ring_buf == NULL) && (s_i2s_cb.ring_buf = xRingbufferCreate(RINGBUF_HIGHEST_WATER_LEVEL, RINGBUF_TYPE_BYTEBUF)) == NULL) {
        ESP_LOGE(I2S_LOG_TAG, "%s, ringbuffer create failed", __func__);
        
		vRingbufferDelete(s_i2s_cb.ring_buf);
		s_i2s_cb.ring_buf = NULL;
    }
    if (s_i2s_cb.write_task_handle == NULL) {
        if (xTaskCreate(i2s_task_handler, "BtI2STask", 4 * 1024, NULL,
                        configMAX_PRIORITIES - 3, &s_i2s_cb.write_task_handle) != pdPASS) {
            ESP_LOGE(I2S_LOG_TAG, "%s, Task create failed", __func__);
            
			vTaskDelete(s_i2s_cb.write_task_handle);
			s_i2s_cb.write_task_handle = NULL;
        }
    }
	
	s_i2s_cb.chan_st = CHANNEL_STATUS_ENABLED;
}

void stop_i2s_channel(void) {
	if (s_i2s_cb.chan_st == CHANNEL_STATUS_ENABLED) {
		ESP_ERROR_CHECK(i2s_channel_disable(s_i2s_cb.tx_chan));
		s_i2s_cb.chan_st = CHANNEL_STATUS_OPENED;
		ESP_LOGI(I2S_LOG_TAG, "state: opened");
	}
}

void update_i2s_channel_config(esp_a2d_mcc_t *mcc) {	
	int sample_rate = 0;
	int ch_count = 0;
	
	if (mcc->type == ESP_A2D_MCT_SBC) {
		ESP_LOGI(I2S_LOG_TAG, "A2DP audio stream configuration, codec type: SBC (%d)", mcc->type);
		// SBC Codec
		if (mcc->cie.sbc_info.samp_freq & ESP_A2D_SBC_CIE_SF_16K) {
			sample_rate = 16000;
		} else if (mcc->cie.sbc_info.samp_freq & ESP_A2D_SBC_CIE_SF_32K) {
			sample_rate = 32000;
		} else if (mcc->cie.sbc_info.samp_freq & ESP_A2D_SBC_CIE_SF_44K) {
			sample_rate = 44100;
		} else if (mcc->cie.sbc_info.samp_freq & ESP_A2D_SBC_CIE_SF_48K) {
			sample_rate = 48000;
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
        ESP_ERROR_CHECK(i2s_channel_reconfig_std_clock(s_i2s_cb.tx_chan, &clk_cfg));
        ESP_ERROR_CHECK(i2s_channel_reconfig_std_slot(s_i2s_cb.tx_chan, &slot_cfg));
        ESP_LOGI(I2S_LOG_TAG, "Configure audio player: 0x%x-0x%x-0x%x-0x%x-0x%x-%d-%d",
                 mcc->cie.sbc_info.samp_freq,
                 mcc->cie.sbc_info.ch_mode,
                 mcc->cie.sbc_info.block_len,
                 mcc->cie.sbc_info.num_subbands,
                 mcc->cie.sbc_info.alloc_mthd,
                 mcc->cie.sbc_info.min_bitpool,
                 mcc->cie.sbc_info.max_bitpool);
		ESP_LOGI(I2S_LOG_TAG, "Audio player configured, sample rate: %d, %d channels", sample_rate, ch_count);
	} else if (mcc->type == ESP_A2D_MCT_M24) {
		ESP_LOGI(I2S_LOG_TAG, "A2DP audio stream configuration, codec type: AAC (%d)", mcc->type);
		// AAC Codec
		if (mcc->cie.m24_info.samp_freq1 & ESP_A2D_M24_CIE_SF1_8K) {
			sample_rate = 8000;
		} else if (mcc->cie.m24_info.samp_freq1 & ESP_A2D_M24_CIE_SF1_11K) {
			sample_rate = 11025;
		} else if (mcc->cie.m24_info.samp_freq1 & ESP_A2D_M24_CIE_SF1_12K) {
			sample_rate = 12000;
		} else if (mcc->cie.m24_info.samp_freq1 & ESP_A2D_M24_CIE_SF1_16K) {
			sample_rate = 16000;
		} else if (mcc->cie.m24_info.samp_freq1 & ESP_A2D_M24_CIE_SF1_22K) {
			sample_rate = 22050;
		} else if (mcc->cie.m24_info.samp_freq1 & ESP_A2D_M24_CIE_SF1_24K) {
			sample_rate = 24000;
		} else if (mcc->cie.m24_info.samp_freq1 & ESP_A2D_M24_CIE_SF1_32K) {
			sample_rate = 32000;
		} else if (mcc->cie.m24_info.samp_freq1 & ESP_A2D_M24_CIE_SF1_44K) {
			sample_rate = 44100;
		} else if (mcc->cie.m24_info.samp_freq1 & ESP_A2D_M24_CIE_SF2_48K) {
			sample_rate = 48000;
		} else if (mcc->cie.m24_info.samp_freq1 & ESP_A2D_M24_CIE_SF2_64K) {
			sample_rate = 64000;
		} else if (mcc->cie.m24_info.samp_freq1 & ESP_A2D_M24_CIE_SF2_88K) {
			sample_rate = 88200;
		} else if (mcc->cie.m24_info.samp_freq1 & ESP_A2D_M24_CIE_SF2_96K) {
			sample_rate = 96000;
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
        ESP_ERROR_CHECK(i2s_channel_reconfig_std_clock(s_i2s_cb.tx_chan, &clk_cfg));
        ESP_ERROR_CHECK(i2s_channel_reconfig_std_slot(s_i2s_cb.tx_chan, &slot_cfg));
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
	} else {
		ESP_LOGE(I2S_LOG_TAG, "Unsupported A2DP audio stream configuration, codec type: %d", mcc->type);
		return;
	}
}

static void i2s_task_handler(void *args) {
	
}

size_t i2s_data_output(const uint8_t *data, size_t size) {
	return 0;
}

