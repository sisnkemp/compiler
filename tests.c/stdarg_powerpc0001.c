static void
foo(int i, ...)
{
	__builtin_va_list ap;

	__builtin_va_start(ap, i);
}

int
main(void)
{
	foo(0, 1, 2);
	return 0;
}
