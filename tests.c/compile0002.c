int
main(void)
{
	int a[6][7][8];

	a[5][6][7] = 1;
	a[5][6][7]++;
	return ++a[5][6][7];
}
