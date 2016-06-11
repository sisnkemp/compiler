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

/*
 * See the section about PATRICIA in Knuth vol. 3 for an explanation.
 */

#include <sys/types.h>
#include <sys/queue.h>

#include <ctype.h>
#include <string.h>

#include "comp/comp.h"

#define ISLEAF(t)	((t)->n_data != NULL)
#define BOTHLEAVES(t)	((t)->n_l->n_data != NULL && (t)->n_r->n_data != NULL)
#define LEFTLEAF(t)	((t)->n_l->n_data != NULL && (t)->n_r->n_data == NULL)
#define RIGHTLEAF(t)	((t)->n_l->n_data == NULL && (t)->n_r->n_data != NULL)

struct nametab names;

static char *insert(struct ntrie *, const char *);
static struct ntrie *find(struct ntrie *, const char *, size_t,
    struct ntrie **, size_t *);

void
ntinit(struct nametab *nt)
{
	int i;

	if (nt == NULL)
		fatalx("ntinit: nt == NULL");
	nt->n_names = NULL;
	for (i = 0; i < NTRIE_MAX; i++)
		nt->n_tries[i] = NULL;
}

char *
ntenter(struct nametab *nt, const char *name)
{
	char *rv;
	struct ntrie **rootp, *t;

	if (nt == NULL)
		fatalx("ntenter: nt == NULL");
	if (name == NULL)
		fatalx("ntenter: name == NULL");

	if (*name == '_')
		rootp = &nt->n_tries[0];
	else if (*name >= 'a' && *name <= 'z')
		rootp = &nt->n_tries[*name - 'a'];
	else if (*name >= 'A' && *name <= 'Z')
		rootp = &nt->n_tries[*name - 'A'];
	else
		rootp = &nt->n_tries[NTRIE_MAX - 1];

	if (*rootp == NULL) {
		t = xmalloc(sizeof *t);
		t->n_key = strdup(name);
		t->n_llink = t;
		t->n_tag = NTRIE_LTAG;
		*rootp = t;
		return t->n_key;
	} 
	rv = insert(*rootp, name);
	return rv;
}

static char *
insert(struct ntrie *head, const char *name)
{
	int diff, l, t;
	size_t i, j, n;
	struct ntrie *p, *q, *r;

	n = strlen(name);
	p = find(head, name, (n + 1) * 8, &q, &j);
	for (i = 0; name[i] && p->n_key[i]; i++) {
		if (name[i] != p->n_key[i])
			break;
	}
	if (p->n_key[i] == '\0' && name[i] == '\0')
		return p->n_key;
	diff = name[i] ^ p->n_key[i];
	for (l = i * 8; !(diff & 1); diff >>= 1)
		l++;

	p = find(head, name, l, &q, &j);
	r = xmalloc(sizeof *r);
	r->n_key = strdup(name);
	if (q->n_llink == p) {
		q->n_llink = r;
		t = q->n_tag & NTRIE_LTAG;
		q->n_tag &= ~NTRIE_LTAG;
	} else {
		q->n_rlink = r;
		t = q->n_tag & NTRIE_RTAG;
		q->n_tag &= ~NTRIE_RTAG;
	}
	
	if (name[l >> 3] & (1 << (l & 7))) {
		r->n_tag = NTRIE_RTAG | (t ? NTRIE_LTAG : 0);
		r->n_rlink = r;
		r->n_llink = p;
	} else {
		r->n_tag = NTRIE_LTAG | (t ? NTRIE_RTAG : 0);
		r->n_llink = r;
		r->n_rlink = p;
	}
	if (t)
		r->n_skip = 1 + l - j;
	else {
		r->n_skip = 1 + l - j + p->n_skip;
		p->n_skip = j - l - 1;
	}

	return r->n_key;
}

static struct ntrie *
find(struct ntrie *head, const char *name, size_t n, struct ntrie **qp,
    size_t *jp)
{
	int j = 0;
	struct ntrie *p, *q;

	*qp = q = p = head;
	*jp = j;
	p = q->n_llink;
	if (q->n_tag & NTRIE_LTAG)
		return p;
	for (;;) {
		j += p->n_skip;
		if (j > n)
			break;
		q = p;
		if (name[(j - 1) >> 3] & (1 << ((j - 1) & 7))) {
			p = q->n_rlink;
			if (q->n_tag & NTRIE_RTAG)
				break;
		} else {
			p = q->n_llink;
			if (q->n_tag & NTRIE_LTAG)
				break;
		}
	}

	*qp = q;
	*jp = j;
	return p;
}
