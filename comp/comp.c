/*
 * Copyright (c) 2008 Stefan Kempf <sisnkemp@gmail.com>
 *
 * Permission to use, copy, modify, and distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
 * ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
 * ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
 * OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */

#include <sys/queue.h>

#include <ctype.h>
#include <err.h>
#include <errno.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "comp/comp.h"
#include "comp/ir.h"
#include "comp/passes.h"

#include "reg.h"

struct srcpos cursp;
char *infile = "<stdin>";
size_t xmallocd;

int Iflag;
static int Pflag;
static int Sflag;

#define P_NODUMP	1
#define P_SJMPSAFE	2

struct pass {
	void	(*p_fn)(struct passinfo *);
	char	*p_name;
	int	p_flags;
};

static struct pass intrapasses[] = {
	{ pass_deadfuncelim, "deadfuncelim" },
	{ pass_emit_header, "emit_header", P_NODUMP }
};

static struct pass interpasses[] = {
	{ pass_uce, "uce" },
	{ pass_parmfixup, "parmfixup", P_SJMPSAFE },
	{ pass_soufixup, "soufixup", P_SJMPSAFE },
	{ pass_vartoreg, "vartoreg" },

	/*
	 * In these passes the code is in SSA form and has to STAY in
	 * SSA form. Besides that, control-flow transformations have to
	 * be made carefully, so that the basic-block information stays
	 * valid.
	 */
	{ pass_ssa, "ssa" },
	{ pass_constprop, "constprop" },
	{ pass_deadvarelim, "deadvarelim" },
	{ pass_constfold, "constfold" },
	{ pass_deadcodeelim, "deadcodeelim" },
	{ pass_gencode, "gencode", P_SJMPSAFE },
	{ pass_undo_ssa, "undo_ssa" },

	{ pass_ralloc, "ralloc", P_SJMPSAFE },
	{ pass_stackoff, "stackoff", P_SJMPSAFE },
	{ pass_gencode, "gencode", P_SJMPSAFE },
	{ pass_emit_func, "emit_func", P_NODUMP | P_SJMPSAFE }
};

static size_t ninterpasses = sizeof interpasses / sizeof *interpasses;
static size_t nintrapasses = sizeof intrapasses / sizeof *intrapasses;

void
compopt(int ch)
{
	switch (ch) {
	case 'I':
		Iflag = 1;
		break;
	case 'P':
		Pflag = 1;
		break;
	case 'S':
		Sflag = 1;
		break;
	case '?':
	default:
		exit(1);
	}
}

void
compile(void)
{
	int dumpno = 1;
	size_t i, j;
	FILE *fp;
	struct ir_func *fn;
	struct passinfo pi;

	if (Iflag) {
		fp = dump_open("IR", "start", "w", 0);
		ir_dump_prog(fp, irprog);
		fclose(fp);
	}

	/*
	 * Passes that work on the whole program must go here. This could be
	 * interprocedural analysis (unlikely we'll ever to that) or inlining.
	 */
	pi.p_fn = NULL;
	for (i = 0; i < nintrapasses; i++) {
		intrapasses[i].p_fn(&pi);
		if (Iflag && !(intrapasses[i].p_flags & P_NODUMP)) {
			fp = dump_open("IR", intrapasses[i].p_name, "w", dumpno++);
			ir_dump_prog(fp, irprog);
			fclose(fp);
		}
	}

	pi.p_fn = NULL;

	/* Passes that work on one function at a time. */
	for (j = 0; !SIMPLEQ_EMPTY(&irprog->ip_funq); j++) {
		fn = SIMPLEQ_FIRST(&irprog->ip_funq);
		SIMPLEQ_REMOVE_HEAD(&irprog->ip_funq, if_link);
		irfunc = pi.p_fn = fn;
		for (i = 0; i < ninterpasses; i++) {
			if (fn->if_flags & IR_FUNC_SETJMP &&
			    !(interpasses[i].p_flags & P_SJMPSAFE))
				continue;
			interpasses[i].p_fn(&pi);
			if (Iflag && !(interpasses[i].p_flags & P_NODUMP)) {
				fp = dump_open("IR", interpasses[i].p_name,
				    j ? "a" : "w", dumpno + i);
				if (j == 0)
					ir_dump_globals(fp, irprog);
				ir_dump_func(fp, fn);
				fclose(fp);
			}
		}
		ir_func_free(fn);
	}

	irfunc = pi.p_fn = NULL;
	pass_emit_data(&pi);
}

void
comp_init(void)
{
	ntinit(&names);
	reginit();
	targinit();
}

static void
printbytes(size_t nbytes)
{
	int gap;

	gap = 0;
	if (nbytes / (1024 * 1024)) {
		fprintf(stderr, "%zuM", nbytes / (1024 * 1024));
		nbytes %= 1024 * 1024;
		gap = 1;
	}
	if (nbytes / 1024) {
		if (gap)
			fprintf(stderr, " ");
		fprintf(stderr, "%zuK", nbytes / 1024);
		nbytes %= 1024;
		gap = 1;
	}
	if (nbytes) {
		if (gap)
			fprintf(stderr, " ");
		fprintf(stderr, "%zu bytes", nbytes);
	}

}

void
comp_exit(int s)
{
	if (Sflag) {
		fprintf(stderr, "xmallocd total: ");
		printbytes(xmallocd);

		fprintf(stderr, "\nmemallocd total: ");
		printbytes(memstats.m_allocd);
		fprintf(stderr, "\nmemfreed total: ");
		printbytes(memstats.m_freed);
		fprintf(stderr, "\nmempeakusage: ");
		printbytes(memstats.m_peakusage);
		fprintf(stderr, "\nminsize: ");
		printbytes(memstats.m_minsize);
		fprintf(stderr, "\nmaxsize: ");
		printbytes(memstats.m_maxsize);
		fprintf(stderr, "\nallocs: %zu\n", memstats.m_nalloc);
		fprintf(stderr, "searches in mem_alloc: %zu\n",
		    memstats.m_nsearch);

		fprintf(stderr, "symbols: %zu\n", irstats.i_syms);
		fprintf(stderr, "expressions: %zu\n", irstats.i_exprs);
		fprintf(stderr, "instructions: %zu\n", irstats.i_insns);
		fprintf(stderr, "functions: %zu\n", irstats.i_funcs);
		fprintf(stderr, "types: %zu\n", irstats.i_types);
	}
	exit(s);
}

void
setinfile(char *path)
{
	if (freopen(path, "r", stdin) == NULL)
		err(1, "could not open input file %s", path);
	infile = path;
}

void
setoutfile(const char *path)
{
	if (freopen(path, "w", stdout) == NULL)
		err(1, "could not open output file %s", path);
}

void
fatal(const char *fmt, ...)
{
	va_list ap;
	int err = errno;

	va_start(ap, fmt);
	fprintf(stderr, "%s: fatal compiler error: ", infile);
	vfprintf(stderr, fmt, ap);
	fprintf(stderr, ": %s\n", strerror(err));
	va_end(ap);
	exit(1);
}

void
fatalx(const char *fmt, ...)
{
	va_list ap;

	va_start(ap, fmt);
	fprintf(stderr, "%s: fatal compiler error: ", infile);
	vfprintf(stderr, fmt, ap);
	fprintf(stderr, "\n");
	va_end(ap);
	exit(1);
}

void
emitf(const char *fmt, ...)
{
	va_list ap;

	va_start(ap, fmt);
	vfprintf(stdout, fmt, ap);
	va_end(ap);
}

void *
xmalloc(size_t size)
{
	void *p;

	if ((p = malloc(size)) == NULL)
		err(1, "xmalloc");
	xmallocd += size;
	return p;
}

void *
xmnalloc(size_t nmemb, size_t size)
{
	void *p;
	uintmax_t n;

	if ((n = (uintmax_t)nmemb * size) > SIZE_MAX)
		fatalx("xmnalloc");
	if ((p = malloc(n)) == NULL)
		err(1, "xmnalloc");
	xmallocd += n;
	return p;

}

void *
xcalloc(size_t nmemb, size_t size)
{
	void *p;

	if ((p = calloc(nmemb, size)) == NULL)
		err(1, "xcalloc");
	xmallocd += nmemb * size;
	return p;
}

void *
xrealloc(void *ptr, size_t size)
{
	void *p;

	if ((p = realloc(ptr, size)) == NULL)
		err(1, "realloc");
	xmallocd += size;
	return p;
}

char *
xstrdup(const char *str)
{
	char *s;

	if ((s = strdup(str)) == NULL)
		err(1, "strdup");
	return s;
}

int
newid(void)
{
	static int id = REG_NREGS;

	return ++id;
}

static int8_t bv_firstbit[16] = {
	-1,	/* 0000 */
	0,	/* 0001 */
	1,	/* 0010 */
	0,	/* 0011 */
	2,	/* 0100 */
	0,	/* 0101 */
	1,	/* 0110 */
	0,	/* 0111 */
	3,	/* 1000 */
	0,	/* 1001 */
	1,	/* 1010 */
	0,	/* 1011 */
	2,	/* 1100 */
	0,	/* 1101 */
	1,	/* 1110 */
	0	/* 1111 */
};

struct bitvec *
bitvec_alloc(struct memarea *ma, size_t nbit)
{
	size_t nelem;
	uintmax_t size;
	struct bitvec *bv;

	nelem = BITVEC_BITSTOELEMS(nbit);
	size = sizeof *bv + (uintmax_t)(nelem - 1) * sizeof bv->b_bits[0];
	if (size > SIZE_MAX)
		fatalx("bitvec_alloc");
	if (ma == NULL)
		bv = xmalloc(size);
	else
		bv = mem_alloc(ma, size);
	bitvec_init(bv, nbit);
	return bv;
}

void
bitvec_init(struct bitvec *bv, size_t nbit)
{
	bv->b_nbit = nbit;
	bv->b_nelem = BITVEC_BITSTOELEMS(nbit);
	bitvec_clearall(bv);
}

int
bitvec_isset(struct bitvec *bv, size_t bit)
{
	size_t bitpos, elmpos;

	if (bit >= bv->b_nbit)
		fatalx("bitvec_isset");

	elmpos = bit >> BITVEC_BITSPERELEM_SHIFT;
	bitpos = bit & (BITVEC_BITSPERELEM - 1);
	return (bv->b_bits[elmpos] & (1U << bitpos)) != 0;
}

size_t
bitvec_firstset(struct bitvec *bv)
{
	u_int val;
	size_t i, j, k, mask;

	for (i = j = 0; i < bv->b_nelem; i++, j += BITVEC_BITSPERELEM) {
		if ((val = bv->b_bits[i]) != 0) {
			k = 16;
			mask = 0xffff;
			while (!(val & 0xf)) {
				if (!(val & mask)) {
					j += k;
					val >>= k;
				}
				k >>= 1;
				mask >>= k;
			}
			return j + bv_firstbit[val & 0xf];
		}
	}
	return bv->b_nbit;
}

size_t
bitvec_nextset(struct bitvec *bv, size_t last)
{
	u_int val;
	size_t i, k, mask, shift;

	last++;
	i = last >> BITVEC_BITSPERELEM_SHIFT;
	shift = (last & (BITVEC_BITSPERELEM - 1));
	for (; i < bv->b_nelem; last = ++i << BITVEC_BITSPERELEM_SHIFT) {
		if ((val = bv->b_bits[i] >> shift) != 0) {
			k = 16;
			mask = 0xffff;
			while (!(val & 0xf)) {
				if (!(val & mask)) {
					last += k;
					val >>= k;
				}
				k >>= 1;
				mask >>= k;
			}
			return last + bv_firstbit[val & 0xf];
		}
		shift = 0;
	}
	return bv->b_nbit;
}

void
bitvec_clearbit(struct bitvec *bv, size_t bit)
{
	size_t bitpos, elmpos;

	if (bit >= bv->b_nbit)
		fatalx("bitvec_clearbit");

	elmpos = bit / BITVEC_BITSPERELEM;
	bitpos = bit % BITVEC_BITSPERELEM;
	bv->b_bits[elmpos] &= ~(1U << bitpos);
}

void
bitvec_clearall(struct bitvec *bv)
{
	memset(bv->b_bits, 0, bv->b_nelem * sizeof *bv->b_bits);
}

void
bitvec_setbit(struct bitvec *bv, size_t bit)
{
	size_t bitpos, elmpos;

	if (bit >= bv->b_nbit)
		fatalx("bitvec_setbit");

	elmpos = bit / BITVEC_BITSPERELEM;
	bitpos = bit % BITVEC_BITSPERELEM;
	bv->b_bits[elmpos] |= 1U << bitpos;
}

void
bitvec_setall(struct bitvec *bv)
{
	memset(bv->b_bits, 0xff, bv->b_nelem * sizeof *bv->b_bits);
}

void
bitvec_not(struct bitvec *bv)
{
	size_t i;

	for (i = 0; i < bv->b_nelem; i++)
		bv->b_bits[i] = ~bv->b_bits[i];
}

void
bitvec_and(struct bitvec *dst, struct bitvec *src)
{
	u_int *p, *q;
	size_t i;

	if (dst->b_nbit != src->b_nbit)
		fatalx("bitvec_and");

	p = dst->b_bits;
	q = src->b_bits;
	for (i = 0; i < dst->b_nelem; i++)
		*p++ &= *q++;
}

void
bitvec_or(struct bitvec *dst, struct bitvec *src)
{
	u_int *p, *q;
	size_t i;

	if (dst->b_nbit != src->b_nbit)
		fatalx("bitvec_or");

	p = dst->b_bits;
	q = src->b_bits;
	for (i = 0; i < dst->b_nelem; i++)
		*p++ |= *q++;
}

int
bitvec_cmp(struct bitvec *p, struct bitvec *q)
{
	size_t i;

	if (p->b_nbit != q->b_nbit)
		fatalx("bitvec_cmp");

	for (i = 0; i < p->b_nelem; i++) {
		if (p->b_bits[i] != q->b_bits[i])
			return 1;
	}
	return 0;
}

void
bitvec_cpy(struct bitvec *dst, struct bitvec *src)
{
	if (dst->b_nbit != src->b_nbit)
		fatalx("bitvec_cpy");
	memcpy(dst->b_bits, src->b_bits, dst->b_nelem * sizeof *dst->b_bits);
}

void
emitcstring(FILE *fp, char *str, size_t size)
{
	size_t i;

	fprintf(fp, "\"");
	for (i = 0; i < size - 1; i++) {
		switch (str[i]) {
		case '\'':
			fputs("\\'", fp);
			break;
		case '\"':
			fputs("\\\"", fp);
			break;
		case '\?':
			fputs("\\?", fp);
			break;
		case '\\':
			fputs("\\\\", fp);
			break;
		case '\a':
			fputs("\\a", fp);
			break;
		case '\b':
			fputs("\\b", fp);
			break;
		case '\e':
			fputs("\\e", fp);
			break;
		case '\f':
			fputs("\\f", fp);
			break;
		case '\n':
			fputs("\\n", fp);
			break;
		case '\r':
			fputs("\\r", fp);
			break;
		case '\t':
			fputs("\\t", fp);
			break;
		case '\v':
			fputs("\\v", fp);
			break;
		default:
			if (isprint(str[i]))
				fputc(str[i], fp);
			else
				fprintf(fp, "\\x%.2x", str[i] & 0xff);
		}
	}
	fputs("\"", fp);
}

FILE *
dump_open(const char *prefix, const char *name, const char *mode, int dumpno)
{
	char *buf = NULL, *p;
	FILE *fp;

	if ((p = strrchr(infile, '/')) == NULL)
		p = infile;
	else
		p++;
	if (asprintf(&buf, "%s.%s.%.4d.%s", prefix, p, dumpno, name) == -1)
		fatalx("ir_dump_open: asprintf");
	if ((fp = fopen(buf, mode)) == NULL)
		fatal("ir_dump_open: fopen");
	free(buf);
	return fp;
}	

void
regset_init(struct regset *rs)
{
	bitvec_init((struct bitvec *)rs, REG_NREGS);
}

void
regset_addreg(struct regset *rs, int reg)
{
	bitvec_setbit((struct bitvec *)rs, reg);
}
