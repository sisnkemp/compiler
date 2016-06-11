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

#include "cgg.h"

int yylex(void);

char *tokstr;
static int tok;

static void nonterms(void);
static void rules(void);
static void ruleuctions(struct nonterm *);
static struct treepat *pattern(void);

static int
gettok(void)
{
	tok = yylex();
	return tok;
}

void
parse(void)
{
	if (gettok() == TOK_LVERB) {
		verbatim();
		for (gettok(); tok != TOK_RVERB; gettok())
			putchar(tok);
		noverbatim();
		gettok();
	}

	nonterms();

	if (tok != TOK_PERC)
		errx(1, "%%%% expected, got %d, %c", tok, tok);

	rules();

	if (tok != TOK_PERC)
		return;

	verbatim();
	for (gettok(); tok != 0; gettok())
		putchar(tok);
}

static void
nonterms(void)
{
	for (;;) {
		if (tok != TOK_NTERM)
			return;
		for (gettok();; gettok()) {
			if (tok != TOK_ID)
				errx(1, "identifier expected");
			nonterm(tokstr);
			if (gettok() != ',')
				break;
		}
	}
}

static void
rules(void)
{
	int nonstart = 0;
	char *id;
	struct nonterm *nt;

	for (;;) {
		for (gettok();;) {
			if (tok != TOK_ID)
				return;
			id = tokstr;
			if ((nt = nonterm_find(id)) == NULL)
				errx(1, "undeclared non-terminal %s", id);
			if (gettok() != ':')
				errx(1, ": expected");
			ruleuctions(nt);
			if (tok != ';')
				errx(1, "; expected");
			gettok();

			if (!nonstart) {
				nonterm_set_start(nt);
				nonstart = 1;
			}
		}
	}
}

static void
ruleuctions(struct nonterm *nt)
{
	int otok, par;
	size_t pos;
	struct rule *p;
	struct treepat *tp;

	for (;;) {
		p = rule();
		tp = pattern();

		for (;;) {
			switch (tok) {
			case TOK_EMIT:
			case TOK_ACTION:
			case TOK_COST:
				otok = tok;
				if (gettok() != '{')
					errx(1, "{ expected");
				par = 1;
				pos = 0;
				verbatim();
				for (gettok(); par != 0; gettok()) {
					if (tok == '}') {
						if (--par == 0)
							break;
					}
					if (tok == '{')
						par++;

					switch (otok) {
					case TOK_EMIT:
						rule_add_emitch(p, tok, pos++);
						break;
					case TOK_ACTION:
						rule_add_actionch(p, tok, pos++);
						break;
					case TOK_COST:
						rule_add_costch(p, tok, pos++);
						break;
					}
				}
				noverbatim();
				gettok();
				continue;
			default:
				break;
			}
			break;
		}

		rule_add_tp(p, tp);
		nonterm_addrule(nt, p);
		if (tok != '|')
			return;
	}
}

static struct treepat *
pattern(void)
{
	int i, par;
	size_t pos;
	struct nonterm *nt;
	struct treepat *tp;

	if (gettok() != TOK_ID)
		errx(1, "tree pattern expected");

	if (islower(*tokstr)) {
		if ((nt = nonterm_find(tokstr)) == NULL)
			errx(1, "undeclared non-terminal %s", tokstr);
		tp = tp_nterm(nt);
	} else
		tp = tp_term(tokstr);

	if (gettok() == '[') {
		par = 1;
		pos = 0 ;
		verbatim();
		for (gettok(); ; gettok()) {
			if (tok == ']') {
				if (--par == 0)
					break;
			}
			if (tok == '[')
				par++;
			tp_add_constrch(tp, tok, pos++);
		}
		noverbatim();
		gettok();
	}

	if (tok != '(')
		return tp;

	while (tok != ')') {
		tp_add_kid(tp, pattern());
		if (tok != ',' && tok != ')')
			errx(1, "tree pattern expected");
	}

	gettok();
	return tp;
}
