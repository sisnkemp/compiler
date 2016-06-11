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

/*
 * Determine which parameters live in registers and which on the stack.
 */

#include <sys/types.h>
#include <sys/queue.h>

#include <stdio.h>

#include "comp/comp.h"
#include "comp/ir.h"
#include "comp/passes.h"

static void fixup(struct ir_func *, struct regpar *);
static void fixup_expr(struct ir_expr **, struct regpar *, int);

void
pass_parmfixup(struct passinfo *pi)
{
	struct ir_func *fn = pi->p_fn;
	struct regpar rp[REG_NREGS + 1];

	pass_parmfixup_machdep(fn, rp);
	fixup(fn, rp);
}

static void
fixup(struct ir_func *fn, struct regpar *rp)
{
	int i;
	struct ir_stasg *asg;
	struct ir_branch *b;
	struct ir_call *call;
	struct ir_expr *lhs, *rhs;
	struct ir_insn *insn;
	struct ir_ret *ret;
	struct ir_symbol *sym;

	for (i = 0; rp[i].r_par != NULL; i++) {
		sym = ir_symbol(IR_VARSYM, NULL, rp[i].r_par->is_size,
		    rp[i].r_par->is_align, rp[i].r_par->is_type);
		ir_func_addvar(fn, sym);
		lhs = ir_var(IR_LVAR, sym);
		rhs = ir_physreg(physregs[rp[i].r_reg], rp[i].r_par->is_type);
		insn = ir_asg(lhs, rhs);
		ir_insnq_add_head(&fn->if_iq, insn);
		rp[i].r_par->is_op = IR_REGSYM;
		rp[i].r_par->is_id = i;
		rp[i].r_tmpvar = lhs->ie_sym;
	}

	TAILQ_FOREACH(insn, &fn->if_iq, ii_link) {
		switch (insn->i_op) {
		case IR_ASG:
		case IR_ST:
			asg = (struct ir_stasg *)insn;
			fixup_expr(&asg->is_l, rp, i);
			fixup_expr(&asg->is_r, rp, i);
			break;
		case IR_B:
			break;
		case IR_BEQ:
		case IR_BNE:
		case IR_BLT:
		case IR_BLE:
		case IR_BGT:
		case IR_BGE:
			b = (struct ir_branch *)insn;
			fixup_expr(&b->ib_l, rp, i);
			fixup_expr(&b->ib_r, rp, i);
			break;
		case IR_CALL:
			call = (struct ir_call *)insn;
			if (call->ic_ret != NULL)
				fixup_expr(&call->ic_ret, rp, i);
			break;
		case IR_RET:
			ret = (struct ir_ret *)insn;
			if (ret->ir_retexpr != NULL)
				fixup_expr(&ret->ir_retexpr, rp, i);
			break;
		case IR_LBL:
			break;
		default:
			fatalx("fixup: bad insn: %d", insn->i_op);
		}
	}
}

static void
fixup_expr(struct ir_expr **xp, struct regpar *rp, int maxrp)
{
	int i;
	struct ir_expr *tmp, *x = *xp;

	for (;;) {
		if (IR_ISBINEXPR(x)) {
			fixup_expr(&x->ie_r, rp, maxrp);
			xp = &x->ie_l;
			x = *xp;
		} else if (IR_ISUNEXPR(x)) {
			xp = &x->ie_l;
			x = *xp;
		} else if (x->i_op == IR_PVAR || x->i_op == IR_PADDR) {
			if ((i = x->ie_sym->is_id) >= maxrp)
				return;
			if (x->i_op == IR_PADDR)
				tmp = ir_addr(IR_LADDR, rp[i].r_tmpvar);
			else
				tmp = ir_var(IR_LVAR, rp[i].r_tmpvar);
			ir_expr_replace(xp, tmp);
			break;
		} else
			break;
	}

}
