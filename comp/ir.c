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
#include <string.h>

#include "comp/comp.h"
#include "comp/ir.h"

struct ir_prog *irprog;
struct ir_func *irfunc;
struct irstat irstats;

int8_t ir_nkids[] = {
	0,
	2, /* IR_ASG */
	2, /* IR_ST */
	0, /* IR_B */
	2, /* IR_BEQ */
	2, /* IR_BNE */
	2, /* IR_BLT */
	2, /* IR_BLE */
	2, /* IR_BGT */
	2, /* IR_BGE */
	1, /* IR_CALL */
	1, /* IR_RET */
	0, /* IR_LBL */
	0, /* IR_PHI */
	0, /* IR_ICON */
	0, /* IR_FCON */
	0, /* IR_REG */
	0, /* IR_GVAR */
	0, /* IR_PVAR */
	0, /* IR_LVAR */
	0, /* IR_GADDR */
	0, /* IR_PADDR */
	0, /* IR_LADDR */
	1, /* IR_LOAD */
	1, /* IR_CAST */
	1, /* IR_UMINUS */
	1, /* IR_BITFLIP */
	1, /* IR_SOUREF */
	2, /* IR_MUL */
	2, /* IR_DIV */
	2, /* IR_MOD */
	2, /* IR_ADD */
	2, /* IR_SUB */
	2, /* IR_ARS */
	2, /* IR_LRS */
	2, /* IR_LS */
	2, /* IR_AND */
	2, /* IR_XOR */
	2, /* IR_OR */
};

struct ir_type ir_i8 = {
	IR_I8, IR_COMPLETE, 1, 1, NULL,
	{ SIMPLEQ_HEAD_INITIALIZER(ir_i8.it_typeq) }
};

struct ir_type ir_u8 = {
	IR_U8, IR_COMPLETE, 1, 1, NULL,
	{ SIMPLEQ_HEAD_INITIALIZER(ir_u8.it_typeq) }
};

struct ir_type ir_i16 = {
	IR_I16, IR_COMPLETE, 2, 2, NULL,
	{ SIMPLEQ_HEAD_INITIALIZER(ir_i16.it_typeq) }
};

struct ir_type ir_u16 = {
	IR_U16, IR_COMPLETE, 2, 2, NULL,
	{ SIMPLEQ_HEAD_INITIALIZER(ir_u16.it_typeq) }
};

struct ir_type ir_i32 = {
	IR_I32, IR_COMPLETE, 4, 4, NULL,
	{ SIMPLEQ_HEAD_INITIALIZER(ir_i32.it_typeq) }
};

struct ir_type ir_u32 = {
	IR_U32, IR_COMPLETE, 4, 4, NULL,
	{ SIMPLEQ_HEAD_INITIALIZER(ir_u32.it_typeq) }
};

struct ir_type ir_i64 = {
	IR_I64, IR_COMPLETE, 8, 8, NULL,
	{ SIMPLEQ_HEAD_INITIALIZER(ir_i64.it_typeq) }
};

struct ir_type ir_u64 = {
	IR_U64, IR_COMPLETE, 8, 8, NULL,
	{ SIMPLEQ_HEAD_INITIALIZER(ir_u64.it_typeq) }
};

struct ir_type ir_f32 = {
	IR_F32, IR_COMPLETE, 4, 4, NULL,
	{ SIMPLEQ_HEAD_INITIALIZER(ir_f32.it_typeq) }
};

struct ir_type ir_f64 = {
	IR_F64, IR_COMPLETE, 8, 8, NULL,
	{ SIMPLEQ_HEAD_INITIALIZER(ir_f64.it_typeq) }
};

struct ir_type ir_void = {
	IR_VOID, IR_COMPLETE, 0, 0, NULL,
	{ SIMPLEQ_HEAD_INITIALIZER(ir_void.it_typeq) }
};

struct ir_type ir_bool = {
	IR_BOOL, IR_COMPLETE, 1, 1, NULL,
	{ SIMPLEQ_HEAD_INITIALIZER(ir_bool.it_typeq) }
};

/* XXX: These have to die. */
struct ir_type ir_ptr = {
	IR_PTR, IR_COMPLETE, IR_PTR_SIZE, IR_PTR_ALIGN, NULL,
	{ SIMPLEQ_HEAD_INITIALIZER(ir_ptr.it_typeq) }
};

struct ir_type ir_obj = {
	IR_OBJ, IR_COMPLETE, 0, 0, NULL,
	{ SIMPLEQ_HEAD_INITIALIZER(ir_obj.it_typeq) }
};

static struct ir_symbol *freesyms;

static void *
fromfuncalloc(size_t size)
{
	if (irfunc == NULL)
		return xmalloc(size);
	return mem_alloc(&irfunc->if_mem, size);
}

static void *
iralloc(int op, size_t size)
{
	struct ir *ir;

	ir = fromfuncalloc(size);
	ir->i_auxdata = NULL;
	ir->i_op = op;
	ir->i_flags = 0;
	ir->i_emit = NULL;
	ir->i_tmpregs = NULL;
	ir->i_tmpregsyms = NULL;
	return ir;
}

static void *
insnalloc(int op, size_t size)
{
	struct ir_insn *insn;

	insn = iralloc(op, size);
	insn->ii_cgskip = 0;
	insn->ii_bb = NULL;
	dfa_initdata(&insn->ii_dfadata);
	irstats.i_insns++;
	return insn;
}

static void *
expralloc(int op, struct ir_type *type)
{
	struct ir_expr *x;

	x = iralloc(op, sizeof *x);
	x->ie_l = x->ie_r = NULL;
	x->ie_type = type;
	x->ie_sym = NULL;
	x->ie_sou = NULL;
	x->ie_elm = NULL;
	x->ie_regs = NULL;
	irstats.i_exprs++;
	return x;
}

static struct ir_symbol *
symalloc(int op)
{
	struct ir_symbol *sym;

	if (freesyms != NULL) {
		sym = freesyms;
		freesyms = freesyms->is_top;
	} else
		sym = xmalloc(sizeof *sym);
	sym->is_id = 0;
	sym->is_name = NULL;
	sym->is_size = sym->is_align = 0;
	sym->is_type = NULL;
	sym->is_off = 0;
	sym->is_flags = 0;
	sym->is_op = op;
	irstats.i_syms++;
	return sym;
}

static struct ir_expr *
refexpr(struct ir_expr *x)
{
	if (x == NULL)
		return NULL;
	if (x->i_flags & IR_EXPR_INUSE)
		x = ir_expr_copy(x);
	x->i_flags |= IR_EXPR_INUSE;
	return x;
}

static struct ir_symbol *
f64sym(double val)
{
	struct ir_symbol *sym;
	struct ir_syminit *symini;

	SIMPLEQ_FOREACH(symini, &irprog->ip_floatq, is_link) {
		sym = symini->is_sym;
		if (!IR_ISF64(sym->is_type))
			fatalx("f64sym: bad type op: %d", sym->is_type->it_op);
		if (symini->is_val.ic_fcon == val)
			return sym;
	}
	sym = ir_symbol(IR_VARSYM, ".L", sizeof(double), sizeof(double),
	    &ir_f64);
	symini = xmalloc(sizeof *symini);
	symini->is_op = IR_FLATINIT;
	symini->is_sym = sym;
	symini->is_val.ic_fcon = val;
	SIMPLEQ_INSERT_TAIL(&irprog->ip_floatq, symini, is_link);
	return sym;
}

struct ir_init *
ir_init(struct ir_symbol *sym)
{
	struct ir_init *init;

	init = xmalloc(sizeof *init);
	SIMPLEQ_INIT(&init->ii_elms);
	init->ii_sym = sym;
	return init;
}

static struct ir_initelm *
initelmalloc(struct ir_init *init, int op, size_t off, size_t size)
{
	struct ir_initelm *elm;

	elm = xmalloc(sizeof *elm);
	elm->ii_op = op;
	elm->ii_cstr = NULL;
	elm->ii_len = 0;
	elm->ii_bitoff = off;
	elm->ii_bitsize = size;
	SIMPLEQ_INSERT_TAIL(&init->ii_elms, elm, ii_link);
	return elm;
}

void
ir_initelm_str(int op, struct ir_init *init, char *str, size_t len, size_t off)
{
	struct ir_initelm *elm;

	elm = initelmalloc(init, op, off, len * 8);
	elm->ii_cstr = str;
	elm->ii_len = len;
}

void
ir_initelm_expr(struct ir_init *init, struct ir_expr *x, size_t off,
    size_t size)
{
	struct ir_initelm *elm;

	elm = initelmalloc(init, IR_INIT_EXPR, off, size);
	elm->ii_expr = refexpr(x);
}

struct ir_prog *
ir_prog(void)
{
	struct ir_prog *prog;

	prog = xmalloc(sizeof *prog);
	SIMPLEQ_INIT(&prog->ip_initq);
	SIMPLEQ_INIT(&prog->ip_roinitq);
	SIMPLEQ_INIT(&prog->ip_floatq);
	ir_symq_init(&prog->ip_cstrq);
	ir_symq_init(&prog->ip_symq);
	SIMPLEQ_INIT(&prog->ip_funq);
	return prog;
}

void
ir_prog_addsym(struct ir_prog *prog, struct ir_symbol *sym)
{
	ir_symq_enq(&prog->ip_symq, sym);
}

void
ir_prog_addfunc(struct ir_prog *prog, struct ir_func *fn)
{
	SIMPLEQ_INSERT_TAIL(&prog->ip_funq, fn, if_link);
}

void
ir_prog_addinit(struct ir_prog *prog, int rodata, struct ir_init *init)
{
	if (rodata)
		SIMPLEQ_INSERT_TAIL(&prog->ip_roinitq, init, ii_link);
	else
		SIMPLEQ_INSERT_TAIL(&prog->ip_initq, init, ii_link);
}

struct ir_func *
ir_func(struct ir_symbol *sym)
{
	struct ir_func *fn;

	if (irfunc != NULL)
		fatalx("ir_func: already in function");
	fn = xmalloc(sizeof *fn);
	fn->if_sym = sym;
	ir_symq_init(&fn->if_parq);
	ir_symq_init(&fn->if_varq);
	ir_symq_init(&fn->if_regq);
	fn->if_regs = NULL;
	ir_insnq_init(&fn->if_iq);
	fn->if_framesz = fn->if_argareasz = 0;
	regset_init(&fn->if_usedregs);
	fn->if_retlab = newid();
	fn->if_cfadata = NULL;
	fn->if_regid = REG_NREGS;
	fn->if_flags = 0;
	irfunc = fn;
	mem_area_init(&fn->if_mem);
	mem_area_init(&fn->if_livevarmem);
	ir_func_machdep(fn);
	irstats.i_funcs++;
	return fn;
}

void
ir_func_finish(struct ir_func *fn)
{
	if (fn != irfunc)
		fatalx("ir_func_finish");
	irfunc = NULL;
}

void
ir_func_linearize_regs(struct ir_func *fn)
{
	int i;
	struct ir_symbol *sym;

	free(fn->if_regs);
	fn->if_regs = xcalloc(fn->if_regid, sizeof *fn->if_regs);
	SIMPLEQ_FOREACH(sym, &fn->if_regq, is_link) {
		if (sym->is_id >= fn->if_regid || sym->is_id < 0)
			fatalx("ir_func_linearize_regs");
		fn->if_regs[sym->is_id] = sym;
	}
	for (i = 0; i < REG_NREGS; i++)
		fn->if_regs[i] = physregs[i];
}

struct ir_param *
ir_parlocs_func(struct ir_func *fn)
{
	int i, npar;
	struct ir_param *parms;
	struct ir_symbol *sym;

	npar = 0;
	SIMPLEQ_FOREACH(sym, &fn->if_parq, is_link)
		npar++;
	parms = xcalloc(npar + 1, sizeof *parms);
	i = 0;
	SIMPLEQ_FOREACH(sym, &fn->if_parq, is_link) {
		parms[i].ip_type = sym->is_type;
		parms[i].ip_argsym = sym;
		i++;
	}
	ir_parlocs(parms, npar);
	return parms;
}

struct ir_param *
ir_parlocs_call(struct ir_insn *call)
{
	int i, npar;
	struct ir_expr *x;
	struct ir_param *parms;

	npar = 0;
	SIMPLEQ_FOREACH(x, &call->ic_argq, ie_link)
		npar++;
	parms = xcalloc(npar + 1, sizeof *parms);
	i = 0;
	SIMPLEQ_FOREACH(x, &call->ic_argq, ie_link) {
		parms[i].ip_argx = x;
		parms[i].ip_argsym = x->ie_sym;
		parms[i].ip_type = x->ie_type;
		i++;
	}
	ir_parlocs(parms, npar);
	return parms;
}

static void
symqfree(struct ir_symq *q)
{
	struct ir_symbol *sym;

	while (!SIMPLEQ_EMPTY(q)) {
		sym = SIMPLEQ_FIRST(q);
		SIMPLEQ_REMOVE_HEAD(q, is_link);
		free(sym);
	}
}

void
ir_func_free(struct ir_func *fn)
{
	cfa_free(fn);
	mem_area_free(&fn->if_mem);
	symqfree(&fn->if_parq);
	symqfree(&fn->if_varq);
	symqfree(&fn->if_regq);
	free(fn);
}

void
ir_func_addparm(struct ir_func *fn, struct ir_symbol *sym)
{
	ir_symq_enq(&fn->if_parq, sym);
}

void
ir_func_addvar(struct ir_func *fn, struct ir_symbol *sym)
{
	ir_symq_enq(&fn->if_varq, sym);
}

void
ir_func_addreg(struct ir_func *fn, struct ir_symbol *sym)
{
	ir_symq_enq(&fn->if_regq, sym);
}

void
ir_func_enqinsns(struct ir_func *fn, struct ir_insnq *iq)
{
	ir_insnq_cat(&fn->if_iq, iq);
}

void
ir_func_setflags(struct ir_func *fn, int flags)
{
	fn->if_flags |= flags;
}

struct ir_symbol *
ir_symbol(int op, char *name, size_t size, size_t align, struct ir_type *type)
{
	struct ir_symbol *sym;

	sym = symalloc(op);
	if (op == IR_REGSYM)
		sym->is_id = irfunc->if_regid++;
	else
		sym->is_id = newid();
	sym->is_name = name;
	sym->is_size = size;
	sym->is_align = align;
	sym->is_type = type;
	return sym;
}

struct ir_symbol *
ir_cstrsym(char *str, size_t size, struct ir_type *type)
{
	struct ir_symbol *sym;

	/* XXX: Speed up */
	SIMPLEQ_FOREACH(sym, &irprog->ip_cstrq, is_link) {
		if (memcmp(sym->is_name, str, size) == 0)
			return sym;
	}
	sym = ir_symbol(IR_CSTRSYM, str, size, IR_PTR_ALIGN, type);
	SIMPLEQ_INSERT_TAIL(&irprog->ip_cstrq, sym, is_link);
	return sym;
}

struct ir_symbol *
ir_physregsym(char *name, int id)
{
	struct ir_symbol *sym;

	sym = symalloc(IR_REGSYM);
	sym->is_name = name;
	sym->is_id = id;
	ir_symbol_setflags(sym, IR_SYM_PHYSREG);
	return sym;
}

struct ir_symbol *
ir_vregsym(struct ir_func *fn, struct ir_type *type)
{
	struct ir_symbol *sym;

	sym = symalloc(IR_REGSYM);
	sym->is_id = fn->if_regid++;
	sym->is_name = NULL;
	sym->is_size = type->it_size;
	sym->is_align = type->it_align;
	sym->is_type = type;
	SIMPLEQ_INSERT_TAIL(&fn->if_regq, sym, is_link);
	return sym;
}

void
ir_symbol_setflags(struct ir_symbol *sym, int flags)
{
	sym->is_flags |= flags;
}

void
ir_symbol_free(struct ir_symbol *sym)
{
	sym->is_top = freesyms;
	freesyms = sym;
}

struct ir_insn *
ir_asg(struct ir_expr *l, struct ir_expr *r)
{
	struct ir_stasg *insn;

	insn = insnalloc(IR_ASG, sizeof *insn);
	insn->is_l = refexpr(l);
	insn->is_r = refexpr(r);
	return (struct ir_insn *)insn;
}

struct ir_insn *
ir_store(struct ir_expr *l, struct ir_expr *r)
{
	struct ir_stasg *insn;

	insn = insnalloc(IR_ST, sizeof *insn);
	insn->is_l = refexpr(l);
	insn->is_r = refexpr(r);
	return (struct ir_insn *)insn;
}

struct ir_insn *
ir_b(struct ir_insn *lbl)
{
	struct ir_branch *insn;

	if (lbl == NULL)
		fatalx("ir_b");
	insn = insnalloc(IR_B, sizeof *insn);
	insn->ib_lbl = (struct ir_lbl *)lbl;
	return (struct ir_insn *)insn;
}

struct ir_insn *
ir_bc(int op, struct ir_expr *l, struct ir_expr *r, struct ir_insn *lbl)
{
	struct ir_branch *insn;

	insn = insnalloc(op, sizeof *insn);
	insn->ib_l = refexpr(l);
	insn->ib_r = refexpr(r);
	insn->ib_lbl = (struct ir_lbl *)lbl;
	return (struct ir_insn *)insn;
}

struct ir_insn *
ir_call(struct ir_expr *ret, struct ir_symbol *fn, struct ir_exprq *argq)
{
	struct ir_call *insn;

	insn = insnalloc(IR_CALL, sizeof *insn);
	insn->ic_fn = fn;
	insn->ic_ret = ret;
	ir_exprq_init(&insn->ic_argq);
	ir_exprq_cat(&insn->ic_argq, argq);
	insn->ic_firstvararg = -1;
	return (struct ir_insn *)insn;
}

struct ir_insn *
ir_ret(struct ir_expr *x, int dead)
{
	struct ir_ret *insn;

	insn = insnalloc(IR_RET, sizeof *insn);
	insn->ir_retexpr = refexpr(x);
	insn->ir_deadret = dead;
	return (struct ir_insn *)insn;
}

struct ir_insn *
ir_lbl(void)
{
	struct ir_lbl *insn;

	insn = insnalloc(IR_LBL, sizeof *insn);
	insn->il_id = newid();
	return (struct ir_insn *)insn;
}

struct ir_insn *
ir_phi(struct ir_symbol *sym)
{
	struct ir_insn *insn;

	insn = insnalloc(IR_PHI, sizeof *insn);
	insn->ip_sym = sym;
	SIMPLEQ_INIT(&insn->ip_args);
	return insn;
}

void
ir_phi_addarg(struct ir_insn *phi, struct ir_symbol *sym, struct cfa_bb *bb)
{
	struct ir_phiarg *arg;

	arg = fromfuncalloc(sizeof *arg);
	arg->ip_arg = sym;
	arg->ip_bb = bb;
	SIMPLEQ_INSERT_TAIL(&phi->ip_args, arg, ip_link);
}

void
ir_prepend_insn(struct ir_insn *oinsn, struct ir_insn *ninsn)
{
	TAILQ_INSERT_BEFORE(oinsn, ninsn, ii_link);
}

void
ir_append_insn(struct ir_func *fn, struct ir_insn *oinsn,
    struct ir_insn *ninsn)
{
	TAILQ_INSERT_AFTER(&fn->if_iq, oinsn, ninsn, ii_link);
}

void
ir_delete_insn(struct ir_func *fn, struct ir_insn *insn)
{
	TAILQ_REMOVE(&fn->if_iq, insn, ii_link);
}

void
ir_insnq_init(struct ir_insnq *iq)
{
	TAILQ_INIT(iq);
}

void
ir_insnq_add_head(struct ir_insnq *iq, struct ir_insn *insn)
{
	TAILQ_INSERT_HEAD(iq, insn, ii_link);
}

void
ir_insnq_enq(struct ir_insnq *iq, struct ir_insn *insn)
{
	TAILQ_INSERT_TAIL(iq, insn, ii_link);
}

void
ir_insnq_cat(struct ir_insnq *iq, struct ir_insnq *append)
{
	TAILQ_CONCAT(iq, append, ii_link);
}

struct ir_expr *
ir_con(int op, union ir_con val, struct ir_type *ty)
{
	struct ir_expr *x;

	ty = ir_type_dequal(ty);
	if (!IR_ISINTEGER(ty) && !IR_ISFLOATING(ty) && ty->it_op != IR_PTR)
		fatalx("ir_con %d", ty->it_op);
	x = expralloc(op, ty);
	x->ie_con = val;
	if (IR_ISFLOATING(ty))
		x->ie_sym = f64sym(val.ic_fcon);
	return x;
}

struct ir_expr *
ir_icon(intmax_t val)
{
	struct ir_expr *x;

	x = expralloc(IR_ICON, &ir_i32);
	x->ie_con.ic_icon = val;
	return x;
}

struct ir_expr *
ir_fcon(double val)
{
	struct ir_expr *x;

	x = expralloc(IR_FCON, &ir_f64);
	x->ie_sym = f64sym(val);
	x->ie_con.ic_fcon = val;
	return x;
}

struct ir_expr *
ir_var(int op, struct ir_symbol *sym)
{
	struct ir_expr *x;

	x = expralloc(op, sym->is_type);
	x->ie_sym = sym;
	return x;
}

struct ir_expr *
ir_addr(int op, struct ir_symbol *sym)
{
       struct ir_expr *x;

       x = expralloc(op, &ir_ptr);
       x->ie_sym = sym;
       return x;
}

struct ir_expr *
ir_newvreg(struct ir_func *fn, struct ir_type *type)
{
	size_t align, size;
	struct ir_symbol *sym;

	switch (type->it_op) {
	case IR_I8:
	case IR_U8:
	case IR_I16:
	case IR_U16:
	case IR_I32:
	case IR_U32:
	case IR_I64:
	case IR_U64:
	case IR_F32:
	case IR_F64:
	case IR_PTR:
#if 0
	case IR_STRUCT:
	case IR_UNION:
#endif
		align = type->it_align;
		size = type->it_size;
		break;
	default:
		fatalx("ir_newvreg: bad type %d", type->it_op);
	}

	sym = ir_symbol(IR_REGSYM, NULL, size, align, type);
	ir_func_addreg(fn, sym);
	return ir_virtreg(sym);
}

struct ir_expr *
ir_virtreg(struct ir_symbol *vreg)
{
	struct ir_expr *reg;

	reg = expralloc(IR_REG, vreg->is_type);
	reg->ie_sym = vreg;
	return reg;
}

struct ir_expr *
ir_physreg(struct ir_symbol *phreg, struct ir_type *type)
{
	struct ir_expr *reg;

	reg = expralloc(IR_REG, type);
	reg->ie_sym = phreg;
	return reg;
}

struct ir_expr *
ir_bin(int op, struct ir_expr *l, struct ir_expr *r, struct ir_type *type)
{
	struct ir_expr *x;

	x = expralloc(op, type);
	x->ie_l = refexpr(l);
	x->ie_r = refexpr(r);
	return x;
}

struct ir_expr *
ir_unary(int op, struct ir_expr *l, struct ir_type *type)
{
	struct ir_expr *x;

	x = expralloc(op, type);
	x->ie_l = refexpr(l);
	return x;
}

struct ir_expr *
ir_souref(struct ir_expr *l, struct ir_type *sou, struct ir_typelm *elm)
{
	struct ir_expr *x;

	x = expralloc(IR_SOUREF, &ir_ptr);
	x->ie_l = refexpr(l);
	x->ie_sou = sou;
	x->ie_elm = elm;
	return x;
}

struct ir_expr *
ir_cast(struct ir_expr *x, struct ir_type *toty)
{
	struct ir_expr *cast;
	struct ir_type *fromty;

	fromty = ir_type_dequal(x->ie_type);
	toty = ir_type_dequal(toty);
	if (ir_type_equal(x->ie_type, toty))
		return x;
	if ((x->i_op == IR_ICON || x->i_op == IR_FCON) && !IR_ISPTR(toty)) {
		ir_con_cast(&x->ie_con, x->ie_type, toty);
		x->ie_type = toty;
		return x;
	}
	if (ir_cast_canskip(x, toty)) {
		x->ie_type = toty;
		return x;
	}
	cast = expralloc(IR_CAST, toty);
	cast->ie_l = refexpr(x);
	return cast;
}

struct ir_expr *
ir_load(struct ir_expr *x, struct ir_type *type)
{
	struct ir_expr *load;

	load = expralloc(IR_LOAD, type);
	load->ie_l = refexpr(x);
	return load;
}

struct ir_expr *
ir_expr_copy(struct ir_expr *x)
{
	struct ir_symbol *sym;

	if (x == NULL)
		return NULL;

	if (IR_ISBINEXPR(x))
		return ir_bin(x->i_op, ir_expr_copy(x->ie_l),
		    ir_expr_copy(x->ie_r), x->ie_type);
	if (x->i_op == IR_SOUREF)
		return ir_souref(ir_expr_copy(x->ie_l), x->ie_sou, x->ie_elm);
	if (IR_ISUNEXPR(x))
		return ir_unary(x->i_op, ir_expr_copy(x->ie_l), x->ie_type);

	switch (x->i_op) {
	case IR_ICON:
	case IR_FCON:
		return ir_con(x->i_op, x->ie_con, x->ie_type);
	case IR_REG:
		sym = x->ie_sym;
		if (sym->is_flags & IR_SYM_PHYSREG)
			return ir_physreg(sym, x->ie_type);
		return ir_virtreg(sym);
	case IR_GVAR:
	case IR_PVAR:
	case IR_LVAR:
		return ir_var(x->i_op, x->ie_sym);
	case IR_GADDR:
	case IR_PADDR:
	case IR_LADDR:
		return ir_addr(x->i_op, x->ie_sym);
	default:
		fatalx("ir_expr_copy: bad op 0x%x", x->i_op);
	}
}

void
ir_expr_replace(struct ir_expr **xp, struct ir_expr *newx)
{
	*xp = newx;
	/* TODO */
}

void
ir_expr_free(struct ir_expr *x)
{
	/* TODO */
}

void
ir_expr_thisfree(struct ir_expr *x)
{
	/* TODO */
}

void
ir_con_cast(union ir_con *ic, struct ir_type *from, struct ir_type *to)
{
	from = ir_type_dequal(from);
	to = ir_type_dequal(to);
	if (IR_ISFLOATING(from)) {
		if (IR_ISF32(to))
			ic->ic_fcon = (float)ic->ic_fcon;
		else if (IR_ISF64(to))
			ic->ic_fcon = (double)ic->ic_fcon;
		else if (IR_ISI8(to))
			ic->ic_icon = (int8_t)ic->ic_fcon;
		else if (IR_ISU8(to))
			ic->ic_ucon = (uint8_t)ic->ic_fcon;
		else if (IR_ISI16(to))
			ic->ic_icon = (int16_t)ic->ic_fcon;
		else if (IR_ISU16(to))
			ic->ic_ucon = (uint16_t)ic->ic_fcon;
		else if (IR_ISI32(to))
			ic->ic_icon = (int32_t)ic->ic_fcon;
		else if (IR_ISU32(to))
			ic->ic_ucon = (uint32_t)ic->ic_fcon;
		else if (IR_ISI64(to))
			ic->ic_icon = (int64_t)ic->ic_fcon;
		else if (IR_ISU64(to))
			ic->ic_ucon = (uint64_t)ic->ic_fcon;
		else
			fatalx("ir_con_cast: bad conversion %d->%d",
			    from->it_op, to->it_op);
		return;
	}
	if (IR_ISSIGNED(from)) {
		if (IR_ISF32(to))
			ic->ic_fcon = (float)ic->ic_icon;
		else if (IR_ISF64(to))
			ic->ic_fcon = (double)ic->ic_icon;
		else if (IR_ISI8(to))
			ic->ic_icon = (int8_t)ic->ic_icon;
		else if (IR_ISU8(to))
			ic->ic_ucon = (uint8_t)ic->ic_icon;
		else if (IR_ISI16(to))
			ic->ic_icon = (int16_t)ic->ic_icon;
		else if (IR_ISU16(to))
			ic->ic_ucon = (uint16_t)ic->ic_icon;
		else if (IR_ISI32(to))
			ic->ic_icon = (int32_t)ic->ic_icon;
		else if (IR_ISU32(to))
			ic->ic_ucon = (uint32_t)ic->ic_icon;
		else if (IR_ISI64(to))
			ic->ic_icon = (int64_t)ic->ic_icon;
		else if (IR_ISU64(to))
			ic->ic_ucon = (uint64_t)ic->ic_icon;
		else
			fatalx("ir_con_cast: bad conversion %d->%d",
			    from->it_op, to->it_op);
		return;
	}
	if (IR_ISUNSIGNED(from)) {
		if (IR_ISF32(to))
			ic->ic_fcon = (float)ic->ic_ucon;
		else if (IR_ISF64(to))
			ic->ic_fcon = (double)ic->ic_ucon;
		else if (IR_ISI8(to))
			ic->ic_icon = (int8_t)ic->ic_ucon;
		else if (IR_ISU8(to))
			ic->ic_ucon = (uint8_t)ic->ic_ucon;
		else if (IR_ISI16(to))
			ic->ic_icon = (int16_t)ic->ic_ucon;
		else if (IR_ISU16(to))
			ic->ic_ucon = (uint16_t)ic->ic_ucon;
		else if (IR_ISI32(to))
			ic->ic_icon = (int32_t)ic->ic_ucon;
		else if (IR_ISU32(to))
			ic->ic_ucon = (uint32_t)ic->ic_ucon;
		else if (IR_ISI64(to))
			ic->ic_icon = (int64_t)ic->ic_ucon;
		else if (IR_ISU64(to))
			ic->ic_ucon = (uint64_t)ic->ic_ucon;
		else
			fatalx("ir_con_cast: bad conversion %d->%d",
			    from->it_op, to->it_op);
		return;
	}
	fatalx("ir_con_cast: bad conversion %d->%d", from->it_op, to->it_op);
}

void
ir_exprq_init(struct ir_exprq *eq)
{
	SIMPLEQ_INIT(eq);
}

void
ir_exprq_enq(struct ir_exprq *eq, struct ir_expr *x)
{
	x = refexpr(x);
	SIMPLEQ_INSERT_TAIL(eq, x, ie_link);
}

void
ir_exprq_cat(struct ir_exprq *eq, struct ir_exprq *append)
{
	SIMPLEQ_CONCAT(eq, append);
}

void
ir_symq_init(struct ir_symq *sq)
{
	SIMPLEQ_INIT(sq);
}

void
ir_symq_enq(struct ir_symq *sq, struct ir_symbol *sym)
{
	SIMPLEQ_INSERT_TAIL(sq, sym, is_link);
}

static struct ir_type *
typealloc(int op)
{
	struct ir_type *type;

	type = xmalloc(sizeof *type);
	type->it_op = op;
	type->it_flags = 0;
	type->it_size = 0;
	type->it_align = 0;
	type->it_base = NULL;
	SIMPLEQ_INIT(&type->it_typeq);
	type->it_dim = 0;
	irstats.i_types++;
	return type;
}

void
ir_type_setflags(struct ir_type *ty, int flags)
{
	ty->it_flags |= flags;
}

struct ir_type *
ir_type_qual(struct ir_type *ty, int qual)
{
	struct ir_type *type;

	if ((ty->it_op & qual) == qual || qual == 0)
		return ty;
	type = typealloc(qual);
	type->it_base = ty;
	type->it_dim = ty->it_dim;
	type->it_size = ty->it_size;
	type->it_align = ty->it_align;
	if (ty->it_flags & IR_COMPLETE)
		type->it_flags |= IR_COMPLETE;
	return type;
}

struct ir_type *
ir_type_ptr(struct ir_type *ty)
{
	struct ir_type *type;

	type = typealloc(IR_PTR);
	type->it_base = ty;
	type->it_size = IR_PTR_SIZE;
	type->it_align = IR_PTR_ALIGN;
	type->it_flags |= IR_COMPLETE;
	return type;
}

struct ir_type *
ir_type_arr(struct ir_type *ty, size_t dim)
{
	struct ir_type *type;

	type = typealloc(IR_ARR);
	type->it_base = ty;
	if (dim == 0)
		type->it_size = ty->it_size;
	else {
		type->it_flags |= IR_COMPLETE;
		type->it_size = ty->it_size * dim;
	}
	type->it_align = ty->it_align;
	type->it_dim = dim;
	return type;
}

struct ir_type *
ir_type_sou(int op)
{
	return typealloc(op);
}

static size_t
souelmalloc(struct ir_typelm *elm, size_t off, int flags)
{
	struct ir_type *ty;

	ty = ir_type_dequal(elm->it_type);
	if (flags & IR_PACKED)
		off = (off + 7) & ~7;
	else
		off = (off + ty->it_align * 8 - 1) & ~(ty->it_align * 8 - 1);
	elm->it_off = off;
	return off + ty->it_size * 8;
}

static size_t
soufldalloc(struct ir_typelm *elm, size_t off, int flags)
{
	size_t sbound;
	struct ir_type *ty;

	ty = ir_type_dequal(elm->it_type);
	if (elm->it_fldsize == 0 && !(flags & IR_PACKED))
		off = (off + ty->it_align * 8 - 1) & ~(ty->it_align * 8 - 1);

	sbound = (off & (ty->it_align * 8 - 1)) + elm->it_fldsize;
	if (sbound > ty->it_align * 8 && !(flags & IR_PACKED))
		off = (off + ty->it_align * 8 - 1) & ~(ty->it_align * 8 - 1);
	elm->it_off = off;
	return off + elm->it_fldsize;
}

/*
 * Must be called with an unqualified type.
 */
void
ir_type_sou_finish(struct ir_type *ty, int flags)
{
	size_t align = 1, off = 0, size = 0;
	struct ir_type *elmty;
	struct ir_typelm *elm;

	if (IR_ISQUAL(ty) || (!IR_ISUNION(ty) && !IR_ISSTRUCT(ty)))
		fatalx("ir_type_sou_finish");

	ty->it_flags = flags | IR_COMPLETE;

	SIMPLEQ_FOREACH(elm, &ty->it_typeq, it_link) {
		if (elm->it_fldsize < 0)
			off = souelmalloc(elm, off, flags);
		else
			off = soufldalloc(elm, off, flags);

		elmty = ir_type_dequal(elm->it_type);
		if (elmty->it_align > align && !(flags & IR_PACKED))
			align = elmty->it_align;

		if (IR_ISUNION(ty)) {
			if (off > size)
				size = off;
			off = 0;
		} else
			size = off;
	}

	ty->it_align = align;
	ty->it_size = (size + align * 8 - 1) & ~(align * 8 - 1);
	ty->it_size /= 8;
}

struct ir_type *
ir_type_func(struct ir_type *base)
{
	struct ir_type *type;

	type = typealloc(IR_FUNTY);
	type->it_base = base;
	type->it_flags |= IR_COMPLETE;
	return type;
}

void
ir_type_newelm(struct ir_type *type, struct ir_typelm *elm)
{
	SIMPLEQ_INSERT_TAIL(&type->it_typeq, elm, it_link);
}

struct ir_typelm *
ir_typelm(char *name, struct ir_type *ty, int size, int usesize)
{
	struct ir_typelm *elm;

	elm = xmalloc(sizeof *elm);
	elm->it_name = name;
	elm->it_type = ty;
	if (usesize)
		elm->it_fldsize = size;
	else
		elm->it_fldsize = -1;
	elm->it_off = 0;
	return elm;
}

/* TODO: Needs to be extended */
int
ir_type_equal(struct ir_type *t1, struct ir_type *t2)
{
	if (t1 == t2)
		return 1;
	if (t1->it_op != t2->it_op)
		return 0;
	if (IR_ISPTR(t1) && IR_ISPTR(t2))
		return 1;
	return 0;
}

struct ir_type *
ir_type_dequal(struct ir_type *ty)
{
	if (IR_ISQUAL(ty))
		ty = ty->it_base;
	return ty;
}

void
ir_setemit(struct ir *ir, char *str)
{
#if 0
	struct ir_insn *insn;

	if (!IR_ISINSN(ir))
		fatalx("ir_setemit called for non-insn %x", ir->i_op);
	insn = (struct ir_insn *)ir;
	insn->ii_emit = str;
#endif

	ir->i_emit = str;
}

#define EMIT_FRAMEREG	0x01
#define EMIT_PRIMREG	0x02
#define EMIT_SECREG	0x04
#define EMIT_OFF	0x08
#define EMIT_UNSG	0x10
#define EMIT_WORD	0x20
#define EMIT_HEX	0x40
#define EMIT_SGN	0x80

void
ir_emit(struct ir *node, struct ir *ancestor)
{
	int fail, flags, n;
	char *p, *q;
	struct ir *ir = node;

	for (p = node->i_emit, flags = 0; *p != '\0'; p++, flags = 0) {
		for (q = p; *p != '@' && *p != '\0'; p++)
			continue;
		EMITWRITE(q, 1, p - q);
		if (*p == '\0')
			break;

		fail = 1;
		if (p[1] == '@') {
			emitc('@');
			p++;
			continue;
		}
again:
		switch (*++p) {
		case 'A':
			ir = ancestor;
			fail = 0;
			goto again;
#if 0
			ir_emit_self(ancestor, 0);
			break;
#endif
		case 'L':
		case 'R':
			fail = 0;
			if ((*p == 'L' && ir_nkids[ir->i_op] < 1) ||
			    (*p == 'R' && ir_nkids[ir->i_op] < 2)) {
				fatalx("couldn't get child %c, at %s[%zd]",
				    *p, node->i_emit, p - node->i_emit);
			} else if (*p == 'L')
				ir = ir->i_l;
			else
				ir = ir->i_r;
			goto again;
		case 'D':
			flags |= EMIT_SGN;
			goto again;
		case 'F':
			flags |= EMIT_FRAMEREG;
			goto again;
		case 'O':
			flags |= EMIT_OFF;
			goto again;
		case 'U':
			flags |= EMIT_UNSG;
			goto again;
		case 'X':
			flags |= EMIT_HEX;
			goto again;
		case 'W':
			flags |= EMIT_WORD;
			goto again;
		case 'P':
		case 'S':
			flags |= *p == 'P' ? EMIT_PRIMREG : EMIT_SECREG;
			goto again;
		case 'T':
			ir_emit_self((struct ir *)ir, flags);
			break;
		case 'Y':
			n = *++p - '0';
			emits(ir->i_tmpregsyms[n]->is_name);
			break;
		case 'Z':
			p = ir_emit_machdep(ir, p);
			break;
		default:
			if (fail) {
#if 0
				ir_dump_insn(stderr, node);
#endif
				fatalx("unknown control char: %c", *p);
			}
			p--;
			if (ir != ancestor && ir->i_emit != NULL)
				ir_emit(ir, ancestor);
			else
				ir_emit_self(ir, flags);
		}

		ir = node;
	}
}

void
ir_emit_self(struct ir *ir, int flags)
{
	struct ir_branch *b = (struct ir_branch *)ir;
	struct ir_expr *x = (struct ir_expr *)ir;
	struct ir_symbol *sym;
	uintmax_t mask;

	switch (ir->i_op) {
	case IR_B:
	case IR_BEQ:
	case IR_BNE:
	case IR_BLT:
	case IR_BLE:
	case IR_BGT:
	case IR_BGE:
		emitf(".L%d", b->ib_lbl->il_id);
		break;
	case IR_ICON:
		if (IR_ISPTR(x->ie_type) || IR_ISUNSIGNED(x->ie_type) ||
		    (flags & (EMIT_UNSG | EMIT_HEX))) {
			mask = ~0;
			if (flags & EMIT_WORD)
				mask = 0xffff;
			if (flags & EMIT_HEX)
				emitf("0x%jx", x->ie_con.ic_ucon & mask);
			else
				emitf("%ju", x->ie_con.ic_ucon & mask);
		} else if (IR_ISSIGNED(x->ie_type) || (flags & EMIT_SGN))
			emitf("%jd", x->ie_con.ic_icon);
		else
			goto bad;
		break;
	case IR_REG:
		sym = x->ie_sym;
		if (flags & EMIT_PRIMREG) {
			ir_emit_regpair(sym, 0);
			break;
		} else if (flags & EMIT_SECREG) {
			ir_emit_regpair(sym, 1);
			break;
		}
		if (sym->is_name != NULL)
			emits(sym->is_name);
		else
			emitf("%%r_%d", sym->is_id);
		break;
	case IR_GVAR:
	case IR_GADDR:
		sym = x->ie_sym;
		if (sym->is_op == IR_CSTRSYM)
			emitf(".L%d", sym->is_id);
		else
			emits(sym->is_name);
		break;
	case IR_LVAR:
	case IR_PVAR:
	case IR_LADDR:
	case IR_PADDR:
		if (flags & EMIT_FRAMEREG) {
			emits(physregs[TARG_FRAMEREG]->is_name);
			break;
		} else if (flags & EMIT_OFF) {
			emitf("%zd", x->ie_sym->is_off);
			break;
		}
	default:
bad:
		fatalx("ir_emit_self: bad op: 0x%x", ir->i_op);
	}
}
