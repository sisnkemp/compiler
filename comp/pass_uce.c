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

/*
 * Unreachable code elimination.
 */

#include <sys/types.h>
#include <sys/queue.h>

#include "comp/comp.h"
#include "comp/ir.h"
#include "comp/passes.h"

void
pass_uce(struct passinfo *pi)
{
	struct cfa_bb *bb;
	struct ir_func *fn = pi->p_fn;
	struct ir_insn *insn, *last;

	cfa_buildcfg(fn);
	cfa_cfgsort(fn, CFA_CFGSORT_ASC);
	SIMPLEQ_FOREACH(bb, &fn->if_cfadata->c_bbqh, cb_glolink) {
		if (bb->cb_dfsno != -1)
			continue;
		if ((last = bb->cb_last) != NULL)
			last = TAILQ_NEXT(last, ii_link);
		for (insn = bb->cb_first; insn != last;
		    insn = TAILQ_NEXT(insn, ii_link))
			ir_delete_insn(fn, insn);
	}
}
