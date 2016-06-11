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

#ifndef LANG_C_C_H
#define LANG_C_C_H

#include <limits.h>
#include <stdint.h>

#include "cconf.h"

#define C_SCHAR_MIN	INT8_MIN
#define C_SCHAR_MAX	INT8_MAX
#define C_UCHAR_MAX	UINT8_MAX

#ifdef C_CHAR_UNSIGNED
#define C_CHAR_MIN	0
#define C_CHAR_MAX	C_UCHAR_MAX
#else
#define C_CHAR_MIN	C_SCHAR_MIN
#define C_CHAR_MAX	C_SCHAR_MAX
#endif

#define C_SHORT_MIN	INT16_MIN
#define C_SHORT_MAX	INT16_MAX
#define C_USHORT_MAX	UINT16_MAX
#define C_INT_MIN	INT32_MIN
#define C_INT_MAX	INT32_MAX
#define C_UINT_MAX	UINT32_MAX

#ifdef C_64BIT_ARCH
#define C_LONG_MIN	INT64_MIN
#define C_LONG_MAX	INT64_MAX
#define C_ULONG_MAX	UINT64_MAX
#else
#define C_LONG_MIN	INT32_MIN
#define C_LONG_MAX	INT32_MAX
#define C_ULONG_MAX	UINT32_MAX
#endif

#define C_LLONG_MIN	INT64_MIN
#define C_LLONG_MAX	INT64_MAX
#define C_ULLONG_MAX	UINT64_MAX

#define C_FLT_MIN	FLT_MIN
#define C_FLT_MAX	FLT_MAX
#define C_DBL_MIN	DBL_MIN
#define C_DBL_MAX	DBL_MAX

#define TOK_START	256
#define TOK_ADDASG	256
#define TOK_ANDAND	257
#define TOK_ANDASG	258
#define TOK_ARROW	259
#define TOK_AUTO	260
#define TOK_BOOL	261
#define TOK_BREAK	262
#define TOK_CASE	263
#define TOK_CHAR	264
#define TOK_COMPLEX	265	/* unused */
#define TOK_CONST	266
#define TOK_CONTINUE	267
#define TOK_DEAD	268
#define TOK_DEC		269
#define TOK_DEFAULT	270
#define TOK_DIVASG	271
#define TOK_DO		272
#define TOK_DOUBLE	273
#define TOK_ELLIPSIS	274
#define TOK_ELSE	275
#define TOK_ENUM	276
#define TOK_EXTERN	277
#define TOK_EQ		278
#define TOK_FCON	279
#define TOK_FLOAT	280
#define TOK_FOR		281
#define TOK_GE		282
#define TOK_GOTO	283
#define TOK_ICON	284
#define TOK_IDENT	285
#define TOK_IF		286
#define TOK_IMAGINARY	287	/* unused */
#define TOK_INC		288
#define TOK_INLINE	289
#define TOK_INT		290
#define TOK_LE		291
#define TOK_LONG	292
#define TOK_LS		293
#define TOK_LSASG	294
#define TOK_MODASG	295
#define TOK_MULASG	296
#define TOK_NE		297
#define TOK_ORASG	298
#define TOK_OROR	299
#define TOK_PACKED	300
#define TOK_REGISTER	301
#define TOK_RESTRICT	302	/* unused */
#define TOK_RETURN	303
#define TOK_RS		304
#define TOK_RSASG	305
#define TOK_SHORT	306
#define TOK_SIGNED	307
#define TOK_SIZEOF	308
#define TOK_STATIC	309
#define TOK_STRUCT	310
#define TOK_STRLIT	311
#define TOK_SUBASG	312
#define TOK_SWITCH	313
#define TOK_TYPEDEF	314
#define TOK_UNION	315
#define TOK_UNSIGNED	316
#define TOK_VOID	317
#define TOK_VOLATILE	318
#define TOK_WHILE	319
#define TOK_XORASG	320
#define TOK_END		320

extern char *toknames[];

extern struct memarea frontmem;

void warnh(const char *, ...);
void warnp(struct srcpos *, const char *, ...);
void errh(const char *, ...);
void errp(struct srcpos *, const char *, ...);
void synh(const char *, ...);
void fatalsynh(const char *, ...);

void lexinit(char *);
int peek(void);
int yylex(void);
void parse(void);

union token {
	struct	ast_expr *t_expr;
	char	*t_strlit;
	char	*t_ident;
};

extern union token token;

int escapechar(char *, char **);
char *cstring(char *, char *, struct memarea *, size_t *);

void builtin_init_machdep(void);
struct ir_expr *builtin_va_start_gencode(struct ir_func *, struct ir_insnq *,
    struct ast_expr *, int);
struct ir_expr *builtin_va_arg_gencode(struct ir_func *, struct ir_insnq *,
    struct ast_expr *, int);
struct ir_expr *builtin_va_end_gencode(struct ir_func *, struct ir_insnq *,
    struct ast_expr *, int);

extern struct ir_type *builtin_va_list;
extern struct symbol *builtin_va_start_sym;
extern struct symbol *builtin_va_arg_sym;
extern struct symbol *builtin_va_end_sym;

#define AST_SC_AUTO	1
#define AST_SC_EXTERN	2
#define AST_SC_REGISTER	3
#define AST_SC_STATIC	4
#define AST_SC_TYPEDEF	5

#define AST_TYSPEC_SIMPLE	1
#define AST_TYSPEC_TDNAME	2
#define AST_TYSPEC_STRUCT	3
#define AST_TYSPEC_UNION	4
#define AST_TYSPEC_ENUM		5

struct ast_tyspec {
	SIMPLEQ_ENTRY(ast_tyspec) at_link;
	struct	srcpos at_sp;
	char	*at_tdname;
	struct	ast_souspec *at_sou;
	struct	ast_enumspec *at_enum;
	short	at_simpletok;			/* XXX: Uses TOK_* here. */
	short	at_op;
};

struct ast_tyspec *ast_tyspec_simple(int);
struct ast_tyspec *ast_tyspec_tdname(char *);
struct ast_tyspec *ast_tyspec_sou(int, struct ast_souspec *);
struct ast_tyspec *ast_tyspec_enum(struct ast_enumspec *);

#define AST_SOU_HASELMS	1
#define AST_SOU_PACKED	2

struct ast_souspec {
	SIMPLEQ_HEAD(, ast_souent) as_ents;
	struct	srcpos as_sp;
	char	*as_name;
	int	as_flags;
};

struct ast_souent {
	SIMPLEQ_ENTRY(ast_souent) as_link;
	struct	ast_declspecs *as_ds;
	struct	ast_decla *as_decla;
	struct	ast_expr *as_fieldexpr;
};

struct ast_decla;

struct ast_souspec *ast_souspec(char *, int);
void ast_souspec_newent(struct ast_souspec *, struct ast_souent *);
struct ast_souent *ast_souent(struct ast_declspecs *, struct ast_decla *,
    struct ast_expr *);

struct ast_enumspec {
	SIMPLEQ_HEAD(, ast_enument) aen_ents;
	struct	srcpos aen_sp;
	char	*aen_ident;
	int	aen_haselms;
};

struct ast_enument {
	SIMPLEQ_ENTRY(ast_enument) aen_link;
	struct	srcpos aen_sp;
	char	*aen_ident;
	struct	ast_expr *aen_expr;
};

struct ast_enumspec *ast_enumspec(char *, int);
void ast_enumspec_newent(struct ast_enumspec *, struct ast_enument *);
struct ast_enument *ast_enument(struct srcpos *, char *, struct ast_expr *);

struct ast_declspecs {
	SIMPLEQ_HEAD(, ast_tyspec) ad_tyspec;
	struct	srcpos ad_sp;
	int	ad_sclass;
	int	ad_tyqual;
	int	ad_fnspec;
};

struct ast_declspecs *ast_declspecs(void);
void ast_declspecs_newsclass(struct ast_declspecs *, int);
void ast_declspecs_newtyqual(struct ast_declspecs *, int);
void ast_declspecs_newfnspec(struct ast_declspecs *, int);
void ast_declspecs_newtyspec(struct ast_declspecs *, struct ast_tyspec *);

#define AST_IDDECLA	1
#define AST_ARRDECLA	2
#define AST_FNDECLA	3
#define AST_PTRDECLA	4

struct ast_decla {
	SIMPLEQ_ENTRY(ast_decla) ad_link;
	struct	srcpos ad_sp;
	struct	ast_decla *ad_decla;
	char	*ad_ident;
	struct	symbol *ad_sym;
	struct	ast_list *ad_parms;
	struct	ast_expr *ad_expr;
	struct	ast_init *ad_init;
	short	ad_tqual;
	short	ad_op;
};

struct ast_decla *ast_decla_array(struct srcpos *, struct ast_decla *,
    struct ast_expr *);
struct ast_decla *ast_decla_ptr(struct srcpos *, struct ast_decla *, int);
struct ast_decla *ast_decla_func(struct srcpos *, struct ast_decla *,
    struct ast_list *);
struct ast_decla *ast_decla_ident(char *);
void ast_decla_setinit(struct ast_decla *, struct ast_init *);

#define AST_DOTDESIG	1
#define AST_ARRDESIG	2

SIMPLEQ_HEAD(ast_designation, ast_designator);

struct ast_designator {
	SIMPLEQ_ENTRY(ast_designator) ad_link;
	struct	srcpos ad_sp;
	char	*ad_ident;
	struct	ast_expr *ad_expr;
	int	ad_op;
};

struct ast_designation *ast_designation(void);
void ast_designation_newdesig(struct ast_designation *,
    struct ast_designator *);
struct ast_designator *ast_designator_ident(struct srcpos *, char *);
struct ast_designator *ast_designator_array(struct srcpos *,
    struct ast_expr *);

#define AST_SIMPLEINIT	1
#define AST_LISTINIT	2

struct ast_init {
	SIMPLEQ_HEAD(, ast_init) ai_inits;
	SIMPLEQ_ENTRY(ast_init) ai_link;
	SIMPLEQ_ENTRY(ast_init) ai_sortlink;
	struct	srcpos ai_sp;
	struct	ast_designation *ai_desig;
	struct	ast_expr *ai_expr;
	struct	ir_type *ai_objty;
	size_t	ai_elems;
	size_t	ai_bitoff;
	size_t	ai_bitsize;
	int	ai_op;
};

struct ast_init *ast_init_list(void);
struct ast_init *ast_init_simple(struct srcpos *, struct ast_expr *);
void ast_init_newinit(struct ast_init *, struct ast_init *);
void ast_init_setdesig(struct ast_init *, struct ast_designation *);

SIMPLEQ_HEAD(ast_declhead, ast_decl);

extern struct ast_declhead ast_program;

struct ast_decl {
	SIMPLEQ_ENTRY(ast_decl) ad_link;
	SIMPLEQ_HEAD(, ast_decla) ad_declas;
	struct	srcpos ad_sp;
	struct	ast_declspecs *ad_ds;
	struct	ast_declhead ad_krdecls;	/* K&R style params */
	struct	ast_stmt *ad_stmt;
};

void ast_declhead_newdecl(struct ast_declhead *, struct ast_decl *);
struct ast_decl *ast_decl(struct ast_declspecs *);
void ast_decl_newdecla(struct ast_decl *, struct ast_decla *);
void ast_decl_newkrdecl(struct ast_decl *, struct ast_decl *);
void ast_decl_setstmt(struct ast_decl *, struct ast_stmt *);

#define AST_IDLIST	1
#define AST_PARLIST	2
#define AST_EXPRLIST	3

struct ast_list {
	struct	ast_declhead al_decls;
	TAILQ_HEAD(, ast_expr) al_exprs;
	struct	srcpos al_sp;
	short	al_ellipsis;
	short	al_op;
};

struct ast_list *ast_list(int);
void ast_list_newdecl(struct ast_list *, struct ast_decl *);
void ast_list_newexpr(struct ast_list *, struct ast_expr *);
void ast_list_setellipsis(struct ast_list *);

#define AST_COMPOUND	1
#define AST_EXPR	2
#define AST_LABEL	3
#define AST_CASE	4
#define AST_DEFAULT	5
#define AST_IF		6
#define AST_SWITCH	7
#define AST_WHILE	8
#define AST_DO		9
#define AST_FOR		10
#define AST_GOTO	11
#define AST_CONTINUE	12
#define AST_BREAK	13
#define AST_RETURN	14
#define AST_ASM		15	/* Not yet implemented */
#define AST_DECLSTMT	16

struct ast_stmt {
	SIMPLEQ_HEAD(, ast_stmt) as_stmts;
	SIMPLEQ_HEAD(, ast_case) as_cases;
	SIMPLEQ_ENTRY(ast_stmt) as_link;
	struct	srcpos as_sp;
	union {
		struct	ast_stmt *_stmt1;
		struct	ast_decl *_decl;
	} u1;
	union {
		struct	ast_expr *_exprs[3];
		struct {
			char	*_ident;
			struct	symbol *_sym;
			struct	ir_insn *_irlbl;
		} _s;
	} u2;
	struct	ast_stmt *as_stmt2;
	int	as_op;

#define as_stmt1 u1._stmt1
#define as_decl	u1._decl

#define as_exprs u2._exprs
#define as_ident u2._s._ident
#define as_sym	u2._s._sym
#define as_irlbl u2._s._irlbl
};

struct ast_case {
	SIMPLEQ_ENTRY(ast_case) ac_link;
	union	ir_con ac_con;
	struct	ast_stmt *ac_stmt;
};

struct ast_stmt *ast_stmt_compound(struct srcpos *);
void ast_stmt_compound_newdecl(struct ast_stmt *, struct ast_decl *);
void ast_stmt_compound_newstmt(struct ast_stmt *, struct ast_stmt *);

struct ast_stmt *ast_stmt_label(struct srcpos *, char *, struct ast_stmt *);
struct ast_stmt *ast_stmt_case(struct srcpos *, struct ast_expr *,
    struct ast_stmt *);
struct ast_stmt *ast_stmt_dflt(struct srcpos *, struct ast_stmt *);
struct ast_stmt *ast_stmt_if(struct srcpos *, struct ast_expr *,
    struct ast_stmt *, struct ast_stmt *);
struct ast_stmt *ast_stmt_switch(struct srcpos *, struct ast_expr *,
    struct ast_stmt *);
struct ast_stmt *ast_stmt_while(struct srcpos *, struct ast_expr *,
    struct ast_stmt *);
struct ast_stmt *ast_stmt_do(struct srcpos *, struct ast_stmt *,
    struct ast_expr *);
struct ast_stmt *ast_stmt_for(struct srcpos *, struct ast_expr *,
    struct ast_expr *, struct ast_expr *, struct ast_stmt *);
struct ast_stmt *ast_stmt_goto(struct srcpos *, char *);
struct ast_stmt *ast_stmt_brkcont(struct srcpos *, int);
struct ast_stmt *ast_stmt_return(struct srcpos *, struct ast_expr *);
struct ast_stmt *ast_stmt_expr(struct srcpos *, struct ast_expr *);
void ast_case(struct ast_stmt *, struct ast_stmt *, struct ast_expr *);

#define AST_IDENT	1
#define AST_ICON	2
#define AST_FCON	3
#define AST_STRLIT	4
#define AST_SUBSCR	5
#define AST_CALL	6
#define AST_SOUDIR	7
#define AST_SOUIND	8
#define AST_POSTINC	9
#define AST_POSTDEC	10
#define AST_PREINC	11
#define AST_PREDEC	12
#define AST_ADDROF	13
#define AST_DEREF	14
#define AST_UPLUS	15
#define AST_UMINUS	16
#define AST_BITFLIP	17
#define AST_NOT		18
#define AST_SIZEOFX	19
#define AST_SIZEOFT	20
#define AST_CAST	21
#define AST_MUL		22
#define AST_DIV		23
#define AST_MOD		24
#define AST_ADD		25
#define AST_SUB		26
#define AST_LS		27
#define AST_RS		28
#define AST_LT		29
#define AST_GT		30
#define AST_LE		31
#define AST_GE		32
#define AST_EQ		33
#define AST_NE		34
#define AST_BWAND	35
#define AST_BWXOR	36
#define AST_BWOR	37
#define AST_ANDAND	38
#define AST_OROR	39
#define AST_COND	40
#define AST_ASG		41
#define AST_MULASG	42
#define AST_DIVASG	43
#define AST_MODASG	44
#define AST_ADDASG	45
#define AST_SUBASG	46
#define AST_LSASG	47
#define AST_RSASG	48
#define AST_ANDASG	49
#define AST_XORASG	50
#define AST_ORASG	51
#define AST_COMMA	52

#define AST_SIDEEFF	1

struct ast_expr {
	TAILQ_ENTRY(ast_expr) ae_link;
	struct	srcpos ae_sp;
	union {
		struct	ast_expr *_l;
		char	*_str;
		union	ir_con _con;
	} ul;
	union {
		struct	ast_expr *_r;
		struct	ast_list *_args;
		struct	ir_typelm *_elm;
		struct	ast_decl *_decl;
		char	*_ident;
	} ur;
	union {
		struct	ast_expr *_m;
		size_t	_strlen;
		struct	symbol *_sym;
	} um;
	struct	ir_type *ae_type;
	int	ae_flags;
	int	ae_op;

#define ae_l	ul._l
#define ae_str	ul._str
#define ae_con	ul._con

#define	ae_r	ur._r
#define ae_args	ur._args
#define ae_elm	ur._elm
#define ae_decl	ur._decl
#define ae_ident ur._ident

#define ae_m	um._m
#define ae_strlen um._strlen
#define ae_sym	um._sym
};

struct ast_expr *ast_expr_icon(struct srcpos *, intmax_t, struct ir_type *);
struct ast_expr *ast_expr_ucon(struct srcpos *, uintmax_t, struct ir_type *);
struct ast_expr *ast_expr_fcon(struct srcpos *, double, struct ir_type *);

struct ast_expr *ast_expr_ident(char *);

struct ast_expr *ast_expr_strlit(void);
void ast_expr_strlit_append(struct ast_expr *, char *);
void ast_expr_strlit_finish(struct ast_expr *);

struct ast_expr *ast_expr_subscr(struct srcpos *, struct ast_expr *,
    struct ast_expr *);
struct ast_expr *ast_expr_call(struct srcpos *, struct ast_expr *,
    struct ast_list *);
struct ast_expr *ast_expr_souref(struct srcpos *, int, struct ast_expr *,
    char *);
struct ast_expr *ast_expr_incdec(struct srcpos *, int, struct ast_expr *);
struct ast_expr *ast_expr_sizeofx(struct srcpos *, struct ast_expr *);
struct ast_expr *ast_expr_sizeoft(struct srcpos *, struct ast_decl *);
struct ast_expr *ast_expr_unop(struct srcpos *, int, struct ast_expr *);
struct ast_expr *ast_expr_cast(struct srcpos *, struct ast_decl *,
    struct ast_expr *);
struct ast_expr *ast_expr_bin(struct srcpos *, int, struct ast_expr *,
    struct ast_expr *);
struct ast_expr *ast_expr_cond(struct srcpos *, struct ast_expr *,
    struct ast_expr *, struct ast_expr *);
struct ast_expr *ast_expr_asg(struct srcpos *, int, struct ast_expr *,
    struct ast_expr *);
struct ast_expr *ast_expr_comma(struct srcpos *, struct ast_expr *,
    struct ast_expr *);

#define AST_TQ_CONST	1
#define AST_TQ_VOLATILE	2

#define AST_FS_INLINE	1
#define AST_FS_DEAD	2

void ast_pretty(char *);
void ast_semcheck(void);

#define GC_FLG_DISCARD	1	/* Skip expressions without side effects. */
#define GC_FLG_INIF	2
#define GC_FLG_ADDR	4
#define GC_FLG_ASG	8

void ast_gencode(void);
struct ir_expr *ast_expr_gencode(struct ir_func *, struct ir_insnq *,
    struct ast_expr *, int);

#define NSORD	0
#define NSLBL	1
#define NSTAG	2
#define NSMAX	3

extern int curscope;

struct symtab {
	SLIST_HEAD(, symbol) s_syms[NSMAX];
	struct	symtab *s_top;
	struct	symbol *s_root[NSMAX];
	int	s_scope;
};

#define SYM_ENUM	1
#define SYM_DEF		2	
#define SYM_TENTDEF	4
#define SYM_USED	8
#define SYM_BUILTIN	16
#define SYM_TYPEDEF	32

struct symbol {
	SLIST_ENTRY(symbol) s_link;
	struct	symbol *s_left;
	struct	symbol *s_right;
	struct	srcpos s_lblpos;
	struct	ir_symbol *s_irsym;
	struct	ir_insn *s_irlbl;
	struct	ir_type *s_type;
	char	*s_ident;
	char	*s_key;
	int	s_scope;
	int	s_sclass;
	int	s_fnspec;
	int	s_flags;
	int	s_enumval;
};

void symtab_builtin_init(void);
void symtabinit(int);
void openscope(void);
void closescope(void);
void pushsymtab(struct symtab *);
struct symtab *popsymtab(void);
struct symbol *symlookup(char *, int);
struct symbol *symenter(char *, struct ir_type *, int);

#define CIR_SCHAR	IR_I8
#define CIR_UCHAR	IR_U8

#ifdef C_CHAR_UNSIGNED
#define CIR_CHAR	CIR_UCHAR
#else
#define CIR_CHAR	CIR_SCHAR
#endif

#define CIR_SHORT	IR_I16
#define CIR_USHORT	IR_U16
#define CIR_INT		IR_I32
#define CIR_UINT	IR_U32

#ifdef C_64BIT_ARCH
#define CIR_LONG	IR_I64
#define CIR_ULONG	IR_U64
#else
#define CIR_LONG	IR_I32
#define CIR_ULONG	IR_U32
#endif

#define CIR_LLONG	IR_I64
#define CIR_ULLONG	IR_U64
#define CIR_FLOAT	IR_F32
#define CIR_DOUBLE	IR_F64
#define CIR_LDOUBLE	IR_F64
#define CIR_BOOL	IR_BOOL
#define CIR_VOID	IR_VOID
#define CIR_ARR		IR_ARR
#define CIR_FUNC	IR_FUNTY
#define CIR_PTR		IR_PTR
#define CIR_ENUM	IR_ENUM
#define CIR_STRUCT	IR_STRUCT
#define CIR_UNION	IR_UNION
#define CIR_CONST	IR_CONST
#define CIR_VOLAT	IR_VOLAT

#define cir_schar	ir_i8
#define cir_uchar	ir_u8

#ifdef C_CHAR_UNSIGNED
#define cir_char	cir_uchar
#else
#define cir_char	cir_schar
#endif

#define cir_short	ir_i16
#define cir_ushort	ir_u16
#define cir_int		ir_i32
#define cir_uint	ir_u32

#ifdef C_64BIT_ARCH
#define cir_long	ir_i64
#define cir_ulong	ir_u64
#else
#define cir_long	ir_i32
#define cir_ulong	ir_u32
#endif

#define cir_llong	ir_i64
#define cir_ullong	ir_u64
#define cir_float	ir_f32
#define cir_double	ir_f64
#define cir_ldouble	ir_f64
#define cir_bool	ir_bool
#define cir_void	ir_void

#define cir_size_t	cir_ulong
#define cir_ptrdiff_t	cir_long
#define cir_uintptr_t	cir_ulong

int tyisarith(struct ir_type *);
#define tyisreal tyisarith

int tyisfloating(struct ir_type *);
int tyisinteger(struct ir_type *);
int tyisobjptr(struct ir_type *);
int tyiscompat(struct ir_type *, struct ir_type *);
int tyisansikrcompat(struct ir_type *, struct ir_type *);
int tyisscalar(struct ir_type *);
struct ir_type *tyuconv(struct ir_type *);
struct ir_type *tybinconv(struct ir_type *, struct ir_type *);
struct ir_type *tyargconv(struct ir_type *);
struct ir_type *tyfldpromote(struct ir_type *, int);

#endif /* LANG_C_C_H */
