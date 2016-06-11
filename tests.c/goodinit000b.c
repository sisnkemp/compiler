struct s {
	int a;
	int b[4];
	int c;
	struct {
		int e;
		int f[2];
		int g;
	} h;
	int i;
};

int
main(void)
{
	struct s a = { 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11 };
	struct s b = { 1, { 2, 3, 4, 5 }, 6, { 7, 8, 9, 10 }, 11 };

	int i, j;

	i = a.a + a.b[0] + a.b[1] + a.b[2] + a.b[3];
	i += a.c + a.h.e + a.h.f[0] + a.h.f[1] + a.h.g;
	i += a.i;

	j = b.a + b.b[0] + b.b[1] + b.b[2] + b.b[3];
	j += b.c + b.h.e + b.h.f[0] + b.h.f[1] + b.h.g;
	j += b.i;

	return i + j;
}
