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
#include <err.h>
#include <string.h>

#include "y.tab.h"

#define YY_NO_INPUT
#define YY_NO_UNPUT

int lineno = 1;

static char *xstrdup(const char *);
%}

%s COMMENT

%%

<INITIAL>"conflicts"		{ return TOK_CONFLICTS; }
<INITIAL>"volatile"		{ return TOK_VOLATILE; }
<INITIAL>"class"		{ return TOK_CLASS; }
<INITIAL>"of"			{ return TOK_OF; }

<INITIAL>[_a-zA-Z][_a-zA-Z0-9]*	{ yylval.y_str = xstrdup(yytext);
				    return TOK_IDENT; }
<INITIAL>"{"			{ return '{'; }
<INITIAL>"}"			{ return '}'; }
<INITIAL>","			{ return ','; }
<INITIAL>";"			{ return ';'; }
<INITIAL>"="			{ return '='; }
<INITIAL>\n			{ lineno++; }
<INITIAL>[ \v\t]		{ }
<INITIAL>.			{ errx(1, "unexpected character: %s",
				    yytext); }

<INITIAL>"/*"			{ BEGIN(COMMENT); }
<COMMENT>"*/"			{ BEGIN(INITIAL); }
<COMMENT>.			{ }
<COMMENT>\n			{ }

%%

static char *
xstrdup(const char *str)
{
	char *p;

	if ((p = strdup(str)) == NULL)
		err(1, "xstrdup");
	return p;
}
