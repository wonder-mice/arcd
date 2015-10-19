#include <string.h>
#include <stdio.h>
#include <unistd.h>
#include <arcd.h>
#include <adaptive_model.h>

void output(const arcd_buf_t buf, const unsigned buf_bits, void *const io)
{
	(void)buf_bits;
	FILE *const f = (FILE *)io;
	fwrite(&buf, sizeof(buf), 1, f);
}

unsigned input(arcd_buf_t *const buf, void *const io)
{
	FILE *const f = (FILE *)io;
	return 8 * fread(buf, sizeof(*buf), 1, f);
}

void usage(FILE *const out)
{
	fprintf(out, "Usage:\n");
	fprintf(out, "    arcd_stream [-e | -d | -h]\n\n");
	fprintf(out, "-e - encode stdin to stdout\n");
	fprintf(out, "-d - decode stdin to stdout\n");
	fprintf(out, "-h - help\n\n");
	fflush(out);
}

typedef unsigned char symbol_t;
static const arcd_char_t EOS = 1 << (8 * sizeof(symbol_t));

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
	adaptive_model_create(&model, EOS + 1);
	if (0 == strcmp("-e", argv[1]))
	{
		arcd_enc enc;
		arcd_enc_init(&enc, adaptive_model_getprob, &model, output, out);
		symbol_t sym;
		while (0 < fread(&sym, sizeof(sym), 1, in))
		{
			arcd_enc_put(&enc, sym);
		}
		arcd_enc_put(&enc, EOS);
		arcd_enc_fin(&enc);
	}
	else if (0 == strcmp("-d", argv[1]))
	{
		arcd_dec dec;
		arcd_dec_init(&dec, adaptive_model_getch, &model, input, in);
		arcd_char_t ch;
		while (EOS != (ch = arcd_dec_get(&dec)))
		{
			const symbol_t sym = (unsigned char)ch;
			fwrite(&sym, sizeof(sym), 1, out);
		}
	}
	adaptive_model_free(&model);
	fclose(in);
	fclose(out);
	return 0;
}
