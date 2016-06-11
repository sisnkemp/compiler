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
 * The implementation of the register allocator is based on these two papers:
 * Lal George and Adrew W. Appel: Iterated Register Coalescing.
 * and
 * Johan Runeson and Sven-Olof Nystrom: Retargetable Graph-Coloring Register
 * Allocation for Irregular Architectures.
 *
 * For a general treatment of register allocation via graph-coloring, see:
 * Steven S. Muchnik: Advanced Compiler Design & Implementation.
 *
 * TODO: Calculate spill costs and make select_spill() smarter.
 */

#include <sys/types.h>
#include <sys/queue.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "comp/comp.h"
#include "comp/ir.h"
#include "comp/passes.h"

static struct memarea mem;
static struct ir_func *curfn;
FILE *dumpfp;

#define ROUNDS_MAX	8

static int nrounds;
static int nconsround;
static int nvreg = -1;
static int vregs[REG_NREGS];

struct move {
	LIST_ENTRY(move) m_wlnext;
	struct	ir_insn *m_insn;
	struct	move *m_top;
	short	m_wl;
};

struct movelink {
	SLIST_ENTRY(movelink) m_link;
	struct	move *m_move;
	struct	movelink *m_top;
};

static struct movelink *allmovelinks;
static struct movelink *freemovelinks;

#define ADDMOVEWL(wl, m) do {					\
	LIST_INSERT_HEAD(&movelists[(wl)], m, m_wlnext);	\
	(m)->m_wl = (wl);					\
} while (0)

#define ADDWLMOVES(m) do {		\
	ADDMOVEWL(ML_WORKLIST, m);	\
} while (0)

#define ADDACTMOVES(m) do {		\
	ADDMOVEWL(ML_ACTIVE, m);	\
} while (0)

#define DELWLMOVES(m) do {		\
	LIST_REMOVE(m, m_wlnext);	\
} while (0)

#define DEQWLMOVES(m) do {			\
	(m) = LIST_FIRST(&worklist_moves);	\
	DELWLMOVES(m);				\
} while (0)

#define DELACTMOVES(m) do {		\
	LIST_REMOVE(m, m_wlnext);	\
} while (0)

static struct move *allmoves;
static struct move *freemoves;

LIST_HEAD(movelist, move);

#define ML_COALESCED	0
#define ML_CONSTRAINED	1
#define ML_FROZEN	2
#define ML_WORKLIST	3
#define ML_ACTIVE	4

static struct movelist movelists[ML_ACTIVE + 1];

#define coalesced_moves		movelists[ML_COALESCED]
#define constrained_moves	movelists[ML_CONSTRAINED]
#define frozen_moves		movelists[ML_FROZEN]
#define worklist_moves		movelists[ML_WORKLIST]
#define active_moves		movelists[ML_ACTIVE]

TAILQ_HEAD(nodelist, node);

struct node {
	TAILQ_ENTRY(node) n_link;
	SLIST_HEAD(, movelink) n_moves;
	struct	ir_symbol *n_sym;
	struct	node *n_alias;
	int	n_color;
	int	n_degree;
	int	n_consround;
	short	n_rclass;
	short	n_flags;
	int	n_wl;
};

#define N_SPILLNODE	1
#define N_COALNODE	2

#define ADDNODEWL(wl, n) do {				\
	TAILQ_INSERT_TAIL(&nodelists[(wl)], n, n_link);	\
	(n)->n_wl = (wl);				\
} while (0)

#define DELNODEWL(wl, n) do {				\
	TAILQ_REMOVE(&nodelists[(wl)], n, n_link);	\
} while (0)

#define PUSH(n) do {					\
	TAILQ_INSERT_HEAD(&select_stack, n, n_link);	\
	(n)->n_wl = NL_SELSTACK;			\
} while (0)

#define DEQNODEWL(wl, n) do {				\
	(n) = TAILQ_FIRST(&nodelists[(wl)]);		\
	TAILQ_REMOVE(&nodelists[(wl)], n, n_link);	\
} while (0)

#define NL_INITIAL	0
#define NL_PRECOLORED	1
#define NL_SIMPLIFYWL	2
#define NL_FREEZEWL	3
#define NL_SPILLWL	4
#define NL_SPILLEDNODES	5
#define NL_COLOREDNODES	6
#define NL_COALNODES	7
#define NL_SELSTACK	8

static struct nodelist nodelists[NL_SELSTACK + 1];

#define initial		nodelists[NL_INITIAL]
#define precolored	nodelists[NL_PRECOLORED]
#define simplifywl	nodelists[NL_SIMPLIFYWL]
#define freezewl	nodelists[NL_FREEZEWL]
#define spillwl		nodelists[NL_SPILLWL]
#define spilled_nodes	nodelists[NL_SPILLEDNODES]
#define colored_nodes	nodelists[NL_COLOREDNODES]
#define coalesced_nodes	nodelists[NL_COALNODES]
#define select_stack	nodelists[NL_SELSTACK]

SLIST_HEAD(adjlist, adjelem);

struct adjelem {
	SLIST_ENTRY(adjelem) a_link;
	struct	adjelem *a_top;
	struct	node *a_node;
};

static struct adjelem *freeadjelems;
static struct adjelem *alladjelems;

#define AMROWCOLBIT(r, c)	((((r) * ((r) + 1)) >> 1) + (c))
#define AMELMBIT(i, j)						\
	((i) > (j) ? AMROWCOLBIT(i, j) : AMROWCOLBIT(j, i))
#define AMELMBYTE(i, j)		(AMELMBIT(i, j) >> 3)
#define AMELMBITOFF(i, j)	(AMELMBIT(i, j) & 7)

#define AMGET(m, i, j)	((m)[AMELMBYTE(i, j)] & (1 << AMELMBITOFF(i, j)))
#define AMSET(m, i, j) do {				\
	(m)[AMELMBYTE(i, j)] |= 1 << AMELMBITOFF(i, j);	\
} while (0)

static uint8_t *adjmatrix;
static size_t amsize;
static struct adjlist *adjlists;

#define PRECOLOR_CALLARGS	0

static void
printnodewl(struct nodelist *nl, const char *name)
{
	struct node *n;

	fprintf(dumpfp, "on %s=%p:\n", name, nl);
	TAILQ_FOREACH(n, nl, n_link) {
		fprintf(dumpfp, " ");
		ir_dump_symbol(dumpfp, n->n_sym);
	}
	fprintf(dumpfp, "\n");
}

static void
printworklists(const char *after)
{
	fprintf(dumpfp, "\nworklists after %s\n", after);
	printnodewl(&initial, "initial");
	printnodewl(&precolored, "precolored");
	printnodewl(&simplifywl, "simplifywl");
	printnodewl(&freezewl, "freezewl");
	printnodewl(&spillwl, "spillwl");
	printnodewl(&spilled_nodes, "spilled_nodes");
	printnodewl(&colored_nodes, "colored_nodes");
	printnodewl(&coalesced_nodes, "coalesced_nodes");
	printnodewl(&select_stack, "select_stack");
	fflush(dumpfp);
}

static void
printgraph(void)
{
	size_t i;
	struct adjelem *edge;
	struct ir_symbol *sym;

	fprintf(dumpfp, "resulting graph by walking the adjacency lists\n");
	for (i = 0; i < amsize; i++) {
		sym = curfn->if_regs[i];
		if (sym == NULL || sym->is_id < REG_NREGS)
			continue;
		ir_dump_symbol(dumpfp, sym);
		fprintf(dumpfp, ", degree %d:", sym->is_node->n_degree);
		SLIST_FOREACH(edge, &adjlists[sym->is_id - REG_NREGS],
		    a_link) {
			if (edge->a_node->n_wl == NL_SELSTACK ||
			    edge->a_node->n_wl == NL_COALNODES)
				continue;
			if (edge->a_node->n_sym->is_id < REG_NREGS &&
			    sym->is_id < REG_NREGS)
				continue;
			fprintf(dumpfp, " ");
			ir_dump_symbol(dumpfp, edge->a_node->n_sym);
		}
		fprintf(dumpfp, "\n");
	}
}

static void
getadjelems(struct adjelem **elems, int n)
{
	int i, j;
	struct adjelem *tmp;

	for (i = 0; i < n && freeadjelems != NULL; i++) {
		elems[i] = freeadjelems;
		freeadjelems = freeadjelems->a_top;
	}
	if (i == n)
		return;
	tmp = mem_mnalloc(&mem, n - i, sizeof *tmp);
	for (j = 0; i < n; i++, j++) {
		tmp[j].a_top = alladjelems;
		elems[i] = alladjelems = &tmp[j];
	}
}

static void
getmovelinks(struct movelink **ml, int n)
{
	int i, j;
	struct movelink *tmp;

	for (i = 0; i < n && freemovelinks; i++) {
		ml[i] = freemovelinks;
		freemovelinks = freemovelinks->m_top;
	}
	if (i == n)
		return;
	tmp = mem_mnalloc(&mem, n - i, sizeof *tmp);
	for (j = 0; i < n; i++, j++) {
		ml[i] = &tmp[j];
		tmp[j].m_top = allmovelinks;
		allmovelinks = &tmp[j];
	}
}

static struct ir_symbol *
newnode(struct ir_symbol *osym)
{
	struct ir_symbol *sym;
	struct node *n;

	sym = ir_vregsym(curfn, osym->is_type);
	n = mem_alloc(&mem, sizeof *n);
	SLIST_INIT(&n->n_moves);
	ADDNODEWL(NL_INITIAL, n);
	n->n_sym = sym;
	sym->is_node = n;
	n->n_degree = 0;
	n->n_rclass = osym->is_node->n_rclass;
	n->n_flags = N_SPILLNODE;
	return sym;
}

/*
 * Add node v to the adjacency list of node u.
 */
static void
addtolist(struct node *u, struct node *v, struct adjlist *l,
    struct adjelem *elem)
{
	elem->a_node = v;
	SLIST_INSERT_HEAD(l, elem, a_link);
	u->n_degree += reg_qbc[u->n_rclass][v->n_rclass];
}

static struct node *
getalias(struct node *n)
{
	while (n->n_wl == NL_COALNODES)
		n = n->n_alias;
	return n;
}

void
pass_ralloc_precolor(struct ir_symbol *sym, int c)
{
	struct node *n;

	n = sym->is_node;
	if (n->n_wl == NL_PRECOLORED && n->n_color != c)
		fatalx("pass_ralloc_precolor");
	n->n_color = c;
	DELNODEWL(n->n_wl, n);
	ADDNODEWL(NL_PRECOLORED, n);
}

void
pass_ralloc_addedge(struct ir_func *fn, size_t i, size_t j)
{
	struct adjelem *elems[2];
	struct node *u, *v;

	if (i == j || AMGET(adjmatrix, i, j))
		return;
	u = fn->if_regs[i]->is_node;
	v = fn->if_regs[j]->is_node;
	if (reg_qbc[u->n_rclass][v->n_rclass] == 0)
		return;

#if !PRECOLOR_CALLARGS
	if ((u->n_wl == NL_PRECOLORED || v->n_wl == NL_PRECOLORED) &&
	    u->n_rclass != v->n_rclass)
		return;
#endif

	AMSET(adjmatrix, i, j);
	if (u->n_wl == NL_PRECOLORED && v->n_wl == NL_PRECOLORED)
		return;

	if (u->n_wl != NL_PRECOLORED && v->n_wl != NL_PRECOLORED)
		getadjelems(elems, 2);
	else if (u->n_wl != NL_PRECOLORED)
		getadjelems(&elems[0], 1);
	else
		getadjelems(&elems[1], 1);
	if (u->n_wl != NL_PRECOLORED)
		addtolist(u, v, &adjlists[i - REG_NREGS], elems[0]);
	if (v->n_wl != NL_PRECOLORED)
		addtolist(v, u, &adjlists[j - REG_NREGS], elems[1]);
}

static void
interfere(struct ir_func *fn, int reg, struct bitvec *live)
{
	size_t i;

	for (i = bitvec_firstset(live); i < live->b_nbit;
	    i = bitvec_nextset(live, i))
		pass_ralloc_addedge(fn, reg, i);
}

static void
node_addmove(struct ir_insn *insn)
{
	struct move *m;
	struct movelink *ml[2];
	struct node *u, *v;

	if (freemoves != NULL) {
		m = freemoves;
		freemoves = freemoves->m_top;
	} else
		m = mem_alloc(&mem, sizeof *m);
	u = insn->is_l->ie_sym->is_node;
	v = insn->is_r->ie_sym->is_node;
	m->m_insn = insn;
	ADDWLMOVES(m);
	getmovelinks(ml, 2);
	ml[0]->m_move = ml[1]->m_move = m;
	SLIST_INSERT_HEAD(&u->n_moves, ml[0], m_link);
	SLIST_INSERT_HEAD(&v->n_moves, ml[1], m_link);
}

static void
addrc(struct ir_symbol *sym, struct regconstr *rc)
{
	int id, i, j;

	id = sym->is_id;
	if (rc->r_how == RC_ALLOWED) {
		for (i = 0; i < REG_NREGS; i++) {
			for (j = 0; rc->r_regs[j] != -1; j++) {
				if (i == rc->r_regs[j])
					goto out;
			}

			pass_ralloc_addedge(curfn, i, id);
out:
			continue;
		}
	} else {
		for (i = 0; rc->r_regs[i] != -1; i++)
			pass_ralloc_addedge(curfn, i, id);
	}
}

static void
addedges(struct ir_expr *x)
{
	struct regconstr *rc;

	for (;;) {
		if (IR_ISBINEXPR(x)) {
			addedges(x->ie_r);
			x = x->ie_l;
		} else if (IR_ISUNEXPR(x))
			x = x->ie_l;
		else if (x->i_op == IR_REG && x->ie_sym->is_id >= REG_NREGS) {
			if ((rc = x->ie_regs) == NULL)
				break;
			addrc(x->ie_sym, rc);
			break;
		} else
			break;
	}
}

static void
addtmp(struct ir *ir, struct bitvec *live)
{
	int i, j;
	struct ir_symbol **sym;

	sym = ir->i_tmpregsyms;
	for (i = 0; ir->i_tmpregs[i] != NULL; i++) {
		for (j = 0; j < i; j++)
			pass_ralloc_addedge(curfn, sym[i]->is_id,
			    sym[j]->is_id);
		if (live != NULL)
			interfere(curfn, sym[i]->is_id, live);
		addrc(sym[i], ir->i_tmpregs[i]->t_rc);
	}
}

static void
addtmp_expr(struct ir_expr *x, struct bitvec *live)
{
	for (;;) {
		if (x->i_tmpregs != NULL)
			addtmp((struct ir *)x, live);
		if (IR_ISBINEXPR(x)) {
			addtmp_expr(x->ie_r, live);
			x = x->ie_l;
		} else if (IR_ISUNEXPR(x))
			x = x->ie_l;
		else
			break;
	}
}

static void
build(struct ir_func *fn)
{
	int retreg;
	size_t i;
	struct bitvec *live;
	struct ir_insn *insn;
	struct ir_symbol *sym;

	TAILQ_FOREACH(insn, &fn->if_iq, ii_link) {
		live = insn->ii_dfadata.d_liveout;
		if (insn->i_tmpregs != NULL)
			addtmp((struct ir *)insn, live);
		if (insn->i_op == IR_LBL || insn->i_op == IR_B)
			continue;
		if (IR_ISBRANCH(insn)) {
			addedges(insn->ib_l);
			addedges(insn->ib_r);
			addtmp_expr(insn->ib_l, live);
			addtmp_expr(insn->ib_r, live);
			continue;
		}
		switch (insn->i_op) {
		case IR_ASG:
			addedges(insn->is_l);
			addedges(insn->is_r);
			if (live == NULL || insn->is_l->i_op != IR_REG)
				break;
			if (insn->is_r->i_op == IR_REG) {
				sym = insn->is_r->ie_sym;
				if (insn->is_l->ie_sym == sym) {
					ir_delete_insn(fn, insn);
					break;
				}
				bitvec_clearbit(live, sym->is_id);
				node_addmove(insn);
			}
			interfere(fn, insn->is_l->ie_sym->is_id, live);
			addtmp_expr(insn->is_l, live);
			addtmp_expr(insn->is_r, live);
			break;
		case IR_ST:
			addedges(insn->is_l);
			addedges(insn->is_r);
			addtmp_expr(insn->is_l, live);
			addtmp_expr(insn->is_r, live);
			break;
		case IR_CALL:
			/* Force arguments into proper registers. */
#if !PRECOLOR_CALLARGS
			pass_ralloc_callargs(fn, insn);
#endif
			if (live == NULL)
				break;

			if (insn->ic_ret != NULL &&
			    insn->ic_ret->i_op == IR_REG) {
				retreg = pass_ralloc_retreg(
				    insn->ic_ret->ie_type);
				interfere(fn, insn->ic_ret->ie_sym->is_id,
				    live);
			} else
				retreg = REG_NREGS;
			if (insn->ic_fn->is_op == IR_REGSYM)
				interfere(fn, insn->ic_fn->is_id, live);

			/*
			 * Variables that are live across the call should not
			 * be assigned to volatile registers, but make sure
			 * that the register used as the return value does not
			 * interfere with the physical return register.
			 */
			for (i = 0; i < nvreg; i++) {
				if (vregs[i] != retreg)
					interfere(fn, vregs[i], live);
			}
			if (retreg != REG_NREGS) {
				bitvec_clearbit(live,
				    insn->ic_ret->ie_sym->is_id);
				interfere(fn, retreg, live);
			}
			break;
		case IR_RET:
			if (insn->ir_retexpr == NULL)
				break;	
			insn->ir_retexpr->ie_sym->is_node->n_color =
			    pass_ralloc_retreg(insn->ir_retexpr->ie_type);
			break;
		}
	}

	if (Iflag)
		printgraph();

}

static int
move_related(struct node *n)
{
	struct move *m;
	struct movelink *ml;

	while (!SLIST_EMPTY(&n->n_moves)) {
		ml = SLIST_FIRST(&n->n_moves);
		m = ml->m_move;
		if (m->m_wl == ML_ACTIVE || m->m_wl == ML_WORKLIST)
			return 1;
		SLIST_REMOVE_HEAD(&n->n_moves, m_link);
	}
	return 0;
}

static void
mkworklist(void)
{
	struct node *n;

	while (!TAILQ_EMPTY(&initial)) {
		DEQNODEWL(NL_INITIAL, n);
		if (n->n_degree >= reg_pb[n->n_rclass])
			ADDNODEWL(NL_SPILLWL, n);
		else if (move_related(n))
			ADDNODEWL(NL_FREEZEWL, n);
		else
			ADDNODEWL(NL_SIMPLIFYWL, n);
	}

	if (Iflag)
		printworklists("mkworklist");
}

static void
do_enable_moves(struct node *n)
{
	struct move *m;
	struct movelink *ml, *mnext, *prev;

	prev = NULL;
	for (ml = SLIST_FIRST(&n->n_moves); ml != NULL; ml = mnext) {
		m = ml->m_move;
		mnext = SLIST_NEXT(ml, m_link);
		if (m->m_wl == ML_ACTIVE) {
			DELACTMOVES(m);
			ADDWLMOVES(m);
		} else if (m->m_wl != ML_WORKLIST) {
			continue;
			if (prev == NULL)
				SLIST_REMOVE_HEAD(&n->n_moves, m_link);
			else
				SLIST_REMOVE_AFTER(prev, m_link);
		} else
			prev = ml;
	}
}

static void
enable_moves(struct node *t)
{
	struct adjelem *edge;
	struct node *n;

	do_enable_moves(t);
	SLIST_FOREACH(edge, &adjlists[t->n_sym->is_id - REG_NREGS], a_link) {
		n = edge->a_node;
		if (n->n_wl == NL_SELSTACK || n->n_wl == NL_COALNODES)
			continue;
		do_enable_moves(n);
	}
}

/*
 * Decrement degree of node m, which is adjacent to node n.
 */
static void
decrement_degree(struct node *m, struct node *n)
{
	int odeg = m->n_degree;

	m->n_degree -= reg_qbc[m->n_rclass][n->n_rclass];
	if (odeg >= reg_pb[m->n_rclass] && m->n_degree < reg_pb[m->n_rclass]) {
#if 0
	if (odeg > m->n_degree && odeg == reg_pb[m->n_rclass]) {
#endif
		enable_moves(m);
		if (m->n_wl != NL_SPILLWL)
			fatalx("decrement_degree: %p=%d on wl %d",
			    m, m->n_sym->is_id, m->n_wl);
		DELNODEWL(NL_SPILLWL, m);
		if (move_related(m))
			ADDNODEWL(NL_FREEZEWL, m);
		else
			ADDNODEWL(NL_SIMPLIFYWL, m);
	}
}

static void
simplify(void)
{
	struct adjelem *edge;
	struct node *m, *n;

	if (Iflag)
		fprintf(dumpfp, "simplify\n");
	DEQNODEWL(NL_SIMPLIFYWL, n);
	PUSH(n);
	SLIST_FOREACH(edge, &adjlists[n->n_sym->is_id - REG_NREGS], a_link) {
		m = edge->a_node;
		if (m->n_wl == NL_SELSTACK || m->n_wl == NL_COALNODES)
			continue;
		decrement_degree(m, n);
	}
	if (Iflag)
		printworklists("simplify");
}

static void
addworklist(struct node *u)
{
	if (u->n_wl != NL_PRECOLORED &&
	    u->n_degree < reg_pb[u->n_rclass] && !move_related(u)) {
		if (u->n_wl != NL_FREEZEWL)
			fatalx("addworklist %d is on %d",
			    u->n_sym->is_id, u->n_wl);
		TAILQ_REMOVE(&freezewl, u, n_link);
		ADDNODEWL(NL_SIMPLIFYWL, u);
	}
}

static int
ok(struct node *u, struct node *v)
{
	struct adjelem *edge;
	struct node *t;

	SLIST_FOREACH(edge, &adjlists[v->n_sym->is_id - REG_NREGS], a_link) {
		t = edge->a_node;
		if (t->n_wl == NL_SELSTACK || t->n_wl == NL_COALNODES)
			continue;
		if (t->n_wl == NL_PRECOLORED ||
		    t->n_degree < reg_pb[t->n_rclass] ||
		    AMGET(adjmatrix, t->n_sym->is_id, u->n_sym->is_id))
			continue;
		return 0;
	}
	return 1;
}

static int
doconservative(struct node *u, struct node *n)
{
	int k = 0;
	struct adjelem *edge;
	struct node *t;

	SLIST_FOREACH(edge, &adjlists[n->n_sym->is_id - REG_NREGS], a_link) {
		t = edge->a_node;
		if (t->n_wl == NL_SELSTACK || t->n_wl == NL_COALNODES)
			continue;
		if (t->n_consround == nconsround)
			continue;
		if (t->n_degree >= reg_pb[t->n_rclass]) {
			k += reg_qbc[u->n_rclass][t->n_rclass];
			if (k >= reg_pb[u->n_rclass])
				return -1;
		}
		t->n_consround = nconsround;
	}
	return k;
}

static int
conservative(struct node *u, struct node *v)
{
	int k1, k2;

	nconsround++;
	k1 = doconservative(u, u);
	if (k1 == -1 || k1 >= reg_pb[u->n_rclass])
		return 0;
	if ((k2 = doconservative(u, v)) == -1)
		return 0;
	return k1 + k2 < reg_pb[u->n_rclass];
}

static void
combine(struct node *u, struct node *v)
{
	int odeg;
	struct adjelem *edge;
	struct move *m;
	struct movelink *ml, *ml2, *mnext, *prev;
	struct movelink *newml;
	struct node *t;

	if (v->n_wl == NL_FREEZEWL)
		TAILQ_REMOVE(&freezewl, v, n_link);
	else if (v->n_wl != NL_SPILLWL)
		fatalx("combine %p=%d, %p=%d on %d",
		    u, u->n_sym->is_id, v, v->n_sym->is_id, v->n_wl);
	else
		TAILQ_REMOVE(&spillwl, v, n_link);
	ADDNODEWL(NL_COALNODES, v);
	v->n_alias = u;
	if (Iflag)
		fprintf(dumpfp, "new alias of v=%d: u=%d\n",
		    v->n_sym->is_id, u->n_sym->is_id);

	prev = NULL;
	for (ml = SLIST_FIRST(&v->n_moves); ml != NULL; ml = mnext) {
		m = ml->m_move;
		mnext = SLIST_NEXT(ml, m_link);

		if (m->m_wl != ML_WORKLIST && m->m_wl != ML_ACTIVE) {
			if (prev == NULL)
				SLIST_REMOVE_HEAD(&v->n_moves, m_link);
			else
				SLIST_REMOVE_AFTER(prev, m_link);
			continue;
		}

		SLIST_FOREACH(ml2, &u->n_moves, m_link) {
			if (m == ml2->m_move)
				goto cont;
		}

		getmovelinks(&newml, 1);
		newml->m_move = m;
		SLIST_INSERT_HEAD(&u->n_moves, newml, m_link);
cont:
		prev = ml;
		continue;
	}

	SLIST_FOREACH(edge, &adjlists[v->n_sym->is_id - REG_NREGS], a_link) {
		t = edge->a_node;
		if (t->n_wl == NL_SELSTACK || t->n_wl == NL_COALNODES)
			continue;
		odeg = t->n_degree;
		pass_ralloc_addedge(curfn, t->n_sym->is_id, u->n_sym->is_id);
		t->n_degree = odeg;
		/* decrement_degree(t, v); */
	}
	if (u->n_wl == NL_FREEZEWL && u->n_degree >= reg_pb[u->n_rclass]) {
		DELNODEWL(NL_FREEZEWL, u);
		ADDNODEWL(NL_SPILLWL, u);
	}

	if (Iflag) {
		fprintf(dumpfp, "combined ");
		ir_dump_symbol(dumpfp, u->n_sym);
		fprintf(dumpfp, " and ");
		ir_dump_symbol(dumpfp, v->n_sym);
		fprintf(dumpfp, "\n");
		printgraph();
		fflush(dumpfp);
	}
}

static void
coalesce(void)
{
	struct move *m;
	struct node *u, *v, *x, *y;

	DEQWLMOVES(m);
	x = m->m_insn->is_l->ie_sym->is_node;
	y = m->m_insn->is_r->ie_sym->is_node;
	if (Iflag)
		fprintf(dumpfp, "coalesce %d = %d?\n",
		    x->n_sym->is_id, y->n_sym->is_id);
	x = getalias(x);
	y = getalias(y);
	if (y->n_wl == NL_PRECOLORED) {
		u = y;
		v = x;
	} else {
		u = x;
		v = y;
	}
	if (u == v) {
		ADDMOVEWL(ML_COALESCED, m);
		addworklist(u);
	} else if (v->n_wl == NL_PRECOLORED ||
	    (AMGET(adjmatrix, u->n_sym->is_id, v->n_sym->is_id))) {
		ADDMOVEWL(ML_CONSTRAINED, m);
		addworklist(u);
		addworklist(v);
	} else if ((u->n_wl == NL_PRECOLORED && ok(u, v)) ||
	    (u->n_wl != NL_PRECOLORED && conservative(u, v))) {
		ADDMOVEWL(ML_COALESCED, m);
		combine(u, v);
		addworklist(u);
	} else
		ADDACTMOVES(m);

	if (Iflag) {
		printworklists("coalesce");
		printgraph();
		fprintf(dumpfp, "\ncoalesce done\n");
		fflush(dumpfp);
	}
}

static void
freeze_moves(struct node *u)
{
	struct move *m;
	struct movelink *ml;
	struct node *u2, *v, *x, *y;

	if (Iflag)
		fprintf(dumpfp, "freeze_moves\n");
	u2 = getalias(u);
	while (!SLIST_EMPTY(&u->n_moves)) {
		ml = SLIST_FIRST(&u->n_moves);
		SLIST_REMOVE_HEAD(&u->n_moves, m_link);
		m = ml->m_move;
		if (m->m_wl != ML_ACTIVE && m->m_wl != ML_WORKLIST)
			continue;

		x = getalias(m->m_insn->is_r->ie_sym->is_node);
		y = getalias(m->m_insn->is_l->ie_sym->is_node);
		if (u2 == y)
			v = x;
		else if (u2 == x)
			v = y;
		else
			fatalx("boo");

		if (m->m_wl == ML_ACTIVE)
			DELACTMOVES(m);
		else
			DELWLMOVES(m);

		ADDMOVEWL(ML_FROZEN, m);
		if (v->n_wl != NL_FREEZEWL)
			continue;
		if (v->n_degree < reg_pb[v->n_rclass] && !move_related(v)) {
			TAILQ_REMOVE(&freezewl, v, n_link);
			ADDNODEWL(NL_SIMPLIFYWL, v);
		}
	}
}

static void
freeze(void)
{
	struct node *u;

	DEQNODEWL(NL_FREEZEWL, u);
	ADDNODEWL(NL_SIMPLIFYWL, u);
	freeze_moves(u);
	if (Iflag)
		printworklists("freeze");
}

static void
select_spill(void)
{
	struct node *m;

	if (Iflag)
		fprintf(dumpfp, "select_spill\n");
	TAILQ_FOREACH(m, &spillwl, n_link) {
		if (m->n_sym->is_flags & IR_SYM_RATMP)
			break;
		if (!(m->n_flags & N_SPILLNODE))
			break;
	}
	if (m == NULL)
		m = TAILQ_FIRST(&spillwl);
	TAILQ_REMOVE(&spillwl, m, n_link);
	ADDNODEWL(NL_SIMPLIFYWL, m);
	freeze_moves(m);

	if (Iflag)
		printworklists("select_spill");
}

static void
assign_colors(void)
{
	int c, i;
	struct adjelem *edge;
	struct node *n, *w;
	struct regset colors;

	regset_init(&colors);
	while (!TAILQ_EMPTY(&select_stack)) {
		DEQNODEWL(NL_SELSTACK, n);
		BITVEC_CPY(&colors, &regclasses[n->n_rclass]);
		SLIST_FOREACH(edge, &adjlists[n->n_sym->is_id - REG_NREGS],
		    a_link) {
			w = getalias(edge->a_node);
			if (w->n_wl != NL_COLOREDNODES &&
			    w->n_wl != NL_PRECOLORED)
				continue;

			c = w->n_color;
			for (i = 0; reg_conflicts[c][i] != -1; i++)
				BITVEC_CLEARBIT(&colors, reg_conflicts[c][i]);
		}
		if ((c = BITVEC_FIRSTSET(&colors)) == colors.b_nbit) {
			ADDNODEWL(NL_SPILLEDNODES, n);
			if (n->n_sym->is_flags & IR_SYM_RATMP)
				fatalx("assign_colors: no color for tmp sym");
		} else {
			/*
			 * TODO: Use a volatile register as color if possible.
			 */
			if (n->n_color != -1 &&
			    BITVEC_ISSET(&colors, n->n_color))
				c = n->n_color;
			ADDNODEWL(NL_COLOREDNODES, n);
			n->n_color = c;
		}
	}
	TAILQ_FOREACH(n, &coalesced_nodes, n_link) {
		w = getalias(n);
		if (w->n_wl != NL_COLOREDNODES && w->n_wl != NL_PRECOLORED)
			fatalx("assign_colors: %p=%d has bad alias %p=%d",
			    n, n->n_sym->is_id, w, w->n_sym->is_id);
		n->n_color = w->n_color;
	}
}

static struct ir_symbol *
genload(struct ir_insn *insn, struct ir_symbol *sym)
{
	struct ir_insn *load;
	struct ir_symbol *rv;

	if (sym->is_node->n_wl != NL_SPILLEDNODES)
		return sym;
	rv = newnode(sym);
	load = ir_asg(ir_virtreg(rv), ir_var(IR_LVAR, sym));
	ir_symbol_setflags(sym, IR_SYM_USED);
	cfa_bb_prepend_insn(insn, load);
	return rv;
}

static void
rewrite_expr(struct ir_insn *insn, struct ir_expr *x)
{
	for (;;) {
		if (IR_ISBINEXPR(x)) {
			rewrite_expr(insn, x->ie_l);
			x = x->ie_r;
		} else if (IR_ISUNEXPR(x))
			x = x->ie_l;
		else if (x->i_op == IR_REG) {
			x->ie_sym = genload(insn, x->ie_sym);
			break;
		} else
			break;
	}
}

static void
rewrite_program(void)
{
	struct ir_expr *x;
	struct ir_insn *insn, *store;
	struct ir_symbol *osym, *sym;
	struct node *n;/*, *next;*/

	TAILQ_FOREACH(insn, &curfn->if_iq, ii_link) {
		if (insn->i_op == IR_LBL || insn->i_op == IR_B)
			continue;
		if (IR_ISBRANCH(insn)) {
			rewrite_expr(insn, insn->ib_l);
			rewrite_expr(insn, insn->ib_r);
			continue;
		}
		switch (insn->i_op) {
		case IR_ASG:
			rewrite_expr(insn, insn->is_r);
			if (insn->is_l->i_op != IR_REG) {
				rewrite_expr(insn, insn->is_l);
				break;
			}
			osym = insn->is_l->ie_sym;
			if (osym->is_node->n_wl != NL_SPILLEDNODES)
				break;
			sym = newnode(osym);
			insn->is_l->ie_sym = sym;
			store = ir_asg(ir_var(IR_LVAR, osym), ir_virtreg(sym));
			cfa_bb_append_insn(curfn, insn, store);
			break;
		case IR_ST:
			rewrite_expr(insn, insn->is_l);
			rewrite_expr(insn, insn->is_r);
			break;
		case IR_CALL:
			if (insn->ic_ret != NULL)
				rewrite_expr(insn, insn->ic_ret);
			SIMPLEQ_FOREACH(x, &insn->ic_argq, ie_link)
				rewrite_expr(insn, x);
			if (insn->ic_fn->is_op == IR_REGSYM)
				insn->ic_fn = genload(insn, insn->ic_fn);
			break;
		case IR_RET:
			if (insn->ir_retexpr != NULL)
				rewrite_expr(insn, insn->ir_retexpr);
			break;
		default:
			fatalx("rewrite_program: bad op: 0x%x", insn->i_op);
		}
	}

	TAILQ_INIT(&spilled_nodes);
	while (!TAILQ_EMPTY(&colored_nodes)) {
		DEQNODEWL(NL_COLOREDNODES, n);
		ADDNODEWL(NL_INITIAL, n);
	}

#if 0
	for (n = TAILQ_FIRST(&coalesced_nodes); n != NULL; n = next) {
		next = TAILQ_NEXT(n, n_link);
		if (!(n->n_flags & N_COALNODE)) {
			DELNODEWL(&coalesced_nodes, n);
			ADDNODEWL(&initial, n);
		}
	}
#endif

#if 1
	while (!TAILQ_EMPTY(&coalesced_nodes)) {
		DEQNODEWL(NL_COALNODES, n);
		if (!(n->n_flags & N_COALNODE))
			ADDNODEWL(NL_INITIAL, n);
	}
#endif
	TAILQ_INIT(&colored_nodes);
#if 1
	TAILQ_INIT(&coalesced_nodes);
#endif
}

static struct ir_symbol *
getphysreg(struct ir_symbol *sym)
{
	struct node *n;

	n = sym->is_node;
	if (n->n_wl != NL_COLOREDNODES && n->n_wl != NL_COALNODES &&
	    n->n_wl != NL_PRECOLORED)
		return sym;
	if (sym->is_id < REG_NREGS)
		return sym;
	regset_addreg(&curfn->if_usedregs, n->n_color);
	return physregs[n->n_color];
}

static void
insert_tmp(struct ir *ir)
{
	int i;

	for (i = 0; ir->i_tmpregs[i] != NULL; i++)
		ir->i_tmpregsyms[i] = getphysreg(ir->i_tmpregsyms[i]);
}

static void
doinsert_regs(struct ir_expr *x)
{
	for (;;) {
		if (x->i_tmpregs != NULL)
			insert_tmp((struct ir *)x);
		if (IR_ISBINEXPR(x)) {
			doinsert_regs(x->ie_r);
			x = x->ie_l;
		} else if (IR_ISUNEXPR(x))
			x = x->ie_l;
		else if (x->i_op != IR_REG)
			break;
		else {
			x->ie_sym = getphysreg(x->ie_sym);
			break;
		}
	}
}

static void
insert_regs(void)
{
	struct ir_expr *x;
	struct ir_insn *insn;
	struct ir_symbol *next, *prev, *sym;

	TAILQ_FOREACH(insn, &curfn->if_iq, ii_link) {
		if (insn->i_tmpregs != NULL)
			insert_tmp((struct ir *)insn);
		if (insn->i_op == IR_LBL || insn->i_op == IR_B)
			continue;
		if (IR_ISBRANCH(insn)) {
			doinsert_regs(insn->ib_l);
			doinsert_regs(insn->ib_r);
			continue;
		}
		switch (insn->i_op) {
		case IR_ASG:
		case IR_ST:
			doinsert_regs(insn->is_l);
			doinsert_regs(insn->is_r);
			break;
		case IR_CALL:
			if (insn->ic_ret != NULL)
				doinsert_regs(insn->ic_ret);
			SIMPLEQ_FOREACH(x, &insn->ic_argq, ie_link)
				doinsert_regs(x);
			if (insn->ic_fn->is_op == IR_REGSYM)
				insn->ic_fn = getphysreg(insn->ic_fn);
			break;
		case IR_RET:
			if (insn->ir_retexpr != NULL)
				doinsert_regs(insn->ir_retexpr);
			break;
		default:
			fatalx("insert_regs: bad op: 0x%x", insn->i_op);
		}
	}

	/* Remove regs that are not used for spilling. */
	prev = NULL;
	for (sym = SIMPLEQ_FIRST(&curfn->if_regq); sym != NULL; sym = next) {
		next = SIMPLEQ_NEXT(sym, is_link);
		if (!(sym->is_flags & IR_SYM_USED)) {
			if (prev == NULL)
				SIMPLEQ_REMOVE_HEAD(&curfn->if_regq, is_link);
			else
				SIMPLEQ_REMOVE_AFTER(&curfn->if_regq, prev,
				    is_link);
			ir_symbol_free(sym);
		} else
			prev = sym;
	}
}

/*
 * Any coalesced moves that happend before the first spill selection
 * can be removed.
 */
static void
delcoalesced(void)
{
#if 0
	struct move *m;
	struct node *n;

	while (!LIST_EMPTY(&coalesced_moves)) {
		m = LIST_FIRST(&coalesced_moves);
		LIST_REMOVE(m, m_wlnext);
		ir_delete_insn(curfn, m->m_insn);
	}
	LIST_INIT(&coalesced_moves);
	TAILQ_FOREACH(n, &coalesced_nodes, n_link)
		n->n_flags |= N_COALNODE;
	if (Iflag)
		ir_dump_func(dumpfp, curfn);
#endif
}

static void
alloctmp(struct ir *ir)
{
	int i;
	struct ir_symbol **sym;
	struct ir_type *ty;

	for (i = 0; ir->i_tmpregs[i] != NULL; i++)
		continue;
	sym = xmnalloc(i, sizeof *sym);
	ir->i_tmpregsyms = sym;
	for (i = 0; ir->i_tmpregs[i] != NULL; i++) {
		ty = ir->i_tmpregs[i]->t_type;
		sym[i] = ir_symbol(IR_REGSYM, NULL, ty->it_size, ty->it_align,
		    ty);
		ir_symbol_setflags(sym[i], IR_SYM_RATMP);
		ir_func_addreg(curfn, sym[i]);
	}
}

static void
alloctmp_expr(struct ir_expr *x)
{
	for (;;) {
		if (x->i_tmpregs != NULL)
			alloctmp((struct ir *)x);
		if (IR_ISBINEXPR(x)) {
			alloctmp_expr(x->ie_r);
			x = x->ie_l;
		} else if (IR_ISUNEXPR(x))
			x = x->ie_l;
		else
			break;
	}
}

void
pass_ralloc(struct passinfo *pi)
{
	int changes, nspill = 0;
	size_t i, j, oldamsize = 0, oldsize = 0, size;
	struct ir_func *fn = pi->p_fn;
	struct ir_insn *insn;
	struct ir_symbol *sym;
	struct node *n, *nodes;
	int oiflag = Iflag;

	static int dumpno;

	if (nvreg == -1) {
		for (i = 0, j = BITVEC_FIRSTSET(&reg_volat);
		    j < reg_volat.b_nbit;
		    j = BITVEC_NEXTSET(&reg_volat, j), i++)
			vregs[i] = j;
		nvreg = i;
	}

#if 0
	Iflag = 0;
#endif

	nconsround = 0;
	nspill = 0;
	curfn = fn;
	BITVEC_CLEARALL(&fn->if_usedregs);
	mem_area_init(&mem);
	adjmatrix = NULL;
	adjlists = NULL;
	allmoves = freemoves = NULL;
	allmovelinks = freemovelinks = NULL;
	alladjelems = freeadjelems = NULL;

	TAILQ_INIT(&initial);
	TAILQ_INIT(&precolored);
	TAILQ_INIT(&simplifywl);
	TAILQ_INIT(&freezewl);
	TAILQ_INIT(&spillwl);
	TAILQ_INIT(&spilled_nodes);
	TAILQ_INIT(&colored_nodes);
	TAILQ_INIT(&coalesced_nodes);

	LIST_INIT(&coalesced_moves);
	LIST_INIT(&constrained_moves);
	LIST_INIT(&frozen_moves);
	LIST_INIT(&worklist_moves);
	LIST_INIT(&active_moves);

	/*
	 * Record which instructions need temporary registers and
	 * allocate ir_symbols for them.
	 */
	TAILQ_FOREACH(insn, &fn->if_iq, ii_link) {
		if (insn->i_tmpregs != NULL)
			alloctmp((struct ir *)insn);
		if (insn->i_op == IR_B || insn->i_op == IR_LBL ||
		    insn->i_op == IR_RET || insn->i_op == IR_CALL)
			continue;
		if (IR_ISBRANCH(insn)) {
			alloctmp_expr(insn->ib_l);
			alloctmp_expr(insn->ib_r);
		} else if (insn->i_op == IR_ASG || insn->i_op == IR_ST) {
			alloctmp_expr(insn->is_l);
			alloctmp_expr(insn->is_r);
		} else
			fatalx("pass_ralloc: bad insn: 0x%x", insn->i_op);
	}

	ir_func_linearize_regs(fn);
	nodes = mem_mnalloc(&mem, fn->if_regid, sizeof *nodes);
	i = 0;
	SIMPLEQ_FOREACH(sym, &fn->if_regq, is_link) {
		sym->is_node = &nodes[i];
		sym->is_flags &= ~IR_SYM_USED;
		if (sym->is_id >= REG_NREGS)
			ADDNODEWL(NL_INITIAL, &nodes[i]);
		else
			fatalx("physical reg is in regq");
		nodes[i].n_sym = sym;
		nodes[i].n_color = -1;
		nodes[i].n_consround = 0;
		nodes[i].n_rclass = ir_symbol_rclass(sym);
		nodes[i].n_flags = 0;
		i++;
	}
	for (j = 0; j < REG_NREGS; i++, j++) {
		physregs[j]->is_node = &nodes[i];
		nodes[i].n_sym = physregs[j];
		ADDNODEWL(NL_PRECOLORED, &nodes[i]);
		nodes[i].n_color = j;
		nodes[i].n_consround = 0;
		nodes[i].n_rclass = ir_symbol_rclass(physregs[j]);
		nodes[i].n_flags = 0;
	}

	/* Precolor call arguments. */
#if PRECOLOR_CALLARGS
	TAILQ_FOREACH(insn, &fn->if_iq, ii_link) {
		if (insn->i_op != IR_CALL)
			continue;
		pass_ralloc_callargs(fn, insn);
	}
#endif

	if (Iflag)
		dumpfp = dump_open("RA", fn->if_sym->is_name, "w", dumpno++);

	cfa_buildcfg(fn);
	for (nrounds = 1; nrounds <= ROUNDS_MAX; nrounds++) {
		freemoves = allmoves;
		freemovelinks = allmovelinks;
		freeadjelems = alladjelems;

		/*
		 * Do a live variables analysis to be able to build the
		 * interference graph.
		 */
		dfa_livevar(fn);

		/* Setup the adjacency matrix and lists. */
		amsize = fn->if_regid;
		size = (amsize * (amsize + 1)) >> 1;
		size = ((size + 7) & ~7) >> 3;
		if (size > oldsize) {
			free(adjmatrix);
			adjmatrix = calloc(size, sizeof *adjmatrix);
		} else
			memset(adjmatrix, 0, oldsize);
		if (amsize > oldamsize) {
			free(adjlists);
			adjlists = calloc(amsize - REG_NREGS,
			    sizeof *adjlists);
		} else
			memset(adjlists, 0,
			    (oldamsize - REG_NREGS) * sizeof *adjlists);
		oldsize = size;
		oldamsize = amsize;
		for (i = 0; i < amsize - REG_NREGS; i++)
			SLIST_INIT(&adjlists[i]);

		TAILQ_INIT(&select_stack);

		TAILQ_FOREACH(n, &initial, n_link) {
			SLIST_INIT(&n->n_moves);
			n->n_alias = NULL;
			n->n_color = -1;
			n->n_degree = 0;
		}
#if PRECOLOR_CALLARGS
		TAILQ_FOREACH(n, &precolored, n_link) {
			SLIST_INIT(&n->n_moves);
			n->n_alias = NULL;
			n->n_degree = INT_MAX / 2;
		}
#else
		for (i = 0; i < REG_NREGS; i++) {
			n = physregs[i]->is_node;
			SLIST_INIT(&n->n_moves);
			n->n_alias = NULL;
			n->n_degree = INT_MAX / 2;
		}
#endif

		build(fn);
		mkworklist();
		do {
			changes = 0;
			if (!TAILQ_EMPTY(&simplifywl)) {
				changes = 1;
				simplify();
			} else if (!LIST_EMPTY(&worklist_moves)) {
				changes = 1;
				coalesce();
			} else if (!TAILQ_EMPTY(&freezewl)) {
				changes = 1;
				freeze();
			} else if (!TAILQ_EMPTY(&spillwl)) {
				changes = 1;
				if (nspill++ == 0)
					delcoalesced();
				select_spill();
			}
		} while (changes);
		if (nspill == 0)
			delcoalesced();
		assign_colors();
		if (TAILQ_EMPTY(&spilled_nodes)) {
			insert_regs();
			mem_area_free(&mem);
			free(adjmatrix);
			free(adjlists);
			if (Iflag)
				fclose(dumpfp);
			Iflag = oiflag;
			return;
		}
		rewrite_program();
	}
	fatalx("register allocator still not done after %d iterations",
	    ROUNDS_MAX);
}
