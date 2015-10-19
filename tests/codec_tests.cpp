#include <vector>
#include <string>
#include <sstream>
#include <numeric>
#include <arcd.h>

#ifndef _countof
#define _countof(v) (sizeof(v) / sizeof((v)[0]))
#endif

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
			const arcd_freq_t vs = arcd_freq_scale(v, range, p.total);
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
		for (unsigned i = ARCD_BUF_BITS, e = ARCD_BUF_BITS - buf_bits; e < i--;)
		{
			s->put(1 & (buf >> i)? '1': '0');
		}
	}

	unsigned input(arcd_buf_t *const buf, void *const io)
	{
		std::istringstream *const s =static_cast<std::istringstream *>(io);
		std::istringstream::char_type ch;
		*buf = 0;
		unsigned bits = ARCD_BUF_BITS;
		while (0 < bits && *s >> ch)
		{
			*buf |= ('0' == ch? 0: 1) << --bits;
		}
		return ARCD_BUF_BITS - bits;
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
			prob.total = sum;
			lower = upper;
		}
		return model;
	}

	struct test_case
	{
		const std::string name;
		const model_t model;
		const std::vector<arcd_char_t> in;
		const std::string out;
	};

	const test_case c_test_cases[] =
	{
		{"0a", mk_model({}), {}, ""},
		{"0b", mk_model({1}), {}, ""},
		{"0c", mk_model({1}), {0}, ""},
		{"0d", mk_model({1}), {0, 0}, ""},
		{"0e", mk_model({1}), {0, 0, 0}, ""},
		{"1a", mk_model({2, 2}), {}, ""},
		{"1b", mk_model({2, 2}), {0}, "0"},
		{"1c", mk_model({2, 2}), {1}, "1"},
		{"1d", mk_model({2, 2}), {1, 0, 1, 0}, "1010"},
		{"1e", mk_model({2, 2}), {0, 1, 0, 1}, "0101"},
		{"1f", mk_model({2, 2}), {1, 1, 1, 1}, "1111"},
		{"1g", mk_model({2, 2}), {0, 0, 0, 0}, "0000"},
		{"2a", mk_model({8, 56}), {1}, "1"},
		{"2b", mk_model({8, 56}), {1, 1}, "1"},
		{"2c", mk_model({8, 56}), {1, 1, 1, 1}, "1"},
		{"2d", mk_model({8, 56}), {1, 1, 1, 1, 1}, "1"},
		{"2e", mk_model({8, 56}), {1, 1, 1, 1, 1, 1}, "11"},
		{"3a", mk_model({56, 8}), {0}, "0"},
		{"3b", mk_model({56, 8}), {0, 0}, "0"},
		{"3c", mk_model({56, 8}), {0, 0, 0, 0}, "0"},
		{"3d", mk_model({56, 8}), {0, 0, 0, 0, 0}, "0"},
		{"3e", mk_model({56, 8}), {0, 0, 0, 0, 0, 0}, "00"},
		{"4a", mk_model({2, 4, 2}), {1}, "01"},
		{"4b", mk_model({2, 4, 2}), {1, 1}, "011"},
		{"4c", mk_model({2, 4, 2}), {1, 1, 1}, "0111"},
		{"4d", mk_model({2, 4, 2}), {1, 1, 1}, "0111"},
		{"5a", mk_model({4, 1, 8, 3}), {0}, "00"},
		{"5b", mk_model({4, 1, 8, 3}), {1}, "0100"},
		{"5c", mk_model({4, 1, 8, 3}), {2}, "10"},
		{"5d", mk_model({4, 1, 8, 3}), {3}, "111"},
		{"6a", mk_model({1, 2, 4}), {0}, "000"},
		{"6b", mk_model({1, 2, 4}), {1}, "010"},
		{"6c", mk_model({1, 2, 4}), {2}, "1"},
		{"6d", mk_model({1, 2, 4}), {0, 0}, "000000"},
		{"6e", mk_model({1, 2, 4}), {0, 1}, "000010"},
		{"6f", mk_model({1, 2, 4}), {0, 2}, "0001"},
		{"6g", mk_model({1, 2, 4}), {1, 0}, "001010"},
		{"6h", mk_model({1, 2, 4}), {1, 1}, "0011"},
		{"6i", mk_model({1, 2, 4}), {1, 2}, "0101"},
		{"6j", mk_model({1, 2, 4}), {2, 0}, "0111"},
		{"6k", mk_model({1, 2, 4}), {2, 1}, "1001"},
		{"6l", mk_model({1, 2, 4}), {2, 2}, "11"},
		{"6m", mk_model({1, 2, 4}), {0, 0, 0}, "000000000"},
		{"6n", mk_model({1, 2, 4}), {1, 1, 1}, "0011010"},
		{"6o", mk_model({1, 2, 4}), {2, 2, 2}, "111"},
		{"6p", mk_model({1, 2, 4}), {0, 2, 1}, "000101"},
		{"6q", mk_model({1, 2, 4}), {0, 1, 2}, "0000101"},
		{"7a", mk_model({1, 3, 5, 7}), {3, 2, 1, 0}, "1010111001"},
		{"7b", mk_model({1, 3, 5, 7}), {0, 1, 2, 3}, "00000010011"},
		{"7c", mk_model({7, 5, 3, 1}), {3, 2, 1, 0}, "11111101011"},
		{"7d", mk_model({7, 5, 3, 1}), {0, 1, 2, 3}, "0101000110"},
		{"8a", mk_model({1, 255, 1}), {0, 2}, "00000000111111101"},
		{"8b", mk_model({1, 255, 1}), {2, 0}, "11111111000000001"},
	};

	bool run_tests()
	{
		bool ok = true;
		for (size_t i = 0; _countof(c_test_cases) > i; ++i)
		{
			const test_case &tc = c_test_cases[i];
			arcd_enc enc;
			std::ostringstream out;
			arcd_enc_init(&enc, getprob, const_cast<model_t *>(&tc.model),
						  output, &out);
			for (size_t k = 0; tc.in.size() > k; ++k)
			{
				arcd_enc_put(&enc, tc.in[k]);
			}
			arcd_enc_fin(&enc);
			const std::string outstr = out.str();
			if (tc.out != outstr)
			{
				fprintf(stderr, "Test case #%zu \"%s\" (encode) failed:\n",
						i, tc.name.c_str());
				fprintf(stderr, "    Actual output:   %s\n", outstr.c_str());
				fprintf(stderr, "    Expected output: %s\n", tc.out.c_str());
				ok = false;
			}
			arcd_dec dec;
			std::istringstream in(outstr);
			arcd_dec_init(&dec, getch, const_cast<model_t *>(&tc.model),
						  input, &in);
			for (size_t k = 0; tc.in.size() > k; ++k)
			{
				const arcd_char_t ch = arcd_dec_get(&dec);
				if (tc.in[k] != ch)
				{
					fprintf(stderr, "Test case #%zu \"%s\" (decode) failed at #%zu:\n",
							i, tc.name.c_str(), k);
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
