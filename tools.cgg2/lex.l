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
#include "cgg.h"

#define YY_NO_INPUT
#define YY_NO_UNPUT

char *tokstr;
size_t lineno = 1;
static int mode;
%}

%s COMMENT VERB

%%

	if (mode == 1) {
		mode = 0;
		BEGIN(VERB);
	} else if (mode == 2) {
		mode = 0;
		BEGIN(INITIAL);
	}

<INITIAL,VERB>^"%{"		{ return TOK_LVERB; }
<INITIAL,VERB>^"%}"		{ return TOK_RVERB; }
<INITIAL>"cost"			{ return TOK_COST; }
<INITIAL>"emit"			{ return TOK_EMIT; }
<INITIAL>"action"		{ return TOK_ACTION; }
<INITIAL>^"%%"			{ return TOK_PERC; }
<INITIAL>"%nonterm".*\n		{ lineno++; }

<INITIAL>"/*"			{ BEGIN(COMMENT); }
<COMMENT>"*/"			{ BEGIN(INITIAL); }
<COMMENT>.			{ }
<COMMENT>\n			{ }

<INITIAL>[a-zA-Z][_0-9a-zA-Z]*	{ tokstr = yytext; return TOK_ID; }

<INITIAL>\n			{ lineno++; }
<INITIAL>[\t\v ]		{ }

<INITIAL>"("			{ return '('; }
<INITIAL>")"			{ return ')'; }
<INITIAL>"["			{ return '['; }
<INITIAL>"]"			{ return ']'; }
<INITIAL>"{"			{ return '{'; }
<INITIAL>"}"			{ return '}'; }
<INITIAL>":"			{ return ':'; }
<INITIAL>","			{ return ','; }
<INITIAL>";"			{ return ';'; }
<INITIAL>"|"			{ return '|'; }
<INITIAL>.			{ fatalsynh("unexpected character: %s",
				    yytext); }

<VERB>\n			{ lineno++; return '\n'; }
<VERB>.				{ return *yytext; }

%%

void
verbatim(void)
{
	mode = 1;
}

void
noverbatim(void)
{
	mode = 2;
}

int
yywrap(void)
{
	return 1;
}
