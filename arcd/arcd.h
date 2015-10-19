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
 * shifting past the type size is illegal. That could be an issue for
 * calculating 2^32 - 1 (which is a legal 32bit int value).
 */
#define _ARCD_2_POW_N(N) (1 << (N))

/* Number of bits used to represent frequency values. This value is exact -
 * maximum frequency value is 2^ARCD_FREQ_BITS - 1.
 */
enum { ARCD_FREQ_BITS = 15 };
/* Number of bits used to represent arithmetic coder intervals. This value is
 * NOT exact - maximum interval value is 2^ARCD_FREQ_BITS which formaly uses
 * ARCD_RANGE_BITS + 1 bits. Though, maximum value is the only range value that
 * uses more than ARCD_RANGE_BITS bits.
 */
enum { ARCD_RANGE_BITS = 17 };
/* Minimum and maximum frequency values. */
enum { ARCD_FREQ_MAX = _ARCD_2_POW_N(ARCD_FREQ_BITS) - 1 };
/* Minimum and maximum interval value. */
enum { ARCD_RANGE_MAX = _ARCD_2_POW_N(ARCD_RANGE_BITS) };

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
 * to hold ARCD_RANGE_BITS + 1 bits. Valid values are in the range
 * [0, ARCD_RANGE_MAX] (inclusive). Intervals themselves are closed from the
 * left and open from the right: [left, right).
 */
typedef unsigned arcd_range_t;
/* Bit buffer. Used by arcd_enc::output() and arcd_dec::input() callbacks. No
 * particular requirements for this type.
 */
typedef unsigned char arcd_buf_t;
/* Private type that can hold result of arcd_freq_t * arcd_range_t without
 * overflowing.
 */
typedef unsigned _arcd_value_t;

/* Number of bits in the bit buffer. */
#define ARCD_BUF_BITS (8 * sizeof(arcd_buf_t))

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

/* Encoder callback. Will be called once per arcd_enc_put() call. Encoder uses
 * it to get cumulative probability interval for a symbol being encoded.
 */
typedef void (*arcd_getprob_t)(arcd_char_t ch, arcd_prob *prob, void *model);
/* Encoder callback. Encoder uses it to output encoded binary stream. Buffer has
 * buf_bits valid most significant bits. The only time when buf_bits could be
 * less than total amount of bits in buf is when it's called the last time from
 * arcd_enc_fin().
 */
typedef void (*acrd_output_t)(arcd_buf_t buf, unsigned buf_bits, void *io);
/* Decoder callback. Will be called once per arcd_dec_get() call. Decoder uses
 * it to get symbol with cumulative probability interval that contains value
 * (v / range). Callback must use arcd_freq_scale() to scale v based on range
 * and current frequency total value. Callback also returns corresponding
 * cumulative probability interval in prob.
 */
typedef arcd_char_t (*arcd_getch_t)(arcd_range_t v, arcd_range_t range,
									arcd_prob *prob, void *model);
/* Decoder callback. Decoder uses it when it needs more bits to decode next
 * symbol. Callback returns number of valid most significant bits in buffer.
 * Once callback returns 0 it will not be called again. In that case decoder
 * will use 0's to continue the input stream.
 */
typedef unsigned (*arcd_input_t)(arcd_buf_t *buf, void *io);

typedef struct _arcd_state
{
	arcd_range_t lower;
	arcd_range_t upper;
	arcd_range_t range;
	arcd_buf_t buf;
	unsigned buf_bits;
	void *model;
	void *io;
}
_arcd_state;

/* Arithmetic encoder. Must be initialized with arcd_enc_init(). */
typedef struct arcd_enc
{
	_arcd_state _state;
	unsigned _pending;
	arcd_getprob_t _getprob;
	acrd_output_t _output;
}
arcd_enc;

/* Arithmetic decoder. Must be initialized with arcd_dec_init(). */
typedef struct arcd_dec
{
	_arcd_state _state;
	arcd_range_t _v;
	unsigned _v_bits;
	unsigned _fin;
	arcd_getch_t _getch;
	arcd_input_t _input;
}
arcd_dec;

/* Initializes arithmetic encoder. Parameters model and io are for external use
 * and will be passed to getprob() and output() callbacks as is.
 */
void arcd_enc_init(arcd_enc *const e,
				   const arcd_getprob_t getprob, void *const model,
				   const acrd_output_t output, void *const io);
/* Encodes one symbol. Will call getprob() callback once. Will call output()
 * callback 0 or more times.
 */
void arcd_enc_put(arcd_enc *const e, const arcd_char_t ch);
/* Finalizes encoded binary sequence. Will call output() callback 0 or more
 * times.
 */
void arcd_enc_fin(arcd_enc *const e);

/* Initializes arithmetic decoder. Parameters model and io are for external use
 * and will be passed to getch() and input() callbacks as is.
 */
void arcd_dec_init(arcd_dec *const d,
				   const arcd_getch_t getch, void *const model,
				   const arcd_input_t input, void *const io);
/* Decodes one symbol. Will call getch() callback once. Will call input()
 * callback 0 or more times. Because of the nature of arithmetic coder this
 * function can be called infinite times. Thus you need some whay to identify
 * the end of the decoded sequence. One way is to put length at the start of
 * the encoded stream. Another way is to extend alphabet by 1 additional
 * "end of stream" symbol and encode it last. In some cases length of the stream
 * could be known out of the context (e.g. chess board has 64 cells).
 */
arcd_char_t arcd_dec_get(arcd_dec *const d);

/* Scales value from coder range to model frequency interval. Must be used
 * inside arcd_getch_t() callback to get cumulative frequency value. Inverse of
 * what encoder does when it maps arcd_prob value on the current range.
 */
static inline
arcd_freq_t arcd_freq_scale(const arcd_range_t v, const arcd_range_t range,
							const arcd_freq_t total)
{
	/* This diagram can help to convince yourself that the formula below works
	 * correctly. Encoder maps x from A to y from B. Decoder maps back y from B
	 * to x from A. Encoder calculates y = floor(|B| * x / |A|). To reverse this
	 * operation decoder needs to find such x that:
	 * max x, |B| * x < |A| * (y + 1)
	 * or, since we do integer arithmetic:
	 * max x, |B| * x <= |A| * (y + 1) - 1
	 * and since 0 < |B| and integer division does floor():
	 * x = (|A| * (y + 1) - 1) / |B|
	 *
	 * A 0                   1                   2                   3
	 *   |---+---+---+---+---|---+---+---+---+---|---+---+---+---+---|--> x
	 *   |<------ |B| ------>|
	 *
	 *   |<-- |A| -->|
	 *   |---+---+---|---+---+---|---+---+---|---+---+---|---+---+---|--> y
	 * B 0           1           2           3           4           5
	 */
	return (((_arcd_value_t)v + 1) * total - 1) / range;
}

#ifdef __cplusplus
}
#endif

#endif
