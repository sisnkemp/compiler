int printf(const char *, ...);

static void
foo(int a, ...)
{
	int b, c;
	char **p;
	__builtin_va_list ap;

	__builtin_va_start(ap, a);
	b = __builtin_va_arg(ap, (int)0);
	c = __builtin_va_arg(ap, (int)0);
	p = __builtin_va_arg(ap, (char **)0);
	printf("%d %d %d %p %s\n", a, b, c, p, *p);
}

int
main(int argc, char **argv)
{
	foo(argc, argc - 1, 42, argv);
}
