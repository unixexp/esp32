#ifndef _DSP_H_
#define _DSP_H_

#include <stdio.h>
#include <sys/types.h>
#include "dsps_fir.h"

#define MAX_SAMPLES 1024

void dsp_pack_dac_stereo(const float *data_in_l, const float *data_in_r, int16_t *data_out);
void dsp_unpack_dac_stereo(int16_t *data_in, float *data_out_l, float *data_out_r, size_t len_bytes);

void dsp_mono_downmix(int16_t *data);
size_t dsp_adjust_volume_stereo(int16_t *data, size_t len_bytes, float multiplier);
size_t dsp_just_copy_stereo(int16_t *data_in, int16_t *data_out, size_t len_bytes);

int dsp_init_fir(fir_f32_t *fir_h, float *fir_delay, float *active_coeffs, const float *coeffs, size_t taps);
void dsp_process_fir(fir_f32_t *fir_h, const float *data_in, float *data_out);

void dsp_init_biquad(double fs, double f0, double q, double gain_db, float *coeffs, float *biquad_delay);
void dsp_process_biquad(const float *data_in, float *data_out, float *biquad_delay, float *coeffs);

#endif
