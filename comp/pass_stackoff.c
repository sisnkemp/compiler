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

#include "comp/comp.h"
#include "comp/ir.h"
#include "comp/passes.h"

static ssize_t regvaroff(struct ir_func *, struct ir_symq *, ssize_t, int);
static ssize_t incoff(ssize_t, size_t, int);

void
pass_stackoff(struct passinfo *pi)
{
	struct ir_func *fn = pi->p_fn;

	fn->if_framesz = 0;
	fn->if_flags &= ~IR_FUNC_PROTSTACK;
	pass_stackoff_machdep(fn);
}

static ssize_t
regvaroff(struct ir_func *fn, struct ir_symq *symq, ssize_t off, int flags)
{
	size_t nsize, osize;
	struct ir_symbol *sym;

	nsize = fn->if_framesz;
	SIMPLEQ_FOREACH(sym, symq, is_link) {
		if ((flags & STACKOFF_ARRFIRST) && !IR_ISARR(sym->is_type))
			continue;
		if (!(flags & STACKOFF_ARRFIRST) && IR_ISARR(sym->is_type))
			continue;
		osize = nsize;
		nsize = (nsize + sym->is_align - 1) & ~(sym->is_align - 1);
		off = incoff(off, nsize - osize, flags & STACKOFF_NEGOFF);

		if (flags & STACKOFF_NEGOFF)
			off -= sym->is_size;
		sym->is_off = off;
		if (!(flags & STACKOFF_NEGOFF))
			off += sym->is_size;

		nsize += sym->is_size;
	}
	fn->if_framesz = nsize;
	return off;
}

ssize_t
pass_stackoff_var(struct ir_func *fn, ssize_t off, int flags)
{
	int flags2;

	if (!SIMPLEQ_EMPTY(&fn->if_varq))
		fn->if_flags |= IR_FUNC_PROTSTACK;
	if (flags & STACKOFF_ARRFIRST)
		flags2 = flags & ~STACKOFF_ARRFIRST;
	else
		flags2 = flags | STACKOFF_ARRFIRST;

	off = regvaroff(fn, &fn->if_varq, off, flags);
	return regvaroff(fn, &fn->if_varq, off, flags2);
}

ssize_t
pass_stackoff_reg(struct ir_func *fn, ssize_t off, int flags)
{
	return regvaroff(fn, &fn->if_regq, off, flags);
}

ssize_t
pass_stackoff_par(struct ir_func *fn, ssize_t off, int flags)
{
	ssize_t noff = 0;
	struct ir_symbol *sym;

	noff = 0;
	SIMPLEQ_FOREACH(sym, &fn->if_parq, is_link) {
		if (sym->is_op != IR_VARSYM)
			continue;
		off = pass_stackoff_paralign(sym, off);
		sym->is_off = off;
		off = incoff(off, sym->is_size, flags);
	}
	return off;
}

static ssize_t
incoff(ssize_t off, size_t inc, int flags)
{
	if (flags & STACKOFF_NEGOFF)
		return off - inc;
	return off + inc;
}
