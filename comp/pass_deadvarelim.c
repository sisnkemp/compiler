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

#include "comp/comp.h"
#include "comp/ir.h"
#include "comp/passes.h"

static void
dve_record(struct ir_expr *x)
{
	for (;;) {
		if (IR_ISBINEXPR(x)) {
			dve_record(x->ie_r);
			x = x->ie_l;
		} else if (IR_ISUNEXPR(x))
			x = x->ie_l;
		else if (x->i_op == IR_REG) {
			ir_symbol_setflags(x->ie_sym, IR_SYM_USED);
			break;
		} else
			break;
	}
}

void
pass_deadvarelim(struct passinfo *pi)
{
	int changes = 0;
	struct ir_expr *x;
	struct ir_func *fn = pi->p_fn;
	struct ir_insn *insn, *ninsn;
	struct ir_symbol *nsym, *sym, *prev;
	struct ir_phiarg *arg;

	for (;;) {
		changes = 0;
		prev = NULL;
		SIMPLEQ_FOREACH(sym, &fn->if_regq, is_link)
			sym->is_flags &= ~IR_SYM_USED;

		/* Mark uses. */
		TAILQ_FOREACH(insn, &fn->if_iq, ii_link) {
			if (insn->i_op == IR_LBL || insn->i_op == IR_B)
				continue;
			if (IR_ISBRANCH(insn)) {
				dve_record(insn->ib_l);
				dve_record(insn->ib_r);
				continue;
			}
			switch (insn->i_op) {
			case IR_ST:
				dve_record(insn->is_l);
			case IR_ASG:
				dve_record(insn->is_r);
				break;
			case IR_CALL:
				SIMPLEQ_FOREACH(x, &insn->ic_argq, ie_link)
					dve_record(x);
				if (insn->ic_fn->is_op == IR_REGSYM)
					ir_symbol_setflags(insn->ic_fn,
					    IR_SYM_USED);
				break;
			case IR_RET:
				if (insn->ir_retexpr != NULL)
					dve_record(insn->ir_retexpr);
				break;
			case IR_PHI:
				SIMPLEQ_FOREACH(arg, &insn->ip_args, ip_link)
					ir_symbol_setflags(arg->ip_arg,
					    IR_SYM_USED);
				break;
			default:
				fatalx("pass_deadvarelim: bad op: 0x%x",
				    insn->i_op);
			}
		}

		/* Remove instructions that compute unused values. */
		for (insn = TAILQ_FIRST(&fn->if_iq); insn != NULL;
		    insn = ninsn) {
			ninsn = TAILQ_NEXT(insn, ii_link);
			if (insn->i_op == IR_PHI &&
			    !(insn->ip_sym->is_flags & IR_SYM_USED)) {
				changes = 1;
				cfa_bb_delinsn(fn, insn->ii_bb, insn);
				continue;
			}
			if (insn->i_op == IR_CALL && insn->ic_ret != NULL &&
			    insn->ic_ret->i_op == IR_REG &&
			    !(insn->ic_ret->ie_sym->is_flags & IR_SYM_USED)) {
				ir_expr_free(insn->ic_ret);
				insn->ic_ret = NULL;
				changes = 1;
				continue;
			}
			if (insn->i_op != IR_ASG || insn->is_l->i_op != IR_REG)
				continue;
			if (insn->is_l->ie_sym->is_id < REG_NREGS)
				continue;
			if (insn->is_l->ie_sym->is_flags & IR_SYM_USED)
				continue;
			changes = 1;
			ir_delete_insn(fn, insn);
		}

		if (!changes)
			break;
	}

	/* Remove unused variables. */
	for (prev = NULL, sym = SIMPLEQ_FIRST(&fn->if_regq); sym != NULL;
	    sym = nsym) {
		nsym = SIMPLEQ_NEXT(sym, is_link);
		if (!(sym->is_flags & IR_SYM_USED)) {
			if (prev == NULL)
				SIMPLEQ_REMOVE_HEAD(&fn->if_regq, is_link);
			else
				SIMPLEQ_REMOVE_AFTER(&fn->if_regq, prev,
				    is_link);
			ir_symbol_free(sym);
		} else
			prev = sym;
	}
}
