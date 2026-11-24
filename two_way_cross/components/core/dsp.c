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

// Pack channels to DAC supported format

void dsp_pack_dac_stereo(const float *data_in_l, const float *data_in_r, int16_t *data_out) {
	if (data_in_l == NULL || data_in_r == NULL || data_out == NULL) {
		return;
	} else {
		for (size_t i = 0; i < MAX_SAMPLES; i++) {
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
	}
}

// Unpack DAC supported format to separated float channels
// data_l_out and data_r_out should be initialized like:
// static float data_out_l[MAX_SAMPLES], data_out_r[MAX_SAMPLES]

void dsp_unpack_dac_stereo(int16_t *data_in, float *data_out_l, float *data_out_r, size_t len_bytes) {
	if (data_in == NULL || data_out_l == NULL || data_out_r == NULL || len_bytes < sizeof(int16_t) * 2) {
        return;
	} else {
		const float scale = 1.0f / 32768.0f;
		for (size_t i = 0; i < MAX_SAMPLES; i++) {
			data_out_l[i] = (float)data_in[i * 2] * scale;
			data_out_r[i] = (float)data_in[i * 2 + 1] * scale;
		}
	}
}

int dsp_init_fir(fir_f32_t *fir_h, float *fir_delay, float *active_coeffs, const float *coeffs, size_t taps) {
	if (fir_h == NULL || fir_delay == NULL || active_coeffs == NULL || coeffs == NULL || taps == 0) {
		ESP_LOGE(DSP_LOG_TAG, "DSP FIR instance not initialized. Invalid params.");
		return 1;
	} else {
		memset(fir_delay, 0, sizeof(float) * taps);
		memcpy(active_coeffs, coeffs, sizeof(float) * taps);
		esp_err_t err = dsps_fir_init_f32(fir_h, active_coeffs, fir_delay, taps);
		if (err != ESP_OK) {
			ESP_LOGE(DSP_LOG_TAG, "dsps_fir_init_f32 failed: %s", esp_err_to_name(err));
			return 2;
		} else {
			ESP_LOGI(DSP_LOG_TAG, "DSP FIR instance initialized.");
		    return 0;	
		}
	}
}

void dsp_process_fir(fir_f32_t *fir_h, const float *data_in, float *data_out) {
	if (fir_h == NULL || data_in == NULL || data_out == NULL) {
		return;
	} else {		
		dsps_fir_f32(fir_h, data_in, data_out, MAX_SAMPLES);	
	}
}

void dsp_mono_downmix(int16_t *data) {
	if (data == NULL) {
		return;
	} else {
		size_t total_int16 = MAX_SAMPLES * 2; // L and R
		for (size_t i = 0; i < total_int16; i+=2) {
			int32_t l = data[i];
			int32_t r = data[i + 1];
			int16_t mono_sample = (int16_t)((l + r) / 2);
			data[i] = mono_sample;
			data[i + 1] = mono_sample;
		}
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

void dsp_init_biquad(double fs, double f0, double q, double gain_db, float *coeffs, float *biquad_delay) {
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

    coeffs[0] = (float)(b0 / a0);
    coeffs[1] = (float)(b1 / a0);
    coeffs[2] = (float)(b2 / a0);
    coeffs[3] = (float)(a1 / a0);
    coeffs[4] = (float)(a2 / a0);

    biquad_delay[0] = 0.0f; biquad_delay[1] = 0.0f;
}


void dsp_process_biquad(const float *data_in, float *data_out, float *biquad_delay, float *coeffs) {
	if (data_in == NULL || data_out == NULL || biquad_delay == NULL || coeffs == NULL) {
		return;
	} else {
		dsps_biquad_f32(data_in, data_out, MAX_SAMPLES, coeffs, biquad_delay);
	}
}

