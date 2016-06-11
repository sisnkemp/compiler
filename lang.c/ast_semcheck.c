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

#include <sys/types.h>
#include <sys/queue.h>

#include <stdio.h>

#include "comp/comp.h"
#include "comp/ir.h"

#include "lang.c/c.h"

#define TS_NONE		0
#define TS_VOID		1
#define TS_SGN		2
#define TS_USGN		3
#define TS_CHAR		4
#define TS_SHORT	5
#define TS_INT		6
#define TS_LONG		7
#define TS_FLOAT	8
#define TS_DOUBLE	9
#define TS_BOOL		10
#define TS_SIMPLEMAX	10

#define TS_SCHAR	11
#define TS_UCHAR	12
#define TS_SHINT	13
#define TS_SSHORT	14
#define TS_SSHINT	15
#define TS_USHORT	16
#define TS_USHINT	17
#define TS_SINT		18
#define TS_UINT		19
#define TS_LONGINT	20
#define TS_SLONG	21
#define TS_SLONGINT	22
#define TS_ULONG	23
#define TS_ULONGINT	24
#define TS_LLONG	25
#define TS_LLONGINT	26
#define TS_SLLONG	27
#define TS_SLLONGINT	28
#define TS_ULLONG	29
#define TS_ULLONGINT	30
#define TS_LDOUBLE	31
#define TS_MAX		32

/* TODO: Could be compressed. */
static signed char tsmatrix[TS_MAX][TS_SIMPLEMAX] = {
	{ TS_VOID, TS_SGN, TS_USGN, TS_CHAR, TS_SHORT, TS_INT,
	  TS_LONG, TS_FLOAT, TS_DOUBLE, TS_BOOL },	/* TS_NONE */
	{ 0 },						/* TS_VOID */
	{ 0, 0, 0, TS_SCHAR, TS_SSHORT, TS_SINT,
	  TS_SLONG },					/* TS_SGN */
	{ 0, 0, 0, TS_UCHAR, TS_USHORT, TS_UINT,
	  TS_ULONG  },					/* TS_USGN */
	{ 0, TS_SCHAR, TS_UCHAR },			/* TS_CHAR */
	{ 0, TS_SSHORT, TS_USHORT, 0, 0, TS_SHINT },	/* TS_SHORT */
	{ 0, TS_SINT, TS_UINT, 0, TS_SHINT, 0,
	  TS_LONGINT },					/* TS_INT */
	{ 0, TS_SLONG, TS_ULONG, 0, 0, TS_LONGINT, TS_LLONG, 0,
	  TS_LDOUBLE },					/* TS_LONG */
	{ 0 },						/* TS_FLOAT */
	{ 0, 0, 0, 0, 0, 0, TS_LDOUBLE },		/* TS_DOUBLE */	
	{ 0 },						/* TS_BOOL */
	{ 0 },						/* TS_SCHAR */
	{ 0 },						/* TS_UCHAR */
	{ 0, TS_SSHINT, TS_USHINT },			/* TS_SHINT */
	{ 0, 0, 0, 0, 0, TS_SSHINT },			/* TS_SSHORT */
	{ 0 },						/* TS_SSHINT */
	{ 0, 0, 0, 0, 0, TS_USHINT },			/* TS_USHORT */
	{ 0 },						/* TS_USHINT */
	{ 0, 0, 0, 0, TS_SSHINT, 0, TS_SLONGINT },	/* TS_SINT */
	{ 0, 0, 0, 0, TS_USHINT, 0, TS_ULONGINT },	/* TS_UINT */
	{ 0, TS_SLONGINT, TS_ULONGINT, 0, 0, 0,
	  TS_LLONGINT },				/* TS_LONGINT */
	{ 0, 0, 0, 0, 0, TS_SLONGINT, TS_SLLONG },	/* TS_SLONG */
	{ 0, 0, 0, 0, 0, 0, TS_SLLONGINT },		/* TS_SLONGINT */
	{ 0, 0, 0, 0, 0, TS_ULONGINT, TS_ULLONG },	/* TS_ULONG */
	{ 0, 0, 0, 0, 0, 0, TS_ULLONGINT },		/* TS_ULONGINT */
	{ 0, TS_SLLONG, TS_ULLONG, 0, 0, TS_LLONGINT },	/* TS_LLONG */
	{ 0, TS_SLLONGINT, TS_ULLONGINT },		/* TS_LLONGINT */

	{ 0, 0, 0, 0, 0, 0, TS_SLLONGINT },		/* TS_SLLONG */
	{ 0 },						/* TS_SLLONGINT */
	{ 0, 0, 0, 0, 0, TS_ULLONGINT },		/* TS_ULLONG */
	{ 0 },						/* TS_ULLONGINT */
	{ 0 },						/* TS_LDOUBLE */
};

#ifdef C_FIELD_UNSIGNED
#define cir_fchar	cir_uchar
#define cir_fshort	cir_ushort
#define cir_fint	cir_uint
#define cir_flong	cir_ulong
#define cir_fllong	cir_ullong
#else
#define cir_fchar	cir_schar
#define cir_fshort	cir_short
#define cir_fint	cir_int
#define cir_flong	cir_long
#define cir_fllong	cir_llong
#endif

static struct ir_type *typematrix[TS_MAX - 1][2] = {
	{ &cir_void, NULL },		/* TS_VOID */
	{ &cir_int, &cir_int },		/* TS_SGN */
	{ &cir_uint, &cir_uint },	/* TS_USGN */
	{ &cir_char, &cir_fchar },	/* TS_CHAR */
	{ &cir_short, &cir_fshort },	/* TS_SHORT */
	{ &cir_int, &cir_fint },	/* TS_INT */
	{ &cir_long, &cir_flong },	/* TS_LONG */
	{ &cir_float, NULL },		/* TS_FLOAT */
	{ &cir_double, NULL },		/* TS_DOUBLE */
	{ &cir_bool, &cir_bool },	/* TS_BOOL */
	{ &cir_schar, &cir_schar },	/* TS_SCHAR */
	{ &cir_uchar, &cir_uchar },	/* TS_UCHAR */
	{ &cir_short, &cir_fshort },	/* TS_SHINT */
	{ &cir_short, &cir_short },	/* TS_SSHORT */
	{ &cir_short, &cir_short },	/* TS_SSHINT */
	{ &cir_ushort, &cir_ushort },	/* TS_USHORT */
	{ &cir_ushort, &cir_ushort },	/* TS_USHINT */
	{ &cir_int, &cir_int },		/* TS_SINT */
	{ &cir_uint, &cir_uint },	/* TS_UINT */
	{ &cir_long, &cir_flong },	/* TS_LONGINT */
	{ &cir_long, &cir_long },	/* TS_SLONG */
	{ &cir_long, &cir_long },	/* TS_SLONGINT */
	{ &cir_ulong, &cir_ulong },	/* TS_ULONG */
	{ &cir_ulong, &cir_ulong },	/* TS_ULONGINT */
	{ &cir_llong, &cir_fllong },	/* TS_LLONG */
	{ &cir_llong, &cir_fllong },	/* TS_LLONGINT */
	{ &cir_llong, &cir_llong },	/* TS_SLLONG */
	{ &cir_llong, &cir_llong },	/* TS_SLLONGINT */
	{ &cir_ullong, &cir_ullong },	/* TS_ULLONG */
	{ &cir_ullong, &cir_ullong },	/* TS_ULLONGINT */
	{ &cir_double, &cir_double },	/* TS_LDOUBLE */
};

static struct symbol *lastpar;
static struct ir_type cir_enum;
static struct ir_type cir_id;
static int ininline;

/* Further constraints when checking expressions. */
#define EXPR_INT	0x001
#define EXPR_CON	0x002
#define EXPR_ADDRCON	0x004
#define EXPR_INADDR	0x008
#define EXPR_INSZOF	0x010
#define EXPR_NOSZOF	0x020
#define EXPR_INCALL	0x040
#define EXPR_VAARG	0x080
#define EXPR_ASG	0x100

#define FLG_INSOU	1
#define FLG_INFLD	2
#define FLG_FUNDEF	4
#define FLG_INLINE	8
#define FLG_CANCONT	16
#define FLG_CANBRK	32

#define ISCON(x)	((x)->ae_op == AST_ICON || (x)->ae_op == AST_FCON)

static int cqualtoirqual(int);
static int toktotcode(int);
static int getfldsize(struct ast_expr *);
static size_t getarrdim(struct ast_expr *);
static int getenumval(struct ast_expr *);

static int canassign(struct ir_type *, struct ast_expr *, int);
static int islvalue(struct ast_expr *);

static struct symbol *decglobl(struct ast_decl *, char *, struct ir_type *);
static struct symbol *declocal(struct ast_decl *, char *, struct ir_type *,
    int);

static int funcdecl_semcheck(struct ast_decl *);
static void decl_semcheck(struct ast_decl *, int);
static struct ir_type *declspecs_semcheck(struct ast_declspecs *, int, int *);
static struct ir_type *tyspec_semcheck(struct ast_declspecs *, int, int *);
static int simplespec_semcheck(struct ast_tyspec *, int);
static struct ir_type *tdnamespec_semcheck(struct ast_tyspec *);
static struct ir_type *souspec_semcheck(struct ast_tyspec *, int *);
static struct ir_type *enumspec_semcheck(struct ast_tyspec *, int *);
static struct ir_type *decla_semcheck(struct ast_decla *, struct ir_type *,
    char **, int);

struct initctx {
	struct	ir_type *i_curobj;
	struct	ir_type *i_atobj;
	struct	ast_init *i_init;
	size_t	i_bitoff;
	size_t	i_bitsize;
	int	i_level;
};

static void initctx_advance(struct initctx *);
static int designator_semcheck(struct initctx *);
static int initexpr_semcheck(struct ast_init *);
static int arrinit_semcheck(struct initctx *);
static int souinit_semcheck(struct initctx *);
static int simpleinit_semcheck(struct initctx *);
static int doinit_semcheck(struct initctx *);
static void init_semcheck(struct ir_type *, struct ast_init *);

static int ident_semcheck(struct ast_expr **, int);
static int subscr_semcheck(struct ast_expr **, int);
static int builtin_semcheck(struct ast_expr *x, int);
static int call_semcheck(struct ast_expr **, int);
static int souref_semcheck(struct ast_expr **, int);
static int incdec_semcheck(struct ast_expr **, int);
static int addrof_semcheck(struct ast_expr **, int);
static int deref_semcheck(struct ast_expr **, int);
static int sizeof_semcheck(struct ast_expr **, int);
static int cast_semcheck(struct ast_expr **, int);
static int uplusminus_semcheck(struct ast_expr **, int);
static int not_semcheck(struct ast_expr **, int);
static int mul_semcheck(struct ast_expr **, int);
static int add_semcheck(struct ast_expr **, int);
static int shift_semcheck(struct ast_expr **, int);
static int rel_semcheck(struct ast_expr **, int);
static int eq_semcheck(struct ast_expr **, int);
static int bw_semcheck(struct ast_expr **, int);
static int bool_semcheck(struct ast_expr **, int);
static int cond_semcheck(struct ast_expr **, int);
static int asg_semcheck(struct ast_expr **, int);
static int comma_semcheck(struct ast_expr **, int);
static int expr_semcheck(struct ast_expr **, int);

static void paramlist_semcheck(struct ast_list *, struct ir_type *, int);

static void compound_semcheck(struct ir_type *, struct ast_stmt *,
    struct ast_stmt *, int);
static void stmt_semcheck(struct ir_type *, struct ast_stmt *,
    struct ast_stmt *, int);

static int
cqualtoirqual(int cqual)
{
	int irqual = 0;

	if (cqual & AST_TQ_CONST)
		irqual |= IR_CONST;
	if (cqual & AST_TQ_VOLATILE)
		irqual |= IR_VOLAT;

	return irqual;
}

static int
toktotcode(int tok)
{
	switch (tok) {
	case TOK_VOID:
		return TS_VOID;
	case TOK_SIGNED:
		return TS_SGN;
	case TOK_UNSIGNED:
		return TS_USGN;
	case TOK_CHAR:
		return TS_CHAR;
	case TOK_SHORT:
		return TS_SHORT;
	case TOK_INT:
		return TS_INT;
	case TOK_LONG:
		return TS_LONG;
	case TOK_FLOAT:
		return TS_FLOAT;
	case TOK_DOUBLE:
		return TS_DOUBLE;
	case TOK_BOOL:
		return TS_BOOL;
	default:
		fatalx("toktotcode: bad token: %d", tok);
	}
	return 0;
}

static int
getfldsize(struct ast_expr *x)
{
	if (IR_ISSIGNED(x->ae_type)) {
		if (x->ae_con.ic_icon < 0 || x->ae_con.ic_icon > C_INT_MAX) {
			errp(&x->ae_sp, "illegal bitfield size");
			return 1;
		}
		return x->ae_con.ic_icon;
	} else if (x->ae_con.ic_ucon > C_INT_MAX) {
		errp(&x->ae_sp, "illegal bitfield size");
		return 1;
	}
	return x->ae_con.ic_ucon;
}

static size_t
getarrdim(struct ast_expr *x)
{
	if (IR_ISSIGNED(x->ae_type)) {
		if (x->ae_con.ic_icon < 0 || x->ae_con.ic_icon > C_ULONG_MAX) {
			errp(&x->ae_sp, "illegal array size");
			return 1;
		}
		return x->ae_con.ic_icon;
	} else if (x->ae_con.ic_ucon > C_ULONG_MAX) {
		errp(&x->ae_sp, "illegal array size");
		return 1;
	}
	return x->ae_con.ic_ucon;
}

static int
getenumval(struct ast_expr *x)
{
	if (IR_ISSIGNED(x->ae_type)) {
		if (x->ae_con.ic_icon < C_INT_MIN ||
		    x->ae_con.ic_icon > C_INT_MAX) {
			errp(&x->ae_sp, "illegal enumeration constant");
			return 1;
		}
		return x->ae_con.ic_icon;
	} else if (x->ae_con.ic_ucon > C_INT_MAX) {
		errp(&x->ae_sp, "illegal enumeration constant");
		return 1;
	}
	return x->ae_con.ic_ucon;
}

static int
canassign(struct ir_type *dst, struct ast_expr *x, int dequal)
{
	int dqual = 0, squal = 0;
	struct ir_type *src, *udst, *usrc;

	src = x->ae_type;
	if ((udst = ir_type_dequal(dst)) != dst)
		dqual = dst->it_op;
	if ((usrc = ir_type_dequal(src)) != src)
		squal = src->it_op;

	if (dqual & IR_CONST)
		return 0;
	if (tyisarith(udst) && tyisarith(usrc))
		return 1;
	if (IR_ISBOOL(udst) && IR_ISPTR(usrc))
		return 1;
	if (IR_ISPTR(udst)) {
		if (IR_ISFUNTY(usrc))
			return tyiscompat(udst->it_base, usrc);
		if (x->ae_op == AST_ICON) {
			if (IR_ISSIGNED(usrc) && x->ae_con.ic_icon == 0)
				return 1;
			if (IR_ISUNSIGNED(usrc) && x->ae_con.ic_ucon == 0)
				return 1;
			return 0;
		}
		if (!IR_ISPTR(usrc) && !IR_ISARR(usrc))
			return 0;
		if (IR_ISVOID(udst->it_base))
			return 1;
		if (IR_ISPTR(usrc) && IR_ISVOID(usrc->it_base))
			return 1;
		if (dequal) {
			usrc = ir_type_dequal(usrc->it_base);
			udst = ir_type_dequal(udst->it_base);
		} else {
			usrc = usrc->it_base;
			udst = udst->it_base;
		}
		return tyiscompat(udst, usrc);
	}

	return tyiscompat(udst, usrc);
}

static int
islvalue(struct ast_expr *x)
{
	switch (x->ae_op) {
	case AST_IDENT:
		if (!IR_ISARR(x->ae_type) && !IR_ISFUNTY(x->ae_type))
			return 1;
		return 0;
	case AST_STRLIT:
	case AST_SUBSCR:
	case AST_DEREF:
	case AST_SOUDIR:
	case AST_SOUIND:
		return 1;
	default:
		return 0;
	}
}
static struct symbol *
decglobl(struct ast_decl *decl, char *ident, struct ir_type *type)
{
	int sclass = AST_SC_EXTERN;
	struct symbol *sym;

	if (decl->ad_ds->ad_fnspec & AST_FS_INLINE)
		fatalx("decglobl: fnspec not yet");
	if (decl->ad_ds->ad_sclass)
		sclass = decl->ad_ds->ad_sclass;
	if ((sym = symlookup(ident, NSORD)) == NULL) {
		sym = symenter(ident, type, NSORD);
		if (decl->ad_ds->ad_sclass == 0 && !IR_ISFUNTY(type))
			sym->s_flags |= SYM_TENTDEF;
		sym->s_sclass = sclass;
		sym->s_fnspec = decl->ad_ds->ad_fnspec;
		return sym;
	}

	if (sclass == AST_SC_EXTERN && sym->s_sclass == AST_SC_STATIC)
		sclass = AST_SC_STATIC;

	if (!tyiscompat(type, sym->s_type)) {
		errp(&decl->ad_sp, "invalid redeclaration of `%s'", ident);
		return sym;
	}

	if (IR_ISARR(sym->s_type) && !IR_ISCOMPLETE(sym->s_type)) {
		if (IR_ISQUAL(sym->s_type))
			fatalx("decglobl: qualified array");
		ir_type_setflags(sym->s_type, IR_COMPLETE);
		sym->s_type->it_dim = type->it_dim;
		sym->s_type->it_size = type->it_size;
	} else if (IR_ISFUNTY(sym->s_type) &&
	    (sym->s_type->it_flags & IR_KRFUNC)) {
		if (!(type->it_flags & IR_KRFUNC))
			sym->s_type = type;
	}
	sym->s_sclass = sclass;
	return sym;
}

static struct symbol *
declocal(struct ast_decl *decl, char *ident, struct ir_type *type, int hasinit)
{
	int compl, sclass = AST_SC_AUTO;
	struct symbol *sym;

	if (decl->ad_ds->ad_sclass)
		sclass = decl->ad_ds->ad_sclass;
	if (IR_ISFUNTY(type) && sclass != AST_SC_STATIC)
		sclass = AST_SC_EXTERN;
	if (decl->ad_ds->ad_fnspec)
		fatalx("declocal: fnspec %d for %s not yet",
		    decl->ad_ds->ad_fnspec, ident);

	if (IR_ISARR(type) && hasinit)
		compl = 1;
	else
		compl = IR_ISCOMPLETE(type);
	if (sclass != AST_SC_EXTERN && !compl) {
		errp(&decl->ad_sp, "`%s' has an incomplete type", ident);
		return NULL;
	}

	if ((sym = symlookup(ident, NSORD)) != NULL) {
		if (sym->s_scope == 1)
			warnp(&decl->ad_sp, "declaration of `%s' shadows a "
			    "parameter", ident);
		else if (sym->s_scope == 0 && sclass == AST_SC_EXTERN) {
			if (!tyiscompat(sym->s_type, type))
				errp(&decl->ad_sp, "incompatible redeclaration "
				    "of `%s'", ident);
			return sym;
		}
		errp(&decl->ad_sp, "redeclaration of `%s'", ident);
		return sym;
	}

	sym = symenter(ident, type, NSORD);
	if (sclass != AST_SC_EXTERN && sclass != AST_SC_TYPEDEF)
		sym->s_flags |= SYM_DEF;
	sym->s_sclass = sclass;
	sym->s_fnspec = decl->ad_ds->ad_fnspec;
	return sym;
}

void
ast_semcheck(void)
{
	struct ast_decl *decl;

	SIMPLEQ_FOREACH(decl, &ast_program, ad_link)
		decl_semcheck(decl, 0);
}

static int
funcdecl_semcheck(struct ast_decl *decl)
{
	struct ast_decla *decla;

	if (decl->ad_ds->ad_sclass == AST_SC_TYPEDEF && decl->ad_stmt != NULL) {
		errp(&decl->ad_ds->ad_sp, "invalid storage "
		    "class for function type");
		decl->ad_ds->ad_sclass = 0;
	} else if (decl->ad_ds->ad_sclass &&
	    decl->ad_ds->ad_sclass != AST_SC_EXTERN &&
	    decl->ad_ds->ad_sclass != AST_SC_STATIC &&
	    decl->ad_ds->ad_sclass != AST_SC_TYPEDEF) {
		errp(&decl->ad_ds->ad_sp, "invalid storage "
		    "class for function type");
		decl->ad_ds->ad_sclass = 0;
	}

	if (decl->ad_stmt == NULL)
		return 1;

	/*
	 * Prevent stuff like typedef int foo(void);
	 * foo f {}
	 */
	for (decla = SIMPLEQ_FIRST(&decl->ad_declas); decla != NULL;
	    decla = decla->ad_decla) {
		if (decla->ad_op == AST_IDDECLA)
			break;
		if (decla->ad_op == AST_FNDECLA)
			return 1;
	}
	errp(&decl->ad_sp, "malformed function definition");
	return 0;
}

static void
decl_semcheck(struct ast_decl *decl, int flags)
{
	char *ident = NULL;
	int i, needdecla = 0, okfunc = 0, qual = 0;
	int scope = curscope;
	struct ast_decla *decla;
	struct ir_type *fulltype = NULL, *type;
	struct symtab *st = NULL;

	if (decl->ad_stmt != NULL)
		flags |= FLG_FUNDEF;

	type = declspecs_semcheck(decl->ad_ds, flags, &needdecla);

	if ((qual = cqualtoirqual(decl->ad_ds->ad_tyqual)))
		type = ir_type_qual(type, qual);

	i = 0;
	SIMPLEQ_FOREACH(decla, &decl->ad_declas, ad_link) {
		if (decl->ad_stmt != NULL && i != 0) {
			errp(&decl->ad_sp, "invalid declaration");
			return;
		}
		fulltype = decla_semcheck(decla, type, &ident, flags);
		i++;
		if (fulltype == NULL)
			continue;

		if (IR_ISFUNTY(fulltype))
			okfunc = funcdecl_semcheck(decl);
		else if (decl->ad_ds->ad_fnspec != 0) {
			errp(&decl->ad_ds->ad_sp,
			    "function specifier for non-function given");
			decl->ad_ds->ad_fnspec = 0;
		} else if (ininline == AST_SC_EXTERN && IR_ISQUAL(fulltype)) {
			if (!IR_ISCONST(fulltype) &&
			    decl->ad_ds->ad_sclass == AST_SC_STATIC) {
				errp(&decl->ad_ds->ad_sp, "invalid "
				    "declaration in inline function");
			}
		}

		if (scope == 0) {
			if (decl->ad_stmt != NULL && okfunc)
				st = popsymtab();
			decla->ad_sym = decglobl(decl, ident, fulltype);
			if (decl->ad_stmt != NULL) {
				if (okfunc)
					pushsymtab(st);
				decla->ad_sym->s_flags |= SYM_DEF;
			} else if (decla->ad_init != NULL ||
			    decla->ad_sym->s_sclass == AST_SC_STATIC) {
				if (decla->ad_sym->s_flags & SYM_DEF)
					errp(&decla->ad_sp, "redefinition "
					    "of `%s'", decla->ad_sym->s_ident);
				decla->ad_sym->s_flags |= SYM_DEF;
				decla->ad_sym->s_flags &= ~SYM_TENTDEF;
			}
		} else {
			if (decla->ad_init != NULL &&
			    decl->ad_ds->ad_sclass == AST_SC_EXTERN)
				errp(&decla->ad_sp, "initializer invalid");
			decla->ad_sym = declocal(decl, ident, fulltype,
			    decla->ad_init != NULL);
		}
		if (decla->ad_init != NULL)
			init_semcheck(fulltype, decla->ad_init);

	}

	if (SIMPLEQ_EMPTY(&decl->ad_declas) && needdecla) {
		errp(&decl->ad_sp, "declaration has no declarator");
		return;
	}

	if (decl->ad_stmt == NULL || scope > 0)
		return;

	if (decl->ad_ds->ad_fnspec & AST_FS_INLINE) {
		if (decl->ad_ds->ad_sclass == 0)
			ininline = AST_SC_EXTERN;
		else
			ininline = AST_SC_STATIC;
	}

	/*
	 * TODO: Make sure that every parameter has a name, check
	 * all K&R declarations.
	 */
	if (fulltype->it_flags & IR_KRFUNC)
		fatalx("decl_semcheck: K&R functions not yet");
	ir_type_setflags(fulltype, IR_FUNDEF);
	compound_semcheck(fulltype, decl->ad_stmt, NULL, flags);
	closescope();
	ininline = 0;
}

static struct ir_type *
declspecs_semcheck(struct ast_declspecs *ds, int flags, int *needdecla)
{
	struct ir_type *type;

	if (SIMPLEQ_EMPTY(&ds->ad_tyspec)) {
		warnp(&ds->ad_sp, "no type specifier given, defaulting to "
		    "int");
		*needdecla = 1;
		type = &cir_int;
	} else
		type = tyspec_semcheck(ds, flags, needdecla);
	return type;
}

static struct ir_type *
tyspec_semcheck(struct ast_declspecs *ds, int flags, int *needdecla)
{
	int expect = -1, tscode = 0;
	struct ir_type *type = &cir_int;
	struct ast_tyspec *ts;

	SIMPLEQ_FOREACH(ts, &ds->ad_tyspec, at_link) {
		if (expect != -1 && expect != ts->at_op) {
			errp(&ts->at_sp, "conflicting type specifiers");
			return &cir_int;
		}
		switch (ts->at_op) {
		case AST_TYSPEC_SIMPLE:
			*needdecla = 1;
			expect = AST_TYSPEC_SIMPLE;
			if ((tscode = simplespec_semcheck(ts, tscode)) == 0)
				return &cir_int;
			break;
		case AST_TYSPEC_TDNAME:
			*needdecla = 1;
			expect = 0;
			type = tdnamespec_semcheck(ts);
			break;
		case AST_TYSPEC_STRUCT:
		case AST_TYSPEC_UNION:
			expect = 0;
			type = souspec_semcheck(ts, needdecla);
			break;
		case AST_TYSPEC_ENUM:
			expect = 0;
			type = enumspec_semcheck(ts, needdecla);
			break;
		default:
			fatalx("tyspec_semcheck: bad op: %d", ts->at_op);
		}
	}

	if (tscode)
		type = typematrix[tscode - 1][(flags & FLG_INFLD) != 0];
	if (expect == 0 && (flags & FLG_INFLD))
		errp(&ds->ad_sp, "invalid bitfield type");
	return type;
}

static int
simplespec_semcheck(struct ast_tyspec *ts, int tscode)
{
	int tc = 0;

	tc = toktotcode(ts->at_simpletok);
	if ((tscode = tsmatrix[tscode][tc - 1]) == 0)
		errp(&ts->at_sp, "conflicting type specifiers");
	return tscode;
}

static struct ir_type *
tdnamespec_semcheck(struct ast_tyspec *ts)
{
	struct symbol *sym;
	struct ir_type *t;

	/* If this happens, something is broken in the parser. */
	if ((sym = symlookup(ts->at_tdname, NSORD)) == NULL)
		fatalx("tdnamespec_semcheck: %s", ts->at_tdname);

	/*
	 * typedef int A[]; A a = { 1 }, b = { 2, 3 }, etc. is legal,
	 * so we need to copy incomplete array types.
	 */
	if (IR_ISARR(sym->s_type) && !IR_ISCOMPLETE(sym->s_type)) {
		t = ir_type_arr(sym->s_type->it_base, 0);
		return t;
	}
	return sym->s_type;
}

static struct ir_type *
souspec_semcheck(struct ast_tyspec *ts, int *needdecla)
{
	char *ident, *what;
	int flags, fldsz, i, ndecla, tyop, usefld;
	struct ast_souspec *sou;
	struct ast_souent *ent;
	struct ir_type *souty, *type;
	struct ir_typelm *elm;
	struct symbol *sym = NULL;

	sou = ts->at_sou;
	if (ts->at_op == AST_TYSPEC_STRUCT) {
		tyop = IR_STRUCT;
		what = "struct";
	} else {
		tyop = IR_UNION;
		what = "union";
	}

	if (sou->as_name != NULL) {
		sym = symlookup(sou->as_name, NSTAG);
		*needdecla = 0;
	} else
		*needdecla = 1;

	if (!(sou->as_flags & AST_SOU_HASELMS)) {
		if (sym == NULL) {
			souty = ir_type_sou(tyop);
			symenter(sou->as_name, souty, NSTAG);
			return souty;
		}
		if (sym->s_scope == curscope) {
			if (!IR_ISTYOP(sym->s_type, tyop)) {
				errp(&sou->as_sp, "`%s' is not a %s",
				    sou->as_name, what);
				return &cir_int;
			}
		}
		return sym->s_type;
	}

	if (sym != NULL) {
		if (IR_ISCOMPLETE(sym->s_type) && sym->s_scope == curscope) {
			errp(&sou->as_sp, "%s `%s' redeclared", what,
			    sou->as_name);
			return &cir_int;
		}
		souty = sym->s_type;
	} else {
		souty = ir_type_sou(tyop);
		if (sou->as_name != NULL)
			symenter(sou->as_name, souty, NSTAG);
	}

	i = 0;
	flags = 0;
	if (sou->as_flags & AST_SOU_PACKED)
		flags |= IR_PACKED;
	SIMPLEQ_FOREACH(ent, &sou->as_ents, as_link) {
		i++;
		fldsz = usefld = 0;
		if (ent->as_fieldexpr != NULL) {
			type = declspecs_semcheck(ent->as_ds, 1, &ndecla);
			if (ent->as_decla != NULL &&
			    ent->as_decla->ad_op != AST_IDDECLA) {
				errp(&ent->as_decla->ad_sp, "invalid "
				    "bitfield declaration");
				continue;
			}
			ident = ent->as_decla->ad_ident;
			if (!expr_semcheck(&ent->as_fieldexpr,
			    EXPR_INT | EXPR_CON))
				fldsz = getfldsize(ent->as_fieldexpr);
			else
				fldsz = 1;
			if (fldsz == 0 && ent->as_decla != NULL) {
				errp(&ent->as_ds->ad_sp, "illegal bitfield "
				    "size");
				fldsz = 1;
			}
			if (fldsz > 8 * type->it_size) {
				errp(&ent->as_ds->ad_sp, "illegal bitfield "
				    "size");
				fldsz = 1;
			}
			usefld = 1;
		} else {
			type = declspecs_semcheck(ent->as_ds, 0, &ndecla);
			type = decla_semcheck(ent->as_decla, type, &ident,
			    FLG_INSOU);
		}

		if (!IR_ISCOMPLETE(type)) {
			if (i == 1 ||
			    SIMPLEQ_NEXT(ent, as_link) !=
			    SIMPLEQ_END(&sou->as_ents) ||
			    !IR_ISARR(type)) {
				errp(&ent->as_ds->ad_sp, "incomplete type in "
				    "struct or union");
				continue;
			}
			if (IR_ISUNION(souty)) {
				errp(&ent->as_ds->ad_sp, "can't use variably "
				    "sized arrays in unions");
				continue;
			}
		}

		elm = NULL;
		if (ident != NULL) {
			SIMPLEQ_FOREACH(elm, &souty->it_typeq, it_link) {
				if (elm->it_name != NULL)
					continue;
				errp(&sou->as_sp, "duplicate member `%s' in "
				    "struct or union");
				break;
			}
		}

		flags |= type->it_op & IR_TYQUALS;
		if (elm == NULL)
			elm = ir_typelm(ident, type, fldsz, usefld);
		ir_type_newelm(souty, elm);
	}

	if (i == 0)
		errp(&ts->at_sp, "struct or union must contain at least one "
		    "element");
	ir_type_sou_finish(souty, flags);
	return souty;
}

static struct ir_type *
enumspec_semcheck(struct ast_tyspec *ts, int *needdecla)
{
	int nextval = 0;
	struct ast_enument *ent;
	struct ast_enumspec *enu;
	struct symbol *sym = NULL;

	enu = ts->at_enum;
	if (enu->aen_ident != NULL) {
		sym = symlookup(enu->aen_ident, NSTAG);
		*needdecla = 0;
	} else
		*needdecla = 1;
	if (!enu->aen_haselms) {
		if (sym == NULL)
			errp(&enu->aen_sp, "undeclared enum `%s'",
			    enu->aen_ident);
		else if (sym->s_type != &cir_enum)
			errp(&enu->aen_sp, "`%s' is not an enum",
			    enu->aen_ident);
		return &cir_int;
	}

	if (sym != NULL && sym->s_scope == curscope) {
		errp(&enu->aen_sp, "enum `%s' redeclared", enu->aen_ident);
		return &cir_int;
	}

	SIMPLEQ_FOREACH(ent, &enu->aen_ents, aen_link) {
		if (ent->aen_expr != NULL) {
			if (!(expr_semcheck(&ent->aen_expr,
			    EXPR_INT | EXPR_CON)))
				nextval = getenumval(ent->aen_expr);
		}

		if ((sym = symlookup(ent->aen_ident, NSORD)) != NULL &&
		    sym->s_scope == curscope) {
			errp(NULL, "`%s' redeclared", ent->aen_ident);
			continue;
		}

		sym = symenter(ent->aen_ident, &cir_int, NSORD);
		sym->s_flags |= SYM_ENUM;
		sym->s_enumval = nextval;
		nextval++;
	}

	if (enu->aen_ident != NULL)
		symenter(enu->aen_ident, &cir_enum, NSORD);
	return &cir_int;
}

static struct ir_type *
decla_semcheck(struct ast_decla *decla, struct ir_type *type, char **idp,
    int flags)
{
	int qual;
	size_t dim;
	struct ast_decla *d;
	struct ir_type *newtype = type;

	for (d = decla; d != NULL; d = d->ad_decla) {
		switch (d->ad_op) {
		case AST_IDDECLA:
			if (idp != NULL)
				*idp = d->ad_ident;
			break;
		case AST_ARRDECLA:
			dim = 0;
			if (d->ad_expr != NULL) {
				if (!(expr_semcheck(&d->ad_expr,
				    EXPR_INT | EXPR_CON)))
					dim = getarrdim(d->ad_expr);
			}

			if (newtype == &cir_void)
				errp(&d->ad_sp, "can't create array of void");
			else if (IR_ISFUNTY(newtype))
				errp(&d->ad_sp, "can't create array of "
				    "function");
			else if (!IR_ISCOMPLETE(newtype))
				errp(&d->ad_sp, "can't create array of "
				    "incomplete type");
			else
				newtype = ir_type_arr(newtype, dim);
			break;
		case AST_FNDECLA:
			if (IR_ISARR(newtype) || IR_ISFUNTY(newtype)) {
				errp(&d->ad_sp, "functions can't return "
				    "arrays or functions");
				newtype = &cir_int;
			}
			newtype = ir_type_func(newtype);
			openscope();
			if (d->ad_parms != NULL)
				paramlist_semcheck(d->ad_parms, newtype,
				    flags);
			else {
				ir_type_setflags(newtype, IR_KRFUNC);
				if (!(flags & FLG_FUNDEF))
					closescope();
			}
			break;
		case AST_PTRDECLA:
			newtype = ir_type_ptr(newtype);
			if ((qual = cqualtoirqual(d->ad_tqual)))
				newtype = ir_type_qual(newtype, qual);
			break;
		default:
			fatalx("decla_semcheck: bad op: %d", d->ad_op);
		}
	}

	if (IR_ISFUNTY(newtype) && (flags & FLG_INSOU)) {
		errp(&d->ad_sp, "function declaration not permitted inside "
		    "struct or union");
		return &cir_int;
	}
	if (newtype == &cir_void) {
		errp(&decla->ad_sp, "can't declare an identifier of type "
		    "void");
		newtype = &cir_int;
	}
	return newtype;
}

static void
initctx_advance(struct initctx *ctxp)
{
	struct ast_init *init = NULL;

	if (ctxp->i_init != NULL)
		init = ctxp->i_init = SIMPLEQ_NEXT(ctxp->i_init, ai_link);
}

static int
designator_semcheck(struct initctx *ctxp)
{
	fatalx("designator_semcheck not yet");
#if 0
	uintmax_t elem;
	struct ast_designator *d;
	struct ast_expr *x;
	struct ast_init *init = ctxp->i_init;
	struct ir_type *type = ctxp->i_curobj;
	struct ir_typelm *elm;

	SIMPLEQ_FOREACH(d, init->ai_desig, ad_link) {
		if (d->ad_op == AST_ARRDESIG) {
			if (!IR_ISARR(type)) {
				errp(&d->ad_sp, "invalid designator");
				return 1;
			}
			if (expr_semcheck(&d->ad_expr, EXPR_CON | EXPR_INT))
				return 1;
			x = d->ad_expr;
			if (IR_ISSIGNED(x->ae_type)) {
				if (x->ae_con.ic_icon < 0) {
					errp(&d->ad_sp, "invalid designator");
					return 1;
				}
				elem = x->ae_con.ic_icon;
			} else
				elem = x->ae_con.ic_ucon;

			if (!IR_ISCOMPLETE(type) && ctxp->i_level > 0) {
				errp(&init->ai_sp, "invalid array");
				return 1;
			}
			if (IR_ISCOMPLETE(type) && type->it_dim <= elem) {
				errp(&d->ad_sp, "invalid designator");
				return 1;
			} else if (!IR_ISCOMPLETE(type) &&
			    elem + 1 > type->it_dim) {
				type->it_dim = elem + 1;
				type->it_size = type->it_dim *
				    type->it_base->it_size;
			}
			type = ir_type_dequal(type->it_base);
		} else if (!IR_ISSOU(type)) {
			errp(&d->ad_sp, "invalid designator");
			return 1;	
		} else {
			SIMPLEQ_FOREACH(elm, &type->it_typeq, it_link) {
				if (elm->it_name == d->ad_ident) {
					type = ir_type_dequal(elm->it_type);
					break;
				}
			}
			if (elm == NULL)
				errp(&d->ad_sp, "invalid designator %s",
				    d->ad_ident);
		}
	}
	ctxp->i_elem = elem;
	ctxp->i_atobj = type;
	return 0;
#endif
}

static int
initexpr_semcheck(struct ast_init *init)
{
	int flags = 0;
	struct ast_init *elem;

	if (curscope == 0)
		flags = EXPR_ADDRCON;
	if (init->ai_op == AST_SIMPLEINIT) {
		if (expr_semcheck(&init->ai_expr, flags))
			return 1;
	}
	SIMPLEQ_FOREACH(elem, &init->ai_inits, ai_link) {
		if (initexpr_semcheck(elem))
			return 1;
	}
	return 0;
}

static int
arrinit_semcheck(struct initctx *ctxp)
{
	uintmax_t elem;
	struct ast_expr *x;
	struct ast_init *init, *oinit;
	struct ir_type *base, *type = ctxp->i_atobj;
	struct initctx myctx;

	init = oinit = ctxp->i_init;
	base = ir_type_dequal(type->it_base);
	if (!IR_ISCOMPLETE(type) && ctxp->i_level > 0) {
		errp(&init->ai_sp, "can't initialize incomplete array");
		return 1;
	}
	if (!IR_ISCOMPLETE(type) && init->ai_op == AST_LISTINIT &&
	    init->ai_elems == 0) {
		errp(&init->ai_sp, "invalid initializer");
		return 1;
	}
	if (init->ai_op == AST_SIMPLEINIT && base == &cir_char) {
		if (init->ai_desig != NULL) {
			errp(&init->ai_sp, "invalid designator");
			return 1;
		}
		x = init->ai_expr;
		if (x->ae_op != AST_STRLIT) {
			errp(&init->ai_sp, "invalid initializer");
			return 1;
		}
		if (IR_ISCOMPLETE(type) && type->it_dim + 1 < x->ae_strlen) {
			errp(&init->ai_sp, "invalid initializer");
			return 1;
		} else if (!IR_ISCOMPLETE(type)) {
			type->it_dim = type->it_size = x->ae_strlen;
			ir_type_setflags(type, IR_COMPLETE);
		}
		init->ai_bitoff = ctxp->i_bitoff;
		init->ai_bitsize = type->it_size * 8;
		init->ai_objty = type;
		initctx_advance(ctxp);
		return 0;
	}

	myctx = *ctxp;
	myctx.i_level = ctxp->i_level + 1;
	elem = 0;
	if (init->ai_op == AST_LISTINIT)
		init = SIMPLEQ_FIRST(&init->ai_inits);
	myctx.i_init = init;
	while (init != NULL) {
		myctx.i_atobj = base;
		myctx.i_bitoff = ctxp->i_bitoff + elem * base->it_size * 8;
		myctx.i_bitsize = base->it_size * 8;
		if (IR_ISCOMPLETE(type) && elem >= type->it_dim)
			break;
		if (!IR_ISCOMPLETE(type) && elem + 1 > type->it_dim) {
			type->it_dim = elem + 1;
			type->it_size = type->it_dim * type->it_base->it_size;
		} if (doinit_semcheck(&myctx))
			return 1;
		elem++;
		init = myctx.i_init;
	}

	if (oinit->ai_op == AST_LISTINIT) {
		if (init != NULL) {
			errp(&ctxp->i_init->ai_sp, "too many initializers arr");
			return 1;
		}
		ctxp->i_init = SIMPLEQ_NEXT(ctxp->i_init, ai_link);
	} else
		ctxp->i_init = init;
	return 0;
}

static int
souinit_semcheck(struct initctx *ctxp)
{
	struct ast_init *init, *oinit;
	struct ir_type *type = ctxp->i_atobj;
	struct ir_typelm *elm;
	struct initctx myctx;

	init = oinit = ctxp->i_init;
	if (init->ai_op == AST_SIMPLEINIT) {
		if (curscope != 0 && canassign(type, init->ai_expr, 1)) {
			init->ai_bitoff = ctxp->i_bitoff;
			init->ai_bitsize = ctxp->i_bitsize;
			init->ai_objty = type;
			initctx_advance(ctxp);
			return 0;
		}
	}
	myctx = *ctxp;
	myctx.i_level = ctxp->i_level + 1;
	elm = SIMPLEQ_FIRST(&type->it_typeq);
	if (init->ai_op == AST_LISTINIT)
		init = SIMPLEQ_FIRST(&init->ai_inits);
	myctx.i_init = init;
	while (init != NULL && elm != NULL) {
		if (elm->it_name == NULL) {
			elm = SIMPLEQ_NEXT(elm, it_link);
			continue;
		}
		myctx.i_atobj = ir_type_dequal(elm->it_type);
		myctx.i_bitoff = ctxp->i_bitoff + elm->it_off;
		if (elm->it_fldsize >= 0)
			myctx.i_bitsize = elm->it_fldsize;
		else
			myctx.i_bitsize = elm->it_type->it_size * 8;
		if (doinit_semcheck(&myctx))
			return 1;
		elm = SIMPLEQ_NEXT(elm, it_link);
		init = myctx.i_init;
		if (IR_ISUNION(type))
			break;
	}

	if (oinit->ai_op == AST_LISTINIT) {
		if (init != NULL) {
			errp(&ctxp->i_init->ai_sp, "too many initializers sou");
			return 1;
		}
		ctxp->i_init = SIMPLEQ_NEXT(ctxp->i_init, ai_link);
	} else
		ctxp->i_init = init;
	return 0;
}

static int
simpleinit_semcheck(struct initctx *ctxp)
{
	int i;
	struct ast_init *init = ctxp->i_init, *oinit = ctxp->i_init;
	struct ir_type *type = ctxp->i_atobj;
	
	for (i = 0; init->ai_op != AST_SIMPLEINIT; i++) {
		if (init->ai_desig != NULL) {
			errp(&init->ai_sp, "invalid designator");
			return 1;
		}
		if (init->ai_elems == 0) {
			initctx_advance(ctxp);
			return 0;
		}
		if (init->ai_elems != 1) {
			errp(&oinit->ai_sp, "invalid initializer");
			return 1;
		}
		init = SIMPLEQ_FIRST(&init->ai_inits);
	}
	if (i > 1)
		warnp(&init->ai_sp, "excess braces around initializer");
	if (!canassign(type, init->ai_expr, 1)) {
		errp(&init->ai_sp, "invalid initializer %d %d", type->it_op,
		    init->ai_expr->ae_type->it_op);
		return 1;
	}
	init->ai_bitoff = ctxp->i_bitoff;
	init->ai_bitsize = ctxp->i_bitsize;
	init->ai_objty = type;
	initctx_advance(ctxp);
	return 0;
}

static int
doinit_semcheck(struct initctx *ctxp)
{
	struct ast_init *init = ctxp->i_init;
	struct ir_type *type = ctxp->i_atobj;

	if (!IR_ISCOMPLETE(ctxp->i_atobj) && !IR_ISARR(ctxp->i_atobj)) {
		errp(&init->ai_sp, "initialization of an incomplete type");
		return 1;
	}
	if (init->ai_desig != NULL && !SIMPLEQ_EMPTY(init->ai_desig)) {
		if (designator_semcheck(ctxp))
			return 1;
		type = ctxp->i_atobj;
	}
	if (IR_ISSTRUCT(type) || IR_ISUNION(type))
		return souinit_semcheck(ctxp);
	if (IR_ISARR(type))
		return arrinit_semcheck(ctxp);
	return simpleinit_semcheck(ctxp);
}

static void
init_semcheck(struct ir_type *type, struct ast_init *init)
{
	struct initctx myctx;

	if (IR_ISFUNTY(type)) {
		errp(&init->ai_sp, "can't initialize functions");
		return;
	}
	if (initexpr_semcheck(init))
		return;
	if (init->ai_desig != NULL) {
		errp(&init->ai_sp, "invalid designator");
		return;
	}
	if (IR_ISARR(type) && init->ai_op != AST_LISTINIT &&
	    ir_type_dequal(type->it_base) != &cir_char) {
		errp(&init->ai_sp, "invalid initializer");
		return;
	}
	myctx.i_curobj = myctx.i_atobj = ir_type_dequal(type);
	myctx.i_init = init;
	myctx.i_level = 0;
	myctx.i_bitoff = 0;
	doinit_semcheck(&myctx);
	ir_type_setflags(type, IR_COMPLETE);
}

static int
ident_semcheck(struct ast_expr **xp, int flags)
{
	struct ast_expr *x = *xp;
	struct symbol *sym;
	struct ir_type *ty;

	if ((sym = symlookup(x->ae_ident, NSORD)) == NULL) {
		if (flags & EXPR_INCALL) {
			warnp(&x->ae_sp, "implicit declaration of function "
			    "`%s'", x->ae_ident);
			ty = &cir_int;
			ty = ir_type_func(ty);
			ir_type_setflags(ty, IR_KRFUNC);
			sym = symenter(x->ae_ident, ty, NSORD);

			/* XXX: Does not belong here. */
			sym->s_irsym = ir_symbol(IR_FUNSYM, x->ae_ident, 0, 0,
			    &cir_int);
		} else {
			errp(&x->ae_sp, "`%s' undeclared", x->ae_ident);
			return 1;
		}
	} else
		ty = sym->s_type;

	if (IR_ISFUNTY(ty) && !(flags & EXPR_INCALL) &&
	    (sym->s_flags & SYM_BUILTIN)) {
		errp(&x->ae_sp, "builtin functions can only be called "
		    "directly");
		return 1;
	}

	sym->s_flags |= SYM_USED;
	if (ininline == AST_SC_STATIC && sym->s_scope == 0 &&
	    sym->s_sclass == AST_SC_STATIC)
		errp(&x->ae_sp, "invalid object reference in inline function");
	if ((flags & EXPR_ADDRCON) && !(flags & (EXPR_INADDR | EXPR_INSZOF))) {
		if (!IR_ISFUNTY(ty) && !IR_ISARR(ty)) {
			errp(&x->ae_sp, "not an address constant expression");
			return 1;
		}
		if (!(sym->s_flags & SYM_ENUM) && (sym->s_scope > 0 ||
		    (sym->s_sclass != AST_SC_STATIC &&
		     sym->s_sclass != AST_SC_EXTERN))) {
			errp(&x->ae_sp, "not an address constant expression");
			return 1;
		}
	}
	x->ae_sym = sym;
	if (sym->s_flags & SYM_ENUM) {
		*xp = x = ast_expr_icon(&x->ae_sp, sym->s_enumval, &cir_int);
		return 0;
	}

	x->ae_type = ty;
	return 0;
}

static int
subscr_semcheck(struct ast_expr **xp, int flags)
{
	struct ast_expr *l, *x = *xp;
	struct ir_type *lty, *rty;

	if (expr_semcheck(&x->ae_l, flags & (EXPR_ADDRCON | EXPR_INSZOF)))
		return 1;
	if (expr_semcheck(&x->ae_r, flags & (EXPR_ADDRCON | EXPR_INSZOF)))
		return 1;
	if ((flags & EXPR_ADDRCON) && !(flags & (EXPR_INADDR | EXPR_INSZOF))) {
		errp(&x->ae_sp, "not an address constant expression");
		return 1;
	}
	lty = ir_type_dequal(x->ae_l->ae_type);
	if (!IR_ISPTR(lty) && !IR_ISARR(lty)) {
		l = x->ae_l;
		x->ae_l = x->ae_r;
		x->ae_r = l;
	}

	lty = ir_type_dequal(x->ae_l->ae_type);
	rty = ir_type_dequal(x->ae_r->ae_type);
	if (IR_ISPTR(lty) && IR_ISFUNTY(lty->it_base)) {
		errp(&x->ae_sp, "invalid array access");
		return 1;
	}
	if ((!IR_ISPTR(lty) && !IR_ISARR(lty)) || !tyisinteger(rty)) {
		errp(&x->ae_sp, "invalid array access");
		return 1;
	}

	x->ae_type = lty->it_base;
	return 0;
}

static int
builtin_semcheck(struct ast_expr *x, int flags)
{
	struct ast_expr *arg, *z;
	struct ir_type *ty;
	struct symbol *sym;

	if (flags & EXPR_ADDRCON && !(flags & EXPR_INSZOF)) {
		errp(&x->ae_sp, "not an address constant expression");
		return 1;
	}
	sym = x->ae_l->ae_sym;
	arg = TAILQ_FIRST(&x->ae_args->al_exprs);
	if (sym == builtin_va_start_sym || sym == builtin_va_arg_sym ||
	    sym == builtin_va_end_sym) {
		z = arg;
		if (expr_semcheck(&z, 0))
			return 1;
		if (ir_type_dequal(z->ae_type) != builtin_va_list) {
			errp(&x->ae_sp, "argument 1 of type va_list expected");
			return 1;
		}
		if (arg != z)
			TAILQ_REPLACE(&x->ae_args->al_exprs, arg, z, ae_link);
	} else
		fatalx("builtin_semcheck");
	if (sym == builtin_va_start_sym) {
		if ((arg = TAILQ_NEXT(arg, ae_link)) == NULL) {
			errp(&x->ae_sp, "va_start needs a second argument");
			return 1;
		}
		if (expr_semcheck(&arg, 0))
			return 1;
		if (arg->ae_op != AST_IDENT) {
			errp(&x->ae_sp, "identifier expected for argument 2");
			return 1;
		}
		if (arg->ae_sym->s_scope != 1) {
			errp(&x->ae_sp, "argument 2 is not a parameter");
			return 1;
		}

		/* XXX */
		if (arg->ae_sym != lastpar) {
			errp(&x->ae_sp, "argument 2 ist not the last "
			    "parameter of the current function");
			return 1;
		}
		x->ae_type = &cir_void;
	} else if (sym == builtin_va_arg_sym) {
		if ((arg = TAILQ_NEXT(arg, ae_link)) == NULL) {
			errp(&x->ae_sp, "va_arg needs a second argument");
			return 1;
		}
		if (arg->ae_op != AST_CAST) {
			errp(&x->ae_sp, "malformed argument 2");
			return 1;
		}
		if (cast_semcheck(&arg, EXPR_VAARG))
			return 1;
		ty = ir_type_dequal(arg->ae_type);
		if (ty != tyargconv(ty)) {
			errp(&x->ae_sp, "type argument of va_start is not a"
			    "promoted type");
			return 1;
		}
		x->ae_type = ty;
	} else if (sym == builtin_va_end_sym) {
		if ((arg = TAILQ_NEXT(arg, ae_link)) != NULL) {
			errp(&x->ae_sp, "too many arguments for va_end");
			return 1;
		}
		x->ae_type = &cir_void;
	}
	return 0;
}

static int
call_semcheck(struct ast_expr **xp, int flags)
{
	int i;
	struct ast_expr *x = *xp, *y, *z;
	struct ir_type *ty;
	struct ir_typelm *elm;

	if ((flags & EXPR_ADDRCON) && !(flags & EXPR_INSZOF)) {
		errp(&x->ae_sp, "not an address constant expression");
		return 1;
	}
	if (expr_semcheck(&x->ae_l, EXPR_INCALL))
		return 1;
	if (x->ae_l->ae_op == AST_IDENT &&
	    (x->ae_l->ae_sym->s_flags & SYM_BUILTIN))
		return builtin_semcheck(x, flags);

	ty = ir_type_dequal(x->ae_l->ae_type);
	if (IR_ISPTR(ty))
		ty = ty->it_base;
	if (!IR_ISFUNTY(ty)) {
		errp(&x->ae_sp, "called object is not a function");
		return 1;
	}

	elm = SIMPLEQ_FIRST(&ty->it_typeq);
	i = 0;
	TAILQ_FOREACH(y, &x->ae_args->al_exprs, ae_link) {
		z = y;
		if (elm == NULL &&
		    !(ty->it_flags & (IR_ELLIPSIS | IR_KRFUNC))) {
			errp(&x->ae_sp, "too many arguments to function");
			return 1;
		}
		if (expr_semcheck(&z, 0))
			return 1;
		z->ae_type = ir_type_dequal(z->ae_type);
		if (elm != NULL) {
			if (!canassign(ir_type_dequal(elm->it_type), z, 1)) {
				errp(&x->ae_sp, "argument %d has bad type",
				    i + 1);
				return 1;
			}
			elm = SIMPLEQ_NEXT(elm, it_link);
		}
		if (y != z)
			TAILQ_REPLACE(&x->ae_args->al_exprs, y, z, ae_link);

		i++;
	}

	if (elm != NULL) {
		errp(&x->ae_sp, "too few arguments for function");
		return 1;
	}
	
	/*
	 * TODO: Check for calls to builtin functions and handle them properly.
	 */

	x->ae_type = ty->it_base;
	return 0;
}

static int
souref_semcheck(struct ast_expr **xp, int flags)
{
	struct ast_expr *x = *xp;
	struct ir_type *ty;
	struct ir_typelm *elm;

	if ((flags & EXPR_ADDRCON) && !(flags & (EXPR_INADDR | EXPR_INSZOF))) {
		errp(&x->ae_sp, "not an address constant expression");
		return 1;
	}
	if (expr_semcheck(&x->ae_l, 0))
		return 1;
	ty = ir_type_dequal(x->ae_l->ae_type);
	if (x->ae_op == AST_SOUIND) {
		if (!IR_ISPTR(ty) && !IR_ISARR(ty)) {
			errp(&x->ae_sp, "left side of -> is not a pointer");
			return 1;
		} else
			ty = ty->it_base;
	}
	ty = ir_type_dequal(ty);
	if (!IR_ISSTRUCT(ty) && !IR_ISUNION(ty)) {
		errp(&x->ae_sp, "left side of -> or . is not a structure or "
		    "union");
		return 1;
	}
	SIMPLEQ_FOREACH(elm, &ty->it_typeq, it_link) {
		if (elm->it_name == x->ae_ident)
			break;
	}
	if (elm == NULL) {
		errp(&x->ae_sp, "unknown struct or union member `%s'",
		    x->ae_ident);
		return 1;
	}
	x->ae_elm = elm;
	if (elm->it_fldsize >= 0)
		x->ae_type = tyfldpromote(elm->it_type, elm->it_fldsize);
	else
		x->ae_type = elm->it_type;
	return 0;
}

static int
incdec_semcheck(struct ast_expr **xp, int flags)
{
	char *op, *opname;
	struct ast_expr *x = *xp;
	struct ir_type *ty;

	if ((flags & EXPR_ADDRCON) && !(flags & EXPR_INSZOF)) {
		errp(&x->ae_sp, "no address constant expression");
		return 1;
	}
	if (expr_semcheck(&x->ae_l, 0))
		return 1;
	if (x->ae_op == AST_PREINC || x->ae_op == AST_POSTINC) {
		op = "++";
		opname = "increment";
	} else {
		op = "--";
		opname = "decrement";
	}

	if (!islvalue(x->ae_l)) {
		errp(&x->ae_sp, "operand of %s is not an lvalue", op);
		return 1;
	}
	ty = x->ae_l->ae_type;
	if (IR_ISCONST(ty)) {
		errp(&x->ae_sp, "operand of %s is not a modifiable lvalue",
		    op);
		return 1;
	}
	ty = ir_type_dequal(ty);
	x->ae_type = ty;
	if (IR_ISPTR(ty)) {
		if (IR_ISFUNTY(ty->it_base)) {
			errp(&x->ae_sp, "can't %s a function", opname);
			return 1;
		} else if (IR_ISVOID(ty->it_base)) {
			errp(&x->ae_sp, "can't %s a void pointer", opname);
			return 1;
		}
		return 0;
	}
	if (!tyisreal(ty)) {
		errp(&x->ae_sp, "invalid operand for %s", op);
		return 1;
	}
	return 0;
}

static int
addrof_semcheck(struct ast_expr **xp, int flags)
{
	struct ast_expr *l, *x = *xp;

	flags &= EXPR_ADDRCON;
	flags |= EXPR_INADDR;
	if (expr_semcheck(&x->ae_l, flags))
		return 1;
	if (x->ae_l->ae_op == AST_IDENT &&
	    x->ae_l->ae_sym->s_sclass == AST_SC_REGISTER) {
		errp(&x->ae_l->ae_sp, "can't take address of object with"
		    "storage class register");
		return 1;
	}
	l = x->ae_l;
	if ((l->ae_op == AST_SOUDIR || l->ae_op == AST_SOUIND) &&
	    l->ae_elm->it_fldsize >= 0) {
		errp(&l->ae_sp, "can't take address of bitfield %s",
		    l->ae_elm->it_name);
		return 1;
	}
	if (IR_ISFUNTY(x->ae_l->ae_type)) {
		if (x->ae_l->ae_op == AST_IDENT) {
			x->ae_type = x->ae_l->ae_type;
			return 0;
		}
		x->ae_type = ir_type_ptr(x->ae_l->ae_type);
		return 0;
	}
	if (!islvalue(x->ae_l)) {
		errp(&x->ae_sp, "invalid operand of unary &");
		return 1;
	}
	x->ae_type = ir_type_ptr(x->ae_l->ae_type);
	return 0;
}

static int
deref_semcheck(struct ast_expr **xp, int flags)
{
	struct ast_expr *x = *xp;
	struct ir_type *ty;

	if (expr_semcheck(&x->ae_l, flags & (EXPR_ADDRCON | EXPR_INSZOF)))
		return 1;
	ty = ir_type_dequal(x->ae_l->ae_type);
	if (!IR_ISPTR(ty) && !IR_ISFUNTY(ty)) {
		errp(&x->ae_sp, "operand of unary * must have pointer type");
		return 1;
	}
	if ((flags & EXPR_ADDRCON) && !(flags & EXPR_INSZOF)) {
		if (!IR_ISFUNTY(ty)) {
			errp(&x->ae_sp, "not an address constant expression");
			return 1;
		}
	}
	if (IR_ISFUNTY(ty))
		x->ae_type = ty;
	else
		x->ae_type = ty->it_base;
	return 0;
}

static int
sizeof_semcheck(struct ast_expr **xp, int flags)
{
	char *id;
	int nd;
	struct ast_decl *decl;
	struct ast_decla *decla;
	struct ast_expr *l, *x = *xp;
	struct ir_type *ty;

	if (x->ae_op == AST_SIZEOFX) {
		if (expr_semcheck(&x->ae_l, EXPR_INSZOF))
			return 1;
		l = x->ae_l;
		if ((l->ae_op == AST_SOUDIR || l->ae_op == AST_SOUIND) &&
		    l->ae_elm->it_fldsize >= 0) {
			errp(&l->ae_sp, "can't apply sizeof to bitfield %s",
			    l->ae_elm->it_name);
			return 1;
		}
		ty = x->ae_l->ae_type;
	} else {
		decl = x->ae_decl;
		ty = declspecs_semcheck(decl->ad_ds, 0, &nd);
		if (!SIMPLEQ_EMPTY(&decl->ad_declas)) {
			decla = SIMPLEQ_FIRST(&decl->ad_declas);
			ty = decla_semcheck(decla, ty, &id, 0);
		}
		if (!(ty->it_flags & IR_COMPLETE))
			errp(&decl->ad_sp, "can't apply sizeof to incomplete "
			    "type");
	}
	
	if (IR_ISFUNTY(ty)) {
		errp(&x->ae_sp, "can't apply sizeof to function type");
		return 1;
	}

	*xp = x = ast_expr_ucon(&x->ae_sp, ty->it_size, &cir_size_t);
	return 0;
}

static int
cast_semcheck(struct ast_expr **xp, int flags)
{
	char *id;
	int nd;
	struct ast_decl *decl;
	struct ast_decla *decla;
	struct ast_expr *l, *x = *xp;
	union ir_con lc;
	struct ir_type *lty, *ty;

	if (expr_semcheck(&x->ae_l, flags & (EXPR_ADDRCON | EXPR_INSZOF)))
		return 1;
	decl = x->ae_decl;
	ty = declspecs_semcheck(decl->ad_ds, 0, &nd);
	if (!SIMPLEQ_EMPTY(&decl->ad_declas)) {
		decla = SIMPLEQ_FIRST(&decl->ad_declas);
		ty = decla_semcheck(decla, ty, &id, 0);
	}
	if (flags & EXPR_VAARG) {
		x->ae_type = ty;
		return 0;
	}

	if (IR_ISSTRUCT(ty) || IR_ISUNION(ty) || IR_ISFUNTY(ty) ||
	    IR_ISARR(ty)) {
		errp(&x->ae_sp, "invalid cast");
		return 1;
	}

	x->ae_type = ty;
	lty = x->ae_l->ae_type;
	if (tyisarith(ty) && tyisarith(lty))
		goto fold;
	if (tyisinteger(ty) && IR_ISPTR(lty))
		return 0;

	/*
	 * NOTE: Does not strictly follow the standard here. We allow
	 * function pointer to be cast to any other pointer and vice versa.
	 */
	if (IR_ISPTR(ty) && (tyisinteger(lty) || IR_ISPTR(lty)))
		return 0;

fold:
	l = x->ae_l;
	if (ISCON(l)) {
		lc = l->ae_con;
		ir_con_cast(&lc, l->ae_type, ty);
		if (IR_ISSIGNED(ty))
			x = ast_expr_icon(&x->ae_sp, lc.ic_icon, ty);
		else if (IR_ISUNSIGNED(ty))
			x = ast_expr_ucon(&x->ae_sp, lc.ic_ucon, ty);
		else
			x = ast_expr_fcon(&x->ae_sp, lc.ic_fcon, ty);
		*xp = x;
	}
	x->ae_type = ty;
	return 0;
}

static int
uplusminus_semcheck(struct ast_expr **xp, int flags)
{
	struct ast_expr *l, *x = *xp;
	union ir_con lc;
	struct ir_type *ty;

	if (expr_semcheck(&x->ae_l, flags & (EXPR_ADDRCON | EXPR_INSZOF)))
		return 1;
	l = x->ae_l;
	if (!tyisarith(l->ae_type)) {
		errp(&x->ae_sp, "invalid operand of unary %c",
		    x->ae_op == AST_UMINUS ? '-' : '+'); 
		return 1;
	}
	ty = tyuconv(l->ae_type);
	if (ISCON(l)) {
		lc = l->ae_con;
		ir_con_cast(&lc, l->ae_type, ty);
		if (x->ae_op == AST_UMINUS) {
			if (IR_ISSIGNED(ty))
				lc.ic_icon = -lc.ic_icon;
			else if (IR_ISUNSIGNED(ty))
				lc.ic_ucon = -lc.ic_ucon;
			else
				lc.ic_fcon = -lc.ic_fcon;
		}
		ir_con_cast(&lc, ty, ty);
		if (IR_ISSIGNED(ty))
			*xp = x = ast_expr_icon(&x->ae_sp, lc.ic_icon, ty);
		else if (IR_ISUNSIGNED(ty))
			*xp = x = ast_expr_ucon(&x->ae_sp, lc.ic_ucon, ty);
		else
			*xp = x = ast_expr_fcon(&x->ae_sp, lc.ic_fcon, ty);
	}
	x->ae_type = ty;
	return 0;
}

static int
not_semcheck(struct ast_expr **xp, int flags)
{
	struct ast_expr *l, *x = *xp;
	union ir_con ic;
	struct ir_type *ty;

	if (expr_semcheck(&x->ae_l, flags & (EXPR_ADDRCON | EXPR_INSZOF)))
		return 1;
	if (x->ae_op == AST_BITFLIP) {
		if (!tyisinteger(x->ae_l->ae_type)) {
			errp(&x->ae_sp, "invalid operand of ~");
			return 1;
		}
		ty = tyuconv(x->ae_l->ae_type);
		if (x->ae_l->ae_op == AST_ICON) {
			ic = x->ae_l->ae_con;
			ir_con_cast(&ic, x->ae_l->ae_type, ty);
			if (IR_ISSIGNED(ty))
				x = ast_expr_icon(&x->ae_sp, ~ic.ic_icon, ty);
			else
				x = ast_expr_ucon(&x->ae_sp, ~ic.ic_ucon, ty);
			ir_con_cast(&x->ae_con, ty, ty);
			*xp = x;	
		}
		x->ae_type = ty;
		return 0;
	}

	if (!tyisscalar(x->ae_l->ae_type)) {
		errp(&x->ae_sp, "invalid operand of !");
		return 1;
	}
	l = x->ae_l;
	if (ISCON(l)) {
		if (IR_ISSIGNED(l->ae_type))
			x = ast_expr_icon(&x->ae_sp, !l->ae_con.ic_icon,
			    &cir_int);
		else if (IR_ISUNSIGNED(l->ae_type))
			x = ast_expr_icon(&x->ae_sp, !l->ae_con.ic_ucon,
			    &cir_int);
		else
			x = ast_expr_icon(&x->ae_sp, !l->ae_con.ic_fcon,
			    &cir_int);
		*xp = x;
	}
	x->ae_type = &cir_int;
	return 0;
}

static int
mul_semcheck(struct ast_expr **xp, int flags)
{
	int op;
	char *opstr;
	struct ast_expr *l, *r, *x = *xp;
	union ir_con lc, rc;
	struct ir_type *ty;

	if (!(flags & EXPR_ASG)) {
		if (expr_semcheck(&x->ae_l,
		    flags & (EXPR_ADDRCON | EXPR_INSZOF)))
			return 1;
		if (expr_semcheck(&x->ae_r,
		    flags & (EXPR_ADDRCON | EXPR_INSZOF)))
			return 1;
	}

	switch (x->ae_op) {
	case AST_MUL:
	case AST_MULASG:
		opstr = "*";
		op = AST_MUL;
		break;
	case AST_DIV:
	case AST_DIVASG:
		opstr = "/";
		op = AST_DIV;
		break;
	case AST_MOD:
	case AST_MODASG:
		opstr = "%";
		op = AST_MOD;
		break;
	default:
		fatalx("mul_semcheck: bad op: %d", x->ae_op);
	}

	if (op == AST_MUL || op == AST_DIV) {
		if (!tyisarith(x->ae_l->ae_type)) {
			errp(&x->ae_sp,
			    flags & EXPR_ASG ? "invalid left side of %s=" :
			    "invalid left operand of binary %s", opstr);
			return 1;
		}
		if (!tyisarith(x->ae_r->ae_type)) {
			errp(&x->ae_sp,
			    flags & EXPR_ASG ? "invalid left side of %s=" :
			    "invalid right operand of binary %s", opstr);
			return 1;
		}
	} else {
		if (!tyisinteger(x->ae_l->ae_type)) {
			errp(&x->ae_sp,
			    flags & EXPR_ASG ? "invalid left side of %%=" :
			        "invalid left operand of binary %%");
			return 1;
		}
		if (!tyisinteger(x->ae_r->ae_type)) {
			errp(&x->ae_sp,
			    flags & EXPR_ASG ? "invalid right side of %%=" :
			    "invalid right operand of binary %%");
			return 1;
		}
	}

	if (flags & EXPR_ASG) {
		x->ae_type = x->ae_l->ae_type;
		return 0;
	}
	l = x->ae_l;
	r = x->ae_r;
	ty = tybinconv(l->ae_type, r->ae_type);
	if (ISCON(l) && ISCON(r)) {
		lc = l->ae_con;
		rc = r->ae_con;
		ir_con_cast(&lc, l->ae_type, ty);
		ir_con_cast(&rc, r->ae_type, ty);
		if (IR_ISSIGNED(ty)) {
			if (x->ae_op != AST_MUL && rc.ic_icon == 0)
				goto out;
			if (x->ae_op == AST_MUL)
				x = ast_expr_icon(&x->ae_sp,
				    lc.ic_icon * rc.ic_icon, ty);
			else if (x->ae_op == AST_DIV)
				x = ast_expr_icon(&x->ae_sp,
				    lc.ic_icon / rc.ic_icon, ty);
			else
				x = ast_expr_icon(&x->ae_sp,
				    lc.ic_icon % rc.ic_icon, ty);
		} else if (IR_ISUNSIGNED(ty)) {
			if (x->ae_op != AST_MUL && rc.ic_ucon == 0)
				goto out;
			if (x->ae_op == AST_MUL)
				x = ast_expr_ucon(&x->ae_sp,
				    lc.ic_ucon * rc.ic_ucon, ty);
			else if (x->ae_op == AST_DIV)
				x = ast_expr_ucon(&x->ae_sp,
				    lc.ic_ucon / rc.ic_ucon, ty);
			else
				x = ast_expr_ucon(&x->ae_sp,
				    lc.ic_ucon % rc.ic_ucon, ty);
		} else {
			if (x->ae_op != AST_DIV && rc.ic_fcon == 0.0)
				goto out;
			if (x->ae_op == AST_MUL)
				x = ast_expr_fcon(&x->ae_sp,
				    lc.ic_fcon * rc.ic_fcon, ty);
			else
				x = ast_expr_fcon(&x->ae_sp, 
				    lc.ic_fcon / rc.ic_fcon, ty);
		}
		*xp = x;
		ir_con_cast(&x->ae_con, ty, ty);
	}
out:
	x->ae_type = ty;
	return 0;
}

static int
add_semcheck(struct ast_expr **xp, int flags)
{
	struct ast_expr *l, *r, *x = *xp;
	union ir_con lc, rc;
	struct ir_type *lty, *rty, *ty;

	if (expr_semcheck(&x->ae_l, flags & (EXPR_ADDRCON | EXPR_INSZOF)))
		return 1;
	if (expr_semcheck(&x->ae_r, flags & (EXPR_ADDRCON | EXPR_INSZOF)))
		return 1;

	lty = x->ae_l->ae_type;
	rty = x->ae_r->ae_type;
	if (tyisarith(lty) && tyisarith(rty))
		goto fold;
	if ((tyisobjptr(lty) || IR_ISARR(lty)) && tyisinteger(rty)) {
		if (IR_ISARR(lty))
			lty = ir_type_ptr(lty->it_base);
		x->ae_type = lty;
		return 0;
	}
	if (x->ae_op == AST_ADD) {
		if ((tyisobjptr(rty) || IR_ISARR(lty)) && tyisinteger(lty)) {
			if (IR_ISARR(rty))
				rty = ir_type_ptr(rty->it_base);
			x->ae_type = rty;
			return 0;
		}
		errp(&x->ae_sp, "invalid operands of binary +");
		return 1;
	}
	if ((!tyisobjptr(lty) && !IR_ISARR(lty)) ||
	    (!tyisobjptr(rty) && !IR_ISARR(rty))) {
		errp(&x->ae_sp, "invalid operands of binary -");
		return 1;
	}
	lty = ir_type_dequal(lty);
	lty = ir_type_dequal(lty->it_base);
	rty = ir_type_dequal(rty);
	rty = ir_type_dequal(rty->it_base);
	if (!tyiscompat(lty, rty)) {
		errp(&x->ae_sp, "invalid operands of binary -");
		return 1;
	}
	x->ae_type = &cir_ptrdiff_t;
	return 0;

fold:
	l = x->ae_l;
	r = x->ae_r;
	ty = tybinconv(lty, rty);
	if (ISCON(l) && ISCON(r)) {
		lc = l->ae_con;
		rc = r->ae_con;
		ir_con_cast(&lc, l->ae_type, ty);
		ir_con_cast(&rc, r->ae_type, ty);
		if (IR_ISSIGNED(ty)) {
			if (x->ae_op == AST_ADD)
				x = ast_expr_icon(&x->ae_sp,
				    lc.ic_icon + rc.ic_icon, ty);
			else
				x = ast_expr_icon(&x->ae_sp,
				    lc.ic_icon - rc.ic_icon, ty);
		} else if (IR_ISUNSIGNED(ty)) {
			if (x->ae_op == AST_ADD)
				x = ast_expr_ucon(&x->ae_sp,
				    lc.ic_ucon + rc.ic_ucon, ty);
			else
				x = ast_expr_ucon(&x->ae_sp,
				    lc.ic_ucon - rc.ic_ucon, ty);
		} else {
			if (x->ae_op == AST_ADD)
				x = ast_expr_fcon(&x->ae_sp,
				    lc.ic_fcon + rc.ic_fcon, ty);
			else
				x = ast_expr_fcon(&x->ae_sp,
				    lc.ic_fcon + rc.ic_fcon, ty);
		}
		*xp = x;
		ir_con_cast(&x->ae_con, ty, ty);
	}
	x->ae_type = ty;
	return 0;
}

static int
shift_semcheck(struct ast_expr **xp, int flags)
{
	char *op;
	struct ast_expr *l, *r, *x = *xp;
	union ir_con lc, rc;
	struct ir_type *ty;

	if (!(flags & EXPR_ASG)) {
		if (expr_semcheck(&x->ae_l,
		    flags & (EXPR_ADDRCON | EXPR_INSZOF)))
			return 1;
		if (expr_semcheck(&x->ae_r,
		    flags & (EXPR_ADDRCON | EXPR_INSZOF)))
			return 1;
	}
	
	switch (x->ae_op) {
	case AST_LS:
		op = "<<";
		break;
	case AST_LSASG:
		op = "<<=";
		break;
	case AST_RS:
		op = ">>";
		break;
	case AST_RSASG:
		op = ">>=";
		break;
	default:
		fatalx("shift_semcheck: bad op: %d", x->ae_op);
	}

	if (!tyisinteger(x->ae_l->ae_type) || !tyisinteger(x->ae_r->ae_type)) {
		errp(&x->ae_sp, "invalid operands of %s", op);
		return 1;
	}
	if (flags & EXPR_ASG) {
		x->ae_type = x->ae_l->ae_type;
		return 0;
	}
	l = x->ae_l;
	r = x->ae_r;
	ty = tyuconv(l->ae_type);
	if (l->ae_op == AST_ICON && r->ae_op == AST_ICON) {
		lc = l->ae_con;
		rc = r->ae_con;
		if (IR_ISSIGNED(ty)) {
			if (x->ae_op == AST_LS)
				x = ast_expr_icon(&x->ae_sp,
				    lc.ic_icon << rc.ic_icon, ty);
			else
				x = ast_expr_icon(&x->ae_sp,
				    lc.ic_icon >> rc.ic_icon, ty);
		} else {
			if (x->ae_op == AST_LS)
				x = ast_expr_ucon(&x->ae_sp,
				    lc.ic_ucon << rc.ic_ucon, ty);
			else
				x = ast_expr_ucon(&x->ae_sp,
				    lc.ic_ucon >> rc.ic_ucon, ty);
		}
		*xp = x;
		ir_con_cast(&x->ae_con, l->ae_type, ty);
	}
	x->ae_type = ty;
	return 0;
}

static int
rel_semcheck(struct ast_expr **xp, int flags)
{
	char *op = NULL;
	struct ast_expr *l, *r, *x = *xp;
	union ir_con lc, rc;
	struct ir_type *lty, *rty, *ty;

	switch (x->ae_op) {
	case AST_LT:
		op = "<";
		break;
	case AST_GT:
		op = ">";
		break;
	case AST_LE:
		op = "<=";
		break;
	case AST_GE:
		op = ">=";
		break;
	}

	if (expr_semcheck(&x->ae_l, flags & (EXPR_ADDRCON | EXPR_INSZOF)))
		return 1;
	if (expr_semcheck(&x->ae_r, flags & (EXPR_ADDRCON | EXPR_INSZOF)))
		return 1;
	x->ae_type = &cir_int;
	lty = ir_type_dequal(x->ae_l->ae_type);
	rty = ir_type_dequal(x->ae_r->ae_type);
	if (tyisreal(lty) && tyisreal(rty))
		goto fold;
	if (IR_ISPTR(lty) && IR_ISPTR(rty)) {
		lty = ir_type_dequal(lty);
		rty = ir_type_dequal(rty);
		if (tyiscompat(lty, rty))
			return 0;
	}
	errp(&x->ae_sp, "invalid operands of %s", op);
	return 1;

fold:
	l = x->ae_l;
	r = x->ae_r;
	if (ISCON(l) && ISCON(r)) {
		lc = l->ae_con;
		rc = r->ae_con;
		ty = tybinconv(lty, rty);
		ir_con_cast(&lc, l->ae_type, ty);
		ir_con_cast(&rc, r->ae_type, ty);
		if (IR_ISSIGNED(ty)) {
			if (x->ae_op == AST_LT)
				x = ast_expr_icon(&x->ae_sp,
				    lc.ic_icon < rc.ic_icon, &cir_int);
			else if (x->ae_op == AST_GT)
				x = ast_expr_icon(&x->ae_sp,
				    lc.ic_icon > rc.ic_icon, &cir_int);
			else if (x->ae_op == AST_LE)
				x = ast_expr_icon(&x->ae_sp,
				    lc.ic_icon <= rc.ic_icon, &cir_int);
			else
				x = ast_expr_icon(&x->ae_sp,
				    lc.ic_icon >= rc.ic_icon, &cir_int);
		} else if (IR_ISUNSIGNED(ty)) {
			if (x->ae_op == AST_LT)
				x = ast_expr_icon(&x->ae_sp,
				    lc.ic_ucon < rc.ic_ucon, &cir_int);
			else if (x->ae_op == AST_GT)
				x = ast_expr_icon(&x->ae_sp,
				    lc.ic_ucon > rc.ic_ucon, &cir_int);
			else if (x->ae_op == AST_LE)
				x = ast_expr_icon(&x->ae_sp,
				    lc.ic_ucon <= rc.ic_ucon, &cir_int);
			else
				x = ast_expr_icon(&x->ae_sp,
				    lc.ic_ucon >= rc.ic_ucon, &cir_int);
		} else {
			if (x->ae_op == AST_LT)
				x = ast_expr_icon(&x->ae_sp,
				    lc.ic_fcon < rc.ic_fcon, &cir_int);
			else if (x->ae_op == AST_GT)
				x = ast_expr_icon(&x->ae_sp,
				    lc.ic_fcon > rc.ic_fcon, &cir_int);
			else if (x->ae_op == AST_LE)
				x = ast_expr_icon(&x->ae_sp,
				    lc.ic_fcon <= rc.ic_fcon, &cir_int);
			else
				x = ast_expr_icon(&x->ae_sp,
				    lc.ic_fcon >= rc.ic_fcon, &cir_int);
		}
		*xp = x;
	}
	x->ae_type = &cir_int;
	return 0;
}

static int
eq_semcheck(struct ast_expr **xp, int flags)
{
	struct ast_expr *l, *r, *x = *xp;
	union ir_con lc, rc;
	struct ir_type *lty, *rty, *ty;

	if (expr_semcheck(&x->ae_l, flags & (EXPR_ADDRCON | EXPR_INSZOF)))
		return 1;
	if (expr_semcheck(&x->ae_r, flags & (EXPR_ADDRCON | EXPR_INSZOF)))
		return 1;
	x->ae_type = &cir_int;
	lty = ir_type_dequal(x->ae_l->ae_type);
	rty = x->ae_r->ae_type;
	if (tyisarith(lty) && tyisarith(rty))
		goto fold;
	if (IR_ISPTR(lty) && IR_ISPTR(rty)) {
		lty = ir_type_dequal(lty);
		rty = ir_type_dequal(rty);
		if (IR_ISVOID(lty) || IR_ISVOID(rty))
			return 0;
		if (tyiscompat(lty, rty))
			return 0;
	}

	if (IR_ISPTR(lty) && x->ae_r->ae_op == AST_ICON) {
		if (IR_ISSIGNED(rty) && x->ae_r->ae_con.ic_icon == 0)
			return 0;
		if (x->ae_r->ae_con.ic_ucon == 0)
			return 0;
	}
	if (IR_ISPTR(rty) && x->ae_r->ae_op == AST_ICON) {
		if (IR_ISSIGNED(lty) && x->ae_l->ae_con.ic_icon == 0)
			return 0;
		if (x->ae_l->ae_con.ic_ucon == 0)
			return 0;
	}
	errp(&x->ae_sp, "invalid operands of %s",
	    x->ae_op == AST_EQ ? "==" : "!=");
	return 1;

fold:
	l = x->ae_l;
	r = x->ae_r;
	if (ISCON(l) && ISCON(r)) {
		lc = l->ae_con;
		rc = r->ae_con;
		ty = tybinconv(lty, rty);
		ir_con_cast(&lc, l->ae_type, ty);
		ir_con_cast(&rc, r->ae_type, ty);
		if (IR_ISSIGNED(lty)) {
			if (x->ae_op == AST_EQ)
				x = ast_expr_icon(&x->ae_sp,
				    lc.ic_icon == rc.ic_icon, &cir_int);
			else
				x = ast_expr_icon(&x->ae_sp,
				    lc.ic_icon != rc.ic_icon, &cir_int);
		} else if (IR_ISUNSIGNED(ty)) {
			if (x->ae_op == AST_EQ)
				x = ast_expr_icon(&x->ae_sp,
				    lc.ic_ucon == rc.ic_ucon, &cir_int);
			else
				x = ast_expr_icon(&x->ae_sp,
				    lc.ic_ucon != rc.ic_ucon, &cir_int);
		} else {
			if (x->ae_op == AST_EQ)
				x = ast_expr_icon(&x->ae_sp,
				    lc.ic_fcon == rc.ic_fcon, &cir_int);
			else
				x = ast_expr_icon(&x->ae_sp,
				    lc.ic_fcon != rc.ic_fcon, &cir_int);
		}
		*xp = x;
	}
	x->ae_type = &cir_int;
	return 0;
}

static int
bw_semcheck(struct ast_expr **xp, int flags)
{
	char *op;
	struct ast_expr *l, *r, *x = *xp;
	union ir_con lc, rc;
	struct ir_type *lty, *rty, *ty;

	if (!(flags & EXPR_ASG)) {
		if (expr_semcheck(&x->ae_l,
		    flags & (EXPR_ADDRCON | EXPR_INSZOF)))
			return 1;
		if (expr_semcheck(&x->ae_r,
		    flags & (EXPR_ADDRCON | EXPR_INSZOF)))
			return 1;
	}
	lty = ir_type_dequal(x->ae_l->ae_type);
	rty = ir_type_dequal(x->ae_r->ae_type);
	if (x->ae_op == AST_BWAND || x->ae_op == AST_ANDASG)
		op = "&";
	else if (x->ae_op == AST_BWOR || x->ae_op == AST_ORASG)
		op = "|";
	else
		op = "^";
	if (!tyisinteger(lty) || !tyisinteger(rty)) {
		errp(&x->ae_sp,
		    flags & EXPR_ASG ? "invalid operands to %s=" :
		    "invalid operands to binary %s", op);
		return 1;
	}
	if (flags & EXPR_ASG) {
		x->ae_type = x->ae_l->ae_type;
		return 0;
	}

	ty = tybinconv(lty, rty);
	l = x->ae_l;
	r = x->ae_r;
	if (l->ae_op == AST_ICON && r->ae_op == AST_ICON) {
		lc = l->ae_con;
		rc = r->ae_con;
		ir_con_cast(&lc, lty, ty);
		ir_con_cast(&rc, rty, ty);
		if (IR_ISSIGNED(ty)) {
			if (x->ae_op == AST_BWAND)
				x = ast_expr_icon(&x->ae_sp,
				    lc.ic_icon & rc.ic_icon, ty);
			else if (x->ae_op == AST_BWOR)
				x = ast_expr_icon(&x->ae_sp,
				    lc.ic_icon | rc.ic_icon, ty);
			else
				x = ast_expr_icon(&x->ae_sp,
				    lc.ic_icon ^ rc.ic_icon, ty);

		} else {
			if (x->ae_op == AST_BWAND)
				x = ast_expr_ucon(&x->ae_sp,
				    lc.ic_ucon & rc.ic_ucon, ty);
			else if (x->ae_op == AST_BWOR)
				x = ast_expr_ucon(&x->ae_sp,
				    lc.ic_ucon | rc.ic_ucon, ty);
			else
				x = ast_expr_ucon(&x->ae_sp,
				    lc.ic_ucon ^ rc.ic_ucon, ty);
		}
		*xp = x;
		ir_con_cast(&x->ae_con, ty, ty);
	}
	x->ae_type = ty;
	return 0;
}

static int
bool_semcheck(struct ast_expr **xp, int flags)
{
	int val;
	struct ast_expr *l, *r, *x = *xp;
	union ir_con lc, rc;

	if (expr_semcheck(&x->ae_l, flags & (EXPR_ADDRCON | EXPR_INSZOF)))
		return 1;
	if (expr_semcheck(&x->ae_r, flags & (EXPR_ADDRCON | EXPR_INSZOF)))
		return 1;
	if (!tyisscalar(x->ae_l->ae_type) || !tyisscalar(x->ae_r->ae_type)) {
		errp(&x->ae_sp, "invalid operands of %s",
		    x->ae_op == AST_ANDAND ? "&&" : "||");
		return 1;
	}

	l = x->ae_l;
	r = x->ae_r;
	lc = r->ae_con;
	rc = r->ae_con;
	if (ISCON(l)) {
		if (IR_ISSIGNED(l->ae_type)) {
			if (x->ae_op == AST_ANDAND && lc.ic_icon == 0) {
				x = ast_expr_icon(&x->ae_sp, 0, &cir_int);
				*xp = x;
				return 0;
			}
			if (x->ae_op == AST_OROR && lc.ic_icon != 0) {
				x = ast_expr_icon(&x->ae_sp, 1, &cir_int);
				*xp = x;
				return 0;
			}
		} else if (IR_ISUNSIGNED(l->ae_type)) {
			if (x->ae_op == AST_ANDAND && lc.ic_ucon == 0) {
				x = ast_expr_icon(&x->ae_sp, 0, &cir_int);
				*xp = x;
				return 0;
			}
			if (x->ae_op == AST_OROR && lc.ic_ucon != 0) {
				x = ast_expr_icon(&x->ae_sp, 1, &cir_int);
				*xp = x;
				return 0;
			}
		} else {
			if (x->ae_op == AST_ANDAND && lc.ic_fcon == 0) {
				x = ast_expr_icon(&x->ae_sp, 0, &cir_int);
				*xp = x;
				return 0;
			}
			if (x->ae_op == AST_OROR && lc.ic_fcon != 0) {
				x = ast_expr_icon(&x->ae_sp, 1, &cir_int);
				*xp = x;
				return 0;
			}
		}
	}

	if (!ISCON(l) || !ISCON(r)) {
		x->ae_type = &cir_int;
		return 0;
	}

	if (IR_ISSIGNED(r->ae_type))
		val = rc.ic_icon != 0;
	else if (IR_ISUNSIGNED(r->ae_type))
		val = rc.ic_ucon != 0;
	else
		val = rc.ic_fcon != 0;
	*xp = x = ast_expr_icon(&x->ae_sp, val, &cir_int);
	return 0;
}

static int
cond_semcheck(struct ast_expr **xp, int flags)
{
	int qual;
	struct ast_expr *l, *x = *xp;
	struct ir_type *lty, *rty, *ty;

	if (expr_semcheck(&x->ae_l, flags & (EXPR_ADDRCON | EXPR_INSZOF)))
		return 1;
	if (expr_semcheck(&x->ae_m, flags & (EXPR_ADDRCON | EXPR_INSZOF)))
		return 1;
	if (expr_semcheck(&x->ae_r, flags & (EXPR_ADDRCON | EXPR_INSZOF)))
		return 1;
	if (!tyisscalar(x->ae_l->ae_type)) {
		errp(&x->ae_sp, "first operand of ?: must have scalar type");
		return 1;
	}

	lty = ir_type_dequal(x->ae_m->ae_type);
	rty = ir_type_dequal(x->ae_r->ae_type);
	if (tyisarith(lty) && tyisarith(rty)) {
		ty = tybinconv(lty, rty);
		goto fold;
	}
	if ((IR_ISSTRUCT(lty) || IR_ISUNION(lty)) && lty == rty) {
		ty = lty;
		goto fold;
	}
	if (IR_ISVOID(lty) && IR_ISVOID(rty)) {
		ty = lty;
		goto fold;
	}
	if (IR_ISPTR(lty) && IR_ISPTR(rty)) {
		qual = 0;
		if (IR_ISQUAL(lty->it_base))
			qual = lty->it_base->it_op;
		if (IR_ISQUAL(rty->it_base))
			qual |= rty->it_base->it_op;

		lty = ir_type_dequal(lty->it_base);
		rty = ir_type_dequal(rty->it_base);
		if (IR_ISFUNTY(lty) || IR_ISFUNTY(rty)) {
			errp(&x->ae_sp, "invalid operands of ?:");
			return 1;
		}
		if (tyiscompat(lty, rty)) {
			ty = ir_type_ptr(ir_type_qual(lty, qual));
			goto fold;
		}
		if (IR_ISVOID(lty) || IR_ISVOID(rty)) {
			ty = ir_type_ptr(ir_type_qual(&cir_void, qual));
			goto fold;
		}
	}
	if (IR_ISPTR(lty) && x->ae_r->ae_op == AST_ICON) {
		qual = 0;
		if (IR_ISQUAL(lty->it_base))
			qual = lty->it_base->it_op;
		if (IR_ISQUAL(rty))
			qual |= rty->it_base->it_op;
		if (IR_ISSIGNED(x->ae_r->ae_type) &&
		    x->ae_r->ae_con.ic_icon == 0) {
			ty = ir_type_ptr(ir_type_qual(lty, qual));
			goto fold;
		}
		if (x->ae_r->ae_con.ic_ucon == 0) {
			ty = ir_type_ptr(ir_type_qual(lty, qual));
			goto fold;
		}
	}
	if (IR_ISPTR(rty) && x->ae_m->ae_op == AST_ICON) {
		qual = 0;
		if (IR_ISQUAL(lty))
			qual = lty->it_base->it_op;
		if (IR_ISQUAL(rty->it_base))
			qual |= rty->it_base->it_op;
		if (IR_ISSIGNED(x->ae_m->ae_type) &&
		    x->ae_m->ae_con.ic_icon == 0) {
			ty = ir_type_ptr(ir_type_qual(rty, qual));
			goto fold;
		}
		if (x->ae_m->ae_con.ic_ucon == 0) {
			ty = ir_type_ptr(ir_type_qual(rty, qual));
			goto fold;
		}
	}
	errp(&x->ae_sp, "invalid operands of ?:");
	return 1;

fold:
	l = x->ae_l;
	if (ISCON(l)) {
		if (IR_ISSIGNED(l->ae_type)) {
			if (l->ae_con.ic_icon == 0)
				*xp = x = x->ae_r;
			else
				*xp = x = x->ae_m;
		} else if (IR_ISUNSIGNED(l->ae_type)) {
			if (l->ae_con.ic_ucon == 0)
				*xp = x = x->ae_r;
			else
				*xp = x = x->ae_m;
		} else {
			if (l->ae_con.ic_fcon == 0)
				*xp = x = x->ae_r;
			else
				*xp = x = x->ae_m;
		}
	} else if ((flags & EXPR_ADDRCON) && !(flags & EXPR_INSZOF)) {
		errp(&x->ae_sp, "expression is not an address constant");
		return 1;
	}
	x->ae_type = ty;
	return 0;
}

static int
asg_semcheck(struct ast_expr **xp, int flags)
{
	struct ast_expr *x = *xp;
	struct ir_type *lty, *rty;

	if ((flags & EXPR_ADDRCON) && !(flags & EXPR_INSZOF)) {
		errp(&x->ae_sp, "expression is not an address constant");
		return 1;
	}
	flags &= (EXPR_ADDRCON | EXPR_INSZOF);
	if (expr_semcheck(&x->ae_l, flags))
		return 1;
	if (expr_semcheck(&x->ae_r, flags))
		return 1;
	lty = x->ae_l->ae_type;
	rty = x->ae_r->ae_type;
	if (!islvalue(x->ae_l)) {
		errp(&x->ae_sp, "left side of assignment is not an lvalue");
		return 1;
	}
	if (IR_ISCONST(lty)) {
		errp(&x->ae_sp, "left side of assignment is not a modifiable "
		    "lvalue");
		return 1;
	}

	x->ae_type = x->ae_l->ae_type;
	switch (x->ae_op) {
	case AST_ASG:
		if (!canassign(lty, x->ae_r, 0)) {
			errp(&x->ae_sp, "incompatible types in assignment");
			return 1;
		}
		return 0;
	case AST_MULASG:
	case AST_DIVASG:
	case AST_MODASG:
		if (mul_semcheck(xp, flags | EXPR_ASG))
			return 1;
		return 0;
	case AST_ADDASG:
	case AST_SUBASG:
		if (tyisarith(lty) && tyisarith(rty))
			return 0;
		if (tyisobjptr(lty) && tyisinteger(rty))
			return 0;
		errp(&x->ae_sp, "incompatible types in assignment");
		return 1;
	case AST_LSASG:
	case AST_RSASG:
		if (shift_semcheck(xp, flags | EXPR_ASG))
			return 1;
		return 0;
	case AST_ANDASG:
	case AST_XORASG:
	case AST_ORASG:
		if (bw_semcheck(xp, flags | EXPR_ASG))
			return 1;
		return 0;
	default:
		fatalx("asg_semcheck: bad op: %d", x->ae_op);
	}

	return 1;
}

static int
comma_semcheck(struct ast_expr **xp, int flags)
{
	struct ast_expr *x = *xp;

	if ((flags & EXPR_ADDRCON) && !(flags & EXPR_INSZOF)) {
		errp(&x->ae_sp, "not an address constant expression");
		return 1;
	}
	if (expr_semcheck(&x->ae_l, 0))
		return 1;
	if (expr_semcheck(&x->ae_r, 0))
		return 1;
	x->ae_type = x->ae_r->ae_type;
	return 0;
}

static int
expr_semcheck(struct ast_expr **xp, int flags)
{
	int oflags = flags;
	struct ast_expr *x = *xp;

	flags &= ~EXPR_CON;
	switch (x->ae_op) {
	case AST_IDENT:
		flags &= ~(EXPR_NOSZOF | EXPR_ASG);
		if (ident_semcheck(xp, flags))
			return 1;
		break;
	case AST_ICON:
	case AST_FCON:
		break;
	case AST_STRLIT:
		break;
	case AST_SUBSCR:
		flags &= ~(EXPR_NOSZOF | EXPR_INCALL | EXPR_ASG);
		if (subscr_semcheck(xp, flags))
			return 1;
		break;
	case AST_CALL:
		flags &= ~(EXPR_NOSZOF | EXPR_INCALL | EXPR_ASG);
		if (call_semcheck(xp, flags))
			return 1;
		break;
	case AST_SOUDIR:
	case AST_SOUIND:
		flags &= ~(EXPR_INCALL | EXPR_ASG);
		if (souref_semcheck(xp, flags))
			return 1;
		break;
	case AST_POSTINC:
	case AST_POSTDEC:
	case AST_PREINC:
	case AST_PREDEC:
		flags &= ~(EXPR_NOSZOF | EXPR_INCALL | EXPR_ASG);
		if (incdec_semcheck(xp, flags))
			return 1;
		break;
	case AST_ADDROF:
		flags &= ~(EXPR_NOSZOF | EXPR_INCALL | EXPR_ASG);
		if (addrof_semcheck(xp, flags))
			return 1;
		break;
	case AST_DEREF:
		flags &= ~(EXPR_NOSZOF | EXPR_INCALL | EXPR_ASG);
		if (deref_semcheck(xp, flags))
			return 1;
		break;
	case AST_UPLUS:
	case AST_UMINUS:
		flags &= ~(EXPR_NOSZOF | EXPR_INCALL | EXPR_ASG);
		if (uplusminus_semcheck(xp, flags))
			return 1;
		break;
	case AST_BITFLIP:
	case AST_NOT:
		flags &= ~(EXPR_NOSZOF | EXPR_INCALL | EXPR_ASG);
		if (not_semcheck(xp, flags))
			return 1;
		break;
	case AST_SIZEOFX:
	case AST_SIZEOFT:
		flags &= ~(EXPR_NOSZOF | EXPR_INCALL | EXPR_ASG);
		if (sizeof_semcheck(xp, flags))
			return 1;
		break;
	case AST_CAST:
		flags &= ~(EXPR_NOSZOF | EXPR_INCALL | EXPR_ASG);
		if (cast_semcheck(xp, flags))
			return 1;
		break;
	case AST_MUL:
	case AST_DIV:
	case AST_MOD:
		flags &= ~(EXPR_NOSZOF | EXPR_INCALL | EXPR_ASG);
		if (mul_semcheck(xp, flags))
			return 1;
		break;
	case AST_ADD:
	case AST_SUB:
		flags &= ~(EXPR_NOSZOF | EXPR_INCALL | EXPR_ASG);
		if (add_semcheck(xp, flags))
			return 1;
		break;
	case AST_LS:
	case AST_RS:
		flags &= ~(EXPR_NOSZOF | EXPR_INCALL | EXPR_ASG);
		if (shift_semcheck(xp, flags))
			return 1;
		break;
	case AST_LT:
	case AST_GT:
	case AST_LE:
	case AST_GE:
		flags &= ~(EXPR_NOSZOF | EXPR_INCALL | EXPR_ASG);
		if (rel_semcheck(xp, flags))
			return 1;
		break;
	case AST_EQ:
	case AST_NE:
		flags &= ~(EXPR_NOSZOF | EXPR_INCALL | EXPR_ASG);
		if (eq_semcheck(xp, flags))
			return 1;
		break;
	case AST_BWAND:
	case AST_BWXOR:
	case AST_BWOR:
		flags &= ~(EXPR_NOSZOF | EXPR_INCALL | EXPR_ASG);
		if (bw_semcheck(xp, flags))
			return 1;
		break;
	case AST_ANDAND:
	case AST_OROR:
		flags &= ~(EXPR_NOSZOF | EXPR_INCALL | EXPR_ASG);
		if (bool_semcheck(xp, flags))
			return 1;
		break;
	case AST_COND:
		flags &= ~(EXPR_NOSZOF | EXPR_INCALL | EXPR_ASG);
		if (cond_semcheck(xp, flags))
			return 1;
		break;
	case AST_ASG:
	case AST_MULASG:
	case AST_DIVASG:
	case AST_MODASG:
	case AST_ADDASG:
	case AST_SUBASG:
	case AST_LSASG:
	case AST_RSASG:
	case AST_ANDASG:
	case AST_XORASG:
	case AST_ORASG:
		flags &= ~(EXPR_NOSZOF | EXPR_INCALL | EXPR_ASG);
		if (asg_semcheck(xp, flags))
			return 1;
		break;
	case AST_COMMA:
		flags &= ~(EXPR_NOSZOF | EXPR_INCALL | EXPR_ASG);
		if (comma_semcheck(xp, flags))
			return 1;
		break;
	default:
		fatalx("expr_semcheck: bad op/not yet: %d", x->ae_op);
	}

	x = *xp;
	if ((oflags & EXPR_CON) && x->ae_op != AST_ICON &&
	    x->ae_op != AST_FCON) {
		errp(&x->ae_sp, "expression is not a constant");
		return 1;
	}
	if ((oflags & EXPR_INT) && !IR_ISINTEGER(x->ae_type)) {
		errp(&x->ae_sp, "expression is not an integer");
		return 1;
	}
	return 0;
}

static void
paramlist_semcheck(struct ast_list *list, struct ir_type *funty, int flags)
{
	char *ident;
	int i, needdecla, qual;
	struct ast_decl *decl;
	struct ast_decla *decla;
	struct ir_type *type;
	struct ir_typelm *elm;
	struct symbol *sym;

	if (list->al_ellipsis)
		ir_type_setflags(funty, IR_ELLIPSIS);
	if (list->al_op == AST_IDLIST) {
		if (!(flags & FLG_FUNDEF))
			errp(&list->al_sp, "identifier lists are only "
			    "permitted for function definitions");
		else
			ir_type_setflags(funty, IR_KRFUNC);
	}

	i = 0;
	SIMPLEQ_FOREACH(decl, &list->al_decls, ad_link) {
		if (list->al_op == AST_PARLIST) {
			type = declspecs_semcheck(decl->ad_ds, 0, &needdecla);
			if (decl->ad_ds->ad_fnspec)
				errp(&decl->ad_ds->ad_sp, "no function "
				    "specifiers permitted in parameter "
				    "declarations");
			if (decl->ad_ds->ad_sclass &&
			    decl->ad_ds->ad_sclass != AST_SC_REGISTER)
				errp(&decl->ad_ds->ad_sp, "only register is "
				    "permitted as storage class for "
				    "parameters");
			if ((qual = cqualtoirqual(decl->ad_ds->ad_tyqual)))
				type = ir_type_qual(type, qual);
		} else
			type = &cir_id;
		
		ident = NULL;
		if ((decla = SIMPLEQ_FIRST(&decl->ad_declas)) != NULL)
			type = decla_semcheck(decla, type, &ident, 0);

		if (IR_ISFUNTY(type))
			type = ir_type_ptr(type);
		if (IR_ISARR(type))
			type = ir_type_ptr(type->it_base);

		if (type == &cir_void &&
		    (i != 0 || SIMPLEQ_NEXT(decl, ad_link) != NULL))
			errp(&decla->ad_sp, "bad use of void type in "
			    "parameter list");
		if (ident != NULL) {
			if ((sym = symlookup(ident, NSORD)) != NULL &&
			    sym->s_scope == curscope) {
				errp(&decla->ad_sp, "`%s' redeclared",
				    ident);
				continue;
			}
			lastpar = decla->ad_sym = symenter(ident, type, NSORD);
		}
		if (i == 0 && type == &cir_void)
			break;
		elm = ir_typelm(ident, type, 0, 0);
		ir_type_newelm(funty, elm);
	}

	if (!(flags & FLG_FUNDEF))
		closescope();
}

static void
compound_semcheck(struct ir_type *funty, struct ast_stmt *stmt,
    struct ast_stmt *swtch, int flags)
{
	struct ast_stmt *s;

	openscope();
	SIMPLEQ_FOREACH(s, &stmt->as_stmts, as_link) {
		if (s->as_op == AST_DECLSTMT)
			decl_semcheck(s->as_decl, 0);
		else
			stmt_semcheck(funty, s, swtch, flags);
	}
	closescope();
}

static void
stmt_semcheck(struct ir_type *funty, struct ast_stmt *stmt,
    struct ast_stmt *swtch, int flags)
{
	struct symbol *sym;

	switch (stmt->as_op) {
	case AST_COMPOUND:
		compound_semcheck(funty, stmt, swtch, flags);
		break;
	case AST_EXPR:
		if (stmt->as_exprs[0] != NULL)
			expr_semcheck(&stmt->as_exprs[0], 0);
		break;
	case AST_LABEL:
		if ((sym = symlookup(stmt->as_ident, NSLBL)) != NULL) {
			if (sym->s_type == NULL)
				sym->s_type = &cir_int;
			else
				errp(&stmt->as_sp, "`%s' redeclared",
				    stmt->as_ident);
		} else
			sym = symenter(stmt->as_ident, &cir_int, NSLBL);
		stmt->as_sym = sym;
		stmt_semcheck(funty, stmt->as_stmt1, swtch, flags);
		break;
	case AST_CASE:
		if (swtch == NULL) {
			errp(&stmt->as_sp, "case statement not within switch");
			break;
		}
		if (expr_semcheck(&stmt->as_exprs[0], EXPR_INT | EXPR_CON))
			break;
		ast_case(swtch, stmt->as_stmt1, stmt->as_exprs[0]);
		stmt_semcheck(funty, stmt->as_stmt1, swtch, flags);
		break;
	case AST_DEFAULT:
		if (swtch == NULL) {
			errp(&stmt->as_sp, "default statement not within "
			    "switch");
			break;
		}
		if (swtch->as_stmt2 != NULL)
			errp(&stmt->as_sp, "multiple default statements in "
			    "switch");
		swtch->as_stmt2 = stmt;
		stmt_semcheck(funty, stmt->as_stmt1, swtch, flags);
		break;
	case AST_IF:
		expr_semcheck(&stmt->as_exprs[0], 0);
		stmt_semcheck(funty, stmt->as_stmt1, swtch, flags);
		if (stmt->as_stmt2 != NULL)
			stmt_semcheck(funty, stmt->as_stmt2, swtch, flags);
		break;
	case AST_SWITCH:
		expr_semcheck(&stmt->as_exprs[0], EXPR_INT);
		stmt_semcheck(funty, stmt->as_stmt1, stmt, flags | FLG_CANBRK);
		break;
	case AST_WHILE:
	case AST_DO:
		expr_semcheck(&stmt->as_exprs[0], 0);
		stmt_semcheck(funty, stmt->as_stmt1, swtch,
		    flags | FLG_CANCONT | FLG_CANBRK);
		break;
	case AST_FOR:
		if (stmt->as_exprs[0] != NULL)
			expr_semcheck(&stmt->as_exprs[0], 0);
		if (stmt->as_exprs[1] != NULL)
			expr_semcheck(&stmt->as_exprs[1], 0);
		if (stmt->as_exprs[2] != NULL)
			expr_semcheck(&stmt->as_exprs[2], 0);

		stmt_semcheck(funty, stmt->as_stmt1, swtch,
		    flags | FLG_CANCONT | FLG_CANBRK);
		break;
	case AST_GOTO:
		if ((sym = symlookup(stmt->as_ident, NSLBL)) == NULL) {
			sym = symenter(stmt->as_ident, NULL, NSLBL);
			SPCPY(&sym->s_lblpos, &stmt->as_sp);
		}
		stmt->as_sym = sym;
		break;
	case AST_CONTINUE:
		if (!(flags & FLG_CANCONT))
			errp(&stmt->as_sp, "continue statement not within "
			    "loop");
		break;
	case AST_BREAK:
		if (!(flags & FLG_CANBRK))
			errp(&stmt->as_sp, "break statement not within loop "
			    "or switch");
		break;
	case AST_RETURN:
		if (stmt->as_exprs[0] != NULL) {
			if (IR_ISVOID(funty->it_base)) {
				errp(&stmt->as_sp, "non-void return in "
				    "function returning void");
				break;
			}
			if (expr_semcheck(&stmt->as_exprs[0], 0))
				break;
			if (!canassign(funty->it_base, stmt->as_exprs[0], 0))
				errp(&stmt->as_sp, "incompatible return "
				    "expression");
		} else if (!IR_ISVOID(funty->it_base))
			errp(&stmt->as_sp, "missing return value in "
			    "non-void function");
		break;
	default:
		fatalx("stmt_semcheck: bad op/not yet: %s", stmt->as_op);
	}
}
