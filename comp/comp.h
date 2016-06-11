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

#ifndef COMP_COMP_H
#define COMP_COMP_H

#include <limits.h>
#include <stdio.h>

/* XXX */
#define REG_NREGS_ONLY
#include "reg.h"
#undef REG_NREGS_ONLY

#define BITVEC_BITSPERELEM	(sizeof(u_int) * CHAR_BIT)
#define BITVEC_BITSPERELEM_SHIFT	5
#define BITVEC_BITSTOELEMS(nbit)					\
	(((nbit) + BITVEC_BITSPERELEM - 1) / BITVEC_BITSPERELEM)

#define BITVEC(name, nbit)	struct name {		\
	size_t	b_nbit;					\
	size_t	b_nelem;				\
	u_int	b_bits[BITVEC_BITSTOELEMS(nbit)];	\
}

#define BITVEC_INITIALIZER(nbit) { (nbit), BITVEC_BITSTOELEMS(nbit) }

BITVEC(regset, REG_NREGS);

#include "targconf.h"

#define COMPOPTS "IPS"

extern int Iflag;

void compopt(int);
void comp_init(void);
__dead void comp_exit(int);
void compile(void);
void targinit(void);

void setinfile(char *);
void setoutfile(const char *);

extern char *infile;

FILE *dump_open(const char *, const char *, const char *, int);

__dead void fatal(const char *, ...);
__dead void fatalx(const char *, ...);

void emitf(const char *, ...);

#define emits(str)			fputs(str, stdout)
#define emitc(c)			putc(c, stdout)
#define EMITWRITE(ptr, size, nmemb)	fwrite(ptr, size, nmemb, stdout)

void *xmalloc(size_t);
void *xmnalloc(size_t, size_t);
void *xcalloc(size_t, size_t);
void *xrealloc(void *, size_t);
char *xstrdup(const char *);

extern size_t xmallocd;

struct srcpos {
	char	*s_file;
	size_t	s_line;
};

extern struct srcpos cursp;

#define SPCPY(dst, src)	do {	\
	*(dst) = *(src);	\
} while (0)

struct ntent {
	struct	ntent *n_next;
	char	*n_name;
};

#define NTRIE_MAX	54
#define NTRIE_LTAG	1
#define NTRIE_RTAG	2

struct ntrie {
	struct	ntrie *n_llink;
	struct	ntrie *n_rlink;
	char	*n_key;
	int	n_tag;
	int	n_skip;
};

struct nametab {
	struct	ntent *n_names;
	struct	ntrie *n_tries[NTRIE_MAX];
};

extern struct nametab names;

void ntinit(struct nametab *);
char *ntenter(struct nametab *, const char *);

int newid(void);

extern struct ir_symbol *physregs[];

struct memchunk {
	SLIST_ENTRY(memchunk) m_next;
	char	*m_base;
	char	*m_nextptr;
	size_t	m_total;
	size_t	m_avail;
};

SLIST_HEAD(memchunkq, memchunk);

struct memarea {
	struct	memchunkq m_avail;
	struct	memchunkq m_full;
};

struct memstat {
	size_t	m_allocd;
	size_t	m_freed;
	size_t	m_peakusage;
	size_t	m_minsize;
	size_t	m_maxsize;
	size_t	m_avgalloc;
	size_t	m_nalloc;
	size_t	m_nsearch;
};

extern struct memstat memstats;

void mem_area_init(struct memarea *);
void mem_area_free(struct memarea *);
void *mem_alloc(struct memarea *, size_t);
void *mem_mnalloc(struct memarea *, size_t, size_t);
void *mem_calloc(struct memarea *, size_t, size_t);

extern size_t memallocd;
extern size_t memfreed;
extern size_t mempeakusage;

BITVEC(bitvec, 1);

struct bitvec *bitvec_alloc(struct memarea *, size_t);
void bitvec_init(struct bitvec *, size_t);
int bitvec_isset(struct bitvec *, size_t);
size_t bitvec_firstset(struct bitvec *);
size_t bitvec_nextset(struct bitvec *, size_t);
void bitvec_clearbit(struct bitvec *, size_t);
void bitvec_clearall(struct bitvec *);
void bitvec_setbit(struct bitvec *, size_t);
void bitvec_setall(struct bitvec *);
void bitvec_not(struct bitvec *);
void bitvec_and(struct bitvec *, struct bitvec *);
void bitvec_or(struct bitvec *, struct bitvec *);
int bitvec_cmp(struct bitvec *, struct bitvec *);
void bitvec_cpy(struct bitvec *, struct bitvec *);

#define BITVEC_INIT(bv, nbit)	bitvec_init((struct bitvec *)(bv), (nbit))
#define BITVEC_ISSET(bv, bit)	bitvec_isset((struct bitvec *)(bv), (bit))
#define BITVEC_FIRSTSET(bv)	bitvec_firstset((struct bitvec *)(bv))
#define BITVEC_NEXTSET(bv, i)	bitvec_nextset((struct bitvec *)(bv), (i))
#define BITVEC_CLEARBIT(bv, bit)			\
	bitvec_clearbit((struct bitvec *)(bv), bit)
#define BITVEC_CLEARALL(bv)	bitvec_clearall((struct bitvec *)(bv))
#define BITVEC_SETBIT(bv, bit)	bitvec_setbit((struct bitvec *)(bv), (bit))
#define BITVEC_SETALL(bv)	bitvec_setall((struct bitvec *)(bv))
#define BITVEC_CPY(dst, src)						\
	bitvec_cpy((struct bitvec *)(dst), (struct bitvec *)(src))

struct regconstr {
	int	*r_regs;
	int	r_how;
};

#define RC_ALLOWED	1
#define RC_FORBIDDEN	2

struct tmpreg {
	struct	regconstr *t_rc;
	struct	ir_type *t_type;
};

void emitcstring(FILE *, char *, size_t);

struct cfadata {
	struct	memarea c_ma;
	SIMPLEQ_HEAD(, cfa_bb) c_bbqh;
	struct	cfa_bb *c_entry;
	struct	cfa_bb *c_exit;
	int	c_nbb;
	int	c_edges;
};

struct cfa_bblink {
	SIMPLEQ_ENTRY(cfa_bblink) cb_link;
	struct	cfa_bb *cb_bb;
};

SIMPLEQ_HEAD(cfa_bblist, cfa_bblink);

struct cfa_bb {
	SIMPLEQ_ENTRY(cfa_bb) cb_glolink;
	struct	cfa_bblist cb_edges[2];
	struct	cfa_bblist cb_idomkids;
	void	*cb_dfasets[2];
	void	*cb_inset;
	void	*cb_outset;
	struct	cfa_bb *cb_immdom;
	struct	ir_insn *cb_first;
	struct	ir_insn *cb_last;
	struct	bitvec *cb_df;
	int	cb_id;
	int	cb_dfsno;

#define CFA_BB_PREDS	0
#define CFA_BB_SUCCS	1
#define CFA_BB_INSET	0
#define CFA_BB_OUTSET	1

#define cb_preds	cb_edges[0]
#define cb_succs	cb_edges[1]
#define cb_inset	cb_dfasets[0]
#define cb_outset	cb_dfasets[1]
};

struct ir_func;

void cfa_buildcfg(struct ir_func *);
void cfa_calcdom(struct ir_func *);
void cfa_calcdf(struct ir_func *);
void cfa_bb_prepend(struct ir_func *, struct cfa_bb *, struct ir_insn *);
void cfa_bb_append(struct ir_func *, struct cfa_bb *, struct ir_insn *);
void cfa_bb_delinsn(struct ir_func *, struct cfa_bb *, struct ir_insn *);
void cfa_bb_prepend_insn(struct ir_insn *, struct ir_insn *);
void cfa_bb_append_insn(struct ir_func *, struct ir_insn *, struct ir_insn *);

#define CFA_CFGSORT_ASC		1
#define CFA_CFGSORT_DESC	-1

void cfa_cfgsort(struct ir_func *, int);
void cfa_free(struct ir_func *);

struct dfadata {
	struct	bitvec *d_livein;
	struct	bitvec *d_liveout;
};

void dfa_initdata(struct dfadata *);

void dfa_livevar(struct ir_func *);

#endif /* COMP_COMP_H */
