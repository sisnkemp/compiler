struct a {
	int a;
	int b;
} a;

int
main(void)
{
	a.a++;
	a.b--;
	return a.a;
}
