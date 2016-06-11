typedef int time_t;
typedef int uint64_t;

struct bintime {
	time_t sec;
	uint64_t frac;
};

static __inline void
bintime_add(struct bintime *bt, uint64_t x)
{
	uint64_t u;

	u = bt->frac;
}
