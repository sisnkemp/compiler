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

#include <stdlib.h>

#include "comp/comp.h"
#include "comp/ir.h"

#include "lang.c/c.h"

void
builtin_init_machdep(void)
{
	int i;
	char *name;
	struct ir_type *charptr, *valist;
	struct ir_typelm *elm;
	struct symbol *sym;

	struct {
		char	*name;
		struct	ir_type *type;
	} elems[4] = {
		{ "gpr", &cir_char },
		{ "fpr", &cir_char },
		{ "overflow_arg_area" },
		{ "reg_save_area" }
	};

	/*
	 * Register __builtin_va_list, the va_list type for powerpc.
	 * According to the ABI, it has the following format:
	 *
	 * typedef struct {
	 * 	char	gpr;
	 * 	char	fpr;
	 * 	char	*overflow_arg_area;
	 * 	char	*reg_save_area;
	 * } __builtin_va_list[1];
	 */
	valist = ir_type_sou(IR_STRUCT);
	charptr = ir_type_ptr(&cir_char);
	elems[2].type = elems[3].type = charptr;
	for (i = 0; i < 4; i++) {
		name = ntenter(&names, elems[i].name);
		elm = ir_typelm(name, elems[i].type, 0, 0);
		ir_type_newelm(valist, elm);
	}
	ir_type_sou_finish(valist, 0);
	valist = ir_type_arr(valist, 1);
	name = ntenter(&names, "__builtin_va_list");
	sym = symenter(name, valist, NSORD);
	sym->s_flags |= SYM_BUILTIN | SYM_TYPEDEF;
	builtin_va_list = valist;
}

static void
storeap(struct ir_func *fn, struct ir_insnq *iq, struct ir_symbol *ap,
    struct ir_expr *val, struct ir_typelm *elm)
{
	struct ir_expr *tmp;

	tmp = ir_virtreg(ap);
	tmp = ir_souref(tmp, builtin_va_list->it_base, elm);
	ir_insnq_enq(iq, ir_store(tmp, val));
}

static struct ir_expr *
loadap(struct ir_symbol *ap, struct ir_typelm *elm)
{
	struct ir_expr *tmp;

	tmp = ir_virtreg(ap);
	tmp = ir_souref(tmp, builtin_va_list->it_base, elm);
	tmp = ir_load(tmp, elm->it_type);
	return tmp;
}

struct ir_expr *
u32con(uint32_t val)
{
	union ir_con con;

	con.ic_ucon = val;
	return ir_con(IR_ICON, con, &ir_u32);
}

static struct ir_expr *
roundval(struct ir_expr *x, uint32_t val)
{
	struct ir_expr *icon, *compl;

	icon = u32con(val - 1);
	compl = u32con(~(val - 1));
	x = ir_bin(IR_ADD, x, icon, x->ie_type);
	return ir_bin(IR_AND, x, compl, x->ie_type);
}

struct ir_expr *
builtin_va_start_gencode(struct ir_func *fn, struct ir_insnq *iq,
    struct ast_expr *x, int flags)
{
	int gpr, fpr, i;
	union ir_con con;
	struct ast_expr *ap, *last;
	struct ir_expr *reg, *tmp;
	struct ir_param *parms;
	struct ir_symbol *sym;
	struct ir_typelm *elm;

	ir_func_va_prepare(fn);
	ap = TAILQ_FIRST(&x->ae_args->al_exprs);
	last = TAILQ_NEXT(ap, ae_link);
	sym = last->ae_sym->s_irsym;
	parms = ir_parlocs_func(fn);

	/*
	 * Get the symbol for the last param and find out
	 * which are the first registers that hold variadic
	 * arguments. Registers below don't need to be saved in
	 * the function prologue.
	 */
	gpr = REG_R3;
	fpr = REG_F1;
	for (i = 0; parms[i].ip_argsym != NULL; i++) {
		if (gpr == parms[i].ip_reg)
			gpr++;
		else if (fpr == parms[i].ip_reg)
			fpr++;
		else if (parms[i].ip_reg >= REG_R3R4 &&
		    parms[i].ip_reg <= REG_R28R29)
			gpr = pairtogpr[parms[i].ip_reg - REG_R3R4][1] + 1;
		if (sym == parms[i].ip_argsym)
			break;
	}
	if (sym == NULL)
		fatalx("builtin_va_start_gencode2");
	fn->ifm_firstgpr = gpr;
	fn->ifm_firstfpr = fpr;

	/*
	 * Generate code to initialize the members in the va_list structure.
	 */
	tmp = ast_expr_gencode(fn, iq, ap, 0);
	reg = ir_newvreg(fn, &ir_ptr);
	ir_insnq_enq(iq, ir_asg(reg, tmp));

	/* Store gpr. */
	elm = SIMPLEQ_FIRST(&builtin_va_list->it_base->it_typeq);
	con.ic_ucon = gpr - REG_R3;
	tmp = ir_con(IR_ICON, con, &cir_uchar);
	storeap(fn, iq, reg->ie_sym, tmp, elm);

	/* Store fpr. */
	elm = SIMPLEQ_NEXT(elm, it_link);
	con.ic_ucon = fpr - REG_F1;
	tmp = ir_con(IR_ICON, con, &cir_uchar);
	storeap(fn, iq, reg->ie_sym, tmp, elm);

	/* Store overflow_arg_area. */
	elm = SIMPLEQ_NEXT(elm, it_link);
	tmp = ir_var(IR_LVAR, fn->ifm_vastack);
	storeap(fn, iq, reg->ie_sym, tmp, elm);

	/* Store reg_save_area. */
	elm = SIMPLEQ_NEXT(elm, it_link);
	tmp = ir_var(IR_LADDR, fn->ifm_vasaves);
	storeap(fn, iq, reg->ie_sym, tmp, elm);

	free(parms);
	return NULL;
}

struct ir_expr *
builtin_va_arg_gencode(struct ir_func *fn, struct ir_insnq *iq,
    struct ast_expr *x, int flags)
{
	struct ast_expr *ap, *typearg;
	struct ir_expr *arg, *icon, *gpr, *fpr, *ptr, *reg, *tmp;
	struct ir_insn *fromstack, *done;
	struct ir_type *ty;
	struct ir_typelm *gprelm, *fprelm, *oaaelm, *rsaelm;

	ir_func_va_prepare(fn);
	ap = TAILQ_FIRST(&x->ae_args->al_exprs);
	typearg = TAILQ_NEXT(ap, ae_link);
	ty = typearg->ae_type;

	tmp = ast_expr_gencode(fn, iq, ap, 0);
	reg = ir_newvreg(fn, &ir_ptr);
	ir_insnq_enq(iq, ir_asg(reg, tmp));

	/*
	 * The code generated for va_arg(ap, type) does something this:
	 * The outcome of if-statements depending on type can be determined
	 * at compile-time, so only a small portion of the code below is
	 * generated.
	 *
	 * if (gprtype(type)) {
	 * 	gpr = ap->gpr;
	 * 	if (quad(type)) {
	 * 		lastreg = 7;
	 * 		gpr = (gpr + 1) & ~1;
	 * 	} else
	 * 		lastreg = 8;
	 * 	if (gpr < lastreg) {
	 * 		arg = *((type *)ap->reg_save_area[gpr * 4];
	 * 		if (quad(type))
	 * 			ap->gpr = gpr + 2;
	 * 		else
	 * 			ap->gpr = gpr + 1;
	 * 	} else {
	 * fromstack:
	 * 		ptr = ap->overflow_save_area;
	 * 		if (quad(type))
	 * 			ptr = (ptr + 7) & ~7;
	 * 		arg = *((type *)ptr);
	 * 		if (quad(type))
	 * 			ap->overflow_save_area = ptr + 8;
	 * 		else
	 * 			ap->overflow_save_area = ptr + 4;
	 * 	}
	 * } else {
	 * 	fpr = ap->fpr;
	 * 	if (fpr < 8) {
	 * 		arg = *((double *)ap->reg_save_area[fpr * 8 + 32]);
	 * 		ap->fpr = fpr + 1;
	 * 	} else {
	 * fromstack:
	 * 		ptr = (ap->overflow_save_area + 7) & ~7;
	 * 		arg = *((double *)ptr);
	 * 		ap->overflow_save_area = ptr + 8;
	 * 	}
	 * }
	 * done:
	 * 	;
	 */

	gprelm = SIMPLEQ_FIRST(&builtin_va_list->it_base->it_typeq);
	fprelm = SIMPLEQ_NEXT(gprelm, it_link);
	oaaelm = SIMPLEQ_NEXT(fprelm, it_link);
	rsaelm = SIMPLEQ_NEXT(oaaelm, it_link);
	
	fromstack = ir_lbl();
	done = ir_lbl();
	if (!IR_ISFLOATING(ty)) {
		if (IR_ISARR(ty) || IR_ISFUNTY(ty))
			ty = &ir_ptr;

		gpr = ir_newvreg(fn, &ir_u32);
		tmp = ir_cast(loadap(reg->ie_sym, gprelm), &ir_u32);
		if (IR_ISQUAD(ty)) {
			tmp = roundval(tmp, 2);
			icon = u32con(7);
		} else
			icon = u32con(8);
		ir_insnq_enq(iq, ir_asg(gpr, tmp));

		gpr = ir_virtreg(gpr->ie_sym);
		ir_insnq_enq(iq, ir_bc(IR_BGE, gpr, icon, fromstack));

		icon = u32con(4);
		tmp = loadap(reg->ie_sym, rsaelm);
		gpr = ir_virtreg(gpr->ie_sym);
		tmp = ir_bin(IR_ADD, tmp, ir_bin(IR_MUL, gpr, icon, &ir_u32),
		    &ir_ptr);
		if (IR_ISSOU(ty)) {
			tmp = ir_load(tmp, &ir_ptr);
			arg = ir_newvreg(fn, &ir_ptr);
		} else {
			tmp = ir_load(tmp, ty);
			arg = ir_newvreg(fn, ty);
		}
		ir_insnq_enq(iq, ir_asg(arg, tmp));

		gpr = ir_virtreg(gpr->ie_sym);
		if (IR_ISQUAD(ty))
			icon = u32con(2);
		else
			icon = u32con(1);
		tmp = ir_bin(IR_ADD, gpr, icon, &ir_u32);
		tmp = ir_cast(tmp, &ir_u8);
		storeap(fn, iq, reg->ie_sym, tmp, gprelm);

		ir_insnq_enq(iq, ir_b(done));

		ir_insnq_enq(iq, fromstack);
		tmp = loadap(reg->ie_sym, oaaelm);
		ptr = ir_newvreg(fn, &ir_ptr);
		if (IR_ISQUAD(ty)) {
			tmp = roundval(tmp, 8);
			icon = u32con(8);
		} else
			icon = u32con(4);
		ir_insnq_enq(iq, ir_asg(ptr, tmp));
		arg = ir_virtreg(arg->ie_sym);
		ptr = ir_virtreg(ptr->ie_sym);
		ir_insnq_enq(iq, ir_asg(arg, ir_load(ptr, arg->ie_type)));

		ptr = ir_virtreg(ptr->ie_sym);
		tmp = ir_bin(IR_ADD, ptr, icon, &ir_ptr);
		storeap(fn, iq, reg->ie_sym, tmp, oaaelm);
	} else {
		fpr = ir_newvreg(fn, &ir_u32);
		tmp = ir_cast(loadap(reg->ie_sym, fprelm), &ir_u32);
		ir_insnq_enq(iq, ir_asg(fpr, tmp));
		
		fpr = ir_virtreg(fpr->ie_sym);
		ir_insnq_enq(iq, ir_bc(IR_BGE, fpr, u32con(8), fromstack));

		fpr = ir_virtreg(fpr->ie_sym);
		tmp = loadap(reg->ie_sym, rsaelm);
		tmp = ir_bin(IR_ADD, tmp, u32con(32), &ir_ptr);
		tmp = ir_bin(IR_ADD, tmp,
		    ir_bin(IR_MUL, fpr, u32con(8), &ir_u32), &ir_ptr);
		tmp = ir_load(tmp, &ir_f64);
		arg = ir_newvreg(fn, &ir_f64);
		ir_insnq_enq(iq, ir_asg(arg, tmp));

		fpr = ir_virtreg(fpr->ie_sym);
		tmp = ir_bin(IR_ADD, fpr, u32con(1), &ir_u32);
		tmp = ir_cast(tmp, &ir_u8);
		storeap(fn, iq, reg->ie_sym, tmp, fprelm);

		ir_insnq_enq(iq, ir_b(done));

		ir_insnq_enq(iq, fromstack);
		tmp = loadap(reg->ie_sym, oaaelm);
		ptr = ir_newvreg(fn, &ir_ptr);
		tmp = roundval(tmp, 8);
		arg = ir_virtreg(arg->ie_sym);
		ptr = ir_virtreg(ptr->ie_sym);
		ir_insnq_enq(iq, ir_asg(arg, ir_load(ptr, &ir_f64)));

		ptr = ir_virtreg(ptr->ie_sym);
		tmp = ir_bin(IR_ADD, ptr, u32con(8), &ir_ptr);
		storeap(fn, iq, reg->ie_sym, tmp, oaaelm);
	}

	ir_insnq_enq(iq, done);
	if (IR_ISSOU(ty)) {
		arg = ir_virtreg(arg->ie_sym);
		return ir_load(arg, ty);	
	}
	return ir_virtreg(arg->ie_sym);
}

struct ir_expr *
builtin_va_end_gencode(struct ir_func *fn, struct ir_insnq *iq,
    struct ast_expr *x, int flags)
{
	struct ast_expr *arg;

	arg = TAILQ_FIRST(&x->ae_args->al_exprs);
	ast_expr_gencode(fn, iq, arg, GC_FLG_DISCARD);
	return NULL;
}
