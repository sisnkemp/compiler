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
 * Very crude form of alias analysis. Just mark which variables have their
 * address computed. These will not be put into registers and won't
 * participate in optimization.
 */

#include <sys/types.h>
#include <sys/queue.h>

#include "comp/comp.h"
#include "comp/ir.h"
#include "comp/passes.h"

static void
doexpr(struct ir_expr *x)
{
	for (;;) {
		if (IR_ISBINEXPR(x)) {
			doexpr(x->ie_l);
			x = x->ie_r;
		} else if (IR_ISUNEXPR(x))
			x = x->ie_l;
		else if (x->i_op == IR_GADDR || x->i_op == IR_LADDR ||
		    x->i_op == IR_PADDR) {
			ir_symbol_setflags(x->ie_sym, IR_SYM_ADDRTAKEN);
			break;
		} else
			break;
	}
}

void
pass_aliasanalysis(struct passinfo *pi)
{
	struct ir_func *fn = pi->p_fn;
	struct ir_insn *insn;
	struct ir_stasg *asg;
	struct ir_branch *b;
	struct ir_call *call;
	struct ir_ret *ret;

	TAILQ_FOREACH(insn, &fn->if_iq, ii_link) {
		switch (insn->i_op) {
		case IR_B:
		case IR_LBL:
			continue;
		case IR_ASG:
		case IR_ST:
			asg = (struct ir_stasg *)insn;
			doexpr(asg->is_l);
			doexpr(asg->is_r);
			continue;
		case IR_BEQ:
		case IR_BNE:
		case IR_BLT:
		case IR_BLE:
		case IR_BGT:
		case IR_BGE:
			b = (struct ir_branch *)insn;
			doexpr(b->ib_l);
			doexpr(b->ib_r);
			continue;
		case IR_CALL:
			call = (struct ir_call *)insn;
			if (call->ic_ret != NULL)
				doexpr(call->ic_ret);
			continue;
		case IR_RET:
			ret = (struct ir_ret *)insn;
			if (ret->ir_retexpr != NULL)
				doexpr(ret->ir_retexpr);
			continue;
		default:
			fatalx("pass_aliasanalysis: bad insn: 0x%x",
			    insn->i_op);
		}
	}
}
