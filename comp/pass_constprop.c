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

struct setmemb {
	union {
		int	_symroot;
		union	ir_con _conroot;
	} u;
	int	s_iscon;

#define s_symroot	u._symroot
#define s_conroot	u._conroot
};

static int
findmemb(struct setmemb *set, int i)
{
	int j, r = i;

	while (!set[r].s_iscon && set[r].s_symroot != r)
		r = set[r].s_symroot;
	while (i != r) {
		j = set[i].s_symroot;
		set[i].s_symroot = r;
		i = j;
	}
	return r;
}

static void
constprop_expr(struct setmemb *set, struct ir_expr *x)
{
	int i;

	for (;;) {
		if (IR_ISBINEXPR(x)) {
			constprop_expr(set, x->ie_r);
			x = x->ie_l;
		} else if (IR_ISUNEXPR(x))
			x = x->ie_l;
		else if (x->i_op == IR_REG) {
			i = findmemb(set, x->ie_sym->is_id);
			if (set[i].s_iscon) {
				x->i_op = IR_ICON;
				x->ie_con = set[i].s_conroot;
			}
			break;
		} else
			break;
	}
}

void
pass_constprop(struct passinfo *pi)
{
	int i;
	struct ir_expr *l, *r;
	struct ir_func *fn = pi->p_fn;
	struct ir_insn *insn;
	struct setmemb *set;

	ir_func_linearize_regs(fn);
	set = xmnalloc(fn->if_regid, sizeof *set);
	for (i = 0; i < fn->if_regid; i++) {
		set[i].s_symroot = i;
		set[i].s_iscon = 0;
	}

	TAILQ_FOREACH(insn, &fn->if_iq, ii_link) {
		l = insn->is_l;
		r = insn->is_r;
		if (insn->i_op != IR_ASG || l->i_op != IR_REG)
			continue;
		if (r->i_op == IR_ICON) {
			if (IR_ISSIGNED(r->ie_type) &&
			    r->ie_con.ic_icon >= TARG_SIMM_MIN &&
			    r->ie_con.ic_icon <= TARG_SIMM_MAX) {
				set[l->ie_sym->is_id].s_iscon = 1;
				set[l->ie_sym->is_id].s_conroot = r->ie_con;
			} else if ((IR_ISPTR(r->ie_type) ||
			    IR_ISUNSIGNED(r->ie_type)) &&
			    r->ie_con.ic_ucon <= TARG_UIMM_MAX) {
				set[l->ie_sym->is_id].s_iscon = 1;
				set[l->ie_sym->is_id].s_conroot = r->ie_con;
			}
		} else if (r->i_op == IR_REG) {
			set[l->ie_sym->is_id].s_symroot = r->ie_sym->is_id;
			set[l->ie_sym->is_id].s_iscon = 0;
		} else
			continue;
	}

	TAILQ_FOREACH(insn, &fn->if_iq, ii_link) {
		if (IR_ISBRANCH(insn) && insn->i_op != IR_B) {
			constprop_expr(set, insn->ib_l);
			constprop_expr(set, insn->ib_r);
		} else if (insn->i_op == IR_ASG)
			constprop_expr(set, insn->is_r);
		else if (insn->i_op == IR_ST) {
			constprop_expr(set, insn->is_l);
			constprop_expr(set, insn->is_r);
		}
	}

	free(set);
}
