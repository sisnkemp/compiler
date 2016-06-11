int
foo(void)
{
	return 3;
}

int
main(void)
{
	int (*p)(void) = foo;

	return p();
}
