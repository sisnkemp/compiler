typedef struct __sFILE {
	int _flags;
} FILE;


int sflag;

extern FILE __sF[];

int fprintf(FILE *, const char *, ...);
int __srget(FILE *);

void
cook_buf(FILE *fp)
{
        int ch, gobble, line, prev;

        for (; (fp) ? 0 : 1; prev = ch) {
                        if (sflag) {
				gobble = 0;
                        }
			fprintf(__sF, "%6d\t", line);
			if (__sF)
				break;
        }
}
