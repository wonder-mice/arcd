#include <assert.h>
#include <stdlib.h>
#include "adaptive_model.h"

static void update(adaptive_model *const m, const arcd_char_t ch)
{
	assert(0 <= ch && ch < m->size - 1);
	for (unsigned i = ch + 1; m->size > i; ++i)
	{
		++m->freq[i];
	}
	if (ARCD_FREQ_MAX > m->freq[m->size - 1])
	{
		return;
	}
	unsigned base = 0;
	for (unsigned i = 1; m->size > i; ++i)
	{
		unsigned d = m->freq[i] - base;
		if (1 < d)
		{
			d /= 2;
		}
		base = m->freq[i];
		m->freq[i] = m->freq[i - 1] + d;
	}
}

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

void adaptive_model_getprob(const arcd_char_t ch, arcd_prob *const prob,
							void *const model)
{
	adaptive_model *const m = (adaptive_model *)model;
	prob->lower = m->freq[ch];
	prob->upper = m->freq[ch + 1];
	prob->total = m->freq[m->size - 1];
	update(m, ch);
}

arcd_char_t adaptive_model_getch(const arcd_range_t v, const arcd_range_t range,
								 arcd_prob *const prob, void *const model)
{
	adaptive_model *const m = (adaptive_model *)model;
	const unsigned last = m->size - 1;
	const arcd_freq_t freq = arcd_freq_scale(v, range, m->freq[last]);
	for (unsigned i = 0; last > i; ++i)
	{
		if (m->freq[i] <= freq && freq < m->freq[i + 1])
		{
			prob->lower = m->freq[i];
			prob->upper = m->freq[i + 1];
			prob->total = m->freq[last];
			update(m, i);
			return i;
		}
	}
	assert(!"Bad range");
	return -1;
}
