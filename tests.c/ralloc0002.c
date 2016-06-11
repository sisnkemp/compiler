void fprintf(void *, void *, int);

void
cook_buf(void)
{
        int ch, gobble, prev;

        for (; gobble ? 0 : 1; prev = ch) {
                if (prev) {
			ch = 0;
			fprintf(0, 0, gobble);
			if (1)
				break;
                }
        }
}
