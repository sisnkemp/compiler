int a;
int *p = &a;

int
main(void)
{
	(*p)++;
	return a;
}
