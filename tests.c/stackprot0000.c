void
foo(void)
{
}

void
bar(void)
{
	int c[2000];
}

void
baz(void)
{
	int *p;
	int l;

	p = &l;
	*p = 1;
}

int
main(int argc, char **argv)
{
	foo();
	bar();
	baz();
	return 0;
}
