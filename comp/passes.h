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

#ifndef COMP_PASSES_H
#define COMP_PASSES_H

struct regpar {
	struct	ir_symbol *r_par;
	struct	ir_symbol *r_tmpvar;
	int	r_addrtaken;
	int	r_reg;
};

struct passinfo {
	struct	ir_func *p_fn;
};

void pass_deadfuncelim(struct passinfo *);

void pass_jmpopt(struct passinfo *);

void pass_uce(struct passinfo *);

void pass_parmfixup(struct passinfo *);
void pass_parmfixup_machdep(struct ir_func *, struct regpar *);

void pass_soufixup(struct passinfo *);

void pass_aliasanalysis(struct passinfo *);

void pass_vartoreg(struct passinfo *);

#define STACKOFF_NEGOFF		1
#define STACKOFF_ARRFIRST	2

void pass_stackoff(struct passinfo *);
void pass_stackoff_machdep(struct ir_func *);
ssize_t pass_stackoff_paralign(struct ir_symbol *, ssize_t);
ssize_t pass_stackoff_var(struct ir_func *, ssize_t, int);
ssize_t pass_stackoff_reg(struct ir_func *, ssize_t, int);
ssize_t pass_stackoff_par(struct ir_func *, ssize_t, int);

void pass_deadcodeelim(struct passinfo *);

void pass_gencode(struct passinfo *);

void pass_ssa(struct passinfo *);
void pass_undo_ssa(struct passinfo *);

void pass_constprop(struct passinfo *);

void pass_deadvarelim(struct passinfo *);

void pass_constfold(struct passinfo *);

void pass_ralloc(struct passinfo *);
void pass_ralloc_precolor(struct ir_symbol *, int);
void pass_ralloc_addedge(struct ir_func *, size_t, size_t);
void pass_ralloc_callargs(struct ir_func *, struct ir_insn *);
int pass_ralloc_retreg(struct ir_type *);

void pass_emit_header(struct passinfo *);
void pass_emit_func(struct passinfo *);
void pass_emit_data(struct passinfo *);
void pass_emit_prologue(struct ir_func *);
void pass_emit_epilogue(struct ir_func *);
void pass_emit_call(struct ir_func *, struct ir_call *);
void pass_emit_ret(struct ir_func *, struct ir_ret *);

#endif /* COMP_PASSES_H */
