char *filename;

void warn(const char *, ...);

int
main(int argc, char *argv[])
{
	filename = "stdin";
	warn("%s", filename);
}
