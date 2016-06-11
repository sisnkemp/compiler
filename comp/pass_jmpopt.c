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
 * Various jump optimizations Also does some other cleanups, like removing
 * unreachable code, redundant labels and labels that are not jumped to.
 *
 * T0: Deletes unreachable code.
 * T1: Kills labels that are never jumped to.
 *
 * T2:
 * bcond L1    b!cond L2
 * b L2     => L1:
 * L1:
 * 
 * T3:
 * b(cond) L1    b(cond) L2    
 * ...        => ...
 * L1: b L2      L1: b L2
 *
 * T4:
 * b(cond) L1 => L1: ...
 * L1: ...
 *
 * T5:
 * L1:           L1:
 * L2:        => ...
 * L3:           b(cond) L1
 * ...           ...
 * b(cond) L1    b(cond) L1
 * ...           ...
 * b(cond) L2    b(cond) L1
 * ...
 * b(cond) L3
 */

#include <sys/types.h>
#include <sys/queue.h>

#include "comp/comp.h"
#include "comp/ir.h"
#include "comp/passes.h"

struct setmemb {
	struct	ir_lbl *s_lbl;
	int	s_root;
	int	s_rank;
};

static void setinit(struct setmemb *, struct ir_lbl *);
static void setunion(struct setmemb *, int, int);
static int setfind(struct setmemb *, int);

struct round {
	int	r_lblid;
	int	r_round;
};

static void updateround(struct ir_insn *, int);

static int t0(struct ir_func *, struct ir_insn **, struct ir_insn **,
    struct ir_insn **);
static int t1(struct ir_func *, struct ir_insn **, struct ir_insn **,
    struct ir_insn **, int);
static int t2(struct ir_func *, struct ir_insn **, struct ir_insn **,
    struct ir_insn **);
static int t3(struct ir_func *, struct ir_insn **, struct ir_insn **,
    struct ir_insn **);
static int t4(struct ir_func *, struct ir_insn **, struct ir_insn **,
    struct ir_insn **);
static int t5(struct ir_func *, struct ir_insn **, struct ir_insn **,
    struct ir_insn **, struct setmemb *);

void
pass_jmpopt(struct passinfo *pi)
{
	int changes, id = 0, round = 1;
	struct ir_func *fn = pi->p_fn;
	struct ir_insn *insn, *next, *prev;
	struct memarea ma;
	struct round *r;
	struct setmemb *memb = NULL;

	mem_area_init(&ma);

	TAILQ_FOREACH(insn, &fn->if_iq, ii_link) {
		if (insn->i_op == IR_LBL) {
			r = mem_alloc(&ma, sizeof *r);
			r->r_round = 0;
			r->r_lblid = id++;
			insn->i_auxdata = r;
		}
	}

	if (id != 0)
		memb = mem_calloc(&ma, id, sizeof *memb);

	do {
		changes = 0;
		prev = NULL;
		for (insn = TAILQ_FIRST(&fn->if_iq); insn != NULL;
		    prev = insn, insn = next) {
			next = TAILQ_NEXT(insn, ii_link);

			if (insn->i_op == IR_RET || insn->i_op == IR_B)
			    changes |= t0(fn, &prev, &insn, &next);
			if (insn == NULL)
				continue;

			changes |= t1(fn, &prev, &insn, &next, round);
			if (insn == NULL)
				continue;

			if (prev != NULL && prev->i_op != IR_B &&
			    IR_ISBRANCH(prev) &&
			    next != NULL && next->i_op == IR_LBL &&
			    insn->i_op == IR_B)
				changes |= t2(fn, &prev, &insn, &next);
			if (insn == NULL)
				continue;

			if (IR_ISBRANCH(insn))
				changes |= t3(fn, &prev, &insn, &next);
			if (insn == NULL)
				continue;

			if (next != NULL &&
			    IR_ISBRANCH(insn) && next->i_op == IR_LBL)
				changes |= t4(fn, &prev, &insn, &next);

			if (insn != NULL)
				changes |= t5(fn, &prev, &insn, &next, memb);
		}
		round++;
	} while (changes);

	mem_area_free(&ma);
}

static void
setinit(struct setmemb *memb, struct ir_lbl *lbl)
{
	int i = ((struct round *)lbl->i_auxdata)->r_lblid;

	if (memb[i].s_lbl == NULL) {
		memb[i].s_lbl = lbl;
		memb[i].s_root = i;
		memb[i].s_rank = 0;
	}
}

static void
setunion(struct setmemb *memb, int i, int j)
{
	if (memb[i].s_rank == memb[j].s_rank) {
		memb[j].s_root = i;
		memb[i].s_rank++;
	} else if (memb[i].s_rank > memb[j].s_rank)
		memb[j].s_root = i;
	else
		memb[i].s_root = j;
}

static int
setfind(struct setmemb *memb, int i)
{
	int r, j;

	if (memb[i].s_lbl == NULL)
		return -1;
	r = i;
	while (memb[r].s_root != r)
		r = memb[r].s_root;
	while (memb[i].s_root != r) {
		j = i;
		i = memb[i].s_root;
		memb[j].s_root = r;
	}
	return r;
}

static void
updateround(struct ir_insn *insn, int round)
{
	struct ir_branch *b;
	struct round *r;

	if (insn == NULL || !IR_ISBRANCH(insn))
		return;
	b = (struct ir_branch *)insn;
	r = b->ib_lbl->i_auxdata;
	r->r_round = round;
}

static int
t0(struct ir_func *fn, struct ir_insn **prevp, struct ir_insn **insnp,
    struct ir_insn **nextp)
{
	int rv = 0;
	struct ir_insn *next = *nextp;

	while (next != NULL && next->i_op != IR_LBL) {
		ir_delete_insn(fn, next);
		next = TAILQ_NEXT(next, ii_link);
		rv = 1;
	}
	*nextp = next;
	return rv;
}

static int
t1(struct ir_func *fn, struct ir_insn **prevp, struct ir_insn **insnp,
    struct ir_insn **nextp, int round)
{
	struct ir_insn *insn = *insnp, *next = *nextp;
	struct round *r;

	updateround(*prevp, round);
	updateround(insn, round);
	updateround(next, round);
	if (insn->i_op != IR_LBL || IR_ISBRANCH(insn))
		return 0;
	r = insn->i_auxdata;
	if (r->r_round < round - 1) {
		ir_delete_insn(fn, insn);
		*insnp = next;
		if (next != NULL)
			*nextp = TAILQ_NEXT(next, ii_link);
		return 1;
	}
	return 0;
}

static int
t2(struct ir_func *fn, struct ir_insn **prevp, struct ir_insn **insnp,
    struct ir_insn **nextp)
{
	struct ir_insn *insn = *insnp, *next = *nextp, *prev = *prevp;
	struct ir_branch *b, *prevb;
	struct ir_lbl *nextlbl;

	static int negcond[] = {
		IR_BNE, IR_BEQ, IR_BGE, IR_BGT, IR_BLE, IR_BLT
	};

	prevb = (struct ir_branch *)prev;
	b = (struct ir_branch *)insn;
	nextlbl = (struct ir_lbl *)next;
	if (prevb->ib_lbl != nextlbl)
		return 0;

	prevb->i_op = negcond[prevb->i_op - IR_BEQ];
	prevb->ib_lbl = b->ib_lbl;
	ir_delete_insn(fn, insn);
	*prevp = TAILQ_PREV(prev, ir_insnq, ii_link);
	*insnp = prev;
	return 1;
}

static int
t3(struct ir_func *fn, struct ir_insn **prevp, struct ir_insn **insnp,
    struct ir_insn **nextp)
{
	struct ir_insn *insn = *insnp, *prev = *prevp;
	struct ir_branch *b, *dstb;

	b = (struct ir_branch *)insn;
	dstb = (struct ir_branch *)TAILQ_NEXT(b->ib_lbl, ii_link);
	if (dstb == NULL || dstb->i_op != IR_B)
		return 0;
	if (dstb->ib_lbl == (struct ir_lbl *)prev)
		return 0;
	b->ib_lbl = dstb->ib_lbl;
	return 1;
}

static int
t4(struct ir_func *fn, struct ir_insn **prevp, struct ir_insn **insnp,
    struct ir_insn **nextp)
{
	struct ir_insn *insn = *insnp, *next = *nextp, *prev = *prevp, *tmp;
	struct ir_branch *b;
	struct ir_lbl *lbl;

	b = (struct ir_branch *)insn;
	for (tmp = next; tmp != NULL && tmp->i_op == IR_LBL;
	    tmp = TAILQ_NEXT(tmp, ii_link)) {
		lbl = (struct ir_lbl *)tmp;
		if (lbl == b->ib_lbl) {
			ir_delete_insn(fn, insn);
			*insnp = prev;
			if (prev != NULL)
				*prevp = TAILQ_PREV(prev, ir_insnq, ii_link);
			return 1;
		}
	}
	return 0;
}

static int
t5(struct ir_func *fn, struct ir_insn **prevp, struct ir_insn **insnp,
    struct ir_insn **nextp, struct setmemb *memb)
{
	int i;
	struct ir_insn *n = *nextp, *p = *insnp;
	struct ir_branch *b;
	struct ir_lbl *lbl;
	struct round *rn, *rp;

	if (IR_ISBRANCH(p)) {
		b = (struct ir_branch *)p;
		rp = b->ib_lbl->i_auxdata;
		if ((i = setfind(memb, rp->r_lblid)) == -1)
			return 0;
		if (b->ib_lbl == memb[i].s_lbl)
			return 0;
		b->ib_lbl = memb[i].s_lbl;
		return 1;
	}

	if (p->i_op != IR_LBL || n == NULL || n->i_op != IR_LBL)
		return 0;
	setinit(memb, (struct ir_lbl *)p);
	rp = p->i_auxdata;
	rn = n->i_auxdata;
	while (n != NULL && n->i_op == IR_LBL) {
		lbl = (struct ir_lbl *)n;
		setinit(memb, lbl);
		setunion(memb, rp->r_lblid, rn->r_lblid);
		if (memb[rp->r_lblid].s_root != rp->r_lblid)
			ir_delete_insn(fn, p);
		p = n;
		rp = rn;
		n = TAILQ_NEXT(n, ii_link);
		if (memb[rp->r_lblid].s_root != rp->r_lblid)
			ir_delete_insn(fn, p);
		if (n != NULL)
			rn = n->i_auxdata;
	}
	if (memb[rp->r_lblid].s_root != rp->r_lblid)
		ir_delete_insn(fn, p);
	*insnp = n;
	if (n != NULL)
		*nextp = TAILQ_NEXT(n, ii_link);
	else
		*nextp = NULL;
	return 1;
}
