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

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "comp/comp.h"
#include "comp/ir.h"

#include "lang.c/c.h"

struct ast_declhead ast_program = SIMPLEQ_HEAD_INITIALIZER(ast_program);

static struct ast_tyspec *
tyspecalloc(int op)
{
	struct ast_tyspec *ts;

	ts = mem_alloc(&frontmem, sizeof *ts);
	SPCPY(&ts->at_sp, &cursp);
	ts->at_tdname = NULL;
	ts->at_sou = NULL;
	ts->at_enum = NULL;
	ts->at_simpletok = 0;
	ts->at_op = op;
	return ts;
}

struct ast_tyspec *
ast_tyspec_simple(int tok)
{
	struct ast_tyspec *ts;

	ts = tyspecalloc(AST_TYSPEC_SIMPLE);
	ts->at_simpletok = tok;
	return ts;
}

struct ast_tyspec *
ast_tyspec_tdname(char *ident)
{
	struct ast_tyspec *ts;

	ts = tyspecalloc(AST_TYSPEC_TDNAME);
	ts->at_tdname = ident;
	return ts;
}

struct ast_tyspec *
ast_tyspec_sou(int op, struct ast_souspec *sou)
{
	struct ast_tyspec *ts;

	ts = tyspecalloc(op);
	ts->at_sou = sou;
	return ts;
}

struct ast_tyspec *
ast_tyspec_enum(struct ast_enumspec *enu)
{
	struct ast_tyspec *ts;

	ts = tyspecalloc(AST_TYSPEC_ENUM);
	ts->at_enum = enu;
	return ts;
}

struct ast_souspec *
ast_souspec(char *ident, int haselms)
{
	struct ast_souspec *sou;

	sou = mem_alloc(&frontmem, sizeof *sou);
	SIMPLEQ_INIT(&sou->as_ents);
	SPCPY(&sou->as_sp, &cursp);
	sou->as_name = ident;
	if (haselms)
		sou->as_flags = AST_SOU_HASELMS;
	else
		sou->as_flags = 0;
	return sou;
}

struct ast_souent *
ast_souent(struct ast_declspecs *ds, struct ast_decla *decla,
    struct ast_expr *x)
{
	struct ast_souent *ent;

	ent = mem_alloc(&frontmem, sizeof *ent);
	ent->as_ds = ds;
	ent->as_decla = decla;
	ent->as_fieldexpr = x;
	return ent;
}

void
ast_souspec_newent(struct ast_souspec *sou, struct ast_souent *ent)
{
	if (ent == NULL)
		return;
	SIMPLEQ_INSERT_TAIL(&sou->as_ents, ent, as_link);
}

struct ast_enumspec *
ast_enumspec(char *ident, int haselms)
{
	struct ast_enumspec *enu;

	enu = mem_alloc(&frontmem, sizeof *enu);
	SIMPLEQ_INIT(&enu->aen_ents);
	SPCPY(&enu->aen_sp, &cursp);
	enu->aen_ident = ident;
	enu->aen_haselms = haselms;
	return enu;
}

void
ast_enumspec_newent(struct ast_enumspec *enu, struct ast_enument *ent)
{
	if (ent == NULL)
		return;
	SIMPLEQ_INSERT_TAIL(&enu->aen_ents, ent, aen_link);
}

struct ast_enument *
ast_enument(struct srcpos *sp, char *ident, struct ast_expr *x)
{
	struct ast_enument *ent;

	ent = mem_alloc(&frontmem, sizeof *ent);
	SPCPY(&ent->aen_sp, sp);
	ent->aen_ident = ident;
	ent->aen_expr = x;
	return ent;
}

struct ast_declspecs *
ast_declspecs(void)
{
	struct ast_declspecs *ds;

	ds = mem_alloc(&frontmem, sizeof *ds);
	SIMPLEQ_INIT(&ds->ad_tyspec);
	SPCPY(&ds->ad_sp, &cursp);
	ds->ad_sclass = ds->ad_tyqual = ds->ad_fnspec = 0;
	return ds;
}

void
ast_declspecs_newsclass(struct ast_declspecs *ds, int sclass)
{
	if (ds->ad_sclass != 0)
		errh("at most one storage class specifier is permitted");
	else
		ds->ad_sclass = sclass;
}

void
ast_declspecs_newtyqual(struct ast_declspecs *ds, int tyqual)
{
	ds->ad_tyqual |= tyqual;
}

void
ast_declspecs_newfnspec(struct ast_declspecs *ds, int fnspec)
{
	ds->ad_fnspec |= fnspec;
}

void
ast_declspecs_newtyspec(struct ast_declspecs *ds, struct ast_tyspec *ts)
{
	if (ts == NULL)
		return;
	SIMPLEQ_INSERT_TAIL(&ds->ad_tyspec, ts, at_link);
}

struct ast_decla *
declaalloc(int op, struct srcpos *sp)
{
	struct ast_decla *decla;

	decla = mem_alloc(&frontmem, sizeof *decla);
	SPCPY(&decla->ad_sp, sp);
	decla->ad_decla = NULL;
	decla->ad_ident = NULL;
	decla->ad_sym = NULL;
	decla->ad_parms = NULL;
	decla->ad_expr = NULL;
	decla->ad_init = NULL;
	decla->ad_op = 0;
	decla->ad_op = op;
	return decla;
}

struct ast_decla *
ast_decla_array(struct srcpos *sp, struct ast_decla *decla, struct ast_expr *x)
{
	struct ast_decla *rv;

	rv = declaalloc(AST_ARRDECLA, sp);
	rv->ad_decla = decla;
	rv->ad_expr = x;
	return rv;
}

struct ast_decla *
ast_decla_ptr(struct srcpos *sp, struct ast_decla *decla, int tq)
{
	struct ast_decla *rv;

	rv = declaalloc(AST_PTRDECLA, sp);
	rv->ad_decla = decla;
	rv->ad_tqual = tq;
	return rv;
}

struct ast_decla *
ast_decla_func(struct srcpos *sp, struct ast_decla *decla,
    struct ast_list *parms)
{
	struct ast_decla *rv;

	rv = declaalloc(AST_FNDECLA, sp);
	rv->ad_decla = decla;
	rv->ad_parms = parms;
	return rv;
}

struct ast_decla *
ast_decla_ident(char *ident)
{
	struct ast_decla *rv;

	rv = declaalloc(AST_IDDECLA, &cursp);
	rv->ad_ident = ident;
	return rv;
}

void
ast_decla_setinit(struct ast_decla *decla, struct ast_init *init)
{
	decla->ad_init = init;
}

struct ast_designation *
ast_designation(void)
{
	struct ast_designation *designation;

	designation = mem_alloc(&frontmem, sizeof *designation);
	SIMPLEQ_INIT(designation);
	return designation;
}

void
ast_designation_newdesig(struct ast_designation *designation,
    struct ast_designator *designator)
{
	if (designator == NULL)
		return;
	SIMPLEQ_INSERT_TAIL(designation, designator, ad_link);
}

static struct ast_designator *
designatoralloc(int op, struct ast_expr *x, char *ident, struct srcpos *sp)
{
	struct ast_designator *d;

	d = mem_alloc(&frontmem, sizeof *d);
	SPCPY(&d->ad_sp, sp);
	d->ad_ident = ident;
	d->ad_expr = x;
	d->ad_op = op;
	return d;
}

struct ast_designator *
ast_designator_ident(struct srcpos *sp, char *ident)
{
	return designatoralloc(AST_DOTDESIG, NULL, ident, sp);
}

struct ast_designator *
ast_designator_array(struct srcpos *sp, struct ast_expr *x)
{
	return designatoralloc(AST_ARRDESIG, x, NULL, sp);
}

struct ast_init *
initalloc(int op, struct ast_expr *x, struct srcpos *sp, size_t elems)
{
	struct ast_init *init;

	init = mem_alloc(&frontmem, sizeof *init);
	SIMPLEQ_INIT(&init->ai_inits);
	SPCPY(&init->ai_sp, sp);
	init->ai_desig = NULL;
	init->ai_expr = x;
	init->ai_objty = NULL;
	init->ai_elems = elems;
	init->ai_bitoff = 0;
	init->ai_bitsize = 0;
	init->ai_op = op;
	return init;
}

struct ast_init *
ast_init_list(void)
{
	return initalloc(AST_LISTINIT, NULL, &cursp, 0);
}

struct ast_init *
ast_init_simple(struct srcpos *sp, struct ast_expr *x)
{
	return initalloc(AST_SIMPLEINIT, x, sp, 1);
}

void
ast_init_newinit(struct ast_init *list, struct ast_init *init)
{
	if (list->ai_op != AST_LISTINIT)
		fatalx("ast_init_newinit: not an initializer list");
	list->ai_elems++;
	SIMPLEQ_INSERT_TAIL(&list->ai_inits, init, ai_link);
}

void
ast_init_setdesig(struct ast_init *init, struct ast_designation *desig)
{
	init->ai_desig = desig;
}

static void
declheadinit(struct ast_declhead *dh)
{
	SIMPLEQ_INIT(dh);
}

void
ast_declhead_newdecl(struct ast_declhead *dh, struct ast_decl *decl)
{
	if (decl == NULL)
		return;
	SIMPLEQ_INSERT_TAIL(dh, decl, ad_link);
}

struct ast_decl *
ast_decl(struct ast_declspecs *ds)
{
	struct ast_decl *decl;

	decl = mem_alloc(&frontmem, sizeof *decl);
	SIMPLEQ_INIT(&decl->ad_declas);
	SPCPY(&decl->ad_sp, &cursp);
	decl->ad_ds = ds;
	declheadinit(&decl->ad_krdecls);
	decl->ad_stmt = NULL;
	return decl;
}

void
ast_decl_newdecla(struct ast_decl *decl, struct ast_decla *decla)
{
	if (decla == NULL)
		return;
	SIMPLEQ_INSERT_TAIL(&decl->ad_declas, decla, ad_link);
}

void
ast_decl_newkrdecl(struct ast_decl *decl, struct ast_decl *krdecl)
{
	if (krdecl == NULL)
		return;
	SIMPLEQ_INSERT_TAIL(&decl->ad_krdecls, krdecl, ad_link);
}

void
ast_decl_setstmt(struct ast_decl *decl, struct ast_stmt *stmt)
{
	decl->ad_stmt = stmt;
}

struct ast_list *
ast_list(int op)
{
	struct ast_list *list;

	list = mem_alloc(&frontmem, sizeof *list);
	declheadinit(&list->al_decls);
	TAILQ_INIT(&list->al_exprs);
	SPCPY(&list->al_sp, &cursp);
	list->al_ellipsis = 0;
	list->al_op = op;
	return list;
}

void
ast_list_newdecl(struct ast_list *list, struct ast_decl *decl)
{
	ast_declhead_newdecl(&list->al_decls, decl);
}

void
ast_list_newexpr(struct ast_list *list, struct ast_expr *x)
{
	if (list->al_op != AST_EXPRLIST)
		fatalx("ast_list_newexpr: not an AST_EXPRLIST");
	TAILQ_INSERT_TAIL(&list->al_exprs, x, ae_link);
}

void
ast_list_setellipsis(struct ast_list *list)
{
	list->al_ellipsis = 1;
}

static struct ast_stmt *
stmtalloc(int op, struct srcpos *sp)
{
	struct ast_stmt *s;

	s = mem_alloc(&frontmem, sizeof *s);
	SIMPLEQ_INIT(&s->as_stmts);
	SIMPLEQ_INIT(&s->as_cases);
	SPCPY(&s->as_sp, sp);
	s->as_exprs[0] = s->as_exprs[1] = s->as_exprs[2] = NULL;
	s->as_stmt1 = s->as_stmt2 = NULL;
	s->as_ident = NULL;
	s->as_decl = NULL;
	s->as_sym = NULL;
	s->as_op = op;
	s->as_irlbl = NULL;
	return s;
}

struct ast_stmt *
ast_stmt_compound(struct srcpos *sp)
{
	struct ast_stmt *s;

	s = stmtalloc(AST_COMPOUND, sp);
	return s;
}

void
ast_stmt_compound_newdecl(struct ast_stmt *block, struct ast_decl *decl)
{
	struct ast_stmt *s;

	if (decl == NULL)
		return;
	s = stmtalloc(AST_DECLSTMT, &decl->ad_sp);
	s->as_decl = decl;
	ast_stmt_compound_newstmt(block, s);
}

void
ast_stmt_compound_newstmt(struct ast_stmt *block, struct ast_stmt *s)
{
	if (s == NULL)
		return;
	SIMPLEQ_INSERT_TAIL(&block->as_stmts, s, as_link);
}

struct ast_stmt *
ast_stmt_label(struct srcpos *sp, char *ident, struct ast_stmt *s)
{
	struct ast_stmt *stmt;

	stmt = stmtalloc(AST_LABEL, sp);
	stmt->as_ident = ident;
	stmt->as_stmt1 = s;
	return stmt;
}

struct ast_stmt *
ast_stmt_case(struct srcpos *sp, struct ast_expr *x, struct ast_stmt *s)
{
	struct ast_stmt *stmt;

	stmt = stmtalloc(AST_CASE, sp);
	stmt->as_exprs[0] = x;
	stmt->as_stmt1 = s;
	return stmt;
}

struct ast_stmt *
ast_stmt_dflt(struct srcpos *sp, struct ast_stmt *s)
{
	struct ast_stmt *stmt;

	stmt = stmtalloc(AST_DEFAULT, sp);
	stmt->as_stmt1 = s;
	return stmt;
}

struct ast_stmt *
ast_stmt_if(struct srcpos *sp, struct ast_expr *x, struct ast_stmt *ifst,
    struct ast_stmt *elsest)
{
	struct ast_stmt *stmt;

	stmt = stmtalloc(AST_IF, sp);
	stmt->as_stmt1 = ifst;
	stmt->as_stmt2 = elsest;
	stmt->as_exprs[0] = x;
	return stmt;
}

struct ast_stmt *
ast_stmt_switch(struct srcpos *sp, struct ast_expr *x, struct ast_stmt *s)
{
	struct ast_stmt *stmt;

	stmt = stmtalloc(AST_SWITCH, sp);
	stmt->as_stmt1 = s;
	stmt->as_exprs[0] = x;
	return stmt;
}

struct ast_stmt *
ast_stmt_while(struct srcpos *sp, struct ast_expr *x, struct ast_stmt *s)
{
	struct ast_stmt *stmt;

	stmt = stmtalloc(AST_WHILE, sp);
	stmt->as_stmt1 = s;
	stmt->as_exprs[0] = x;
	return stmt;
}

struct ast_stmt *
ast_stmt_do(struct srcpos *sp, struct ast_stmt *s, struct ast_expr *x)
{
	struct ast_stmt *stmt;

	stmt = stmtalloc(AST_DO, sp);
	stmt->as_stmt1 = s;
	stmt->as_exprs[0] = x;
	return stmt;
}

struct ast_stmt *
ast_stmt_for(struct srcpos *sp, struct ast_expr *x, struct ast_expr *y,
    struct ast_expr *z, struct ast_stmt *s)
{
	struct ast_stmt *stmt;

	stmt = stmtalloc(AST_FOR, sp);
	stmt->as_stmt1 = s;
	stmt->as_exprs[0] = x;
	stmt->as_exprs[1] = y;
	stmt->as_exprs[2] = z;
	return stmt;
}

struct ast_stmt *
ast_stmt_goto(struct srcpos *sp, char *ident)
{
	struct ast_stmt *stmt;

	stmt = stmtalloc(AST_GOTO, sp);
	stmt->as_ident = ident;
	return stmt;
}

struct ast_stmt *
ast_stmt_brkcont(struct srcpos *sp, int op)
{
	return stmtalloc(op, sp);
}

struct ast_stmt *
ast_stmt_return(struct srcpos *sp, struct ast_expr *x)
{
	struct ast_stmt *stmt;

	stmt = stmtalloc(AST_RETURN, sp);
	stmt->as_exprs[0] = x;
	return stmt;
}

struct ast_stmt *
ast_stmt_expr(struct srcpos *sp, struct ast_expr *x)
{
	struct ast_stmt *stmt;

	stmt = stmtalloc(AST_EXPR, sp);
	stmt->as_exprs[0] = x;
	return stmt;
}

void
ast_case(struct ast_stmt *swtch, struct ast_stmt *castmt, struct ast_expr *x)
{
	struct ast_case *cas;
	union ir_con con;
	struct ir_type *swty;

	swty = tyuconv(swtch->as_exprs[0]->ae_type);
	con = x->ae_con;
	ir_con_cast(&con, x->ae_type, swty);
	SIMPLEQ_FOREACH(cas, &swtch->as_cases, ac_link) {
		if (IR_ISSIGNED(swty)) {
			if (con.ic_icon == cas->ac_con.ic_icon) {
				errp(&castmt->as_sp, "duplicate case in "
				    "switch");
				return;
			}
		} else if (IR_ISUNSIGNED(swty)) {
			if (con.ic_ucon == cas->ac_con.ic_ucon) {
				errp(&castmt->as_sp, "duplicate case in "
				    "switch");
				return;
			}
		}
	}

	cas = mem_alloc(&frontmem, sizeof *cas);
	cas->ac_con = con;
	cas->ac_stmt = castmt;
	SIMPLEQ_INSERT_TAIL(&swtch->as_cases, cas, ac_link);
}

static struct ast_expr *
expralloc(int op, struct srcpos *sp)
{
	struct ast_expr *x;

	x = mem_alloc(&frontmem, sizeof *x);
	SPCPY(&x->ae_sp, sp);
	x->ae_l = x->ae_m = x ->ae_r = NULL;
	x->ae_type = NULL;
	x->ae_elm = NULL;
	x->ae_args = NULL;
	x->ae_decl = NULL;
	x->ae_ident = NULL;
	x->ae_str = NULL;
	x->ae_strlen = 0;
	x->ae_sym = NULL;
	x->ae_flags = 0;
	x->ae_op = op;
	return x;
}

struct ast_expr *
ast_expr_icon(struct srcpos *sp, intmax_t val, struct ir_type *ty)
{
	struct ast_expr *x;

	x = expralloc(AST_ICON, sp);
	x->ae_con.ic_icon = val;
	x->ae_type = ty;
	return x;
}

struct ast_expr *
ast_expr_ucon(struct srcpos *sp, uintmax_t val, struct ir_type *ty)
{
	struct ast_expr *x;

	x = expralloc(AST_ICON, sp);
	x->ae_con.ic_ucon = val;
	x->ae_type = ty;
	return x;
}

struct ast_expr *
ast_expr_fcon(struct srcpos *sp, double val, struct ir_type *ty)
{
	struct ast_expr *x;

	x = expralloc(AST_FCON, sp);
	x->ae_con.ic_fcon = val;
	x->ae_type = ty;
	return x;
}

struct ast_expr *
ast_expr_ident(char *ident)
{
	struct ast_expr *x;

	x = expralloc(AST_IDENT, &cursp);
	x->ae_ident = ident;
	return x;
}

struct ast_expr *
ast_expr_strlit(void)
{
	struct ast_expr *x;

	x = expralloc(AST_STRLIT, &cursp);
	x->ae_type = ir_type_ptr(&cir_char);
	return x;
}

void
ast_expr_strlit_append(struct ast_expr *x, char *strlit)
{
	char *buf;
	size_t newlen;

	/*
	 * Note that strlen() will count the terminating ", so
	 * we don't need to add + 1, since we won't save the ".
	 */
	newlen = strlen(&strlit[1]) + x->ae_strlen;
	buf = xrealloc(x->ae_str, newlen);
	if (x->ae_str == NULL)
		*buf = '\0';
	x->ae_str = buf;
	strlcat(x->ae_str, &strlit[1], newlen);
	x->ae_str[newlen - 1] = 0;
	x->ae_strlen = newlen - 1;
}

void
ast_expr_strlit_finish(struct ast_expr *x)
{
	char *str;
	size_t len;

	str = cstring(x->ae_str, &x->ae_str[x->ae_strlen - 1], NULL, &len);
	free(x->ae_str);
	x->ae_str = str;
	x->ae_strlen = len;
}

struct ast_expr *
ast_expr_subscr(struct srcpos *sp, struct ast_expr *base,
    struct ast_expr *index)
{
	struct ast_expr *x;

	x = expralloc(AST_SUBSCR, sp);
	x->ae_l = base;
	x->ae_r = index;
	x->ae_flags |= (base->ae_flags | index->ae_flags) | AST_SIDEEFF;
	return x;
}

struct ast_expr *
ast_expr_call(struct srcpos *sp, struct ast_expr *x, struct ast_list *list)
{
	struct ast_expr *expr;

	expr = expralloc(AST_CALL, sp);
	expr->ae_l = x;
	expr->ae_args = list;
	expr->ae_flags |= AST_SIDEEFF;
	return expr;
}

struct ast_expr *
ast_expr_souref(struct srcpos *sp, int op, struct ast_expr *x, char *ident)
{
	struct ast_expr *expr;

	expr = expralloc(op, sp);
	expr->ae_l = x;
	expr->ae_ident = ident;
	expr->ae_flags = x->ae_flags & AST_SIDEEFF;
	return expr;
}

struct ast_expr *
ast_expr_incdec(struct srcpos *sp, int op, struct ast_expr *x)
{
	struct ast_expr *expr;

	expr = expralloc(op, sp);
	expr->ae_l = x;
	expr->ae_flags |= AST_SIDEEFF;
	return expr;
}

struct ast_expr *
ast_expr_sizeofx(struct srcpos *sp, struct ast_expr *x)
{
	struct ast_expr *expr;

	expr = expralloc(AST_SIZEOFX, sp);
	expr->ae_l = x;
	return expr;
}

struct ast_expr *
ast_expr_sizeoft(struct srcpos *sp, struct ast_decl *decl)
{
	struct ast_expr *expr;

	expr = expralloc(AST_SIZEOFT, sp);
	expr->ae_decl = decl;
	return expr;
}

struct ast_expr *
ast_expr_unop(struct srcpos *sp, int tok, struct ast_expr *x)
{
	int op;
	struct ast_expr *expr;

	switch (tok) {
	case '&':
		op = AST_ADDROF;
		break;
	case '*':
		op = AST_DEREF;
		break;
	case '+':
		op = AST_UPLUS;
		break;
	case '-':
		op = AST_UMINUS;
		break;
	case '~':
		op = AST_BITFLIP;
		break;
	case '!':
		op = AST_NOT;
		break;
	default:
		fatalx("ast_expr_unop");
	}
	expr = expralloc(op, sp);
	expr->ae_l = x;
	expr->ae_flags = x->ae_flags & AST_SIDEEFF;
	return expr;
}

struct ast_expr *
ast_expr_cast(struct srcpos *sp, struct ast_decl *decl, struct ast_expr *x)
{
	struct ast_expr *expr;

	expr = expralloc(AST_CAST, sp);
	expr->ae_l = x;
	expr->ae_decl = decl;
	expr->ae_flags = x->ae_flags & AST_SIDEEFF;
	return expr;
}

struct ast_expr *
ast_expr_bin(struct srcpos *sp, int tok, struct ast_expr *l,
    struct ast_expr *r)
{
	int op;
	struct ast_expr *expr;

	switch (tok) {
	case '*':
		op = AST_MUL;
		break;
	case '/':
		op = AST_DIV;
		break;
	case '%':
		op = AST_MOD;
		break;
	case '+':
		op = AST_ADD;
		break;
	case '-':
		op = AST_SUB;
		break;
	case TOK_LS:
		op = AST_LS;
		break;
	case TOK_RS:
		op = AST_RS;
		break;
	case '<':
		op = AST_LT;
		break;
	case TOK_LE:
		op = AST_LE;
		break;
	case '>':
		op = AST_GT;
		break;
	case TOK_GE:
		op = AST_GE;
		break;
	case TOK_EQ:
		op = AST_EQ;
		break;
	case TOK_NE:
		op = AST_NE;
		break;
	case '&':
		op = AST_BWAND;
		break;
	case '^':
		op = AST_BWXOR;
		break;
	case '|':
		op = AST_BWOR;
		break;
	case TOK_ANDAND:
		op = AST_ANDAND;
		break;
	case TOK_OROR:
		op = AST_OROR;
		break;
	default:
		fatalx("ast_expr_bin");
	}

	expr = expralloc(op, sp);
	expr->ae_l = l;
	expr->ae_r = r;
	expr->ae_flags = (l->ae_flags | r->ae_flags) & AST_SIDEEFF;
	return expr;
}

struct ast_expr *
ast_expr_cond(struct srcpos *sp, struct ast_expr *l, struct ast_expr *m,
    struct ast_expr *r)
{
	struct ast_expr *expr;

	expr = expralloc(AST_COND, sp);
	expr->ae_l = l;
	expr->ae_m = m;
	expr->ae_r = r;
	expr->ae_flags = l->ae_flags | m->ae_flags | r->ae_flags;
	expr->ae_flags &= AST_SIDEEFF;
	return expr;
}

struct ast_expr *
ast_expr_asg(struct srcpos *sp, int tok, struct ast_expr *l,
    struct ast_expr *r)
{
	int op;
	struct ast_expr *expr;

	switch (tok) {
	case '=':
		op = AST_ASG;
		break;
	case TOK_MULASG:
		op = AST_MULASG;
		break;
	case TOK_DIVASG:
		op = AST_DIVASG;
		break;
	case TOK_MODASG:
		op = AST_MODASG;
		break;
	case TOK_ADDASG:
		op = AST_ADDASG;
		break;
	case TOK_SUBASG:
		op = AST_SUBASG;
		break;
	case TOK_LSASG:
		op = AST_LSASG;
		break;
	case TOK_RSASG:
		op = AST_RSASG;
		break;
	case TOK_ANDASG:
		op = AST_ANDASG;
		break;
	case TOK_XORASG:
		op = AST_XORASG;
		break;
	case TOK_ORASG:
		op = AST_ORASG;
		break;
	default:
		fatalx("ast_expr_asg");
	}

	expr = expralloc(op, sp);
	expr->ae_l = l;
	expr->ae_r = r;
	expr->ae_flags |= AST_SIDEEFF;
	return expr;
}

struct ast_expr *
ast_expr_comma(struct srcpos *sp, struct ast_expr *l, struct ast_expr *r)
{
	struct ast_expr *expr;

	expr = expralloc(AST_COMMA, sp);
	expr->ae_l = l;
	expr->ae_r = r;
	expr->ae_flags |= (l->ae_flags | r->ae_flags) & AST_SIDEEFF;
	return expr;
}
