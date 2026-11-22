#include <stdio.h>
#include <math.h>
#include <sys/types.h>
#include "dsp.h"
#include "dsps_biquad.h"
#include "dsps_biquad_platform.h"
#include "dsps_fir.h"
#include "esp_dsp.h"
#include "esp_err.h"
#include "esp_log.h"
#include "fir_tables.h"

static const char *DSP_LOG_TAG = "DSP";

static float active_lpf_coeffs[FIR_TAPS];
static float fir_delay_lpf_l[FIR_TAPS];
static float fir_delay_lpf_r[FIR_TAPS];
static fir_f32_t fir_lpf_l;
static fir_f32_t fir_lpf_r;
static float in_lpf_l[BLOCK_SAMPLES], out_lpf_l[BLOCK_SAMPLES];
static float in_lpf_r[BLOCK_SAMPLES], out_lpf_r[BLOCK_SAMPLES];
static bool fir_lpf_initialized = false;

static float active_hpf_coeffs[FIR_TAPS];
static float fir_delay_hpf_l[FIR_TAPS];
static float fir_delay_hpf_r[FIR_TAPS];
static fir_f32_t fir_hpf_l;
static fir_f32_t fir_hpf_r;
static float in_hpf_l[BLOCK_SAMPLES], out_hpf_l[BLOCK_SAMPLES];
static float in_hpf_r[BLOCK_SAMPLES], out_hpf_r[BLOCK_SAMPLES];
static bool fir_hpf_initialized = false;

static float in_peak_l[BLOCK_SAMPLES], out_peak_l[BLOCK_SAMPLES];
static float in_peak_r[BLOCK_SAMPLES], out_peak_r[BLOCK_SAMPLES];
static float peak_coeffs[5];
static float peak_delay_line_l[2] = {0.0f, 0.0f};
static float peak_delay_line_r[2] = {0.0f, 0.0f};

// Pack channels to DAC supported format

void dsp_pack_dac_stereo(const float *data_in_l, const float *data_in_r, int16_t *data_out, size_t len, size_t max_chunk) {
	if (data_in_l == NULL || data_in_r == NULL || data_out == NULL ||
		len < sizeof(int16_t) * 2 || max_chunk == 0) {
		return;
	} else {
		size_t total_samples = len / (sizeof(int16_t) * 2);
		size_t processed_samples = 0;
			
		while (processed_samples < total_samples) {
			size_t chunk_samples = total_samples - processed_samples;
			if (chunk_samples > max_chunk) {
				chunk_samples = max_chunk;
			}
			
			for (size_t i = 0; i < chunk_samples; i++) {
				float l = data_in_l[i] * 32768.0f;
				float r = data_in_r[i] * 32768.0f;
				
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
				data_out[idx] = (int16_t)l;
				data_out[idx + 1] = (int16_t)r;
			}
			
			processed_samples += chunk_samples;
		}
	}
}

// Unpack DAC supported format to separated float channels
// data_l_out and data_r_out should be initialized with max chunk size:
// #define MAX_CHUNK 512
// static float data_out_l[MAX_CHUNK], data_out_r[MAX_CHUNK]

void dsp_unpack_dac_stereo(int16_t *data_in, float *data_out_l, float *data_out_r, size_t len, size_t max_chunk) {
	if (data_in == NULL || data_out_l == NULL || data_out_r == NULL ||
		len < sizeof(int16_t) * 2 || max_chunk == 0) {
        return;
	} else {
		const float scale = 1.0f / 32768.0f;
		size_t total_samples = len / (sizeof(int16_t) * 2);
		size_t processed_samples = 0;
			
		while (processed_samples < total_samples) {
			size_t chunk_samples = total_samples - processed_samples;
			if (chunk_samples > max_chunk) {
				chunk_samples = max_chunk;
			}
			
			for (size_t i = 0; i < chunk_samples; i++) {
				size_t idx = (processed_samples + i) * 2;
				data_out_l[i] = (float)data_in[idx] * scale;
				data_out_r[i] = (float)data_in[idx + 1] * scale;
			}
			
			processed_samples += chunk_samples;
		}
	}
}

void dsp_init_fir_lpf(u_int8_t idx) {
	size_t size = sizeof(fir_lpf_table) / sizeof(fir_lpf_table[0]);
	if (size == 0) {
		ESP_LOGE(DSP_LOG_TAG, "DSP FIR Low Pass instance not initialized. Any coefficients found.");
	} else if ((size_t)idx >= size) {
		ESP_LOGE(DSP_LOG_TAG, "DSP FIR Low Pass instance not initialized. Coefficient index outbound.");
	} else if (!fir_lpf_initialized) {
		memcpy(active_lpf_coeffs, fir_lpf_table[idx], sizeof(float) * FIR_TAPS);
		ESP_ERROR_CHECK(dsps_fir_init_f32(&fir_lpf_l, active_lpf_coeffs, fir_delay_lpf_l, FIR_TAPS));
		ESP_ERROR_CHECK(dsps_fir_init_f32(&fir_lpf_r, active_lpf_coeffs, fir_delay_lpf_r, FIR_TAPS));
		fir_lpf_initialized = true;
		ESP_LOGI(DSP_LOG_TAG, "DSP FIR Low Pass instance initialized.");
	}
}

void dsp_init_fir_hpf(u_int8_t idx) {
	size_t size = sizeof(fir_hpf_table) / sizeof(fir_hpf_table[0]);
	if (size == 0) {
		ESP_LOGE(DSP_LOG_TAG, "DSP FIR High Pass instance not initialized. Any coefficients found.");
	} else if ((size_t)idx >= size) {
		ESP_LOGE(DSP_LOG_TAG, "DSP FIR High Pass instance not initialized. Coefficient index outbound.");
	} else if (!fir_hpf_initialized) {
		memcpy(active_hpf_coeffs, fir_hpf_table[idx], sizeof(float) * FIR_TAPS);
		ESP_ERROR_CHECK(dsps_fir_init_f32(&fir_hpf_l, active_hpf_coeffs, fir_delay_hpf_l, FIR_TAPS));
		ESP_ERROR_CHECK(dsps_fir_init_f32(&fir_hpf_r, active_hpf_coeffs, fir_delay_hpf_r, FIR_TAPS));
		fir_hpf_initialized = true;
		ESP_LOGI(DSP_LOG_TAG, "DSP FIR High Pass instance initialized.");
	}
}

/*
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
		
		ESP_LOGI(DSP_LOG_TAG, "item_size: %d", item_size);
		ESP_LOGI(DSP_LOG_TAG, "total_samples: %d", total_samples);
		ESP_LOGI(DSP_LOG_TAG, "processed_samples: %d", processed_samples);
		ESP_LOGI(DSP_LOG_TAG, "chunk_samples: %d", chunk_samples);
		ESP_LOGI(DSP_LOG_TAG, "-------------------------------");
		
		processed_samples += chunk_samples;
	}
}

void dsp_cross_fir_hpf(void *data_in, void *data_out, size_t size_bytes) {
	if (!fir_hpf_initialized) return;
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
			in_hpf_l[i] = (float)samples_in[idx] / 32768.0f;
			in_hpf_r[i] = (float)samples_in[idx + 1] / 32768.0f;
		}
		
		// Accept filter
		dsps_fir_f32(&fir_hpf_l, in_hpf_l, out_hpf_l, chunk_samples);
		dsps_fir_f32(&fir_hpf_r, in_hpf_r, out_hpf_r, chunk_samples);
		
		// Interleaving to stereo pairs and record to output data
		for (size_t i = 0; i < chunk_samples; i++) {
			float l = out_hpf_l[i] * 32768.0f;
			float r = out_hpf_r[i] * 32768.0f;
			
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
		
		processed_samples += chunk_samples;
	}
}

void peak_filter_init_coeffs(double fs, double f0, double q, double gain_db) {
    double A = pow(10.0, gain_db / 40.0);
    double w0 = 2.0 * M_PI * f0 / fs;
    double sin_w0 = sin(w0);
    double cos_w0 = cos(w0);
    double alpha = sin_w0 / (2.0 * q);

    double b0 = 1.0 + alpha * A;
    double b1 = -2.0 * cos_w0;
    double b2 = 1.0 - alpha * A;
    double a0 = 1.0 + alpha / A;
    double a1 = -2.0 * cos_w0;
    double a2 = 1.0 - alpha / A;

    peak_coeffs[0] = (float)(b0 / a0);
    peak_coeffs[1] = (float)(b1 / a0);
    peak_coeffs[2] = (float)(b2 / a0);
    peak_coeffs[3] = (float)(a1 / a0);
    peak_coeffs[4] = (float)(a2 / a0);

    peak_delay_line_l[0] = 0.0f; peak_delay_line_l[1] = 0.0f;
    peak_delay_line_r[0] = 0.0f; peak_delay_line_r[1] = 0.0f;
}

void peak_filter(void *data_in, void *data_out, size_t size_bytes) {
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
			in_peak_l[i] = (float)samples_in[idx] / 32768.0f;
			in_peak_r[i] = (float)samples_in[idx + 1] / 32768.0f;
		}
		
		// Accept filter
		dsps_biquad_f32(in_peak_l, out_peak_l, chunk_samples, peak_coeffs, peak_delay_line_l);
		dsps_biquad_f32(in_peak_r, out_peak_r, chunk_samples, peak_coeffs, peak_delay_line_r);
		
		// Interleaving to stereo pairs and record to output data
		for (size_t i = 0; i < chunk_samples; i++) {
			float l = out_peak_l[i] * 32768.0f;
			float r = out_peak_r[i] * 32768.0f;
			
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
		
		processed_samples += chunk_samples;
	}
}

void dsp_adjust_volume(void *data, size_t item_size, float multiplier) {
	int16_t *samples = (int16_t *)data;
    int num_samples = item_size / sizeof(int16_t);
    for (int i = 0; i < num_samples; i++) {
        samples[i] = (int16_t)(samples[i] * multiplier);
    }
}

void dsp_mono_downmix(void *data, size_t item_size) {
	int16_t *samples = (int16_t *)data;
    int num_samples = item_size / sizeof(int16_t);
    for (int i = 0; i < num_samples; i+=2) {
		int32_t left = samples[i];
		int32_t right = samples[i+1];
		int16_t mono_sample = (int16_t)((left + right) / 2);
        samples[i] = mono_sample;
		samples[i+1] = mono_sample;
    }
}

*/
