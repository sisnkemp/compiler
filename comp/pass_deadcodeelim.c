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
 * Perform dead code elimination. Based on
 * Steven S. Muchnick: Advanced Compiler Design & Implementation,
 * chapter 18.10.
 */

#include <sys/types.h>
#include <sys/queue.h>

#include <stdlib.h>

#include "comp/comp.h"
#include "comp/ir.h"
#include "comp/passes.h"

struct dceaux {
	struct	dceaux *d_top;
	struct	ir_insn *d_insn;
	int	d_live;
};

static struct dceaux *worklist;

struct duelem {
	struct	duelem *d_left;
	struct	duelem *d_right;
	struct	ir_insn *d_insn;
};

struct udelem {
	struct	ir_insn *u_insn;
};

static struct memarea mem;

static void
dce_walkdu(struct duelem *du)
{
	struct dceaux *aux;
	struct ir_insn *insn;

	for (;;) {
		if (du == NULL)
			return;
		insn = du->d_insn;
		aux = insn->i_auxdata;
		if (!aux->d_live && IR_ISBRANCH(insn) && insn->i_op != IR_B) {
			aux->d_live = 1;
			if (aux->d_top == NULL) {
				aux->d_top = worklist;
				worklist = aux;
			}
		}
		dce_walkdu(du->d_left);
		du = du->d_right;
	}
}

static void
dce_walkud(int round, struct udelem *ud, struct ir_symbol *sym)
{
	struct dceaux *aux;

	if (ud[sym->is_id].u_insn == NULL)
		return;
	aux = ud[sym->is_id].u_insn->i_auxdata;
	if (!aux->d_live) {
		aux->d_live = 1;
		if (aux->d_top == NULL) {
			aux->d_top = worklist;
			worklist = aux;
		}
	}
}

static void
dce_findvars(int round, struct udelem *ud, struct ir_expr *x)
{
	for (;;) {
		if (IR_ISBINEXPR(x)) {
			dce_findvars(round, ud, x->ie_r);
			x = x->ie_l;
		} else if (IR_ISUNEXPR(x))
			x = x->ie_l;
		else if (x->i_op == IR_REG) {
			dce_walkud(round, ud, x->ie_sym);
			break;
		} else
			break;
	}
}

static void
dce_adduse(struct duelem **du, struct ir_insn *insn, struct ir_symbol *sym)
{
	struct duelem *p, **pp;

	pp = &du[sym->is_id];
	while (*pp != NULL) {
		p = *pp;
		if (p->d_insn == insn)
			return;
		if (insn < p->d_insn)
			pp = &p->d_left;
		else
			pp = &p->d_right;
	}
	*pp = p = mem_alloc(&mem, sizeof *p);
	p->d_left = p->d_right = NULL;
	p->d_insn = insn;
}

static void
dce_getuses(struct duelem **du, struct ir_insn *insn, struct ir_expr *x)
{
	struct dceaux *aux;

	for (;;) {
		if (IR_ISBINEXPR(x)) {
			dce_getuses(du, insn, x->ie_r);
			x = x->ie_l;
		} else if (IR_ISUNEXPR(x))
			x = x->ie_l;
		else if (x->i_op == IR_REG) {
			dce_adduse(du, insn, x->ie_sym);
			break;
		} else if (x->i_op == IR_GVAR || x->i_op == IR_LVAR ||
		    x->i_op == IR_PVAR || x->i_op == IR_GADDR ||
		    x->i_op == IR_LADDR || x->i_op == IR_PADDR) {
			aux = insn->i_auxdata;
			if (!aux->d_live) {
				aux->d_live = 1;
				if (aux->d_top == NULL) {
					aux->d_top = worklist;
					worklist = aux;
				}
			}
			break;
		} else
			break;
	}
}

void
pass_deadcodeelim(struct passinfo *pi)
{
	int round;
	struct dceaux *aux;
	struct duelem **du;
	struct udelem *ud;
	struct ir_func *fn = pi->p_fn;
	struct ir_expr *x;
	struct ir_insn *insn, *ninsn;
	struct ir_phiarg *arg;
	mem_area_init(&mem);

	/*
	 * Calculate the du- and ud-chains. Because the IR is in SSA form,
	 * the ud-chains are sets with at most one element and both of them
	 * are easy to compute. While there, mark the essential instructions.
	 */
	ir_func_linearize_regs(fn);
	ud = xcalloc(fn->if_regid, sizeof *ud);
	du = xcalloc(fn->if_regid, sizeof *du);
	worklist = NULL;
	TAILQ_FOREACH(insn, &fn->if_iq, ii_link) {
		aux = insn->i_auxdata = mem_alloc(&mem, sizeof *aux);
		aux->d_top = NULL;
		aux->d_insn = insn;
		aux->d_live = 0;
		if (insn->i_op == IR_B || insn->i_op == IR_LBL) {
			aux->d_live = 1;
			continue;
		}
		if (IR_ISBRANCH(insn)) {
			aux->d_live = 1;
			aux->d_top = worklist;
			worklist = aux;
			dce_getuses(du, insn, insn->ib_l);
			dce_getuses(du, insn, insn->ib_r);
			continue;
		}
		switch (insn->i_op) {
		case IR_ASG:
			if (insn->is_l->i_op == IR_REG)
				ud[insn->is_l->ie_sym->is_id].u_insn = insn;
			else {
				aux->d_live = 1;
				aux->d_top = worklist;
				worklist = aux;
			}
			dce_getuses(du, insn, insn->is_r);
			break;
		case IR_ST:
			aux->d_live = 1;
			aux->d_top = worklist;
			worklist = aux;
			dce_getuses(du, insn, insn->is_r);
			dce_getuses(du, insn, insn->is_l);
			break;
		case IR_CALL:
			aux->d_live = 1;
			aux->d_top = worklist;
			worklist = aux;
			if (insn->ic_ret != NULL)
				ud[insn->ic_ret->ie_sym->is_id].u_insn = insn;
			SIMPLEQ_FOREACH(x, &insn->ic_argq, ie_link)
				dce_getuses(du, insn, x);
			if (insn->ic_fn->is_op == IR_REGSYM)
				dce_adduse(du, insn, insn->ic_fn);
			break;
		case IR_RET:
			aux->d_live = 1;
			aux->d_top = worklist;
			worklist = aux;
			if (insn->ir_retexpr != NULL)
				dce_getuses(du, insn, insn->ir_retexpr);
			break;
		case IR_PHI:
			ud[insn->ip_sym->is_id].u_insn = insn;
			SIMPLEQ_FOREACH(arg, &insn->ip_args, ip_link)
				dce_adduse(du, insn, arg->ip_arg);
			break;
		default:
			fatalx("pass_deadcodeelim: bad op: 0x%x", insn->i_op);
		}
	}

	/*
	 * Walk the essential instructions. Instructions that compute
	 * values used by essential instructions are essential as well.
	 * Conditional branches that use values computed by essential
	 * instructions are essential too.
	 */
	for (round = 1; worklist != NULL; round ++) {
		aux = worklist;
		worklist = worklist->d_top;
		aux->d_top = NULL;
		insn = aux->d_insn;
		if (IR_ISBRANCH(insn)) {
			dce_findvars(round, ud, insn->ib_l);
			dce_findvars(round, ud, insn->ib_r);
			continue;
		}
		switch (insn->i_op) {
		case IR_ASG:
			dce_findvars(round, ud, insn->is_r);
			if (insn->is_l->i_op == IR_REG)
				dce_walkdu(du[insn->is_l->ie_sym->is_id]);
			break;
		case IR_ST:
			dce_findvars(round, ud, insn->is_l);
			dce_findvars(round, ud, insn->is_r);
			break;
		case IR_CALL:
			SIMPLEQ_FOREACH(x, &insn->ic_argq, ie_link)
				dce_findvars(round, ud, x);
			if (insn->ic_ret != NULL)
				dce_walkdu(du[insn->ic_ret->ie_sym->is_id]);
			if (insn->ic_fn->is_op == IR_REGSYM)
				dce_walkud(round, ud, insn->ic_fn);
			break;
		case IR_RET:
			if (insn->ir_retexpr != NULL)
				dce_findvars(round, ud, insn->ir_retexpr);
			break;
		case IR_PHI:
			SIMPLEQ_FOREACH(arg, &insn->ip_args, ip_link)
				dce_walkud(round, ud, arg->ip_arg);
			dce_walkdu(du[insn->ip_sym->is_id]);
			break;
		}
	}

	/*
	 * Delete instructions that are not essential.
	 */
	for (insn = TAILQ_FIRST(&fn->if_iq); insn != NULL; insn = ninsn) {
		ninsn = TAILQ_NEXT(insn, ii_link);
		aux = insn->i_auxdata;
		if (!aux->d_live)
			cfa_bb_delinsn(fn, insn->ii_bb, insn);
	}

	free(ud);
	free(du);
	mem_area_free(&mem);
}
