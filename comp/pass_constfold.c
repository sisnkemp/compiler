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
 * This pass is intended to not do constant folding only but also perform
 * strength reduction and reassociation later.
 */

#include <sys/types.h>
#include <sys/queue.h>

#include "comp/comp.h"
#include "comp/ir.h"
#include "comp/passes.h"

static struct ir_expr *
constfold_expr(struct ir_expr *x)
{
	struct ir_expr *l, *r;

	if (!IR_ISINTEGER(x->ie_type) && !IR_ISPTR(x->ie_type))
		return x;
	if (IR_ISBINEXPR(x)) {
		x->ie_l = constfold_expr(x->ie_l);
		x->ie_r = constfold_expr(x->ie_r);
	} else if (IR_ISUNEXPR(x))
		x->ie_l = constfold_expr(x->ie_l);
	else
		return x;

	/*
	 * The transformations done here are based on the tree patterns
	 * in Steven S. Muchnick: Advanced Compiler Design & Implementation,
	 * section 12.3.1.
	 */
	switch (x->i_op) {
	case IR_MUL:
		l = x->ie_l;
		r = x->ie_r;

		if (l->i_op == IR_ICON && r->i_op == IR_ICON) {	/* R3 */
			if (IR_ISSIGNED(x->ie_type))
				x->ie_con.ic_icon = l->ie_con.ic_icon *
				    r->ie_con.ic_icon;
			else
				x->ie_con.ic_ucon = l->ie_con.ic_ucon *
				    r->ie_con.ic_ucon;
			ir_con_cast(&x->ie_con, x->ie_type, x->ie_type);
			x->i_op = IR_ICON;
			ir_expr_free(l);
			ir_expr_free(r);
			return x;
		}
		if (l->i_op == IR_ICON) {
			if (IR_ISSIGNED(l->ie_type)) {
				if (l->ie_con.ic_icon == 0)
					x->ie_con.ic_icon = 0;
				else if (l->ie_con.ic_icon == 1) {
					ir_expr_free(l);
					ir_expr_thisfree(x);
					return r;
				} else
					break;
			} else if (l->ie_con.ic_ucon == 0)
				x->ie_con.ic_ucon = 0;
			else if (l->ie_con.ic_ucon == 1) {
				ir_expr_free(l);
				ir_expr_thisfree(x);
				return r;
			} else
				break;
			ir_con_cast(&x->ie_con, x->ie_type, x->ie_type);
			x->i_op = IR_ICON;
			ir_expr_free(l);
			ir_expr_free(r);
			return x;
		}
		if (r->i_op == IR_ICON) {
			if (IR_ISSIGNED(r->ie_type)) {
				if (r->ie_con.ic_icon == 0)
					x->ie_con.ic_icon = 0;
				else if (r->ie_con.ic_icon == 1) {
					ir_expr_free(r);
					ir_expr_thisfree(x);
					return l;
				} else {		/* R4 */
					x->ie_r = l;
					x->ie_l = r;
					return x;
				}
			} else if (r->ie_con.ic_ucon == 0)
				x->ie_con.ic_ucon = 0;
			else if (r->ie_con.ic_ucon == 1) {
				ir_expr_free(r);
				ir_expr_thisfree(x);
				return l;
			} else {			/* R4 */
				x->ie_r = l;
				x->ie_l = r;
				return x;
			}
			ir_con_cast(&x->ie_con, x->ie_type, x->ie_type);
			x->i_op = IR_ICON;
			ir_expr_free(l);
			ir_expr_free(r);
			return x;
		}
		break;
	case IR_ADD:
		l = x->ie_l;
		r = x->ie_r;
		if (l->i_op == IR_ICON && r->i_op == IR_ICON) {	/* R1 */
			if (IR_ISSIGNED(x->ie_type))
				x->ie_con.ic_icon = l->ie_con.ic_icon +
				    r->ie_con.ic_icon;
			else
				x->ie_con.ic_ucon = l->ie_con.ic_ucon +
				    r->ie_con.ic_ucon;
			ir_con_cast(&x->ie_con, x->ie_type, x->ie_type);
			x->i_op = IR_ICON;
			ir_expr_free(l);
			ir_expr_free(r);
			return x;
		}
		if (l->i_op == IR_ICON) {
			if ((IR_ISSIGNED(l->ie_type) &&
			     l->ie_con.ic_icon == 0) ||
			    l->ie_con.ic_ucon == 0) {
				ir_expr_free(l);
				ir_expr_thisfree(x);
				return r;
			}
			break;
		}
		if (r->i_op == IR_ICON) {
			if ((IR_ISSIGNED(r->ie_type) &&
			     r->ie_con.ic_icon == 0) ||
			    r->ie_con.ic_ucon == 0) {
				ir_expr_free(r);
				ir_expr_thisfree(x);
				return l;
			}
			if (l->i_op == IR_ADD && l->ie_l->i_op == IR_ICON) {
				/* R9 */
				x->ie_r = l->ie_r;
				if (IR_ISSIGNED(x->ie_type))
					r->ie_con.ic_icon = r->ie_con.ic_icon +
					    l->ie_l->ie_con.ic_icon;
				else
					r->ie_con.ic_ucon = r->ie_con.ic_ucon +
					    l->ie_l->ie_con.ic_ucon;
				x->ie_l = r;
				ir_expr_free(l->ie_l);
				ir_expr_thisfree(l);
				return x;
			}
			/* R2 */
			x->ie_r = l;
			x->ie_l = r;
			return x;
		}
		if (r->i_op == IR_ADD) {	/* R7 */
			x->ie_r = r->ie_r;
			r->ie_r = r->ie_l;
			r->ie_l = l;
			x->ie_l = r;
			x->ie_r = constfold_expr(x->ie_r);
			return x;
		}
		break;
	case IR_SUB:
		l = x->ie_l;
		r = x->ie_r;
		if (l->i_op == IR_ICON && r->i_op == IR_ICON) {	/* R5 */
			if (IR_ISSIGNED(x->ie_type))
				x->ie_con.ic_icon = l->ie_con.ic_icon -
				    r->ie_con.ic_icon;
			else
				x->ie_con.ic_ucon = l->ie_con.ic_ucon -
				    r->ie_con.ic_ucon;
			ir_con_cast(&x->ie_con, x->ie_type, x->ie_type);
			x->i_op = IR_ICON;
			ir_expr_free(l);
			ir_expr_free(r);
			return x;
		}
		if (l->i_op == IR_ICON) {
			if ((IR_ISSIGNED(l->ie_type) &&
			     l->ie_con.ic_icon == 0) ||
			    l->ie_con.ic_ucon == 0) {
				r = ir_unary(IR_UMINUS, r, r->ie_type);
				ir_expr_free(l);
				ir_expr_thisfree(x);
				return r;
			}
			break;
		}
		if (r->i_op == IR_ICON) {
			if ((IR_ISSIGNED(r->ie_type) &&
			     r->ie_con.ic_icon == 0) ||
			    r->ie_con.ic_ucon == 0) {
				ir_expr_free(r);
				ir_expr_thisfree(x);
				return l;
			}
			if (IR_ISSIGNED(r->ie_type))
				r->ie_con.ic_icon = -r->ie_con.ic_icon;
			else
				r->ie_con.ic_ucon = -r->ie_con.ic_ucon;
			/* R6 */
			ir_con_cast(&r->ie_con, r->ie_type, r->ie_type);
			x->ie_l = r;
			x->ie_r = l;
			x->i_op = IR_ADD;
			break;
		}
		break;
	default:
		break;
	}

	return x;
}

void
pass_constfold(struct passinfo *pi)
{
	struct ir_func *fn = pi->p_fn;
	struct ir_insn *insn;

	TAILQ_FOREACH(insn, &fn->if_iq, ii_link) {
		if (insn->i_op != IR_B && IR_ISBRANCH(insn)) {
			insn->ib_l = constfold_expr(insn->ib_l);
			insn->ib_r = constfold_expr(insn->ib_r);
		} else if (insn->i_op == IR_ASG)
			insn->is_r = constfold_expr(insn->is_r);
		else if (insn->i_op == IR_ST) {
			insn->is_l = constfold_expr(insn->is_l);
			insn->is_r = constfold_expr(insn->is_r);
		}
	}
}
