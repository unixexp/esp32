#ifndef _DSP_H_
#define _DSP_H_

#include <stdio.h>
#include <sys/types.h>
#include "dsps_fir.h"

#define MAX_SAMPLES 1024

size_t dsp_pack_dac_stereo(const float *data_in_l, const float *data_in_r, int16_t *data_out, size_t len_bytes);
size_t dsp_unpack_dac_stereo(int16_t *data_in, float *data_out_l, float *data_out_r, size_t len_bytes);

size_t dsp_mono_downmix(int16_t *data, size_t len_bytes);
size_t dsp_adjust_volume_stereo(int16_t *data, size_t len_bytes, float multiplier);
size_t dsp_just_copy_stereo(int16_t *data_in, int16_t *data_out, size_t len_bytes);

int dsp_init_fir(fir_f32_t *fir_h, float *fir_delay, float *coeffs, size_t taps);
void dsp_process_fir(fir_f32_t *fir_h, const float *data_in, float *data_out, size_t len_bytes);

void peak_filter_init_coeffs(double fs, double f0, double q, double gain_db);

/*
void dsp_cross_fir_lpf(void *data_in, void *data_out, size_t item_size);
void dsp_cross_fir_hpf(void *data_in, void *data_out, size_t item_size);
void peak_filter(void *data_in, void *data_out, size_t size_bytes);
*/

#endif
