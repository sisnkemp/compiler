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

#ifndef I386_TARGCONF_H
#define I386_TARGCONF_H

#define IR_PTR_SIZE	4
#define IR_PTR_ALIGN	4

#define TARG_I386
#define TARG_BYTE_ORDER	LITTLE_ENDIAN

#define TARG_EAX	0
#define TARG_EBX	1
#define TARG_ECX	2
#define TARG_EDX	3
#define TARG_ESI	4
#define TARG_EDI	5
#define TARG_EBP	6
#define TARG_ESP	7

#define TARG_AX		8
#define TARG_BX		9
#define TARG_CX		10
#define TARG_DX		11
#define TARG_SI		12
#define TARG_DI		13
#define TARG_BP		14
#define TARG_SP		15

#define TARG_AL		16
#define TARG_AH		17
#define TARG_BL		18
#define TARG_BH		19
#define TARG_CL		20
#define TARG_CH		21
#define TARG_DL		22
#define TARG_DH		23

#define TARG_FP0	24
#define TARG_FP1	25
#define TARG_FP2	26
#define TARG_FP3	27
#define TARG_FP4	28
#define TARG_FP5	29
#define TARG_FP6	30
#define TARG_FP7	31

#define TARG_EAXEBX	32
#define TARG_ESIEDI	33

#define TARG_MAXREG	34
#define TARG_MAXPARREG	0

#define TARG_FRAMEREG	TARG_EBP
#define TARG_FRAMEALIGN	16

#define TARG_FUNCALIGN	4

#endif /* I386_TARGCONF_H */
