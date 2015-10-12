#pragma once

#ifndef _ARCD_H_
#define _ADCD_H_

/* To detect incompatible changes you can define ARCD_VERSION_REQUIRED to
 * the current value of ARCD_VERSION before including this file (or via
 * compiler command line):
 *
 *   #define ARCD_VERSION_REQUIRED 1
 *   #include <arcd.h>
 *
 * In that case compilation will fail when included file has incompatible
 * version.
 */
#define ARCD_VERSION 1
#if defined(ARCD_VERSION_REQUIRED)
	#if ARCD_VERSION_REQUIRED != ARCD_VERSION
		#error different arcd version required
	#endif
#endif

#ifdef __cplusplus
extern "C" {
#endif

/* Type for alphabet symbol. */
typedef unsigned arcd_char_t;
typedef unsigned arcd_range_t;
typedef unsigned char arcd_buf_t;

typedef struct arcd_prob
{
	arcd_range_t lower;
	arcd_range_t upper;
	arcd_range_t range;
}
arcd_prob;

typedef struct arcd_state
{
	arcd_range_t lower;
	arcd_range_t upper;
	arcd_range_t range;
	arcd_buf_t buf;
	unsigned buf_bits;
	void *model;
	void *io;
}
arcd_state;

typedef struct arcd_enc
{
	arcd_state state;
	unsigned pending;
	void (*getprob)(arcd_char_t ch, arcd_prob *prob, void *model);
	void (*output)(arcd_buf_t buf, unsigned buf_bits, void *io);
}
arcd_enc;

typedef struct arcd_dec
{
	arcd_state state;
	arcd_range_t v;
	unsigned v_bits;
	arcd_char_t (*getch)(arcd_range_t v, arcd_range_t range, arcd_prob *prob,
						 void *model);
	unsigned (*input)(arcd_buf_t *buf, void *io);
}
arcd_dec;

void arcd_enc_init(arcd_enc *const e, void *const model, void *const io);
void arcd_enc_put(arcd_enc *const e, const arcd_char_t ch);
void arcd_enc_fin(arcd_enc *const e);

void arcd_dec_init(arcd_dec *const d, void *const model, void *const io);
arcd_char_t arcd_dec_get(arcd_dec *const d);

#ifdef __cplusplus
}
#endif

#endif
