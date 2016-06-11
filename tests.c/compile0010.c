struct {
	int a;
	struct {
		int b;
	} t;
} s;

int
main(void)
{

	s.a = 0;
	s.t.b = 0;
	return 0;
}
