/*
 * Copyright (c) 2008 Stefan Kempf <sisnkemp@gmail.com>
 *
 * Permission to use, copy, modify, and distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
 * ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
 * ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
 * OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */

#include <sys/types.h>
#include <sys/queue.h>

#include <limits.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "comp/comp.h"

#define MEMCHUNKSZ	16384

static size_t curusage;
static struct memchunkq freemem = SLIST_HEAD_INITIALIZER(freemem);

union align {
	char c;
	short s;
	int i;
	long l;
	long long ll;
	void *p;
	float f;
	double d;
	struct memchunk mc;
};

struct memstat memstats;

void
mem_area_init(struct memarea *m)
{
	SLIST_INIT(&m->m_avail);
	SLIST_INIT(&m->m_full);
}

static void
do_mem_area_free(struct memchunkq *mq)
{
	struct memchunk *mc;

	while (!SLIST_EMPTY(mq)) {
		mc = SLIST_FIRST(mq);
		SLIST_REMOVE_HEAD(mq, m_next);
		memstats.m_freed += mc->m_total;
		curusage -= mc->m_total;
		SLIST_INSERT_HEAD(&freemem, mc, m_next);
	}
}

void
mem_area_free(struct memarea *m)
{
	do_mem_area_free(&m->m_full);
	do_mem_area_free(&m->m_avail);
	mem_area_init(m);
}

static struct memchunk *
getchunk(size_t size)
{
	size_t chunksz;
	struct memchunk *mc, *prev;

	prev = NULL;
	SLIST_FOREACH(mc, &freemem, m_next) {
		memstats.m_nsearch++;
		if (size > mc->m_total) {
			prev = mc;
			continue;
		}

		curusage += mc->m_total;
		if (curusage > memstats.m_peakusage)
			memstats.m_peakusage = curusage;
		memstats.m_allocd += mc->m_total;
		mc->m_avail = mc->m_total - size;
		mc->m_nextptr = mc->m_base + size;
		if (prev == NULL)
			SLIST_REMOVE_HEAD(&freemem, m_next);
		else
			SLIST_REMOVE_AFTER(prev, m_next);
		return mc;
	}

	if (size > MEMCHUNKSZ)
		chunksz = ((size + MEMCHUNKSZ - 1) / MEMCHUNKSZ) * MEMCHUNKSZ;
	else
		chunksz = MEMCHUNKSZ;	
	mc = xmalloc(chunksz + sizeof(union align));
	curusage += chunksz;
	if (curusage > memstats.m_peakusage)
		memstats.m_peakusage = curusage;
	memstats.m_allocd += chunksz;
	mc->m_total = chunksz;
	mc->m_base = (char *)mc + sizeof(union align);
	mc->m_avail = chunksz - size;
	mc->m_nextptr = mc->m_base + size;
	xmallocd -= chunksz;	/* XXX */

	return mc;
}

void *
mem_alloc(struct memarea *m, size_t size)
{
	void *rv;
	struct memchunk *mc, *prev = NULL;

	size = (size + 15) & ~15;
	if (size == 0)
		fatalx("mem_alloc");

	memstats.m_nalloc++;
	if (size > memstats.m_maxsize)
		memstats.m_maxsize = size;
	else if (size < memstats.m_minsize || memstats.m_minsize == 0)
		memstats.m_minsize = size;

	SLIST_FOREACH(mc, &m->m_avail, m_next) {
		memstats.m_nsearch++;
		if (size > mc->m_avail) {
			prev = mc;
			continue;
		}
		rv = mc->m_nextptr;
		mc->m_nextptr += size;
		mc->m_avail -= size;
		if (mc->m_avail < 16) {
			if (prev == NULL)
				SLIST_REMOVE_HEAD(&m->m_avail, m_next);
			else
				SLIST_REMOVE_AFTER(prev, m_next);
			SLIST_INSERT_HEAD(&m->m_full, mc, m_next);
		}
		return rv;
	}

	mc = getchunk(size);
	if (mc->m_avail < 16)
		SLIST_INSERT_HEAD(&m->m_full, mc, m_next);
	else
		SLIST_INSERT_HEAD(&m->m_avail, mc, m_next);
	return mc->m_base;
}

void *
mem_mnalloc(struct memarea *m, size_t nmemb, size_t size)
{
	if (nmemb == 0 || size == 0)
		fatalx("mem_mnalloc: bad sizes: %zu, %zu", nmemb, size);
	if (nmemb > SIZE_MAX / size)
		fatalx("mem_mnalloc");
	return mem_alloc(m, nmemb * size);
}

void *
mem_calloc(struct memarea *m, size_t nmemb, size_t size)
{
	void *p;

	p = mem_mnalloc(m, nmemb, size);
	memset(p, 0, nmemb * size);
	return p;
}
