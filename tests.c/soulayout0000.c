struct __sbuf {
	unsigned char *_base;
	int _size;
};

struct {
	unsigned char *_p;
	int	_r;
	int	_w;
	short	_flags;	
	short	_file;	
	struct	__sbuf _bf;
	int	_lbfsize;

	void	*_cookie;
	int	(*_close)(void *);
	int	(*_read)(void *, char *, int);
	long long	(*_seek)(void *, long long, int);
	int	(*_write)(void *, const char *, int);

	struct	__sbuf _ext;
	unsigned char *_up;
	int	_ur;

	unsigned char _ubuf[3];
	unsigned char _nbuf[1];
	struct	__sbuf _lb;
	int	_blksize;
	long long	_offset;
} s;

int printf(const char *, ...);

int
main(void)
{
	unsigned long i;
	
	i = sizeof s;
	return 0;
}
