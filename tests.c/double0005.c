double
myexp(int base, int pot)
{
	double b;
	double rv;
	int i;

	b = base;
	rv = 1.0;
	i = 0;
	while (i < pot) {
		i = i + 1;
		rv = rv * b;
	}

	return rv;
}

int
main()
{
	int pot;
	int i;
	double tmp;
	double nv;
	double ov;

	scanf("%d", &pot);
	if (pot <= 1) {
		pot = 2;
	}

	ov = 0.0;
	nv = 0.0001;
	i = 1;
	while (nv - ov >= 0.0001) {
		tmp = 1 / myexp(i, pot);
		ov = nv;
		nv = nv + tmp;
		i = i + 1;
	}

	printf("%d\n%f\n", i, nv);
	return 0;
}
