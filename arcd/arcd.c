#include <assert.h>
#include "arcd.h"

#define STATIC_ASSERT(name, cond) \
	typedef char assert_##name[(cond)? 1: -1]

#define BITS(n) (8 * sizeof(n))

//FIXME: Use closed intervals
static const unsigned RANGE_BITS = 16;
static const arcd_range_t RANGE_MIN = 0;
static const arcd_range_t RANGE_MAX = 1 << RANGE_BITS;
static const arcd_range_t RANGE_ONE_HALF = RANGE_MAX / 2;
static const arcd_range_t RANGE_ONE_FOURTHS = RANGE_MAX / 4;
static const arcd_range_t RANGE_THREE_FOURTHS = 3 * RANGE_ONE_FOURTHS;

static void state_init(arcd_state *const state,
					   void *const prob_ctx, void *const io_ctx)
{
	state->lower = RANGE_MIN;
	state->upper = RANGE_MAX;
	state->range = RANGE_MAX - RANGE_MIN;
	state->buf = 0;
	state->buf_bits= 0;
	state->prob_ctx = prob_ctx;
	state->io_ctx = io_ctx;
}

static void output_bit(arcd_enc *const e, const unsigned bit)
{
	assert(0 == bit || 1 == bit);
	e->state.buf = e->state.buf << 1 | bit;
	if (BITS(e->state.buf) == ++e->state.buf_bits)
	{
		e->output(e->state.buf, e->state.buf_bits, e->state.io_ctx);
		// Setting buf to 0 is not necessary, but simplifies debugging.
		e->state.buf = 0;
		e->state.buf_bits = 0;
	}
}

static void output_bits(arcd_enc *const e, const unsigned bit)
{
	assert(0 == bit || 1 == bit);
	output_bit(e, bit);
	for (const unsigned inv = !bit; 0 < e->pending; --e->pending)
	{
		output_bit(e, inv);
	}
}

static unsigned input_bit(arcd_dec *const d)
{
	if (0 == d->state.buf_bits)
	{
		if (0 == (d->state.buf_bits = d->input(&d->state.buf, d->state.io_ctx)))
		{
			// If real input is empty, then it can be continued by infinite
			// amount of 0s or 1s (or a mix of).
			d->state.buf_bits = ~0;
		}
	}
	if (BITS(d->state.buf) < d->state.buf_bits)
	{
		// If input is empty, any return value will work.
		return 0;
	}
	return 1 & (d->state.buf >> --d->state.buf_bits);
}

static void zoom_in(arcd_state *const state, arcd_prob *const prob)
{
	assert(state->lower < state->upper);
	assert(state->range == state->upper - state->lower);
	assert(prob->lower < prob->upper);
	assert(prob->upper <= prob->range);
	assert(prob->range <= RANGE_MAX);
	state->upper = state->lower + (state->range * prob->upper) / prob->range;
	state->lower = state->lower + (state->range * prob->lower) / prob->range;
}

void arcd_enc_init(arcd_enc *const e, void *const prob_ctx, void *const io_ctx)
{
	state_init(&e->state, prob_ctx, io_ctx);
	e->pending = 0;
}

void arcd_enc_put(arcd_enc *const e, const arcd_char_t ch)
{
	arcd_prob prob;
	e->getprob(ch, &prob, e->state.prob_ctx);
	zoom_in(&e->state, &prob);
	for (;;)
	{
		if (e->state.upper <= RANGE_ONE_HALF)
		{
			output_bits(e, 0);
			e->state.lower = 2 * e->state.lower;
			e->state.upper = 2 * e->state.upper;
		}
		else if (e->state.lower >= RANGE_ONE_HALF)
		{
			output_bits(e, 1);
			e->state.lower = 2 * (e->state.lower - RANGE_ONE_HALF);
			e->state.upper = 2 * (e->state.upper - RANGE_ONE_HALF);
		}
		else if (e->state.lower >= RANGE_ONE_FOURTHS &&
				 e->state.upper <= RANGE_THREE_FOURTHS)
		{
			++e->pending;
			e->state.lower = 2 * (e->state.lower - RANGE_ONE_FOURTHS);
			e->state.upper = 2 * (e->state.upper - RANGE_ONE_FOURTHS);
		}
		else
		{
			break;
		}
	}
	e->state.range = e->state.upper - e->state.lower;
}

void arcd_enc_fin(arcd_enc *const e)
{
	if (0 == e->state.lower)
	{
		assert(RANGE_ONE_HALF < e->state.upper);
		if (RANGE_MAX != e->state.upper)
		{
			output_bits(e, 0);
		}
	}
	else if (RANGE_MAX == e->state.upper)
	{
		assert(RANGE_ONE_HALF > e->state.lower);
		if (0 != e->state.lower)
		{
			output_bits(e, 1);
		}
	}
	else
	{
		++e->pending;
		output_bits(e, RANGE_ONE_FOURTHS <= e->state.lower);
	}
	if (0 != e->state.buf_bits)
	{
		e->output(e->state.buf, e->state.buf_bits, e->state.io_ctx);
	}
}

void arcd_dec_init(arcd_dec *const d, void *const prob_ctx, void *const io_ctx)
{
	state_init(&d->state, prob_ctx, io_ctx);
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
	const arcd_char_t ch = d->getch(d->v, d->state.range, &prob, d->state.prob_ctx);
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
	}
	d->state.range = d->state.upper - d->state.lower;
	return ch;
}
