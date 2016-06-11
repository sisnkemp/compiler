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
deadfuncelim_doexpr(struct ir_expr *x)
{
	for (;;) {
		if (IR_ISBINEXPR(x)) {
			deadfuncelim_doexpr(x->ie_l);
			x = x->ie_r;
		} else if (IR_ISUNEXPR(x))
			x = x->ie_l;
		else if (x->i_op != IR_ICON && x->i_op != IR_FCON) {
			ir_symbol_setflags(x->ie_sym, IR_SYM_USED);
			break;
		} else
			break;
	}
}

static void
deadfuncelim_doinit(struct ir_init *init)
{
	struct ir_initelm *elm;

	SIMPLEQ_FOREACH(elm, &init->ii_elms, ii_link) {
		if (elm->ii_op == IR_INIT_EXPR)
			deadfuncelim_doexpr(elm->ii_expr);
	}
}

void
pass_deadfuncelim(struct passinfo *pi)
{
	struct ir_expr *x;
	struct ir_func *fn, *next, *prev;
	struct ir_symbol *sym;
	struct ir_init *init;
	struct ir_insn *insn;

	SIMPLEQ_FOREACH(fn, &irprog->ip_funq, if_link)
		fn->if_sym->is_flags &= ~IR_SYM_USED;
	SIMPLEQ_FOREACH(init, &irprog->ip_roinitq, ii_link)
		deadfuncelim_doinit(init);
	SIMPLEQ_FOREACH(init, &irprog->ip_initq, ii_link)
		deadfuncelim_doinit(init);

	SIMPLEQ_FOREACH(fn, &irprog->ip_funq, if_link) {
		irfunc = fn;
		TAILQ_FOREACH(insn, &fn->if_iq, ii_link) {
			if (insn->i_op == IR_LBL || insn->i_op == IR_B)
				continue;
			if (IR_ISBRANCH(insn)) {
				deadfuncelim_doexpr(insn->ib_l);
				deadfuncelim_doexpr(insn->ib_r);
				continue;
			}
			switch (insn->i_op) {
			case IR_ASG:
			case IR_ST:
				deadfuncelim_doexpr(insn->is_l);
				deadfuncelim_doexpr(insn->is_r);
				break;
			case IR_CALL:
				if (insn->ic_ret != NULL)
					deadfuncelim_doexpr(insn->ic_ret);
				ir_symbol_setflags(insn->ic_fn, IR_SYM_USED);
				SIMPLEQ_FOREACH(x, &insn->ic_argq, ie_link)
					deadfuncelim_doexpr(x);
				break;
			case IR_RET:
				if (insn->ir_retexpr != NULL)
					deadfuncelim_doexpr(insn->ir_retexpr);
				break;
			default:
				fatalx("pass_deadfuncelim: bad op: 0x%x",
				    insn->i_op);
			}
		}
	}

	prev = NULL;
	for (fn = SIMPLEQ_FIRST(&irprog->ip_funq); fn != NULL;
	    fn = next) {
		next = SIMPLEQ_NEXT(fn, if_link);
		sym = fn->if_sym;
		if (!(sym->is_flags & (IR_SYM_USED | IR_SYM_GLOBL))) {
			if (prev == NULL)
				SIMPLEQ_REMOVE_HEAD(&irprog->ip_funq, if_link);
			else
				SIMPLEQ_REMOVE_AFTER(&irprog->ip_funq, prev,
				    if_link);
			ir_func_free(fn);
		} else
			prev = fn;
	}
}
