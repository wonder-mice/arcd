#include <iostream>
#include <vector>
#include <array>

namespace
{
#define STATIC_ASSERT(name, cond) \
	typedef char assert_##name[(cond)? 1: -1]

	typedef unsigned arcd_char_t;
	typedef unsigned arcd_range_t;

	const unsigned ARCD_RANGE_BITS = 15;
	const arcd_range_t ARCD_RANGE_MIN = 0;
	const arcd_range_t ARCD_RANGE_MAX = 1 << ARCD_RANGE_BITS;
	const arcd_range_t ARCD_RANGE_ONE_HALF = ARCD_RANGE_MAX / 2;
	const arcd_range_t ARCD_RANGE_ONE_FORTHS = ARCD_RANGE_MAX / 4;
	const arcd_range_t ARCD_RANGE_THREE_FORTHS = 3 * ARCD_RANGE_ONE_FORTHS;

	struct arcd_prob
	{
		arcd_range_t lower;
		arcd_range_t upper;
		arcd_range_t range;
	};

	struct arcd_state
	{
		arcd_range_t range;
		arcd_range_t lower;
		arcd_range_t upper;
		arcd_range_t one_half;
		arcd_range_t one_forths;
		arcd_range_t three_fourths;
	};

	struct arenc
	{
		unsigned range;
		unsigned lower;
		unsigned upper;
		unsigned one_half;
		unsigned one_forths;
		unsigned three_fourths;
		unsigned pending;
		unsigned char buf;
		unsigned buf_sz;
		void (*getprob)(unsigned ch, arprob *prob);
		void (*output)(arenc *c);
	};

	struct arcd_decoder
	{
		arcd_state state;
		unsigned char buf;
		unsigned buf_bits;
		arcd_range_t v;
		unsigned v_bits;
		arcd_char_t (*getch)(arcd_range_t v, arcd_range_t range, arcd_prob *prob);
		unsigned (*input)(arcd_decoder *d);
	};

	void arenc_init(arenc *const c)
	{
		c->range = 1 << (8 * sizeof(c->upper) - 1);
		c->lower = 0;
		c->upper = c->range - 1;
		c->one_half = c->upper / 2;
		c->one_forths = c->upper / 4;
		c->three_fourths = c->one_forths + c->one_half;
		c->pending = 0;
		c->buf_sz = 0;
	}

	static void output_bit(arenc *const c, const unsigned bit)
	{
		assert (0 == bit || 1 == bit);
		c->buf = c->buf << 1 | bit;
		if (8 * sizeof(c->buf) == ++c->buf_sz)
		{
			c->output(c);
			c->buf_sz = 0;
		}
	}

	static void output_bits(arenc *const c, const unsigned bit)
	{
		output_bit(c, bit);
		for (const unsigned inv = ~bit; 0 < c->pending; --c->pending)
		{
			output_bit(c, inv);
		}
	}

	void arenc_enc(arenc *const c, const int ch)
	{
		arprob prob;
		c->getprob(ch, &prob);
		assert(prob.lower < prob.upper);
		assert(prob.upper <= prob.range);
		unsigned range = c->upper - c->lower;
		//FIXME: overflow
		c->upper = c->lower + (range * prob.upper)/prob.range;
		c->lower = c->lower + (range * prob.lower)/prob.range;
		for (;;)
		{
			if (c->upper < c->one_half)
			{
				output_bits(c, 0);
				c->lower = 2 * c->lower;
				c->upper = 2 * c->upper;
			}
			else if (c->lower >= c->one_half)
			{
				output_bits(c, 1);
				c->lower = 2 * c->lower - c->one_half;
				c->upper = 2 * c->upper - c->one_half;
			}
			else if (c->lower >= c->one_forths && c->upper < c->three_fourths)
			{
				++c->pending_bits;
				c->lower = 2 * c->lower - c->one_half;
				c->upper = 2 * c->upper - c->one_half;
			} else {
				break;
			}
		}
	}

	void arenc_fin(arenc *const c)
	{
		++c->pending;
		output_bits(c, c->lower >= c->one_forths);
		if (0 != c->buf_sz)
		{
			c->output(c);
		}
	}

	unsigned input_bit(arcd_decoder *const d)
	{
		if (0 == d->buf_bits)
		{
			d->input(d);
		}
		// if input is empty, any return value will work
		return 1 & (d->buf >> --d->buf_bits);
	}

	void input_bits(arcd_decoder *const d)
	{

		d->buf_bits = d->input(d, &d->buf);

	}

	void arcd_decoder_init(arcd_decoder *const d)
	{
		d->buf = 0;
		d->buf_bits = 0;
		d->state.lower = ARCD_RANGE_MIN;
		d->state.upper = ARCD_RANGE_MAX;
		d->state.range = ARCD_RANGE_MAX - ARCD_RANGE_MIN;
	}

	arcd_char_t arcd_get(arcd_decoder *const d)
	{
		if (0 == d->v_bits)
		{
			for (unsigned i = ARCD_RANGE_BITS; 0 < i--;)
			{
				d->v = (d->v << 1) | input_bit(d);
			}
			d->v_bits = ARCD_RANGE_BITS;
		}
		arcd_prob prob;
		const arcd_char_t ch = d->getch(d->v, d->state.range, &prob);
		d->state.upper = d->state.lower + d->state.range * prob.upper / prob.range;
		d->state.lower = d->state.lower + d->state.range * prob.lower / prob.range;
		d->state.range = d->state.upper - d->state.lower;
		for (;;)
		{
			if (d->state.upper < ARCD_RANGE_ONE_HALF)
			{
				d->state.lower = 2 * d->state.lower;
				d->state.upper = 2 * d->state.upper;
			}
			else if (d->state.lower >= ARCD_RANGE_ONE_HALF)
			{
				d->state.lower = 2 * d->state.lower - ARCD_RANGE_ONE_HALF;
				d->state.upper = 2 * d->state.upper - ARCD_RANGE_ONE_HALF;
				d->v -= ARCD_RANGE_ONE_HALF;
			}
			else if (d->state.lower >= ARCD_RANGE_ONE_FORTHS &&
					 d->state.upper < ARCD_RANGE_THREE_FORTHS)
			{
				d->state.lower = 2 * d->state.lower - ARCD_RANGE_ONE_FORTHS;
				d->state.upper = 2 * d->state.upper - ARCD_RANGE_ONE_FORTHS;
				d->v -= ARCD_RANGE_ONE_FORTHS;
			} else {
				break;
			}
			d->v = (d->v << 1) | input_bit(d);
		}
		return ch;
	}

	std::string out;
	void output_bit(bool bit)
	{
		out.append(bit? "1": "0");
	}

	void output_bit_plus_pending(bool bit, int &pending_bits)
	{
		output_bit( bit );
		for ( int i = 0 ; i < pending_bits ; i++ ) {
			output_bit( !bit );
		}
		pending_bits = 0;
	}

	std::vector<int> g_input;//{{1, 2, 0, 3, 6, 4, 5, 7, 8, 9, 10, 11, 12, 13, 14, 15}};
	std::vector<int> g_pos;
	std::vector<int> g_elems;

	int inv(const size_t i)
	{
		const int n = g_input.size();
		const int symbol = g_input[i];
		const int v = g_pos[symbol];
		g_pos[g_elems[n - i - 1]] = g_pos[symbol];
		g_elems[g_pos[symbol]] = g_elems[n - i - 1];
		//fprintf(stderr, "i=%zi: in=%i, out=%i\n", i, (int)g_input[i], (int)v);
		return v;
	}
}

const unsigned CODE_VALUE_BITS = 28;
typedef unsigned CODE_VALUE;
static const CODE_VALUE MAX_CODE      = (CODE_VALUE(1) << CODE_VALUE_BITS) - 1;
static const CODE_VALUE ONE_FOURTH    = CODE_VALUE(1) << (CODE_VALUE_BITS - 2);
static const CODE_VALUE ONE_HALF      = 2 * ONE_FOURTH;
static const CODE_VALUE THREE_FOURTHS = 3 * ONE_FOURTH;

int main()
{
	const int N = 512*512;
	g_input.resize(N);
	g_pos.resize(N);
	g_elems.resize(N);
	for (int i = 0; N > i; ++i) {
		g_input[i] = (int)i;
		g_elems[i] = (int)i;
		g_pos[i] = (int)i;
	}
	unsigned int high = MAX_CODE;
	unsigned int low = 0;
	int pending_bits = 0;
	int c;
	unsigned pcount = g_input.size();
	unsigned plower;
	unsigned pupper;

	int counter_i = 0;
	int counter_j = 0;
	for (size_t i = 0; g_input.size() - 1 > i; ++i)
	{
		++counter_i;
		c = inv(i);
		plower = c;
		pupper = c + 1;
		//fprintf(stderr, "low=%u (%x), up=%u (%x), pcount=%u (%x)\n",
		//		plower, plower, pupper, pupper, pcount, pcount);

		unsigned range = high - low + 1;
		high = low + (range * pupper)/pcount - 1;
		low = low + (range * plower)/pcount;
		--pcount;

		for (;;)
		{
			++counter_j;
			if ( high < ONE_HALF ) {
				output_bit_plus_pending( 0, pending_bits );
			} else if ( low >= ONE_HALF ) {
				output_bit_plus_pending( 1, pending_bits );
			} else if ( low >= ONE_FOURTH && high < THREE_FOURTHS ) {
				pending_bits++;
				low -= ONE_FOURTH;
				high -= ONE_FOURTH;
			} else {
				break;
			}
			high <<= 1;
			high++;
			low <<= 1;
			high &= MAX_CODE;
			low &= MAX_CODE;
		}
	}
	pending_bits++;
	if ( low < ONE_FOURTH )
		output_bit_plus_pending(0, pending_bits);
	else
		output_bit_plus_pending(1, pending_bits);
	fprintf(stderr, "counter_i=%i, counter_i=%i\n", counter_i, counter_j);
	fprintf(stderr, "len=%zu\n", out.size());
	//fprintf(stderr, "%s\n", out.c_str());
	return 0;
}
