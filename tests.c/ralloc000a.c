int foo(int, unsigned long);
int bar(void);

int
main(int argc, char *argv[])
{
	int ch;

	if ((ch = bar()))
		return 1;
	return foo(0, bar());
}
