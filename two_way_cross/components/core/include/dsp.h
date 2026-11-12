#ifndef _DSP_H_
#define _DSP_H_

#include <stdio.h>
#include <sys/types.h>

void dsp_adjust_volume(void *data, size_t item_size, float multiplier);
void dsp_init_fir_lpf(u_int8_t step);
void dsp_cross_fir_lpf(void *data_in, void *data_out, size_t item_size);

#endif
