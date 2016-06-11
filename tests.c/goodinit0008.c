#include <sys/cdefs.h>

struct {
	int a;
	char b;
	short c;
	int d : 1;
	int e : 2;
	short f : 3;
	int g;
} a = { 1, 2, 3, 0, 2, 5 };

static struct {
	char a;
	int b;
	short c;
	int d : 2;
	char e : 7;
	short f : 9;
	int g;
} __packed b = { 1, 2, 3, 3, 0, 0x1ff, 0 };
