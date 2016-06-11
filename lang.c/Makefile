NOMAN=	1 
PROG=	c_${MACHINE_ARCH}

LDADD=	${ODIR}/libcomp.a -pg

SRCS=	c.c c_${MACHINE_ARCH}.c ast.c ast_gencode.c ast_pretty.c ast_semcheck.c
SRCS+=	lex.l parse.c symtab.c
DPADD=	${ODIR}/libcomp.a

.include <bsd.prog.mk>
