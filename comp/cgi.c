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
 * Code generator interface and driver.
 *
 * Part of this design is based on:
 * Christopher W. Fraser, David R. Hanson and Todd A. Proebsting:
 *     Engineering a Simple, Efficient Code Generator Generator
 */

#include <sys/types.h>
#include <sys/queue.h>

#include <stdio.h>

#include "comp/cgi.h"
#include "cg.h"

#include "comp/comp.h"
#include "comp/ir.h"

struct cgdatastack {
	struct	cgdatastack *c_top;
	struct	cg_data c_data;
};

static struct memarea cgarea;
static struct cgdatastack *allcgdata;
static struct cgdatastack *nextcgdata;

void
cgi_prematch(CGI_IR *ir)
{
	struct ir_expr *x;

	if (ir->i_op == IR_REG) {
		x = (struct ir_expr *)ir;
		x->ie_regs = NULL;
	}
}

void
cgi_meminit(void)
{
	mem_area_init(&cgarea);
}

void *
cgi_malloc(size_t size)
{
	return mem_alloc(&cgarea, size);
}

static struct cg_data *
cgi_getdata(void)
{
	short *p;
	struct cg_data *cd;
	struct cgdatastack *elem;

	if (nextcgdata == NULL) {
		elem = mem_alloc(&cgarea, sizeof *elem);
		elem->c_top = allcgdata;
		allcgdata = elem;
	} else {
		elem = nextcgdata;
		nextcgdata = nextcgdata->c_top;
	}

	cd = &elem->c_data;
	cd->cd_pattern[0] = -1;
	for (p = cd->cd_cost; p < &cd->cd_cost[CG_NTERMS]; p++)
		*p = CG_MAXCOST;
	return cd;
}

void
cgi_freeall(void)
{
	mem_area_free(&cgarea);
	nextcgdata = allcgdata = NULL;
}

void
cg_start(void)
{
	cgi_meminit();
}

void
cgi_recycle(void)
{
	nextcgdata = allcgdata;
}

void
cg(CGI_IR *ir)
{
	int i;
	CGI_IR *p;
	struct cg_data *cd;

	cgi_prematch(ir);
	for (i = 0; i < ir_nkids[ir->i_op]; i++) {
		p = CGI_IRCHILD(ir, i);
		cg(p);
	}
	cd = cgi_getdata();
	CGI_DATA(ir) = cd;
	cg_match(ir, cd);
}

static int
cg_do_action(CGI_CTX *ctx, CGI_IR *insn, CGI_IR **ir, int nt)
{
	short idx;
	int i, pattern, skip = 1;
	short *nts;
	struct cg_data *cd;
	CGI_IR *n, **p;

	cd = CGI_DATA(*ir);
	if (cd == NULL) {
		CGI_IRDUMP(insn);
		CGI_FATALX("no cg_data for instruction");
	}
	pattern = cd->cd_pattern[nt];
	idx = cg_pattern_nts[pattern];
	nts = cg_nts[idx];

	for (i = 0; nts[i] != -1; i++) {
		p = cg_pattern_subtree(ir, pattern, i);
		skip &= cg_do_action(ctx, insn, p, nts[i]);
	}

	n = *ir;
	if (cg_emitstrs[pattern] != NULL)
		CGI_IREMIT(n, cg_emitstrs[pattern]);
	if (cg_hasaction[pattern]) {
		*ir = cg_actionemit(ctx, insn, n, pattern);
		skip = 0;
	}
	return skip;
}

int
cg_action(CGI_CTX *ctx, CGI_IR *ir)
{
	CGI_IR *p = ir;
	int skip;
	struct cg_data *cd;

	cd = CGI_DATA(ir);
	if (cd->cd_cost[cg_startnt] >= CG_MAXCOST) {
		CGI_IRDUMP(ir);
		CGI_FATALX("could not match instruction");
	}

	skip = cg_do_action(ctx, ir, &p, cg_startnt);
	if (ir != p)
		CGI_FATALX("attempted to rewrite top-level insn");
	return skip;
}

void
cg_finish(void)
{
	cgi_freeall();
}

int
cg_addmatch(struct cg_data *cd, int nt, int p, int cost)
{
	if (cost < cd->cd_cost[nt]) {
		cd->cd_cost[nt] = cost;
		cd->cd_pattern[nt] = p;
		return 1;
	}
	return 0;
}
