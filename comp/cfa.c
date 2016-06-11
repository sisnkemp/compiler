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

#include <sys/queue.h>
#include <sys/types.h>

#include <stdlib.h>

#include "comp/comp.h"
#include "comp/ir.h"
#include "comp/passes.h"

static void cfgdump(struct ir_func *);
static int cfg_dfs(struct cfa_bb *, int, int);
static struct cfa_bb *bballoc(struct cfadata *);
static void addsucc(struct cfadata *, struct cfa_bb *, struct cfa_bb *);
static void calcdom_simple(struct ir_func *);
static void calcdom_lentar(struct ir_func *);

/*
 * Creates a completely new CFG from the IR.
 */
void
cfa_buildcfg(struct ir_func *fn)
{
	int leader;
	struct cfadata *cfa;
	struct cfa_bb *curbb;
	struct ir_branch *b;
	struct ir_insn *insn;
	struct passinfo pi;

	pi.p_fn = fn;
	cfa_free(fn);
	pass_jmpopt(&pi);

	TAILQ_FOREACH(insn, &fn->if_iq, ii_link)
		insn->ii_bb = NULL;
	cfa = xmalloc(sizeof *cfa);
	mem_area_init(&cfa->c_ma);
	SIMPLEQ_INIT(&cfa->c_bbqh);
	cfa->c_nbb = cfa->c_edges = 0;
	cfa->c_entry = bballoc(cfa);
	cfa->c_exit = bballoc(cfa);
	fn->if_cfadata = cfa;
	curbb = cfa->c_entry;

	/* Mark leaders by allocating basic blocks. */
	leader = 1;
	TAILQ_FOREACH(insn, &fn->if_iq, ii_link) {
		if (leader || insn->i_op == IR_LBL) {
			if (insn->ii_bb == NULL)
				insn->ii_bb = bballoc(cfa);
			leader = 0;
		}
		if (IR_ISBRANCH(insn)) {
			b = (struct ir_branch *)insn;
			if (b->ib_lbl->ii_bb == NULL)
				b->ib_lbl->ii_bb = bballoc(cfa);
			leader = 1;
		}
		if (insn->i_op == IR_RET)
			leader = 1;
	}

	/* Find the leaders and link the basic blocks. */
	TAILQ_FOREACH(insn, &fn->if_iq, ii_link) {
		if (insn->ii_bb != NULL) {
			if (curbb != NULL)
				addsucc(cfa, curbb, insn->ii_bb);
			curbb = insn->ii_bb;
			curbb->cb_first = insn;
		}
		curbb->cb_last = insn;
		insn->ii_bb = curbb;
		if (insn->i_op == IR_RET) {
			addsucc(cfa, curbb, cfa->c_exit);
			curbb = NULL;
		}
		if (IR_ISBRANCH(insn)) {
			b = (struct ir_branch *)insn;
			if ((struct ir_insn *)b->ib_lbl !=
			    TAILQ_NEXT(insn, ii_link))
				addsucc(cfa, curbb, b->ib_lbl->ii_bb);
			if (insn->i_op == IR_B)
				curbb = NULL;
		}
	}
	insn = TAILQ_LAST(&fn->if_iq, ir_insnq);
	if (curbb != NULL && (insn == NULL || insn->i_op != IR_RET))
		addsucc(cfa, curbb, cfa->c_exit);
	if (Iflag)
		cfgdump(fn);

}

static void
cfgdump(struct ir_func *fn)
{
	size_t i;
	FILE *fp;
	struct cfadata *cfa = fn->if_cfadata;
	struct cfa_bb *curbb;
	struct cfa_bblink *bbl;

	static int dumpno;

	fp = dump_open("CFG", fn->if_sym->is_name, "w", dumpno++);
	fprintf(fp, "CFG for %s\n", fn->if_sym->is_name);
	fprintf(fp, "%d nodes, %d edges\n", cfa->c_nbb, cfa->c_edges);
	SIMPLEQ_FOREACH(curbb, &cfa->c_bbqh, cb_glolink) {
		fprintf(fp, "%d", curbb->cb_id);
		if (curbb == cfa->c_entry)
			fprintf(fp, " (entry):\n");
		else if (curbb == cfa->c_exit)
			fprintf(fp, " (exit):\n");
		else {
			fprintf(fp, ":\n\tfirst: ");
			ir_dump_insn(fp, curbb->cb_first);
			fprintf(fp, "\tlast: ");
			ir_dump_insn(fp, curbb->cb_last);
		}

		fprintf(fp, "\tpreds:");
		SIMPLEQ_FOREACH(bbl, &curbb->cb_preds, cb_link)
			fprintf(fp, " %d", bbl->cb_bb->cb_id);
		fprintf(fp, "\n\tsuccs:");
		SIMPLEQ_FOREACH(bbl, &curbb->cb_succs, cb_link)
			fprintf(fp, " %d", bbl->cb_bb->cb_id);
		fprintf(fp, "\n");
		if (curbb->cb_immdom != NULL)
			fprintf(fp, "\timmdom: %d\n", curbb->cb_immdom->cb_id);
		fprintf(fp, "\timmediately dominates:");
		SIMPLEQ_FOREACH(bbl, &curbb->cb_idomkids, cb_link)
			fprintf(fp, " %d", bbl->cb_bb->cb_id);
		fprintf(fp, "\n");
		if (curbb->cb_df != NULL) {
			fprintf(fp, "\tdominance frontier:");
			for (i = bitvec_firstset(curbb->cb_df);
			    i < curbb->cb_df->b_nbit;
			    i = bitvec_nextset(curbb->cb_df, i))
				fprintf(fp, " %zu", i);
		}
		fprintf(fp, "\n\n");
	}
	fclose(fp);
}

/*
 * Calculate the immediate dominators of a CFG. This function assumes that
 * the CFG has been constructed already.
 */
void
cfa_calcdom(struct ir_func *fn)
{
	struct cfa_bb *bb;

	SIMPLEQ_FOREACH(bb, &fn->if_cfadata->c_bbqh, cb_glolink)
		SIMPLEQ_INIT(&bb->cb_idomkids);

	if (0)
		calcdom_simple(fn);
	else
		calcdom_lentar(fn);
}

static void
setidom(struct cfadata *cfa, struct cfa_bb *bb, struct cfa_bb *idom)
{
	struct cfa_bblink *idomlink;

	idomlink = mem_alloc(&cfa->c_ma, sizeof *idomlink);
	idomlink->cb_bb = bb;
	SIMPLEQ_INSERT_TAIL(&idom->cb_idomkids, idomlink, cb_link);
	bb->cb_immdom = idom;
}

/*
 * Simple iterative algorithm to calculate (immediate) dominators, as
 * described in Steven S. Muchnick: Advanced Compiler Design & Implementation,
 * chapter 7.3.
 */
static void
calcdom_simple(struct ir_func *fn)
{
	int changes, i;
	size_t first, j, k;
	struct bitvec **domin;
	struct bitvec *t;
	struct cfadata *cfa = fn->if_cfadata;
	struct cfa_bb **bbs, **dfsbbs;
	struct cfa_bb *n;
	struct cfa_bblink *p;

	cfa_cfgsort(fn, CFA_CFGSORT_ASC);
	bbs = xmnalloc(cfa->c_nbb, sizeof *bbs);
	dfsbbs = xcalloc(cfa->c_nbb, sizeof *dfsbbs);
	SIMPLEQ_FOREACH(n, &cfa->c_bbqh, cb_glolink) {
		bbs[n->cb_id] = n;
		dfsbbs[n->cb_dfsno] = n;
	}

	/* Calculate the dominators of each block n. */
	domin = xmnalloc(cfa->c_nbb, sizeof *domin);
	t = bitvec_alloc(NULL, cfa->c_nbb);
	for (i = 0; i < cfa->c_nbb; i++) {
		domin[i] = bitvec_alloc(NULL, cfa->c_nbb);
		if (cfa->c_entry->cb_id == i)
			bitvec_setbit(domin[i], i);
		else
			bitvec_setall(domin[i]);
	}
	do {
		changes = 0;
		for (i = 0; i < cfa->c_nbb; i++) {
			if ((n = dfsbbs[i]) == NULL)
				break;
			if (n == cfa->c_entry)
				continue;
			bitvec_setall(t);
			SIMPLEQ_FOREACH(p, &n->cb_preds, cb_link)
				bitvec_and(t, domin[p->cb_bb->cb_id]);
			bitvec_setbit(t, n->cb_id);
			if (bitvec_cmp(t, domin[n->cb_id]) != 0) {
				changes = 1;
				bitvec_cpy(domin[n->cb_id], t);
			}
		}
	} while (changes);

	/* Calculate the immediate dominator of each block n. */
	for (i = 0; i < cfa->c_nbb; i++)
		bitvec_clearbit(domin[i], i);
	for (i = 0; i < cfa->c_nbb; i++) {
		if ((n = dfsbbs[i]) == NULL)
			break;
		if (n == cfa->c_entry)
			continue;
		for (j = first = bitvec_firstset(domin[n->cb_id]);
		    j < domin[n->cb_id]->b_nbit;
		    j = bitvec_nextset(domin[n->cb_id], j)) {
			for (k = first; k < j;
			    k = bitvec_nextset(domin[n->cb_id], k)) {
				if (bitvec_isset(domin[j], k))
					bitvec_clearbit(domin[n->cb_id], k);
			}
		}
	}
	for (i = 0; i < cfa->c_nbb; i++) {
		if ((j = bitvec_firstset(domin[i])) >= domin[i]->b_nbit) {
			bbs[i]->cb_immdom = NULL;
			if (bbs[i] != cfa->c_entry)
				fatalx("%d has no immdom", i);
		} else
			setidom(cfa, bbs[i], bbs[j]);
	}

	for (i = 0; i < cfa->c_nbb; i++)
		free(domin[i]);
	free(domin);
	free(t);
	free(bbs);
	free(dfsbbs);

	if (Iflag)
		cfgdump(fn);
}

struct lentar {
	struct	cfa_bb *l_bucket;
	int	l_label;
	int	l_ancestor;
	int	l_child;
	int	l_parent;
	int	l_size;
	int	l_sdno;
};

static int
lentar_dfs(struct cfadata *cfa, struct lentar *data, struct cfa_bb **bbs,
    int *ndfs, struct cfa_bb *v, int n)
{
	struct cfa_bb *w;
	struct cfa_bblink *bbl;	

	/*
	 * Note that it's ok here to assign the entry block a sdno of
	 * 0. This block has no precedessors, so we won't encounter
	 * it in the loop below.
	 */
	data[v->cb_id].l_sdno = n;
	ndfs[n] = data[v->cb_id].l_label = v->cb_id;
	bbs[v->cb_id] = v;
	data[v->cb_id].l_ancestor = data[v->cb_id].l_child = cfa->c_nbb;
	data[v->cb_id].l_size = 1;

	SIMPLEQ_FOREACH(bbl, &v->cb_succs, cb_link) {
		w = bbl->cb_bb;
		if (data[w->cb_id].l_sdno == 0) {
			data[w->cb_id].l_parent = v->cb_id;
			n = lentar_dfs(cfa, data, bbs, ndfs, w, n + 1);
		}
	}
	return n;
}

static void
lentar_compress(struct lentar *data, int v, int n0)
{
	int anc, lblanc;

	anc = data[v].l_ancestor;
	if (data[anc].l_ancestor != n0) {
		lentar_compress(data, anc, n0);
		lblanc = data[anc].l_label;
		if (data[lblanc].l_sdno < data[data[v].l_label].l_sdno)
			data[v].l_label = lblanc;
		data[v].l_ancestor = data[anc].l_ancestor;
	}
}

static int
lentar_eval(struct lentar *data, struct cfa_bb *v, int n0)
{
	int lblanc, lblv;

	if (data[v->cb_id].l_ancestor == n0)
		return data[v->cb_id].l_label;
	lentar_compress(data, v->cb_id, n0);
	lblanc = data[data[v->cb_id].l_ancestor].l_label;
	lblv = data[v->cb_id].l_label;
	if (data[lblanc].l_sdno >= data[lblv].l_sdno)
		return lblv;
	return lblanc;
}

static void
lentar_link(struct lentar *data, int v, int w, int n0)
{
	int cs, ccs, s = w, tmp;
	int wsdno;

	wsdno = data[data[w].l_label].l_sdno;
	while (wsdno < data[data[data[s].l_child].l_label].l_sdno) {
		cs = data[s].l_child;
		ccs = data[cs].l_child;
		if (data[s].l_size + data[ccs].l_size >= 2 * data[cs].l_size) {
			data[cs].l_ancestor = s;
			data[s].l_child = ccs;
		} else {
			data[cs].l_size = data[s].l_size;
			s = data[s].l_ancestor = cs;
		}
	}
	data[s].l_label = data[w].l_label;
	data[v].l_size += data[w].l_size;
	if (data[v].l_size < 2 * data[w].l_size) {
		tmp = s;
		s = data[v].l_child;
		data[v].l_child = tmp;
	}
	while (s != n0) {
		data[s].l_ancestor = v;
		s = data[s].l_child;
	}
}

/*
 * Fast algorithm to calculate (immediate) dominators, as described in
 * Thomas Lengauer and Robert Endre Tarjan: A Fast Algorithm for Finding
 * Dominators in a Flow Graph.
 * The actual implementation is adapted from Muchnick.
 */
static void
calcdom_lentar(struct ir_func *fn)
{
	int i, n;
	int *ndfs;
	int u, w;
	struct cfadata *cfa;
	struct lentar *data, *buck, *n0;
	struct cfa_bb **bbs, *v;
	struct cfa_bblink *bbl;

	cfa = fn->if_cfadata;
	data = xcalloc(cfa->c_nbb + 1, sizeof *data);
	ndfs = xmnalloc(cfa->c_nbb + 1, sizeof *ndfs);
	bbs = xmnalloc(cfa->c_nbb, sizeof *bbs);

	n0 = &data[cfa->c_nbb];
	n0->l_ancestor = n0->l_label = cfa->c_nbb;

	n = lentar_dfs(cfa, data, bbs, ndfs, cfa->c_entry, 1);
	for (i = n; i >= 2; i--) {
		w = ndfs[i];
		SIMPLEQ_FOREACH(bbl, &bbs[w]->cb_preds, cb_link) {
			v = bbl->cb_bb;
			u = lentar_eval(data, v, cfa->c_nbb);
			if (data[u].l_sdno < data[w].l_sdno)
				data[w].l_sdno = data[u].l_sdno;
		}

		/*
		 * XXX: Abuses immdom field, does not calculate the
		 * immdom yet.
		 */
		buck = &data[ndfs[data[w].l_sdno]];
		bbs[w]->cb_immdom = buck->l_bucket;
		buck->l_bucket = bbs[w];

		lentar_link(data, data[w].l_parent, w, cfa->c_nbb);

		buck = &data[data[w].l_parent];
		while (buck->l_bucket != NULL) {
			v = buck->l_bucket;
			buck->l_bucket = v->cb_immdom;
			u = lentar_eval(data, v, cfa->c_nbb);
			if (data[u].l_sdno < data[v->cb_id].l_sdno)
				v->cb_immdom = bbs[u];
			else
				v->cb_immdom = bbs[data[w].l_parent];
		}
	}
	cfa->c_entry->cb_immdom = NULL;
	for (i = 2; i <= n; i++) {
		w = ndfs[i];
		if (bbs[w]->cb_immdom != bbs[ndfs[data[w].l_sdno]])
			setidom(cfa, bbs[w], bbs[w]->cb_immdom->cb_immdom);
		else
			setidom(cfa, bbs[w], bbs[w]->cb_immdom);
	}

	free(data);
	free(ndfs);
	free(bbs);
}

/*
 * Calculate dominance frontiers. See Steven S. Muchnick: Advanced Compiler
 * Design & Implementation, chapter 8.11 for
 * the algorithm.
 *
 * This function assumes that the immediate dominators have been computed.
 */
void
cfa_calcdf(struct ir_func *fn)
{
	int i;
	size_t j;
	struct cfadata *cfa = fn->if_cfadata;
	struct cfa_bb *bb, **bbs, **dfsbbs;
	struct cfa_bblink *y, *z;

	cfa_cfgsort(fn, CFA_CFGSORT_DESC);
	dfsbbs = xcalloc(cfa->c_nbb, sizeof *dfsbbs);
	bbs = xmnalloc(cfa->c_nbb, sizeof *bbs);
	SIMPLEQ_FOREACH(bb, &cfa->c_bbqh, cb_glolink) {
		dfsbbs[bb->cb_dfsno] = bb;
		bbs[bb->cb_id] = bb;
	}

	for (i = 0; i < cfa->c_nbb; i++) {
		if (dfsbbs[i] == NULL) {
			fprintf(stderr, "breaking at %d\n", i);
			break;
		}
		dfsbbs[i]->cb_df = bitvec_alloc(&cfa->c_ma, cfa->c_nbb);

		/* Compute DF_local(x). */
		SIMPLEQ_FOREACH(y, &dfsbbs[i]->cb_succs, cb_link) {
			if (y->cb_bb->cb_immdom != dfsbbs[i])
				bitvec_setbit(dfsbbs[i]->cb_df,
				    y->cb_bb->cb_id);
		}

		/* Union with DF_up(x, z). */
		SIMPLEQ_FOREACH(z, &dfsbbs[i]->cb_idomkids, cb_link) {
			for (j = bitvec_firstset(z->cb_bb->cb_df);
			    j < z->cb_bb->cb_df->b_nbit;
			    j = bitvec_nextset(z->cb_bb->cb_df, j)) {
				if (bbs[j]->cb_immdom != dfsbbs[i])
					bitvec_setbit(dfsbbs[i]->cb_df, j);
			}
		}
	}

	free(dfsbbs);
	free(bbs);

	if (Iflag)
		cfgdump(fn);
}

void
cfa_bb_prepend(struct ir_func *fn, struct cfa_bb *bb, struct ir_insn *insn)
{
	insn->ii_bb = bb;
	if (bb->cb_first != NULL) {
		if (bb->cb_first->i_op == IR_LBL)
			TAILQ_INSERT_AFTER(&fn->if_iq, bb->cb_first, insn,
			    ii_link);
		else {
			TAILQ_INSERT_BEFORE(bb->cb_first, insn, ii_link);
			bb->cb_first = insn;
		}
	} else if (fn->if_cfadata->c_entry == bb) {
		TAILQ_INSERT_HEAD(&fn->if_iq, insn, ii_link);
		bb->cb_first = bb->cb_last = insn;
	} else if (fn->if_cfadata->c_exit == bb) {
		TAILQ_INSERT_TAIL(&fn->if_iq, insn, ii_link);
		bb->cb_first = bb->cb_last = insn;
	} else
		fatalx("cfa_bb_prepend block %d", bb->cb_id);
}

void
cfa_bb_append(struct ir_func *fn, struct cfa_bb *bb, struct ir_insn *insn)
{
	insn->ii_bb = bb;
	if (bb->cb_last != NULL) {
		if (IR_ISBRANCH(bb->cb_last))
			TAILQ_INSERT_BEFORE(bb->cb_last, insn, ii_link);
		else {
			TAILQ_INSERT_AFTER(&fn->if_iq, bb->cb_last, insn,
			    ii_link);
			bb->cb_last = insn;
		}
	} else if (fn->if_cfadata->c_entry == bb) {
		TAILQ_INSERT_HEAD(&fn->if_iq, insn, ii_link);
		bb->cb_first = bb->cb_last = insn;
	} else if (fn->if_cfadata->c_exit == bb) {
		TAILQ_INSERT_TAIL(&fn->if_iq, insn, ii_link);
		bb->cb_first = bb->cb_last = insn;
	} else
		fatalx("cfa_bb_prepend block %d", bb->cb_id);
}

void
cfa_bb_delinsn(struct ir_func *fn, struct cfa_bb *bb, struct ir_insn *insn)
{
	if (insn->ii_bb != bb)
		fatalx("cfa_bb_delinsn");
	if (bb->cb_first == insn) {
		if (insn == bb->cb_last)
			bb->cb_first = bb->cb_last = NULL;
		else if ((bb->cb_first = TAILQ_NEXT(insn, ii_link)) == NULL)
			bb->cb_last = NULL;
	} else if (bb->cb_last == insn) {
		if (insn == bb->cb_first)
			bb->cb_first = bb->cb_last = NULL;
		else if ((bb->cb_last = TAILQ_PREV(insn, ir_insnq,
		    ii_link)) == NULL)
			bb->cb_first = NULL;
	}
	ir_delete_insn(fn, insn);
}

void
cfa_bb_prepend_insn(struct ir_insn *old, struct ir_insn *insn)
{
	struct ir_insn *first;

	if ((first = old->ii_bb->cb_first) == NULL || first == old)
		old->ii_bb->cb_first = insn;
	if (old->ii_bb->cb_last == NULL)
		old->ii_bb->cb_last = insn;
	insn->ii_bb = old->ii_bb;
	ir_prepend_insn(old, insn);
}

void
cfa_bb_append_insn(struct ir_func *fn, struct ir_insn *old,
    struct ir_insn *insn)
{
	struct ir_insn *last;

	if ((last = old->ii_bb->cb_last) == NULL || last == old)
		old->ii_bb->cb_last = insn;
	if (old->ii_bb->cb_first == NULL)
		old->ii_bb->cb_first = insn;
	insn->ii_bb = old->ii_bb;
	ir_append_insn(fn, old, insn);
}

void
cfa_cfgsort(struct ir_func *fn, int how)
{
	int start;
	struct cfadata *cfa = fn->if_cfadata;
	struct cfa_bb *bb;

	SIMPLEQ_FOREACH(bb, &cfa->c_bbqh, cb_glolink)
		bb->cb_dfsno = -1;
	if (how == CFA_CFGSORT_ASC)
		start = 0;
	else
		start = cfa->c_nbb - 1;
	cfg_dfs(cfa->c_entry, start, how);
}

static int
cfg_dfs(struct cfa_bb *bb, int no, int inc)
{
	struct cfa_bblink *bbl;

	bb->cb_dfsno = no;
	no += inc;
	SIMPLEQ_FOREACH(bbl, &bb->cb_succs, cb_link) {
		if (bbl->cb_bb->cb_dfsno == -1)
			no = cfg_dfs(bbl->cb_bb, no, inc);
	}
	return no;
}

void
cfa_free(struct ir_func *fn)
{
	if (fn->if_cfadata != NULL) {
		mem_area_free(&fn->if_cfadata->c_ma);
		free(fn->if_cfadata);
		fn->if_cfadata = NULL;
	}
}

static struct cfa_bb *
bballoc(struct cfadata *cfa)
{
	struct cfa_bb *bb;

	bb = mem_alloc(&cfa->c_ma, sizeof *bb);
	SIMPLEQ_INIT(&bb->cb_preds);
	SIMPLEQ_INIT(&bb->cb_succs);
	SIMPLEQ_INIT(&bb->cb_idomkids);
	bb->cb_immdom = NULL;
	bb->cb_id = cfa->c_nbb++;
	bb->cb_first = bb->cb_last = NULL;
	bb->cb_df = NULL;
	SIMPLEQ_INSERT_TAIL(&cfa->c_bbqh, bb, cb_glolink);
	return bb;
}

static void
addsucc(struct cfadata *cfa, struct cfa_bb *pred, struct cfa_bb *succ)
{
	struct cfa_bblink *predlink, *succlink;

	predlink = mem_alloc(&cfa->c_ma, sizeof *predlink);
	succlink = mem_alloc(&cfa->c_ma, sizeof *succlink);
	predlink->cb_bb = pred;
	succlink->cb_bb = succ;
	cfa->c_edges++;
	SIMPLEQ_INSERT_TAIL(&pred->cb_succs, succlink, cb_link);
	SIMPLEQ_INSERT_TAIL(&succ->cb_preds, predlink, cb_link);
}
