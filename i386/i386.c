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
#include "comp/passes.h"

struct ir_symbol *physregs[TARG_MAXREG + 1];
struct regdef targregs[TARG_MAXREG];
static struct regvec r32regs = BITVEC_INITIALIZER(TARG_MAXREG);
struct regvec div32dst = BITVEC_INITIALIZER(TARG_MAXREG);
struct regvec div32l = BITVEC_INITIALIZER(TARG_MAXREG);
struct regvec div32r = BITVEC_INITIALIZER(TARG_MAXREG);
struct regvec calleesaveregs = BITVEC_INITIALIZER(TARG_MAXREG);
static struct regvec r64regs = BITVEC_INITIALIZER(TARG_MAXREG);
static struct regvec f64regs = BITVEC_INITIALIZER(TARG_MAXREG);

static size_t emit_push(struct ir_expr *, size_t);

void
targinit(void)
{
	int i, j, k;

	/*
	 * XXX: This code should be generated (semi-)automatically by
	 * cgg.
	 */

	/*
	 * %ebx and %ecx can be used as a scratch register as long as the
	 * simple register allocator is used.
	 */

	static char *regnames[] = {
		"%eax", "%ebx", "%ecx", "%edx",
		"%esi", "%edi", "%ebp", "%esp",
		"%ax", "%bx", "%cx", "%dx",
		"%si", "%di", "%bp", "%sp",
		"%al", "%ah", "%bl", "%bh",
		"%cl", "%ch", "%dl", "%dh",
		"%st(0)", "%st(1)", "%st(2)", "%st(3)",
		"%st(4)", "%st(5)", "%st(6)", "%st(7)",
		"%eaxebx", "%esiedi"
	};

	BITVEC_SETBIT(&r32regs, TARG_EAX);
/*	BITVEC_SETBIT(&r32regs, TARG_ECX); */
	BITVEC_SETBIT(&r32regs, TARG_EDX);

	BITVEC_SETBIT(&r64regs, TARG_EAXEBX);
	BITVEC_SETBIT(&r64regs, TARG_ESIEDI);

	BITVEC_SETBIT(&div32dst, TARG_EAX);
	BITVEC_SETBIT(&div32l, TARG_EAX);
/*	BITVEC_SETBIT(&div32r, TARG_ECX); */
	BITVEC_SETBIT(&div32r, TARG_EDX);

	for (i = TARG_FP0; i <= TARG_FP1; i++)
		BITVEC_SETBIT(&f64regs, i);

	BITVEC_SETALL(&freeregs);
	BITVEC_CLEARBIT(&freeregs, TARG_EBP);
	BITVEC_CLEARBIT(&freeregs, TARG_ESP);
	BITVEC_CLEARBIT(&freeregs, TARG_BP);
	BITVEC_CLEARBIT(&freeregs, TARG_SP);

	for (i = 0; i < TARG_MAXREG; i++) {
		physregs[i] = ir_physregsym(regnames[i], i);
		targregs[i].r_reg = physregs[i];
		BITVEC_INIT(&targregs[i].r_overlap, TARG_MAXREG);
	}
	physregs[TARG_MAXREG] = NULL;

	targregs[TARG_EBX].r_flags = REGDEF_CALLERSAVE;
	targregs[TARG_BX].r_flags = REGDEF_CALLERSAVE;
	targregs[TARG_BL].r_flags = REGDEF_CALLERSAVE;
	targregs[TARG_BH].r_flags = REGDEF_CALLERSAVE;
	targregs[TARG_ESI].r_flags = REGDEF_CALLERSAVE;
	targregs[TARG_SI].r_flags = REGDEF_CALLERSAVE;
	targregs[TARG_EDI].r_flags = REGDEF_CALLERSAVE;
	targregs[TARG_DI].r_flags = REGDEF_CALLERSAVE;
	targregs[TARG_EBP].r_flags = REGDEF_CALLERSAVE;
	targregs[TARG_BP].r_flags = REGDEF_CALLERSAVE;
	targregs[TARG_ESP].r_flags = REGDEF_CALLERSAVE;
	targregs[TARG_SP].r_flags = REGDEF_CALLERSAVE;

	for (i = TARG_EAX, j = TARG_AX, k = TARG_AL;
	    i <= TARG_EDX; i++, j++, k += 2) {
		BITVEC_SETBIT(&targregs[i].r_overlap, j);
		BITVEC_SETBIT(&targregs[i].r_overlap, k);
		BITVEC_SETBIT(&targregs[i].r_overlap, k + 1);

		BITVEC_SETBIT(&targregs[j].r_overlap, i);
		BITVEC_SETBIT(&targregs[j].r_overlap, k);
		BITVEC_SETBIT(&targregs[j].r_overlap, k + 1);

		BITVEC_SETBIT(&targregs[k].r_overlap, i);
		BITVEC_SETBIT(&targregs[k].r_overlap, j);
		BITVEC_SETBIT(&targregs[k + 1].r_overlap, i);
		BITVEC_SETBIT(&targregs[k + 1].r_overlap, j);
	}

	BITVEC_SETBIT(&targregs[TARG_ESI].r_overlap, TARG_SI);
	BITVEC_SETBIT(&targregs[TARG_SI].r_overlap, TARG_ESI);

	BITVEC_SETBIT(&targregs[TARG_EDI].r_overlap, TARG_DI);
	BITVEC_SETBIT(&targregs[TARG_DI].r_overlap, TARG_EDI);
}

char *
ir_emit_machdep(struct ir *ir, char *str)
{
	int id;
	struct ir_stasg *asg;

	if (str[1] == 'L' && str[2] == 'D') {
		asg = (struct ir_stasg *)ir;
		id = asg->is_r->ie_sym->is_id;
		emitf("\tfldl\t.L%d\n", id);
		return &str[2];
	}

	return str;
}

void
ir_emit_regpair(struct ir_symbol *sym, int sec)
{
	if (sym->is_id == TARG_EAXEBX) {
		if (sec)
			emitf("%%ebx");
		else
			emitf("%%eax");
	} else if (sym->is_id == TARG_ESIEDI) {
		if (sec)
			emitf("%%edi");
		else
			emitf("%%esi");
	} else
		fatalx("ir_emit_regpair");
}

struct regvec *
ir_type_regvec(struct ir_type *type)
{
	switch (type->it_op) {
	case IR_I8:
	case IR_U8:
	case IR_I16:
	case IR_U16:
	case IR_I32:
	case IR_U32:
	case IR_PTR:
		return &r32regs;
	case IR_I64:
	case IR_U64:
		return &r64regs;
	case IR_F64:
		return &f64regs;
	default:
		fatalx("ir_type_regvec: bad type");
	}
}

void
pass_parmfixup_machdep(struct ir_func *fn, struct regpar *rp)
{
	rp->r_par = NULL;
}

void
pass_stackoff_machdep(struct ir_func *fn)
{
	ssize_t off;

	/*
	 * XXX: Start offset and initial space for nonvolatile registers
	 * is hardcoded. Has to go some day.
	 */
	fn->if_framesz = 12;
	off = pass_stackoff_var(fn, -12, STACKOFF_NEGOFF);
	pass_stackoff_reg(fn, off, STACKOFF_NEGOFF);
	pass_stackoff_par(fn, 8, 0);
}

ssize_t
pass_stackoff_paralign(struct ir_symbol *sym, ssize_t off)
{
	if (IR_ISLONG(sym->is_type) || IR_ISPTR(sym->is_type) ||
	    IR_ISQUAD(sym->is_type))
		return (off + 3) & ~3;
	fatalx("pass_stackoff_paralign: bad type: %d", sym->is_type->it_op);
	return 0;
}

void
pass_ralloc_interfere_callargs(struct ir_func *fn, struct ir_insn *insn)
{
}

int
pass_ralloc_retreg(struct ir_type *type)
{
	fatalx("i386: pass_ralloc_retreg");
}

void
pass_emit_call(struct ir_func *fn, struct ir_call *call)
{
	size_t pushed;
	struct ir_type *rety = NULL;

	if (call->ic_ret != NULL) {
		rety = call->ic_ret->ie_type;
		if (!IR_ISLONG(rety) && !IR_ISPTR(rety) && !IR_ISF64(rety))
			fatalx("pass_emit_call: unsupported return type: %d",
			    rety->it_op);
	}

	pushed = emit_push(SIMPLEQ_FIRST(&call->ic_argq), 0);

	if (call->ic_fn->i_op == IR_REGSYM) {
		emitf("\tmovl\t%zd(%%ebp), %%eax\n", call->ic_fn->is_off);
		emitf("\tcall\t*%%eax\n");
	} else
		emitf("\tcall\t%s\n", call->ic_fn->is_name);
	emitf("\taddl\t$%zu, %%esp\n", pushed);

	if (rety == NULL)
		return;
	if (IR_ISLONG(rety) || IR_ISPTR(rety))
		emitf("\tmovl\t%%eax, %zd(%%ebp)\n",
		    call->ic_ret->ie_sym->is_off);
	else if (IR_ISF64(rety))
		emitf("\tfstpl\t%zd(%%ebp)\n",
		    call->ic_ret->ie_sym->is_off);
	else
		fatalx("pass_emit_call: unsupported return type: %d",
		    rety->it_op);
}

static size_t
emit_push(struct ir_expr *x, size_t pushed)
{
	if (x == NULL)
		return pushed;
	pushed = emit_push(SIMPLEQ_NEXT(x, ie_link), pushed);
	if (IR_ISLONG(x->ie_type) || IR_ISPTR(x->ie_type))
		emitf("\tpushl\t%zd(%%ebp)\n", x->ie_sym->is_off);
	else if (IR_ISF64(x->ie_type)) {
		emitf("\tpushl\t%zd(%%ebp)\n", x->ie_sym->is_off + 4);
		pushed += 8;
	} else
		fatalx("emit_push: bad type: %d", x->ie_type->it_op);
	return pushed + 4;
}

void
pass_emit_ret(struct ir_func *fn, struct ir_ret *ret)
{
	struct ir_type *rety;

	if (ret->ir_expr != NULL) {
		rety = ret->ir_expr->ie_type;
		if (IR_ISLONG(rety))
			emitf("\tmovl\t%zd(%%ebp), %%eax\n",
			    ret->ir_expr->ie_sym->is_off);
		else if (IR_ISF64(rety))
			emitf("\tfldl\t%zd(%%ebp)\n",
			    ret->ir_expr->ie_sym->is_off);
		else
			fatalx("pass_emit_ret: bad return type: %d",
			    rety->it_op);
	}
	emitf("\tjmp\t.L%d\n", fn->if_retlab);
}

void
pass_emit_prologue(struct ir_func *fn)
{
	emitf("\tpushl\t%%ebp\n");
	emitf("\tmovl\t%%esp, %%ebp\n");
	emitf("\tsubl\t$%zu, %%esp\n", fn->if_framesz);

	/* XXX: Don't hardcode this. */
	emitf("\tmovl\t%%ebx, -4(%%ebp)\n");
	emitf("\tmovl\t%%esi, -8(%%ebp)\n");
	emitf("\tmovl\t%%edi, -12(%%ebp)\n");
}

void
pass_emit_epilogue(struct ir_func *fn)
{
	emitf(".L%d:\n", fn->if_retlab);

	/* XXX: Don't hardcode this. */
	emitf("\tmovl\t-4(%%ebp), %%ebx\n");
	emitf("\tmovl\t-8(%%ebp), %%esi\n");
	emitf("\tmovl\t-12(%%ebp), %%edi\n");

	emitf("\tleave\n");
	emitf("\tret\n");
}
