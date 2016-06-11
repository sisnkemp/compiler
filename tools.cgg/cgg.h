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

#define TOK_ID		256
#define TOK_COST	257
#define TOK_EMIT	258
#define TOK_ACTION	259
#define TOK_LVERB	260
#define TOK_RVERB	261
#define TOK_PERC	262
#define TOK_NTERM	263

extern char *tokstr;

void verbatim(void);
void noverbatim(void);
void parse(void);

#define TP_MAXKIDS	2
#define TP_MAXSTR	256
#define TP_TERM		0
#define TP_NTERM	1

struct treepat {
	struct	treepat *t_kids[TP_MAXKIDS];
	char	t_constr[TP_MAXSTR];
	char	*t_termid;
	struct	nonterm *t_nt;
	int	t_op;
	int	t_nkids;
	int	t_ntot;
};

struct treepat *tp_nterm(struct nonterm *);
struct treepat *tp_term(char *);
void tp_add_kid(struct treepat *, struct treepat *);
void tp_add_constrch(struct treepat *, int, size_t);
int tp_has_constr(struct treepat *);

#define R_MAXSTR	256
#define R_MAXEMIT	2048

SIMPLEQ_HEAD(ruleq, rule);

struct rule {
	SIMPLEQ_ENTRY(rule) r_link;
	SIMPLEQ_ENTRY(rule) r_glolink;
	SIMPLEQ_ENTRY(rule) r_chlink;
	struct	treepat *r_tp;
	struct	nonterm *r_lhs;
	char	r_cost[R_MAXSTR];
	char	r_action[R_MAXSTR];
	char	r_emit[R_MAXEMIT];
	int	r_id;
	int	r_nts;
};

#define NTMAX	2048

struct rule *rule(void);
void rule_add_costch(struct rule *, int, size_t);
void rule_add_actionch(struct rule *, int, size_t);
void rule_add_emitch(struct rule *, int, size_t);
void rule_add_tp(struct rule *, struct treepat *);
int rule_has_cost(struct rule *);
int rule_has_emit(struct rule *);
int rule_has_action(struct rule *);

struct nonterm {
	char	*n_name;
	int	n_id;
	struct	ruleq n_rules;

	/* Queue of chain rules where this nonterm appears on the rhs. */
	struct	ruleq n_chainrq;
};

struct nonterm *nonterm(char *);
struct nonterm *nonterm_find(char *);
void nonterm_addrule(struct nonterm *, struct rule *);
void nonterm_add_chain(struct nonterm *, struct rule *);
void nonterm_set_start(struct nonterm *);

#endif /* TOOLS_CGG_CGG_H */
