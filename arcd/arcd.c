#include <assert.h>
#include "arcd.h"

#define STATIC_ASSERT(name, cond) \
	typedef char assert_##name[(cond)? 1: -1]

#define BITS(n) (8 * sizeof(n))

STATIC_ASSERT(freq_bits_fits_in_type,
		ARCD_FREQ_BITS <= BITS(arcd_freq_t));
STATIC_ASSERT(range_bits_fits_in_type,
		ARCD_RANGE_BITS <= BITS(arcd_range_t));
STATIC_ASSERT(range_times_freq_not_overflow,
		ARCD_RANGE_BITS + ARCD_FREQ_BITS <= BITS(_arcd_value_t));
STATIC_ASSERT(range_one_fourth_contains_freqs,
		ARCD_RANGE_BITS >= ARCD_FREQ_BITS + 2);

/* This constants have a bit different semantic from ARCD_RANGE_XXX values which
 * mostly describe the type itself. This values define intervals used by the
 * coder.
 */
static const unsigned RANGE_BITS = ARCD_RANGE_BITS;
static const arcd_range_t RANGE_MIN = 0;
static const arcd_range_t RANGE_MAX = ARCD_RANGE_MAX;
static const arcd_range_t RANGE_ONE_HALF = RANGE_MAX / 2;
static const arcd_range_t RANGE_ONE_FOURTH = RANGE_MAX / 4;
static const arcd_range_t RANGE_THREE_FOURTH = 3 * RANGE_ONE_FOURTH;
/* Bit used to extend encoded bit stream past its end. Currently can be set to
 * either 0 or 1. However, encoder can have a small optimization to remove last
 * sequence of CONTINUATION_BITs from the output (since decoder will insert them
 * back anyway).
 */
static const unsigned CONTINUATION_BIT = 0;

static void state_init(_arcd_state *const state,
					   void *const model, void *const io)
{
	state->lower = RANGE_MIN;
	state->upper = RANGE_MAX;
	state->range = RANGE_MAX - RANGE_MIN;
	state->buf = 0;
	state->buf_bits = 0;
	state->model = model;
	state->io = io;
}

static void output_bit(arcd_enc *const e, const unsigned bit)
{
	assert(0 == bit || 1 == bit);
	e->_state.buf |= bit << (ARCD_BUF_BITS - ++e->_state.buf_bits);
	if (ARCD_BUF_BITS == e->_state.buf_bits)
	{
		e->_output(e->_state.buf, ARCD_BUF_BITS, e->_state.io);
		e->_state.buf = 0;
		e->_state.buf_bits = 0;
	}
}

static void output_bits(arcd_enc *const e, const unsigned bit)
{
	assert(0 == bit || 1 == bit);
	output_bit(e, bit);
	for (const unsigned inv = !bit; 0 < e->_pending; --e->_pending)
	{
		output_bit(e, inv);
	}
}

static unsigned input_bit(arcd_dec *const d)
{
	if (d->_fin)
	{
		return CONTINUATION_BIT;
	}
	if (0 == d->_state.buf_bits)
	{
		d->_state.buf_bits = d->_input(&d->_state.buf, d->_state.io);
		if (0 == d->_state.buf_bits)
		{
			d->_fin = !d->_fin;
			return CONTINUATION_BIT;
		}
	}
	const unsigned bit = d->_state.buf >> (ARCD_BUF_BITS - 1);
	assert(0 == bit || 1 == bit);
	d->_state.buf <<= 1;
	--d->_state.buf_bits;
	return bit;
}

static void zoom_in(_arcd_state *const state, arcd_prob *const prob)
{
	assert(state->upper <= ARCD_RANGE_MAX);
	assert(state->lower < state->upper);
	assert(state->range == state->upper - state->lower);
	assert(prob->lower < prob->upper);
	assert(prob->upper <= prob->total);
	assert(prob->total <= ARCD_FREQ_MAX);
	assert(state->range >= prob->total);
	const _arcd_value_t range = state->range;
	state->upper = state->lower + prob->upper * range / prob->total;
	state->lower = state->lower + prob->lower * range / prob->total;
	/* Don't update range, it will be updated later by encoder or decoder. */
}

void arcd_enc_init(arcd_enc *const e,
				   const arcd_getprob_t getprob, void *const model,
				   const acrd_output_t output, void *const io)
{
	state_init(&e->_state, model, io);
	e->_getprob = getprob;
	e->_output = output;
	e->_pending = 0;
}

void arcd_enc_put(arcd_enc *const e, const arcd_char_t ch)
{
	arcd_prob prob;
	e->_getprob(ch, &prob, e->_state.model);
	zoom_in(&e->_state, &prob);
	for (;;)
	{
		if (e->_state.upper <= RANGE_ONE_HALF)
		{
			output_bits(e, 0);
			e->_state.lower = 2 * e->_state.lower;
			e->_state.upper = 2 * e->_state.upper;
		}
		else if (e->_state.lower >= RANGE_ONE_HALF)
		{
			output_bits(e, 1);
			e->_state.lower = 2 * (e->_state.lower - RANGE_ONE_HALF);
			e->_state.upper = 2 * (e->_state.upper - RANGE_ONE_HALF);
		}
		else if (e->_state.lower >= RANGE_ONE_FOURTH &&
				 e->_state.upper <= RANGE_THREE_FOURTH)
		{
			++e->_pending;
			e->_state.lower = 2 * (e->_state.lower - RANGE_ONE_FOURTH);
			e->_state.upper = 2 * (e->_state.upper - RANGE_ONE_FOURTH);
		}
		else
		{
			break;
		}
	}
	e->_state.range = e->_state.upper - e->_state.lower;
}

void arcd_enc_fin(arcd_enc *const e)
{
	if (RANGE_MIN == e->_state.lower && 0 == e->_pending)
	{
		assert(RANGE_ONE_HALF < e->_state.upper);
		if (RANGE_MAX != e->_state.upper)
		{
			output_bits(e, 0);
		}
	}
	else if (RANGE_MAX == e->_state.upper && 0 == e->_pending)
	{
		assert(RANGE_ONE_HALF > e->_state.lower);
		if (0 != e->_state.lower)
		{
			output_bits(e, 1);
		}
	}
	else
	{
		if (RANGE_MIN != e->_state.lower && RANGE_MAX != e->_state.upper)
		{
			++e->_pending;
		}
		output_bits(e, RANGE_ONE_FOURTH <= e->_state.lower);
	}
	if (0 != e->_state.buf_bits)
	{
		e->_output(e->_state.buf, e->_state.buf_bits, e->_state.io);
	}
}

void arcd_dec_init(arcd_dec *const d,
				   const arcd_getch_t getch, void *const model,
				   const arcd_input_t input, void *const io)
{
	state_init(&d->_state, model, io);
	d->_v = 0;
	d->_v_bits = 0;
	d->_fin = 0;
	d->_getch = getch;
	d->_input = input;
}

arcd_char_t arcd_dec_get(arcd_dec *const d)
{
	if (0 == d->_v_bits)
	{
		for (unsigned i = RANGE_BITS; 0 < i--;)
		{
			d->_v = d->_v << 1 | input_bit(d);
		}
		d->_v_bits = RANGE_BITS;
	}
	assert(d->_state.lower <= d->_v);
	assert(d->_state.upper > d->_v);
	arcd_prob prob;
	const arcd_range_t v = d->_v - d->_state.lower;
	const arcd_char_t ch = d->_getch(v, d->_state.range, &prob, d->_state.model);
	zoom_in(&d->_state, &prob);
	for (;;)
	{
		if (d->_state.upper <= RANGE_ONE_HALF)
		{
			d->_state.lower = 2 * d->_state.lower;
			d->_state.upper = 2 * d->_state.upper;
		}
		else if (d->_state.lower >= RANGE_ONE_HALF)
		{
			d->_state.lower = 2 * (d->_state.lower - RANGE_ONE_HALF);
			d->_state.upper = 2 * (d->_state.upper - RANGE_ONE_HALF);
			d->_v -= RANGE_ONE_HALF;
		}
		else if (d->_state.lower >= RANGE_ONE_FOURTH &&
				 d->_state.upper <= RANGE_THREE_FOURTH)
		{
			d->_state.lower = 2 * (d->_state.lower - RANGE_ONE_FOURTH);
			d->_state.upper = 2 * (d->_state.upper - RANGE_ONE_FOURTH);
			d->_v -= RANGE_ONE_FOURTH;
		} else {
			break;
		}
		d->_v = d->_v << 1 | input_bit(d);
		assert(0 <= d->_v && d->_v < RANGE_MAX);
	}
	d->_state.range = d->_state.upper - d->_state.lower;
	return ch;
}
