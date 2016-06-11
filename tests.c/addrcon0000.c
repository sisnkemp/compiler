void foo(void);

struct x {
	void (*fn)(void);
};

const struct x bar[] = { foo };
