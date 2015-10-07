#include <vector>
#include <string>
#include <sstream>
#include <numeric>
#include <arcd.h>

#ifndef _countof
#define _countof(v) (sizeof(v) / sizeof((v)[0]))
#endif
#define BITS(n) (8 * sizeof(n))

namespace
{
	typedef std::vector<arcd_prob> model_t;

	void getprob(const arcd_char_t ch, arcd_prob *const prob, void *const model)
	{
		const model_t *const probs = static_cast<const model_t *>(model);
		*prob = probs->at(ch);
	}

	arcd_char_t getch(const arcd_range_t v, const arcd_range_t range,
					  arcd_prob *const prob, void *const model)
	{
		const model_t *const probs = static_cast<const model_t *>(model);
		for (size_t i = probs->size(); 0 < i--;)
		{
			const arcd_prob &p = probs->at(i);
			const arcd_range_t vs = v * p.range / range;
			if (p.lower <= vs && vs < p.upper)
			{
				*prob = p;
				return (arcd_char_t)i;
			}
		}
		return -1;
	}

	void output(const arcd_buf_t buf, const unsigned buf_bits, void *const io)
	{
		std::ostringstream *const s = static_cast<std::ostringstream *>(io);
		for (unsigned i = buf_bits; 0 < i--;)
		{
			s->put(1 & (buf >> i)? '1': '0');
		}
	}

	unsigned input(arcd_buf_t *const buf, void *const io)
	{
		std::istringstream *const s =static_cast<std::istringstream *>(io);
		std::istringstream::char_type ch;
		unsigned bits;
		for (bits = 0; BITS(*buf) > bits && *s >> ch; ++bits)
		{
			*buf = *buf << 1 | ('0' == ch? 0: 1);
		}
		return bits;
	}

	model_t mk_model(const std::vector<arcd_range_t> &ps)
	{
		const arcd_range_t sum = std::accumulate(ps.begin(), ps.end(), 0);
		arcd_range_t lower = 0;
		model_t model(ps.size());
		for (size_t i = 0, e = ps.size(); e > i; ++i)
		{
			const arcd_range_t upper = lower + ps[i];
			arcd_prob &prob = model[i];
			prob.lower = lower;
			prob.upper = upper;
			prob.range = sum;
			lower = upper;
		}
		return model;
	}

	struct test_case
	{
		const model_t model;
		const std::vector<arcd_char_t> in;
		const std::string out;
	};

	const test_case c_test_cases[] =
	{
		{mk_model({2, 2}), {}, ""},
		{mk_model({2, 2}), {0}, "0"},
		{mk_model({2, 2}), {1}, "1"},
		{mk_model({2, 2}), {1, 0, 1, 0}, "1010"},
		{mk_model({2, 2}), {0, 1, 0, 1}, "0101"},
		{mk_model({2, 2}), {1, 1, 1, 1}, "1111"},
		{mk_model({2, 2}), {0, 0, 0, 0}, "0000"},
		{mk_model({8, 56}), {1}, "1"},
		{mk_model({8, 56}), {1, 1}, "1"},
		{mk_model({8, 56}), {1, 1, 1, 1}, "1"},
		{mk_model({8, 56}), {1, 1, 1, 1, 1}, "1"},
		{mk_model({8, 56}), {1, 1, 1, 1, 1, 1}, "11"},
		{mk_model({56, 8}), {0}, "0"},
		{mk_model({56, 8}), {0, 0}, "0"},
		{mk_model({56, 8}), {0, 0, 0, 0}, "0"},
		{mk_model({56, 8}), {0, 0, 0, 0, 0}, "0"},
		{mk_model({56, 8}), {0, 0, 0, 0, 0, 0}, "00"},
		{mk_model({2, 4, 2}), {1}, "01"},
		{mk_model({2, 4, 2}), {1, 1}, "011"},
		{mk_model({2, 4, 2}), {1, 1, 1}, "0111"},
	};

	bool run_tests()
	{
		bool ok = true;
		for (size_t i = 0; _countof(c_test_cases) > i; ++i)
		{
			const test_case &tc = c_test_cases[i];
			arcd_enc enc;
			std::ostringstream out;
			arcd_enc_init(&enc, const_cast<model_t *>(&tc.model), &out);
			enc.output = output;
			enc.getprob = getprob;
			for (size_t k = 0; tc.in.size() > k; ++k)
			{
				arcd_enc_put(&enc, tc.in[k]);
			}
			arcd_enc_fin(&enc);
			const std::string outstr = out.str();
			if (tc.out != outstr)
			{
				fprintf(stderr, "Test case #%zu (encode) failed:\n", i);
				fprintf(stderr, "    Actual output:   %s\n", outstr.c_str());
				fprintf(stderr, "    Expected output: %s\n", tc.out.c_str());
				ok = false;
			}
			arcd_dec dec;
			std::istringstream in(outstr);
			arcd_dec_init(&dec, const_cast<model_t *>(&tc.model), &in);
			dec.input = input;
			dec.getch = getch;
			for (size_t k = 0; tc.in.size() > k; ++k)
			{
				const arcd_char_t ch = arcd_dec_get(&dec);
				if (tc.in[k] != ch)
				{
					fprintf(stderr, "Test case #%zu (decode) failed at #%zu:\n", i, k);
					fprintf(stderr, "    Actual symbol:   %u\n", ch);
					fprintf(stderr, "    Expected symbol: %u\n", tc.in[k]);
					ok = false;
					break;
				}
			}
		}
		return ok;
	}
}

int main(int argc, char *argv[])
{
	(void)argc; (void)argv;
	return run_tests()? 0: 1;
}
