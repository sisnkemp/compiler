struct a {
	int i;
};

void
foo(struct a *p)
{
	if (p == 0)
		return;
	p->i = 0;
}

int
main(void)
{
	foo((void *)0);
	return 0;
}
