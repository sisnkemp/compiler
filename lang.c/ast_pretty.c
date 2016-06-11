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

#include <sys/types.h>
#include <sys/queue.h>

#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "comp/comp.h"
#include "comp/ir.h"

#include "lang.c/c.h"

static char *tyquals[] = { "const", "volatile", "const volatile" };
static int declprecs[] = { 1, 2, 2, 3 };
static int precs[] = {
	0,	/* AST_IDENT */
	0,	/* AST_ICON */
	0,	/* AST_FCON */
	0,	/* AST_STRLIT */
	1,	/* AST_SUBSCR */
	1,	/* AST_CALL */
	1,	/* AST_SOUDIR */
	1,	/* AST_SOUIND */
	2,	/* AST_POSTINC */
	2,	/* AST_POSTDEC */
	2,	/* AST_PREINC */
	2,	/* AST_PREDEC */
	2,	/* AST_ADDROF */
	2,	/* AST_DEREF */
	2,	/* AST_UPLUS */
	2,	/* AST_UMINUS */
	2,	/* AST_BITFLIP */
	2,	/* AST_NOT */
	2,	/* AST_SIZEOFX */
	2,	/* AST_SIZEOFT */
	2,	/* AST_CAST */
	3,	/* AST_MUL */
	3,	/* AST_DIV */
	3,	/* AST_MOD */
	4,	/* AST_ADD */
	4,	/* AST_SUB */
	6,	/* AST_LS */
	6,	/* AST_RS */
	7,	/* AST_LT */
	7,	/* AST_GT */
	7,	/* AST_LE */
	7,	/* AST_GE */
	8,	/* AST_EQ */
	8,	/* AST_NE */
	9,	/* AST_BWAND */
	10,	/* AST_BWXOR */
	11,	/* AST_BWOR */
	12,	/* AST_ANDAND */
	13,	/* AST_OROR */
	14,	/* AST_COND */
	15,	/* AST_ASG */
	15,	/* AST_MULASG */
	15,	/* AST_DIVASG */
	15,	/* AST_MODASG */
	15,	/* AST_ADDASG */
	15,	/* AST_SUBASG */
	15,	/* AST_LSASG */
	15,	/* AST_RSASG */
	15,	/* AST_ANDASG */
	15,	/* AST_XORASG */
	15,	/* AST_ORASG */
	16	/* AST_COMMA */
};

static void ifprintf(int, FILE *, const char *, ...);

static void declhead_pretty(int, FILE *, int, struct ast_declhead *);
static void decl_pretty(int, FILE *, int, struct ast_decl *);
static void declspecs_pretty(int, FILE *, struct ast_declspecs *);
static void souspec_pretty(int, FILE *, struct ast_souspec *);
static void enumspec_pretty(int, FILE *, struct ast_enumspec *);
static void decla_pretty(int, FILE *, struct ast_decla *);
static void braceddecla_pretty(int, FILE *, struct ast_decla *,
    struct ast_decla *);
static void list_pretty(int, FILE *, struct ast_list *);
static void init_pretty(int, FILE *, struct ast_init *);
static void designation_pretty(int, FILE *, struct ast_designation *);
static void stmt_pretty(int, FILE *, struct ast_stmt *);
static void expr_pretty(int, FILE *, struct ast_expr *);
static void strlit_pretty(int, FILE *, struct ast_expr *);
static void bracedexpr_pretty(int, FILE *, struct ast_expr *,
    struct ast_expr *);

static void
ifprintf(int indent, FILE *fp, const char *fmt, ...)
{
	va_list ap;

	while (indent--)
		fprintf(fp, "\t");
	va_start(ap, fmt);
	vfprintf(fp, fmt, ap);
	va_end(ap);
}

void
ast_pretty(char *name)
{
	FILE *fp;

	static int dumpno;
	
	if (name != NULL)
		fp = dump_open("AST", name, "w", dumpno++);
	else
		fp = stderr;

	declhead_pretty(0, fp, ';', &ast_program);
	fprintf(fp, "\n");
	if (name != NULL)
		fclose(fp);
}

static void
declhead_pretty(int indent, FILE *fp, int term, struct ast_declhead *dh)
{
	struct ast_decl *decl;

	SIMPLEQ_FOREACH(decl, dh, ad_link) {
		if (decl != SIMPLEQ_FIRST(dh))
			fprintf(fp, "\n");
		decl_pretty(0, fp, term, decl);
	}
}

static void
decl_pretty(int indent, FILE *fp, int term, struct ast_decl *decl)
{
	int ind = indent;
	struct ast_decla *decla;

	if (decl->ad_ds != NULL) {
		declspecs_pretty(ind, fp, decl->ad_ds);
		fprintf(fp, " ");
		ind = 0;
	}
	SIMPLEQ_FOREACH(decla, &decl->ad_declas, ad_link) {
		decla_pretty(ind, fp, decla);
		if (SIMPLEQ_NEXT(decla, ad_link) !=
		    SIMPLEQ_END(&decl->ad_declas))
			fprintf(fp, ", ");	
	}
	if (decl->ad_stmt == NULL) {
		fprintf(fp, "%c", term);
		return;
	}
	fprintf(fp, "\n");
	if (!SIMPLEQ_EMPTY(&decl->ad_krdecls)) {
		declhead_pretty(indent + 1, fp, ';', &decl->ad_krdecls);
		fprintf(fp, "\n");
	}
	stmt_pretty(indent, fp, decl->ad_stmt);
}

static void
declspecs_pretty(int indent, FILE *fp, struct ast_declspecs *ds)
{
	int gap = 0, ind = indent;
	struct ast_tyspec *ts;

	static char *sclasses[] = {
		"auto", "extern", "register", "static", "typedef"
	};
	static char *fnspecs[] = { "inline" };

	if (ds->ad_sclass > 0) {
		ifprintf(ind, fp, "%s", sclasses[ds->ad_sclass - 1]);
		ind = 0;
		gap = 1;
	}
	if (ds->ad_fnspec > 0) {
		ifprintf(ind, fp, gap ? " %s" : "%s",
		    fnspecs[ds->ad_fnspec - 1]);
		ind = 0;
		gap = 1;
	}
	if (ds->ad_tyqual > 0) {
		ifprintf(ind, fp, gap ? " %s" : "%s",
		    tyquals[ds->ad_tyqual - 1]);
		ind = 0;
		gap = 1;
	}

	if (gap)
		fprintf(fp, " ");
	SIMPLEQ_FOREACH(ts, &ds->ad_tyspec, at_link) {
		switch (ts->at_op) {
		case AST_TYSPEC_SIMPLE:
			switch (ts->at_simpletok) {
			case TOK_VOID:
				ifprintf(ind, fp, "void");
				break;
			case TOK_CHAR:
				ifprintf(ind, fp, "char");
				break;
			case TOK_SHORT:
				ifprintf(ind, fp, "short");
				break;
			case TOK_INT:
				ifprintf(ind, fp, "int");
				break;
			case TOK_LONG:
				ifprintf(ind, fp, "long");
				break;
			case TOK_FLOAT:
				ifprintf(ind, fp, "float");
				break;
			case TOK_DOUBLE:
				ifprintf(ind, fp, "double");
				break;
			case TOK_SIGNED:
				ifprintf(ind, fp, "signed");
				break;
			case TOK_UNSIGNED:
				ifprintf(ind, fp, "unsigned");
				break;
			case TOK_BOOL:
				ifprintf(ind, fp, "_Bool");
				break;
			default:
				fatalx("declspecs_pretty: bad token: %d",
				    ts->at_simpletok);
			}
			break;
		case AST_TYSPEC_TDNAME:
			ifprintf(ind, fp, "%s", ts->at_tdname);
			break;
		case AST_TYSPEC_STRUCT:
		case AST_TYSPEC_UNION:
			if (ts->at_op == AST_TYSPEC_STRUCT)
				ifprintf(ind, fp, "struct ");
			else
				ifprintf(ind, fp, "union ");
			souspec_pretty(indent, fp, ts->at_sou);
			break;
		case AST_TYSPEC_ENUM:
			ifprintf(ind, fp, "enum ");
			enumspec_pretty(indent, fp, ts->at_enum);
			break;
		default:
			fatalx("declspecs_pretty: bad at_op: %d", ts->at_op);
		}
		if (SIMPLEQ_NEXT(ts, at_link) != SIMPLEQ_END(&ds->ad_tyspec))
			fprintf(fp, " ");
		ind = 0;
	}
}

static void
souspec_pretty(int indent, FILE *fp, struct ast_souspec *sou)
{
	struct ast_souent *ent;

	if (sou->as_name != NULL)
		fprintf(fp, "%s", sou->as_name);
	if (SIMPLEQ_EMPTY(&sou->as_ents))
		return;
	if (sou->as_name != NULL)
		fprintf(fp, " ");
	fprintf(fp, "{\n");
	SIMPLEQ_FOREACH(ent, &sou->as_ents, as_link) {
		declspecs_pretty(indent + 1, fp, ent->as_ds);
		fprintf(fp, " ");
		decla_pretty(0, fp, ent->as_decla);
		if (ent->as_fieldexpr != NULL) {
			fprintf(fp, " : ");
			expr_pretty(0, fp, ent->as_fieldexpr);
		}
		fprintf(fp, ";\n");
	}
	fprintf(fp, "}");
	if (sou->as_flags & AST_SOU_PACKED)
		fprintf(fp, " __packed");
}

static void
enumspec_pretty(int indent, FILE *fp, struct ast_enumspec *enu)
{
	struct ast_enument *ent;

	if (enu->aen_ident != NULL)
		fprintf(fp, "%s", enu->aen_ident);
	if (SIMPLEQ_EMPTY(&enu->aen_ents))
		return;
	if (enu->aen_ident != NULL)
		fprintf(fp, " ");
	fprintf(fp, "{\n");
	SIMPLEQ_FOREACH(ent, &enu->aen_ents, aen_link) {
		ifprintf(indent + 1, fp, "%s", ent->aen_ident);
		if (ent->aen_expr != NULL) {
			fprintf(fp, " = ");
			expr_pretty(0, fp, ent->aen_expr);
		}
		fprintf(fp, ",");
	}
	fprintf(fp, "}");
}

static void
decla_pretty(int indent, FILE *fp, struct ast_decla *decla)
{
	if (decla == NULL)
		return;

	switch (decla->ad_op) {
	case AST_ARRDECLA:
		braceddecla_pretty(indent, fp, decla, decla->ad_decla);
		fprintf(fp, "[");
		if (decla->ad_expr != NULL)
			expr_pretty(0, fp, decla->ad_expr);
		fprintf(fp, "]");
		break;
	case AST_PTRDECLA:
		ifprintf(indent, fp, "*");
		if (decla->ad_tqual != 0)
			fprintf(fp, " %s ", tyquals[decla->ad_tqual - 1]);
		decla_pretty(0, fp, decla->ad_decla);
		break;
	case AST_FNDECLA:
		braceddecla_pretty(indent, fp, decla, decla->ad_decla);
		fprintf(fp, "(");
		if (decla->ad_parms != NULL)
			list_pretty(0, fp, decla->ad_parms);
		fprintf(fp, ")");
		break;
	case AST_IDDECLA:
		ifprintf(indent, fp, "%s", decla->ad_ident);
		break;
	default:
		fatalx("decla_pretty: bad op: %d", decla->ad_op);
	}
	if (decla->ad_init != NULL) {
		fprintf(fp, " =");
		init_pretty(0, fp, decla->ad_init);
	}
}

static void
braceddecla_pretty(int indent, FILE *fp, struct ast_decla *top,
    struct ast_decla *sub)
{
	if (sub == NULL)
		return;
	if (declprecs[sub->ad_op - 1] > declprecs[top->ad_op - 1]) {
		ifprintf(indent, fp, "(");
		decla_pretty(0, fp, sub);
		fprintf(fp, ")");
		return;
	}
	decla_pretty(indent, fp, sub);
}

static void
list_pretty(int indent, FILE *fp, struct ast_list *list)
{
	int ind = indent;
	struct ast_decl *decl;
	struct ast_expr *x;

	if (list->al_op == AST_IDLIST || list->al_op == AST_PARLIST) {
		SIMPLEQ_FOREACH(decl, &list->al_decls, ad_link) {
			if (decl->ad_ds != NULL) {
				declspecs_pretty(ind, fp, decl->ad_ds);
				if (SIMPLEQ_NEXT(decl, ad_link) !=
				    SIMPLEQ_END(&list->al_decls))
					fprintf(fp, " ");
				ind = 0;
				if (!SIMPLEQ_EMPTY(&decl->ad_declas))
					fprintf(fp, " ");
			}
			decla_pretty(ind, fp, SIMPLEQ_FIRST(&decl->ad_declas));
			ind = 0;
			if (SIMPLEQ_NEXT(decl, ad_link) !=
			    SIMPLEQ_END(&list->al_decls))
				fprintf(fp, ", ");
		}
		return;
	}

	TAILQ_FOREACH(x, &list->al_exprs, ae_link) {
		expr_pretty(0, fp, x);
		if (TAILQ_NEXT(x, ae_link) != TAILQ_END(&list->al_exprs))
			fprintf(fp, ", ");
	}
}

static void
init_pretty(int indent, FILE *fp, struct ast_init *init)
{
	struct ast_init *i;

	if (init->ai_op == AST_SIMPLEINIT) {
		expr_pretty(indent, fp, init->ai_expr);
		return;
	}

	ifprintf(indent, fp, " { ");
	SIMPLEQ_FOREACH(i, &init->ai_inits, ai_link) {
		if (i->ai_desig != NULL)
			designation_pretty(0, fp, i->ai_desig);
		init_pretty(0, fp, i);
		if (SIMPLEQ_NEXT(i, ai_link) != SIMPLEQ_END(&init->ai_inits))
			fprintf(fp, ",");
		fprintf(fp, " ");
	}
	fprintf(fp, "}");
}

static void
designation_pretty(int indent, FILE *fp, struct ast_designation *desig)
{
	int ind = indent;
	struct ast_designator *d;

	SIMPLEQ_FOREACH(d, desig, ad_link) {
		if (d->ad_op == AST_DOTDESIG)
			ifprintf(ind, fp, ".%s", d->ad_ident);
		else {
			ifprintf(ind, fp, "[");
			expr_pretty(0, fp, d->ad_expr);
			fprintf(fp, "]");
		}	
		ind = 0;
	}
	fprintf(fp, " = ");
}

static void
stmt_pretty(int indent, FILE *fp, struct ast_stmt *s)
{
	int ind = indent;
	struct ast_stmt *stmt;

	switch (s->as_op) {
	case AST_COMPOUND:
		ifprintf(indent, fp, "{\n");
		SIMPLEQ_FOREACH(stmt, &s->as_stmts, as_link)
			stmt_pretty(indent + 1, fp, stmt);
		ifprintf(indent, fp, "}\n");
		break;
	case AST_EXPR:
		if (s->as_exprs[0] != NULL) {
			expr_pretty(indent, fp, s->as_exprs[0]);
			ind = 0;
		}
		ifprintf(ind, fp, ";\n");
		break;
	case AST_LABEL:
		fprintf(fp, "%s:\n", s->as_ident);
		stmt_pretty(indent, fp, s->as_stmt1);
		break;
	case AST_CASE:
		ifprintf(indent, fp, "case ");
		expr_pretty(0, fp, s->as_exprs[0]);
		fprintf(fp, ":\n");
		if (s->as_stmt1->as_op != AST_COMPOUND)
			ind++;
		stmt_pretty(ind, fp, s->as_stmt1);
		break;
	case AST_DEFAULT:
		ifprintf(indent, fp, "default:\n");
		if (s->as_stmt1->as_op != AST_COMPOUND)
			ind++;
		stmt_pretty(ind, fp, s->as_stmt1);
		break;
	case AST_IF:
		ifprintf(indent, fp, "if (");
		expr_pretty(0, fp, s->as_exprs[0]);
		fprintf(fp, ")\n");
		if (s->as_stmt1->as_op != AST_COMPOUND)
			ind++;
		stmt_pretty(ind, fp, s->as_stmt1);
		if (s->as_stmt2 == NULL)
			break;
		ifprintf(indent, fp, "else\n");
		ind = indent;
		if (s->as_stmt2->as_op != AST_COMPOUND)
			ind++;
		stmt_pretty(ind, fp, s->as_stmt2);
		break;
	case AST_SWITCH:
		ifprintf(indent, fp, "switch (");
		expr_pretty(0, fp, s->as_exprs[0]);
		fprintf(fp, ")\n");
		if (s->as_stmt1->as_op != AST_COMPOUND)
			ind++;
		stmt_pretty(ind, fp, s->as_stmt1);
		break;
	case AST_WHILE:
		ifprintf(indent, fp, "while (");
		expr_pretty(0, fp, s->as_exprs[0]);
		fprintf(fp, ")\n");
		if (s->as_stmt1->as_op != AST_COMPOUND)
			ind++;
		stmt_pretty(ind, fp, s->as_stmt1);
		break;
	case AST_DO:
		ifprintf(indent, fp, "do\n");
		if (s->as_stmt1->as_op != AST_COMPOUND)
			ind++;
		stmt_pretty(ind, fp, s->as_stmt1);
		ifprintf(indent, fp, "while (");
		expr_pretty(0, fp, s->as_exprs[0]);
		fprintf(fp, ");\n");
		break;
	case AST_FOR:
		ifprintf(indent, fp, "for (");
		if (s->as_exprs[0] != NULL)
			expr_pretty(0, fp, s->as_exprs[0]);
		fprintf(fp, "; ");
		if (s->as_exprs[1] != NULL)
			expr_pretty(0, fp, s->as_exprs[1]);
		fprintf(fp, "; ");
		if (s->as_exprs[2] != NULL)
			expr_pretty(0, fp, s->as_exprs[2]);
		fprintf(fp, ")\n");
		if (s->as_stmt1->as_op != AST_COMPOUND)
			ind++;
		stmt_pretty(ind, fp, s->as_stmt1);
		break;
	case AST_GOTO:
		ifprintf(indent, fp, "goto %s;\n", s->as_ident);
		break;
	case AST_CONTINUE:
	case AST_BREAK:
		ifprintf(indent, fp,
		    s->as_op == AST_CONTINUE ? "continue" : "break");
		fprintf(fp, ";\n");
		break;
	case AST_RETURN:
		ifprintf(indent, fp, "return");
		if (s->as_exprs[0] != NULL) {
			fprintf(fp, " ");
			expr_pretty(0, fp, s->as_exprs[0]);
		}
		fprintf(fp, ";\n");
		break;
	case AST_DECLSTMT:
		decl_pretty(indent, fp, ';', s->as_decl);
		fprintf(fp, "\n");
		break;
	default:
		fatalx("stmt_pretty: bad op: %d", s->as_op);
	}
}

static void
expr_pretty(int indent, FILE *fp, struct ast_expr *x)
{
	static char *unops[] = { "&", "*", "+", "-", "~", "!" };
	static char *binops[] = {
		"*", "/", "%", "+", "-", "<<", ">>", "<", ">", "<=", ">=",
		"==", "!=", "&", "^", "|", "&&", "||", "?:", "=", "*=", "/=",
		"%=", "+=", "-=", "<<=", ">>=", "&=", "^=", "|="
	};

	if (x->ae_op >= AST_MUL && x->ae_op <= AST_ORASG &&
	    x->ae_op != AST_COND) {
		bracedexpr_pretty(indent, fp, x, x->ae_l);
		fprintf(fp, " %s ", binops[x->ae_op - AST_MUL]);
		bracedexpr_pretty(0, fp, x, x->ae_r);
		return;
	}

	switch (x->ae_op) {
	case AST_IDENT:
		ifprintf(indent, fp, "%s", x->ae_ident);
		break;
	case AST_ICON:
		if (IR_ISSIGNED(x->ae_type))
			ifprintf(indent, fp, "%jd", x->ae_con.ic_icon);
		else if (IR_ISUNSIGNED(x->ae_type))
			ifprintf(indent, fp, "%juU", x->ae_con.ic_ucon);
		break;
	case AST_FCON:
		ifprintf(indent, fp, "%f", x->ae_con.ic_fcon);
		break;
	case AST_STRLIT:
		strlit_pretty(indent, fp, x);
		break;
	case AST_SUBSCR:
		bracedexpr_pretty(indent, fp, x, x->ae_l);
		fprintf(fp, "[");
		expr_pretty(0, fp, x->ae_r);
		fprintf(fp, "]");
		break;
	case AST_CALL:
		bracedexpr_pretty(indent, fp, x, x->ae_l);
		fprintf(fp, "(");
		if (x->ae_args != NULL)
			list_pretty(0, fp, x->ae_args);
		fprintf(fp, ")");
		break;
	case AST_SOUDIR:
	case AST_SOUIND:
		bracedexpr_pretty(indent, fp, x, x->ae_l);
		fprintf(fp, x->ae_op == AST_SOUDIR ? "." : "->");
		fprintf(fp, "%s", x->ae_ident);
		break;
	case AST_POSTINC:
	case AST_POSTDEC:
		bracedexpr_pretty(indent, fp, x, x->ae_l);
		fprintf(fp, x->ae_op == AST_POSTINC ? "++" : "--");
		break;
	case AST_PREINC:
	case AST_PREDEC:
		ifprintf(indent, fp, x->ae_op == AST_PREINC ? "++" : "--");
		bracedexpr_pretty(0, fp, x, x->ae_l);
		break;
	case AST_ADDROF:
	case AST_DEREF:
	case AST_UPLUS:
	case AST_UMINUS:
	case AST_BITFLIP:
	case AST_NOT:
		ifprintf(indent, fp, "%s", unops[x->ae_op - AST_ADDROF]);
		bracedexpr_pretty(0, fp, x, x->ae_l);
		break;
	case AST_SIZEOFX:
		ifprintf(indent, fp, "sizeof ");
		bracedexpr_pretty(0, fp, x, x->ae_l);
		break;
	case AST_SIZEOFT:
	case AST_CAST:
		if (x->ae_op == AST_SIZEOFT)
			ifprintf(indent, fp, "sizeof(");
		else
			ifprintf(indent, fp, "(");
		declspecs_pretty(0, fp, x->ae_decl->ad_ds);
		if (!SIMPLEQ_EMPTY(&x->ae_decl->ad_declas))
			fprintf(fp, " ");
		decla_pretty(0, fp, SIMPLEQ_FIRST(&x->ae_decl->ad_declas));
		fprintf(fp, ")");
		if (x->ae_op != AST_SIZEOFT)
			bracedexpr_pretty(0, fp, x, x->ae_l);
		break;
	case AST_COND:
		bracedexpr_pretty(indent, fp, x, x->ae_l);
		fprintf(fp, " ? ");
		bracedexpr_pretty(0, fp, x, x->ae_m);
		fprintf(fp, " : ");
		bracedexpr_pretty(0, fp, x, x->ae_r);
		break;
	case AST_COMMA:
		expr_pretty(indent, fp, x->ae_l);
		fprintf(fp, ", ");
		expr_pretty(0, fp, x->ae_r);
		break;
	default:
		fatalx("expr_pretty: bad op: %d", x->ae_op);
	}
}

static void
strlit_pretty(int indent, FILE *fp, struct ast_expr *x)
{
	while (indent--)
		fprintf(fp, "\t");
	emitcstring(fp, x->ae_str, x->ae_strlen);
}

static void
bracedexpr_pretty(int indent, FILE *fp, struct ast_expr *top,
    struct ast_expr *sub)
{
	if (top->ae_op <= 0 || top->ae_op > AST_COMMA)
		fatalx("bracedexpr_pretty: bad top op: %d", top->ae_op);
	if (sub->ae_op <= 0 || sub->ae_op > AST_COMMA)
		fatalx("bracedexpr_pretty: bad sub op: %d", sub->ae_op);

	if (precs[sub->ae_op - 1] > precs[top->ae_op - 1]) {
		ifprintf(indent, fp, "(");
		expr_pretty(0, fp, sub);
		fprintf(fp, ")");
		return;
	}
	expr_pretty(indent, fp, sub);
}
