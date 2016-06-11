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
 * Put local variables that do not have their address computed and which are
 * not volatile into virtual registers.
 */

#include <sys/types.h>
#include <sys/queue.h>

#include <err.h>

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
		else if (x->i_op == IR_LVAR && x->ie_sym->is_op == IR_REGSYM) {
			x->i_op = IR_REG;
			break;
		} else
			break;
	}
}

void
pass_vartoreg(struct passinfo *pi)
{
	struct ir_func *fn = pi->p_fn;
	struct ir_symbol *nsym, *psym, *sym;
	struct ir_insn *insn;
	struct ir_stasg *asg;
	struct ir_branch *b;
	struct ir_call *call;
	struct ir_ret *ret;

	pass_aliasanalysis(pi);
	psym = NULL;
	for (sym = SIMPLEQ_FIRST(&fn->if_varq); sym != NULL; sym = nsym) {
		nsym = SIMPLEQ_NEXT(sym, is_link);
		if (sym->is_op != IR_VARSYM) {
			warnx("pass_vartoreg in %s: symbol %d is in varq but "
			    "is not an IR_VARSYM",
			    fn->if_sym->is_name, sym->is_id);
			sym->is_op = IR_VARSYM;
		}
		if (sym->is_flags & (IR_SYM_ADDRTAKEN | IR_SYM_VOLAT) ||
		    !IR_ISSCALAR(sym->is_type)) {
			psym = sym;
			continue;
		}
		if (psym == NULL)
			SIMPLEQ_REMOVE_HEAD(&fn->if_varq, is_link);
		else
			SIMPLEQ_REMOVE_AFTER(&fn->if_varq, psym, is_link);
		sym->is_op = IR_REGSYM;
		SIMPLEQ_INSERT_TAIL(&fn->if_regq, sym, is_link);
		sym->is_id = fn->if_regid++;
	}

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
			fatalx("pass_vartoreg: bad op: 0x%x", insn->i_op);
		}
	}
}
