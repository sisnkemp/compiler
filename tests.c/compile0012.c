int main(void)
{
	int c;
	extern const char *_ctype_;

	c = '\e';
        return (c == -1 ? 0 : ((_ctype_ + 1)[c] & 0x20));
}
