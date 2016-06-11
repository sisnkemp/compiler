int
main(void)
{
	int i, j;

	i = 1;
	j = 1;
	i += i + (j += i + j); 

	return i;
}
