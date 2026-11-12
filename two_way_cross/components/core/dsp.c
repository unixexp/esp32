#include <stdio.h>
#include "dsp.h"
#include "esp_dsp.h"
#include "esp_err.h"
#include "fir_tables.h"

static const char *DSP_LOG_TAG = "DSP";

#define BLOCK_SAMPLES 512
static float fir_coeffs_lpf[FIR_TAPS];
static float fir_delay_lpf_l[FIR_TAPS];
static float fir_delat_lpf_r[FIR_TAPS];
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

static void dsp_init_fir_lpf(void) {
	ESP_ERROR_CHECK(dsps_fir_init_f32(&fir_lpf_l, fir_coeffs_lpf, fir_delay_lpf_l, FIR_TAPS));
	ESP_ERROR_CHECK(dsps_fir_init_f32(&fir_lpf_r, fir_coeffs_lpf, fir_delat_lpf_r, FIR_TAPS));
	fir_lpf_initialized = true;
	ESP_LOGI(DSP_LOG_TAG, "DSP Fir Low pass instances initialized.");
}

void dsp_cross_fir_lpf(void *data_in, void *data_out, size_t item_size, int current_step) {
	if (item_size == 0) return;
	
	const int16_t *samples_in = (int16_t *)data_in;
	int16_t *samples_out = (int16_t *)data_out;
	
	size_t total_samples = item_size / sizeof(int16_t);
	size_t processed_samples = 0;
	
	while (processed_samples < total_samples) {
		size_t chunk_samples = total_samples - processed_samples;
		if (chunk_samples > BLOCK_SAMPLES) {
			chunk_samples = BLOCK_SAMPLES;
		}
		
		/*
		ESP_LOGI(DSP_LOG_TAG, "item_size: %d", item_size);
		ESP_LOGI(DSP_LOG_TAG, "total_samples: %d", total_samples);
		ESP_LOGI(DSP_LOG_TAG, "processed_samples: %d", processed_samples);
		ESP_LOGI(DSP_LOG_TAG, "chunk_samples: %d", chunk_samples);
		ESP_LOGI(DSP_LOG_TAG, "-------------------------------");
		
		Ex:
		I (77218) DSP: item_size: 4096
		I (77218) DSP: total_samples: 2048
		I (77218) DSP: processed_samples: 0
		I (77228) DSP: chunk_samples: 512 (> 2048)
		*/
		
		processed_samples += chunk_samples;
	}
	
	// if (!fir_lpf_initialized) {
	//	dsp_init_fir_lpf();
	//}
}


