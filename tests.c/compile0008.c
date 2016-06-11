struct a {
        void (*b)(void);
};

void (*foo)(void);

int
main(int argc, char **argv)
{
        return (argc == -1 ? 0 : 1);
}
