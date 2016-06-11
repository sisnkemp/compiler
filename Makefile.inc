.if exists(../${MACHINE_ARCH}/obj)
ODIR=	${.CURDIR}/../${MACHINE_ARCH}/obj
.else
ODIR=	${.CURDIR}/../${MACHINE_ARCH}
.endif

TOOLDIR=	/usr/local/bin

INCS=		-I${.CURDIR}/.. -I${.CURDIR}/../${MACHINE_ARCH} -I${.OBJDIR}
INCS+=		-I${ODIR}
DEBUG=		-g
PROF=		-pg
COPTS?=		-Werror -Wall ${INCS} ${PROF}
