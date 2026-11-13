#include <stdio.h>
#include <sys/types.h>
#include "dsp.h"
#include "dsps_fir.h"
#include "esp_dsp.h"
#include "esp_err.h"
#include "esp_log.h"
#include "fir_tables.h"

static const char *DSP_LOG_TAG = "DSP";

#define BLOCK_SAMPLES 512
static float active_lpf_coeffs[FIR_TAPS];
static float fir_delay_lpf_l[FIR_TAPS];
static float fir_delay_lpf_r[FIR_TAPS];
static fir_f32_t fir_lpf_l;
static fir_f32_t fir_lpf_r;
static float in_lpf_l[BLOCK_SAMPLES], out_lpf_l[BLOCK_SAMPLES];
static float in_lpf_r[BLOCK_SAMPLES], out_lpf_r[BLOCK_SAMPLES];
static bool fir_lpf_initialized = false;

// value from 0.0 to 1.0
void dsp_adjust_volume(void *data, size_t item_size, float multiplier) {
	int16_t *samples = (int16_t *)data;
    int num_samples = item_size / sizeof(int16_t);
    for (int i = 0; i < num_samples; i++) {
        samples[i] = (int16_t)(samples[i] * 0.60f);
    }
}

void dsp_init_fir_lpf(u_int8_t step) {
	if (!fir_lpf_initialized) {
		memcpy(active_lpf_coeffs, fir_lpf_table[step], sizeof(float) * FIR_TAPS);
		ESP_ERROR_CHECK(dsps_fir_init_f32(&fir_lpf_l, active_lpf_coeffs, fir_delay_lpf_l, FIR_TAPS));
		ESP_ERROR_CHECK(dsps_fir_init_f32(&fir_lpf_r, active_lpf_coeffs, fir_delay_lpf_r, FIR_TAPS));
		fir_lpf_initialized = true;
		ESP_LOGI(DSP_LOG_TAG, "DSP FIR Low Pass instance initialized.");	
	}
}

void dsp_just_copy(void *data_in, void *data_out, size_t size_bytes) {
	if (data_in == NULL || data_out == NULL || size_bytes == 0) {
		return;
	}
	memcpy(data_out, data_in, size_bytes);
}

void dsp_cross_fir_lpf(void *data_in, void *data_out, size_t size_bytes) {
	if (!fir_lpf_initialized) return;
	if (size_bytes == 0) return;
	
	const int16_t *samples_in = (int16_t *)data_in;
	int16_t *samples_out = (int16_t *)data_out;
	
	// Summary count of stereo-pairs (one channel - 2 byte, one pair - 4 byte)
	size_t total_samples = size_bytes / (sizeof(int16_t) * 2);
	size_t processed_samples = 0;
	
	while (processed_samples < total_samples) {
		size_t chunk_samples = total_samples - processed_samples;
		if (chunk_samples > BLOCK_SAMPLES) {
			chunk_samples = BLOCK_SAMPLES;
		}
		
		// Deinterleaving input data of stereo pairs
		for (size_t i = 0; i < chunk_samples; i++) {
			size_t idx = (processed_samples + i) * 2;
			in_lpf_l[i] = (float)samples_in[idx] / 32768.0f;
			in_lpf_r[i] = (float)samples_in[idx + 1] / 32768.0f;
		}
		
		// Accept filter
		dsps_fir_f32(&fir_lpf_l, in_lpf_l, out_lpf_l, chunk_samples);
		dsps_fir_f32(&fir_lpf_r, in_lpf_r, out_lpf_r, chunk_samples);
		
		// Interleaving to stereo pairs and record to output data
		for (size_t i = 0; i < chunk_samples; i++) {
			float l = out_lpf_l[i] * 32768.0f;
			float r = out_lpf_r[i] * 32768.0f;
			
			// int16 overload protection
			if (l > 32767.0f) {
				l = 32767.0f;
			} else if (l < -32768.0f) {
				l = -32768.0f;
			}

            if (r > 32767.0f) {
				r = 32767.0f;
			} else if (r < -32768.0f) {
				r = -32768.0f;
			}
			
			// Record to output data
			size_t idx = (processed_samples + i) * 2;
			samples_out[idx] = (int16_t)l;
			samples_out[idx + 1] = (int16_t)r;
		}
		
		/*
		ESP_LOGI(DSP_LOG_TAG, "item_size: %d", item_size);
		ESP_LOGI(DSP_LOG_TAG, "total_samples: %d", total_samples);
		ESP_LOGI(DSP_LOG_TAG, "processed_samples: %d", processed_samples);
		ESP_LOGI(DSP_LOG_TAG, "chunk_samples: %d", chunk_samples);
		ESP_LOGI(DSP_LOG_TAG, "-------------------------------");
		*/
		
		processed_samples += chunk_samples;
	}
}


