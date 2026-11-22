#ifndef _DSP_H_
#define _DSP_H_

#include <stdio.h>
#include <sys/types.h>

void dsp_pack_dac_stereo(const float *data_in_l, const float *data_in_r, int16_t *data_out, size_t len, size_t max_chunk);
void dsp_unpack_dac_stereo(int16_t *data_in, float *data_out_l, float *data_out_r, size_t len, size_t max_chunk);

void dsp_init_fir_lpf(u_int8_t step);
void dsp_init_fir_hpf(u_int8_t step);
/*
void dsp_adjust_volume(void *data, size_t item_size, float multiplier);
void dsp_cross_fir_lpf(void *data_in, void *data_out, size_t item_size);
void dsp_cross_fir_hpf(void *data_in, void *data_out, size_t item_size);
void dsp_just_copy(void *data_in, void *data_out, size_t item_size);
void dsp_mono_downmix(void *data, size_t item_size);
void peak_filter(void *data_in, void *data_out, size_t size_bytes);
*/

#endif
