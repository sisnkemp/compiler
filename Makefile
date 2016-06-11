TOOLS=	tools.cgg2 tools.rag
SUBDIR= tools.cgg2 tools.rag ${MACHINE_ARCH} lang.c cc

beforedepend:
#	cd tools.cgg2 && make depend all && ${SUDO} make install
#	cd tools.rag && make depend all && ${SUDO} make install

.PHONY: tools
tools:
	for i in ${TOOLS}; do cd $$i && make depend install

.include <bsd.dep.mk>
.include <bsd.subdir.mk>
