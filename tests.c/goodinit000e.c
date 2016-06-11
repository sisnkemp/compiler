struct a {
	int a;
} a;

struct b {
	int *p;
} foo = { &a.a };
