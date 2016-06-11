struct small1 {
	short i;
	short j;
	short k;
};

struct small2 {
	float f;
};

struct large1 {
	int i[8];
};

struct small1
foo(void)
{
	struct small1 s;

	s.i = 1;
	return s;
}

struct large1
bar(void)
{
	struct large1 l;

	l.i[0] = 1;
	return l;
}

struct small2
baz(void)
{
	struct small2 s;

	s.f = 1.0;
	return s;
}

int
main(void)
{
	struct small1 s1;
	struct small2 s2;
	struct large1 l1;

	s1 = foo();
	l1 = bar();
	s2 = baz();
	bar();
	return s1.i + l1.i[0] + (int)s2.f;
}
