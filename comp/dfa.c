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
 * Data-flow analysis framework and passes.
 */

#include <sys/types.h>
#include <sys/queue.h>

#include <stdlib.h>

#include "comp/comp.h"
#include "comp/ir.h"

#define HEAP_LEFT(i)	(((i) << 1) | 1)
#define HEAP_RIGHT(i)	(((i) << 1) + 2)
#define HEAP_PARENT(i)	(((i) - 1) >> 1)

static void makeheap(int, struct cfa_bb **, int);
static void minheapify(struct cfa_bb **, int, int);
static struct cfa_bb *getmin(struct cfa_bb **, int);
static void minheapinsert(struct cfa_bb **, struct cfa_bb *, int);

static void livevar_calcuse(struct ir_func *);
static void *livevar_meet(struct ir_func *, void *, void *);
static int livevar_flow(struct ir_func *, struct cfa_bb *, void *);
static void livevar_getuse(struct ir_expr *, struct bitvec *);

void
dfa_initdata(struct dfadata *dfa)
{
	dfa->d_livein = NULL;
}

/*
 * See Steven S. Muchnick: Advanced Compiler Design & Implementation, chapter
 * 8.4 and Aho, Lam, Sethi, Ullman: Compilers - Principles, Techniques & Tools
 * Second Edition, chapter 9.3.3 for an explanation.
 */
static void
dfa(struct ir_func *fn, int forw,
    void *(*meet)(struct ir_func *, void *, void *),
    int (*flow)(struct ir_func *, struct cfa_bb *, void *), void *init,
    void *T)
{
	uint8_t *inheap;
	int edges, ents, flowset, meetset;
	void *meetres;
	struct cfa_bb *bb, **heap;
	struct cfa_bblink *bbl;
	struct cfadata *cfa = fn->if_cfadata;

	flowset = forw ? CFA_BB_INSET : CFA_BB_OUTSET;
	meetset = forw ? CFA_BB_OUTSET : CFA_BB_INSET;
	if (forw)
		cfa->c_entry->cb_outset = init;
	else
		cfa->c_exit->cb_inset = init;

	/*
	 * Put basic blocks into a priority queue so they are extracted
	 * in DFS order for forward analyses and reverse DFS order for
	 * backward analyses, to keep the number of iterations in the while
	 * loop below small.
	 */
	cfa_cfgsort(fn, forw ? CFA_CFGSORT_ASC : CFA_CFGSORT_DESC);
	heap = xmnalloc(cfa->c_nbb - 1, sizeof *heap);
	inheap = xmnalloc(cfa->c_nbb - 1, sizeof *inheap);
	ents = 0;
	SIMPLEQ_FOREACH(bb, &cfa->c_bbqh, cb_glolink) {
		if (forw && bb != cfa->c_entry) {
			heap[ents++] = bb;
			bb->cb_outset = T;
			inheap[bb->cb_id] = 1;
		} else if (!forw && bb != cfa->c_exit) {
			heap[ents++] = bb;
			bb->cb_inset = T;
			inheap[bb->cb_id] = 1;
		}
	}
	makeheap(forw, heap, ents);

	edges = forw ? CFA_BB_PREDS : CFA_BB_SUCCS;
	while (ents != 0) {
		bb = getmin(heap, ents);
		inheap[bb->cb_id] = 0;
		ents--;

		meetres = T;
		SIMPLEQ_FOREACH(bbl, &bb->cb_edges[edges], cb_link)
			meetres = meet(fn, meetres,
			    bbl->cb_bb->cb_dfasets[meetset]);

		if (flow(fn, bb, meetres)) {
			SIMPLEQ_FOREACH(bbl, &bb->cb_edges[edges ^ 1],
			    cb_link) {
				if (inheap[bbl->cb_bb->cb_id] != 0)
					continue;
				minheapinsert(heap, bbl->cb_bb, ents);
				ents++;
			}
		}
	}

	free(heap);
	free(inheap);
}

static void
makeheap(int min, struct cfa_bb **heap, int n)
{
	int i;

	for (i = (n + 1) / 2; i > 0; i--)
		minheapify(heap, i - 1, n);
}

static void
minheapify(struct cfa_bb **heap, int i, int n)
{
	int min;
	int l, r;
	struct cfa_bb *tmp;

	for (;;) {
		l = HEAP_LEFT(i);
		r = HEAP_RIGHT(i);
		if (l < n && heap[l]->cb_dfsno < heap[i]->cb_dfsno)
			min = l;
		else
			min = i;
		if (r < n && heap[r]->cb_dfsno < heap[min]->cb_dfsno)
			min = r;
		if (min == i)
			break;
		tmp = heap[i];
		heap[i] = heap[min];
		heap[min] = tmp;
		i = min;
	}
}

static struct cfa_bb *
getmin(struct cfa_bb **heap, int n)
{
	struct cfa_bb *rv = heap[0];

	heap[0] = heap[n - 1];
	minheapify(heap, 0, n - 1);
	return rv;
}

static void
minheapinsert(struct cfa_bb **heap, struct cfa_bb *bb, int n)
{
	struct cfa_bb *tmp;

	heap[n] = bb;
	while (n > 0 && heap[HEAP_PARENT(n)]->cb_dfsno > heap[n]->cb_dfsno) {
		tmp = heap[HEAP_PARENT(n)];
		heap[HEAP_PARENT(n)] = heap[n];
		heap[n] = tmp;
		n = HEAP_PARENT(n);
	}
}

static struct bitvec *lv_def;
static struct bitvec *lv_use;

void
dfa_livevar(struct ir_func *fn)
{
	int i;
	FILE *fp;
	struct ir_insn *insn;
	static int dumpno;

	mem_area_free(&fn->if_livevarmem);
	ir_func_linearize_regs(fn);
	livevar_calcuse(fn);
	lv_def = bitvec_alloc(NULL, fn->if_regid);
	lv_use = bitvec_alloc(NULL, fn->if_regid);
	dfa(fn, 0, livevar_meet, livevar_flow, NULL, NULL);
	free(lv_def);
	free(lv_use);

	if (Iflag) {
		fp = dump_open("DFA.LIVE", fn->if_sym->is_name, "w", dumpno++);
		TAILQ_FOREACH(insn, &fn->if_iq, ii_link) {
			ir_dump_insn(fp, insn);
			fprintf(fp, "#");
			for (i = 0; i < fn->if_regid; i++) {

				if (insn->ii_dfadata.d_livein == NULL ||
				    !bitvec_isset(insn->ii_dfadata.d_liveout,
				    i))
					continue;
				fprintf(fp, " ");
				ir_dump_symbol(fp, fn->if_regs[i]);
			}
			fprintf(fp, "\n");
		}
		fclose(fp);
	}
}

static void
livevar_calcuse(struct ir_func *fn)
{
	struct bitvec *in;
	struct ir_expr *x;
	struct ir_insn *insn;

	TAILQ_FOREACH(insn, &fn->if_iq, ii_link) {
		insn->ii_dfadata.d_livein = NULL;
		if (insn->i_op == IR_B || insn->i_op == IR_LBL)
			continue;
		in = bitvec_alloc(&fn->if_livevarmem, fn->if_regid);
		insn->ii_dfadata.d_livein = in;
		if (IR_ISBRANCH(insn)) {
			livevar_getuse(insn->ib_l, in);
			livevar_getuse(insn->ib_r, in);
			continue;
		}
		switch (insn->i_op) {
		case IR_ST:
			livevar_getuse(insn->is_l, in);
		case IR_ASG:
			livevar_getuse(insn->is_r, in);
			break;
		case IR_CALL:
			SIMPLEQ_FOREACH(x, &insn->ic_argq, ie_link)
				livevar_getuse(x, in);
			if (insn->ic_fn->is_op == IR_REGSYM)
				bitvec_setbit(in, insn->ic_fn->is_id);
			break;
		case IR_RET:
			if (insn->ir_retexpr != NULL)
				livevar_getuse(insn->ir_retexpr, in);
			break;
		default:
			fatalx("livevar_calcuse: bad insn: 0x%x", insn->i_op);
		}
	}
}

static void *
livevar_meet(struct ir_func *fn, void *v, void *w)
{
	struct bitvec *dst = v, *src = w;

	if (dst == NULL)
		dst = bitvec_alloc(&fn->if_livevarmem, fn->if_regid);
	if (src != NULL)
		bitvec_or(dst, src);
	return dst;
}

static int
livevar_flow(struct ir_func *fn, struct cfa_bb *bb, void *v)
{
	int changes = 0;
	struct bitvec *def, *in, *out, *use;
	struct ir_insn *insn, *term, *prev;

	if (bb->cb_first != NULL)
		term = TAILQ_PREV(bb->cb_first, ir_insnq, ii_link);
	else
		term = NULL;

	in = out = v;
	bb->cb_outset = out;
	def = lv_def;
	use = lv_use;
	if (bb->cb_inset != NULL)
		bitvec_cpy(use, bb->cb_inset);
	for (insn = bb->cb_last; insn != term; insn = prev) {
		prev = TAILQ_PREV(insn, ir_insnq, ii_link); 
		insn->ii_dfadata.d_liveout = out;
		if (insn->i_op == IR_B || insn->i_op == IR_LBL) {
			insn->ii_dfadata.d_livein = in;
			continue;
		}

		if (out == NULL)
			continue;

		bitvec_cpy(def, out);
		in = insn->ii_dfadata.d_livein;
		if (insn->i_op == IR_ASG && insn->is_l->i_op == IR_REG)
			bitvec_clearbit(def, insn->is_l->ie_sym->is_id);
		else if (insn->i_op == IR_CALL && insn->ic_ret != NULL &&
		    insn->ic_ret->i_op == IR_REG)
			bitvec_clearbit(def, insn->ic_ret->ie_sym->is_id);

		bitvec_or(in, def);
		out = in;
	}
	if (bb->cb_inset == NULL)
		changes = in != NULL;
	else
		changes = bitvec_cmp(in, use);
	bb->cb_inset = in;
	return changes;
}

static void
livevar_getuse(struct ir_expr *x, struct bitvec *use)
{
	for (;;) {
		if (IR_ISBINEXPR(x)) {
			livevar_getuse(x->ie_l, use);
			x = x->ie_r;
			continue;
		} else if (IR_ISUNEXPR(x)) {
			x = x->ie_l;
			continue;
		} else {
			if (x->i_op == IR_REG && x->ie_sym->is_op == IR_REGSYM)
				bitvec_setbit(use, x->ie_sym->is_id);
			break;
		}
	}
}
