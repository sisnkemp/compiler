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
#include <sys/types.h>

#include <stdio.h>
#include <stdlib.h>

#include "comp/comp.h"
#include "comp/ir.h"

#include "lang.c/c.h"

union token token;

char *toknames[] = {
	"+=", "&&", "&=", "->",
	"auto", "_Bool", "break", "case",
	"char", "_Complex", "const", "continue",
	"__dead", "--", "default", "/=", "do",
	"double", "...", "else", "enum",
	"extern", "==", "floating-point constant", "float",
	"for", ">=", "goto", "integer constant",
	"identifier", "if", "_Imaginary", "++",
	"inline", "int", "<=", "long",
	"<<", "<<=", "%=", "*=",
	"!=", "|=", "||", "__packed", "register",
	"restrict", "return", ">>", ">>=",
	"short", "signed", "sizeof", "static",
	"struct", "character string", "-=", "switch",
	"typedef", "union", "unsigned", "void",
	"volatile", "while", "^=",
};

static int gettok(void);
static void expect(int);
static void skipto(int *);
static void synexpect(const char *);

static int istypename(char *);
static int isinfirst(int *, int);
static int isinfirst_declspecs(int);

static int storageclass(struct ast_declspecs *);
static struct ast_expr *bitfield_expr(void);
static struct ast_souspec *typespec_sou(void);
static struct ast_enumspec *typespec_enum(void);
static struct ast_tyspec *typespec(int *);
static int typequal(void);
static int funspec(void);
static struct ast_declspecs *declspecs(int, int);
static int pointer(void);
static struct ast_decla *param_declarator(void);
static struct ast_list *paramtype_list(void);
static struct ast_list *ident_list(void);
static struct ast_decla *declarator_suffix(struct ast_decla *);
static struct ast_decla *abstract_declarator(void);
static struct ast_decla *declarator(char **);
static struct ast_designation *designation(void);
static struct ast_init *initializer_list(void);
static struct ast_init *initializer(void);
static void register_typedef(struct ast_declspecs *, char *);
static struct ast_decl *decl(int);

static struct ast_decl *typename(void);

static struct ast_list *argexprlist(void);
static struct ast_expr *postfixexpr(void);
static struct ast_expr *postfixexpr_suffix(struct ast_expr *);
static struct ast_expr *unexpr(void);
static struct ast_expr *castexpr(void);
static struct ast_expr *binexpr(int);
static struct ast_expr *condexpr(void);
static struct ast_expr *asgexpr(void);
static struct ast_expr *expr(void);

static struct ast_stmt *compound_stmt(struct srcpos *);
static struct ast_stmt *stmt(void);

static int tok;

static struct ir_type dummytype;

#define MAXBINPREC	9

static int binopprecs[MAXBINPREC + 1][5] = {
	{ '*', '/', '%', -1 },
	{ '+', '-', -1 },
	{ TOK_LS, TOK_RS, -1 },
	{ '<', TOK_LE, '>', TOK_GE, -1 },
	{ TOK_EQ, TOK_NE, -1 },
	{ '&', -1 },
	{ '^', -1 },
	{ '|', -1 },
	{ TOK_ANDAND, -1 },
	{ TOK_OROR, - 1}
};

static int FIRST_declspecs[] = {
	TOK_TYPEDEF, TOK_EXTERN, TOK_STATIC, TOK_AUTO, TOK_REGISTER, TOK_VOID,
	TOK_CHAR, TOK_SHORT, TOK_INT, TOK_LONG, TOK_FLOAT, TOK_DOUBLE,
	TOK_SIGNED, TOK_UNSIGNED, TOK_BOOL, TOK_STRUCT, TOK_UNION, TOK_ENUM,
	TOK_CONST, TOK_VOLATILE, TOK_INLINE, TOK_DEAD, -1
};

static int FIRST_expr[] = {
	TOK_IDENT, TOK_ICON, TOK_FCON, TOK_STRLIT, '(', TOK_INC, TOK_DEC, '&',
	'*', '+', '-', '~', '!', TOK_SIZEOF, -1
};

static int
gettok(void)
{
	tok = yylex();
	return tok;
}

static void
expect(int ch)
{
	if (tok == ch) {
		gettok();
		return;
	}

	if (ch > TOK_END)
		fatalx("expect: ch(%d) > TOK_END", ch);
	if (tok > TOK_END)
		fatalx("expect: tok(%d) > TOK_END", tok);

	if (ch < TOK_START && tok < TOK_START)
		synh("`%c' expected, got `%c'", ch, tok);
	else if (ch < TOK_START && tok <= TOK_END)
		synh("`%c' expected, got %s", ch, toknames[tok - TOK_START]);
	else if (ch <= TOK_END && tok < TOK_START)
		synh("%s expected, got `%c'", toknames[ch - TOK_START], tok);
	else
		synh("%s expected, got %s",
		    toknames[ch - TOK_START], toknames[tok - TOK_START]);
}

static void
skipto(int *tokens)
{
	int i;

	for (;;) {
		for (i = 0; tokens[i] != -1; i++) {
			if (tok == tokens[i])
				return;
		}
		if (gettok() == 0) {
			synh("unexpected end of file");
			exit(1);
		}
	}
}

static void
synexpect(const char *msg)
{
	if (tok < TOK_START)
		synh("%s expected, got `%c'", msg, tok);
	else if (tok <= TOK_END)
		synh("%s expected, got %s", msg, toknames[tok - TOK_START]);
	else
		fatalx("synexpect: unknown token: %d", tok);
}

void
parse(void)
{
	struct ast_decl *dcl;

	gettok();
	while (tok != 0) {
		if ((dcl = decl(1)) != NULL)
			ast_declhead_newdecl(&ast_program, dcl);
	}
}

static int
isinfirst(int *first, int tok)
{
	while (*first != -1) {
		if (*first++ == tok)
			return 1;
	}
	return 0;
}

static int
isinfirst_declspecs(int tok)
{
	return (tok == TOK_IDENT && istypename(token.t_ident)) ||
	    isinfirst(FIRST_declspecs, tok);
}

/*
 * Determine is ident is a typedef name by querying the symbol table.
 */
static int
istypename(char *ident)
{
	struct symbol *sym;

	if ((sym = symlookup(ident, NSORD)) == NULL)
		return 0;
	return sym->s_flags & SYM_TYPEDEF;
}

static int
storageclass(struct ast_declspecs *ad)
{
	int rv;

	switch (tok) {
	case TOK_TYPEDEF:
		rv = AST_SC_TYPEDEF;
		break;
	case TOK_EXTERN:
		rv = AST_SC_EXTERN;
		break;
	case TOK_STATIC:
		rv = AST_SC_STATIC;
		break;
	case TOK_AUTO:
		rv = AST_SC_AUTO;
		break;
	case TOK_REGISTER:
		rv = AST_SC_REGISTER;
		break;
	default:
		return 0;
	}

	gettok();
	return rv;
}

static struct ast_expr *
bitfield_expr(void)
{
	gettok();	/* consume ':' */
	return asgexpr();
}

static struct ast_souspec *
typespec_sou(void)
{
	char *ident = NULL;
	struct ast_decla *decla;
	struct ast_declspecs *ds;
	struct ast_expr *x;
	struct ast_souent *ent;
	struct ast_souspec *sou;

	if (tok == TOK_IDENT) {
		ident = token.t_ident;
		if (gettok() != '{')
			return ast_souspec(ident, 0);
	}

	sou = ast_souspec(ident, 1);
	expect('{');

	for (;;) {	/* struct_declaration_list */
		if (!isinfirst_declspecs(tok))
			break;

		ds = declspecs(0, 1);
		for (;;) {	/* struct_declarator_list */
			decla = NULL;
			x = NULL;
			if (tok == ':')
				x = bitfield_expr();
			else {
				decla = declarator(NULL);
				if (tok == ':')
					x = bitfield_expr();
			}

			ent = ast_souent(ds, decla, x);
			ast_souspec_newent(sou, ent);
			if (tok != ',')
				break;
			gettok();
		}

		expect(';');
	}

	expect('}');
	if (tok == TOK_PACKED) {
		gettok();
		sou->as_flags |= AST_SOU_PACKED;
	}
	return sou;
}

static struct ast_enumspec *
typespec_enum(void)
{
	char *ident = NULL;
	struct ast_enument *ent;
	struct ast_enumspec *enu;
	struct ast_expr *x;
	struct srcpos sp;

	if (tok == TOK_IDENT) {
		ident = token.t_ident;
		if (gettok() != '{')
			return ast_enumspec(ident, 0);
	}

	enu = ast_enumspec(ident, 1);
	expect('{');

	for (;;) {
		if (tok != TOK_IDENT)
			break;
		ident = token.t_ident;
		SPCPY(&sp, &cursp);
		x = NULL;
		if (gettok() == '=') {
			gettok();
			x = asgexpr();
		}

		ent = ast_enument(&sp, ident, x);
		ast_enumspec_newent(enu, ent);
		if (tok != ',')
			break;
		gettok();
	}

	expect('}');
	return enu;
}

static struct ast_tyspec *
typespec(int *notypedef)
{
	int op;
	struct ast_enumspec *enu;
	struct ast_souspec *sou;
	struct ast_tyspec *ts = NULL;

	switch (tok) {
	case TOK_VOID:
	case TOK_CHAR:
	case TOK_SHORT:
	case TOK_INT:
	case TOK_LONG:
	case TOK_FLOAT:
	case TOK_DOUBLE:
	case TOK_SIGNED:
	case TOK_UNSIGNED:
	case TOK_BOOL:
		ts = ast_tyspec_simple(tok);
		gettok();
		*notypedef = 1;
		return ts;
	case TOK_STRUCT:
	case TOK_UNION:
		op = tok == TOK_STRUCT ? AST_TYSPEC_STRUCT : AST_TYSPEC_UNION;
		gettok();
		*notypedef = 1;
		sou = typespec_sou();
		return ast_tyspec_sou(op, sou);
	case TOK_ENUM:
		gettok();
		*notypedef = 1;
		enu = typespec_enum();
		return ast_tyspec_enum(enu);
	case TOK_IDENT:
		if (*notypedef)
			return NULL;
		if ((*notypedef = istypename(token.t_ident))) {
			ts = ast_tyspec_tdname(token.t_ident);
			gettok();
		}
		return ts;
	default:
		break;
	}
	return NULL;
}

static int
typequal(void)
{
	int op;

	switch (tok) {
	case TOK_CONST:
	case TOK_VOLATILE:
		op = tok == TOK_CONST ? AST_TQ_CONST : AST_TQ_VOLATILE;
		gettok();
		return op;
	default:
		break;
	}
	return 0;
}

static int
funspec(void)
{
	switch (tok) {
	case TOK_INLINE:
		gettok();
		return AST_FS_INLINE;
	case TOK_DEAD:
		gettok();
		return AST_FS_DEAD;
	default:
		break;
	}
	return 0;
}

static struct ast_declspecs *
declspecs(int sclass, int mandatory)
{
	int notd = 0, rv;
	struct ast_declspecs *ds;
	struct ast_tyspec *ts;

	ds = ast_declspecs();
	if (!isinfirst_declspecs(tok)) {
		if (mandatory) {
			synexpect("declaration specifiers");
			skipto(FIRST_declspecs);
		}
		return ds;
	}

	for (;;) {
		if ((rv = storageclass(ds))) {
			if (!sclass)
				synh("unexpected storage class specifier");
			ast_declspecs_newsclass(ds, rv);
		} else if ((ts = typespec(&notd)) != NULL)
			ast_declspecs_newtyspec(ds, ts);
		else if ((rv = typequal())) {
			ast_declspecs_newtyqual(ds, rv);
		} else if ((rv = funspec()))
			ast_declspecs_newfnspec(ds, rv);
		else
			break;
	}

	return ds;
}

static int
pointer(void)
{
	int rv = 0, tq;

	gettok();
	do {
		tq = typequal();
		rv |= tq;
	} while (tq != 0);

	return rv;
}

static struct ast_decla *
param_declarator(void)
{
	int tq;
	struct ast_decla *decla;
	struct ast_expr *x;
	struct srcpos sp;

	if (tok == '*') {
		SPCPY(&sp, &cursp);
		tq = pointer();
		if (tok == '*' || tok == '[' || tok == '(' || tok == TOK_IDENT)
			return ast_decla_ptr(&sp, param_declarator(), tq);
		return ast_decla_ptr(&sp, NULL, tq);
	}

	if (tok == TOK_IDENT) {
		decla = ast_decla_ident(token.t_ident);
		gettok();
	} else if (tok == '[') {
		SPCPY(&sp, &cursp);
		x = NULL;
		if (gettok() != ']')
			x = expr();
		expect(']');
		decla = ast_decla_array(&sp, NULL, x);
	} else if (tok == '(') {
		SPCPY(&sp, &cursp);
		if (gettok() != ')') {
			if (isinfirst_declspecs(tok))
				decla = ast_decla_func(&sp, NULL,
				    paramtype_list());
			else
				decla = param_declarator();
		} else
			decla = ast_decla_func(&sp, NULL,
			    ast_list(AST_IDLIST));
		expect(')');
	} else {
		synexpect("parameter declarator");
		return NULL;
	}
	return declarator_suffix(decla);
}

static struct ast_list *
paramtype_list(void)
{
	struct ast_declspecs *ds;
	struct ast_decl *decl;
	struct ast_decla *decla;
	struct ast_list *list;

	list = ast_list(AST_PARLIST);
	for (;;) {
		ds = declspecs(1, 1);
		decl = ast_decl(ds);
		ast_list_newdecl(list, decl);
		if (tok == ')')
			break;
		if (tok != ',') {
			if ((decla = param_declarator()) != NULL)
				ast_decl_newdecla(decl, decla);
		}
		if (tok != ',')
			break;
		if (gettok() == TOK_ELLIPSIS) {
			ast_list_setellipsis(list);
			gettok();
			break;
		}
	}
	return list;
}

static struct ast_list *
ident_list(void)
{
	struct ast_decl *decl;
	struct ast_decla *decla;
	struct ast_list *list;

	list = ast_list(AST_IDLIST);
	while (tok == TOK_IDENT) {
		decl = ast_decl(NULL);
		decla = ast_decla_ident(token.t_ident);
		ast_decl_newdecla(decl, decla);
		ast_list_newdecl(list, decl);
		if (gettok() != ',')
			break;
		gettok();
	}
	return list;
}

static struct ast_decla *
declarator_suffix(struct ast_decla *prefix)
{
	struct ast_expr *x;
	struct srcpos sp;

	for (;;) {
		SPCPY(&sp, &cursp);
		if (tok == '[') {
			x = NULL;
			if (gettok() != ']')
				x = expr();
			expect(']');
			prefix = ast_decla_array(&sp, prefix, x);
		} else if (tok == '(') {
			if (gettok() == TOK_IDENT &&
			    !istypename(token.t_ident))
				prefix = ast_decla_func(&sp, prefix,
				    ident_list());
			else if (tok != ')')
				prefix = ast_decla_func(&sp, prefix,
				    paramtype_list());
			else
				prefix = ast_decla_func(&sp, prefix, NULL);
			expect(')');
		} else
			break;
	}
	return prefix;
}

static struct ast_decla *
abstract_declarator(void)
{
	int tq;
	struct ast_decla *decla;
	struct srcpos sp;

	SPCPY(&sp, &cursp);
	if (tok == '*') {
		tq = pointer();
		if (tok == '[' || tok == '(' || tok == '*')
			return ast_decla_ptr(&sp, abstract_declarator(), tq);
		return ast_decla_ptr(&sp, NULL, tq);
	}

	if (tok == '[')
		return declarator_suffix(NULL);
	if (tok != '(')
		return NULL;

	gettok();
	if (tok == '*' || tok == '[' || tok == '(')
		decla = abstract_declarator();
	else if (tok != ')')
		decla = ast_decla_func(&sp, NULL, paramtype_list());
	else
		decla = ast_decla_func(&sp, NULL, NULL);
	expect(')');
	return declarator_suffix(decla);
}

static struct ast_decla *
declarator(char **idp)
{
	int tq;
	struct ast_decla *decla;
	struct srcpos sp;

	if (tok == '*') {
		SPCPY(&sp, &cursp);
		tq = pointer();
		return ast_decla_ptr(&sp, declarator(idp), tq);
	}

	switch (tok) {
	case TOK_IDENT:
		if (idp != NULL)
			*idp = token.t_ident;
		decla = ast_decla_ident(token.t_ident);
		gettok();
		break;
	case '(':
		gettok();
		decla = declarator(idp);
		expect(')');
		break;
	default:
		synexpect("declarator");
		return NULL;
	}

	return declarator_suffix(decla);
}

static struct ast_designation *
designation(void)
{
	return NULL;
#if 0
	struct ast_designation *designation;
	struct ast_designator *designator;
	struct srcpos sp;

	static int skip[] = { '=', -1 };

	if (tok != '.' && tok != '[')
		return NULL;

	designation = ast_designation();
	for (;;) {
		SPCPY(&sp, &cursp);
		if (tok == '.') {
			if (gettok() != TOK_IDENT) {
				synexpect("identifier");
				skipto(skip);
				break;
			}
			designator = ast_designator_ident(&sp, token.t_ident);
			ast_designation_newdesig(designation, designator);
			gettok();
		} else if (tok == '[') {
			gettok();
			designator = ast_designator_array(&sp, expr());
			expect(']');
			ast_designation_newdesig(designation, designator);
		} else
			break;
	}
	
	expect('=');
	return designation;
#endif
}

static struct ast_init *
initializer_list(void)
{
	struct ast_designation *desig;
	struct ast_init *init, *i;

	init = ast_init_list();
	for (;;) {
		desig = designation();
		i = initializer();
		ast_init_setdesig(i, desig);
		ast_init_newinit(init, i);
		if (tok != ',')
			break;
		if (gettok() == '}')
			break;
	}
	expect('}');
	return init;
}

static struct ast_init *
initializer(void)
{
	struct ast_init *init;
	struct srcpos sp;

	SPCPY(&sp, &cursp);
	if (tok == '{') {
		if (gettok() == '}') {
			gettok();
			return ast_init_list();
		}
		init = initializer_list();
	} else
		init = ast_init_simple(&sp, asgexpr());
	return init;
}

/*
 * Lookup the identifier in the symbol table.
 * If the identifier is found and it is a typedef name,
 * add ident into the symtab.
 *
 * If ident is a typedef name but ident is not yet in the
 * symtab, add it.
 *
 * If ident is not a typedef name and it is not in the
 * symtab, don't add it.
 */
static void
register_typedef(struct ast_declspecs *ad, char *ident)
{
	struct symbol *sym;

	sym = symlookup(ident, NSORD);
	if (sym == NULL) {
		if (ad->ad_sclass != AST_SC_TYPEDEF)
			return;
		sym = symenter(ident, &dummytype, NSORD);
		sym->s_flags |= SYM_TYPEDEF;
		return;
	}

	if (sym->s_scope == curscope)
		return;

	if ((sym->s_flags & SYM_TYPEDEF) || ad->ad_sclass == AST_SC_TYPEDEF) {
		if (ad->ad_sclass == AST_SC_TYPEDEF) {
			symenter(ident, &dummytype, NSORD);
			sym->s_flags |= SYM_TYPEDEF;
		} else
			symenter(ident, &dummytype, NSORD);
	}
}

static struct ast_decl *
decl(int global)
{
	char *ident = NULL;
	struct ast_decl *dcl;
	struct ast_decla *decla;
	struct ast_declspecs *ds;
	struct srcpos sp;
	static int skip[] = { '}', -1 };

	if (tok == ';') {
		warnh("empty declaration");
		gettok();
		return NULL;
	}

	ds = declspecs(1, 0);
	dcl = ast_decl(ds);
	for (;;) {
		if (tok == ';')
			break;

		decla = declarator(&ident);
		ast_decl_newdecla(dcl, decla);
		if (ident != NULL)
			register_typedef(ds, ident);
		if (tok == ',') {
			gettok();
			continue;
		}
		if (tok == '=') {
			gettok();
			ast_decla_setinit(decla, initializer());
			if (tok == ';')
				break;
			if (tok == ',')
				gettok();
			continue;
		}
		break;
	}

	if (tok == ';') {
		gettok();
		return dcl;
	}

	if (!global) {
		synh("malformed non-global declaration");
		skipto(skip);
		gettok();
		return dcl;
	}

	if (tok != '{') {
		while (isinfirst_declspecs(tok))
			ast_decl_newkrdecl(dcl, decl(0));
	}

	SPCPY(&sp, &cursp);
	expect('{');
	ast_decl_setstmt(dcl, compound_stmt(&sp));
	expect('}');
	return dcl;
}

static struct ast_decl *
typename(void)
{
	struct ast_decl *decl;

	decl = ast_decl(declspecs(0, 1));
	if (tok == '[' || tok == '(' || tok == '*')
		ast_decl_newdecla(decl, abstract_declarator());
	return decl;
}

static struct ast_list *
argexprlist(void)
{
	struct ast_expr *x;
	struct ast_list *list;

	list = ast_list(AST_EXPRLIST);
	if ((x = asgexpr()) != NULL)
		ast_list_newexpr(list, x);
	for (;;) {
		if (tok != ',')
			break;
		gettok();
		if ((x = asgexpr()) != NULL)
			ast_list_newexpr(list, x);
	}
	return list;
}

static struct ast_expr *
postfixexpr(void)
{
	struct ast_expr *x;

	switch (tok) {
	case TOK_IDENT:
		x = ast_expr_ident(token.t_ident);
		gettok();
		break;
	case TOK_FCON:
	case TOK_ICON:
		x = token.t_expr;
		gettok();
		break;
	case TOK_STRLIT:
		/* TODO: Handle wide strings, mixed normal and wide strings. */
		x = ast_expr_strlit();
		while (tok == TOK_STRLIT) {
			ast_expr_strlit_append(x, token.t_strlit);
			gettok();
		}
		ast_expr_strlit_finish(x);
		break;
	case '(':
		gettok();
		x = expr();
		expect(')');
		break;
	default:
		synh("malformed expression");
		return ast_expr_icon(&cursp, 0, &cir_int);
	}

	return postfixexpr_suffix(x);
}

static struct ast_expr *
postfixexpr_suffix(struct ast_expr *prefix)
{
	int op;
	struct ast_list *args = NULL;
	struct ast_expr *x;
	struct srcpos sp;

	for (;;) {
		SPCPY(&sp, &cursp);
		if (tok == '[') {
			gettok();
			x = expr();
			expect(']');
			prefix = ast_expr_subscr(&sp, prefix, x);
			continue;
		}
		if (tok == '(') {
			gettok();
			if (tok != ')')
				args = argexprlist();
			else
				args = ast_list(AST_EXPRLIST);
			expect(')');
			prefix = ast_expr_call(&sp, prefix, args);
			continue;
		}
		if (tok == '.' || tok == TOK_ARROW) {
			op = tok == '.' ? AST_SOUDIR : AST_SOUIND;
			if (gettok() != TOK_IDENT) {
				synexpect("identifier");
				break;
			}
			prefix = ast_expr_souref(&sp, op, prefix,
			    token.t_ident);
			gettok();
			continue;
		}
		if (tok == TOK_INC || tok == TOK_DEC) {
			op = tok == TOK_INC ? AST_POSTINC : AST_POSTDEC;
			prefix = ast_expr_incdec(&sp, op, prefix);
			gettok();
			continue;
		}
		break;
	}
	return prefix;
}

static struct ast_expr *
unexpr(void)
{
	int op;
	struct ast_expr *x;
	struct srcpos sp;

	SPCPY(&sp, &cursp);
	if (tok == TOK_INC || tok == TOK_DEC) {
		op = tok == TOK_INC ? AST_PREINC : AST_PREDEC;
		gettok();
		return ast_expr_incdec(&sp, op, unexpr());
	}
	if (tok == TOK_SIZEOF) {
		if (gettok() != '(')
			return ast_expr_sizeofx(&sp, unexpr());
		gettok();
		if (isinfirst_declspecs(tok)) {
			x = ast_expr_sizeoft(&sp, typename());
			expect(')');
			return x;
		}

		x = expr();
		expect(')');
		return ast_expr_sizeofx(&sp, postfixexpr_suffix(x));
	}

	switch (tok) {
	case '&':
	case '*':
	case '+':
	case '-':
	case '~':
	case '!':
		op = tok;
		gettok();
		x = ast_expr_unop(&sp, op, castexpr());
		break;
	default:
		x = postfixexpr();
	}
	return x;
}

static struct ast_expr *
castexpr(void)
{
	struct ast_decl *ad;
	struct ast_expr *x;
	struct srcpos sp;

	SPCPY(&sp, &cursp);
	if (tok != '(')
		return unexpr();

	gettok();
	if (isinfirst_declspecs(tok)) {
		ad = typename();
		expect(')');
		return ast_expr_cast(&sp, ad, castexpr());
	}

	x = expr();
	expect(')');
	return postfixexpr_suffix(x);
}

static struct ast_expr *
binexpr(int prec)
{
	int i = 0, j, op = 0;
	struct ast_expr *l = NULL, *r;
	struct srcpos sp;

again:
	if (i == 0)
		r = castexpr();
	else
		r = binexpr(i - 1);

	if (l == NULL)
		l = r;
	else
		l = ast_expr_bin(&sp, op, l, r);

	for (i = 0; i <= prec; i++) {
		for (j = 0; binopprecs[i][j] != -1; j++) {
			if (binopprecs[i][j] == tok) {
				op = tok;
				SPCPY(&sp, &cursp);
				gettok();
				goto again;
			}
		}
	}

	return l;
}

static struct ast_expr *
condexpr(void)
{
	struct ast_expr *l, *m;
	struct srcpos sp;

	l = binexpr(MAXBINPREC);
	if (tok != '?')
		return l;
	SPCPY(&sp, &cursp);
	gettok();
	m = expr();
	expect(':');
	return ast_expr_cond(&sp, l, m, condexpr());
}

static struct ast_expr *
asgexpr(void)
{
	int op;
	struct ast_expr *l;
	struct srcpos sp;

	l = condexpr();
	SPCPY(&sp, &cursp);
	switch (tok) {
	case '=':
	case TOK_MULASG:
	case TOK_DIVASG:
	case TOK_MODASG:
	case TOK_ADDASG:
	case TOK_SUBASG:
	case TOK_LSASG:
	case TOK_RSASG:
	case TOK_ANDASG:
	case TOK_XORASG:
	case TOK_ORASG:
		op = tok;
		gettok();
		return ast_expr_asg(&sp, op, l, asgexpr());
	default:
		break;
	}
	return l;
}

static struct ast_expr *
expr(void)
{
	struct ast_expr *l = NULL, *r;
	struct srcpos sp;

	if (!isinfirst(FIRST_expr, tok)) {
		synexpect("expression");
		skipto(FIRST_expr);
	}

	for (;;) {
		r = asgexpr();
		if (l == NULL)
			l = r;
		else
			l = ast_expr_comma(&sp, l, r);
		if (tok != ',')
			return l;
		SPCPY(&sp, &cursp);
		gettok();
	}

	/* NOTREACHED */
	return NULL;
}

static struct ast_stmt *
compound_stmt(struct srcpos *spp)
{
	char *ident;
	struct ast_stmt *block, *s;
	struct srcpos sp;

	openscope();
	block = ast_stmt_compound(spp);
	for (; tok != '}';) {
		if (tok == TOK_IDENT && peek() == ':') {
			ident = token.t_ident;
			SPCPY(&sp, &cursp);
			gettok();
			gettok();
			s = ast_stmt_label(&sp, ident, stmt());
			ast_stmt_compound_newstmt(block, s);
			continue;
		}
		if (isinfirst_declspecs(tok)) {
			ast_stmt_compound_newdecl(block, decl(0));
			continue;
		}

		ast_stmt_compound_newstmt(block, stmt());
	}
	closescope();
	return block;
}

static struct ast_stmt *
stmt(void)
{
	char *ident;
	int op;
	struct ast_expr *x, *y, *z;
	struct ast_stmt *s, *t;
	struct srcpos sp;

	static int skip[] = { ';', -1 };

	SPCPY(&sp, &cursp);
	if (tok == TOK_IDENT && peek() == ':') {
		ident = token.t_ident;
		gettok();
		gettok();
		s = stmt();
		return ast_stmt_label(&sp, ident, s);
	}
	switch (tok) {
	case ';':
		gettok();
		return ast_stmt_expr(&sp, NULL);
	case '{':
		gettok();
		s = compound_stmt(&sp);
		expect('}');
		return s;
	case TOK_CASE:
		gettok();
		x = expr();
		expect(':');
		return ast_stmt_case(&sp, x, stmt());
	case TOK_DEFAULT:
		gettok();
		expect(':');
		return ast_stmt_dflt(&sp, stmt());
	case TOK_IF:
		gettok();
		expect('(');
		x = expr();
		expect(')');
		s = stmt();
		t = NULL;
		if (tok == TOK_ELSE) {
			gettok();
			t = stmt();
		}
		return ast_stmt_if(&sp, x, s, t);
	case TOK_SWITCH:
	case TOK_WHILE:
		op = tok;
		gettok();
		expect('(');
		x = expr();
		expect(')');
		s = stmt();
		if (op == TOK_SWITCH)
			return ast_stmt_switch(&sp, x, s);
		return ast_stmt_while(&sp, x, s);
	case TOK_DO:
		gettok();
		s = stmt();
		expect(TOK_WHILE);
		expect('(');
		x = expr();
		expect(')');
		expect(';');
		return ast_stmt_do(&sp, s, x);
	case TOK_FOR:
		x = y = z = NULL;
		gettok();
		expect('(');
		if (tok != ';')
			x = expr();
		expect(';');
		if (tok != ';')
			y = expr();
		expect(';');
		if (tok != ')')
			z = expr();
		expect(')');
		return ast_stmt_for(&sp, x, y, z, stmt());
	case TOK_GOTO:
		if (gettok() != TOK_IDENT) {
			synexpect("identifier");
			skipto(skip);
			return NULL;
		}
		ident = token.t_ident;
		gettok();
		expect(';');
		return ast_stmt_goto(&sp, ident);
	case TOK_CONTINUE:
	case TOK_BREAK:
		op = tok == TOK_CONTINUE ? AST_CONTINUE : AST_BREAK;
		gettok();
		expect(';');
		return ast_stmt_brkcont(&sp, op);
	case TOK_RETURN:
		x = NULL;
		if (gettok() != ';')
			x = expr();
		expect(';');
		return ast_stmt_return(&sp, x);
	}
	if (isinfirst(FIRST_expr, tok)) {
		x = expr();
		expect(';');
		return ast_stmt_expr(&sp, x);
	}

	synexpect("statement");
	skipto(skip);
	return NULL;
}
