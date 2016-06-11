int puts(const char *);

void
foo(void)
{
	puts("hi");
}

int
main(void)
{
	void (*fp)(void) = foo;

	(*fp)();
	return 0;
}
