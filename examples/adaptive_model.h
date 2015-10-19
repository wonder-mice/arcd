#pragma once

#include <arcd.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct adaptive_model
{
	unsigned size;
	unsigned short *freq;
}
adaptive_model;

void adaptive_model_create(adaptive_model *const m, const unsigned size);
void adaptive_model_free(adaptive_model *const m);
void adaptive_model_getprob(const arcd_char_t ch, arcd_prob *const prob,
							void *const model);
arcd_char_t adaptive_model_getch(const arcd_range_t v, const arcd_range_t range,
								 arcd_prob *const prob, void *const model);

#ifdef __cplusplus
}
#endif
