struct changer_command {
        char *cc_name;

        int (*cc_handler)(char *, int, char **);
};

extern char *__progname;

static void usage(void);

static int do_exchange(char *, int, char **);
void exit(int);

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


extern FILE __sF[];
int fprintf(FILE *, const char *, ...);
const struct changer_command commands[] = {
        { "exchange", do_exchange },
        { 0L, 0 },
};

int
main(int argc, char *argv[])
{
        int i;

        for (i = 0; commands[i].cc_name; i++)
                fprintf((&__sF[2]), " %s", commands[i].cc_name);
        fprintf((&__sF[2]), "\n");
}

static int
do_exchange(char *cname, int argc, char *argv[])
{
	return 0;
}
