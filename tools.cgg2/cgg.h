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

#ifndef TOOLS_CGG_CGG_H
#define TOOLS_CGG_CGG_H

#include <sys/types.h>
#include <sys/queue.h>

#define TOK_START	256
#define TOK_ID		256
#define TOK_COST	257
#define TOK_EMIT	258
#define TOK_ACTION	259
#define TOK_LVERB	260
#define TOK_RVERB	261
#define TOK_PERC	262
#define TOK_END		262

extern char *tokstr;
extern size_t lineno;

int yylex(void);
void verbatim(void);
void noverbatim(void);
void errh(const char *, ...);
void synh(const char *, ...);
void fatalsynh(const char *, ...);
void parse(void);

struct nonterm {
	SIMPLEQ_HEAD(, pattern) n_chainpats;
	struct	rule *n_rule;
	int	n_init;
	int	n_noclosure;
};

struct rule {
	SIMPLEQ_ENTRY(rule) r_glolink;
	SIMPLEQ_HEAD(, pattern) r_patterns;
	short	r_nterm;
};

struct pattern;

struct rule *rule(const char *);
void rule_addpattern(struct rule *, struct pattern *);

#define P_MAXSTR	2048

struct pattern {
	SIMPLEQ_ENTRY(pattern) p_rlink;
	SIMPLEQ_ENTRY(pattern) p_ntslink;
	SIMPLEQ_ENTRY(pattern) p_chlink;
	struct	rule *p_rule;
	struct	tree *p_tree;
	char	p_action[P_MAXSTR];
	char	p_cost[P_MAXSTR];
	char	p_emit[P_MAXSTR];
	short	p_id;
	short	p_ntsidx;
};

struct pattern *pattern(struct tree *);
void pattern_addace(struct pattern *, int, int, int);

#define T_MAXKIDS	2
#define T_MAXSTR	256
#define T_TERM		0
#define T_NTERM		1

struct tree {
	struct	tree *t_kids[T_MAXKIDS];
	struct	pattern *t_pattern;
	char	t_constr[T_MAXSTR];
	short	t_nkids;
	short	t_id;
	short	t_kind;
};

struct tree *tree(const char *);
void tree_addconstr(struct tree *, int, int);
void tree_addchild(struct tree *, struct tree *);

#endif /* TOOLS_CGG_CGG_H */
