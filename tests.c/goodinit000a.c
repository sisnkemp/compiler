int
main(void)
{
	int a[4] = { 1, 2, 3, 4 };
	int i, n;

	for (i = 0, n = 0; i < 4; i++)
		n += a[i];
	return n;
}
