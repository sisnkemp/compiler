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
#include <stdlib.h>

#include "comp/comp.h"
#include "comp/ir.h"

#include "lang.c/c.h"

SIMPLEQ_HEAD(initq, ast_init);

static void chaininit(struct ast_init *, struct initq *);
static void sortinits(struct ast_init *, struct initq *);
static void globalinit(struct initq *, struct symbol *);
static void bfinit(struct ir_func *, struct ir_insnq *, struct ir_expr *,
    struct ir_expr *, struct ast_init *);
static void unalginit(struct ir_func *, struct ir_insnq *, struct ir_expr *,
    struct ir_expr *, struct ast_init *);
static void localinit(struct ir_func *, struct ir_insnq *, struct initq *,
    struct symbol *);
static char *uniquename(char *);

static void decl_gencode(struct ir_func *, struct ir_insnq *,
    struct ast_decl *);
static void func_gencode(struct ast_decl *);

static void compound_gencode(struct ir_func *, struct ir_insnq *,
    struct ast_stmt *);
static void if_gencode(struct ir_func *, struct ir_insnq *, struct ast_stmt *);
static void while_gencode(struct ir_func *, struct ir_insnq *,
    struct ast_stmt *);
static void do_gencode(struct ir_func *, struct ir_insnq *, struct ast_stmt *);
static void for_gencode(struct ir_func *, struct ir_insnq *,
    struct ast_stmt *);
static void switch_gencode(struct ir_func *, struct ir_insnq *,
    struct ast_stmt *);
static void stmt_gencode(struct ir_func *, struct ir_insnq *,
    struct ast_stmt *);

static void jump_gencode(struct ir_func *, struct ir_insnq *,
    struct ir_insn **, struct ir_insn **, struct ast_expr *, int);

#define CANSKIP(flags, x)					\
	((flags & GC_FLG_DISCARD) && !(x->ae_flags & AST_SIDEEFF))

static struct ir_expr *ident_gencode(struct ir_func *, struct ir_insnq *,
    struct ast_expr *, int);
static struct ir_expr *subscr_gencode(struct ir_func *, struct ir_insnq *,
    struct ast_expr *, int);
static struct ir_symbol *indcall_gencode(struct ir_func *, struct ir_insnq *,
    struct ast_expr *);
static struct ir_expr *builtin_gencode(struct ir_func *, struct ir_insnq *,
    struct ast_expr *, int);
static struct ir_expr *call_gencode(struct ir_func *, struct ir_insnq *,
    struct ast_expr *, int);
static struct ir_expr *souref_gencode(struct ir_func *, struct ir_insnq *,
    struct ast_expr *, int);
static struct ir_expr *incdec_gencode(struct ir_func *, struct ir_insnq *,
    struct ast_expr *, int);
static struct ir_expr *deref_gencode(struct ir_func *, struct ir_insnq *,
    struct ast_expr *, int);
static struct ir_expr *uplusminus_gencode(struct ir_func *, struct ir_insnq *,
    struct ast_expr *, int);
static struct ir_expr *bitflip_gencode(struct ir_func *, struct ir_insnq *,
    struct ast_expr *, int);
static struct ir_expr *cast_gencode(struct ir_func *, struct ir_insnq *,
    struct ast_expr *, int);
static int bin_gencode(struct ir_func *, struct ir_insnq *, struct ast_expr *,
    int, struct ir_expr **, struct ir_expr **, struct ir_expr **);
static struct ir_expr *mul_gencode(struct ir_func *, struct ir_insnq *,
    struct ast_expr *, struct ir_expr **, int);
static struct ir_expr *add_gencode(struct ir_func *, struct ir_insnq *,
    struct ast_expr *, struct ir_expr **, int);
static struct ir_expr *shift_gencode(struct ir_func *, struct ir_insnq *,
    struct ast_expr *, struct ir_expr **, int);
static struct ir_expr *bw_gencode(struct ir_func *, struct ir_insnq *,
    struct ast_expr *, struct ir_expr **, int);
static struct ir_expr *bool_gencode(struct ir_func *, struct ir_insnq *,
    struct ast_expr *, int);
static struct ir_expr *cond_gencode(struct ir_func *, struct ir_insnq *,
    struct ast_expr *, int);
static struct ir_expr *asg_gencode(struct ir_func *, struct ir_insnq *,
    struct ast_expr *, int);
static struct ir_expr *comma_gencode(struct ir_func *, struct ir_insnq *,
    struct ast_expr *, int);

/* XXX */
static struct ir_symbol *souretparm;
static struct ir_symbol *souretvar;
static struct ir_symbol *memcpysym;

void
ast_gencode(void)
{
	struct ast_decl *decl;

	irprog = ir_prog();
	SIMPLEQ_FOREACH(decl, &ast_program, ad_link)
		decl_gencode(NULL, NULL, decl);
}

static void
chaininit(struct ast_init *init, struct initq *initq)
{
	struct ast_init *i;

	if (init->ai_op == AST_SIMPLEINIT) {
		SIMPLEQ_INSERT_TAIL(initq, init, ai_sortlink);
		return;
	}

	SIMPLEQ_FOREACH(i, &init->ai_inits, ai_link)
		chaininit(i, initq);
}

/*
 * Sort the initializers into ascending order of ai_bitoff. Since we don't
 * support designators yet, this list is automatically in order. Chain the
 * simple initializers together with initq as its head.
 */
static void
sortinits(struct ast_init *init, struct initq *initq)
{
	chaininit(init, initq);
}

static void
globalinit(struct initq *initq, struct symbol *sym)
{
	int op, ro = 0;
	size_t len;
	struct ast_init *init;
	struct ir_expr *irx;
	struct ir_func *oldfn;
	struct ir_init *irinit;
	struct ir_symbol *irsym;

	/* XXX */
	oldfn = irfunc;
	irfunc = NULL;

	irinit = ir_init(sym->s_irsym);
	if (IR_ISCONST(sym->s_type))
		ro = 1;
	SIMPLEQ_FOREACH(init, initq, ai_sortlink) {
		if (init->ai_expr->ae_op == AST_STRLIT) {
			if (IR_ISPTR(init->ai_objty)) {
				irsym = ir_cstrsym(init->ai_expr->ae_str,
				    init->ai_expr->ae_strlen,
				    init->ai_expr->ae_type);
				irx = ir_addr(IR_GADDR, irsym);
				ir_initelm_expr(irinit, irx, init->ai_bitoff,
				    init->ai_bitsize);
				continue;
			}
			if (init->ai_objty->it_size <
			    init->ai_expr->ae_strlen) {
				len = init->ai_objty->it_size;
				op = IR_INIT_STR;
			} else {
				len = init->ai_expr->ae_strlen;
				op = IR_INIT_CSTR;
			}
			ir_initelm_str(op, irinit, init->ai_expr->ae_str, len,
			    init->ai_bitoff);
			continue;
		}


		irx = ast_expr_gencode(NULL, NULL, init->ai_expr, 0);
		irx = ir_cast(irx, ir_type_dequal(init->ai_objty));
		ir_initelm_expr(irinit, irx, init->ai_bitoff,
		    init->ai_bitsize);
	}
	irfunc = oldfn;
	ir_prog_addinit(irprog, ro, irinit);
}

static void
bfinit(struct ir_func *fn, struct ir_insnq *iq, struct ir_expr *dst,
    struct ir_expr *r, struct ast_init *init)
{
	fatalx("bfinit not yet");
}

static void
unalginit(struct ir_func *fn, struct ir_insnq *iq, struct ir_expr *dst,
    struct ir_expr *r, struct ast_init *init)
{
	fatalx("unalginit");
}

static void
localinit(struct ir_func *fn, struct ir_insnq *iq, struct initq *initq,
    struct symbol *sym)
{
	struct ast_init *init;
	union ir_con con;
	struct ir_expr *base, *dst, *r;
	struct ir_type *ty;

	ty = sym->s_type;
	if (tyisscalar(ty)) {
		init = SIMPLEQ_FIRST(initq);
		dst = ir_var(IR_LVAR, sym->s_irsym);
		r = ast_expr_gencode(fn, iq, init->ai_expr, 0);
		ir_insnq_enq(iq, ir_asg(dst, r));
		return;
	}

	r = ir_var(IR_LADDR, sym->s_irsym);
	base = ir_newvreg(fn, &ir_ptr);
	ir_insnq_enq(iq, ir_asg(base, r));
	SIMPLEQ_FOREACH(init, initq, ai_sortlink) {
		if (init->ai_bitoff == 0)
			dst = ir_virtreg(base->ie_sym);
		else {
			dst = ir_virtreg(base->ie_sym);
			con.ic_ucon = init->ai_bitoff / 8;
			r = ir_con(IR_ICON, con, &cir_uintptr_t);
			r = ir_bin(IR_ADD, dst, r, &cir_uintptr_t);
			dst = ir_newvreg(fn, &ir_ptr);
			ir_insnq_enq(iq, ir_asg(dst, r));
			dst = ir_virtreg(dst->ie_sym);
		}

		r = ast_expr_gencode(fn, iq, init->ai_expr, 0);
		if (init->ai_bitoff % 8 || init->ai_bitsize % 8) {
			bfinit(fn, iq, dst, r, init);
			return;
		}
		if (init->ai_bitoff % (init->ai_objty->it_align * 8)) {
			unalginit(fn, iq, dst, r, init);
			return;
		}
		ir_insnq_enq(iq, ir_store(dst, r));
	}
}

static char *
uniquename(char *p)
{
	char *buf, *rv;
	static int id = 1;

	asprintf(&buf, "%s.%d", p, id++);
	if (buf == NULL)
		fatalx("uniquename: asprintf");
	rv = ntenter(&names, buf);
	free(buf);
	return rv;
}

static void
decl_gencode(struct ir_func *fn, struct ir_insnq *iq, struct ast_decl *decl)
{
	char *p;
	struct ast_decla *decla;
	struct ir_symbol *irsym;
	struct initq initq;
	struct ir_type *ty;
	struct symbol *sym;

	if (decl->ad_stmt != NULL) {
		func_gencode(decl);
		return;
	}
	SIMPLEQ_FOREACH(decla, &decl->ad_declas, ad_link) {
		sym = decla->ad_sym;
		if (IR_ISFUNTY(sym->s_type) &&
		    !(sym->s_type->it_flags & IR_FUNDEF)) {
			ty = ir_type_dequal(sym->s_type);
			ty = ir_type_dequal(ty->it_base);

			/* XXX: Fix when struct assignments are fixed */
			if (IR_ISSOU(ty))
				ty = &ir_void;

			sym->s_irsym = ir_symbol(IR_FUNSYM, sym->s_ident,
			    0, 0, ty);
			continue;
		}
		ty = ir_type_dequal(sym->s_type);
		if (!(sym->s_flags & (SYM_DEF | SYM_TENTDEF)))
			continue;
		if (sym->s_sclass == AST_SC_STATIC && sym->s_scope > 0) {
			p = uniquename(sym->s_ident);
			irsym = ir_symbol(IR_VARSYM, p, ty->it_size,
			    ty->it_align, ty);
		} else if (sym->s_sclass != AST_SC_TYPEDEF)
			irsym = ir_symbol(IR_VARSYM, sym->s_ident,
			    ty->it_size, ty->it_align, ty);
		else
			continue;
		if (IR_ISVOLAT(sym->s_type))
			ir_symbol_setflags(irsym, IR_SYM_VOLAT);
		if (sym->s_sclass == AST_SC_EXTERN)
			ir_symbol_setflags(irsym, IR_SYM_GLOBL);
		sym->s_irsym = irsym;
		if (fn != NULL && sym->s_sclass != AST_SC_STATIC)
			ir_func_addvar(fn, irsym);
		else if (decla->ad_init == NULL)
			ir_prog_addsym(irprog, irsym);
		if (decla->ad_init != NULL) {
			SIMPLEQ_INIT(&initq);
			sortinits(decla->ad_init, &initq);
			if (sym->s_scope == 0 || sym->s_sclass == AST_SC_STATIC)
				globalinit(&initq, sym);
			else
				localinit(fn, iq, &initq, sym);
		}

	}
}

static void
func_gencode(struct ast_decl *decl)
{
	struct ast_decla *decla;
	struct ast_decl *parmdecl;
	struct symbol *sym;
	struct ir_func *irfn;
	struct ir_insnq iq;
	struct ir_symbol *irsym;
	struct ir_type *fnty;

	decla = SIMPLEQ_FIRST(&decl->ad_declas);
	sym = decla->ad_sym;
	while (decla != NULL) {
		if (decla->ad_op == AST_IDDECLA || decla->ad_op == AST_FNDECLA)
			break;
		decla = decla->ad_decla;
	}
	if (decla == NULL || decla->ad_op != AST_FNDECLA)
		fatalx("func_gencode");
	if (sym->s_sclass == AST_SC_STATIC && !(sym->s_flags & SYM_USED))
		return;

	/* XXX: Fix when struct assignments are fixed */
	fnty = sym->s_type->it_base;
	if ((irsym = sym->s_irsym) == NULL) {
		if (IR_ISSOU(fnty))
			irsym = ir_symbol(IR_FUNSYM, sym->s_ident, 0, 0,
			    &ir_void);
		else
			irsym = ir_symbol(IR_FUNSYM, sym->s_ident, 0, 0, fnty);
		sym->s_irsym = irsym;
	}
	if (sym->s_sclass == AST_SC_EXTERN)
		ir_symbol_setflags(irsym, IR_SYM_GLOBL);
	if (sym->s_type->it_flags & IR_KRFUNC)
		fatalx("K&R functions not yet");
	irfn = ir_func(irsym);
	if (sym->s_type->it_flags & IR_ELLIPSIS)
		ir_func_setflags(irfn, IR_FUNC_VARARGS);

	if (IR_ISSOU(fnty)) {
		irsym = ir_symbol(IR_VARSYM, NULL, IR_PTR_SIZE, IR_PTR_ALIGN,
		    &ir_ptr);
		ir_func_addparm(irfn, irsym);
		souretparm = irsym;
	}
	SIMPLEQ_FOREACH(parmdecl, &decla->ad_parms->al_decls, ad_link) {
		decla = SIMPLEQ_FIRST(&parmdecl->ad_declas);
		if (decla == NULL)
			continue;
		sym = decla->ad_sym;
		irsym = ir_symbol(IR_VARSYM, sym->s_ident,
		    sym->s_type->it_size, sym->s_type->it_align,
		    ir_type_dequal(sym->s_type));
		if (IR_ISVOLAT(sym->s_type))
			ir_symbol_setflags(irsym, IR_SYM_VOLAT);
		sym->s_irsym = irsym;
		ir_func_addparm(irfn, irsym);
	}

	ir_insnq_init(&iq);
	compound_gencode(irfn, &iq, decl->ad_stmt);
	ir_func_enqinsns(irfn, &iq);
	ir_prog_addfunc(irprog, irfn);
	ir_func_finish(irfn);
}

static void
compound_gencode(struct ir_func *fn, struct ir_insnq *iq,
    struct ast_stmt *stmt)
{
	struct ast_stmt *s;

	SIMPLEQ_FOREACH(s, &stmt->as_stmts, as_link)
		stmt_gencode(fn, iq, s);
}

static struct ir_insn *brklbl;
static struct ir_insn *cntlbl;
static struct ir_insn *dfltlbl;

static void
if_gencode(struct ir_func *fn, struct ir_insnq *iq, struct ast_stmt *stmt)
{
	struct ir_insn *iflbl, *outlbl, *thenlbl;

	iflbl = thenlbl = NULL;
	jump_gencode(fn, iq, &iflbl, &thenlbl, stmt->as_exprs[0], 0);
	ir_insnq_enq(iq, iflbl);
	stmt_gencode(fn, iq, stmt->as_stmt1);
	if (stmt->as_stmt2 != NULL) {
		outlbl = ir_lbl();
		ir_insnq_enq(iq, ir_b(outlbl));
		ir_insnq_enq(iq, thenlbl);
		stmt_gencode(fn, iq, stmt->as_stmt2);
	} else
		outlbl = thenlbl;
	ir_insnq_enq(iq, outlbl);
}

static void
while_gencode(struct ir_func *fn, struct ir_insnq *iq, struct ast_stmt *stmt)
{
	struct ir_insn *entlbl;
	struct ir_insnq condq, loopq;

	brklbl = entlbl = NULL;
	cntlbl = ir_lbl();
	ir_insnq_init(&condq);
	ir_insnq_init(&loopq);
	jump_gencode(fn, &condq, &entlbl, &brklbl, stmt->as_exprs[0], 0);
	stmt_gencode(fn, &loopq, stmt->as_stmt1);
	ir_insnq_enq(iq, ir_b(cntlbl));
	ir_insnq_enq(iq, entlbl);
	ir_insnq_enq(&loopq, cntlbl);
	ir_insnq_enq(&condq, brklbl);
	ir_insnq_cat(iq, &loopq);
	ir_insnq_cat(iq, &condq);
}

static void
do_gencode(struct ir_func *fn, struct ir_insnq *iq, struct ast_stmt *stmt)
{
	struct ir_insn *entlbl;
	struct ir_insnq condq, loopq;

	brklbl = entlbl = NULL;
	cntlbl = ir_lbl();
	ir_insnq_init(&condq);
	ir_insnq_init(&loopq);
	jump_gencode(fn, &condq, &entlbl, &brklbl, stmt->as_exprs[0], 0);
	stmt_gencode(fn, &loopq, stmt->as_stmt1);
	ir_insnq_enq(iq, entlbl);
	ir_insnq_enq(&loopq, cntlbl);
	ir_insnq_enq(&condq, brklbl);
	ir_insnq_cat(iq, &loopq);
	ir_insnq_cat(iq, &condq);
}

static void
for_gencode(struct ir_func *fn, struct ir_insnq *iq, struct ast_stmt *stmt)
{
	struct ir_insn *endlbl, *entlbl, *obrklbl, *ocntlbl;
	struct ir_insnq condq, loopq;

	obrklbl = brklbl;
	ocntlbl = cntlbl;
	brklbl = entlbl = NULL;
	cntlbl = ir_lbl();
	endlbl = ir_lbl();
	ir_insnq_init(&condq);
	ir_insnq_init(&loopq);
	if (stmt->as_exprs[1] != NULL)
		jump_gencode(fn, &condq, &entlbl, &brklbl, stmt->as_exprs[1],
		    0);
	else {
		entlbl = ir_lbl();
		brklbl = ir_lbl();
		ir_insnq_enq(&condq, ir_b(entlbl));
	}
	ir_insnq_enq(&condq, brklbl);
	stmt_gencode(fn, &loopq, stmt->as_stmt1);
	ir_insnq_enq(&loopq, cntlbl);
	if (stmt->as_exprs[2] != NULL)
		ast_expr_gencode(fn, &loopq, stmt->as_exprs[2],
		    GC_FLG_DISCARD);
	ir_insnq_enq(&loopq, endlbl);
	if (stmt->as_exprs[0] != NULL)
		ast_expr_gencode(fn, iq, stmt->as_exprs[0], GC_FLG_DISCARD);
	ir_insnq_enq(iq, ir_b(endlbl));
	ir_insnq_enq(iq, entlbl);
	ir_insnq_cat(iq, &loopq);
	ir_insnq_cat(iq, &condq);
	brklbl = obrklbl;
	cntlbl = ocntlbl;
}

/*
 * TODO: To see what must be improved, look in the TODO file what you can
 * find about switch.
 */
static void
switch_gencode(struct ir_func *fn, struct ir_insnq *iq, struct ast_stmt *stmt)
{
	struct ast_case *cas;
	struct ir_expr *reg, *x;
	struct ir_insn *insn;
	struct ir_type *swty;

	if (stmt->as_stmt2 != NULL) {
		dfltlbl = ir_lbl();
		brklbl = ir_lbl();
		stmt->as_stmt2->as_irlbl = dfltlbl;
	} else
		dfltlbl = brklbl = ir_lbl();

	swty = ir_type_dequal(tyuconv(stmt->as_exprs[0]->ae_type));
	x = ast_expr_gencode(fn, iq, stmt->as_exprs[0], 0);
	x = ir_cast(x, swty);
	reg = ir_newvreg(fn, x->ie_type);
	ir_insnq_enq(iq, ir_asg(reg, x));

	SIMPLEQ_FOREACH(cas, &stmt->as_cases, ac_link) {
		x = ir_con(IR_ICON, cas->ac_con, swty);
		reg = ir_virtreg(reg->ie_sym);
		cas->ac_stmt->as_irlbl = ir_lbl();
		insn = ir_bc(IR_BEQ, reg, x, cas->ac_stmt->as_irlbl);
		ir_insnq_enq(iq, insn);
	}

	ir_insnq_enq(iq, ir_b(dfltlbl));
	stmt_gencode(fn, iq, stmt->as_stmt1);

	ir_insnq_enq(iq, brklbl);
}

static void
stmt_gencode(struct ir_func *fn, struct ir_insnq *iq, struct ast_stmt *stmt)
{
	struct ir_expr *reg, *x;
	struct ir_insn *insn;

	struct ir_insn *obrk, *ocnt, *odflt;

	obrk = brklbl;
	ocnt = cntlbl;
	odflt = dfltlbl;

	switch (stmt->as_op) {
	case AST_COMPOUND:
		compound_gencode(fn, iq, stmt);
		break;
	case AST_EXPR:
		if (stmt->as_exprs[0] != NULL)
			ast_expr_gencode(fn, iq, stmt->as_exprs[0],
			    GC_FLG_DISCARD);
		break;
	case AST_LABEL:
		if (stmt->as_sym->s_irlbl != NULL)
			insn = stmt->as_sym->s_irlbl;
		else
			stmt->as_sym->s_irlbl = insn = ir_lbl();
		ir_insnq_enq(iq, insn);
		stmt_gencode(fn, iq, stmt->as_stmt1);
		break;
	case AST_DEFAULT:
		ir_insnq_enq(iq, stmt->as_irlbl);
		stmt_gencode(fn, iq, stmt->as_stmt1);
		break;
	case AST_CASE:
		ir_insnq_enq(iq, stmt->as_stmt1->as_irlbl);
		stmt_gencode(fn, iq, stmt->as_stmt1);
		break;
	case AST_IF:
		if_gencode(fn, iq, stmt);
		break;
	case AST_SWITCH:
		switch_gencode(fn, iq, stmt);
		break;
	case AST_WHILE:
		while_gencode(fn, iq, stmt);
		break;
	case AST_DO:
		do_gencode(fn, iq, stmt);
		break;
	case AST_FOR:
		for_gencode(fn, iq, stmt);
		break;
	case AST_GOTO:
		if (stmt->as_sym->s_irlbl == NULL)
			stmt->as_sym->s_irlbl = insn = ir_lbl();
		else
			insn = stmt->as_sym->s_irlbl;
		ir_insnq_enq(iq, ir_b(insn));
		break;
	case AST_CONTINUE:
	case AST_BREAK:
		insn = ir_b(stmt->as_op == AST_BREAK ? brklbl : cntlbl);
		ir_insnq_enq(iq, insn);
		break;
	case AST_RETURN:
		if (stmt->as_exprs[0] == NULL) {
			ir_insnq_enq(iq, ir_ret(NULL, 0));
			break;
		}
		x = ast_expr_gencode(fn, iq, stmt->as_exprs[0], 0);
		x = ir_cast(x, fn->if_sym->is_type);
		reg = ir_newvreg(fn, x->ie_type);
		ir_insnq_enq(iq, ir_asg(reg, x));
		reg = ir_virtreg(reg->ie_sym);
		ir_insnq_enq(iq, ir_ret(reg, 0));
		break;
	case AST_DECLSTMT:
		decl_gencode(fn, iq, stmt->as_decl);
		break;
	case AST_ASM:
	default:
		fatalx("stmt_gencode: unsupported op: %d", stmt->as_op);
	}

	brklbl = obrk;
	cntlbl = ocnt;
	dfltlbl = odflt;
}

static void
jump_gencode(struct ir_func *fn, struct ir_insnq *iq, struct ir_insn **tp,
    struct ir_insn **fp, struct ast_expr *x, int flags)
{
	int op;
	union ir_con con;
	struct ir_expr *l = NULL, *r = NULL;
	struct ir_insn *f, *t, *tmpf, *tmpt;
	struct ir_type *ty;

	if (CANSKIP(flags, x)) {
		if (*tp == NULL)
			*tp = ir_lbl();
		if (*fp == NULL)
			*fp = ir_lbl();
		return;
	}

	switch (x->ae_op) {
	case AST_NOT:
		jump_gencode(fn, iq, fp, tp, x->ae_l, flags);
		return;
	case AST_LT:
		op = IR_BLT;
		break;
	case AST_GT:
		op = IR_BGT;
		break;
	case AST_LE:
		op = IR_BLE;
		break;
	case AST_GE:
		op = IR_BGE;
		break;
	case AST_EQ:
		op = IR_BEQ;
		break;
	case AST_NE:
		op = IR_BNE;
		break;
	case AST_ANDAND:
	case AST_OROR:
		tmpf = tmpt = NULL;
		if (x->ae_op == AST_ANDAND) {
			jump_gencode(fn, iq, &tmpt,
			    fp, x->ae_l, flags & ~GC_FLG_DISCARD);
			ir_insnq_enq(iq, tmpt);
		} else {
			jump_gencode(fn, iq, tp, &tmpf, x->ae_l,
			    flags & ~GC_FLG_DISCARD);
			ir_insnq_enq(iq, tmpf);
		}

		if (CANSKIP(flags, x->ae_r)) {
			if (*tp == NULL)
				*tp = tmpt;
			else
				*fp = tmpf;
			return;
		}
		jump_gencode(fn, iq, tp, fp, x->ae_r, flags);
		return;
	default:
		if ((l = ast_expr_gencode(fn, iq, x, flags)) == NULL) {
			if (*tp == NULL)
				*tp = ir_lbl();
			if (*fp == NULL)
				*fp = ir_lbl();
			return;
		}
		ty = tyuconv(l->ie_type);
		l = ir_cast(l, ty);
		if (IR_ISSIGNED(ty)) {
			con.ic_icon = 0;
			r = ir_con(IR_ICON, con, ty);
		} else if (IR_ISUNSIGNED(ty) || IR_ISPTR(ty)) {
			if (IR_ISPTR(ty))
				ty = &cir_uintptr_t;
			con.ic_ucon = 0;
			r = ir_con(IR_ICON, con, ty);
		} else if (IR_ISFLOATING(ty)) {
			con.ic_fcon = 0;
			r = ir_con(IR_FCON, con, ty);
		} else
			fatalx("jump_gencode: bad expr type: %d", ty->it_op);
		op = IR_BNE;
		break;
	}

	if (!CANSKIP(flags, x->ae_l) && l == NULL)
		l = ast_expr_gencode(fn, iq, x->ae_l, flags);
	if (!CANSKIP(flags, x->ae_r) && r == NULL)
		r = ast_expr_gencode(fn, iq, x->ae_r, flags & ~GC_FLG_DISCARD);
	if (!(flags & GC_FLG_DISCARD)) {
		ty = tybinconv(l->ie_type, r->ie_type);
		l = ir_cast(l, ty);
		r = ir_cast(r, ty);
	}
	if ((t = *tp) == NULL)
		t = ir_lbl();
	if ((f = *fp) == NULL)
		f = ir_lbl();
	if (flags & GC_FLG_DISCARD)
		return;
	ir_insnq_enq(iq, ir_bc(op, l, r, t));
	ir_insnq_enq(iq, ir_b(f));
	*tp = t;
	*fp = f;
}

static struct ir_expr *
ident_gencode(struct ir_func *fn, struct ir_insnq *iq, struct ast_expr *x,
    int flags)
{
	int op;
	struct symbol *sym = x->ae_sym;
	struct ir_symbol *irsym;
	struct ir_type *ty;

	if (CANSKIP(flags, x))
		return NULL;
	if (sym->s_irsym == NULL) {
		ty = ir_type_dequal(x->ae_type);
		irsym = ir_symbol(IR_VARSYM, sym->s_ident, ty->it_size,
		    ty->it_align, ty);
		if (IR_ISVOLAT(x->ae_type))
			ir_symbol_setflags(irsym, IR_SYM_VOLAT);
	} else {
		irsym = sym->s_irsym;
		ty = irsym->is_type;
	}

	if (IR_ISARR(x->ae_type) || IR_ISFUNTY(x->ae_type))
		flags |= GC_FLG_ADDR;
	if (sym->s_scope == 0 || sym->s_sclass == AST_SC_STATIC ||
	    sym->s_sclass == AST_SC_EXTERN)
		op = flags & GC_FLG_ADDR ? IR_GADDR : IR_GVAR;
	else if (sym->s_scope == 1)
		op = flags & GC_FLG_ADDR ? IR_PADDR : IR_PVAR;
	else
		op = flags & GC_FLG_ADDR ? IR_LADDR : IR_LVAR;

	if (flags & GC_FLG_ADDR)
		return ir_addr(op, irsym);
	return ir_var(op, irsym);
}

static struct ir_expr *
subscr_gencode(struct ir_func *fn, struct ir_insnq *iq, struct ast_expr *x,
    int flags)
{
	struct ast_expr *idx, *oldx = x;
	size_t arrsz;
	union ir_con con;
	struct ir_expr *irbase, *iridx = NULL, *irx, *oiridx = NULL;

	if (CANSKIP(flags, x))
		return NULL;

	do {
		arrsz = x->ae_type->it_size;
		idx = x->ae_r;
		x = x->ae_l;
		if (CANSKIP(flags, idx))
			continue;

		iridx = ast_expr_gencode(fn, iq, idx, flags & ~GC_FLG_ADDR);
		if (flags & GC_FLG_DISCARD)
			continue;

		iridx = ir_cast(iridx, &cir_uintptr_t);
		if (arrsz != 1) {
			if (arrsz == 0)
				fatalx("subscr_gencode");
			con.ic_ucon = arrsz;
			iridx = ir_bin(IR_MUL, iridx,
			    ir_con(IR_ICON, con, &cir_uintptr_t),
			    &cir_uintptr_t);
		}
		if (oiridx != NULL)
			iridx = ir_bin(IR_ADD, iridx, oiridx, &cir_uintptr_t);

		oiridx = iridx;
	} while (x->ae_op == AST_SUBSCR);

	if (CANSKIP(flags, x))
		return NULL;

	if (IR_ISPTR(x->ae_type))
		irbase = ast_expr_gencode(fn, iq, x, flags & ~GC_FLG_ADDR);
	else
		irbase = ast_expr_gencode(fn, iq, x, flags | GC_FLG_ADDR);
	if (flags & GC_FLG_DISCARD)
		return NULL;

	irx = ir_bin(IR_ADD, irbase, iridx, &ir_ptr);
	if (flags & GC_FLG_ADDR)
		return irx;
	irx = ir_load(irx, ir_type_dequal(oldx->ae_type));
	return irx;
}

static struct ir_symbol *
indcall_gencode(struct ir_func *fn, struct ir_insnq *iq, struct ast_expr *x)
{
	struct ir_expr *reg, *tmp;
	struct ir_symbol *sym;

	tmp = ast_expr_gencode(fn, iq, x, 0);
	reg = ir_newvreg(fn, tmp->ie_type);
	sym = reg->ie_sym;
	ir_insnq_enq(iq, ir_asg(reg, tmp));
	return sym;
}

static struct ir_expr *
builtin_gencode(struct ir_func *fn, struct ir_insnq *iq, struct ast_expr *x,
    int flags)
{
	struct symbol *sym;

	sym = x->ae_l->ae_sym;
	if (sym == builtin_va_start_sym)
		return builtin_va_start_gencode(fn, iq, x, flags);
	if (sym == builtin_va_arg_sym)
		return builtin_va_arg_gencode(fn, iq, x, flags);
	if (sym == builtin_va_end_sym)
		return builtin_va_end_gencode(fn, iq, x, flags);
	fatalx("builtin_gencode");
}

static struct ir_expr *
call_gencode(struct ir_func *fn, struct ir_insnq *iq, struct ast_expr *x,
    int flags)
{
	int argno, ellipsis;
	struct ast_expr *arg;
	struct ir_expr *reg, *tmp;
	struct ir_exprq argq;
	struct ir_insn *call;
	struct ir_symbol *sym;
	struct ir_type *fnty, *ty;
	struct ir_typelm *elm;
	struct symbol *csym;

	static char *setjmpstr;

	csym = NULL;
	if (x->ae_l->ae_op == AST_IDENT) {
		csym = x->ae_l->ae_sym;
		if (csym->s_flags & SYM_BUILTIN)
			return builtin_gencode(fn, iq, x, flags);
		if (setjmpstr == NULL)
			setjmpstr = ntenter(&names, "setjmp");
		if (csym->s_ident == setjmpstr)
			ir_func_setflags(fn, IR_FUNC_SETJMP);
	}
	fnty = x->ae_l->ae_type;
	if (IR_ISPTR(fnty))
		fnty = fnty->it_base;
	if (!IR_ISFUNTY(fnty))
		fatalx("call_gencode: not a function type");

	ir_exprq_init(&argq);
	elm = SIMPLEQ_FIRST(&fnty->it_typeq);
	argno = ellipsis = 0;
	TAILQ_FOREACH(arg, &x->ae_args->al_exprs, ae_link) {
		tmp = ast_expr_gencode(fn, iq, arg, 0);
		if ((fnty->it_flags & IR_KRFUNC) || ellipsis) {
			ty = ir_type_dequal(arg->ae_type);
			tmp = ir_cast(tmp, tyargconv(ty));
		}
		if (IR_ISARR(tmp->ie_type) || IR_ISFUNTY(tmp->ie_type))
			reg = ir_newvreg(fn, &ir_ptr);
		else
			reg = ir_newvreg(fn, tmp->ie_type);
		ir_insnq_enq(iq, ir_asg(reg, tmp));
		reg = ir_virtreg(reg->ie_sym);
		ir_exprq_enq(&argq, reg);
		if (elm != NULL) {
			elm = SIMPLEQ_NEXT(elm, it_link);
			argno++;
		}
		if ((fnty->it_flags & IR_ELLIPSIS) && !ellipsis)
			ellipsis = 1;
	}
	if (IR_ISFUNTY(x->ae_l->ae_type)) {
		if (x->ae_l->ae_op != AST_IDENT)
			sym = indcall_gencode(fn, iq, x->ae_l);
		else {
			sym = x->ae_l->ae_sym->s_irsym;
			if (sym->is_op != IR_FUNSYM)
				sym = indcall_gencode(fn, iq, x->ae_l);
		}
	} else
		sym = indcall_gencode(fn, iq, x->ae_l);

	ty = ir_type_dequal(x->ae_type);
	reg = NULL;
	if (!IR_ISVOID(ty) && !(flags & GC_FLG_DISCARD))
		reg = ir_newvreg(fn, ty);
	call = ir_call(reg, sym, &argq);
	ir_insnq_enq(iq, call);
	if (!IR_ISVOID(ty) && !(flags & GC_FLG_DISCARD))
		reg = ir_virtreg(reg->ie_sym);
	if (ellipsis)
		((struct ir_call *)call)->ic_firstvararg = argno;
	if (csym != NULL && (csym->s_fnspec & AST_FS_DEAD))
		ir_insnq_enq(iq, ir_ret(NULL, 1));
	return reg;
}

static struct ir_expr *
souref_gencode(struct ir_func *fn, struct ir_insnq *iq, struct ast_expr *x,
    int flags)
{
	struct ir_expr *irx;
	struct ir_typelm *elm;
	struct ir_type *ty;

	if (CANSKIP(flags, x))
		return NULL;

	if (x->ae_op == AST_SOUDIR)
		irx = ast_expr_gencode(fn, iq, x->ae_l, flags | GC_FLG_ADDR);
	else
		irx = ast_expr_gencode(fn, iq, x->ae_l, flags & ~GC_FLG_ADDR);

	if (flags & GC_FLG_DISCARD)
		return NULL;

	ty = ir_type_dequal(x->ae_l->ae_type);
	if (x->ae_op == AST_SOUIND)
		ty = ir_type_dequal(ty->it_base);

	if (!IR_ISSTRUCT(ty) && !IR_ISUNION(ty))
		fatalx("souref_gencode: not a struct or union: %d", ty->it_op);
	elm = x->ae_elm;
	irx = ir_souref(irx, ty, elm);
	if ((flags & GC_FLG_ADDR) || IR_ISARR(elm->it_type))
		return irx;
	irx = ir_load(irx, elm->it_type);
	return ir_cast(irx, ir_type_dequal(x->ae_type));
}

static struct ir_expr *
incdec_gencode(struct ir_func *fn, struct ir_insnq *iq, struct ast_expr *x,
    int flags)
{
	int op, post;
	union ir_con con;
	struct ir_expr *dst, *inc, *newval, *oldval, *tmp, *val;
	struct ir_type *ty;

	if (x->ae_l->ae_op == AST_IDENT) {
		val = ast_expr_gencode(fn, iq, x->ae_l,
		    flags & ~(GC_FLG_DISCARD | GC_FLG_ADDR));
		dst = ast_expr_gencode(fn, iq, x->ae_l,
		    flags & ~(GC_FLG_DISCARD | GC_FLG_ADDR));
		ty = val->ie_type;
	} else {
		tmp = ast_expr_gencode(fn, iq, x->ae_l,
		    (flags | GC_FLG_ADDR) & ~(GC_FLG_DISCARD));
		dst = ir_newvreg(fn, tmp->ie_type);
		ir_insnq_enq(iq, ir_asg(dst, tmp));
		dst = ir_virtreg(dst->ie_sym);
		ty = ir_type_dequal(x->ae_l->ae_type);
		val = ir_load(dst, ty);
		dst = ir_virtreg(dst->ie_sym);
	}

	post = x->ae_op == AST_POSTINC || x->ae_op == AST_POSTDEC;
	if (x->ae_op == AST_POSTINC || x->ae_op == AST_PREINC)
		op = IR_ADD;
	else
		op = IR_SUB;

	if (IR_ISPTR(ty)) {
		con.ic_ucon = ty->it_base->it_size;
		inc = ir_con(IR_ICON, con, &cir_uintptr_t);
	} else if (IR_ISSIGNED(ty)) {
		con.ic_icon = 1;
		inc = ir_con(IR_ICON, con, ty);
	} else if (IR_ISUNSIGNED(ty)) {
		con.ic_ucon = 1;
		inc = ir_con(IR_ICON, con, ty);
	} else {
		con.ic_fcon = 1.0;
		inc = ir_con(IR_FCON, con, ty);
	}

	newval = ir_newvreg(fn, val->ie_type);
	oldval = NULL;
	if (post && !(flags & GC_FLG_DISCARD)) {
		oldval = ir_newvreg(fn, val->ie_type);
		ir_insnq_enq(iq, ir_asg(oldval, val));
		val = ir_virtreg(oldval->ie_sym);
		oldval = ir_virtreg(oldval->ie_sym);
	}
	ir_insnq_enq(iq, ir_asg(newval, ir_bin(op, val, inc, ty)));
	newval = ir_virtreg(newval->ie_sym);

	if (x->ae_l->ae_op == AST_IDENT)
		ir_insnq_enq(iq, ir_asg(dst, newval));
	else
		ir_insnq_enq(iq, ir_store(dst, newval));

	if (flags & GC_FLG_DISCARD)
		return NULL;
	if (post)
		return oldval;
	return ir_virtreg(newval->ie_sym);
}

static struct ir_expr *
deref_gencode(struct ir_func *fn, struct ir_insnq *iq, struct ast_expr *x,
    int flags)
{
	struct ir_expr *irx;
	struct ir_type *ty;

	if (CANSKIP(flags, x))
		return NULL;

	irx = ast_expr_gencode(fn, iq, x->ae_l, flags & ~GC_FLG_ADDR);
	if (flags & GC_FLG_DISCARD)
		return NULL;
	ty = ir_type_dequal(x->ae_type);
	if (IR_ISFUNTY(ty) || (IR_ISPTR(ty) && IR_ISFUNTY(ty->it_base)))
		return irx;
	if (flags & GC_FLG_ADDR)
		return irx;
	return ir_load(irx, ty);
}

static struct ir_expr *
uplusminus_gencode(struct ir_func *fn, struct ir_insnq *iq, struct ast_expr *x,
    int flags)
{
	struct ir_expr *irx;
	struct ir_type *promty;

	if (CANSKIP(flags, x))
		return NULL;

	irx = ast_expr_gencode(fn, iq, x, flags);
	if (flags & GC_FLG_DISCARD)
		return NULL;

	promty = ir_type_dequal(x->ae_type);
	irx = ir_cast(irx, promty);
	if (x->ae_op == AST_UMINUS)
		return ir_unary(IR_UMINUS, irx, promty);
	return irx;
}

static struct ir_expr *
bitflip_gencode(struct ir_func *fn, struct ir_insnq *iq, struct ast_expr *x,
    int flags)
{
	struct ir_expr *irx;

	if (CANSKIP(flags, x))
		return NULL;

	irx = ast_expr_gencode(fn, iq, x->ae_l, flags);
	irx = ir_cast(irx, tyuconv(irx->ie_type));
	return ir_unary(IR_BITFLIP, irx, irx->ie_type);
}

static struct ir_expr *
cast_gencode(struct ir_func *fn, struct ir_insnq *iq, struct ast_expr *x,
    int flags)
{
	struct ir_expr *irx;
	struct ir_type *toty;

	if (CANSKIP(flags, x))
		return NULL;

	irx = ast_expr_gencode(fn, iq, x->ae_l, flags);
	if (flags & GC_FLG_DISCARD)
		return NULL;

	toty = ir_type_dequal(x->ae_type);
	return ir_cast(irx, toty);
}

static int
bin_gencode(struct ir_func *fn, struct ir_insnq *iq, struct ast_expr *x,
    int flags, struct ir_expr **lp, struct ir_expr **rp, struct ir_expr **dstp)
{
	struct ir_expr *dst, *l, *r, *tmp;

	if (CANSKIP(flags, x) && !(flags & GC_FLG_ASG))
		return 1;

	dst = l = r = NULL;
	if (!CANSKIP(flags, x->ae_l) || !(flags & GC_FLG_ASG))
		l = ast_expr_gencode(fn, iq, x->ae_l, flags & ~GC_FLG_ASG);
	if (!CANSKIP(flags, x->ae_r) || !(flags & GC_FLG_ASG))
		r = ast_expr_gencode(fn, iq, x->ae_r, flags & ~GC_FLG_ASG);
	if ((flags & GC_FLG_DISCARD) && !(flags & GC_FLG_ASG))
		return 1;

	if (flags & GC_FLG_ASG) {
		if (x->ae_l->ae_op == AST_IDENT) {
			dst = ast_expr_gencode(fn, iq, x->ae_l,
			    flags & ~(GC_FLG_DISCARD | GC_FLG_ASG));
			l = ast_expr_gencode(fn, iq, x->ae_l,
			    flags & ~(GC_FLG_DISCARD | GC_FLG_ASG));
		} else {
			dst = ast_expr_gencode(fn, iq, x->ae_l,
			    (flags | GC_FLG_ADDR) &
			    ~(GC_FLG_DISCARD | GC_FLG_ASG));
			tmp = ir_newvreg(fn, dst->ie_type);
			ir_insnq_enq(iq, ir_asg(tmp, dst));
			dst = ir_virtreg(tmp->ie_sym);
			l = ir_load(dst, ir_type_dequal(x->ae_l->ae_type));
			dst = ir_virtreg(tmp->ie_sym);
		}
	}

	*dstp = dst;
	*lp = l;
	*rp = r;
	return 0;
}

static struct ir_expr *
mul_gencode(struct ir_func *fn, struct ir_insnq *iq, struct ast_expr *x,
    struct ir_expr **dstp, int flags)
{
	int op;
	struct ir_expr *dst, *l, *r;
	struct ir_type *ty;

	if (bin_gencode(fn, iq, x, flags, &l, &r, &dst))
		return NULL;

	if (flags & GC_FLG_ASG)
		ty = tybinconv(l->ie_type, r->ie_type);
	else
		ty = ir_type_dequal(x->ae_type);

	l = ir_cast(l, ty);
	r = ir_cast(r, ty);
	if (x->ae_op == AST_MUL || x->ae_op == AST_MULASG)
		op = IR_MUL;
	else if (x->ae_op == AST_DIV || x->ae_op == AST_DIVASG)
		op = IR_DIV;
	else
		op = IR_MOD;
	r = ir_bin(op, l, r, ty);
	if (flags & GC_FLG_ASG)
		*dstp = dst;
	return r;
}

static struct ir_expr *
add_gencode(struct ir_func *fn, struct ir_insnq *iq, struct ast_expr *x,
    struct ir_expr **dstp, int flags)
{
	int op;
	union ir_con con;
	struct ir_expr *dst, *intx, *l = NULL, *mul, *r = NULL;
	struct ir_type *ty, *pty;

	if (bin_gencode(fn, iq, x, flags & ~GC_FLG_ADDR, &l, &r, &dst))
		return NULL;

	if (x->ae_op == AST_ADD || x->ae_op == AST_ADDASG)
		op = IR_ADD;
	else
		op = IR_SUB;

	if (flags & GC_FLG_ASG)
		ty = tybinconv(l->ie_type, r->ie_type);
	else
		ty = ir_type_dequal(x->ae_type);

	if (IR_ISPTR(ty)) {
		intx = NULL;
		if (IR_ISINTEGER(l->ie_type)) {
			intx = l;
			pty = ir_type_dequal(x->ae_r->ae_type);
		} else if (IR_ISINTEGER(r->ie_type)) {
			intx = r;
			pty = ir_type_dequal(x->ae_l->ae_type);
		}
		if (intx != NULL) {
			con.ic_ucon = pty->it_base->it_size;
			mul = ir_con(IR_ICON, con, &cir_uintptr_t);
			intx = ir_cast(intx, &cir_uintptr_t);
			intx = ir_bin(IR_MUL, intx, mul, &cir_uintptr_t);
			if (IR_ISINTEGER(l->ie_type))
				l = intx;
			else if (IR_ISINTEGER(r->ie_type))
				r = intx;
		}
	} else {
		l = ir_cast(l, ty);
		r = ir_cast(r, ty);
	}

	r = ir_bin(op, l, r, ty);
	if (flags & GC_FLG_ASG)
		*dstp = dst;
	return r;
}

static struct ir_expr *
shift_gencode(struct ir_func *fn, struct ir_insnq *iq, struct ast_expr *x,
    struct ir_expr **dstp, int flags)
{
	int op;
	struct ir_expr *dst = NULL, *l = NULL, *r = NULL;

	if (bin_gencode(fn, iq, x, flags, &l, &r, &dst))
		return NULL;

	l = ir_cast(l, tyuconv(l->ie_type));
	r = ir_cast(r, tyuconv(r->ie_type));
	if (x->ae_op == AST_LS || x->ae_op == AST_LSASG)
		op = IR_LS;
	else
		op = IR_LRS;
	if (op == IR_LRS && IR_ISSIGNED(l->ie_type))
		op = IR_ARS;
	r = ir_bin(op, l, r, l->ie_type);
	if (flags & GC_FLG_ASG)
		*dstp = dst;
	return r;
}

static struct ir_expr *
bw_gencode(struct ir_func *fn, struct ir_insnq *iq, struct ast_expr *x,
    struct ir_expr **dstp, int flags)
{
	int op;
	struct ir_expr *dst = NULL, *l = NULL, *r = NULL;
	struct ir_type *ty;

	if (bin_gencode(fn, iq, x, flags, &l, &r, &dst))
		return NULL;

	if (flags & GC_FLG_ASG)
		ty = tybinconv(l->ie_type, r->ie_type);
	else
		ty = ir_type_dequal(x->ae_type);
	l = ir_cast(l, ty);
	r = ir_cast(r, ty);
	if (x->ae_op == AST_BWOR || x->ae_op == AST_ORASG)
		op = IR_OR;
	else if (x->ae_op == AST_BWXOR || x->ae_op == AST_XORASG)
		op = IR_XOR;
	else
		op = IR_AND;
	r = ir_bin(op, l, r, ty);
	if (flags & GC_FLG_ASG)
		*dstp = dst;
	return r;
}

static struct ir_expr *
bool_gencode(struct ir_func *fn, struct ir_insnq *iq, struct ast_expr *x,
    int flags)
{
	union ir_con ocon, zcon;
	struct ir_expr *one, *reg, *zero;
	struct ir_insn *f, *t;
	struct ir_insn *out;

	if (CANSKIP(flags, x))
		return NULL;

	f = t = NULL;
	one = zero = NULL;
	jump_gencode(fn, iq, &t, &f, x, flags);
	reg = NULL;
	if (!(flags & GC_FLG_DISCARD))
		reg = ir_newvreg(fn, &cir_int);
	ocon.ic_icon = 1;
	zcon.ic_icon = 0;
	out = ir_lbl();
	if (!(flags & GC_FLG_DISCARD)) {
		one = ir_con(IR_ICON, ocon, &cir_int);
		zero = ir_con(IR_ICON, zcon, &cir_int);
	}
	if (t != NULL)
		ir_insnq_enq(iq, t);
	if (!(flags & GC_FLG_DISCARD)) {
		reg = ir_virtreg(reg->ie_sym);
		ir_insnq_enq(iq, ir_asg(reg, one));
	}
	ir_insnq_enq(iq, ir_b(out));
	if (f != NULL)
		ir_insnq_enq(iq, f);
	if (!(flags & GC_FLG_DISCARD)) {
		reg = ir_virtreg(reg->ie_sym);
		ir_insnq_enq(iq, ir_asg(reg, zero));
	}
	ir_insnq_enq(iq, out);
	if (flags & GC_FLG_DISCARD)
		return NULL;
	return ir_virtreg(reg->ie_sym);
}

static struct ir_expr *
cond_gencode(struct ir_func *fn, struct ir_insnq *iq, struct ast_expr *x,
    int flags)
{
	struct ir_expr *m, *r, *reg;
	struct ir_insn *f, *out, *t;
	struct ir_type *ty;

	if (CANSKIP(flags, x))
		return NULL;
	if (CANSKIP(flags, x->ae_m) && CANSKIP(flags, x->ae_r)) {
		ast_expr_gencode(fn, iq, x->ae_l, flags);
		return NULL;
	}

	ty = ir_type_dequal(x->ae_type);
	reg = NULL;

	out = ir_lbl();
	m = r = NULL;
	f = t = NULL;
	jump_gencode(fn, iq, &t, &f, x->ae_l, flags & ~GC_FLG_DISCARD);
	ir_insnq_enq(iq, t);
	if (!CANSKIP(flags, x->ae_m))
		m = ast_expr_gencode(fn, iq, x->ae_m, flags);
	if (!(flags & GC_FLG_DISCARD)) {
		m = ir_cast(m, ty);
		reg = ir_newvreg(fn, ty);
		ir_insnq_enq(iq, ir_asg(reg, m));
	}
	ir_insnq_enq(iq, ir_b(out));
	ir_insnq_enq(iq, f);
	if (!CANSKIP(flags, x->ae_r))
		r = ast_expr_gencode(fn, iq, x->ae_r, flags);
	if (!(flags & GC_FLG_DISCARD)) {
		r = ir_cast(r, ty);
		if (reg == NULL)
			reg = ir_newvreg(fn, ty);
		else
			reg = ir_virtreg(reg->ie_sym);
		ir_insnq_enq(iq, ir_asg(reg, r));
	}
	ir_insnq_enq(iq, out);

	if (flags & GC_FLG_DISCARD)
		return NULL;

	reg = ir_virtreg(reg->ie_sym);
	return reg;
}

static struct ir_expr *
souasg_gencode(struct ir_func *fn, struct ir_insnq *iq, struct ast_expr *x,
    int flags)
{
	union ir_con con;
	struct ir_expr *icon, *l, *r, *reg, *reg2, *len;
	struct ir_exprq argq;
	struct ir_symbol *osouret;

	l = ast_expr_gencode(fn, iq, x->ae_l,
	    (flags | GC_FLG_ADDR) & ~GC_FLG_DISCARD);
	if (x->ae_r->ae_op == AST_CALL) {
		osouret = souretvar;
		reg = ir_newvreg(fn, &ir_ptr);
		ir_insnq_enq(iq, ir_asg(reg, l));
		souretvar = reg->ie_sym;
		ast_expr_gencode(fn, iq, x->ae_r, flags & ~GC_FLG_DISCARD);
		if (flags & GC_FLG_DISCARD) {
			souretvar = osouret;
			return NULL;
		}
		return ir_virtreg(reg->ie_sym);
	}
	r = ast_expr_gencode(fn, iq, x->ae_r,
	    (flags | GC_FLG_ADDR) & ~GC_FLG_DISCARD);
	reg = ir_newvreg(fn, &ir_ptr);
	ir_insnq_enq(iq, ir_asg(reg, l));
	reg2 = ir_newvreg(fn, &ir_ptr);
	ir_insnq_enq(iq, ir_asg(reg2, r));
	con.ic_ucon = x->ae_type->it_size;
	icon = ir_con(IR_ICON, con, &cir_size_t);
	len = ir_newvreg(fn, &cir_size_t);
	ir_insnq_enq(iq, ir_asg(len, icon));
	len = ir_virtreg(len->ie_sym);
	ir_exprq_init(&argq);
	ir_exprq_enq(&argq, reg);
	ir_exprq_enq(&argq, reg2);
	ir_exprq_enq(&argq, len);
	if (memcpysym == NULL)
		memcpysym = ir_symbol(IR_FUNSYM, ntenter(&names, "memcpy"),
		    0, 0, &ir_void);
	ir_insnq_enq(iq, ir_call(NULL, memcpysym, &argq));
	if (flags & GC_FLG_DISCARD)
		return NULL;
	return ir_virtreg(reg->ie_sym);
}

static struct ir_expr *
asg_gencode(struct ir_func *fn, struct ir_insnq *iq, struct ast_expr *x,
    int flags)
{
	int op;
	struct ir_expr *l, *r, *reg;
	struct ir_type *ty;

	op = x->ae_l->ae_op == AST_IDENT ? IR_ASG : IR_ST;
	ty = ir_type_dequal(x->ae_type);

	switch (x->ae_op) {
	case AST_ASG:
		if (IR_ISSOU(x->ae_type))
			return souasg_gencode(fn, iq, x, flags);
		if (op == IR_ASG)
			l = ast_expr_gencode(fn, iq, x->ae_l,
			    flags & ~GC_FLG_DISCARD);
		else
			l = ast_expr_gencode(fn, iq, x->ae_l,
			    (flags | GC_FLG_ADDR) & ~GC_FLG_DISCARD);
		r = ast_expr_gencode(fn, iq, x->ae_r, flags & ~GC_FLG_DISCARD);
		break;
	case AST_MULASG:
	case AST_DIVASG:
	case AST_MODASG:
		r = mul_gencode(fn, iq, x, &l,
		    (flags | GC_FLG_ASG) & ~GC_FLG_DISCARD);
		break;
	case AST_ADDASG:
	case AST_SUBASG:
		r = add_gencode(fn, iq, x, &l,
		    (flags | GC_FLG_ASG) & ~(GC_FLG_DISCARD));
		break;
	case AST_LSASG:
	case AST_RSASG:
		r = shift_gencode(fn, iq, x, &l,
		    (flags | GC_FLG_ASG) & ~GC_FLG_DISCARD);
		break;
	case AST_ANDASG:
	case AST_XORASG:
	case AST_ORASG:
		r = bw_gencode(fn, iq, x, &l,
		    (flags | GC_FLG_ASG) & ~GC_FLG_DISCARD);
		break;
	default:
		fatalx("asg_gencode: bad op: %d", x->ae_op);
	}

	r = ir_cast(r, ty);
	reg = ir_newvreg(fn, ty);
	ir_insnq_enq(iq, ir_asg(reg, r));
	reg = ir_virtreg(reg->ie_sym);
	if (op == IR_ASG)
		ir_insnq_enq(iq, ir_asg(l, reg));
	else
		ir_insnq_enq(iq, ir_store(l, reg));
	if (flags & GC_FLG_DISCARD)
		return NULL;
	return ir_virtreg(reg->ie_sym);
}

static struct ir_expr *
comma_gencode(struct ir_func *fn, struct ir_insnq *iq, struct ast_expr *x,
    int flags)
{
	struct ir_expr *r;

	if (CANSKIP(flags, x))
		return NULL;
	if (!CANSKIP(flags, x->ae_l))
		ast_expr_gencode(fn, iq, x->ae_l, flags);
	if (CANSKIP(flags, x->ae_r))
		return NULL;
	r = ast_expr_gencode(fn, iq, x->ae_r, flags);
	if (flags & GC_FLG_DISCARD)
		return NULL;
	return r;
}

struct ir_expr *
ast_expr_gencode(struct ir_func *fn, struct ir_insnq *iq, struct ast_expr *x,
    int flags)
{
	struct ir_expr *irx;
	struct ir_symbol *sym;

	switch (x->ae_op) {
	case AST_IDENT:
		return ident_gencode(fn, iq, x, flags);
	case AST_ICON:
		return ir_con(IR_ICON, x->ae_con, x->ae_type);
	case AST_FCON:
		return ir_con(IR_FCON, x->ae_con, x->ae_type);
	case AST_STRLIT:
		sym = ir_cstrsym(x->ae_str, x->ae_strlen, x->ae_type);
		irx = ir_addr(IR_GADDR, sym);
		return irx;
	case AST_SUBSCR:
		return subscr_gencode(fn, iq, x, flags);
	case AST_CALL:
		return call_gencode(fn, iq, x, flags);
	case AST_SOUDIR:
	case AST_SOUIND:
		return souref_gencode(fn, iq, x, flags);
	case AST_POSTINC:
	case AST_POSTDEC:
	case AST_PREINC:
	case AST_PREDEC:
		return incdec_gencode(fn, iq, x, flags);
	case AST_ADDROF:
		return ast_expr_gencode(fn, iq, x->ae_l, flags | GC_FLG_ADDR);
	case AST_DEREF:
		return deref_gencode(fn, iq, x, flags);
	case AST_UPLUS:
	case AST_UMINUS:
		return uplusminus_gencode(fn, iq, x, flags);
	case AST_BITFLIP:
		return bitflip_gencode(fn, iq, x, flags);
	case AST_NOT:
		return bool_gencode(fn, iq, x, flags);
	case AST_CAST:
		return cast_gencode(fn, iq, x, flags);
	case AST_MUL:
	case AST_DIV:
	case AST_MOD:
		return mul_gencode(fn, iq, x, NULL, flags);
	case AST_ADD:
	case AST_SUB:
		return add_gencode(fn, iq, x, NULL, flags);
	case AST_LS:
	case AST_RS:
		return shift_gencode(fn, iq, x, NULL, flags);
	case AST_BWAND:
	case AST_BWOR:
		return bw_gencode(fn, iq, x, NULL, flags);
	case AST_LT:
	case AST_GT:
	case AST_LE:
	case AST_GE:
	case AST_EQ:
	case AST_NE:
	case AST_ANDAND:
	case AST_OROR:
		return bool_gencode(fn, iq, x, flags);
	case AST_COND:
		return cond_gencode(fn, iq, x, flags);
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
		return asg_gencode(fn, iq, x, flags);
	case AST_COMMA:
		return comma_gencode(fn, iq, x, flags);
	default:
		fatalx("ast_expr_gencode: bad op: %d", x->ae_op);
	}
	return NULL;
}
