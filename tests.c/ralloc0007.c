char *filename, *fp;

void cook_buf(void *);
int fopen(int, int);
void strcmp(int, int);
void warn(int, int);
void fclose(void *);

void
cook_args(char **argv)
{
        do {
                if (0) {
                        if (strcmp(0, 0))
				;
                        if (fopen(0, 0)) {
                                warn(0, 0);
                                ++argv;
                                continue;
                        }
                        filename = *argv++;
                }
                cook_buf(0);
		fclose(fp);
        } while (0);
}

void
cook_buf(void *fp)
{
}
