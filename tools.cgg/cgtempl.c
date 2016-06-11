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
#include <limits.h>

#define CG_MAXCOST	SHRT_MAX
#define CG_MAXMATCH	16

struct cg_data {
	int	cd_cost[CG_NTERMS];
	int	cd_rule[CG_NTERMS];
};

void cgi_meminit(void);
void *cgi_malloc(size_t);
void cgi_freeall(void);
CGI_IR *cg_actionemit(CGI_CTX *, CGI_IR *, CGI_IR *, int);

void cg_match(CGI_IR *, struct cg_data *);

void
cg_start(void)
{
	cgi_meminit();
}

void
cg(CGI_IR *ir)
{
	int i;
	CGI_IR *p;
	struct cg_data *cd;

	cgi_prematch(ir);
	for (i = 0, p = CGI_IRCHILD(ir, 0); p != NULL;
	    p = CGI_IRCHILD(ir, i + 1), i++)
		cg(p);
	cd = cgi_malloc(sizeof *cd);
	for (i = 0; i < CG_NTERMS; i++) {
		cd->cd_rule[i] = -1;
		cd->cd_cost[i] = CG_MAXCOST;
	}
	CGI_DATA(ir) = cd;
	cg_match(ir, cd);
}

static void
cg_do_action(CGI_CTX *ctx, CGI_IR *insn, CGI_IR **ir, int nt)
{
	int i, rule;
	int *nts;
	struct cg_data *cd;
	CGI_IR **p;

	cd = CGI_DATA(*ir);
	if (cd == NULL) {
		CGI_IRDUMP(insn);
		CGI_FATALX("no cg_data for instruction");
	}
	rule = cd->cd_rule[nt];
	nts = cg_rulents[rule];

	for (i = 0; nts[i] != -1; i++) {
		p = cg_rule_subtree(ir, rule, i);
		cg_do_action(ctx, insn, p, nts[i]);
	}

	*ir = cg_actionemit(ctx, insn, *ir, rule);
}

void
cg_action(CGI_CTX *ctx, CGI_IR *ir)
{
	CGI_IR *p = ir;
	struct cg_data *cd;

	cd = CGI_DATA(ir);
	if (cd->cd_cost[cg_startnt] >= CG_MAXCOST) {
		CGI_IRDUMP(ir);
		CGI_FATALX("could not match instruction");
	}

	cg_do_action(ctx, ir, &p, cg_startnt);
	if (ir != p)
		CGI_FATALX("attempted to rewrite top-level insn");
}

void
cg_finish(void)
{
	cgi_freeall();
}

void
cg_addmatch(struct cg_data *cd, int nt, int rule, int cost)
{
	if (cost < cd->cd_cost[nt]) {
		cd->cd_cost[nt] = cost;
		cd->cd_rule[nt] = rule;
	}
}
