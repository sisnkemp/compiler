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

#include <err.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "comp/comp.h"
#include "comp/ir.h"

#include "lang.c/c.h"

struct memarea frontmem;

struct ir_type *builtin_va_list;
struct symbol *builtin_va_start_sym;
struct symbol *builtin_va_arg_sym;
struct symbol *builtin_va_end_sym;

static int nerrors;

static void builtin_init(void);
static void tokdump(void);

int
main(int argc, char **argv)
{
	int ch;
	int aflag = 0, gflag = 0, pflag = 0, sflag = 0, tflag = 0;

	comp_init();

	while ((ch = getopt(argc, argv, "agpst"COMPOPTS)) != -1) {
		switch (ch) {
		case 'a':
			aflag = 1;
			break;
		case 'g':
			gflag = 1;
			break;
		case 'p':
			pflag = 1;
			break;
		case 's':
			sflag = 1;
			break;
		case 't':
			tflag = 1;
			break;
		default:
			compopt(ch);
		}
	}
	argc -= optind;
	argv += optind;

	if (argc > 0) {
		setinfile(argv[0]);
		if (argc > 1)
			setoutfile(argv[1]);
	}

	mem_area_init(&frontmem);
	lexinit(infile);
	if (tflag) {
		tokdump();
		comp_exit(0);
	}

	builtin_init();
	symtabinit(0);
	parse();
	symtabinit(1);

	if (nerrors)
		comp_exit(1);
	if (aflag)
		ast_pretty("start");
	if (pflag)
		comp_exit(0);
	ast_semcheck();
	if (nerrors)
		comp_exit(1);
	if (aflag)
		ast_pretty("semcheck");
	if (sflag)
		comp_exit(0);
	ast_gencode();
	mem_area_free(&frontmem);
	compile();
	comp_exit(0);
}

static void
builtin_init(void)
{
	int i;
	char *name;
	struct ir_type *fn;
	struct ir_typelm *elm;
	struct symbol *sym;

	struct {
		char	*name;
		struct	symbol **symp;
		int	ellip;
	} va_sae[3] = {
		{ "__builtin_va_start", &builtin_va_start_sym, 1 },
		{ "__builtin_va_arg", &builtin_va_arg_sym, 1 },
		{ "__builtin_va_end", &builtin_va_end_sym, 0 }
	};

	symtab_builtin_init();
	builtin_init_machdep();

	for (i = 0; i < 3; i++) {
		name = ntenter(&names, va_sae[i].name);
		fn = ir_type_func(&cir_void);
		elm = ir_typelm(NULL, builtin_va_list, 0, 0);
		ir_type_newelm(fn, elm);
		if (va_sae[i].ellip)
			ir_type_setflags(fn, IR_ELLIPSIS);
		sym = symenter(name, fn, NSORD);
		sym->s_flags |= SYM_BUILTIN;
		*va_sae[i].symp = sym;
	}
}

static void
tokdump(void)
{
	int first = 1, i;
	size_t col;

	col = 1;
	while ((i = yylex()) != 0) {
		if (i < TOK_START) {
			if ((col += 1) >= 79) {
				fprintf(stderr, "\n");
				col = 1;
			} else if (!first) {
				col++;
				fprintf(stderr, " ");
			}
			fprintf(stderr, "%c", i);
		} else if (i <= TOK_END) {
			if ((col += strlen(toknames[i - TOK_START])) >= 79) {
				fprintf(stderr, "\n");
				col = strlen(toknames[i - TOK_START]);
			} else if (!first) {
				col++;
				fprintf(stderr, " ");
			}
			fprintf(stderr, "%s", toknames[i - TOK_START]);
		} else
			errx(1, "bad token");

		first = 0;
	}

	fprintf(stderr, "\n");
}

static void
msg(const char *what, const char *fmt, va_list ap, struct srcpos *sp)
{
	fprintf(stderr, "%s:%zu: %s: ", sp->s_file, sp->s_line, what);
	vfprintf(stderr, fmt, ap);
	fprintf(stderr, "\n");
}

void
warnh(const char *fmt, ...)
{
	va_list ap;

	va_start(ap, fmt);
	msg("warning", fmt, ap, &cursp);
	va_end(ap);
}

void
warnp(struct srcpos *sp, const char *fmt, ...)
{
	va_list ap;

	va_start(ap, fmt);
	msg("warning", fmt, ap, sp);
	va_end(ap);
}

void
errh(const char *fmt, ...)
{
	va_list ap;

	va_start(ap, fmt);
	msg("error", fmt, ap, &cursp);
	va_end(ap);
	nerrors++;
}

void
errp(struct srcpos *sp, const char *fmt, ...)
{
	va_list ap;

	va_start(ap, fmt);
	msg("error", fmt, ap, sp);
	va_end(ap);
	nerrors++;
}

void
synh(const char *fmt, ...)
{
	va_list ap;

	va_start(ap, fmt);
	msg("syntax error", fmt, ap, &cursp);
	va_end(ap);
	nerrors++;
}

void
fatalsynh(const char *fmt, ...)
{
	va_list ap;

	va_start(ap, fmt);
	msg("syntax error", fmt, ap, &cursp);
	va_end(ap);
	exit(1);
}

int
escapechar(char *p, char **ep)
{
	char *q = &p[1];
	int ch, i;

	switch (*p) {
	case '\'':
		ch = '\'';
		break;
	case '\"':
		ch = '\"';
		break;
	case '?':
		ch = '\?';
		break;
	case '\\':
		ch = '\\';
		break;
	case 'a':
		ch = '\a';
		break;
	case 'b':
		ch = '\b';
		break;
	case 'e':
		ch = '\e';
		break;
	case 'f':
		ch = '\f';
		break;
	case 'n':
		ch = '\n';
		break;
	case 'r':
		ch = '\r';
		break;
	case 't':
		ch = '\t';
		break;
	case 'v':
		ch = '\v';
		break;
	case 'x':
		ch = 0;
		for (i = 1; i <= 2; i++) {
			if (p[i] >= '0' && p[i] <= '9')
				ch = ch * 16 + p[i] - '0';
			else if (p[i] >= 'a' && p[i] <= 'f')
				ch = ch * 16 + 10 + p[i] - 'a';
			else if (p[i] >= 'A' && p[i] <= 'F')
				ch = ch * 16 + 10 + p[i] - 'A';
			else
				break;
			q++;
		}
		break;
	default:	/* must be octal escape sequence */
		if (*p < '0' || *p > '7')
			fatalx("esccon: bad input: %d(%s)", *p, p);
		ch = *p - '0';
		q++;
		for (i = 1; i <= 2; i++) {
			if (p[i] < '0' || p[i] > '7')
				break;
			q++;
			ch = ch * 8 + p[i] - '0';
		}
		break;
	}

	if (ep != NULL)
		*ep = q;
	return ch;
}

char *
cstring(char *start, char *end, struct memarea *m, size_t *lenp)
{
	char nul;
	char *p, *tmp;
	int ch, esc;
	size_t i;

	if (start > end) {
		nul = '\0';
		i = 1;
		tmp = &nul;
	} else {
		tmp = xmalloc(end - start + 2);
		tmp[0] = '\0';
		p = start;
		i = esc = 0;
		while (p <= end) {
			if (esc == 0) {
				if (*p != '\\')
					tmp[i++] = *p;
				else
					esc = 1;
				p++;
				continue;
			}

			ch = escapechar(p, &p);
			tmp[i++] = ch;
			esc = 0;
		}
		tmp[i++] = 0;
		if (esc)
			fatalx("cstring: still in escape mode: %s", start);
	}

	if (m == NULL)
		p = xmalloc(i);
	else
		p = mem_alloc(m, i);
	if (lenp != NULL)
		*lenp = i;

	memcpy(p, tmp, i);
	if (tmp != &nul)
		free(tmp);
	return p;
}

int
tyisarith(struct ir_type *ty)
{
	return tyisinteger(ty) || tyisfloating(ty);
}

int
tyisfloating(struct ir_type *ty)
{
	return IR_ISFLOATING(ty);
}

int
tyisinteger(struct ir_type *ty)
{
	return IR_ISINTEGER(ty) || IR_ISBOOL(ty);
}

int
tyisobjptr(struct ir_type *ty)
{
	if (!IR_ISPTR(ty))
		return 0;
	ty = ir_type_dequal(ty);
	if (IR_ISVOID(ty) || IR_ISFUNTY(ty))
		return 0;
	return 1;
}

int
tyiscompat(struct ir_type *lty, struct ir_type *rty)
{
	struct ir_typelm *lelm, *relm;

	if (lty == rty)
		return 1;
	if (lty->it_op != rty->it_op)
		return 0;
	if (IR_ISUNION(lty) || IR_ISSTRUCT(lty))
		fatalx("tyiscompat");	/* Should be handled by lty == rty. */
	if (IR_ISQUAL(lty))
		return tyiscompat(ir_type_dequal(lty), ir_type_dequal(rty));
	if (IR_ISPTR(lty))
		return tyiscompat(lty->it_base, rty->it_base);
	if (IR_ISARR(lty)) {
		if (IR_ISCOMPLETE(lty) && IR_ISCOMPLETE(rty)) {
			if (lty->it_dim != rty->it_dim)
				return 0;
		}
		return tyiscompat(lty->it_base, rty->it_base);
	}
	if (!IR_ISFUNTY(lty))
		fatalx("tyiscompat %d", lty->it_op);
	if (!tyiscompat(lty->it_base, rty->it_base))
		return 0;
	if (lty->it_flags & rty->it_flags & IR_KRFUNC)
		return 1;
	if (((lty->it_flags | rty->it_flags) & IR_KRFUNC) == 0) {
		if ((lty->it_flags ^ rty->it_flags) & IR_ELLIPSIS)
			return 0;

		lelm = SIMPLEQ_FIRST(&lty->it_typeq);
		relm = SIMPLEQ_FIRST(&rty->it_typeq);
		while (lelm != NULL && relm != NULL) {
			if (!tyiscompat(lelm->it_type, relm->it_type))
				return 0;
			lelm = SIMPLEQ_NEXT(lelm, it_link);
			relm = SIMPLEQ_NEXT(relm, it_link);
		}
		if (lelm != NULL || relm != NULL)
			return 0;
		return 1;
	}

	if (rty->it_flags & IR_KRFUNC)
		return tyisansikrcompat(lty, rty);
	return tyisansikrcompat(rty, lty);
}

int
tyisansikrcompat(struct ir_type *ansi, struct ir_type *kr)
{
	struct ir_type *ansity, *krty;
	struct ir_typelm *ansielm, *krelm;

	if (!(kr->it_flags & IR_FUNDEF)) {
		if (ansi->it_flags & IR_ELLIPSIS)
			return 0;
		SIMPLEQ_FOREACH(ansielm, &ansi->it_typeq, it_link) {
			ansity = ir_type_dequal(tyargconv(ansielm->it_type));
			if (!tyiscompat(ansity,
			    ir_type_dequal(ansielm->it_type)))
				return 0;
		}
		return 1;
	}

	ansielm = SIMPLEQ_FIRST(&ansi->it_typeq);
	krelm = SIMPLEQ_FIRST(&kr->it_typeq);
	while (ansielm != NULL && krelm != NULL) {
		ansielm = SIMPLEQ_NEXT(ansielm, it_link);
		krelm = SIMPLEQ_NEXT(krelm, it_link);
		krty = ir_type_dequal(tyargconv(krelm->it_type));
		ansity = ir_type_dequal(ansielm->it_type);
		if (!tyiscompat(ansity, krty))
			return 0;
	}
	if (ansielm != NULL || krelm != NULL)
		return 0;
	return 1;
}

int
tyisscalar(struct ir_type *ty)
{
	return IR_ISPTR(ty) || tyisarith(ty);
}

struct ir_type *
tyuconv(struct ir_type *ty)
{
	int qual = 0;

	if (IR_ISQUAL(ty)) {
		qual = ty->it_op;
		ty = ir_type_dequal(ty);
	}

	if (IR_ISARR(ty))
		return ir_type_qual(ir_type_ptr(ty->it_base), qual);
	if (IR_ISFUNTY(ty))
		return ir_type_qual(ir_type_ptr(ty), qual);
	if (!IR_ISINTEGER(ty))
		return ir_type_qual(ty, qual);
	if (ty->it_op < IR_I32)
		return ir_type_qual(&cir_int, qual);
	return ir_type_qual(ty, qual);
}

struct ir_type *
tybinconv(struct ir_type *l, struct ir_type *r)
{
	l = ir_type_dequal(tyuconv(l));
	r = ir_type_dequal(tyuconv(r));
	if (IR_ISPTR(l))
		return l;
	if (IR_ISPTR(r))
		return r;
	if (!tyisarith(l) || !tyisarith(r))
		fatalx("tybinconv %d, %d", l->it_op, r->it_op);

	if (l == &cir_ldouble || r == &cir_ldouble)
		return &cir_ldouble;
	if (l == &cir_double || r == &cir_double)
		return &cir_double;
	if (l == &cir_float || r == &cir_float)
		return &cir_float;
	if ((IR_ISUNSIGNED(l) && IR_ISUNSIGNED(r)) ||
	    (IR_ISSIGNED(l) && IR_ISSIGNED(r))) {
		if (l->it_op > r->it_op)
			return l;
		return r;
	}
	if (IR_ISUNSIGNED(l) && IR_ISSIGNED(r)) {
		if (r->it_op < l->it_op)
			return l;
		return r;
	}
	if (IR_ISSIGNED(l) && IR_ISUNSIGNED(r)) {
		if (l->it_op < r->it_op)
			return r;
		return l;
	}
	fatalx("tybinconv %d, %d", l->it_op, r->it_op);
	return l;
}

struct ir_type *
tyargconv(struct ir_type *ty)
{
	int qual = 0;

	if (IR_ISQUAL(ty)) {
		qual = ty->it_op;
		ty = ir_type_dequal(ty);
	}
	if (ty == &cir_float)
		return ir_type_qual(&cir_double, qual);
	return ir_type_qual(tyuconv(ty), qual);
}

struct ir_type *
tyfldpromote(struct ir_type *ty, int fldsize)
{
	int qual = 0;

	if (IR_ISQUAL(ty)) {
		qual = ty->it_op;
		ty = ir_type_dequal(ty);
	}

	if (fldsize <= 31 && ty->it_op < IR_I32)
		return ir_type_qual(&cir_int, qual);
	return ir_type_qual(ty, qual);
}
