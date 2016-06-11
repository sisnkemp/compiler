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

/* TODO: Handle structures as function call arguments. */
#include <sys/types.h>
#include <sys/queue.h>

#include "comp/comp.h"
#include "comp/ir.h"
#include "comp/passes.h"

static void soufixup_expr(struct ir_func *, struct ir_expr **,
    struct ir_insn *);
static struct ir_expr *soufixup_elm(struct ir_func *, struct ir_expr *,
    struct ir_insn *);
static struct ir_expr *soufixup_packelm(struct ir_func *, struct ir_expr *,
    struct ir_insn *);
static struct ir_expr *soufixup_fld(struct ir_func *, struct ir_expr *,
    struct ir_insn *);
static struct ir_expr *soufixup_packfld(struct ir_func *, struct ir_expr *,
    struct ir_insn *);
static void soufixup_fldasg(struct ir_func *, struct ir_stasg *);

void
pass_soufixup(struct passinfo *pi)
{
	struct ir_func *fn = pi->p_fn;
	struct ir_insn *insn;
	struct ir_stasg *asg;
	struct ir_branch *b;
	struct ir_call *call;
	struct ir_ret *ret;

	TAILQ_FOREACH(insn, &fn->if_iq, ii_link) {
		if (insn->i_op == IR_B || insn->i_op == IR_LBL)
			continue;
		switch (insn->i_op) {
		case IR_ASG:
		case IR_ST:
			asg = (struct ir_stasg *)insn;
			if (asg->is_l->i_op == IR_SOUREF &&
			    asg->is_l->ie_elm->it_fldsize > 0) {
				soufixup_fldasg(fn, asg);
			}
			soufixup_expr(fn, &asg->is_l, insn);
			soufixup_expr(fn, &asg->is_r, insn);
			break;
		case IR_BEQ:
		case IR_BNE:
		case IR_BLT:
		case IR_BLE:
		case IR_BGT:
		case IR_BGE:
			b = (struct ir_branch *)insn;
			soufixup_expr(fn, &b->ib_l, insn);
			soufixup_expr(fn, &b->ib_r, insn);
			break;
		case IR_CALL:
			call = (struct ir_call *)insn;
			if (call->ic_ret != NULL)
				soufixup_expr(fn, &call->ic_ret, insn);
			break;
		case IR_RET:
			ret = (struct ir_ret *)insn;
			if (ret->ir_retexpr != NULL)
				soufixup_expr(fn, &ret->ir_retexpr, insn);
			break;
		default:
			fatalx("soufixup: bad op: %x", insn->i_op);
		}
	}
}

static void
soufixup_expr(struct ir_func *fn, struct ir_expr **xp, struct ir_insn *insn)
{
	struct ir_expr *x;
	struct ir_type *ty;
	struct ir_typelm *elm;

again:
	x = *xp;
	if (IR_ISLEAFEXPR(x))
		return;
	if (IR_ISBINEXPR(x)) {
		soufixup_expr(fn, &x->ie_l, insn);
		xp = &x->ie_r;
		goto again;
	}
	if (x->i_op == IR_LOAD && x->ie_l->i_op == IR_SOUREF) {
		if (x->ie_l->ie_elm->it_fldsize < 0) {
			xp = &x->ie_l;
			goto again;
		}
		x = x->ie_l;
	} else if (IR_ISUNEXPR(x) && x->i_op != IR_SOUREF) {
		xp = &x->ie_l;
		goto again;
	}

	ty = x->ie_sou;
	elm = x->ie_elm;
	soufixup_expr(fn, &x->ie_l, insn);
	if (elm->it_fldsize < 0) {
		if (ty->it_flags & IR_PACKED)
			*xp = soufixup_packelm(fn, x, insn);
		else
			*xp = soufixup_elm(fn, x, insn);
	} else if (ty->it_flags & IR_PACKED)
		*xp = soufixup_packfld(fn, x, insn);
	else
		*xp = soufixup_fld(fn, x, insn);
}

static struct ir_expr *
soufixup_elm(struct ir_func *fn, struct ir_expr *x, struct ir_insn *insn)
{
	union ir_con con;
	struct ir_expr *off;

	if (x->ie_elm->it_off == 0)
		return x->ie_l;
	con.ic_ucon = x->ie_elm->it_off / 8;
	off = ir_con(IR_ICON, con, &ir_addrcon);
	return ir_bin(IR_ADD, x->ie_l, off, x->ie_l->ie_type);
}

static struct ir_expr *
soufixup_packelm(struct ir_func *fn, struct ir_expr *x, struct ir_insn *insn)
{
	fatalx("soufixup_packelm");
	return NULL;
}

static struct ir_expr *
soufixup_fld(struct ir_func *fn, struct ir_expr *x, struct ir_insn *insn)
{
	fatalx("soufixup_fld");
	return NULL;
}

static struct ir_expr *
soufixup_packfld(struct ir_func *fn, struct ir_expr *x, struct ir_insn *insn)
{
	fatalx("soufixup_packfld");
	return NULL;
}

static void
soufixup_fldasg(struct ir_func *fn, struct ir_stasg *asg)
{
	fatalx("soufixup_fldasg not yet");
}
