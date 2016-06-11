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
#include "comp/passes.h"

static int nonvol[] = { REG_RBX, REG_R12, REG_R13, REG_R14, REG_R15 };

#define GPRMAP_B	0
#define GPRMAP_W	1
#define GPRMAP_L	2
#define GPRMAP_Q	3

static int8_t gprmap[][4] = {
	{ REG_AL, REG_AX, REG_EAX, REG_RAX },		/* REG_AL */
	{ REG_AH, REG_AX, REG_EAX, REG_RAX },		/* REG_AH */
	{ REG_BL, REG_BX, REG_EBX, REG_RBX },		/* REG_BL */
	{ REG_BH, REG_BX, REG_EBX, REG_RBX },		/* REG_BH */
	{ REG_CL, REG_CX, REG_ECX, REG_EDX },		/* REG_CL */
	{ REG_CH, REG_CX, REG_ECX, REG_RCX },		/* REG_CH */
	{ REG_DL, REG_DX, REG_EDX, REG_RDX },		/* REG_DL */
	{ REG_DH, REG_DX, REG_EDX, REG_RDX },		/* REG_DH */
	{ REG_SIL, REG_SI, REG_ESI, REG_RSI },		/* REG_SIL */
	{ REG_DIL, REG_DI, REG_EDI, REG_RDI },		/* REG_DIL */
	{ REG_R8B, REG_R8W, REG_R8D, REG_R8 },		/* REG_R8B */
	{ REG_R9B, REG_R9W, REG_R9D, REG_R9 },		/* REG_R9B */
	{ REG_R10B, REG_R10W, REG_R10D, REG_R10 },	/* REG_R10B */
	{ REG_R11B, REG_R11W, REG_R11D, REG_R11 },	/* REG_R11B */
	{ REG_R12B, REG_R12W, REG_R12D, REG_R12 },	/* REG_R12B */
	{ REG_R13B, REG_R13W, REG_R13D, REG_R13 },	/* REG_R13B */
	{ REG_R14B, REG_R14W, REG_R14D, REG_R14 },	/* REG_R14B */
	{ REG_R15B, REG_R15W, REG_R15D, REG_R15 },	/* REG_R15B */

	{ REG_AL, REG_AX, REG_EAX, REG_RAX },		/* REG_AX */
	{ REG_BL, REG_BX, REG_EBX, REG_RBX },		/* REG_BX */
	{ REG_CL, REG_CX, REG_ECX, REG_RCX },		/* REG_CX */
	{ REG_DL, REG_DX, REG_EDX, REG_RDX },		/* REG_DX */
	{ REG_SIL, REG_SI, REG_ESI, REG_RSI },		/* REG_SI */
	{ REG_DIL, REG_DI, REG_EDI, REG_RDI },		/* REG_DI */
	{ REG_R8B, REG_R8W, REG_R8D, REG_R8 },		/* REG_R8W */
	{ REG_R9B, REG_R9W, REG_R9D, REG_R9 },		/* REG_R9W */
	{ REG_R10B, REG_R10W, REG_R10D, REG_R10 },	/* REG_R10W */
	{ REG_R11B, REG_R11W, REG_R11D, REG_R11 },	/* REG_R11W */
	{ REG_R12B, REG_R12W, REG_R12D, REG_R12 },	/* REG_R12W */
	{ REG_R13B, REG_R13W, REG_R13D, REG_R13 },	/* REG_R13W */
	{ REG_R14B, REG_R14W, REG_R14D, REG_R14 },	/* REG_R14W */
	{ REG_R15B, REG_R15W, REG_R15D, REG_R15 },	/* REG_R15W */

	{ REG_AL, REG_AX, REG_EAX, REG_RAX },		/* REG_EAX */
	{ REG_BL, REG_BX, REG_EBX, REG_RBX },		/* REG_EBX */
	{ REG_CL, REG_CX, REG_ECX, REG_RCX },		/* REG_ECX */
	{ REG_DL, REG_DX, REG_EDX, REG_RDX },		/* REG_EDX */
	{ REG_SIL, REG_SI, REG_ESI, REG_RSI },		/* REG_ESI */
	{ REG_DIL, REG_DI, REG_RDI, REG_RDI },		/* REG_EDI */
	{ REG_R8B, REG_R8W, REG_R8D, REG_R8 },		/* REG_R8D */
	{ REG_R9B, REG_R9W, REG_R9D, REG_R9 },		/* REG_R9D */
	{ REG_R10B, REG_R10W, REG_R10D, REG_R10 },	/* REG_R10D */
	{ REG_R11B, REG_R11W, REG_R11D, REG_R11 },	/* REG_R11D */
	{ REG_R12B, REG_R12W, REG_R12D, REG_R12 },	/* REG_R12D */
	{ REG_R13B, REG_R13W, REG_R13D, REG_R13 },	/* REG_R13D */
	{ REG_R14B, REG_R14W, REG_R14D, REG_R14 },	/* REG_R14D */
	{ REG_R15B, REG_R15W, REG_R15D, REG_R15 },	/* REG_R15D */

	{ REG_AL, REG_AX, REG_EAX, REG_RAX },		/* REG_RAX */
	{ REG_BL, REG_BX, REG_EBX, REG_RBX },		/* REG_RBX */
	{ REG_CL, REG_CX, REG_ECX, REG_RCX },		/* REG_RCX */
	{ REG_DL, REG_DX, REG_EDX, REG_RDX },		/* REG_RDX */
	{ REG_SIL, REG_SI, REG_ESI, REG_RSI },		/* REG_RSI */
	{ REG_DIL, REG_DI, REG_EDI, REG_RSI },		/* REG_RDI */
	{ REG_R8B, REG_R8W, REG_R8D, REG_R8 },		/* REG_R8 */
	{ REG_R9B, REG_R9W, REG_R9D, REG_R9 },		/* REG_R9 */
	{ REG_R10B, REG_R10W, REG_R10D, REG_R10 },	/* REG_R10 */
	{ REG_R11B, REG_R11W, REG_R11D, REG_R11 },	/* REG_R11 */
	{ REG_R12B, REG_R12W, REG_R12D, REG_R12 },	/* REG_R12 */
	{ REG_R13B, REG_R13W, REG_R13D, REG_R13 },	/* REG_R13 */
	{ REG_R14B, REG_R14W, REG_R14D, REG_R14 },	/* REG_R14 */
	{ REG_R15B, REG_R15W, REG_R15D, REG_R15 }	/* REG_R15 */
};

void
targinit(void)
{
}

int
ir_cast_canskip(struct ir_expr *x, struct ir_type *toty)
{
	if (IR_ISLONG(x->ie_type) && IR_ISLONG(toty))
		return 1;
	if (IR_ISQUAD(x->ie_type) && IR_ISQUAD(toty))
		return 1;
	if (IR_ISQUAD(x->ie_type) && IR_ISPTR(toty))
		return 1;
	if (IR_ISPTR(x->ie_type) && IR_ISQUAD(toty))
		return 1;
	return 0;
}

char *
ir_emit_machdep(struct ir *ir, char *str)
{
	int id, idx;
	struct ir_expr *x;
	struct ir_stasg *asg;

	if (str[1] == 'L' && str[2] == 'D') {
		asg = (struct ir_stasg *)ir;
		id = asg->is_r->ie_sym->is_id;
		emitf("\tmovsd\t.L%d, %s\n", id, asg->is_l->ie_sym->is_name);
		return &str[2];
	} else if (str[1] == 'R') {
		x = (struct ir_expr *)ir;
		switch (str[2]) {
		case 'B':
			idx = GPRMAP_B;
			break;
		case 'W':
			idx = GPRMAP_W;
			break;
		case 'L':
			idx = GPRMAP_L;
			break;
		case 'Q':
			idx = GPRMAP_Q;
			break;
		default:
			fatalx("ir_emit_machdep: bad operand for ZR: %c",
			    str[2]);
		}
		emits(physregs[gprmap[x->ie_sym->is_id][idx]]->is_name);
		return &str[2];
	}

	return str;
}

void
ir_emit_regpair(struct ir_symbol *sym, int sec)
{
	fatalx("ir_emit_regpair");
}

void
ir_func_machdep(struct ir_func *fn)
{
}

int
ir_symbol_rclass(struct ir_symbol *sym)
{
	if (sym->is_id >= REG_AL && sym->is_id <= REG_R15B)
		return REG_R8REGS;
	if (sym->is_id >= REG_AX && sym->is_id <= REG_R15W)
		return REG_R16REGS;
	if (sym->is_id >= REG_EAX && sym->is_id <= REG_R15D)
		return REG_R32REGS;
	if (sym->is_id >= REG_RAX && sym->is_id <= REG_R15)
		return REG_R64REGS;
	if (sym->is_id >= REG_XMM0 && sym->is_id <= REG_XMM15)
		return REG_F64REGS;
	switch (sym->is_type->it_op) {
	case IR_I8:
	case IR_U8:
		return REG_R8REGS;
	case IR_I16:
	case IR_U16:
		return REG_R16REGS;
	case IR_I32:
	case IR_U32:
		return REG_R32REGS;
	case IR_I64:
	case IR_U64:
	case IR_PTR:
		return REG_R64REGS;
	case IR_F64:
		return REG_F64REGS;
	default:
		fatalx("ir_symbol_rclass: bad type: %d", sym->is_type->it_op);
	}
}

void
ir_parlocs(struct ir_param *parms, int nparms)
{
	int fprarg, gprarg, i;

	static struct {
		int	r8;
		int	r16;
		int	r32;
		int	r64;
	} gprs[] = {
		{ -1, REG_DI, REG_EDI, REG_RDI },
		{ -1, REG_SI, REG_ESI, REG_RSI },
		{ REG_DL, REG_DX, REG_EDX, REG_RDX },
		{ REG_CL, REG_CX, REG_ECX, REG_RCX },
		{ REG_R8B, REG_R8W, REG_R8D, REG_R8 },
		{ REG_R9B, REG_R9W, REG_R9D, REG_R9 }
	};

	static const int ngprs = sizeof gprs / sizeof gprs[0];

	for (i = gprarg = 0, fprarg = REG_XMM0; i < nparms; i++) {
		parms[i].ip_reg = -1;
		if (IR_ISLONG(parms[i].ip_type)) {
			if (gprarg < ngprs)
				parms[i].ip_reg = gprs[gprarg++].r32;
		} else if (IR_ISQUAD(parms[i].ip_type) ||
		    IR_ISPTR(parms[i].ip_type)) {
			if (gprarg < ngprs)
				parms[i].ip_reg = gprs[gprarg++].r64;
		} else if (IR_ISF64(parms[i].ip_type)) {
			if (fprarg < REG_XMM8)
				parms[i].ip_reg = fprarg++;
		} else
			fatalx("parlocs: bad type op: %d",
			    parms[i].ip_type->it_op);
	}
}

void
pass_parmfixup_machdep(struct ir_func *fn, struct regpar *rp)
{
	int i, j, nparms;
	struct ir_symbol *sym;
	struct ir_param *parms;

	nparms = 0;
	SIMPLEQ_FOREACH(sym, &fn->if_parq, is_link)
		nparms++;

	parms = xcalloc(nparms, sizeof *parms);
	i = 0;
	SIMPLEQ_FOREACH(sym, &fn->if_parq, is_link) {
		parms[i].ip_type = sym->is_type;
		parms[i].ip_argsym = sym;
		i++;
	}

	ir_parlocs(parms, nparms);
	for (i = j = 0; i < nparms; i++) {
		if (parms[i].ip_reg == -1)
			continue;
		rp[j].r_par = parms[i].ip_argsym;
		rp[j].r_reg = parms[i].ip_reg;
		j++;
	}

	rp[j].r_par = NULL;
	free(parms);
}

void
pass_stackoff_machdep(struct ir_func *fn)
{
	int i, j, reg;
	ssize_t off = 0;

	fn->if_framesz = 0;
	for (i = 0; i < sizeof nonvol / sizeof nonvol[0]; i++) {
		for (j = 0; j < 4; j++) {
			reg = gprmap[nonvol[i]][j];
			if (BITVEC_ISSET(&fn->if_usedregs, reg)) {
				fn->if_framesz += 8;
				off -= 8;
				break;
			}
		}
	}

	fn->if_framesz = (fn->if_framesz + 15) & ~15;
	off = pass_stackoff_var(fn, off, STACKOFF_NEGOFF);
	pass_stackoff_reg(fn, off, STACKOFF_NEGOFF);
	fn->if_framesz = (fn->if_framesz + 15) & ~15;
	pass_stackoff_par(fn, 16, 0);
}

ssize_t
pass_stackoff_paralign(struct ir_symbol *sym, ssize_t off)
{
	if (!IR_ISLONG(sym->is_type) && !IR_ISF64(sym->is_type))
		fatalx("pass_stackoff_paralign: bad type: %d",
		    sym->is_type->it_op);
	return (off + 7) & ~7;
}

void
pass_emit_call(struct ir_func *fn, struct ir_call *call)
{
	int i, nsse;
	size_t pushed;
	struct ir_type *rety = NULL;
	struct ir_param *args;
	struct ir_symbol *sym;

	if (call->ic_ret != NULL) {
		rety = call->ic_ret->ie_type;
		if (!IR_ISLONG(rety) && !IR_ISQUAD(rety) && !IR_ISPTR(rety) &&
		    !IR_ISF64(rety))
			fatalx("pass_emit_call: unsupported return type: %d",
			    rety->it_op);
	}

	args = ir_parlocs_call((struct ir_insn *)call);
	for (i = 0; args[i].ip_type != NULL; i++)
			continue;
	for (pushed = nsse = 0; i > 0; i--) {
		if (args[i - 1].ip_reg != -1) {
			if (IR_ISF64(args[i - 1].ip_type))
				nsse++;
			continue;
		}

		pushed += 8;
		if (IR_ISI32(args[i - 1].ip_type)) {
			emitf("\tmovl\t%zd(%%rbp), %%eax\n",
			    args[i - 1].ip_argx->ie_sym->is_off);
			emitf("\tpushq\t%%rax\n");
		} else if (IR_ISF64(args[i - 1].ip_type)) {
			emitf("\tmovq\t%zd(%%rbp), %%rax\n",
			    args[i - 1].ip_argx->ie_sym->is_off);
			emitf("\tpushq\t%%rax\n");
		} else
			fatalx("pass_emit_call: bad argtype: %d",
			    args[i - 1].ip_type->it_op);
	}
	free(args);

	if (call->ic_fn->is_op == IR_REGSYM)
		fatalx("indirect calls not yet");

	if (call->ic_firstvararg != -1) {
		if (nsse == 0)
			printf("\txorl\t%%eax, %%eax\n");
		else
			printf("\tmovb\t$%d, %%al\n", nsse);
	}

	emitf("\tcall\t%s\n", call->ic_fn->is_name);
	if (pushed != 0)
		emitf("\taddq\t$%zu, %%rsp\n", pushed);
	if (rety == NULL)
		return;
	sym = call->ic_ret->ie_sym;
	if (IR_ISBYTE(rety)) {
		if (sym->is_id != REG_AL)
			emitf("\tmovb\t%s, %%al\n", sym->is_name);
	} else if (IR_ISWORD(rety)) {
		if (sym->is_id != REG_AX)
			emitf("\tmovw\t%s, %%ax\n", sym->is_name);
	} else if (IR_ISLONG(rety)) {
		if (sym->is_id != REG_EAX)
			emitf("\tmovl\t%s, %%eax\n", sym->is_name);
	} else if (IR_ISQUAD(rety) || IR_ISPTR(rety)) {
		if (sym->is_id != REG_RAX)
			emitf("\tmovq\t%s, %%rax\n", sym->is_name);
	} else if (IR_ISF64(rety)) {
		if (sym->is_id != REG_XMM0)
			emitf("\tmovsd\t%s, %%xmm0\n", sym->is_name);
	} else
		fatalx("pass_emit_call: can't handle return type %d",
		    rety->it_op);
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
		rc = ir_symbol_rclass(parms[i].ip_argsym);
		for (j = 0; j < REG_NREGS; j++) {
			if (j == REG_RBP || parms[i].ip_reg == j)
				continue;
			if (rc != rclasses[j])
				continue;
			pass_ralloc_addedge(fn, j, parms[i].ip_argsym->is_id);
		}
	}
	free(parms);
}

int
pass_ralloc_retreg(struct ir_type *type)
{
	switch (type->it_op) {
	case IR_I8:
	case IR_U8:
		return REG_AL;
	case IR_I16:
	case IR_U16:
		return REG_AX;
	case IR_I32:
	case IR_U32:
		return REG_EAX;
	case IR_I64:
	case IR_U64:
	case IR_PTR:
		return REG_RAX;
	case IR_F64:
		return REG_XMM0;
	default:
		fatalx("amd64: pass_ralloc_retreg: bad type: %d", type->it_op);
	}
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
		if (IR_ISBYTE(rety)) {
			if (sym->is_id != REG_AL)
				emitf("\tmovb\t%s, %%al\n", sym->is_name);
		} else if (IR_ISWORD(rety)) {
			if (sym->is_id != REG_AX)
				emitf("\tmovw\t%s, %%ax\n", sym->is_name);
		} else if (IR_ISLONG(rety)) {
			if (sym->is_id != REG_EAX)
				emitf("\tmovl\t%s, %%eax\n", sym->is_name);
		} else if (IR_ISQUAD(rety) || IR_ISPTR(rety)) {
			if (sym->is_id != REG_RAX)
				emitf("\tmovq\t%s, %%rax\n", sym->is_name);
		} else if (IR_ISF64(rety)) {
			if (sym->is_id != REG_XMM0)
				emitf("\tmovsd\t%s, %%xmm0\n", sym->is_name);
		} else
			fatalx("pass_emit_ret: bad return type: %d",
			    rety->it_op);
	}
	if ((struct ir_insn *)ret != TAILQ_LAST(&fn->if_iq, ir_insnq))
		emitf("\tjmp\t.L%d\n", fn->if_retlab);
}

void
pass_emit_prologue(struct ir_func *fn)
{
	int i, j, nvreg, reg;
	ssize_t off;

	emitf("\tpushq\t%%rbp\n");
	emitf("\tmovq\t%%rsp, %%rbp\n");
	if (fn->if_framesz != 0)
		emitf("\tsubq\t$%zu, %%rsp\n", fn->if_framesz);
	for (i = 0, off = -8; i < sizeof nonvol / sizeof nonvol[0]; i++) {
		nvreg = nonvol[i];
		for (j = 0; j < 4; j++) {
			reg = gprmap[nonvol[i]][j];
			if (BITVEC_ISSET(&fn->if_usedregs, reg)) {
				emitf("\tmovq\t%s, %zd(%%rbp)\n",
				    physregs[nvreg]->is_name, off);
				off -= 8;
				break;
			}
		}
	}
}

void
pass_emit_epilogue(struct ir_func *fn)
{
	int i, j, nvreg, reg;
	ssize_t off;

	emitf(".L%d:\n", fn->if_retlab);
	for (i = 0, off = -8; i < sizeof nonvol / sizeof nonvol[0]; i++) {
		nvreg = nonvol[i];
		for (j = 0; j < 4; j++) {
			reg = gprmap[nonvol[i]][j];
			if (BITVEC_ISSET(&fn->if_usedregs, reg)) {
				emitf("\tmovq\t%zd(%%rbp), %s\n",
				    off, physregs[nvreg]->is_name);
				off -= 8;
				break;
			}
		}
	}
	emitf("\tleave\n");
	emitf("\tret\n");
}
