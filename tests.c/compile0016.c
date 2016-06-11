long long
         strtonum(const char *, long long, long long, const char **);
char *strdup(const char *);

extern const char *_ctype_;
extern const short *_tolower_tab_;
extern const short *_toupper_tab_;
static inline int isalnum(int c)
{
	return 0;
}

static inline int isdigit(int c)
{
	return 0;
}

static inline int ispunct(int c)
{
	return 0;
}

static inline int isspace(int c)
{
	return 0;
}

static inline int isupper(int c)
{
	return 0;
}

 void err(int, const char *, ...)
                        ;

struct __sbuf {
        unsigned char *_base;
        int _size;
};

typedef struct __sFILE {
        unsigned char *_p;
        int _r;
        int _w;
        short _flags;
        short _file;
        struct __sbuf _bf;
        int _lbfsize;


        void *_cookie;
        int (*_close)(void *);
        int (*_read)(void *, char *, int);
        long long (*_seek)(void *, long long, int);
        int (*_write)(void *, const char *, int);


        struct __sbuf _ext;

        unsigned char *_up;
        int _ur;


        unsigned char _ubuf[3];
        unsigned char _nbuf[1];


        struct __sbuf _lb;


        int _blksize;
        long long _offset;
} FILE;

static struct file {
        struct { struct file *tqe_next; struct file **tqe_prev; } entry;
        FILE *stream;
        char *name;
        int lineno;
        int errors;
} *file, *topfile;

typedef struct {
        union {
                long long number;
                char *string;
        } v;
        int lineno;
} YYSTYPE;

YYSTYPE yylval;

struct keywords {
        const char *k_name;
        int k_val;
};

int
yylex(void)
{
        char buf[8096];
        char *p;
        int quotec, next, c;
        int token;
	const char *errstr = 0L;

	yylval.v.number = strtonum(buf, (-0x7fffffffffffffffLL-1),
			0x7fffffffffffffffLL, &errstr);
	lungetc(*--p);

	return 0;
}
