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

#ifndef CG_H
#define CG_H

#define CG_MAXCOST	SHRT_MAX
#define CG_NTERMS	35

struct cg_data {
	short	cd_cost[CG_NTERMS];
	short	cd_pattern[CG_NTERMS];
};
extern int cg_startnt;
extern short *cg_nts[];
extern short cg_pattern_nts[];
extern int8_t cg_hasaction[];
extern char *cg_emitstrs[];
CGI_IR **cg_pattern_subtree(CGI_IR **, int, int);
CGI_IR *cg_actionemit(CGI_CTX *, CGI_IR *, CGI_IR *, int);
int cg_addmatch(struct cg_data *, int, int, int);

#endif /* CG_H */
