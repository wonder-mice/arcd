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

/* This will work fine when N is strictly less than number of bits in underlying
 * type. However N=32 will result in undefined behavior for 32bit int, since
 * shifting past the type size is illegal. Update this if it could be a problem.
 */
#define _ARCD_2_POW_N_MINUS_1(N) ((1 << (N)) - 1)

/* Number of bits used to represent frequency values. This values is exact -
 * maximum frequency values is 2^ARCD_FREQ_BITS - 1.
 */
enum { ARCD_FREQ_BITS = 15 };
/* Number of bits used to represent arithmetic coder intervals. This value is
 * NOT entirely
 */
enum { ARCD_RANGE_BITS = 17 };
/* Maximum frequency value. */
enum { ARCD_FREQ_MAX = _ARCD_2_POW_N_MINUS_1(ARCD_FREQ_BITS) };
/* Maximum interval value. */
enum { ARCD_RANGE_MAX = _ARCD_2_POW_N_MINUS_1(ARCD_RANGE_BITS)  + 1};

/* Alphabet symbol. Library has no particular requirements for this type. Its
 * values are transparantly passed to arcd_enc::getprob() and from
 * arcd_dec::getch() callbacks.
 */
typedef unsigned arcd_char_t;
/* Cumulative frequency of a symbol. Should be large enough to hold
 * ARCD_FREQ_BITS bits. Valid values are in the range [0, ACRD_FREQ_MAX]
 * (inclusive).
 */
typedef unsigned arcd_freq_t;
/* Arithmetic coder interval or value from that interval. Should be large enough
 * to hold ARCD_RANGE_BITS bits. Valid values are in the range
 * [0, ARCD_RANGE_MAX] (inclusive). Intervals themselves are closed from the
 * left and open from the right: [left, right).
 */
typedef unsigned arcd_range_t;
/* Bit buffer. Used by arcd_enc::output() and arcd_dec::input() callbacks. No
 * particular requirements for this type.
 */
typedef unsigned char arcd_buf_t;

/* Cumulative probability of a symbol. Corresponding regular probability is
 * (upper - lower) / total. For example, if alphabet has 4 symbols (a, b, c, d)
 * with corresponding probabilities 2, 3, 2, 1, then cumulative probabilities
 * could be represented as:
 *  8       +-+- d.upper, total
 *  7     +-+ |- c.upper, d.lower
 *  6     | | |
 *  5   +-+ | |- b.upper, c.lower
 *  4   | | | |
 *  3   | | | |
 *  2 +-+ | | |- a.upper, b.lower
 *  1 | | | | |
 *  0 +-+-+-+-+- a.lower
 */
typedef struct arcd_prob
{
	arcd_freq_t lower;
	arcd_freq_t upper;
	arcd_freq_t total;
}
arcd_prob;

/* Internal state shared between encoder and decoder. */
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

typedef void arcd_getprob_f(arcd_char_t ch, arcd_prob *prob, void *model);
typedef void acrd_output_f(arcd_buf_t buf, unsigned buf_bits, void *io);

/* Arithmetic encoder. Must be initialized*/
typedef struct arcd_enc
{
	arcd_state _state;
	unsigned _pending;
	arcd_getprob_f *_getprob;
	acrd_output_f *_output;
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

void arcd_enc_init(arcd_enc *const e,
				   arcd_getprob_f *const getprob, void *const model,
				   acrd_output_f *const output, void *const io);
void arcd_enc_put(arcd_enc *const e, const arcd_char_t ch);
void arcd_enc_fin(arcd_enc *const e);

void arcd_dec_init(arcd_dec *const d, void *const model, void *const io);
arcd_char_t arcd_dec_get(arcd_dec *const d);
static inline
arcd_freq_t arcd_dec_map(const arcd_range_t v, const arcd_range_t range,
						 const arcd_freq_t total)
{
	return  v * total / range;
}

#ifdef __cplusplus
}
#endif

#endif
