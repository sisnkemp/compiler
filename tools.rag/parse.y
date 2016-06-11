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

%{
#include <sys/queue.h>

#include <stdio.h>
#include <stdlib.h>

#include "rag.h"

extern int lineno;

int yylex(void);
void yyerror(const char *);
%}

%token TOK_CLASS TOK_CONFLICTS TOK_IDENT TOK_OF TOK_VOLATILE

%union {
	char	*y_str;
	struct	regq *y_regq;
	struct	regclass *y_rclass;
}

%type <y_str> TOK_IDENT
%type <y_regq> reglist

%%

target_model:	regs classes
	;

regs:	reg
	| regs reg
	;

reg:	TOK_IDENT ';'				{ regdef($1, 0, NULL); }
	| TOK_IDENT TOK_CONFLICTS reglist ';'	{ regdef($1, 0, $3); }
	| TOK_VOLATILE TOK_IDENT ';'		{ regdef($2, 1, NULL); }
	| TOK_VOLATILE TOK_IDENT TOK_CONFLICTS reglist ';'
	    { regdef($2, 1, $4); }
	;

reglist:	TOK_IDENT		{ $$ = regq($1); }
		| reglist ',' TOK_IDENT { $$ = $1; regq_enq($1, $3); }
		;

classes:	class
		| classes class
		;

class:	TOK_CLASS TOK_IDENT '=' '{' reglist '}' ';'	{ regclass($2, $5); }
	;

%%

void
yyerror(const char *msg)
{
	fprintf(stderr, "line %d: %s\n", lineno, msg);
	exit(1);
}

int
yywrap(void)
{
	return 1;
}
