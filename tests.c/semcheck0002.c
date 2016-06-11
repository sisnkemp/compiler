struct changer {
	struct changer *next;
};

int
main(void)
{
	struct changer c;

	c.next = 0;
}
