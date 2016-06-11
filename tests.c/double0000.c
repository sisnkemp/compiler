double
read_double(void)
{
	double d;

	scanf("%lf", &d);
	return d;
}

void
write_double(double d)
{
	printf("%f\n", d);
}

int
main()
{
	double d;

	d = read_double();
	write_double(d);

	return 0;
}
