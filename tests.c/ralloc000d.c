long foo(int);
long bar(int, long);

int
main(int argc, char *argv[])
{
	int rfd;
        int wfd;
        long nr, nw, off;

	rfd = 0;
	wfd = 1;
	off = 0;
        while (nr = foo(rfd)) {
                for (; nr; nr -= nw, off += nw)
                        nw = bar(wfd, off);
	}
}
