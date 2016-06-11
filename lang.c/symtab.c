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

#include <stdio.h>
#include <stdlib.h>

#include "comp/comp.h"
#include "comp/ir.h"

#include "lang.c/c.h"

#ifdef __LP64__
#define SCRAMBLE_A	11400714819323198485UL
#else
#define SCRAMBLE_A	2654435769UL
#endif

#define SCRAMBLE(ident)	((char *)((uintptr_t)(ident) * SCRAMBLE_A))

int curscope = -1;

static int stkeepsyms;
static struct symtab builtintab;
static struct symtab *labeltab;
static struct symtab *toptab;
static struct symtab *freetabs;
static struct symbol *freesyms;

static void
tabinit(struct symtab *st, int scope)
{
	int i;

	for (i = 0; i < NSMAX; i++) {
		SLIST_INIT(&st->s_syms[i]);
		st->s_root[i] = NULL;
	}
	st->s_top = toptab;
	st->s_scope = scope;
	toptab = st;
}

void
symtab_builtin_init(void)
{
	toptab = NULL;
	tabinit(&builtintab, -1);
}

void
symtabinit(int keepsyms)
{
	while (curscope >= 0)
		closescope();
	stkeepsyms = keepsyms;
	if (toptab != &builtintab)
		fatalx("symtabinit");
	curscope = 0;
}

void
openscope(void)
{
	curscope++;
}

static void
killtab(struct symtab *st)
{
	int i;
	struct symbol *sym;

	if (!stkeepsyms) {
		for (i = 0; i < NSMAX; i++) {
			while (!SLIST_EMPTY(&st->s_syms[i])) {
				sym = SLIST_FIRST(&st->s_syms[i]);
				SLIST_REMOVE_HEAD(&st->s_syms[i], s_link);
				sym->s_left = freesyms;
				freesyms = sym;
			}
		}
	}
	st->s_top = freetabs;
	freetabs = st;
}

void
closescope(void)
{
	struct symbol *sym;
	struct symtab *st;

	if (toptab != &builtintab && toptab->s_scope == curscope) {
		st = toptab;
		toptab = toptab->s_top;
		if (st != labeltab)
			killtab(st);
	}
	if (labeltab != NULL && curscope == 2) {
		SLIST_FOREACH(sym, &labeltab->s_syms[NSLBL], s_link) {
			if (sym->s_type != NULL)
				continue;
			errp(&sym->s_lblpos, "label `%s' undefined (first"
			    "used here)", sym->s_ident);
		}
		killtab(labeltab);
		labeltab = NULL;
	}
	curscope--;
}

void
pushsymtab(struct symtab *st)
{
	if (curscope != 0)
		fatalx("pushsymtab only for parameter scope supported");
	curscope = 1;
	if (st == NULL)
		return;
	st->s_top = toptab;
	toptab = st;
}

struct symtab *
popsymtab(void)
{
	struct symtab *st;

	if (curscope != 1)
		fatalx("popsymtab only for parameter scope supported");
	if (toptab == &builtintab || toptab->s_scope != curscope) {
		curscope = 0;
		return NULL;
	}

	st = toptab;
	toptab = toptab->s_top;
	curscope = 0;
	return st;
}

static struct symbol *
searchtab(struct symtab *st, char *ident, int ns)
{
	char *key;
	struct symbol *sym;

	sym = st->s_root[ns];
	key = SCRAMBLE(ident);
	while (sym != NULL) {
		if (sym->s_key == key)
			return sym;
		if (key < sym->s_key)
			sym = sym->s_left;
		else
			sym = sym->s_right;
	}
	return NULL;
}

struct symbol *
symlookup(char *ident, int ns)
{
	struct symbol *sym;
	struct symtab *st;
	
	if (ns == NSLBL) {
		if (labeltab == NULL)
			return NULL;
		return searchtab(labeltab, ident, NSLBL);
	}

	for (st = toptab; st != NULL; st = st->s_top) {
		if ((sym = searchtab(st, ident, ns)) != NULL)
			return sym;
	}
	return NULL;
}

/*
 * Caller is responsible that ident is not entered more than once.
 */
struct symbol *
symenter(char *ident, struct ir_type *type, int ns)
{
	char *key;
	struct symbol *sym, *p, **pp;
	struct symtab *st = toptab;

	if (ns == NSLBL && labeltab != NULL)
		st = labeltab;
	else if (toptab->s_scope != curscope) {
		if ((st = freetabs) != NULL)
			freetabs = freetabs->s_top;
		else
			st = mem_alloc(&frontmem, sizeof *st);
		tabinit(st, curscope);
		if (curscope >= 2 && labeltab == NULL)
			labeltab = st;
		if (ns == NSLBL)
			st = labeltab;
	}

	if ((sym = freesyms) != NULL)
		freesyms = freesyms->s_left;
	else
		sym = mem_alloc(&frontmem, sizeof *sym);
	sym->s_left = sym->s_right = NULL;
	sym->s_irsym = NULL;
	sym->s_irlbl = NULL;
	sym->s_type = type;
	sym->s_ident = ident;
	key = sym->s_key = SCRAMBLE(ident);
	sym->s_scope = curscope;
	sym->s_sclass = sym->s_fnspec = 0;
	sym->s_flags = 0;
	sym->s_enumval = 0;
	SLIST_INSERT_HEAD(&st->s_syms[ns], sym, s_link);
	pp = &st->s_root[ns];
	while (*pp != NULL) {
		p = *pp;
		if (key < p->s_key)
			pp = &p->s_left;
		else if (key > p->s_key)
			pp = &p->s_right;
		else
			fatalx("symenter %s(%p) == %s(%p)", ident, key,
			    p->s_ident, p->s_key);
	}
	*pp = sym;
	return sym;
}
