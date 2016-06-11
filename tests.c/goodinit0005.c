struct {
	int a;
	struct {
		struct {
			char *s;
			char *t;
		} b;
		int k;
	} c;
	char *d;
} a = { 1, .c.b.s = "s", "t", 2 };
