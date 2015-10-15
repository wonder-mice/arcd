#include <assert.h>
#include <stdio.h>
#include <unistd.h>
#include <arcd.h>

namespace
{
	struct adaptive_model
	{
		unsigned size;
		unsigned short *freq;
	};

	void adaptive_model_create(adaptive_model *const m, const unsigned size)
	{
		assert(ARCD_FREQ_MAX >= size);
		m->size = size + 1;
		m->freq = (unsigned short *)malloc(sizeof(m->freq[0]) * m->size);
		for (unsigned i = 0; m->size > i; ++i)
		{
			m->freq[i] = i;
		}
	}

	void adaptive_model_free(adaptive_model *const m)
	{
		free(m->freq);
	}

	void _adaptive_model_update(adaptive_model *const m, const arcd_char_t ch)
	{
		for (unsigned i = ch + 1; m->size > i; ++i)
		{
			++m->freq[i];
		}
		if (ARCD_FREQ_MAX < m->freq[m->size - 1])
		{
			for (unsigned i = 1; m->size > i; ++i)
			{
				if (1 < m->freq[i] - m->freq[i - 1])
				{
					m->freq[i] /= 2;
				}
			}
		}
	}

	void adaptive_model_getprob(arcd_char_t ch, arcd_prob *prob, void *model)
	{
		adaptive_model *const m = (adaptive_model *)model;
		prob->lower = m->freq[ch];
		prob->upper = m->freq[ch + 1];
		prob->total = m->freq[m->size - 1];
		_adaptive_model_update(m, ch);
	}

	arcd_char_t adaptive_model_getch(arcd_range_t v, arcd_range_t range,
									 arcd_prob *prob, void *model)
	{
		adaptive_model *const m = (adaptive_model *)model;
		const unsigned end = m->size - 1;
		const arcd_freq_t freq = arcd_freq_scale(v, range, m->freq[end]);
		for (unsigned i = 0; end > i; ++i)
		{
			if (m->freq[i] <= freq && freq < m->freq[i + 1])
			{
				prob->lower = m->freq[i];
				prob->upper = m->freq[i + 1];
				prob->total = m->freq[end];
				_adaptive_model_update(m, i);
				return i;
			}
		}
		assert(!"Bad range");
	}

	void usage(FILE *const out)
	{
		fprintf(out, "Usage:\n");
		fprintf(out, "    arcd_stream [-e | -d | -h]\n\n");
		fprintf(out, "-e - encode\n");
		fprintf(out, "-d - decode\n");
		fprintf(out, "-h - help\n");
		fflush(out);
	}
}

int main(int argc, char *argv[])
{
	if (2 != argc)
	{
		usage(stderr);
		return 1;
	}
	if (0 == strcmp("-h", argv[1]))
	{
		usage(stdout);
		return 0;
	}
	FILE *const in = fdopen(dup(fileno(stdin)), "rb");
	FILE *const out = fdopen(dup(fileno(stdout)), "wb");
	adaptive_model model;
	adaptive_model_create(&model, (2 << 8) + 1);
	if (0 == strcmp("-e", argv[1]))
	{
		arcd_enc enc;
		arcd_enc_init(&enc, adaptive_model_getprob, 0, output, out);
		unsigned char ch;
		while (0 < fread(&ch, 1, 1, in))
		{
			arcd_enc_put(&enc, ch);
		}
		arcd_enc_put(&enc, 2 << 8);
		arcd_enc_fin(&enc);
	}
	else if (0 == strcmp("-d", argv[1]))
	{
		arcd_dec dec;
		arcd_dec_init(&dec, adaptive_model_getch, 0, input, in);
		unsigned char ch;
		while (0 < fread(&ch, 1, 1, in))
		{
			const int sym = arcd_dec_get(&dec);
			if (2 << 8 == sym)
			{
				break;
			}
			ch = (unsigned char)sym;
			fwrite(&ch, 1, 1, out);
		}
	}
	adaptive_model_free(&model);
	fclose(in);
	fclose(out);
	return 0;
}
