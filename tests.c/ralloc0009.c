struct timespec {
        int tv_sec;
        long tv_nsec;
};

struct stat {
        int st_dev;
        unsigned st_ino;
        unsigned st_mode;
        unsigned st_nlink;
        unsigned st_uid;
        unsigned st_gid;
        int st_rdev;
        int st_lspare0;

        struct timespec st_atimespec;
        struct timespec st_mtimespec;
        struct timespec st_ctimespec;
        long long st_size;
        long long st_blocks;
        unsigned st_blksize;
        unsigned st_flags;
        unsigned st_gen;
        int st_lspare1;

        struct timespec __st_birthtimespec;




        long long st_qspare[2];
};

int fstat(int, struct stat *);
void *malloc(unsigned);
long read(int, void *, unsigned long);
void warn(const char *, ...);

char *filename;

void raw_cat(int);

int
main(int argc, char *argv[])
{
        int ch;

	filename = "stdin";
	raw_cat(0);
}

void
raw_cat(int rfd)
{
        long nr, off;
        static char *buf = 0L;
        struct stat sbuf;

        if (buf == 0L) {
                if (fstat(1, &sbuf))
			return;
                buf = malloc(1024);
        }
        while ((nr = read(rfd, buf, 1024)) != -1)
		break;
        if (nr < 0)
                warn("%s", filename);
}
