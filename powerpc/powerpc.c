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
#include "comp/passes.h"

static void stackoff_calc_argareasz(struct ir_func *);

int8_t gprtopair[REG_R30 - REG_R0 + 1] = {
	-1, -1,			/* REG_R0, REG_R1 */
	REG_R3R4, REG_R3R4,	/* REG_R3, REG_R4 */
	REG_R5R6, REG_R5R6,	/* REG_R5, REG_R6 */
	REG_R7R8, REG_R7R8,	/* REG_R7, REG_R8 */
	REG_R9R10, REG_R9R10,	/* REG_R9, REG_R10 */
	REG_R11R12, REG_R11R12,	/* REG_R11, REG_R12 */
	REG_R14R15, REG_R14R15,	/* REG_R14, REG_R15 */
	REG_R16R17, REG_R16R17,	/* REG_R16, REG_R17 */
	REG_R18R19, REG_R18R19,	/* REG_R18, REG_R19 */
	REG_R20R21, REG_R20R21,	/* REG_R20, REG_R21 */
	REG_R22R23, REG_R22R23,	/* REG_R22, REG_R23 */
	REG_R24R25, REG_R24R25,	/* REG_R24, REG_R25 */
	REG_R26R27, REG_R26R27,	/* REG_R26, REG_R27 */
	REG_R28R29, REG_R28R29,	/* REG_R28, REG_R29 */
	-1,
};

int8_t pairtogpr[REG_R28R29 - REG_R3R4 + 1][2] = {
	{ REG_R3, REG_R4 }, { REG_R5, REG_R6 },
	{ REG_R7, REG_R8 }, { REG_R9, REG_R10 },
	{ REG_R11, REG_R12 }, { REG_R14, REG_R15 },
	{ REG_R16, REG_R17 }, { REG_R18, REG_R19 },
	{ REG_R20, REG_R21 }, { REG_R22, REG_R23 },
	{ REG_R24, REG_R25 }, { REG_R26, REG_R27 },
	{ REG_R28, REG_R29 }
};

void
targinit(void)
{
}

static void
loadint(char *reg, struct ir_expr *con, int hi)
{
	int32_t val;

	if (hi)
		val = con->ie_con.ic_icon >> 32;
	else
		val = con->ie_con.ic_icon;

	if (val >= TARG_SIMM_MIN && val <= TARG_SIMM_MAX)
		emitf("\tli\t%s, %d\n", reg, val);
	else {
		emitf("\tlis\t%s, 0x%x\n", reg, (uint32_t)val >> 16);
		if (val & 0xffff)
			emitf("\tori\t%s, %s, 0x%x\n",
			    reg, reg, val & 0xffff);
	}
}

int
ir_cast_canskip(struct ir_expr *x, struct ir_type *toty)
{
	if (IR_ISLONG(x->ie_type) && IR_ISLONG(toty))
		return 1;
	if (IR_ISLONG(x->ie_type) && IR_ISPTR(toty))
		return 1;
	if (IR_ISPTR(x->ie_type) && IR_ISLONG(toty))
		return 1;
	return 0;
}

char *
ir_emit_machdep(struct ir *ir, char *str)
{
	char *hireg, *loreg;
	int id;
	struct ir_stasg *asg;

	if (str[1] == 'L' && str[2] == 'D') {
		asg = (struct ir_stasg *)ir;
		id = asg->is_r->ie_sym->is_id;
		emitf("\tlis\t%%r12, .L%d@ha\n", id);
		emitf("\tlfd\t%s, .L%d@l(%%r12)\n",
		    asg->is_l->ie_sym->is_name, id);
		return &str[2];
	} else if (str[1] == 'L' && str[2] == 'I') {
		asg = (struct ir_stasg *)ir;
		loadint(asg->is_l->ie_sym->is_name, asg->is_r, 0);
		return &str[2];
	} else if (str[1] == 'Q' && str[2] == 'I') {
		asg = (struct ir_stasg *)ir;
		id = asg->is_l->ie_sym->is_id;
		hireg = physregs[pairtogpr[id - REG_R3R4][0]]->is_name;
		loreg = physregs[pairtogpr[id - REG_R3R4][1]]->is_name;
		loadint(hireg, asg->is_r, 1);
		loadint(loreg, asg->is_r, 0);
		return &str[2];
	}
	return str;
}

void
ir_emit_regpair(struct ir_symbol *sym, int sec)
{
	int reg;

	if (sym->is_id < REG_R3R4 || sym->is_id > REG_R28R29)
		fatalx("ir_emit_regpair %s_%d, %d",
		    sym->is_name, sym->is_id, sec);
	reg = pairtogpr[sym->is_id - REG_R3R4][sec];
	emitf("%s", physregs[reg]->is_name);
}

void
ir_func_machdep(struct ir_func *fn)
{
	fn->ifm_vasaves = fn->ifm_vastack = NULL;
	fn->ifm_saver31 = 0;
}

void
ir_func_va_prepare(struct ir_func *fn)
{
	if (fn->ifm_vastack != NULL)
		return;

	/* Allocate memory for the register save area. */
	fn->ifm_vasaves = ir_symbol(IR_VARSYM, NULL, 8 * 4 + 8 * 8, 8,
	    &ir_ptr);
	ir_func_addvar(fn, fn->ifm_vasaves);

	/* Address of the argument save area of the caller will go here. */
	fn->ifm_vastack = ir_symbol(IR_VARSYM, NULL, 4, 4, &ir_u32);
	ir_symbol_setflags(fn->ifm_vastack, IR_SYM_VOLAT);
	ir_func_addvar(fn, fn->ifm_vastack);
}

int
ir_symbol_rclass(struct ir_symbol *sym)
{
	if (sym->is_id >= REG_R0 && sym->is_id <= REG_R31)
		return REG_R32REGS;
	if (sym->is_id >= REG_F0 && sym->is_id <= REG_F31)
		return REG_F64REGS;
	if (sym->is_id >= REG_R3R4 && sym->is_id <= REG_R28R29)
		return REG_R64REGS;
	switch (sym->is_type->it_op) {
	case IR_I8:
	case IR_U8:
	case IR_I16:
	case IR_U16:
	case IR_I32:
	case IR_U32:
	case IR_PTR:
		return REG_R32REGS;
	case IR_I64:
	case IR_U64:
		return REG_R64REGS;
	case IR_F64:
		return REG_F64REGS;
	default:
		fatalx("ir_type_rclass: bad type: %d", sym->is_type->it_op);
	}
}

void
ir_parlocs(struct ir_param *parms, int nparms)
{
	int gpr, fpr, i, pair;

	gpr = REG_R3;
	fpr = REG_F1;
	for (i = 0; i < nparms; i++) {
		parms[i].ip_reg = -1;
		if (IR_ISQUAD(parms[i].ip_type)) {
			if (gpr <= REG_R9) {
				pair = gpr + ((gpr - REG_R3) & 1) - REG_R0;
				pair = gprtopair[pair];
				parms[i].ip_reg = pair;
				gpr = pairtogpr[pair - REG_R3R4 + 1][0];
			}
		} else if (IR_ISINTEGER(parms[i].ip_type) ||
		    IR_ISPTR(parms[i].ip_type)) {
			if (gpr <= REG_R10)
				parms[i].ip_reg = gpr++;
		} else if (IR_ISFLOATING(parms[i].ip_type)) {
			if (fpr <= REG_F8)
				parms[i].ip_reg = fpr++;
		} else
			fatalx("parlocs: bad type op: %d",
			    parms[i].ip_type->it_op);
	}
}

void
pass_parmfixup_machdep(struct ir_func *fn, struct regpar *rp)
{
	int i, j;
	struct ir_param *parms;

	parms = ir_parlocs_func(fn);
	for (i = j = 0; parms[i].ip_type != NULL; i++) {
		if (parms[i].ip_reg == -1)
			continue;
		rp[j].r_par = parms[i].ip_argsym;
		rp[j].r_reg = parms[i].ip_reg;
		j++;
	}
	rp[i].r_par = NULL;
	free(parms);
}

void
pass_stackoff_machdep(struct ir_func *fn)
{
	int i;
	ssize_t off;

	fn->if_framesz = 8;
	stackoff_calc_argareasz(fn);
	fn->if_framesz += fn->if_argareasz;

	off = pass_stackoff_reg(fn, fn->if_framesz, 0);
	off = pass_stackoff_var(fn, off, 0);
	if (fn->if_flags & IR_FUNC_PROTSTACK) {
		fn->ifm_guardoff = ((fn->if_framesz + 3) & ~3);
		fn->if_framesz = ((fn->if_framesz + 3) & ~3) + 4;
	}
	if (fn->ifm_saver31) {
		fn->ifm_r31off = (fn->if_framesz + 3) & ~3;
		fn->if_framesz = ((fn->if_framesz + 3) & ~3) + 4;
	}

	for (i = REG_R14; i <= REG_R31; i++) {
		if (BITVEC_ISSET(&fn->if_usedregs, i))
			fn->if_framesz = ((fn->if_framesz + 3) & ~3) + 4;
	}
	for (i = REG_F14; i <= REG_F31; i++) {
		if (BITVEC_ISSET(&fn->if_usedregs, i))
			fn->if_framesz = ((fn->if_framesz + 7) & ~7) + 8;
	}

	fn->if_framesz = (fn->if_framesz + 15) & ~15;
	pass_stackoff_par(fn, fn->if_framesz + 8, 0);
	if (fn->if_framesz > 32 * 1024)
		fatalx("pass_stackoff_machdep: stack frame too large");
}

ssize_t
pass_stackoff_paralign(struct ir_symbol *sym, ssize_t off)
{
	if (IR_ISQUAD(sym->is_type) || IR_ISF64(sym->is_type))
		return (off + 7) & ~7;
	else if (IR_ISPTR(sym->is_type) || IR_ISINTEGER(sym->is_type))
		return (off + 3) & ~3;
	else
		fatalx("pass_stackoff_paralign: bad type: %d",
		    sym->is_type->it_op);
	return off;
}

static void
stackoff_calc_argareasz(struct ir_func *fn)
{
	size_t maxsz, sz;
	int i;
	struct ir_call *call;
	struct ir_insn *insn;
	struct ir_param *parms;

	maxsz = sz = 0;
	TAILQ_FOREACH(insn, &fn->if_iq, ii_link) {
		if (insn->i_op != IR_CALL)
			continue;

		call = (struct ir_call *)insn;
		parms = ir_parlocs_call(insn);
		for (i = sz = 0; parms[i].ip_type != NULL; i++) {
			if (parms[i].ip_reg != -1)
				continue;
			if ((IR_ISINTEGER(parms[i].ip_type) &&
			    !IR_ISQUAD(parms[i].ip_type)) ||
			    IR_ISPTR(parms[i].ip_type))
				sz += 4;
			else if (IR_ISQUAD(parms[i].ip_type) ||
			    IR_ISF64(parms[i].ip_type))
				sz = ((sz + 7) & ~7) + 8;
			else
				fatalx("stackoff_calc_argareasz: "
				    "unsupported type %d",
				    parms[i].ip_type->it_op);
		}
		free(parms);
		if (sz > maxsz)
			maxsz = sz;
	}
	fn->if_argareasz = maxsz;
}

void
pass_ralloc_callargs(struct ir_func *fn, struct ir_insn *insn)
{
	int i = 0, j, rc;
	struct ir_param *parms;
	
	static int init;
	static int rclasses[REG_NREGS];

	if (!init) {
		init = 1;
		for (i = 0; i < REG_NREGS; i++)
			rclasses[i] = ir_symbol_rclass(physregs[i]);
	}

	parms = ir_parlocs_call(insn);
	for (i = 0; parms[i].ip_argsym != NULL; i++) {
		if (parms[i].ip_reg == -1)
			continue;
#if 0
		j = parms[i].ip_reg;
		pass_ralloc_precolor(parms[i].ip_argsym, j);
#else

		rc = ir_symbol_rclass(parms[i].ip_argsym);
		for (j = 0; j < REG_NREGS; j++) {
			if (j == REG_R1 || parms[i].ip_reg == j)
				continue;
			if (rc != rclasses[j])
				continue;
			pass_ralloc_addedge(fn, j, parms[i].ip_argsym->is_id);
		}
#endif
	}
	free(parms);
}

int
pass_ralloc_retreg(struct ir_type *type)
{
	if (IR_ISQUAD(type))
		return REG_R3R4;
	if (IR_ISINTEGER(type) || IR_ISPTR(type))
		return REG_R3;
	if (IR_ISFLOATING(type))
		return REG_F1;
	fatalx("pass_ralloc_retreg: bad type: %d", type->it_op);
}

static void
storereg(char *op, struct ir_symbol *reg, size_t off)
{
	if (off > TARG_SIMM_MAX) {
		/*
		 * XXX: We can use r11 here since it's volatile and if it is
		 * used by a variable that is live across the call, it must
		 * have been spilled already.
		 */
		emitf("\tlis\t%%r11, %zu@ha\n", off);
		emitf("\taddi\t%%r11, %%r11, %zu@l\n", off);
		emitf("\t%sx\t%s, %%r1, %%r11\n", op, reg->is_name);
	} else
		emitf("\t%s\t%s, %zd(%%r1)\n", op, reg->is_name, off);
}

void
pass_emit_call(struct ir_func *fn, struct ir_call *call)
{
	int i, hasflt;
	int dhi, dlo, shi, slo;
	struct ir_param *parms;
	size_t off;
	struct ir_expr *x;
	struct ir_symbol *sym;
	struct ir_type *rety = NULL;

	if (call->ic_ret != NULL)
		rety = call->ic_ret->ie_type;

	parms = ir_parlocs_call((struct ir_insn *)call);
	off = 8;
	for (i = hasflt = 0; parms[i].ip_type != NULL; i++) {
		x = parms[i].ip_argx;
		sym = parms[i].ip_argsym;
		if ((IR_ISINTEGER(x->ie_type) || IR_ISPTR(x->ie_type)) &&
		    !IR_ISQUAD(x->ie_type)) {
			if (parms[i].ip_reg == -1) {
				storereg("stw", sym, off);
				off += 4;
			} else if (parms[i].ip_reg != sym->is_id)
				emitf("\tmr\t%s, %s\n",
				    physregs[parms[i].ip_reg]->is_name,
				    sym->is_name);
		} else if (IR_ISQUAD(x->ie_type)) {
			if (parms[i].ip_reg == -1) {
				off = (off + 7) & ~7;
				storereg("stw",
				    physregs[pairtogpr[sym->is_id][1]], off);
				storereg("stw",
				    physregs[pairtogpr[sym->is_id][0]],
				    off + 4);
				off += 8;
			} else if (parms[i].ip_reg != sym->is_id) {
				shi = pairtogpr[sym->is_id - REG_R3R4][0];
				slo = pairtogpr[sym->is_id - REG_R3R4][1];
				dhi = pairtogpr[parms[i].ip_reg - REG_R3R4][0];
				dlo = pairtogpr[parms[i].ip_reg - REG_R3R4][1];
				emitf("\tmr\t%s, %s\n",
				    physregs[dhi]->is_name,
				    physregs[shi]->is_name);
				emitf("\tmr\t%s, %s\n",
				    physregs[dlo]->is_name,
				    physregs[slo]->is_name);
			}
		} else if (IR_ISF64(x->ie_type)) {
			if (i >= call->ic_firstvararg)
				hasflt = 1;
			if (parms[i].ip_reg == -1) {
				off = (off + 7) & ~7;
				storereg("stfd", sym, off);
				off += 8;
			} else if (parms[i].ip_reg != sym->is_id)
				emitf("\tfmr\t%s, %s\n",
				    physregs[parms[i].ip_reg]->is_name,
				    sym->is_name);
		} else
			fatalx("pass_emit_call: %s: unsupported argtype: %d",
			    fn->if_sym->is_name, x->ie_type->it_op);
	}
	free(parms);

	if (call->ic_firstvararg != -1) {
		if (hasflt)
			emits("\tcreqv\tr6, 6, 6\n");
		else
			emits("\tcrxor\t6, 6, 6\n");
	}
	if (call->ic_fn->is_op == IR_REGSYM) {
		emitf("\tmtctr\t%s\n", call->ic_fn->is_name);
		emits("\tbctrl\n");
	} else
		emitf("\tbl\t%s\n", call->ic_fn->is_name);
	if (rety == NULL)
		return;
	sym = call->ic_ret->ie_sym;
	if (IR_ISQUAD(rety) && sym->is_id != REG_R3R4) {
		shi = pairtogpr[sym->is_id - REG_R3R4][0];
		slo = pairtogpr[sym->is_id - REG_R3R4][1];
		emitf("\tmr\t%%r3, %s\n", physregs[shi]->is_name);
		emitf("\tmr\t%%r4, %s\n", physregs[slo]->is_name);
	} else if (IR_ISF64(rety) && sym->is_id != REG_F1)
		emitf("\tfmr\t%s, %%f1\n", sym->is_name);
	else if (!IR_ISF64(rety) && !IR_ISQUAD(rety) && sym->is_id != REG_R3)
		emitf("\tmr\t%s, %%r3\n", sym->is_name);
}

void
pass_emit_ret(struct ir_func *fn, struct ir_ret *ret)
{
	struct ir_symbol *sym;
	struct ir_type *rety;

	if (ret->ir_deadret)
		return;
	if (ret->ir_retexpr != NULL) {
		rety = ret->ir_retexpr->ie_type;
		sym = ret->ir_retexpr->ie_sym;
		if (IR_ISBYTE(rety) || IR_ISWORD(rety) || IR_ISLONG(rety) ||
		    IR_ISPTR(rety)) {
			if (sym->is_id != REG_R3)
				emitf("\tmr\t%%r3, %s\n", sym->is_name);
		} else if (IR_ISF64(rety)) {
			if (sym->is_id != REG_F1)
				emitf("\tfmr\t%%f1, %s\n", sym->is_name);
		} else
			fatalx("pass_emit_ret: bad return type: %d",
			    rety->it_op);
	}
	if ((struct ir_insn *)ret != TAILQ_LAST(&fn->if_iq, ir_insnq))
		emitf("\tb\t.L%d\n", fn->if_retlab);
}

void
pass_emit_prologue(struct ir_func *fn)
{
	int i, lbl, reg;
	size_t off, sub = 0;

	/* XXX: Can't use stwu if frame is large. */
	if (fn->if_framesz)
		emitf("\tstwu\t%%r1, -%zu(%%r1)\n", fn->if_framesz);
	if (!(fn->if_flags & IR_FUNC_LEAF)) {
		emitf("\tmflr\t%%r0\n");
		emitf("\tstw\t%%r0, %zu(%%r1)\n", fn->if_framesz + 4);
	}

	if (fn->if_flags & IR_FUNC_PROTSTACK) {
		emits("\tlis\t%r11, __guard@ha\n");
		emits("\tlwz\t%r11, __guard@l(%r11)\n");
		if (fn->ifm_guardoff > TARG_SIMM_MAX) {
			emitf("\tlis\t%%r12, %zu@ha\n", fn->ifm_guardoff);
			emitf("\taddi\t%%r12, %%r12, %zu@l\n",
			    fn->ifm_guardoff);
			emits("\tstwx\t%r11, %r1, %r12\n");
		} else
			emitf("\tstw\t%%r11, %zu(%%r1)\n", fn->ifm_guardoff);
	}
	if (fn->ifm_saver31) {
		if (fn->ifm_r31off > TARG_SIMM_MAX) {
			emitf("\tlis\t%%r12, %zu@ha\n", fn->ifm_r31off);
			emitf("\taddi\t%%r12, %%r12, %zu@l\n",
			    fn->ifm_r31off);
			emits("\tstwx\t%r31, %r1, %r12\n");
		} else
			emitf("\tstw\t%%r11, %zu(%%r1)\n", fn->ifm_r31off);
	}

	/*
	 * TODO: Check which nonvolatile 64-bit register pairs are in use
	 * and mark their corresponding 32-bit registers as used.
	 */
	for (i = REG_F14; i <= REG_F31; i++) {
		if (BITVEC_ISSET(&fn->if_usedregs, i)) {
			sub += 8;
			emitf("\tstfd\t%s, %zu(%%r1)\n",
			    physregs[i]->is_name, fn->if_framesz - sub);
		}
	}
	for (i = REG_R14; i <= REG_R31; i++) {
		if (BITVEC_ISSET(&fn->if_usedregs, i)) {
			sub += 4;
			emitf("\tstw\t%s, %zu(%%r1)\n",
			    physregs[i]->is_name, fn->if_framesz - sub);
		}
	}

	if ((fn->if_flags & IR_FUNC_VARARGS) && fn->ifm_vasaves != NULL) {
		off = fn->if_framesz + 8;
		if (off > TARG_SIMM_MAX) {
			emitf("\tlis\t%%r11, %zu@ha\n", off);
			emitf("\taddi\t%%r11, %%r11, %zu@l\n", off);
			emitf("\tadd\t%%r11, %%r11, %%r1\n");
		} else
			emitf("\taddi\t%%r11, %%r1, %zu\n", off);
		emitf("\tstw\t%%r11, %zd(%%r1)\n", fn->ifm_vastack->is_off);

		off = (fn->ifm_firstgpr - REG_R3) * 4 +
		    fn->ifm_vasaves->is_off;
		for (reg = fn->ifm_firstgpr; reg <= REG_R10; reg++) {
			if (off > TARG_SIMM_MAX) {
				emitf("\tlis\t%%r11, %zu@ha\n", off);
				emitf("\taddi\t%%r11, %%r11, %zu@l\n", off);
				emitf("\tstwx\t%s, %%r1, %%r11\n",
				    physregs[reg]->is_name);
			} else
				emitf("\tstw\t%s, %zu(%%r1)\n",
				    physregs[reg]->is_name, off);
			off += 4;
		}
		lbl = newid();
		emitf("\tbne\t1, .L%d\n", lbl);
		off = (fn->ifm_firstfpr - REG_F1) * 8 +
		    fn->ifm_vasaves->is_off + 32;
		for (reg = fn->ifm_firstfpr; reg <= REG_F8; reg++) {
			if (off > TARG_SIMM_MAX) {
				emitf("\tlis\t%%r11, %zu@ha\n", off);
				emitf("\taddi\t%%r11, %%r11, %zu@l\n", off);
				emitf("\tstfdx\t%s, %%r1, %%r11\n",
				    physregs[reg]->is_name);
			} else
				emitf("\tstfd\t%s, %zu(%%r1)\n",
				    physregs[reg]->is_name, off);
			off += 8;
		}
		emitf(".L%d:\n", lbl);
	}
}

void
pass_emit_epilogue(struct ir_func *fn)
{
	int i, lbl;
	size_t sub = 0;

	emitf(".L%d:\n", fn->if_retlab);
	if (fn->if_flags & IR_FUNC_PROTSTACK) {
		lbl = newid();
		if (fn->ifm_guardoff > TARG_SIMM_MAX) {
			emitf("\tlis\t%%r11, %zu@ha\n", fn->ifm_guardoff);
			emitf("\taddi\t%%r11, %%r11, %zu@l\n",
			    fn->ifm_guardoff);
			emits("\tlwzx\t%r11, %r1, %r11\n");
		} else
			emitf("\tlwz\t%%r11, %zu(%%r1)\n", fn->ifm_guardoff);
		emits("\tlis\t%r4, __guard@ha\n");
		emits("\tlwz\t%r4, __guard@l(%r4)\n");
		emits("\tcmpw\t%r11, %r4\n");
		emitf("\tbeq\t.L%d\n", lbl);
		emitf("\tlis\t%%r3, .L%d@ha\n", fn->if_sym->is_id);
		emitf("\taddi\t%%r3, %%r3, .L%d@l\n", fn->if_sym->is_id);
		emits("\tbl\t__stack_smash_handler\n");
		emitf(".L%d:\n", lbl);
	}
	if (fn->ifm_saver31) {
		if (fn->ifm_r31off > TARG_SIMM_MAX) {
			emitf("\tlis\t%%r11, %zu@ha\n", fn->ifm_r31off);
			emitf("\taddi\t%%r11, %%r11, %zu@l\n",
			    fn->ifm_r31off);
			emits("\tlwzx\t%r31, %r1, %r11\n");
		} else
			emitf("\tlwz\t%%r31, %zu(%%r1)\n", fn->ifm_r31off);
	}
	if (!(fn->if_flags & IR_FUNC_LEAF)) {
		emitf("\tlwz\t%%r0, %zu(%%r1)\n", fn->if_framesz + 4);
		emitf("\tmtlr\t%%r0\n");
	}

	for (i = REG_F14; i <= REG_F31; i++) {
		if (BITVEC_ISSET(&fn->if_usedregs, i)) {
			sub += 8;
			emitf("\tlfd\t%s, %zu(%%r1)\n",
			    physregs[i]->is_name, fn->if_framesz - sub);
		}
	}
	for (i = REG_R14; i <= REG_R31; i++) {
		if (BITVEC_ISSET(&fn->if_usedregs, i)) {
			sub += 4;
			emitf("\tlwz\t%s, %zu(%%r1)\n",
			    physregs[i]->is_name, fn->if_framesz - sub);
		}
	}

	if (fn->if_framesz) {
		if (fn->if_framesz < TARG_SIMM_MAX)
			emitf("\taddi\t%%r1, %%r1, %zu\n", fn->if_framesz);
		else
			emitf("\tlwz\t%%r1, 0(%%r1)\n");
	}
	emits("\tblr\n");
}
