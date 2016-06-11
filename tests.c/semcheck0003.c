int puts(const char *);

int
main(void)
{
	char buf[3];
	char *p;
	int i;

	p = buf;
	buf[0] = 'h';
	buf[1] = 'i';
	buf[2] = 0;
	puts(&buf[p - buf]);
}
