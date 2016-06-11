int foo(int i) { return i + 1; }
int bar(int i) { return i + 2; }

int
main(int argc, char **argv)
{
	return (argc > 1 ? foo : bar)(argc);
}
