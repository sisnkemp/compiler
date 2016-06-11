void
write_double(double d)
{
	printf("%f\n", d);
}

int
main()
{
	int i;
	double d;

	i = 42;
	d = i;
	write_double(d);
	
	return 0;
}
