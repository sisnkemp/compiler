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

#ifndef COMP_CGI_H
#define COMP_CGI_H

typedef struct ir CGI_IR;

typedef struct {
	struct	ir_func *cc_fn;
	int	cc_changes;
} CGI_CTX;

#define CGI_IROP(n)		((n)->i_op)
#define CGI_DATA(n)		((n)->i_auxdata)
#define CGI_GETDATA(n)		((struct cg_data *)(n)->i_auxdata)
#define CGI_IREMIT(n, str)	ir_setemit(n, str)
#define CGI_IRCHILD(n, ch)					\
	((ch) == 0 ? (n)->i_l : ((ch) == 1 ? (n)->i_r : NULL))
#define CGI_IRCHILDP(n, ch)						\
	 ((ch) == 0 ? &(n)->i_l : ((ch) == 1 ? &(n)->i_r : NULL))

/* XXX */
#define CGI_IRDUMP(n)		ir_dump_insn(stderr, (struct ir_insn *)(n))
#define CGI_FATALX		fatalx

void cgi_prematch(CGI_IR *);
void cgi_meminit(void);
void *cgi_malloc(size_t);
void cgi_freeall(void);
void cgi_recycle(void);

/* XXX: Doesn't belong here */
struct cg_ctx {
	CGI_IR	*cc_insn;
	CGI_IR	*cc_node;
	CGI_IR	*cc_newnode;
	CGI_CTX	*cc_ctx;
};

struct cg_data;

void cg_start(void);
void cg_match(CGI_IR *, struct cg_data *);
void cg(CGI_IR *);
int cg_action(CGI_CTX *, CGI_IR *);
void cg_finish(void);

#endif /* COMP_CGI_H */
