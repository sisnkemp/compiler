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

#include <ctype.h>
#include <err.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "cgg.h"

static FILE *templfp;
static int ntents;
static struct nonterm nonterms[NTMAX];
static struct nonterm *startnt;
static struct ruleq rulesq = SIMPLEQ_HEAD_INITIALIZER(rulesq);
static int nrules;
static int maxtrees;

static void usage(void);
static void *xmalloc(size_t);
static struct treepat *treepat(int);
static void rule_addch(char *, int, size_t, const char *);
static void gencg(void);
static int extractnts(struct treepat *);
static void genrulents(void);
static void genrulekidap(struct rule *, int);
static void genrulegetkid(void);
static void genactionemit(void);
static void genmatch(void);
static void emitinput(void);
static void emitchains(void);
static void emitrule(struct rule *);
static void emittp(struct treepat *);

int
main(int argc, char **argv)
{
	int i;

	if (argc < 2)
		usage();
	if ((templfp = fopen(argv[1], "r")) == NULL)
		err(1, "couldn't open template file");
	if (argc > 2) {
		if (freopen(argv[2], "r", stdin) == NULL)
			err(1, "couldn't open %s", argv[2]);
		if (argc > 3) {
			if (freopen(argv[3], "w", stdout) == NULL)
				err(1, "couldn't open %s", argv[2]);
		}
	}
	parse();
	for (i = 0; i < ntents; i++) {
		if (SIMPLEQ_EMPTY(&nonterms[i].n_rules))
			errx(1, "unused non-terminal %s", nonterms[i].n_name);
	}

	gencg();
	return 0;
}

static void
usage(void)
{
	extern char *__progname;

	fprintf(stderr, "usage: %s template infile outfile\n", __progname);
	exit(1);
}

static void *
xmalloc(size_t size)
{
	void *rv;

	if ((rv = malloc(size)) == NULL)
		err(1, "xmalloc");
	return rv;
}

static struct treepat *
treepat(int op)
{
	int i;
	struct treepat *tp;

	tp = xmalloc(sizeof *tp);
	tp->t_op = op;
	for (i = 0; i < TP_MAXKIDS; i++)
		tp->t_kids[i] = NULL;
	tp->t_constr[0] = '\0';
	tp->t_termid = NULL;
	tp->t_nkids = 0;
	tp->t_ntot = 1;
	return tp;
}

struct treepat *
tp_nterm(struct nonterm *nt)
{
	struct treepat *tp;

	tp = treepat(TP_NTERM);
	tp->t_nt = nt;
	return tp;
}

struct treepat *
tp_term(char *id)
{
	size_t len;
	struct treepat *tp;

	len = strlen(id);
	tp = treepat(TP_TERM);
	tp->t_termid = xmalloc(len + 1);
	strlcpy(tp->t_termid, id, len + 1);
	return tp;
}

void
tp_add_kid(struct treepat *tp, struct treepat *kid)
{
	if (tp->t_op == TP_NTERM)
		errx(1, "can't add subtrees to nonterminals");
	if (tp->t_nkids >= TP_MAXKIDS)
		errx(1, "too many kids");
	tp->t_kids[tp->t_nkids++] = kid;
	tp->t_ntot += kid->t_ntot;
	if (tp->t_ntot > maxtrees)
		maxtrees = tp->t_ntot;
}

void
tp_add_constrch(struct treepat *tp, int ch, size_t pos)
{
	if (pos >= TP_MAXSTR - 1)
		errx(1, "constraint too long");
	tp->t_constr[pos] = ch;
	tp->t_constr[pos + 1] = '\0';
}

int
tp_has_constr(struct treepat *tp)
{
	return tp->t_constr[0] != '\0';
}

struct rule *
rule(void)
{
	struct rule *r;
	static int nextid;

	r = xmalloc(sizeof *r);
	r->r_cost[0] = r->r_action[0] = r->r_emit[0] = '\0';
	r->r_id = nextid++;
	r->r_tp = NULL;
	r->r_lhs = NULL;
	r->r_nts = 0;
	return r;
}

static void
rule_addch(char *buf, int ch, size_t pos, const char *what)
{
	if (pos >= R_MAXSTR - 1)
		errx(1, "%s too long: %s", what, buf);
	buf[pos] = ch;
	buf[pos + 1] = '\0';
}

void
rule_add_costch(struct rule *r, int ch, size_t pos)
{
	rule_addch(r->r_cost, ch, pos, "cost");
}

void
rule_add_actionch(struct rule *r, int ch, size_t pos)
{
	rule_addch(r->r_action, ch, pos, "action");
}

void
rule_add_emitch(struct rule *r, int ch, size_t pos)
{
	rule_addch(r->r_emit, ch, pos, "emit");
}

void
rule_add_tp(struct rule *r, struct treepat *tp)
{
	r->r_tp = tp;
}

int
rule_has_cost(struct rule *r)
{
	return r->r_cost[0] != '\0';
}

int
rule_has_emit(struct rule *r)
{
	return r->r_emit[0] != '\0';
}

int
rule_has_action(struct rule *r)
{
	return r->r_action[0] != '\0';
}

struct nonterm *
nonterm(char *id)
{
	char *p;
	struct nonterm *nt;

	for (p = id; *p != '\0'; p++) {
		if (!islower(*p) && !isdigit(*p))
			errx(1, "nonterminals must be lowercase: %s", id);
	}

	if ((nt = nonterm_find(id)) != NULL)
		return nt;
	if (ntents == NTMAX)
		errx(1, "too many nonterminals");
	if ((p = strdup(id)) == NULL)
		err(1, "strdup");
	nonterms[ntents].n_name = p;
	nonterms[ntents].n_id = ntents;
	SIMPLEQ_INIT(&nonterms[ntents].n_rules);
	SIMPLEQ_INIT(&nonterms[ntents].n_chainrq);
	ntents++;
	return &nonterms[ntents - 1];
}

struct nonterm *
nonterm_find(char *id)
{
	int i;

	for (i = 0; i < ntents; i++) {
		if (strcmp(id, nonterms[i].n_name) == 0)
			return &nonterms[i];
	}
	return NULL;
}

void
nonterm_addrule(struct nonterm *nt, struct rule *r)
{
	SIMPLEQ_INSERT_TAIL(&nt->n_rules, r, r_link);
	SIMPLEQ_INSERT_TAIL(&rulesq, r, r_glolink);
	nrules++;
	r->r_lhs = nt;
	if (r->r_tp->t_op == TP_NTERM)
		SIMPLEQ_INSERT_TAIL(&r->r_tp->t_nt->n_chainrq, r, r_chlink);
}

void
nonterm_add_chain(struct nonterm *nt, struct rule *r)
{
	SIMPLEQ_INSERT_TAIL(&nt->n_chainrq, r, r_chlink);
}

void
nonterm_set_start(struct nonterm *nt)
{
	startnt = nt;
}

static void
gencg(void)
{
	int ch, i;

	if (startnt == NULL)
		errx(1, "no start non-terminal");

	printf("#if 0\n");
	printf("start: %s\n", startnt->n_name);
	emitchains();
	emitinput();
	printf("#endif\n\n");

	printf("#define CG_NTERMS\t%d\n\n", ntents);

	printf("static int cg_startnt = %d;\n\n", startnt->n_id);

	genrulents();
	genrulegetkid();

	while ((ch = getc(templfp)) != EOF)
		putchar(ch);
	if (ferror(templfp))
		err(1, "error reading template");

	genactionemit();
	printf("\n\n");
	genmatch();
}

static int
extractnts(struct treepat *tp)
{
	int i, rv = 0;

	if (tp->t_op == TP_NTERM) {
		printf("%d, ", tp->t_nt->n_id);
		return 1;
	}

	for (i = 0; i < tp->t_nkids; i++)
		rv += extractnts(tp->t_kids[i]);
	return rv;
}

static void
genrulents(void)
{
	struct rule *r;

	SIMPLEQ_FOREACH(r, &rulesq, r_glolink) {
		printf("static int cg_rule%d_nts[] = { ", r->r_id);
		r->r_nts = extractnts(r->r_tp);
		printf("-1 };\n");
	}

	printf("\nstatic int *cg_rulents[] = {\n");
	SIMPLEQ_FOREACH(r, &rulesq, r_glolink) {
		printf("\tcg_rule%d_nts", r->r_id);
		if (SIMPLEQ_NEXT(r, r_glolink) != NULL)
			printf(",");
		printf("\n");
	}
	printf("};\n\n");
}

struct apkid {
	SIMPLEQ_ENTRY(apkid) a_link;
	int	a_kid;
};

struct apctx {
	SIMPLEQ_HEAD(, apkid) a_kidq;
	int	a_no;
	int	a_curno;
	int	a_depth;
};

static int
dogenrulekidap(struct treepat *tp, struct apctx *ac)
{
	int i;
	struct apkid *akp, ak;

	if (tp->t_op == TP_NTERM) {
		if (ac->a_curno == ac->a_no) {
			if (ac->a_depth)
				printf("CGI_IRCHILDP(");
			for (i = 1; i < ac->a_depth; i++)
				printf("CGI_IRCHILD(");
			if (ac->a_depth != 0)
				printf("*");
			printf("ir");
			for (i = 0; i < ac->a_depth; i++) {
				akp = SIMPLEQ_FIRST(&ac->a_kidq);
				SIMPLEQ_REMOVE_HEAD(&ac->a_kidq, a_link);
				printf(", %d)", akp->a_kid);
			}
			return 1;
		}
		ac->a_curno++;
		return 0;
	}

	SIMPLEQ_INSERT_TAIL(&ac->a_kidq, &ak, a_link);
	ac->a_depth++;
	for (i = 0; i < tp->t_nkids; i++) {
		ak.a_kid = i;
		if (dogenrulekidap(tp->t_kids[i], ac)) {
			ac->a_depth--;
			return 1;
		}
	}
	ac->a_depth--;
	return 0;
}

static void
genrulekidap(struct rule *r, int no)
{
	int curno = 0, arg = 0;
	struct apctx ac;

	SIMPLEQ_INIT(&ac.a_kidq);
	ac.a_no = no;
	ac.a_curno = 0;
	ac.a_depth = 0;
	if (!dogenrulekidap(r->r_tp, &ac))
		errx(1, "could not get access path for rule %d, kid %d",
		    r->r_id, no);
}

static void
genrulegetkid(void)
{
	int i;
	struct rule *r;

	printf("CGI_IR **\n");
	printf("cg_rule_subtree(CGI_IR **ir, int rule, int no)\n{\n");
	printf("\tCGI_IR **st = NULL;\n\n");
	printf("\tswitch (rule) {\n");

	SIMPLEQ_FOREACH(r, &rulesq, r_glolink) {
		printf("\tcase %d:\n", r->r_id);
		printf("\t\tswitch (no) {\n");
		for (i = 0; i < r->r_nts; i++) {
			printf("\t\tcase %d:\n", i);
			printf("\t\t\tst = ");
			genrulekidap(r, i);
			printf(";\n");
			printf("\t\t\tbreak;\n");
		}
		printf("\t\tdefault:\n");
		printf("\t\t\tCGI_FATALX(\"cg_rule_subtree: bad no %%d for "
		    " rule %%d\", no, rule);\n");
		printf("\t\t}\n");
		printf("\t\tbreak;\n");
	}

	printf("\tdefault:\n\t\tCGI_FATALX(\"cg_rule_subtree: bad rule: %%d\","
	    " rule);\n");
	printf("\t}\n");
	printf("\treturn st;\n");
	printf("}\n\n");
}

static void
genactionemit(void)
{
	struct rule *r;

	printf("CGI_IR *\n");
	printf("cg_actionemit(CGI_CTX *ctx, CGI_IR *insn, CGI_IR *node, "
	    "int ruleno)\n{\n");
	printf("\tstruct cg_ctx nctx = { insn, node, node, ctx };\n");
	printf("\tchar *emitstr = NULL;\n\n");
	printf("\tswitch (ruleno) {\n");
	SIMPLEQ_FOREACH(r, &rulesq, r_glolink) {
		printf("\tcase %d:\n", r->r_id);
		if (rule_has_emit(r))
			printf("\t\temitstr = %s;\n", r->r_emit);
		if (rule_has_action(r))
			printf("\t\t%s;\n", r->r_action);
		printf("\t\tbreak;\n");
	}
	printf("\tdefault:\n");
	printf("\t\tCGI_FATALX(\"cg_actionemit: bad rule: %%d\", ruleno);\n");
	printf("\t}\n");
	printf("\tif (emitstr != NULL)\n");
	printf("\t\tCGI_IREMIT(node, emitstr);\n");
	printf("\treturn nctx.cc_newnode;\n");
	printf("}\n");
}

static int
gentpmatch(struct treepat *tp, int no)
{
	int i, next = no + 1;

	if (tp->t_op == TP_TERM)
		printf("CGI_IROP(ir%d) == %s", no, tp->t_termid);
	else
		printf("CGI_GETDATA(ir%d)->cd_cost[%d] < CG_MAXCOST",
		    no, tp->t_nt->n_id);

	for (i = 0; i < tp->t_nkids; i++) {
		printf(" &&\n\t    (ir%d = CGI_IRCHILD(ir%d, %d),",
		    next, no, i);
		if (tp->t_kids[i]->t_op == TP_NTERM) {

			printf("\n\t        ");
			printf("cost0 += CGI_GETDATA(ir%d)->cd_cost[%d],",
			    next, tp->t_kids[i]->t_nt->n_id);
		}

		printf("\n\t        1) &&\n\t    ");
		next = gentpmatch(tp->t_kids[i], next);
	}

	if (tp_has_constr(tp))
		printf(" &&\n\t    (n = ir%d, %s)", no, tp->t_constr);
	return next;
}

static void
prindent(int indent)
{
	while (indent--)
		printf("\t");
}

static int
genchainmatch(int indent, int no, struct nonterm *start, struct nonterm *nt)
{
	int i, next = no + 1;
	struct rule *r;

	SIMPLEQ_FOREACH(r, &nt->n_chainrq, r_chlink) {
		if (r->r_tp->t_op != TP_NTERM)
			errx(1, "found TP_TERM in chain rule");
		prindent(indent);
		if (tp_has_constr(r->r_tp))
			printf("if (%s) ", r->r_tp->t_constr);
		printf("{\n");
		prindent(indent + 1);
		printf("int cost%d = cost%d + ", next, no);
		if (rule_has_cost(r))
			printf("%s;\n", r->r_cost);
		else
			printf("1;\n");
		prindent(indent + 1);
		printf("cg_addmatch(CGI_DATA(ir0), %d, %d, cost%d);\n",
		    r->r_lhs->n_id, r->r_id, next);
		if (r->r_lhs == start)
			errx(1, "found cycle in chain rules");
		next = genchainmatch(indent + 1, next, start, r->r_lhs);
		prindent(indent);
		printf("}\n");
	}

	return next;
}

static void
genmatch(void)
{
	int i;
	struct rule *r;

	printf("void\ncg_match(CGI_IR *ir0, struct cg_data *cd)\n{\n");
	printf("\tCGI_IR *n;\n");
	printf("\tint cost0;\n");

	for (i = 0; i < maxtrees - 1; i++)
		printf("\tCGI_IR *ir%d = NULL;\n", i + 1);
	printf("\n");

	SIMPLEQ_FOREACH(r, &rulesq, r_glolink) {
		if (r->r_tp->t_op == TP_NTERM)
			continue;
		printf("\t/* match rule %d */\n", r->r_id);
		printf("\tcost0 = 0;\n");
		printf("\tn = ir0;\n");
		printf("\tif (");
		gentpmatch(r->r_tp, 0);
		printf("&&\n\t    (cost0 += ");
		if (rule_has_cost(r))
			printf("%s", r->r_cost);
		else
			printf("1");
		printf(") < CGI_GETDATA(ir0)->cd_cost[%d]",
		    r->r_lhs->n_id);
		printf(") {\n");
		printf("\t\tn = ir0;\n");
		printf("\t\tcg_addmatch(CGI_DATA(ir0), %d, %d, cost0);\n",
		    r->r_lhs->n_id, r->r_id);
		genchainmatch(2, 0, r->r_lhs, r->r_lhs);
		printf("\t}\n");
	}
	printf("}\n");
}

static void
emitchains(void)
{
	int i;
	struct nonterm *nt;
	struct rule *r;

	for (i = 0; i < ntents; i++) {
		nt = &nonterms[i];
		if (SIMPLEQ_EMPTY(&nt->n_chainrq))
			continue;
		printf("%s chains to ", nt->n_name);
		SIMPLEQ_FOREACH(r, &nt->n_chainrq, r_chlink) {
			printf("%s", r->r_lhs->n_name);
			if (SIMPLEQ_NEXT(r, r_chlink) != 0)
				printf(", ");
		}
		printf("\n");
	}
	printf("\n");
}

static void
emitinput(void)
{
	int i;
	struct rule *r;

	for (i = 0; i < ntents; i++) {
		printf("%s = %d:\t", nonterms[i].n_name, nonterms[i].n_id);
		SIMPLEQ_FOREACH(r, &nonterms[i].n_rules, r_link) {
			emitrule(r);
			if (SIMPLEQ_NEXT(r, r_link) != NULL)
				printf("\t| ");
		}
		printf("\t;\n");
		if (i != ntents - 1)
			printf("\n");
	}
}

static void
emitrule(struct rule *r)
{
	emittp(r->r_tp);
	if (r->r_cost[0] != '\0')
		printf("\n\t    cost {%s}", r->r_cost);
	if (r->r_emit[0] != '\0')
		printf("\n\t    emit {%s}", r->r_emit);
	if (r->r_action[0] != '\0')
		printf("\n\t    action {%s}", r->r_action);
	printf(" /* rule %d */\n", r->r_id);
}

static void
emittp(struct treepat *tp)
{
	int i;

	switch (tp->t_op) {
	case TP_TERM:
		printf("%s", tp->t_termid);
		break;
	case TP_NTERM:
		printf("%s", tp->t_nt->n_name);
		break;
	default:
		errx(1, "emittp: bad op: %d", tp->t_op);
	}

	if (tp->t_constr[0] != '\0')
		printf("[%s]", tp->t_constr);

	if (tp->t_nkids == 0)
		return;
	printf("(");
	for (i = 0; i < tp->t_nkids; i++) {
		emittp(tp->t_kids[i]);
		if (i != tp->t_nkids - 1)
			printf(", ");
	}
	printf(")");
}
