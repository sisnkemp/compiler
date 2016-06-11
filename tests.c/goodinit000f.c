struct keywords {
	char *k_name;
        int k_val;
};

void
lookup(void)
{
        static const struct keywords keywords[] = {
                { "changer", 257}
        };
}

int
lgetc(int quotec)
{
	return 0;
}
