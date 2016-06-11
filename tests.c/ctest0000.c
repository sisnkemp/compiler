double
foo(double d)
{
	return d + 2.0;
}

int
main(int argc, char **argv)
{
	double d;

	d = foo(1.0) + 1.0;
	return 0;
}
