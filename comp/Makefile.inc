NOPROFILE=	1
NOPIC=		1

LIB=	comp
LDFLAGS += -pg

.PATH:	${.CURDIR}/../comp

CGG=	${TOOLDIR}/cgg
CGGIN=	${.CURDIR}/../${MACHINE_ARCH}/${MACHINE_ARCH}.cgg
CGGOUT=	${.OBJDIR}/cg.c
CGGH=	${.OBJDIR}/cg.h

RAG=	${TOOLDIR}/rag
RAGIN=	${.CURDIR}/../${MACHINE_ARCH}/regspec.rag
RAGC=	${.OBJDIR}/reg.c
RAGH=	${.OBJDIR}/reg.h

SRCS+=	cfa.c cgi.c comp.c dfa.c ir.c ir_dump.c mem.c nametab.c
SRCS+=	pass_aliasanalysis.c pass_constfold.c pass_constprop.c
SRCS+=	pass_deadcodeelim.c pass_deadfuncelim.c pass_deadvarelim.c pass_emit.c
SRCS+=	pass_gencode.c pass_jmpopt.c pass_parmfixup.c pass_ralloc.c pass_ssa.c
SRCS+=	pass_soufixup.c pass_stackoff.c pass_uce.c pass_vartoreg.c
SRCS+=	${CGGOUT} ${RAGC}

CLEANFILES+=	${CGGOUT} ${CGGH} ${RAGC} ${RAGH}

${CGGOUT}: ${CGGIN}
	${CGG} ${CGGIN} ${CGGOUT} ${CGGH}

${RAGC}: ${RAGIN}
	${RAG} ${RAGIN} ${RAGC} ${RAGH}

.include <bsd.lib.mk>
