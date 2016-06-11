struct s {
	int i;
};

int
main(void)
{
	struct s a, b;

	a.i = 1;
	b = a;
	return b.i;
}
