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
 * This code-generator generator is based on these papers:
 *
 * Christopher W. Fraser, David R. Hanson and Todd A. Proebsting:
 *     Engineering a Simple, Efficient Code Generator Generator
 *
 * and
 *
 * Helmut Emmelmann, Friedrich-Wilhelm Schroeer and Rudolf Landwehr:
 *     BEG - a Generator for Efficient Back Ends
 *
 * Putting all the patterns in a trie was inspired by:
 *
 * Alfred V. Aho, Mahadevan Ganapathi and Steven W. K. Tijang:
 *     Code Generation Using Tree Matching and Dynamic Programming
 *
 * However, we directly generate code out of the trie instead of transforming
 * it into an automaton.
 */

#include <ctype.h>
#include <err.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "cgg.h"

#define MAXIDS	1024

extern char *__progname;

#define NTSMAX	4

struct ntsseq {
	SIMPLEQ_HEAD(, pattern) n_patterns;
	short	n_nts[NTSMAX];
	short	n_len;
	short	n_sufof;
	short	n_sufpos;
};

static int ntsseqcmp_nts(const void *, const void *);
static int ntsseqcmp_suf(const void *, const void *);
static int ntsseq_issuf(struct ntsseq *, struct ntsseq *);

static struct ntsseq **ntsseq;
static int nnts;

static char *treenames[MAXIDS];
static char *ntermnames[MAXIDS];
static struct nonterm nonterms[MAXIDS];

static int findname(char **, const char *);

static char *infile;
static char *cfile;
static char *hfile;
static int nerrors;
static int nterms;
static int npatterns;
static int gflag;

static SIMPLEQ_HEAD(, rule) rules = SIMPLEQ_HEAD_INITIALIZER(rules);

struct path {
	struct	path *p_prev;
	int	p_kid;
};

SIMPLEQ_HEAD(triehead, trie);

struct trie {
	SIMPLEQ_ENTRY(trie) t_link;
	struct	triehead t_head;
	struct	pattern	*t_pattern;
	char	*t_constr;
	short	t_kind;
	short	t_id;
	short	t_kid;
	short	t_depth;
	short	t_leaf;
};

static struct triehead trieh = SIMPLEQ_HEAD_INITIALIZER(trieh);
static int trie_maxdepth;

static void print_pattern_nts(void);
static void print_cg_pattern_subtree(void);
static int print_subtree_access(struct tree *t, struct path *, int, int);
static void print_subtree_access_trailer(struct path *);
static void print_cg_closures(void);
static void print_cg_actionemit(void);
static void print_cg_match(void);
static void print_cg_trie(struct triehead *, int);
static void print_cg_term(struct trie *, int);
static void print_cg_nterm(struct trie *, int);
static int print_cg_chains(int, int, int);
static void print_trie(struct triehead *, int);
static void printgrammar(void);
static void printtree(struct tree *);

static void check_nterm_chains(void);
static void check_chain(int, int);
static void collect_pattern_nts(void);
static void collect_tree_nts(struct ntsseq *, struct tree *);
static void build_trie(void);
static struct triehead *trie_insert_tree(struct triehead *, struct trie **,
    struct tree *, int, int);

static int treecmp(const void *, const void *);

static void *xmalloc(size_t);
static void *xcalloc(size_t, size_t);
static void iprintf(int, const char *, ...);
static void usage(void);

int
main(int argc, char **argv)
{
	int ch;
	int i;
	FILE *hfp;

	while ((ch = getopt(argc, argv, "g")) != -1) {
		switch (ch) {
		case 'g':
			gflag = 1;
			break;
		default:
			usage();
		}
	}

	argc -= optind;
	argv += optind;
	if (argc < 3)
		usage();
	infile = argv[0];
	cfile = argv[1];
	hfile = argv[2];
	if (freopen(infile, "r", stdin) == NULL)
		err(1, "couldn't open %s", infile);
	if (freopen(cfile, "w", stdout) == NULL)
		err(1, "couldn't open %s", cfile);
	if ((hfp = fopen(hfile, "w")) == NULL)
		err(1, "couldn't open %s", hfile);

	fprintf(hfp, "#ifndef CG_H\n");
	fprintf(hfp, "#define CG_H\n\n");

	parse();
	if (nerrors)
		return 1;

	check_nterm_chains();

	if (gflag) {
		printf("\n#if 0\n");
		printgrammar();
		printf("#endif\n");
	}

	printf("#include \"cg.h\"");
	fprintf(hfp, "#define CG_MAXCOST\tSHRT_MAX\n");
	fprintf(hfp, "#define CG_NTERMS\t%d\n", nterms);
	fprintf(hfp, "\nstruct cg_data {\n\tshort\tcd_cost[CG_NTERMS];\n");
	fprintf(hfp, "\tshort\tcd_pattern[CG_NTERMS];\n};\n");
	fprintf(hfp, "extern int cg_startnt;\n");
	fprintf(hfp, "extern short *cg_nts[];\n");
	fprintf(hfp, "extern short cg_pattern_nts[];\n");
	fprintf(hfp, "extern int8_t cg_hasaction[];\n");
	fprintf(hfp, "extern char *cg_emitstrs[];\n");
	fprintf(hfp, "CGI_IR **cg_pattern_subtree(CGI_IR **, int, int);\n");
	fprintf(hfp, "CGI_IR *cg_actionemit(CGI_CTX *, CGI_IR *, CGI_IR *, "
	    "int);\n");
	fprintf(hfp, "int cg_addmatch(struct cg_data *, int, int, int);\n");

	printf("\nint cg_startnt = %d;\n", 0);

	for (i = 0; i < nterms; i++)
		printf("\n#define CG_NT_%s\t%d", ntermnames[i], i);
	printf("\n\n");

	collect_pattern_nts();
	print_pattern_nts();
	print_cg_pattern_subtree();
	print_cg_closures();
	print_cg_actionemit();
	build_trie();
	if (gflag) {
		printf("#if 0\n");
		print_trie(&trieh, 0);
		printf("#endif\n");
	}
	print_cg_match();
	fprintf(hfp, "\n#endif /* CG_H */\n");
	return 0;
}

static void
check_nterm_chains(void)
{
	int i;
	struct pattern *p;

	if (gflag)
		printf("\n#if 0\n");
	for (i = 0; i < nterms; i++) {
		check_chain(i, i);
		if (gflag) {
			printf("%s ->", ntermnames[i]);
			SIMPLEQ_FOREACH(p, &nonterms[i].n_chainpats,
			    p_chlink)
				printf(" %s", ntermnames[p->p_rule->r_nterm]);
			printf("\n");
		}
	}
	if (gflag)
		printf("#endif\n");
}

static void
check_chain(int start, int nterm)
{
	struct pattern *p;

	SIMPLEQ_FOREACH(p, &nonterms[nterm].n_chainpats, p_chlink) {
		if (p->p_rule->r_nterm == start)
			errx(1, "chain rule cycle. start: %s, from : %s",
			    ntermnames[start], ntermnames[nterm]);
		if (p->p_cost[0] != '\0' || p->p_tree->t_constr[0] != '\0')
			nonterms[nterm].n_noclosure = 1;
		check_chain(start, p->p_rule->r_nterm);
		if (nonterms[p->p_rule->r_nterm].n_noclosure)
			nonterms[nterm].n_noclosure = 1;
	}
}

/*
 * Collect which nonterminals each pattern uses. The sequence of nonterminals
 * is in prefix order.
 */
static void
collect_pattern_nts(void)
{
	int i, j;
	struct ntsseq *nts, *suf;
	struct pattern *p;
	struct rule *r;

	ntsseq = xcalloc(npatterns, sizeof *ntsseq);

	i = 0;
	SIMPLEQ_FOREACH(r, &rules, r_glolink) {
		SIMPLEQ_FOREACH(p, &r->r_patterns, p_rlink) {
			if (i != p->p_id)
				errx(1, "collect_pattern_nts: confused order");
			ntsseq[i] = xmalloc(sizeof **ntsseq);
			ntsseq[i]->n_sufof = -1;
			SIMPLEQ_INIT(&ntsseq[i]->n_patterns);
			ntsseq[i]->n_len = 0;
			collect_tree_nts(ntsseq[i], p->p_tree);
			SIMPLEQ_INSERT_TAIL(&ntsseq[i]->n_patterns, p,
			    p_ntslink);
			p->p_ntsidx = i;
			i++;
		}
	}

	/*
	 * Sort sequences into ascending order, and eliminate duplicates.
	 */
	nnts = npatterns;
	qsort(ntsseq, npatterns, sizeof *ntsseq, ntsseqcmp_nts);
	for (i = 0; i < npatterns; i = j) {
		for (j = i + 1; j < npatterns; j++) {
			if (ntsseqcmp_nts(&ntsseq[i], &ntsseq[j]) != 0)
				break;
			while (!SIMPLEQ_EMPTY(&ntsseq[j]->n_patterns)) {
				p = SIMPLEQ_FIRST(&ntsseq[j]->n_patterns);
				SIMPLEQ_REMOVE_HEAD(&ntsseq[j]->n_patterns,
				    p_ntslink);
				SIMPLEQ_INSERT_TAIL(&ntsseq[i]->n_patterns, p,
				    p_ntslink);
			}
			free(ntsseq[j]);
			ntsseq[j] = NULL;
			nnts--;
		}
	}
	for (j = 0; j < npatterns; j++) {
		if (ntsseq[j] == NULL)
			break;
	}
	for (i = 0; i < npatterns; i++) {
		if (ntsseq[i] == NULL)
			continue;
		if (i > j) {
			ntsseq[j++] = ntsseq[i];
			ntsseq[i] = NULL;
		}
	}
	/* Sort suffixes into ascending order. */
	qsort(ntsseq, nnts, sizeof *ntsseq, ntsseqcmp_suf);

	/*
	 * Register which sequence is a suffix of another and merge it into
	 * the suffix with the longer sequence.
	 */
	j = 0;
	for (i = nnts; i >= 2; i--) {
		nts = ntsseq[i - 1];
		suf = ntsseq[i - 2];
		if (ntsseq_issuf(nts, suf)) {
			if (nts->n_sufof == -1)
				suf->n_sufof = i - 1;
			else
				suf->n_sufof = nts->n_sufof;
			suf->n_sufpos = ntsseq[suf->n_sufof]->n_len - suf->n_len;
		}
	}

	for (i = 0; i < nnts; i++) {
		SIMPLEQ_FOREACH(p, &ntsseq[i]->n_patterns, p_ntslink)
			p->p_ntsidx = i;
	}
}

static void
collect_tree_nts(struct ntsseq *nts, struct tree *t)
{
	int i;

	if (t->t_kind == T_NTERM) {
		if (nts->n_len >= NTSMAX)
			errx(1, "collect_tree_nts: tree has too many nonterms");
		nts->n_nts[nts->n_len++] = t->t_id;
	}
	for (i = 0; i < t->t_nkids; i++)
		collect_tree_nts(nts, t->t_kids[i]);
}

static void
build_trie(void)
{
	struct rule *r;
	struct pattern *p;
	struct tree *t;
	struct trie *trie, *ptrie;

	SIMPLEQ_FOREACH(r, &rules, r_glolink) {
		SIMPLEQ_FOREACH(p, &r->r_patterns, p_rlink) {
			t = p->p_tree;
			if (t->t_kind == T_NTERM)
				continue;
			trie_insert_tree(&trieh, &trie, t, -1, 0);
			ptrie = xmalloc(sizeof *trie);
			ptrie->t_kind = -1;
			ptrie->t_pattern = p;
			ptrie->t_constr = NULL;
			SIMPLEQ_INSERT_TAIL(&trie->t_head, ptrie, t_link);
		}
	}
}

static struct triehead *
trie_insert_tree(struct triehead *th, struct trie **tp, struct tree *t,
    int kid, int depth)
{
	int i;
	struct trie *trie;

	SIMPLEQ_FOREACH(trie, th, t_link) {
		if (trie->t_kind == t->t_kind && trie->t_id == t->t_id &&
		    trie->t_kid == kid && trie->t_depth == depth)
			break;
	}

	if (trie == NULL) {
		trie = xmalloc(sizeof *trie);
		SIMPLEQ_INIT(&trie->t_head);
		trie->t_kind = t->t_kind;
		trie->t_id = t->t_id;
		trie->t_kid = kid;
		SIMPLEQ_INSERT_TAIL(th, trie, t_link);
		trie->t_constr = NULL;
		trie->t_pattern = NULL;
		if (t->t_constr[0] != '\0')
			trie->t_constr = t->t_constr;
		else
			trie->t_constr = NULL;
		trie->t_depth = depth;
		if (depth > trie_maxdepth)
			trie_maxdepth = depth;
	}
	th = &trie->t_head;
	if (t->t_constr[0] != '\0') {
		trie = xmalloc(sizeof *trie);
		SIMPLEQ_INIT(&trie->t_head);
		trie->t_kind = -1;
		trie->t_id = trie->t_kid = -1;
		SIMPLEQ_INSERT_TAIL(th, trie, t_link);
		trie->t_constr = t->t_constr;
		trie->t_pattern = NULL;
		th = &trie->t_head;
		trie->t_depth = depth;
		if (depth > trie_maxdepth)
			trie_maxdepth = depth;
	}

	*tp = trie;
	trie->t_leaf = 1;
	for (i = 0; i < t->t_nkids; i++) {
		th = trie_insert_tree(th, tp, t->t_kids[i], i, depth + 1);
		trie->t_leaf = 0;
	}
	return th;
}

static int
ntsseqcmp_nts(const void *p, const void *q)
{
	int i;
	struct ntsseq *s = *(struct ntsseq **)p, *t = *(struct ntsseq **)q;

	for (i = 0; i < s->n_len && i < t->n_len; i++) {
		if (s->n_nts[i] != t->n_nts[i])
			break;
	}
	if (s->n_len < t->n_len)
		return -1;
	if (s->n_len > t->n_len)
		return 1;
	if (i == s->n_len)
		return 0;
	return s->n_nts[i] - t->n_nts[i];
}

static int
ntsseqcmp_suf(const void *p, const void *q)
{
	int i, j;
	struct ntsseq *s = *(struct ntsseq **)p, *t = *(struct ntsseq **)q;

	for (i = s->n_len, j = t->n_len; i && j; i--, j--) {
		if (s->n_nts[i - 1] != t->n_nts[j - 1])
			break;
	}
	if (i == 0 && j == 0)
		return 0;
	if (i == 0)
		return -1;
	if (j == 0)
		return 1;
	return s->n_nts[i - 1] - t->n_nts[j - 1];
}

/*
 * Check if suf is a suffix of nts and if so, return 1.
 * Otherwise, return 0.
 */
static int
ntsseq_issuf(struct ntsseq *nts, struct ntsseq *suf)
{
	int i;

	if (suf->n_len >= nts->n_len)
		return 0;
	for (i = 1; i <= suf->n_len; i++) {
		if (suf->n_nts[suf->n_len - i] != nts->n_nts[nts->n_len - i])
			return 0;
	}
	return 1;
}

static void
print_pattern_nts(void)
{
	int i, j;
	struct pattern *p;
	struct rule *r;

	for (i = 0; i < nnts; i++) {
		if (ntsseq[i]->n_sufof != -1)
			continue;
		printf("static short cg_nts%d[] = { ", i);
		for (j = 0; j < ntsseq[i]->n_len; j++)
			printf("%d, ", ntsseq[i]->n_nts[j]);
		printf("-1 };\n");
	}

	printf("\nshort *cg_nts[] = {\n\t");
	for (i = 0, j = 8; i < nnts; i++) {
		if (ntsseq[i]->n_sufof != -1) {
			printf("&cg_nts%d[%d]",
			    ntsseq[i]->n_sufof, ntsseq[i]->n_sufpos);
			j += 20;
		} else {
			printf("cg_nts%d", i);
			j += 12;
		} if (i != npatterns - 1) {
			printf(",");
			j += 2;
			if (j >= 79 ) {
				printf("\n\t");
				j = 8;
			} else
				printf(" ");
		}
	}
	printf("\n};\n\n");
	printf("short cg_pattern_nts[%d] = {\n\t", npatterns);
	i = j = 0;
	SIMPLEQ_FOREACH(r, &rules, r_glolink) {
		SIMPLEQ_FOREACH(p, &r->r_patterns, p_rlink) {
			if (i != p->p_id)
				errx(1, "print_pattern_nts: confused order");
			if (p->p_ntsidx >= nnts)
				errx(1, "pattern %d has bad nts %d", i, p->p_ntsidx);
			printf("%d", p->p_ntsidx);
			if (i != npatterns - 1) {
				printf(",");
				if (++j == 10) {
					printf("\n\t");
					j = 0;
				} else
					printf(" ");
			}
			i++;
		}
	}
	printf("\n};\n");
}

/*
 * For each given pattern p that is matched by a CGI_IR tree, generate code
 * that extracts the subtree for nonterminal n.
 */
static void
print_cg_pattern_subtree(void)
{
	int i;
	struct pattern *p;
	struct rule *r;
	struct tree **trees;

	/* Sort trees that have the same structure. */
	trees = xcalloc(npatterns, sizeof *trees);
	i = 0;
	SIMPLEQ_FOREACH(r, &rules, r_glolink) {
		SIMPLEQ_FOREACH(p, &r->r_patterns, p_rlink)
			trees[i++] = p->p_tree;
	}
	qsort(trees, i, sizeof *trees, treecmp);	

	printf("\nCGI_IR **\n");
	printf("cg_pattern_subtree(CGI_IR **ir, int p, int n)\n{\n");
	printf("\tCGI_IR **st = NULL;\n\n");
	printf("\tswitch (p) {\n");

	for (i = 0; i < npatterns; i++) {
		if (trees[i]->t_kind == T_TERM && trees[i]->t_nkids == 0)
			continue;
		printf("\tcase %d:\n", trees[i]->t_pattern->p_id);
		if (i != npatterns - 1 && !treecmp(&trees[i], &trees[i + 1]))
			continue;
		printf("\t\tswitch (n) {\n");
		print_subtree_access(trees[i], NULL, 0, 0);
		printf("\t\tdefault:\n\t\t\t");
		printf("CGI_FATALX(\"cg_pattern_subtree: bad n %%d for "
		    "pattern %%d\", n, p);\n");
		printf("\t\t}\n");
		printf("\t\tbreak;\n");
	}

	printf("\tdefault:\n\t\t");
	printf("CGI_FATALX(\"cg_pattern_subtree: bad pattern: %%d\", p);\n");
	printf("\t}\n");
	printf("\treturn st;\n");
	printf("\n}\n");

	free(trees);
}

static int
print_subtree_access(struct tree *t, struct path *path, int depth, int n)
{
	int i;
	struct path p;

	if (t->t_kind == T_NTERM) {
		printf("\t\tcase %d:\n\t\t\t", n);
		printf("st = ");
		if (depth == 0)
			printf("ir;\n");
		else {
			printf("CGI_IRCHILDP(");
			for (i = 0; i < depth - 1; i++)
				printf("CGI_IRCHILD(");
			printf("*ir");
			print_subtree_access_trailer(path);
			printf(";\n");
		}
		printf("\t\t\tbreak;\n");
		return n + 1;
	}
	
	p.p_prev = path;
	for (i = 0; i < t->t_nkids; i++) {
		p.p_kid = i;
		n = print_subtree_access(t->t_kids[i], &p, depth + 1, n);
	}
	return n;
}

static void
print_subtree_access_trailer(struct path *path)
{
	if (path->p_prev == NULL) {
		printf(", %d)", path->p_kid);
		return;
	}
	print_subtree_access_trailer(path->p_prev);
	printf(", %d)", path->p_kid);
}

static void
print_cg_closures(void)
{
	int i;
	struct pattern *p;

	for (i = 0; i < nterms; i++) {
		if (nonterms[i].n_noclosure != 0)
			continue;
		printf("static void\n");
		printf("cg_closure_%s(struct cg_data *cd, int p, int cost)\n",
		    ntermnames[i]);
		printf("{\n");

		printf("\tif (cost < cd->cd_cost[CG_NT_%s]) {\n", ntermnames[i]);
		printf("\t\tcd->cd_cost[CG_NT_%s] = cost;\n", ntermnames[i]);
		printf("\t\tcd->cd_pattern[CG_NT_%s] = p;\n", ntermnames[i]);
		if (!SIMPLEQ_EMPTY(&nonterms[i].n_chainpats))
			printf("\t\tcost++;\n");
		SIMPLEQ_FOREACH(p, &nonterms[i].n_chainpats, p_chlink)
			printf("\t\tcg_closure_%s(cd, %d, cost);\n",
			    ntermnames[p->p_rule->r_nterm], p->p_id);
		printf("\t}\n");
		printf("}\n");
	}
}

static void
print_cg_actionemit(void)
{
	int col = 78, val;
	struct rule *r;
	size_t i, nrest;
	struct pattern *p;
	struct pattern **rest;

	nrest = 0;
	rest = xcalloc(npatterns, sizeof *rest);

	printf("char *cg_emitstrs[] = {\n");
	SIMPLEQ_FOREACH(r, &rules, r_glolink) {
		SIMPLEQ_FOREACH(p, &r->r_patterns, p_rlink) {
			if (p->p_emit[0] != '\0')
				printf("\t%s,\n", p->p_emit);
			else
				printf("\tNULL,\n");
		}
	}
	printf("\n};\n");

	printf("int8_t cg_hasaction[] = {");
	SIMPLEQ_FOREACH(r, &rules, r_glolink) {
		SIMPLEQ_FOREACH(p, &r->r_patterns, p_rlink) {
			if (col >= 78) {
				printf("\n\t");
				col = 8;
			} else {
				printf(" ");
				col++;
			}
			val = 0;
			if (p->p_action[0] != '\0') {
				val = 1;
				rest[nrest++] = p;
			}
			printf("%d", val);
			col++;
			if (SIMPLEQ_NEXT(p, p_rlink) != NULL ||
			    SIMPLEQ_NEXT(r, r_glolink) != NULL) {
				printf(",");
				col++;
			}
		}
	}
	printf("\n};\n");

	printf("\nCGI_IR *\n");
	printf("cg_actionemit(CGI_CTX *ctx, CGI_IR *insn, CGI_IR *n, "
	    "int p)\n{\n");
	printf("\tstruct cg_ctx nctx = { insn, n, n, ctx };\n");
#if 0
	printf("\tchar *emitstr = NULL;\n");
#endif
	printf("\tswitch (p) {\n");

	for (i = 0; i < nrest; i++) {
		p = rest[i];
		printf("\tcase %d:\n", p->p_id);
#if 0
		if (p->p_emit[0] != '\0')
			printf("\t\temitstr = %s;\n", p->p_emit);
#endif
		if (p->p_action[0] != '\0')
			printf("\t\t%s;\n", p->p_action);
		printf("\t\tbreak;\n");
	}

	printf("\tdefault:\n");
	printf("\t\tCGI_FATALX(\"cg_actionemit: bad pattern: %%d\", p);\n");
	printf("\t}\n");
#if 0
	printf("\tif (emitstr != NULL)\n");
	printf("\t\tCGI_IREMIT(node, emitstr);\n");
#endif
	printf("\treturn nctx.cc_newnode;\n");
	printf("}\n");

	free(rest);
}

static void
print_cg_match(void)
{
	int i;

	printf("\nvoid\ncg_match(CGI_IR *ir0, struct cg_data *cd)\n{\n");
	printf("\tint cost0 = 0;\n");
	for (i = 1; i <= trie_maxdepth; i++)
		printf("\tCGI_IR *ir%d;\n", i);
	printf("\tCGI_IR *n;\n");
	printf("\tint tmp;\n");
	printf("\n\tn = ir0;\n");
	print_cg_trie(&trieh, 1);
	printf("}\n");
}

static void
print_cg_trie(struct triehead *th, int ind)
{
	struct trie *trie;
	struct pattern *p;

	trie = SIMPLEQ_FIRST(th);
	while (trie != NULL) {
		if (trie->t_pattern != NULL) {
			p = trie->t_pattern;
			iprintf(ind, "/* match pattern %d */\n", p->p_id);
			iprintf(ind, "{\n");

			if (nonterms[p->p_rule->r_nterm].n_noclosure)
				iprintf(ind + 1, "int old = cost0;\n");

			if (p->p_cost[0] != '\0')
				iprintf(ind + 1, "tmp = %s;\n", p->p_cost);
			else
				iprintf(ind + 1, "tmp = 1;\n");
			if (nonterms[p->p_rule->r_nterm].n_noclosure) {
				iprintf(ind + 1, "cost0 += tmp;\n");
				iprintf(ind + 1,
				    "if (cg_addmatch(CGI_DATA(ir0), CG_NT_%s, "
				    "%d, cost0)",
				    ntermnames[p->p_rule->r_nterm], p->p_id);
				print_cg_chains(ind + 1, p->p_rule->r_nterm,
				    0);
				iprintf(ind + 1, "}\n");
				iprintf(ind + 1, "cost0 = old;\n");
			} else
				iprintf(ind + 1, "cg_closure_%s("
				    "CGI_DATA(ir0), %d, cost0 + tmp);\n",
				    ntermnames[p->p_rule->r_nterm], p->p_id);
			iprintf(ind, "}\n");
		} else if (trie->t_kind == T_TERM) {
			if (trie->t_depth != 0)
				iprintf(ind,
				    "ir%d = CGI_IRCHILD(ir%d, %d);\n",
				    trie->t_depth, trie->t_depth - 1,
				    trie->t_kid);
			iprintf(ind, "switch (CGI_IROP(ir%d)) {\n",
			    trie->t_depth);
			while (trie != NULL && trie->t_kind == T_TERM) {
				print_cg_term(trie, ind);
				trie = SIMPLEQ_NEXT(trie, t_link);
			}
			iprintf(ind, "}\n");
			continue;
		}
		if (trie->t_kind == T_NTERM)
			print_cg_nterm(trie, ind);
		else if (trie->t_constr != NULL) {
			if (!trie->t_leaf || trie == SIMPLEQ_FIRST(th))
				iprintf(ind, "n = ir%d;\n", trie->t_depth);
			iprintf(ind, "if (%s) {\n", trie->t_constr);
			print_cg_trie(&trie->t_head, ind + 1);
			iprintf(ind, "}\n");
		}
		trie = SIMPLEQ_NEXT(trie, t_link);
	}
}

static void
print_cg_term(struct trie *trie, int ind)
{
	iprintf(ind, "case %s:\n", treenames[trie->t_id]);
	print_cg_trie(&trie->t_head, ind + 1);
	iprintf(ind + 1, "break;\n");
}

static void
print_cg_nterm(struct trie *trie, int ind)
{
	iprintf(ind, "ir%d = CGI_IRCHILD(ir%d, %d);\n",
	    trie->t_depth, trie->t_depth - 1, trie->t_kid);
	iprintf(ind, "tmp = CGI_GETDATA(ir%d)->cd_cost[CG_NT_%s];\n",
	    trie->t_depth, ntermnames[trie->t_id]);
	iprintf(ind, "if (tmp < CG_MAXCOST) {\n");
	iprintf(ind + 1, "int old = cost0;\n\n");
	iprintf(ind + 1, "cost0 += tmp;\n");
	print_cg_trie(&trie->t_head, ind + 1);
	iprintf(ind + 1, "cost0 = old;\n");
	iprintf(ind, "}\n");
}

static int
print_cg_chains(int ind, int nterm, int costno)
{
	int next = costno + 1;
	struct pattern *p;

	if (SIMPLEQ_EMPTY(&nonterms[nterm].n_chainpats)) {
		printf(") {\n");
		return next;
	}

	SIMPLEQ_FOREACH(p, &nonterms[nterm].n_chainpats, p_chlink) {
		if (p->p_tree->t_constr[0] != '\0')
			printf(" && %s) {\n", p->p_tree->t_constr);
		else
			printf(") {\n");

		if (p->p_cost[0] != '\0')
			iprintf(ind + 1, "int cost%d = cost%d + %s;\n\n",
			    next, costno, p->p_cost);
		else
			iprintf(ind + 1, "int cost%d = cost%d + 1;\n\n",
			    next, costno);

		iprintf(ind + 1,
		    "if (cg_addmatch(CGI_DATA(ir0), CG_NT_%s, %d, cost%d)",
		    ntermnames[p->p_rule->r_nterm], p->p_id, next);
		next = print_cg_chains(ind + 1, p->p_rule->r_nterm, next);
		iprintf(ind + 1, "}\n");
	}

	return next;
}

static void
print_trie(struct triehead *th, int indent)
{
	struct trie *t;

	SIMPLEQ_FOREACH(t, th, t_link) {
		iprintf(indent, "%d:", t->t_kid);
		switch (t->t_kind) {
		case T_NTERM:
			printf("%s", ntermnames[t->t_id]);
			break;
		case T_TERM:
			printf("%s", treenames[t->t_id]);
			break;
		default:
			if (t->t_constr != NULL) {
				printf("[%s]", t->t_constr);
				break;
			} else if (t->t_pattern != NULL) {
				printf("pattern %d\n", t->t_pattern->p_id);
				return;
			}
			errx(1, "print_trie");
		}
		printf("\n");
		print_trie(&t->t_head, indent + 1);
	}
}

static void
printgrammar(void)
{
	struct rule *r;
	struct pattern *p;

	SIMPLEQ_FOREACH(r, &rules, r_glolink) {
		printf("/* nonterm %d */\n", r->r_nterm);
		printf("%s:", ntermnames[r->r_nterm]);
		SIMPLEQ_FOREACH(p, &r->r_patterns, p_rlink) {
			printf("\t");
			if (p != SIMPLEQ_FIRST(&r->r_patterns))
				printf("| ");
			printtree(p->p_tree);
			if (p->p_action[0] != '\0')
				printf(" action {%s}", p->p_action);
			if (p->p_cost[0] != '\0')
				printf(" cost {%s}", p->p_cost);
			if (p->p_emit[0] != '\0')
				printf(" emit {%s}", p->p_emit);
			printf(" /* pattern %d */\n", p->p_id);
		}
		printf("\t;\n\n");
	}
}

static void
printtree(struct tree *t)
{
	int i;

	if (t->t_kind == T_TERM)
		printf("%s", treenames[t->t_id]);
	else
		printf("%s", ntermnames[t->t_id]);
	if (t->t_constr[0] != '\0')
		printf("[%s]", t->t_constr);
	if (t->t_nkids != 0) {
		printf("(");
		for (i = 0; i < t->t_nkids; i++) {
			printtree(t->t_kids[i]);
			if (i != t->t_nkids - 1)
				printf(", ");
		}
		printf(")");
	}
}

struct rule *
rule(const char *name)
{
	int id;
	struct rule *r;

	id = findname(ntermnames, name);
	if (nonterms[id].n_rule != NULL) 
		return nonterms[id].n_rule;
	r = xmalloc(sizeof *r);
	SIMPLEQ_INIT(&r->r_patterns);
	SIMPLEQ_INSERT_TAIL(&rules, r, r_glolink);
	nonterms[id].n_rule = r;
	if (nonterms[id].n_init == 0)
		SIMPLEQ_INIT(&nonterms[id].n_chainpats);
	nonterms[id].n_init = 1;
	r->r_nterm = id;
	if (id + 1 > nterms)
		nterms = id + 1;
	return r;
}

void
rule_addpattern(struct rule *r, struct pattern *p)
{
	SIMPLEQ_INSERT_TAIL(&r->r_patterns, p, p_rlink);
	p->p_rule = r;
	if (p->p_tree->t_kind == T_NTERM)
		SIMPLEQ_INSERT_TAIL(&nonterms[p->p_tree->t_id].n_chainpats, p,
		    p_chlink);
}

struct pattern *
pattern(struct tree *t)
{
	struct pattern *p;

	p = xmalloc(sizeof *p);
	p->p_tree = t;
	p->p_action[0] = p->p_cost[0] = p->p_emit[0] = '\0';
	p->p_id = npatterns++;
	p->p_ntsidx = 0;
	t->t_pattern = p;
	return p;
}

void
pattern_addace(struct pattern *p, int op, int idx, int ch)
{
	char *buf;

	switch (op) {
	case TOK_ACTION:
		buf = p->p_action;
		break;
	case TOK_COST:
		buf = p->p_cost;
		break;
	case TOK_EMIT:
		buf = p->p_emit;
		break;
	default:
		errx(1, "pattern_addace");
	}

	if (idx >= P_MAXSTR) {
		errh("pattern_addace: too many chars for buffer: %s", buf);
		return;
	}
	buf[idx] = ch;
	buf[idx + 1] = '\0';
}

struct tree *
tree(const char *name)
{
	int i, kind;
	struct tree *t;

	if (isupper(*name)) {
		kind = T_TERM;
		i = findname(treenames, name);
	} else {
		kind = T_NTERM;
		i = findname(ntermnames, name);
		if (nonterms[i].n_init == 0) {
			SIMPLEQ_INIT(&nonterms[i].n_chainpats);
			nonterms[i].n_init = 1;
			if (i + 1 > nterms)
				nterms = i + 1;
		}
	}

	t = xmalloc(sizeof *t);
	t->t_constr[0] = '\0';
	t->t_nkids = 0;
	t->t_id = i;
	t->t_kind = kind;
	return t;
}

void
tree_addconstr(struct tree *t, int idx, int ch)
{
	if (idx >= T_MAXSTR) {
		errh("constraint string too long: %s", t->t_constr);
		return;
	}
	t->t_constr[idx] = ch;
	t->t_constr[idx + 1] = '\0';
}

void
tree_addchild(struct tree *t, struct tree *child)
{
	if (t->t_nkids >= T_MAXKIDS)
		errx(1, "tree_addchild: too many children");
	t->t_kids[t->t_nkids++] = child;
}

/*
 * Compare if two trees are structurally equivalent. That is, they have the
 * same number of children which are structurally equivalent and the t_kind
 * code must be the same.
 */
static int
treecmp(const void *p, const void *q)
{
	int i, j;
	struct tree *t = *(struct tree **)p, *u = *(struct tree **)q;

	if (t->t_kind != u->t_kind)
		return t->t_kind - u->t_kind;
	if (t->t_nkids != u->t_nkids)
		return t->t_nkids - u->t_nkids;
	for (i = 0; i < t->t_nkids; i++) {
		if ((j = treecmp(&t->t_kids[i], &u->t_kids[i])) != 0)
			return j;
	}
	return 0;
}

void
errh(const char *fmt, ...)
{
	va_list ap;

	va_start(ap, fmt);
	fprintf(stderr, "%s:%zu: error: ", infile, lineno);
	vfprintf(stderr, fmt, ap);
	fprintf(stderr, "\n");
	va_end(ap);
	nerrors++;
}

void
synh(const char *fmt, ...)
{
	va_list ap;

	va_start(ap, fmt);
	fprintf(stderr, "%s:%zu syntax error: ", infile, lineno);
	vfprintf(stderr, fmt, ap);
	fprintf(stderr, "\n");
	va_end(ap);
	nerrors++;
}

void
fatalsynh(const char *fmt, ...)
{
	va_list ap;

	va_start(ap, fmt);
	fprintf(stderr, "%s:%zu: syntax error: ", infile, lineno);
	vfprintf(stderr, fmt, ap);
	fprintf(stderr, "\n");
	va_end(ap);
	exit(1);
}

static int
findname(char **tab, const char *name)
{
	int i;

	for (i = 0; i < MAXIDS && tab[i] != NULL; i++) {
		if (strcmp(tab[i], name) == 0) {
			return i;
		}
	}

	if (i == MAXIDS)
		errx(1, "too many names");
	if ((tab[i] = strdup(name)) == NULL)
		err(1, "strdup");
	return i;
}

static void *
xmalloc(size_t size)
{
	void *p;

	if ((p = malloc(size)) == NULL)
		err(1, "malloc");
	return p;
}

static void *
xcalloc(size_t nmemb, size_t size)
{
	void *p;

	if ((p = calloc(nmemb, size)) == NULL)
		err(1, "calloc");
	return p;
}

static void
iprintf(int indent, const char *fmt, ...)
{
	va_list ap;

	while (indent--)
		printf("\t");

	va_start(ap, fmt);
	vprintf(fmt, ap);
	va_end(ap);
}

static void
usage(void)
{
	extern char *__progname;

	fprintf(stderr, "usage: %s infile cfile hfile\n", __progname);
	exit(1);
}
