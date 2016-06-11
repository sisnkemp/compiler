static int
foo(void)
{
	return 1;
}

int
main(int argc, char **argv)
{
	int i, j;

	i = 0;
	j = foo();
	return i + 1;
}
