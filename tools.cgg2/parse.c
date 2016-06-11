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

#include <ctype.h>
#include <err.h>
#include <stdio.h>

#include "cgg.h"

static int tok;

static char *toknames[] = {
	"identifier", "cost", "emit", "action", "%{", "%}", "%%"
};

static int gettok(void);
static void expect(int);
static void synexpect(const char *);

static void parserules(void);
static void parsepatterns(struct rule *);
static struct tree *parsetree(void);
static void parseconstraint(struct tree *);
static void parseace(struct pattern *);

static int
gettok(void)
{
	tok = yylex();
	return tok;
}

static void
expect(int token)
{
	if (tok == token) {
		gettok();
		return;
	}
	if (tok > TOK_END)
		errx(1, "tok(%d) > TOK_END", tok);
	if (token > TOK_END)
		errx(1, "token(%d) > TOK_END", token);
	if (tok >= TOK_START && token < TOK_START)
		synh("`%c' expected, got %s",
		    token, toknames[tok - TOK_START]);
	else if (tok >= TOK_START && token >= TOK_START)
		synh("%s expected, got %s",
		    toknames[token - TOK_START], toknames[tok - TOK_START]);
	else if (tok < TOK_START && token < TOK_START)
		synh("`%c' expected, got `%c'", token, tok);
	else
		synh("%s expected, got `%c'",
		    toknames[token - TOK_START], tok);
}

static void
synexpect(const char *msg)
{
	if (tok > TOK_END)
		errx(1, "tok(%d) > TOK_END", tok);
	if (tok >= TOK_START)
		synh("%s expected, got %s", msg, toknames[tok - TOK_START]);
	else
		synh("%s expected, got `%c'", msg, tok);
}

void
parse(void)
{
	if (gettok() == TOK_LVERB) {
		verbatim();
		for (gettok(); tok != TOK_RVERB; gettok()) {
			if (tok == 0)
				fatalsynh("premature end of file");
			putchar(tok);
		}
		noverbatim();
		gettok();
	}

	expect(TOK_PERC);
	parserules();
	verbatim();
	if (tok != TOK_PERC)
		synexpect("%%");
	gettok();
	printf("\n");
	verbatim();
	for (gettok(); tok != 0; gettok())
		putchar(tok);
	noverbatim();
}

static void
parserules(void)
{
	struct rule *r;

	for (;;) {
		if (tok != TOK_ID)
			return;

		r = rule(tokstr);
		gettok();
		expect(':');
		parsepatterns(r);
		expect(';');
	}
}

static void
parsepatterns(struct rule *r)
{
	struct pattern *p;
	struct tree *t;

	for (;;) {
		t = parsetree();
		p = pattern(t);
		parseace(p);
		rule_addpattern(r, p);
		if (tok != '|')
			return;
		gettok();
	}
}

static struct tree *
parsetree(void)
{
	int kids;
	struct tree *t;

	if (tok != TOK_ID) {
		synexpect("tree");
		return NULL;
	}
	t = tree(tokstr);
	gettok();
	if (tok == '[')
		parseconstraint(t);
	if (tok != '(')
		return t;
	if (t->t_kind == T_NTERM)
		errh("nonterminal trees can't have subtrees");
	gettok();
	for (kids = 0;; kids++) {
		if (kids >= T_MAXKIDS) {
			errh("too many children in tree");
			return t;
		}
		tree_addchild(t, parsetree());
		if (tok != ',')
			break;
		gettok();
	}
	expect(')');
	return t;
}

static void
parseconstraint(struct tree *t)
{
	int braces = 1, i;

	verbatim();
	gettok();
	for (braces = 1, i = 0; braces != 0; gettok(), i++) {
		if (tok == 0)
			fatalsynh("premature end of file");
		else if (tok == '[')
			braces++;
		else if (tok == ']') {
			if (--braces == 0) {
				noverbatim();
				gettok();
				break;
			}
		}
		tree_addconstr(t, i, tok);
	}
}

static void
parseace(struct pattern *p)
{
	int braces, i, op;

	for (;;) {
		if (tok != TOK_ACTION && tok != TOK_COST && tok != TOK_EMIT)
			return;
		op = tok;
		gettok();
		verbatim();
		if (tok != '{')
			synexpect("{");
		else
			gettok();
		for (braces = 1, i = 0; braces != 0; gettok(), i++) {
			if (tok == 0)
				fatalsynh("premature end of file");
			else if (tok == '{')
				braces++;
			else if (tok == '}') {
				if (--braces == 0) {
					noverbatim();
					gettok();
					break;
				}
			}
			pattern_addace(p, op, i, tok);
		}
	}
}
