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
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "rag.h"

static FILE *hfilefp;
static int8_t **conflict;
static struct regdefq regdefs = SIMPLEQ_HEAD_INITIALIZER(regdefs);
static struct regdef **linregdef;
static int nregs;
static struct regclassq regclasses = SIMPLEQ_HEAD_INITIALIZER(regclasses);
static int nclasses;

static void calcpq(void);
static void printconflicts(void);
static struct reg *reg(char *);
static struct regdef *regdef_find(char *);
static void *xmalloc(size_t);
static void *xcalloc(size_t, size_t);
static void usage(void);

int
main(int argc, char **argv)
{
	char *p;
	int i;
	struct reg *r;
	struct regdef *rd;
	struct regclass *rc;

	int yyparse(void);

	if (argc != 4)
		usage();

	if (freopen(argv[1], "r", stdin) == NULL)
		err(1, "couldn't open %s", argv[1]);
	if (freopen(argv[2], "w", stdout) == NULL)
		err(1, "couldn't open %s", argv[2]);
	if ((hfilefp = fopen(argv[3], "w")) == NULL)
		err(1, "couldn't open %s", argv[3]);

	yyparse();
	fprintf(hfilefp, "#ifdef REG_NREGS_ONLY\n");
	fprintf(hfilefp, "#define REG_NREGS %d\n", nregs);
	fprintf(hfilefp, "#else\n");
	fprintf(hfilefp, "#ifndef REG_H\n#define REG_H\n\n");
	printf("#include <sys/types.h>\n");
	printf("#include <sys/queue.h>\n\n");
	printf("#include \"comp/comp.h\"\n");
	printf("#include \"comp/ir.h\"\n");
	printf("#include \"%s\"\n\n", argv[3]);

	/*
	 * Make conflict relation symmetric and each register conflict with
	 * itself. Make sure that all registers are declared and sort them
	 * by id.
	 */
	linregdef = xcalloc(nregs, sizeof *linregdef);
	conflict = xcalloc(nregs, sizeof *conflict);
	for (i = 0; i < nregs; i++) {
		conflict[i] = xcalloc(nregs, sizeof **conflict);
		conflict[i][i] = 1;
	}
	SIMPLEQ_FOREACH(rd, &regdefs, r_link) {
		if (rd->r_id == -1)
			errx(1, "undefined register %s", rd->r_name);
		linregdef[rd->r_id] = rd;
		if (rd->r_conflicts == NULL)
			continue;
		SIMPLEQ_FOREACH(r, rd->r_conflicts, r_link) {
			conflict[r->r_def->r_id][rd->r_id] = 1;
			conflict[rd->r_id][r->r_def->r_id] = 1;
		}
	}

	printf("struct ir_symbol *physregs[REG_NREGS];\n");
	printf("static char *regnames[] = {\n");
	for (i = 0; i < nregs; i++) {
		rd = linregdef[i];
		printf("\t\"%%%s\"", rd->r_name);
		if (i != nregs - 1)
			printf(",");
		printf("\n");
		fprintf(hfilefp, "#define REG_");
		for (p = rd->r_name; *p != '\0'; p++) {
			if (isalpha(*p))
				putc(toupper(*p), hfilefp);
			else
				putc(*p, hfilefp);
		}
		fprintf(hfilefp, " %d\n", i);
	}
	printf("};\n\n");
	fprintf(hfilefp, "#define REG_NREGS %d\n\n", nregs);
	fprintf(hfilefp, "extern struct regset reg_nonvolat;\n");
	fprintf(hfilefp, "extern struct regset reg_volat;\n");
	fprintf(hfilefp, "extern struct regset regclasses[%d];\n\n", nclasses);
	fprintf(hfilefp, "void reginit(void);\n");
	fprintf(hfilefp, "void regset_init(struct regset *);\n");
	fprintf(hfilefp, "void regset_addreg(struct regset *, int);\n\n");
	SIMPLEQ_FOREACH(rc, &regclasses, r_link) {
		fprintf(hfilefp, "#define reg_%s (&regclasses[%d])\n",
		    rc->r_name, rc->r_id);
		fprintf(hfilefp, "#define REG_");
		for (p = rc->r_name; *p != '\0'; p++) {
			if (isalpha(*p))
				putc(toupper(*p), hfilefp);
			else
				putc(*p, hfilefp);
		}
		fprintf(hfilefp, " %d\n", rc->r_id);
	}

	fprintf(hfilefp, "\n");
	printf("struct regset reg_nonvolat;\n");
	printf("struct regset reg_volat;\n");
	printf("struct regset regclasses[%d];\n", nclasses);

	printf("void\nreginit(void)\n{\n");
	printf("\tint i;\n");
	printf("\n\n");
	printf("\tfor (i = 0; i < REG_NREGS; i++)\n");
	printf("\t\tphysregs[i] = ir_physregsym(regnames[i], i);\n");
	printf("\n\tregset_init(&reg_nonvolat);\n");
	printf("\tregset_init(&reg_volat);\n");
	SIMPLEQ_FOREACH(rd, &regdefs, r_link) {
		if (!rd->r_volatile)
			printf("\tregset_addreg(&reg_nonvolat, %d);\n",
			    rd->r_id);
		else
			printf("\tregset_addreg(&reg_volat, %d);\n", rd->r_id);
	}
	SIMPLEQ_FOREACH(rc, &regclasses, r_link) {
		printf("\tregset_init(&regclasses[%d]);\n", rc->r_id);
		SIMPLEQ_FOREACH(r, rc->r_regs, r_link)
			printf("\tregset_addreg(&regclasses[%d], %d);\n",
			    rc->r_id, r->r_def->r_id);
	}
	printf("}\n");

	printconflicts();
	calcpq();
	fprintf(hfilefp, "#endif /* REG_H */\n");
	fprintf(hfilefp, "#endif /* REG_NREGS_ONLY */\n");
	return 0;
}

static void
printconflicts(void)
{
	int i, j;

	fprintf(hfilefp, "extern short *reg_conflicts[];\n");

	for (i = 0; i < nregs; i++) {
		printf("static short reg%dconf[] = {", i);
		for (j = 0; j < nregs; j++) {
			if (conflict[i][j])
				printf(" %d,", j);
		}
		printf(" -1 };\n");
	}
	printf("\nshort *reg_conflicts[] = {\n");
	for (i = 0; i < nregs; i++) {
		printf("\treg%dconf", i);
		if (i != nregs - 1)
			printf(",");
		printf("\n");
	}
	printf("};\n");
}

static void
calcpq(void)
{
	int max, n;
	struct reg *rb, *rc;
	struct regclass *rcb, *rcc;

	fprintf(hfilefp, "extern int reg_pb[];\n");

	printf("int reg_pb[] = {");
	SIMPLEQ_FOREACH(rcc, &regclasses, r_link) {
		printf(" %d", rcc->r_nregs);
		if (SIMPLEQ_NEXT(rcc, r_link) != NULL)
			printf(",");
	}
	printf(" };\n");

	fprintf(hfilefp, "extern int reg_qbc[%d][%d];\n", nclasses, nclasses);

	printf("int reg_qbc[%d][%d] = {", nclasses, nclasses);
	SIMPLEQ_FOREACH(rcb, &regclasses, r_link) {
		printf(" {");
		SIMPLEQ_FOREACH(rcc, &regclasses, r_link) {
			max = 0;
			SIMPLEQ_FOREACH(rc, rcc->r_regs, r_link) {
				n = 0;
				SIMPLEQ_FOREACH(rb, rcb->r_regs, r_link) {
					n += conflict[rc->r_def->r_id]
					    [rb->r_def->r_id];
				}
				if (n > max)
					max = n;
			}
			printf(" %d", max);
			if (SIMPLEQ_NEXT(rcc, r_link) != NULL)
				printf(",");
		}
		printf(" }");
		if (SIMPLEQ_NEXT(rcb, r_link) != NULL)
			printf(",");
	}
	printf(" };\n");
}

static struct reg *
reg(char *name)
{
	struct reg *r;

	r = xmalloc(sizeof *r);
	r->r_def = regdef_find(name);
	return r;
}

struct regq *
regq(char *name)
{
	struct reg *r;
	struct regq *rq;

	rq = xmalloc(sizeof *rq);
	SIMPLEQ_INIT(rq);
	r = reg(name);
	SIMPLEQ_INSERT_TAIL(rq, r, r_link);
	return rq;
}

void
regq_enq(struct regq *regq, char *name)
{
	struct reg *r;

	SIMPLEQ_FOREACH(r, regq, r_link) {
		if (strcmp(name, r->r_def->r_name) == 0)
			errx(1, "register %s is already in list", name);
	}
	r = reg(name);
	SIMPLEQ_INSERT_TAIL(regq, r, r_link);
}

static struct regdef *
regdef_find(char *name)
{
	struct regdef *rd;

	SIMPLEQ_FOREACH(rd, &regdefs, r_link) {
		if (strcmp(rd->r_name, name) == 0)
			return rd;
	}
	rd = xmalloc(sizeof *rd);
	rd->r_conflicts = NULL;
	rd->r_name = name;
	rd->r_id = -1;
	rd->r_volatile = 0;
	SIMPLEQ_INSERT_TAIL(&regdefs, rd, r_link);
	return rd;
}

void
regdef(char *name, int volat, struct regq *conflicts)
{
	struct regdef *rd;

	static int nextid;

	rd = regdef_find(name);
	if (rd->r_id != -1)
		errx(1, "register %s already defined", name);
	rd->r_id = nextid++;
	rd->r_conflicts = conflicts;
	rd->r_volatile = volat;
	nregs++;
}

void
regclass(char *name, struct regq *regs)
{
	int n = 0;
	struct reg *r;
	struct regclass *rc;

	static int nextid;

	SIMPLEQ_FOREACH(rc, &regclasses, r_link) {
		if (strcmp(rc->r_name, name) == 0)
			errx(1, "class %s already defined", name);
	}
	n = 0;
	SIMPLEQ_FOREACH(r, regs, r_link)
		n++;
	rc = xmalloc(sizeof *rc);
	rc->r_regs = regs;
	rc->r_name = name;
	rc->r_id = nextid++;
	rc->r_nregs = n;
	SIMPLEQ_INSERT_TAIL(&regclasses, rc, r_link);
	nclasses++;
}

static void *
xmalloc(size_t size)
{
	void *p;

	if ((p = malloc(size)) == NULL)
		err(1, "xmalloc");
	return p;
}

static void
*xcalloc(size_t nmemb, size_t size)
{
	void *p;

	if ((p = calloc(nmemb, size)) == NULL)
		err(1, "xcalloc");
	return p;
}

static void
usage(void)
{
	extern char *__progname;

	fprintf(stderr, "usage: %s infile cfile hfile\n", __progname);
	exit(1);
}
