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

#include <ctype.h>
#include <err.h>
#include <limits.h>
#include <math.h>
#include <stdlib.h>
#include <string.h>

#include "comp/comp.h"
#include "comp/ir.h"

#include "lang.c/c.h"

static void nl(void);
static void ppline(void);
static void charcon(void);
static void intcon(int);
static void floatcon(void);
static void hexfloatcon(void);

static struct srcpos nextsp;
%}

BEP	[pP][-+]?[0-9]+
EP	[eE][-+]?[0-9]+
HD	[0-9a-fA-F]
ISUF	([uU]?([lL]|ll|LL))|(([lL]|ll|LL)?[uU])

CC	[^'\\\n]|{ESC}
SC	[^"\\\n]|{ESC}
ESC	\\(['"?\\abefnrtv]|{OESC}|{HESC})
OESC	[0-7]{1,3}
HESC	x{HD}{1,2}

LINE	(line{WS}+)?[0-9]+{WS}*
WS	[\t\v ]
STR	{WS}+\"{SC}*\"

%%

"auto"					{ return TOK_AUTO; }
"break"					{ return TOK_BREAK; }
"case"					{ return TOK_CASE; }
"char"					{ return TOK_CHAR; }
"const"|"__const"			{ return TOK_CONST; }
"continue"				{ return TOK_CONTINUE; }
"default"				{ return TOK_DEFAULT; }
"do"					{ return TOK_DO; }
"double"				{ return TOK_DOUBLE; }
"else"					{ return TOK_ELSE; }
"enum"					{ return TOK_ENUM; }
"extern"				{ return TOK_EXTERN; }
"float"					{ return TOK_FLOAT; }
"for"					{ return TOK_FOR; }
"goto"					{ return TOK_GOTO; }
"if"					{ return TOK_IF; }
"inline"|"__inline"|"__inline__"	{ /*return TOK_INLINE;*/ }
"int"					{ return TOK_INT; }
"long"					{ return TOK_LONG; }
"register"				{ return TOK_REGISTER; }
"restrict"
"return"				{ return TOK_RETURN; }
"short"					{ return TOK_SHORT; }
"signed"|"__signed"			{ return TOK_SIGNED; }
"sizeof"				{ return TOK_SIZEOF; }
"static"				{ return TOK_STATIC; }
"struct"				{ return TOK_STRUCT; }
"switch"				{ return TOK_SWITCH; }
"typedef"				{ return TOK_TYPEDEF; }
"union"					{ return TOK_UNION; }
"unsigned"				{ return TOK_UNSIGNED; }
"void"					{ return TOK_VOID; }
"volatile"|"__volatile"|"__volatile__"	{ return TOK_VOLATILE; }
"while"					{ return TOK_WHILE; }
"_Bool"					{ return TOK_BOOL; }

"__dead"				{ return TOK_DEAD; }
"__packed"				{ return TOK_PACKED; }

"["					{ return '['; }
"]"					{ return ']'; }
"("					{ return '('; }
")"					{ return ')'; }
"{"					{ return '{'; }
"}"					{ return '}'; }
"."					{ return '.'; }
"->"					{ return TOK_ARROW; }
"++"					{ return TOK_INC; }
"--"					{ return TOK_DEC; }
"&"					{ return '&'; }
"*"					{ return '*'; }
"+"					{ return '+'; }
"-"					{ return '-'; }
"~"					{ return '~'; }
"!"					{ return '!'; }
"/"					{ return '/'; }
"%"					{ return '%'; }
"<<"					{ return TOK_LS; }
">>"					{ return TOK_RS; }
"<"					{ return '<'; }
">"					{ return '>'; }
"<="					{ return TOK_LE; }
">="					{ return TOK_GE; }
"=="					{ return TOK_EQ; }
"!="					{ return TOK_NE; }
"^"					{ return '^'; }
"|"					{ return '|'; }
"&&"					{ return TOK_ANDAND; }
"||"					{ return TOK_OROR; }
"?"					{ return '?'; }
":"					{ return ':'; }
";"					{ return ';'; }
"..."					{ return TOK_ELLIPSIS; }
"="					{ return '='; }
"*="					{ return TOK_MULASG; }
"/="					{ return TOK_DIVASG; }
"%="					{ return TOK_MODASG; }
"+="					{ return TOK_ADDASG; }
"-="					{ return TOK_SUBASG; }
"<<="					{ return TOK_LSASG; }
">>="					{ return TOK_RSASG; }
"&="					{ return TOK_ANDASG; }
"^="					{ return TOK_XORASG; }
"|="					{ return TOK_ORASG; }
","					{ return ','; }

\n					{ nl(); }
{WS}

^#{WS}*{LINE}{STR}?.*\n			{ ppline(); }
^#.*\n					{ nl(); }

0[0-7]*{ISUF}?				{ intcon(8); return TOK_ICON; }
[1-9][0-9]*{ISUF}?			{ intcon(10); return TOK_ICON; }
0[xX]{HD}+{ISUF}?			{ intcon(16); return TOK_ICON; }

[0-9]*"."[0-9]+{EP}?[fFlL]?		{ floatcon(); return TOK_FCON; }
[0-9]+(".")?{EP}?[fFlL]?		{ floatcon(); return TOK_FCON; }
0[xX]{HD}*"."{HD}+{BEP}[fFlL]?		{ hexfloatcon(); return TOK_FCON; }
0[xX]{HD}+"."{BEP}[fFlL]?		{ hexfloatcon(); return TOK_FCON; }
0[xX]{HD}+{BEP}[fFlL]?			{ hexfloatcon(); return TOK_FCON; }

L?'{CC}+'				{ charcon(); return TOK_ICON; }
L?\"{SC}*\"				{ 
						token.t_ident = yytext;
						if (*yytext == 'L')
							token.t_strlit++;
						return TOK_STRLIT;
					}

[a-zA-Z_]([a-zA-Z_]|[0-9])*		{
						token.t_ident = ntenter(&names,
						    yytext);
						return TOK_IDENT;
					}

.					{
						fatalsynh(
						    "unexpected character: %s",
						    yytext);
					}

%%

void
lexinit(char *path)
{
	cursp.s_file = nextsp.s_file = path;
	cursp.s_line = nextsp.s_line = 1;
}

static void
nl(void)
{
	cursp.s_line = ++nextsp.s_line;
}

static void
ppline(void)
{
	char *ep, *p = &yytext[1], *str;
	size_t l;

	while (isspace(*p))
		p++;
	if (*p == 'l') {
		p += 4;
		while (isspace(*p))
			p++;
	}

	l = strtoul(p, &ep, 10);
	p = ep;
	if ((p = strchr(p, '\"')) == NULL) {
		cursp.s_line = nextsp.s_line = l;
		return;
	}

	ep = &p[strlen(p)];
	while (ep > p && *ep != '\"')
		ep--;
	if (ep == p || *ep != '\"')
		fatalx("ppline: error parsing %s", yytext);
	p++;
	ep--;
	str = cstring(p, ep, NULL, NULL);
	
	/* XXX: str might be \0 terminated in the middle */
	cursp.s_file = nextsp.s_file = ntenter(&names, str);
	free(str);
	cursp.s_line = nextsp.s_line = l;
}

static void
charcon(void)
{
	char *p = yytext;
	int ch;

	if (*p == 'L')	/* XXX */
		p++;
	p++;	/* skip ' */
	if (*p != '\\') {
		token.t_expr = ast_expr_icon(&cursp, *p, &cir_int);
		return;
	}

	p++;
	ch = escapechar(p, NULL);
	token.t_expr = ast_expr_icon(&cursp, ch, &cir_int);
}

static void
intcon(int base)
{
	char *ep;
	int l, u;
	uint64_t val;

	errno = 0;
	val = strtoull(yytext, &ep, 0);
	if (errno == ERANGE && val == ULLONG_MAX) {
		warnh("integer constant %s too large, using 1 instead",
		    yytext);
		val = 1;
	}

	l = u = 0;
	while (*ep != '\0') {
		switch (*ep) {
		case 'u':
		case 'U':
			u++;
			break;
		case 'l':
		case 'L':
			l++;
			break;
		default:
			fatalx("intcon: bad integer suffix accepted by lexer: "
			    "%s ", yytext);
		}
		ep++;
	}

	if (u > 1 || l > 2)
		fatalx("intcon: bad integer suffix accepted by lexer: %s",
		    yytext);

	/* See 6.4.4 for the rules that determine the type of the constant. */
	if (u == 1 && l == 2) {
		token.t_expr = ast_expr_ucon(&cursp, val, &cir_ullong);
		return;
	}
	if ((u == 0 && l == 2) || (u == 1 && l == 1)) {
		if (val <= C_LLONG_MAX)
			token.t_expr = ast_expr_icon(&cursp, val, &cir_llong);
		else	/* dec gets extended type unsigned long long */
			token.t_expr = ast_expr_ucon(&cursp, val, &cir_ullong);
		return;
	}
	if (u == 1 && l == 0) {
		if (val <= C_UINT_MAX)
			token.t_expr = ast_expr_ucon(&cursp, val, &cir_uint);
		else if (val <= C_ULONG_MAX)
			token.t_expr = ast_expr_ucon(&cursp, val, &cir_ulong);
		else
			token.t_expr = ast_expr_ucon(&cursp, val, &cir_ullong);
		return;
	}
	if (u == 0 && l == 1) {
		if (val <= C_LONG_MAX)
			token.t_expr = ast_expr_icon(&cursp, val, &cir_long);
		else if (base != 10 && val <= C_ULONG_MAX)
			token.t_expr = ast_expr_ucon(&cursp, val, &cir_ulong);
		else if (val <= C_LLONG_MAX)
			token.t_expr = ast_expr_icon(&cursp, val, &cir_llong);
		else	/* dec gets extended type unsigned long long */
			token.t_expr = ast_expr_ucon(&cursp, val, &cir_ullong);
		return;
	}

	/* u == 0 && l == 0 */
	if (val <= C_INT_MAX)
		token.t_expr = ast_expr_icon(&cursp, val, &cir_int);
	else if (base != 10 && val <= C_UINT_MAX)
		token.t_expr = ast_expr_ucon(&cursp, val, &cir_uint);
	else if (val <= C_LONG_MAX)
		token.t_expr = ast_expr_icon(&cursp, val, &cir_long);
	else if (base != 10 && val <= C_ULONG_MAX)
		token.t_expr = ast_expr_ucon(&cursp, val, &cir_ulong);
	else if (val <= C_LLONG_MAX)
		token.t_expr = ast_expr_icon(&cursp, val, &cir_llong);
	else	/* dec gets extended type unsigned long long */
		token.t_expr = ast_expr_ucon(&cursp, val, &cir_ullong);
}

static void
floatcon(void)
{
	char *ep, *f, *l;
	double dval;

	if ((f = strrchr(yytext, 'f')) == NULL)
		f = strrchr(yytext, 'F');
	if ((l = strchr(yytext, 'l')) == NULL)
		l = strchr(yytext, 'L');

	errno = 0;
	dval = strtod(yytext, &ep);
	if (errno == ERANGE && dval == HUGE_VAL) {
		warnh("floating-point constant too large, using 1.0");
		dval = 1.0;
	}

	if (f != NULL)
		token.t_expr = ast_expr_fcon(&cursp, dval, &cir_double);
	else
		token.t_expr = ast_expr_fcon(&cursp, dval, &cir_float);
}

static void
hexfloatcon(void)
{
	fatalx("hexadecimal floating-point constants are not supported yet");
}

int
peek(void)
{
	int ch;

	for (;;) {
		if ((ch = input()) == '\n')
			nl();
		else if (!isspace(ch))
			break;
	}
	unput(ch);
	return ch;
}

int
yywrap(void)
{
	return 1;
}
