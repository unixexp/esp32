#include <stdio.h>
#include <math.h>
#include <sys/types.h>
#include "dsp.h"
#include "dsps_biquad.h"
#include "dsps_biquad_platform.h"
#include "esp_err.h"
#include "esp_log.h"
#include "dsps_fir.h"

static const char *DSP_LOG_TAG = "DSP";

static float in_peak_l[512], out_peak_l[512];
static float in_peak_r[512], out_peak_r[512];
static float peak_coeffs[5];
static float peak_delay_line_l[2] = {0.0f, 0.0f};
static float peak_delay_line_r[2] = {0.0f, 0.0f};

// Pack channels to DAC supported format

size_t dsp_pack_dac_stereo(const float *data_in_l, const float *data_in_r, int16_t *data_out, size_t len_bytes) {
	if (data_in_l == NULL || data_in_r == NULL || data_out == NULL || len_bytes < sizeof(int16_t) * 2) {
		return 0;
	} else {
		size_t samples = len_bytes / (sizeof(int16_t) * 2);
		if (samples > MAX_SAMPLES) {
			samples = MAX_SAMPLES;
		}
			
		for (size_t i = 0; i < samples; i++) {
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
			data_out[i * 2] = (int16_t)l;
			data_out[i * 2 + 1] = (int16_t)r;
		}
		
		return samples;
	}
}

// Unpack DAC supported format to separated float channels
// data_l_out and data_r_out should be initialized like:
// static float data_out_l[MAX_SAMPLES], data_out_r[MAX_SAMPLES]

size_t dsp_unpack_dac_stereo(int16_t *data_in, float *data_out_l, float *data_out_r, size_t len_bytes) {
	if (data_in == NULL || data_out_l == NULL || data_out_r == NULL || len_bytes < sizeof(int16_t) * 2) {
        return 0;
	} else {
		const float scale = 1.0f / 32768.0f;
		size_t samples = len_bytes / (sizeof(int16_t) * 2);
		if (samples > MAX_SAMPLES) {
			samples = MAX_SAMPLES;
		}
			
		for (size_t i = 0; i < samples; i++) {
			data_out_l[i] = (float)data_in[i * 2] * scale;
			data_out_r[i] = (float)data_in[i * 2 + 1] * scale;
		}
		
		return samples;
	}
}

int dsp_init_fir_ch(fir_f32_t *fir_h, float *fir_delay, uint8_t idx, const float *fir_table, int taps) {
	size_t size = sizeof(fir_table) / sizeof(fir_table[0]);
	if (size == 0) {
		ESP_LOGE(DSP_LOG_TAG, "DSP FIR High Pass instance not initialized. No coefficients found.");
		return 1;
	} else if ((size_t)idx >= size) {
		ESP_LOGE(DSP_LOG_TAG, "DSP FIR High Pass instance not initialized. Coefficient index outbound.");
		return 2;
	} else {
		memset(fir_delay, 0, sizeof(float) * taps);
		memcpy(active_hpf_coeffs, fir_table[idx], sizeof(float) * taps);
		ESP_ERROR_CHECK(dsps_fir_init_f32(fir_h, active_hpf_coeffs, fir_delay, taps));
		ESP_LOGI(DSP_LOG_TAG, "DSP FIR High Pass instance initialized.");
		return 0;
	}
}

void dsp_cross_fir_hpf_ch(fir_f32_t *fir_h, const float *data_in_ch, float *data_out_ch, size_t len_bytes) {
	if (fir_h == NULL || data_in_ch == NULL || data_out_ch == NULL || len_bytes < sizeof(int16_t) * 2) {
		return;
	} else {
		dsps_fir_f32(fir_h, data_in_ch, data_out_ch, len_bytes);	
	}
}

size_t dsp_mono_downmix(int16_t *data, size_t len_bytes) {
	if (data == NULL || len_bytes < sizeof(int16_t) * 2) {
		return 0;
	} else {
		size_t samples = len_bytes / (sizeof(int16_t) * 2);
		if (samples > MAX_SAMPLES) {
			samples = MAX_SAMPLES;
		}
		
		size_t total_int16 = samples * 2; // L and R

		for (size_t i = 0; i < total_int16; i+=2) {
			int32_t l = data[i];
			int32_t r = data[i + 1];
			int16_t mono_sample = (int16_t)((l + r) / 2);
			data[i] = mono_sample;
			data[i + 1] = mono_sample;
		}
		
		return samples;
	}
}

size_t dsp_adjust_volume_stereo(int16_t *data, size_t len_bytes, float multiplier) {
	if (data == NULL || len_bytes < sizeof(int16_t) * 2) {
        return 0;
	} else {
		size_t samples = len_bytes / (sizeof(int16_t) * 2);
		if (samples > MAX_SAMPLES) {
			samples = MAX_SAMPLES;
		}
		
		size_t total_int16 = samples * 2; // L and R
		
		for (size_t i = 0; i < total_int16; i++) {
			float val = (float)data[i] * multiplier;
			
			if (val > 32767.0f) {
				val = 32767.0f;
			} else if (val < -32768.0f) {
				val = -32768.0f;
			}
			
			data[i] = (int16_t)val;
		}
		
		return samples;
	}
}

size_t dsp_just_copy_stereo(int16_t *data_in, int16_t *data_out, size_t len_bytes) {
	if (data_in == NULL || data_out == NULL || len_bytes < sizeof(int16_t) * 2) {
		return 0;
	}
	size_t stereo_frame_bytes = sizeof(int16_t) * 2;
	size_t copy_bytes = (len_bytes / stereo_frame_bytes);
	if (copy_bytes <= 0) {
		return 0;
	} else {
		memcpy(data_out, data_in, copy_bytes);
		return copy_bytes;
	}
}

/*
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

*/

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
