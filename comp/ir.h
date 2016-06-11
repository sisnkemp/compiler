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

#ifndef COMP_IR_H
#define COMP_IR_H

#include <stdint.h>

#include "targconf.h"

#define IR_ASG	0x1
#define IR_ST	0x2
#define IR_B	0x3
#define IR_BEQ	0x4
#define IR_BNE	0x5
#define IR_BLT	0x6
#define IR_BLE	0x7
#define IR_BGT	0x8
#define IR_BGE	0x9
#define IR_CALL	0xa
#define IR_RET	0xb
#define IR_LBL	0xc
#define IR_PHI	0xd

#define IR_ISINSN(ir)	((ir)->i_op >= IR_ASG && (ir)->i_op <= IR_LBL)
#define IR_ISBRANCH(ir)	((ir)->i_op >= IR_B && (ir)->i_op <= IR_BGE)

/*
 * Leaves of an expression tree.
 */
#define IR_ICON		0xe
#define IR_FCON		0xf
#define IR_REG		0x10
#define IR_GVAR		0x11
#define IR_PVAR		0x12
#define IR_LVAR		0x13
#define IR_GADDR	0x14
#define IR_PADDR	0x15
#define IR_LADDR	0x16

/*
 * Unary expressions.
 */
#define IR_LOAD		0x17
#define IR_CAST		0x18
#define IR_UMINUS	0x19
#define IR_BITFLIP	0x1a
#define IR_SOUREF	0x1b

/*
 * Binary expressions.
 */
#define IR_MUL		0x1c
#define IR_DIV		0x1d
#define IR_MOD		0x1e
#define IR_ADD		0x1f
#define IR_SUB		0x20
#define IR_ARS		0x21
#define IR_LRS		0x22
#define IR_LS		0x23
#define IR_AND		0x24
#define IR_XOR		0x25
#define IR_OR		0x26

extern int8_t ir_nkids[];

#define IR_ISEXPR(ir)	((ir)->i_op >= IR_ICON && (ir)->i_op <= IR_OR)
#define IR_ISLEAFEXPR(ir)					\
	((ir)->i_op >= IR_ICON && (ir)->i_op <= IR_LADDR)
#define IR_ISUNEXPR(ir)						\
	((ir)->i_op >= IR_LOAD && (ir)->i_op <= IR_SOUREF)
#define IR_ISBINEXPR(ir)					\
	((ir)->i_op >= IR_MUL && (ir)->i_op <= IR_OR)

#define IR_VARSYM	1
#define IR_REGSYM	2
#define IR_FUNSYM	3
#define IR_CSTRSYM	4

#define IR_FLATINIT	1
#define IR_RECINIT	2

/*
 * The order of the I8 to U64 is important!
 */
#define IR_VOID		1
#define IR_I8		2
#define IR_U8		3
#define IR_I16		4
#define IR_U16		5
#define IR_I32		6
#define IR_U32		7
#define IR_I64		8
#define IR_U64		9
#define IR_F32		10
#define IR_F64		11
#define IR_PTR		12
#define IR_ARR		13
#define IR_STRUCT	14
#define IR_UNION	15
#define IR_FUNTY	16
#define IR_OBJ		17
#define IR_BOOL		18
#define IR_CONST	32
#define IR_VOLAT	64
#define IR_TYQUALS	(IR_CONST | IR_VOLAT)

/* XXX: Some of them don't really belong here. */
#define IR_COMPLETE	1
#define IR_ELLIPSIS	2
#define IR_PACKED	4
#define IR_KRFUNC	8
#define IR_FUNDEF	16

#define IR_ISQUAL(type)		((type)->it_op & IR_TYQUALS)
#define IR_DEQUAL(type)		(IR_ISQUAL(type) ? (type)->it_base : (type))
#define IR_DQOP(type, op)					\
	(IR_ISQUAL(type) && (type)->it_base->it_op == (op))

#define IR_ISTYOP(type, op)	((type)->it_op == (op) || IR_DQOP(type, op))

#define IR_DQTYPE(type, need)				\
	(IR_ISQUAL(type) && (type)->it_base == (need))

#define IR_ISVOID(type)	((type) == &ir_void || IR_DQTYPE(type, &ir_void))
#define IR_ISI8(type)	((type) == &ir_i8 || IR_DQTYPE(type, &ir_i8))
#define IR_ISU8(type)	((type) == &ir_u8 || IR_DQTYPE(type, &ir_u8))
#define IR_ISI16(type)	((type) == &ir_i16 || IR_DQTYPE(type, &ir_i16))
#define IR_ISU16(type)	((type) == &ir_u16 || IR_DQTYPE(type, &ir_u16))
#define IR_ISI32(type)	((type) == &ir_i32 || IR_DQTYPE(type, &ir_i32))
#define IR_ISU32(type)	((type) == &ir_u32 || IR_DQTYPE(type, &ir_u32))
#define IR_ISI64(type)	((type) == &ir_i64 || IR_DQTYPE(type, &ir_i64))
#define IR_ISU64(type)	((type) == &ir_u64 || IR_DQTYPE(type, &ir_u64))
#define IR_ISF32(type)	((type) == &ir_f32 || IR_DQTYPE(type, &ir_f32))
#define IR_ISF64(type)	((type) == &ir_f64 || IR_DQTYPE(type, &ir_f64))
#define IR_ISPTR(type)	IR_ISTYOP(type, IR_PTR)
#define IR_ISARR(type)	IR_ISTYOP(type, IR_ARR)
#define IR_ISSTRUCT(type)	IR_ISTYOP(type, IR_STRUCT)
#define IR_ISUNION(type)	IR_ISTYOP(type, IR_UNION)
#define IR_ISSOU(type)		(IR_ISSTRUCT(type) || IR_ISUNION(type))
#define IR_ISFUNTY(type)	IR_ISTYOP(type, IR_FUNTY)
#define IR_ISOBJ(type)	((type) == &ir_obj || IR_DQTYPE(type, &ir_obj))
#define IR_ISBOOL(type)	((type) == &ir_bool || IR_DQTYPE(type, &ir_bool))
#define IR_ISCONST(type)	((type)->it_op & IR_CONST)
#define IR_ISVOLAT(type)	((type)->it_op & IR_VOLAT)
#define IR_ISCOMPLETE(type)	((type)->it_flags & IR_COMPLETE)

#define IR_ISINTEGER(type)						\
	(IR_DEQUAL(type)->it_op >= IR_I8 && IR_DEQUAL(type)->it_op <= IR_U64)

#define IR_ISSIGNED(type)					\
	(IR_ISINTEGER(type) && !(IR_DEQUAL(type)->it_op & 1))
#define IR_ISUNSIGNED(type)					\
	(IR_ISINTEGER(type) && (IR_DEQUAL(type)->it_op & 1))

#define IR_ISFLOATING(type)						\
	(IR_DEQUAL(type)->it_op == IR_F32 || IR_DEQUAL(type)->it_op == IR_F64)

#define IR_ISBYTE(type)	(IR_ISI8(type) || IR_ISU8(type))
#define IR_ISWORD(type)	(IR_ISI16(type) || IR_ISU16(type))
#define IR_ISLONG(type)	(IR_ISI32(type) || IR_ISU32(type))
#define IR_ISQUAD(type)	(IR_ISI64(type) || IR_ISU64(type))
#define IR_ISSCALAR(type)						\
	(IR_ISINTEGER(type) || IR_ISFLOATING(type) || IR_ISPTR(type))

SIMPLEQ_HEAD(ir_exprq, ir_expr);

union ir_con {
	intmax_t	ic_icon;
	uintmax_t	ic_ucon;
	double		ic_fcon;
};

struct ir_phiarg {
	SIMPLEQ_ENTRY(ir_phiarg) ip_link;
	struct	ir_symbol *ip_arg;
	struct	cfa_bb *ip_bb;
};

SIMPLEQ_HEAD(ir_phiargq, ir_phiarg);

#define IR_HEADER				\
	char	*i_emit;			\
	union {					\
		void	*_auxdata;		\
	} u;					\
	union {					\
		struct	ir *_l;			\
		struct	ir_expr *_lx;		\
		struct	ir_symbol *_sym;	\
		union	ir_con _con;		\
	} ul;					\
	union {					\
		struct	ir *_r;			\
		struct	ir_expr *_rx;		\
		struct	ir_typelm *_elm;	\
		struct	ir_symbol *_sym2;	\
	} ur;					\
	union {					\
		struct	ir_type *_sou;		\
		SIMPLEQ_ENTRY(ir_expr) _link;	\
		struct	ir_lbl *_lbl;		\
		struct	ir_exprq _argq;		\
		struct	ir_phiargq _phiargs;	\
		int	_id;			\
		int	_deadret;		\
	} um;					\
	struct	tmpreg **i_tmpregs;		\
	struct	ir_symbol **i_tmpregsyms;	\
	short	i_op;				\
	short	i_flags

#define i_auxdata	u._auxdata
#define i_l		ul._l
#define i_r		ur._r

#define ie_l		ul._lx
#define ie_sym		ul._sym
#define ie_con		ul._con
#define ie_r		ur._rx
#define ie_elm		ur._elm
#define ie_sou		um._sou
#define ie_link		um._link

#define	is_l		ul._lx
#define is_r		ur._rx
#define ib_l		ul._lx
#define ip_l		ul._lx
#define ib_r		ur._rx
#define ib_lbl		um._lbl
#define ic_fn		ur._sym2
#define ic_ret		ul._lx
#define ic_argq		um._argq
#define ir_retexpr	ul._lx
#define ir_deadret	um._deadret
#define il_id		um._id
#define ip_sym		ul._sym
#define ip_args		um._phiargs

#define IR_EXPR_INUSE	0x1

struct ir {
	IR_HEADER;
};

struct irstat {
	size_t	i_syms;
	size_t	i_exprs;
	size_t	i_insns;
	size_t	i_funcs;
	size_t	i_types;
};

extern struct irstat irstats;

struct ir_type {
	int	it_op;
	int	it_flags;
	size_t	it_size;
	size_t	it_align;
	struct	ir_type *it_base;
	union {
		SIMPLEQ_HEAD(, ir_typelm) _typeq;
		size_t	_dim;
	} u;

#define it_typeq	u._typeq
#define it_dim		u._dim
};

struct ir_typelm {
	SIMPLEQ_ENTRY(ir_typelm) it_link;
	char	*it_name;
	struct	ir_type *it_type;
	size_t	it_off;
	int	it_fldsize;
};

extern struct ir_type ir_i8;
extern struct ir_type ir_u8;
extern struct ir_type ir_i16;
extern struct ir_type ir_u16;
extern struct ir_type ir_i32;
extern struct ir_type ir_u32;
extern struct ir_type ir_i64;
extern struct ir_type ir_u64;
extern struct ir_type ir_f32;
extern struct ir_type ir_f64;
extern struct ir_type ir_void;
extern struct ir_type ir_bool;

/* XXX: These have to die. */
extern struct ir_type ir_ptr;
extern struct ir_type ir_obj;

#if IR_PTR_SIZE == 4
#define ir_addrcon	ir_u32
#else
#define ir_addrcon	ir_u64
#endif

TAILQ_HEAD(ir_insnq, ir_insn);
SIMPLEQ_HEAD(ir_symq, ir_symbol);
SIMPLEQ_HEAD(ir_syminitq, ir_syminit);

#define IR_INIT_CSTR	1
#define IR_INIT_STR	2
#define IR_INIT_EXPR	3

struct ir_init {
	SIMPLEQ_ENTRY(ir_init) ii_link;
	SIMPLEQ_HEAD(, ir_initelm) ii_elms;
	struct ir_symbol *ii_sym;
};

struct ir_initelm {
	SIMPLEQ_ENTRY(ir_initelm) ii_link;
	struct	ir_expr *ii_expr;
	char	*ii_cstr;
	size_t	ii_len;
	size_t	ii_bitoff;
	size_t	ii_bitsize;
	int	ii_op;
};

struct ir_init *ir_init(struct ir_symbol *);
void ir_initelm_str(int, struct ir_init *, char *, size_t, size_t);
void ir_initelm_expr(struct ir_init *, struct ir_expr *, size_t, size_t);

struct ir_prog {
	SIMPLEQ_HEAD(, ir_init) ip_initq;
	SIMPLEQ_HEAD(, ir_init) ip_roinitq;
	SIMPLEQ_HEAD(, ir_func) ip_funq;
	struct	ir_syminitq ip_floatq;
	struct	ir_symq ip_cstrq;
	struct	ir_symq ip_symq;
};

extern struct ir_prog *irprog;

struct ir_func {
	SIMPLEQ_ENTRY(ir_func) if_link;
	struct	ir_symq if_parq;
	struct	ir_symq if_varq;
	struct	ir_symq if_regq;
	struct	ir_symbol **if_regs;	/* Linearized version of if_regq. */
	struct	ir_insnq if_iq;
	struct	regset if_usedregs;
	struct	memarea if_mem;
	struct	memarea if_livevarmem;
	struct	ir_symbol *if_sym;
	struct	cfadata *if_cfadata;
	size_t	if_framesz;
	size_t	if_argareasz;
	int	if_retlab;
	int	if_regid;
	int	if_flags;

#ifdef IR_FUNC_MACHDEP
	IR_FUNC_MACHDEP;
#endif
};

#define IR_FUNC_LEAF		1
#define IR_FUNC_VARARGS		2
#define IR_FUNC_SETJMP		4
#define IR_FUNC_PROTSTACK	8

extern struct ir_func *irfunc;

struct ir_param {
	struct	ir_type *ip_type;
	struct	ir_expr *ip_argx;
	struct	ir_symbol *ip_argsym;
	int	ip_reg;
};

void ir_parlocs(struct ir_param *, int);
struct ir_param *ir_parlocs_func(struct ir_func *);
struct ir_param *ir_parlocs_call(struct ir_insn *);

struct ir_expr {
	IR_HEADER;
	struct	ir_type *ie_type;
	struct	regconstr *ie_regs;
};

#define IR_INSN_HEADER				\
	IR_HEADER;				\
	TAILQ_ENTRY(ir_insn) ii_link;		\
	struct	dfadata ii_dfadata;		\
	struct	cfa_bb *ii_bb;			\
	int8_t	ii_cgskip

struct ir_insn {
	IR_INSN_HEADER;
};

struct ir_stasg {
	IR_INSN_HEADER;
};

struct ir_branch {
	IR_INSN_HEADER;
};

struct ir_call {
	IR_INSN_HEADER;
	int	ic_firstvararg;
};

struct ir_ret {
	IR_INSN_HEADER;
};

struct ir_lbl {
	IR_INSN_HEADER;
};

struct ir_phi {
	IR_INSN_HEADER;
};

#define IR_SYM_GLOBL		0x01
#define IR_SYM_VOLAT		0x02
#define IR_SYM_ADDRTAKEN	0x04
#define IR_SYM_USED		0x08
#define IR_SYM_RATMP		0x10
#define IR_SYM_PHYSREG		0x80

struct ir_symbol {
	SIMPLEQ_ENTRY(ir_symbol) is_link;
	char	*is_name;
	struct	ir_type *is_type;
	size_t	is_size;
	size_t	is_align;
	union {
		ssize_t	_off;
		struct	ir_symbol *_top;
		struct	node *_node;
		struct	ir_insn *_lbl;
	} u;
	int	is_id;
	short	is_flags;
	short	is_op;

#define is_off	u._off
#define is_top	u._top
#define is_node	u._node
#define is_lbl	u._lbl
};

struct ir_syminit {
	SIMPLEQ_ENTRY(ir_syminit) is_link;
	struct	ir_symbol *is_sym;
	union	ir_con is_val;
	int	is_op;
};

struct ir_prog *ir_prog(void);
void ir_prog_addsym(struct ir_prog *, struct ir_symbol *);
void ir_prog_addfunc(struct ir_prog *, struct ir_func *);
void ir_prog_addinit(struct ir_prog *, int, struct ir_init *);

struct ir_func *ir_func(struct ir_symbol *);
void ir_func_machdep(struct ir_func *);
void ir_func_linearize_regs(struct ir_func *fn);
void ir_func_finish(struct ir_func *);
void ir_func_free(struct ir_func *);
void ir_func_addparm(struct ir_func *, struct ir_symbol *);
void ir_func_addvar(struct ir_func *, struct ir_symbol *);
void ir_func_addreg(struct ir_func *, struct ir_symbol *);
void ir_func_enqinsns(struct ir_func *, struct ir_insnq *);
void ir_func_setflags(struct ir_func *, int);

struct ir_symbol *ir_symbol(int, char *, size_t, size_t, struct ir_type *);
struct ir_symbol *ir_cstrsym(char *, size_t, struct ir_type *);
struct ir_symbol *ir_physregsym(char *, int);
struct ir_symbol *ir_vregsym(struct ir_func *, struct ir_type *);
int ir_symbol_rclass(struct ir_symbol *);
void ir_symbol_setflags(struct ir_symbol *, int);
void ir_symbol_free(struct ir_symbol *);

struct ir_insn *ir_asg(struct ir_expr *, struct ir_expr *);
struct ir_insn *ir_store(struct ir_expr *, struct ir_expr *);
struct ir_insn *ir_b(struct ir_insn *);
struct ir_insn *ir_bc(int, struct ir_expr *, struct ir_expr *,
    struct ir_insn *);
struct ir_insn *ir_call(struct ir_expr *, struct ir_symbol *,
    struct ir_exprq *);
struct ir_insn *ir_ret(struct ir_expr *, int);
struct ir_insn *ir_lbl(void);
struct ir_insn *ir_phi(struct ir_symbol *);

struct cfa_bb;

void ir_phi_addarg(struct ir_insn *, struct ir_symbol *, struct cfa_bb *);

void ir_prepend_insn(struct ir_insn *, struct ir_insn *);
void ir_append_insn(struct ir_func *, struct ir_insn *, struct ir_insn *);
void ir_delete_insn(struct ir_func *, struct ir_insn *);

void ir_insnq_init(struct ir_insnq *);
void ir_insnq_add_head(struct ir_insnq *, struct ir_insn *);
void ir_insnq_enq(struct ir_insnq *, struct ir_insn *);
void ir_insnq_cat(struct ir_insnq *, struct ir_insnq *);

struct ir_expr *ir_con(int, union ir_con, struct ir_type *);
struct ir_expr *ir_icon(intmax_t);	/* Deprecated */
struct ir_expr *ir_fcon(double);	/* Deprecated */
struct ir_expr *ir_var(int, struct ir_symbol *);
struct ir_expr *ir_addr(int, struct ir_symbol *);
struct ir_expr *ir_newvreg(struct ir_func *, struct ir_type *);
struct ir_expr *ir_virtreg(struct ir_symbol *);
struct ir_expr *ir_physreg(struct ir_symbol *, struct ir_type *);
struct ir_expr *ir_bin(int, struct ir_expr *, struct ir_expr *,
    struct ir_type *);
struct ir_expr *ir_unary(int, struct ir_expr *, struct ir_type *);
struct ir_expr *ir_souref(struct ir_expr *, struct ir_type *,
    struct ir_typelm *);
struct ir_expr *ir_cast(struct ir_expr *, struct ir_type *);
int ir_cast_canskip(struct ir_expr *, struct ir_type *);
struct ir_expr *ir_load(struct ir_expr *, struct ir_type *);

struct ir_expr *ir_expr_copy(struct ir_expr *);
void ir_expr_replace(struct ir_expr **, struct ir_expr *);
void ir_expr_free(struct ir_expr *);
void ir_expr_thisfree(struct ir_expr *);

void ir_con_cast(union ir_con *, struct ir_type *, struct ir_type *);

void ir_exprq_init(struct ir_exprq *);
void ir_exprq_enq(struct ir_exprq *, struct ir_expr *);
void ir_exprq_cat(struct ir_exprq *, struct ir_exprq *);

void ir_symq_init(struct ir_symq *);
void ir_symq_enq(struct ir_symq *, struct ir_symbol *);

void ir_type_setflags(struct ir_type *, int);
struct ir_type *ir_type_qual(struct ir_type *, int);
struct ir_type *ir_type_ptr(struct ir_type *);
struct ir_type *ir_type_arr(struct ir_type *, size_t);
struct ir_type *ir_type_sou(int);
void ir_type_sou_finish(struct ir_type *, int);
struct ir_type *ir_type_func(struct ir_type *);
void ir_type_newelm(struct ir_type *, struct ir_typelm *);
struct ir_typelm *ir_typelm(char *, struct ir_type *, int, int);
int ir_type_equal(struct ir_type *, struct ir_type *);
struct ir_type *ir_type_dequal(struct ir_type *);

void ir_dump_prog(FILE *, struct ir_prog *);
void ir_dump_globals(FILE *, struct ir_prog *);
void ir_dump_func(FILE *, struct ir_func *);
void ir_dump_insn(FILE *, struct ir_insn *);
void ir_dump_symbol(FILE *, struct ir_symbol *);

void ir_setemit(struct ir *, char *);

void ir_emit(struct ir *, struct ir *);
char *ir_emit_machdep(struct ir *, char *);
void ir_emit_regpair(struct ir_symbol *, int);
void ir_emit_self(struct ir *, int);

#endif /* COMP_IR_H */
