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

#ifndef POWERPC_TARGCONF_H
#define POWERPC_TARGCONF_H

#include "reg.h"

#define IR_PTR_SIZE	4
#define IR_PTR_ALIGN	4

#define TARG_POWERPC
#define TARG_BYTE_ORDER	BIG_ENDIAN

#define TARG_FRAMEREG	REG_R1

#define TARG_FUNCALIGN	4

#define TARG_UIMM_MAX	65535
#define TARG_SIMM_MIN	-32768
#define TARG_SIMM_MAX	32765

#define IR_FUNC_MACHDEP				\
	struct	ir_symbol *ifm_vasaves;		\
	struct	ir_symbol *ifm_vastack;		\
	size_t	ifm_guardoff;			\
	size_t	ifm_r31off;			\
	short	ifm_firstgpr;			\
	short	ifm_firstfpr;			\
	short	ifm_saver31;

struct ir_func;
void ir_func_va_prepare(struct ir_func *);

extern int8_t pairtogpr[][2];
 
#endif /* POWERPC_TARGCONF_H */
