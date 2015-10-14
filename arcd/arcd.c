#include <assert.h>
#include "arcd.h"

#define STATIC_ASSERT(name, cond) \
	typedef char assert_##name[(cond)? 1: -1]

#define BITS(n) (8 * sizeof(n))

//FIXME: Use closed intervals
static const unsigned RANGE_BITS = ARCD_RANGE_BITS;
static const arcd_range_t RANGE_MIN = 0;
static const arcd_range_t RANGE_MAX = ARCD_RANGE_MAX;
static const arcd_range_t RANGE_ONE_HALF = RANGE_MAX / 2;
static const arcd_range_t RANGE_ONE_FOURTHS = RANGE_MAX / 4;
static const arcd_range_t RANGE_THREE_FOURTHS = 3 * RANGE_ONE_FOURTHS;

typedef unsigned value_t;

STATIC_ASSERT(freq_bits_fits_in_type,
		ARCD_FREQ_BITS <= BITS(arcd_freq_t));
STATIC_ASSERT(range_bits_fits_in_type,
		ARCD_RANGE_BITS <= BITS(arcd_range_t));
STATIC_ASSERT(name,
		ARCD_RANGE_BITS + ARCD_FREQ_BITS <= BITS(value_t));
STATIC_ASSERT(name1,
		ARCD_RANGE_BITS >= ARCD_FREQ_BITS + 2);

static void state_init(arcd_state *const state,
					   void *const model, void *const io)
{
	state->lower = RANGE_MIN;
	state->upper = RANGE_MAX;
	state->range = RANGE_MAX - RANGE_MIN;
	state->buf = 0;
	state->buf_bits= 0;
	state->model = model;
	state->io = io;
}

static void output_bit(arcd_enc *const e, const unsigned bit)
{
	assert(0 == bit || 1 == bit);
	e->_state.buf = e->_state.buf << 1 | bit;
	if (BITS(e->_state.buf) == ++e->_state.buf_bits)
	{
		e->_output(e->_state.buf, e->_state.buf_bits, e->_state.io);
		// Setting buf to 0 is not necessary, but simplifies debugging.
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
	if (0 == d->state.buf_bits)
	{
		if (0 == (d->state.buf_bits = d->input(&d->state.buf, d->state.io)))
		{
			// If real input is empty, then it can be continued by infinite
			// amount of 0s or 1s (or a mix of).
			d->state.buf_bits = ~0;
		}
	}
	if (BITS(d->state.buf) < d->state.buf_bits)
	{
		// If input is empty, any return value will work.
		// Aparently there could be a mod that expects that 0s are used to
		// for input continuation. That will reduce the size of some sequences.
		return 0;
	}
	return 1 & (d->state.buf >> --d->state.buf_bits);
}

static void zoom_in(arcd_state *const state, arcd_prob *const prob)
{
	assert(state->lower < state->upper);
	assert(state->range == state->upper - state->lower);
	assert(prob->lower < prob->upper);
	assert(prob->upper <= prob->total);
	//FIXME: Not RANGE_MAX, but RANGE_MAX / 4
	assert(prob->total <= RANGE_MAX);
	state->upper = state->lower + (state->range * prob->upper) / prob->total;
	state->lower = state->lower + (state->range * prob->lower) / prob->total;
}

void arcd_enc_init(arcd_enc *const e,
				   arcd_getprob_f *const getprob, void *const model,
				   acrd_output_f *const output, void *const io)
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
		else if (e->_state.lower >= RANGE_ONE_FOURTHS &&
				 e->_state.upper <= RANGE_THREE_FOURTHS)
		{
			++e->_pending;
			e->_state.lower = 2 * (e->_state.lower - RANGE_ONE_FOURTHS);
			e->_state.upper = 2 * (e->_state.upper - RANGE_ONE_FOURTHS);
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
		output_bits(e, RANGE_ONE_FOURTHS <= e->_state.lower);
	}
	if (0 != e->_state.buf_bits)
	{
		e->_output(e->_state.buf, e->_state.buf_bits, e->_state.io);
	}
}

void arcd_dec_init(arcd_dec *const d, void *const model, void *const io)
{
	state_init(&d->state, model, io);
	d->v = 0;
	d->v_bits = 0;
}

arcd_char_t arcd_dec_get(arcd_dec *const d)
{
	if (0 == d->v_bits)
	{
		for (unsigned i = RANGE_BITS; 0 < i--;)
		{
			d->v = d->v << 1 | input_bit(d);
		}
		d->v_bits = RANGE_BITS;
	}
	arcd_prob prob;
	const arcd_range_t v = d->v - d->state.lower;
	const arcd_char_t ch = d->getch(v, d->state.range, &prob, d->state.model);
	zoom_in(&d->state, &prob);
	for (;;)
	{
		if (d->state.upper < RANGE_ONE_HALF)
		{
			d->state.lower = 2 * d->state.lower;
			d->state.upper = 2 * d->state.upper;
		}
		else if (d->state.lower >= RANGE_ONE_HALF)
		{
			d->state.lower = 2 * (d->state.lower - RANGE_ONE_HALF);
			d->state.upper = 2 * (d->state.upper - RANGE_ONE_HALF);
			d->v -= RANGE_ONE_HALF;
		}
		else if (d->state.lower >= RANGE_ONE_FOURTHS &&
				 d->state.upper < RANGE_THREE_FOURTHS)
		{
			d->state.lower = 2 * (d->state.lower - RANGE_ONE_FOURTHS);
			d->state.upper = 2 * (d->state.upper - RANGE_ONE_FOURTHS);
			d->v -= RANGE_ONE_FOURTHS;
		} else {
			break;
		}
		d->v = d->v << 1 | input_bit(d);
		assert(0 <= d->v && d->v < RANGE_MAX);
	}
	d->state.range = d->state.upper - d->state.lower;
	return ch;
}
