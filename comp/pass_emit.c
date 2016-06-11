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
#include <string.h>

#include "comp/comp.h"
#include "comp/ir.h"
#include "comp/passes.h"

static void emit_align(size_t);
static void doemit_expr(struct ir_expr *);
static void emit_expr(struct ir_expr *, size_t);
static void emit_field(uintmax_t, size_t);
static void emit_init(struct ir_init *);
static void emit_func(struct ir_func *);

void
pass_emit_header(struct passinfo *pi)
{
	char *p;

	if ((p = strrchr(infile, '/')) == NULL)
		p = infile;
	else
		p++;

	emitf("\t.file\t\"%s\"\n", p);
	if (!SIMPLEQ_EMPTY(&irprog->ip_funq))
		emits("\t.text\n");
}

void
pass_emit_func(struct passinfo *pi)
{
	struct ir_func *fn = pi->p_fn;

	emits("\n");
	if (fn->if_flags & IR_FUNC_PROTSTACK) {
		emits("\t.section .rodata\n");
		emitf(".L%d:\n", fn->if_sym->is_id);
		emitf("\t.asciz\t\"%s\"\n", fn->if_sym->is_name);
	}
	emits("\t.text\n");
	emit_align(TARG_FUNCALIGN);
	if (fn->if_sym->is_flags & IR_SYM_GLOBL)
		emitf("\t.globl\t%s\n", fn->if_sym->is_name);
	emitf("\t.type\t%s, @function\n", fn->if_sym->is_name);
	emitf("%s:\n", fn->if_sym->is_name);
	pass_emit_prologue(fn);
	emit_func(fn);
	pass_emit_epilogue(fn);
	emitf("\t.size\t%s, .-%s\n",
	    fn->if_sym->is_name, fn->if_sym->is_name);
}

void
pass_emit_data(struct passinfo *pi)
{
	struct ir_init *init;
	struct ir_symbol *sym;
	struct ir_syminit *symini;

	if (!SIMPLEQ_EMPTY(&irprog->ip_roinitq) ||
	    !SIMPLEQ_EMPTY(&irprog->ip_floatq) ||
	    !SIMPLEQ_EMPTY(&irprog->ip_cstrq))
		emits("\n\t.section .rodata\n");
	SIMPLEQ_FOREACH(init, &irprog->ip_roinitq, ii_link)
		emit_init(init);
	SIMPLEQ_FOREACH(symini, &irprog->ip_floatq, is_link) {
		union {
			double	f64;
			uint32_t arr[2];
		} u;

		if (!IR_ISF64(symini->is_sym->is_type))
			fatalx("pass_emit: bad float type: %d",
			    symini->is_sym->is_type);

		u.f64 = symini->is_val.ic_fcon;
		emitf(".L%d:\n", symini->is_sym->is_id);
#if _BYTE_ORDER == TARG_BYTE_ORDER
		emitf("\t.long\t%u\n", u.arr[0]);
		emitf("\t.long\t%u\n", u.arr[1]);
#else
		emitf("\t.long\t%u\n", u.arr[1]);
		emitf("\t.long\t%u\n", u.arr[0]);
#endif
	}

	SIMPLEQ_FOREACH(sym, &irprog->ip_cstrq, is_link) {
		emit_align(IR_PTR_ALIGN);
		emitf(".L%d:\t.asciz ", sym->is_id);
		emitcstring(stdout, sym->is_name, sym->is_size);
		emits("\n");
	}

	if (!SIMPLEQ_EMPTY(&irprog->ip_initq))
		emits("\n\t.data\n");
	SIMPLEQ_FOREACH(init, &irprog->ip_initq, ii_link)
		emit_init(init);

	if (!SIMPLEQ_EMPTY(&irprog->ip_symq))
		emits("\n\t.section .bss\n");
	SIMPLEQ_FOREACH(sym, &irprog->ip_symq, is_link) {
		if (sym->is_op != IR_VARSYM)
			continue;
		emitf(".comm %s, %zu\n", sym->is_name, sym->is_size);
	}
}

static void
emit_align(size_t size)
{
	emitf("\t.balign\t%zu\n", size);
}

static void
doemit_expr(struct ir_expr *x)
{
	uintmax_t mask;
	union {
		float	f32;
		double	f64;
		uint32_t arr[2];
	} u;

	static char *ops[] = {
		"*", "/", "%", "+", "-", ">>", ">>", "<<", "&", "^", "|"
	};

	if (IR_ISBINEXPR(x)) {
		doemit_expr(x->ie_l);
		emitf(" %s ", ops[x->i_op - IR_MUL]);	
		doemit_expr(x->ie_r);
	}

	mask = ((uintmax_t)1 << (x->ie_type->it_size * 8)) - 1;
	switch (x->i_op) {
	case IR_ICON:
		if (IR_ISSIGNED(x->ie_type))
			emitf("%jd", x->ie_con.ic_icon & mask);
		else
			emitf("%jd", x->ie_con.ic_ucon & mask);
		break;
	case IR_FCON:
		if (IR_ISF64(x->ie_type)) {
			u.f64 = x->ie_con.ic_fcon;
#if _BYTE_ORDER == TARG_BYTE_ORDER
			emitf("\t%u", u.arr[0]);
			emitf("\t%u", u.arr[1]);
#else
			emitf("\t%u", u.arr[1]);
			emitf("\t%u", u.arr[0]);
#endif
		} else {
			u.f32 = x->ie_con.ic_fcon;
			emitf("0x%x\n", u.arr[0]);
		}
		break;
	case IR_GADDR:
		if (x->ie_sym->is_name == NULL ||
	     	    x->ie_sym->is_op == IR_CSTRSYM)
			emitf(".L%d", x->ie_sym->is_id);
		else
			emitf("%s", x->ie_sym->is_name);
		break;
	case IR_CAST:
		doemit_expr(x->ie_l);
		break;
	case IR_UMINUS:
	case IR_BITFLIP:
		emitf(x->i_op == IR_UMINUS ? "-" : "~");
		doemit_expr(x->ie_l);
		break;
	case IR_SOUREF:
		doemit_expr(x->ie_l);
		emitf(" + %zu", x->ie_elm->it_off / 8);
		break;
	default:
		fatalx("doemit_expr: bad op: 0x%x", x->i_op);
	}
}

static void
emit_expr(struct ir_expr *x, size_t off)
{
	switch (x->ie_type->it_size) {
	case 1:
		emits("\t.byte\t");
		break;
	case 2:
		if (off & 1)
			emits("\t.2byte\t");
		else
			emits("\t.short\t");
		break;
	case 4:
		if (off & 3)
			emits("\t.4byte\t");
		else
			emits("\t.long\t");
		break;
	case 8:
		if (off & 7)
			emits("\t.8byte\t");
		else
			emits("\t.quad\t");
		break;
	default:
		fatalx("emit_expr");
	}
	doemit_expr(x);
	emits("\n");
}

static void
emit_field(uintmax_t val, size_t size)
{
	size_t byte, i, mask, roundsz;

	roundsz = (size + 7) & ~7;

#if TARG_BYTE_ORDER == BIG_ENDIAN
	val = val << (roundsz - size);
	mask = 0xff << (roundsz - 8);
#else
	mask = 0xff;
#endif

	emits("\t.byte\t");
	for (i = 0; i < roundsz; i += 8) {
#if TARG_BYTE_ORDER == BIG_ENDIAN
		byte = (val & mask) >> (roundsz - i - 8);
		mask >>= 8;
#else
		byte = (val & mask) >> i;
		mask <<= 8;
#endif
		emitf("0x%zx", byte);
		if (i + 8 != roundsz)
			emits(", ");
	}
	emits("\n");
}

static void
emit_init(struct ir_init *init)
{
	size_t len;
	uintmax_t val, tmp;
	size_t bfldoff = 0, lastoff, size;
	struct ir_initelm *elm, *nelm;
	struct ir_symbol *sym = init->ii_sym;

	emit_align(sym->is_align);
	if (sym->is_name == NULL) {
		emitf("\t.type\t.L%d, @object\n", sym->is_id);
		emitf("\t.size\t.L%d, %zu\n", sym->is_id, sym->is_size);
		emitf(".L%d:\n", sym->is_id);
	} else {
		if (sym->is_flags & IR_SYM_GLOBL)
			emitf("\t.globl\t%s\n", sym->is_name);
		emitf("\t.type\t%s, @object\n", sym->is_name);
		emitf("\t.size\t%s, %zu\n", sym->is_name, sym->is_size);
		emitf("%s:\n", sym->is_name);
	}

	lastoff = 0;
	val = 0;
	size = 0;
	SIMPLEQ_FOREACH(elm, &init->ii_elms, ii_link) {
		nelm = SIMPLEQ_NEXT(elm, ii_link);
		if (!(elm->ii_bitoff & 3) && size) {
			emit_field(val, size);
			lastoff = elm->ii_bitoff;
			val = 0;
			size = 0;
		} else if (elm->ii_bitoff & 3 ||
		    (nelm != NULL && (nelm->ii_bitoff & 3))) {
			if (elm->ii_bitsize == 0)
				fatalx("emit_init: aligning bitfield takes "
				    "part in initialization");
			if (size == 0)
				bfldoff = elm->ii_bitoff;
			if (IR_ISSIGNED(elm->ii_expr->ie_type))
				tmp = elm->ii_expr->ie_con.ic_icon;
			else if (IR_ISUNSIGNED(elm->ii_expr->ie_type))
				tmp = elm->ii_expr->ie_con.ic_ucon;
			else
				fatalx("emit_init: bad field type");
			val &= (1 << size) - 1;
#if TARG_BYTE_ORDER == LITTLE_ENDIAN
			val = val | (tmp << size);
#else
			val = (val << elm->ii_bitsize) | tmp;
#endif
			size += elm->ii_bitsize;
			continue;
		}

		if (size)
			fatalx("emit_init: infld");

		if (lastoff > elm->ii_bitoff)
			fatalx("padding from %zu to %zu",
			    lastoff, elm->ii_bitoff);
		if (lastoff < elm->ii_bitoff)
			emitf("\t.zero\t%zu\n",
			    (elm->ii_bitoff - lastoff) / 8);

		if (elm->ii_op == IR_INIT_CSTR || elm->ii_op == IR_INIT_STR) {
			if (elm->ii_op == IR_INIT_STR) {
				emits("\t.ascii ");
				len = elm->ii_len + 1;
			} else {
				emits("\t.asciz ");
				len = elm->ii_len;
			}
			emitcstring(stdout, elm->ii_cstr, len);
			lastoff += elm->ii_len * 8;
			emits("\n");
			continue;
		}
		emit_expr(elm->ii_expr, elm->ii_bitoff / 8);
		lastoff += elm->ii_expr->ie_type->it_size * 8;
	}
	if (size) {
		lastoff = (bfldoff + size + 7) & ~7;
		emit_field(val, size);
	}
	lastoff /= 8;
	if (lastoff > sym->is_size)
		fatalx("padding from %zu to %zu",
		    lastoff, sym->is_size);
	if (lastoff < sym->is_size)
		emitf("\t.zero\t%zu\n", sym->is_size - lastoff);
	emits("\n");
}

static void
emit_func(struct ir_func *fn)
{
	struct ir_expr *l, *r;
	struct ir_insn *insn;
	struct ir_lbl *lbl;

	TAILQ_FOREACH(insn, &fn->if_iq, ii_link) {
		switch (insn->i_op) {
		case IR_PHI:
			fatalx("phi function in emit_func");
			continue;
		case IR_CALL:
			pass_emit_call(fn, (struct ir_call *)insn);
			break;
		case IR_RET:
			pass_emit_ret(fn, (struct ir_ret *)insn);
			break;
		case IR_LBL:
			lbl = (struct ir_lbl *)insn;
			emitf(".L%d:\n", lbl->il_id);
			break;
		case IR_ASG:
			l = insn->is_l;
			r = insn->is_r;
			if (l->i_op == IR_REG && r->i_op == IR_REG &&
			    l->ie_sym == r->ie_sym)
				continue;
			ir_emit((struct ir *)insn, (struct ir *)l);
			break;
		default:
			if (insn->i_emit == NULL) {
				ir_dump_insn(stderr, insn);
				fatalx("no emit string for instruction");
			}
			ir_emit((struct ir *)insn, NULL);
		}
	}
}
