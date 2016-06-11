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

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "comp/comp.h"
#include "comp/ir.h"

static void exprdump(FILE *, struct ir_expr *);
static void bracedexprdump(FILE *, struct ir_expr *, struct ir_expr *);

static int precs[] = {
	0, /* IR_ICON */
	0, /* IR_FCON */
	0, /* IR_REG */
	0, /* IR_GVAR */
	0, /* IR_PVAR */
	0, /* IR_LVAR */
	1, /* IR_GADDR */
	1, /* IR_PADDR */
	1, /* IR_LADDR */
	1, /* IR_LOAD */
	1, /* IR_CAST */
	1, /* IR_UMINUS */
	1, /* IR_BITFLIP */
	0, /* IR_SOUREF */
	2, /* IR_MUL */
	2, /* IR_DIV */
	2, /* IR_MOD */
	3, /* IR_ADD */
	3, /* IR_SUB */
	4, /* IR_ARS */
	4, /* IR_LRS */
	4, /* IR_LS */
	5, /* IR_AND */
	5, /* IR_XOR */
	5, /* IR_OR */
};

FILE *
ir_dump_open(const char *name, const char *mode, int dumpno)
{
	char *buf = NULL, *p;
	FILE *fp;

	if ((p = strrchr(infile, '/')) == NULL)
		p = infile;
	else
		p++;
	if (asprintf(&buf, "IRDUMP.%s.%.4d.%s", p, dumpno, name) == -1)
		fatalx("ir_dump_open: asprintf");
	if ((fp = fopen(buf, mode)) == NULL)
		fatal("ir_dump_open: fopen");
	free(buf);
	return fp;
}

static char *
typestr(struct ir_type *t)
{
	static char *tstr[] = {
		"invalid type", "void", "i8", "u8", "i16", "u16", "i32",
		"u32", "i64", "u64", "f32", "f64", "ptr", "arr",
		"struct", "union", "func", "obj"
	};

	t = IR_DEQUAL(t);
	if (t->it_op < 0 || t->it_op >= sizeof tstr / sizeof tstr[0])
		t->it_op = 0;
	return tstr[t->it_op];
}

static void
bracedexprdump(FILE *fp, struct ir_expr *top, struct ir_expr *sub)
{
	int subop = sub->i_op, topop = top->i_op;

	if (topop < IR_ICON || topop > IR_OR)
		fatalx("bracedexprdump: bad op op: %d", topop);
	if (subop < IR_ICON || subop > IR_OR)
		fatalx("bracedexprdump: bad op: %d", subop);

	topop = top->i_op - IR_ICON;
	subop = sub->i_op - IR_ICON;
	if (precs[subop] > precs[topop]) {
		fprintf(fp, "(");
		exprdump(fp, sub);
		fprintf(fp, ")");
		return;
	}
	exprdump(fp, sub);
}

static void
exprdump(FILE *fp, struct ir_expr *x)
{
	struct ir_symbol *sym;

	switch (x->i_op) {
	case IR_ICON:
		if (IR_ISSIGNED(x->ie_type))
			fprintf(fp, "%jd", x->ie_con.ic_icon);
		else
			fprintf(fp, "%ju", x->ie_con.ic_ucon);
		break;
	case IR_FCON:
		fprintf(fp, "%f", x->ie_con.ic_fcon);
		break;
	case IR_REG:
		ir_dump_symbol(fp, x->ie_sym);
		break;
	case IR_GVAR:
		sym = x->ie_sym;
		fprintf(fp, "(g, ");
		if (sym->is_op == IR_CSTRSYM)
			fprintf(fp, ".L%d", sym->is_id);
		else
			fprintf(fp, "%s", sym->is_name);
		fprintf(fp, ", %s)", typestr(x->ie_type));
		break;
	case IR_PVAR:
		fprintf(fp, "(p, %s, %s)", x->ie_sym->is_name,
		    typestr(x->ie_type));
		break;
	case IR_LVAR:
		fprintf(fp, "(l, ");
		ir_dump_symbol(fp, x->ie_sym);
		fprintf(fp, ", %s)", typestr(x->ie_type));
		break;
	case IR_GADDR:
		sym = x->ie_sym;
		fprintf(fp, "&(g, ");
		if (sym->is_op == IR_CSTRSYM)
			fprintf(fp, ".L%d", sym->is_id);
		else
			fprintf(fp, "%s", sym->is_name);
		fprintf(fp, ", %s)", typestr(x->ie_type));
		break;
	case IR_PADDR:
		fprintf(fp, "&(p, %s, %s)", x->ie_sym->is_name,
		    typestr(x->ie_type));
		break;
	case IR_LADDR:
		fprintf(fp, "&(l, ");
		ir_dump_symbol(fp, x->ie_sym);
		fprintf(fp, ", %s)", typestr(x->ie_type));
		break;
	case IR_LOAD:
		fprintf(fp, "*(");
		exprdump(fp, x->ie_l);
		fprintf(fp, ")");
		break;
	case IR_CAST:
		fprintf(fp, "(%s -> %s)", typestr(x->ie_l->ie_type),
		    typestr(x->ie_type));
		bracedexprdump(fp, x, x->ie_l);
		break;
	case IR_UMINUS:
		fprintf(fp, "-");
		bracedexprdump(fp, x, x->ie_l);
		break;
	case IR_BITFLIP:
		fprintf(fp, "~");
		bracedexprdump(fp, x, x->ie_l);
		break;
	case IR_SOUREF:
		bracedexprdump(fp, x, x->ie_l);
		fprintf(fp, "->%s@%zu", x->ie_elm->it_name, x->ie_elm->it_off);
		break;
	case IR_MUL:
		bracedexprdump(fp, x, x->ie_l);
		fprintf(fp, " * ");
		bracedexprdump(fp, x, x->ie_r);
		break;
	case IR_DIV:
		bracedexprdump(fp, x, x->ie_l);
		fprintf(fp, " / ");
		bracedexprdump(fp, x, x->ie_r);
		break;
	case IR_MOD:
		bracedexprdump(fp, x, x->ie_l);
		fprintf(fp, " %% ");
		bracedexprdump(fp, x, x->ie_r);
		break;
	case IR_ADD:
		bracedexprdump(fp, x, x->ie_l);
		fprintf(fp, " + ");
		bracedexprdump(fp, x, x->ie_r);
		break;
	case IR_SUB:
		bracedexprdump(fp, x, x->ie_l);
		fprintf(fp, " - ");
		bracedexprdump(fp, x, x->ie_r);
		break;
	case IR_ARS:
		bracedexprdump(fp, x, x->ie_l);
		fprintf(fp, " a>> ");
		bracedexprdump(fp, x, x->ie_r);
		break;
	case IR_LRS:
		bracedexprdump(fp, x, x->ie_l);
		fprintf(fp, " l>> ");
		bracedexprdump(fp, x, x->ie_r);
		break;
	case IR_LS:
		bracedexprdump(fp, x, x->ie_l);
		fprintf(fp, " << ");
		bracedexprdump(fp, x, x->ie_r);
		break;
	case IR_AND:
		bracedexprdump(fp, x, x->ie_l);
		fprintf(fp, " & ");
		bracedexprdump(fp, x, x->ie_r);
		break;
	case IR_XOR:
		bracedexprdump(fp, x, x->ie_l);
		fprintf(fp, " ^ ");
		bracedexprdump(fp, x, x->ie_r);
		break;
	case IR_OR:
		bracedexprdump(fp, x, x->ie_l);
		fprintf(fp, " | ");
		bracedexprdump(fp, x, x->ie_r);
		break;
	default:
		fatalx("exprdump: bad op: %x", x->i_op);
	}
}

void
ir_dump_symbol(FILE *fp, struct ir_symbol *sym)
{
	if (sym->is_name != NULL ||
	    (sym->is_op == IR_REGSYM && sym->is_id < REG_NREGS)) {
		fprintf(fp, "%s", sym->is_name);
		if (sym->is_id >= REG_NREGS)
			fprintf(fp, "_%d", sym->is_id);
	} else if (sym->is_op == IR_REGSYM)
		fprintf(fp, "%%r%s_%d", typestr(sym->is_type), sym->is_id);
	else
		fprintf(fp, "anon%s_%d", typestr(sym->is_type), sym->is_id);
}

void
ir_dump_insn(FILE *fp, struct ir_insn *insn)
{
	struct ir_branch *ib;
	struct ir_call *ic;
	struct ir_expr *x;
	struct ir_lbl *il;
	struct ir_ret *ir;
	struct ir_stasg *is;
	struct ir_phiarg *arg;

	switch (insn->i_op) {
	case IR_ASG:
		fprintf(fp, "\t");
		is = (struct ir_stasg *)insn;
		exprdump(fp, is->is_l);
		fprintf(fp, " = ");
		exprdump(fp, is->is_r);
		break;
	case IR_ST:
		fprintf(fp, "\t");
		is = (struct ir_stasg *)insn;
		exprdump(fp, is->is_l);
		fprintf(fp, " <- ");
		exprdump(fp, is->is_r);
		break;
	case IR_PHI:
		fprintf(fp, "\t");
		ir_dump_symbol(fp, insn->ip_sym);
		fprintf(fp, " = phi(");
		SIMPLEQ_FOREACH(arg, &insn->ip_args, ip_link) {
			ir_dump_symbol(fp, arg->ip_arg);
			if (SIMPLEQ_NEXT(arg, ip_link) != NULL)
				fprintf(fp, ", ");
		}
		fprintf(fp, ")");
		break;
	case IR_B:
		ib = (struct ir_branch *)insn;
		fprintf(fp, "\tb L%d", ib->ib_lbl->il_id);
		break;
	case IR_CALL:
		ic = (struct ir_call *)insn;
		fprintf(fp, "\t");
		if (ic->ic_ret != NULL) {
			exprdump(fp, ic->ic_ret);
			fprintf(fp, " = ");
		}
		fprintf(fp, "call ");
		ir_dump_symbol(fp, ic->ic_fn);
		fprintf(fp, "(");
		SIMPLEQ_FOREACH(x, &ic->ic_argq, ie_link) {
			exprdump(fp, x);
			if (SIMPLEQ_NEXT(x, ie_link) != NULL)
				fprintf(fp, ", ");
		}
		fprintf(fp, ")");
		break;
	case IR_RET:
		ir = (struct ir_ret *)insn;
		if (ir->ir_deadret)
			fprintf(fp, "\tdead ret");
		else
			fprintf(fp, "\tret");
		if (ir->ir_retexpr != NULL) {
			fprintf(fp, " ");
			exprdump(fp, ir->ir_retexpr);
		}
		break;
	case IR_LBL:
		il = (struct ir_lbl *)insn;
		fprintf(fp, "L%d:", il->il_id);
		break;
	case IR_BEQ:
		ib = (struct ir_branch *)insn;
		fprintf(fp, "\tbeq ");
		exprdump(fp, ib->ib_l);
		fprintf(fp, " == ");
		exprdump(fp, ib->ib_r);
		fprintf(fp, ", L%d", ib->ib_lbl->il_id);
		break;
	case IR_BNE:
		ib = (struct ir_branch *)insn;
		fprintf(fp, "\tbne ");
		exprdump(fp, ib->ib_l);
		fprintf(fp, " != ");
		exprdump(fp, ib->ib_r);
		fprintf(fp, ", L%d", ib->ib_lbl->il_id);
		break;
	case IR_BLT:
		ib = (struct ir_branch *)insn;
		fprintf(fp, "\tblt ");
		exprdump(fp, ib->ib_l);
		fprintf(fp, " < ");
		exprdump(fp, ib->ib_r);
		fprintf(fp, ", L%d", ib->ib_lbl->il_id);
		break;
	case IR_BLE:
		ib = (struct ir_branch *)insn;
		fprintf(fp, "\tble ");
		exprdump(fp, ib->ib_l);
		fprintf(fp, " <= ");
		exprdump(fp, ib->ib_r);
		fprintf(fp, ", L%d", ib->ib_lbl->il_id);
		break;
	case IR_BGT:
		ib = (struct ir_branch *)insn;
		fprintf(fp, "\tbgt ");
		exprdump(fp, ib->ib_l);
		fprintf(fp, " > ");
		exprdump(fp, ib->ib_r);
		fprintf(fp, ", L%d", ib->ib_lbl->il_id);
		break;
	case IR_BGE:
		ib = (struct ir_branch *)insn;
		fprintf(fp, "\tbge ");
		exprdump(fp, ib->ib_l);
		fprintf(fp, " >= ");
		exprdump(fp, ib->ib_r);
		fprintf(fp, ", L%d", ib->ib_lbl->il_id);
		break;
	default:
		fatalx("ir_dump_insn: bad op: %d", insn->i_op);
	}
	fprintf(fp, "\n");
}

void
ir_dump_prog(FILE *fp, struct ir_prog *prog)
{
	struct ir_func *fn;

	ir_dump_globals(fp, prog);
	SIMPLEQ_FOREACH(fn, &prog->ip_funq, if_link)
		ir_dump_func(fp, fn);
}

void
ir_dump_globals(FILE *fp, struct ir_prog *prog)
{
	struct ir_symbol *sym;

	SIMPLEQ_FOREACH(sym, &prog->ip_symq, is_link) {
		fprintf(fp, "%s %s, size %zu, align %zu\n",
		    typestr(sym->is_type), sym->is_name, sym->is_size,
		    sym->is_align);
	}
}

void
ir_dump_func(FILE *fp, struct ir_func *fn)
{
	struct ir_symbol *sym;
	struct ir_insn *insn;

	fprintf(fp, "\n");
	fprintf(fp, "%s %s(", typestr(fn->if_sym->is_type),
	    fn->if_sym->is_name);
	SIMPLEQ_FOREACH(sym, &fn->if_parq, is_link) {
		fprintf(fp, "%s %s_%d", typestr(sym->is_type),
		    sym->is_name, sym->is_id);
		if (SIMPLEQ_NEXT(sym, is_link) != NULL)
			fprintf(fp, ", ");
	}
	fprintf(fp, ")\n{\n");
	SIMPLEQ_FOREACH(sym, &fn->if_varq, is_link) {
		if (sym->is_name != NULL)
			fprintf(fp, "\t%s %s_%d@%zd\n",
			    typestr(sym->is_type), sym->is_name,
			    sym->is_id, sym->is_off);
		else
			fprintf(fp, "\t%s anon%s_%d@%zd\n",
			    typestr(sym->is_type),
			    typestr(sym->is_type), sym->is_id, sym->is_off);
	}
	SIMPLEQ_FOREACH(sym, &fn->if_regq, is_link) {
		fprintf(fp, "\t%s %%r%s_%d@%zd\n",
		    typestr(sym->is_type), typestr(sym->is_type),
		    sym->is_id, sym->is_off);
	}
	fprintf(fp, "\n");
	TAILQ_FOREACH(insn, &fn->if_iq, ii_link)
		ir_dump_insn(fp, insn);
	fprintf(fp, "}\n");
}
