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

static int totalasg;
static int oldvarid;
static struct ir_symbol **oldvars;
static struct {
	struct	cfa_bb **bbs;
	struct	ir_symbol *s;
	int nasg;
	int nbb;
} *symdata;

static void
ssa_placephi(struct ir_func *fn)
{
	int inw, itercount = 0;
	size_t i;
	struct cfadata *cfa = fn->if_cfadata;
	struct cfa_bb *x, *y, **w, **wtop;
	struct cfa_bblink *bbl;
	struct ir_insn *insn, *ninsn, *phi;
	struct ir_symbol *sym;
	
	struct {
		struct	cfa_bb *bb;
		int	hasalready;
		int	work;
	} *d;

	itercount = 0;
	d = xcalloc(cfa->c_nbb, sizeof *d);
	w = xmnalloc(cfa->c_nbb, sizeof *w);
	SIMPLEQ_FOREACH(x, &cfa->c_bbqh, cb_glolink)
		d[x->cb_id].bb = x;

	/*
	 * Record how often each symbol gets assigned to, along with
	 * the basic blocks that contain the assignments. This calculates
	 * the set A(X) the algorithm refers to.
	 */
	SIMPLEQ_FOREACH(x, &cfa->c_bbqh, cb_glolink) {
		if ((ninsn = x->cb_last) != NULL)
			ninsn = TAILQ_NEXT(ninsn, ii_link);
		for (insn = x->cb_first; insn != ninsn;
		    insn = TAILQ_NEXT(insn, ii_link)) {
			if (insn->i_op == IR_ASG) {
				if (insn->is_l->i_op != IR_REG)
					continue;
				sym = insn->is_l->ie_sym;
			} else if (insn->i_op == IR_CALL) {
				if (insn->ic_ret == NULL ||
				    insn->ic_ret->i_op != IR_REG)
					continue;
				sym = insn->ic_ret->ie_sym;
			} else
				continue;

			if (sym->is_id < REG_NREGS)
				continue;
			if (symdata[sym->is_id].bbs[x->cb_id] == NULL) {
				symdata[sym->is_id].nbb++;
				symdata[sym->is_id].bbs[x->cb_id] = x;
			}
			symdata[sym->is_id].nasg++;
			totalasg++;
		}
	}

	SIMPLEQ_FOREACH(sym, &fn->if_regq, is_link) {
		itercount++;
		wtop = w;
		inw = 0;

		/*
		 * Put basic blocks that contain assignments to sym into
		 * worklist.
		 */
		for (i = 0; i < cfa->c_nbb; i++) {
			if (symdata[sym->is_id].nbb &&
			    symdata[sym->is_id].bbs[i] != NULL) {
				*wtop++ = symdata[sym->is_id].bbs[i];
				inw++;
			}
		}

		while (inw) {
			x = *--wtop;
			inw--;
			for (i = bitvec_firstset(x->cb_df);
			    i < x->cb_df->b_nbit;
			    i = bitvec_nextset(x->cb_df, i)) {
				y = d[i].bb;
				if (d[i].hasalready >= itercount)
					continue;

				/*
				 * Phi functions in the exit block are
				 * pretty useless, so don't put them there.
				 * This is a modification to the original
				 * algorithm. Furthermore, don't introduce
				 * phi functions for variables that got
				 * assigned to only once.
				 */
				if (y != cfa->c_exit &&
				    symdata[sym->is_id].nasg > 1) {
					phi = ir_phi(sym);
					SIMPLEQ_FOREACH(bbl, &y->cb_preds,
					    cb_link)
						ir_phi_addarg(phi, sym,
						    bbl->cb_bb);
					cfa_bb_prepend(fn, y, phi);
					totalasg++;
				}

				d[i].hasalready = itercount;
				if (d[i].work >= itercount)
					continue;
				*wtop++ = y;
				inw++;
			}
		}
	}

	free(d);
	free(w);
}

static struct ir_symbol *
replacesym(struct ir_symbol *sym)
{
	if (sym->is_id < REG_NREGS)
		return sym;
	return symdata[sym->is_id].s;
}

static void
ssa_replace(struct ir_expr *x)
{
	for (;;) {
		if (IR_ISBINEXPR(x)) {
			ssa_replace(x->ie_l);
			x = x->ie_r;
		} else if (IR_ISUNEXPR(x))
			x = x->ie_l;
		else if (x->i_op == IR_REG) {
			x->ie_sym = replacesym(x->ie_sym);
			break;
		} else
			break;
	}
}

static void
ssa_search(struct ir_func *fn, struct cfa_bb *x)
{
	int hadphi;
	struct cfa_bblink *bbl;
	struct ir_symbol *oldsym, *sym;
	struct ir_expr *parm;
	struct ir_insn *asgs = NULL;
	struct ir_insn *insn, *ninsn;
	struct ir_phiarg *arg;

	if ((ninsn = x->cb_last) != NULL)
		ninsn = TAILQ_NEXT(ninsn, ii_link);
	for (insn = x->cb_first; insn != ninsn;
	    insn = TAILQ_NEXT(insn, ii_link)) {
		if (insn->i_op == IR_B || insn->i_op == IR_LBL)
			continue;
		if (IR_ISBRANCH(insn)) {
			ssa_replace(insn->ib_l);
			ssa_replace(insn->ib_r);
			continue;
		}
		switch (insn->i_op) {
		case IR_ASG:
			ssa_replace(insn->is_r);
			if (insn->is_l->i_op != IR_REG)
				continue;
			oldsym = insn->is_l->ie_sym;
			break;
		case IR_ST:
			ssa_replace(insn->is_l);
			ssa_replace(insn->is_r);
			continue;
		case IR_CALL:
			SIMPLEQ_FOREACH(parm, &insn->ic_argq, ie_link)
				ssa_replace(parm);
			if (insn->ic_ret == NULL ||
			    insn->ic_ret->i_op != IR_REG)
				continue;
			oldsym = insn->ic_ret->ie_sym;
			if (insn->ic_fn->is_op == IR_REGSYM)
				insn->ic_fn = replacesym(insn->ic_fn);
			break;
		case IR_RET:
			if (insn->ir_retexpr != NULL)
				ssa_replace(insn->ir_retexpr);
			continue;
		case IR_PHI:
			oldsym = insn->ip_sym;
			break;
		default:
			fatalx("ssa_search: bad op: 0x%x", insn->i_op);
		}

		if (symdata[oldsym->is_id].nasg == 1)
			continue;

		sym = ir_vregsym(fn, oldsym->is_type);
		if (insn->i_op == IR_ASG)
			insn->is_l->ie_sym = sym;
		else if (insn->i_op == IR_CALL)
			insn->ic_ret->ie_sym = sym;
		else if (insn->i_op == IR_PHI)
			insn->ip_sym = sym;
		if (sym->is_id - oldvarid >= totalasg)
			fatalx("asgs");
		oldvars[sym->is_id - oldvarid] = oldsym;
		sym->is_top = symdata[oldsym->is_id].s;
		symdata[oldsym->is_id].s = sym;
		insn->i_auxdata = asgs;
		asgs = insn;
	}

	SIMPLEQ_FOREACH(bbl, &x->cb_succs, cb_link) {
		hadphi = 0;
		ninsn = bbl->cb_bb->cb_last;
		for (insn = bbl->cb_bb->cb_first; insn != ninsn;
		    insn = TAILQ_NEXT(insn, ii_link)) {
			if (insn->i_op != IR_PHI) {
				if (hadphi)
					break;
				continue;
			}
			hadphi = 1;
			SIMPLEQ_FOREACH(arg, &insn->ip_args, ip_link) {
				if (arg->ip_bb != x)
					continue;
				arg->ip_arg = symdata[arg->ip_arg->is_id].s;
			}
		}
	}

	SIMPLEQ_FOREACH(bbl, &x->cb_idomkids, cb_link)
		ssa_search(fn, bbl->cb_bb);

	while (asgs != NULL) {
		if (asgs->i_op == IR_PHI)
			sym = asgs->ip_sym;
		else if (asgs->i_op == IR_ASG)
			sym = asgs->is_l->ie_sym;
		else
			sym = asgs->ic_ret->ie_sym;
		if (sym->is_id - oldvarid >= totalasg)
			fatalx("asgs");
		sym = oldvars[sym->is_id - oldvarid];
		if (symdata[sym->is_id].nasg > 1)
			symdata[sym->is_id].s = symdata[sym->is_id].s->is_top;
		if (symdata[sym->is_id].s == NULL)
			fatalx("ssa_search");
		asgs = asgs->i_auxdata;
	}
}

static void
ssa_rename(struct ir_func *fn)
{
	struct ir_symbol *sym;

	/*
	 * The algorithm in the paper sets S(V) to the empty stack.
	 * We can't do that, however, since a variable might be used
	 * uninitialized. When we then access Top(S(V)) in an expression
	 * where there is no reaching definition for V, we would access
	 * the empty stack.
	 */
	SIMPLEQ_FOREACH(sym, &fn->if_regq, is_link) {
		sym->is_top = NULL;
		symdata[sym->is_id].s = sym;
	}
	oldvarid = fn->if_regid;
	oldvars = xcalloc(totalasg, sizeof *oldvars);
	ssa_search(fn, fn->if_cfadata->c_entry);
	free(oldvars);
}

/*
 * See Ron Cytron, Jeanne Ferrante, Barry K. Rosen, Mark N. Wegman and
 * F. Kenneth Zadeck: Efficiently Computing Static Single Assignment Form
 * and the Control Dependence Graph, section 5.1 and 5.2.
 */
void
pass_ssa(struct passinfo *pi)
{
	int i, j;
	uintmax_t elems;
	struct cfa_bb **bbs;
	struct ir_func *fn = pi->p_fn;

	cfa_buildcfg(fn);
	cfa_calcdom(fn);
	cfa_calcdf(fn);
	symdata = xcalloc(fn->if_regid, sizeof *symdata);
	elems = (uintptr_t)fn->if_cfadata->c_nbb * fn->if_regid;
	if (elems > SIZE_MAX)
		fatalx("pass_ssa");
	bbs = xcalloc(elems, sizeof *bbs);
	for (i = j = 0; i < fn->if_regid; i++, j += fn->if_cfadata->c_nbb)
		symdata[i].bbs = &bbs[j];
	totalasg = 0;
	ssa_placephi(fn);
	ssa_rename(fn);

	free(bbs);
	free(symdata);
}

/*
 * This is the reverse operation to transforming the code to SSA form.
 */
void
pass_undo_ssa(struct passinfo *pi)
{
	int hadphi;
	struct ir_func *fn = pi->p_fn;
	struct ir_expr *dst, *src;
	struct ir_insn *asg, *insn, *next, *end;
	struct ir_phiarg *arg;
	struct cfa_bb *bb;

	SIMPLEQ_FOREACH(bb, &fn->if_cfadata->c_bbqh, cb_glolink) {
		hadphi = 0;
		if ((end = bb->cb_last) != NULL)
			end = TAILQ_NEXT(end, ii_link);
		for (insn = bb->cb_first; insn != end; insn = next) {
			next = TAILQ_NEXT(insn, ii_link);
			if (insn->i_op != IR_PHI) {
				if (hadphi)
					break;
				continue;
			}
			hadphi = 1;
			SIMPLEQ_FOREACH(arg, &insn->ip_args, ip_link) {
				if (insn->ip_sym == arg->ip_arg)
					continue;
				dst = ir_virtreg(insn->ip_sym);
				src = ir_virtreg(arg->ip_arg);
				asg = ir_asg(dst, src);
				cfa_bb_append(fn, arg->ip_bb, asg);
			}
			ir_delete_insn(fn, insn);
		}
	}
}
