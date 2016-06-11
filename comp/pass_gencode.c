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

#include "comp/cgi.h"
#include "comp/comp.h"
#include "comp/ir.h"
#include "comp/passes.h"

void
pass_gencode(struct passinfo *pi)
{
	int i;
	struct ir_func *fn = pi->p_fn;
	struct ir_insn *insn;
	CGI_CTX ctx;

	cg_start();
	ctx.cc_fn = fn;
	i = 0;

	ir_func_setflags(fn, IR_FUNC_LEAF);
	do {
		ctx.cc_changes = 0;
		TAILQ_FOREACH(insn, &fn->if_iq, ii_link) {
			if (insn->i_op == IR_CALL) {
				fn->if_flags &= ~IR_FUNC_LEAF;
				continue;
			}
			if (insn->i_op == IR_RET || insn->i_op == IR_LBL ||
			    insn->i_op == IR_PHI)
				continue;
			if (i && insn->ii_cgskip)
				continue;

			cg((struct ir *)insn);
			insn->ii_cgskip = cg_action(&ctx, (struct ir *)insn);
			cgi_recycle();
		}
		if (i++ > 8)
			fatalx("pass_gencode still not done after 8 "
			    "iterations in %s", fn->if_sym->is_name);
	} while (ctx.cc_changes);
	cg_finish();
}
