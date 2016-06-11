long long foo(char *, long long, long long);

int
main(void)
{
        char buf[8096];
	long long l;

	l = foo(buf, -0x7fffffffffffffffLL-1, 0x7fffffffffffffffLL);
	return 0;
}
