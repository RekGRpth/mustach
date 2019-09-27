/*
 Author: José Bollo <jobol@nonadev.net>
 Author: José Bollo <jose.bollo@iot.bzh>

 https://gitlab.com/jobol/mustach

 SPDX-License-Identifier: ISC
*/

#define _GNU_SOURCE

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#ifdef _WIN32
#include <malloc.h>
#endif
#ifdef __sun
# include <alloca.h>
#endif

#include "mustach.h"
#include "mustach-wrap.h"

#if defined(NO_EXTENSION_FOR_MUSTACH)
# undef  NO_SINGLE_DOT_EXTENSION_FOR_MUSTACH
# define NO_SINGLE_DOT_EXTENSION_FOR_MUSTACH
# undef  NO_EQUAL_VALUE_EXTENSION_FOR_MUSTACH
# define NO_EQUAL_VALUE_EXTENSION_FOR_MUSTACH
# undef  NO_COMPARE_VALUE_EXTENSION_FOR_MUSTACH
# define NO_COMPARE_VALUE_EXTENSION_FOR_MUSTACH
# undef  NO_JSON_POINTER_EXTENSION_FOR_MUSTACH
# define NO_JSON_POINTER_EXTENSION_FOR_MUSTACH
# undef  NO_OBJECT_ITERATION_FOR_MUSTACH
# define NO_OBJECT_ITERATION_FOR_MUSTACH
# undef  NO_INCLUDE_PARTIAL_FALLBACK
# define NO_INCLUDE_PARTIAL_FALLBACK
#endif

#if !defined(NO_INCLUDE_PARTIAL_FALLBACK) \
  &&  !defined(INCLUDE_PARTIAL_EXTENSION)
# define INCLUDE_PARTIAL_EXTENSION ".mustache"
#endif

#if !defined(NO_COMPARE_VALUE_EXTENSION_FOR_MUSTACH)
# undef NO_EQUAL_VALUE_EXTENSION_FOR_MUSTACH
#endif

struct wrap {
	void *closure;
	struct mustach_wrap_itf *itf;
	mustach_wrap_emit_cb *emitcb;
	mustach_wrap_write_cb *writecb;
};

/* length given by masking with 3 */
enum comp {
	C_no = 0,
	C_eq = 1,
	C_lt = 5,
	C_le = 6,
	C_gt = 9,
	C_ge = 10
};

enum sel {
	S_none = 0,
	S_ok = 1,
	S_objiter = 2,
	S_ok_or_objiter = S_ok | S_objiter
};

#if !defined(NO_EQUAL_VALUE_EXTENSION_FOR_MUSTACH)
static enum comp getcomp(char *head)
{
	return head[0] == '=' ? C_eq
#if !defined(NO_COMPARE_VALUE_EXTENSION_FOR_MUSTACH)
		: head[0] == '<' ? (head[1] == '=' ? C_le : C_lt)
		: head[0] == '>' ? (head[1] == '=' ? C_ge : C_gt)
#endif
		: C_no;
}

static char *keyval(char *head, int isptr, enum comp *comp)
{
	char *w, c, s;
	enum comp k;

	k = C_no;
#if !defined(NO_USE_VALUE_ESCAPE_FIRST_EXTENSION_FOR_MUSTACH)
	s = getcomp(head) != C_no;
#else
	s = 0;
#endif
	c = *(w = head);
	while (c && (s || (k = getcomp(head)) == C_no)) {
		if (s)
			s = 0;
		else
			s = (isptr ? c == '~' : c == '\\')
			    && (getcomp(head + 1) != C_no);
		if (!s)
			*w++ = c;
		c = *++head;
	}
	*w = 0;
	*comp = k;
	return k == C_no ? NULL : &head[k & 3];
}

static int evalcomp(struct wrap *w, const char *value, enum comp k)
{
	int r, c;

	c = w->itf->compare(w->closure, value);
	switch (k) {
	case C_eq: r = c == 0; break;
#if !defined(NO_COMPARE_VALUE_EXTENSION_FOR_MUSTACH)
	case C_lt: r = c < 0; break;
	case C_le: r = c <= 0; break;
	case C_gt: r = c > 0; break;
	case C_ge: r = c >= 0; break;
#endif
	default: r = 0; break;
	}
	return r;
}
#else
static inline char *keyval(char *head, int isptr, enum comp *comp)
{
	*comp = C_no;
	return NULL;
}
static inline int compare(struct wrap *w, char *value, enum comp k)
{
	return 0;
}
#endif

static char *key(char **head, int isptr)
{
	char *r, *i, *w, c;

	c = *(i = *head);
	if (!c)
		r = NULL;
	else {
		r = w = i;
#if !defined(NO_JSON_POINTER_EXTENSION_FOR_MUSTACH)
		if (isptr)
		{
			while (c && c != '/') {
				if (c == '~')
					switch (i[1]) {
					case '1': c = '/'; /*@fallthrough@*/
					case '0': i++;
					}
				*w++ = c;
				c = *++i;
			}
			*w = 0;
			while (c == '/')
				c = *++i;
		}
		else
#endif
		{
			while (c && c != '.') {
				if (c == '\\' && (i[1] == '.' || i[1] == '\\'))
					c = *++i;
				*w++ = c;
				c = *++i;
			}
			*w = 0;
			while (c == '.')
				c = *++i;
		}
		*head = i;
	}
	return r;
}

static int sel(struct wrap *w, const char *name)
{
	enum sel r;
	int i, isptr;
	char *n, *c, *v;
	enum comp k;

	n = alloca(1 + strlen(name));
	strcpy(n, name);
	isptr = 0;
#if !defined(NO_JSON_POINTER_EXTENSION_FOR_MUSTACH)
	isptr = n[0] == '/';
	n += isptr;
#endif

	v = keyval(n, isptr, &k);
#if !defined(NO_SINGLE_DOT_EXTENSION_FOR_MUSTACH)
	/* case of . alone: select current */
	if (!n[1] && n[0] == '.')
		r = w->itf->sel(w->closure, NULL) ? S_ok : S_none;
	else
#endif
	{
		c = key(&n, isptr);
		if (c == NULL)
			return 0;
		r = w->itf->sel(w->closure, c) ? S_ok : S_none;
#if !defined(NO_OBJECT_ITERATION_FOR_MUSTACH)
		if (r == S_none && c[0] == '*' && !c[1] && !v && !*n)
			r = w->itf->sel(w->closure, NULL) ? S_ok_or_objiter : S_none;
#endif
		if (r == S_ok) {
			c = key(&n, isptr);
			while(r == S_ok && c) {
				r = w->itf->subsel(w->closure, c) ? S_ok : S_none;
#if !defined(NO_OBJECT_ITERATION_FOR_MUSTACH)
				if (r == S_none && c[0] == '*' && !c[1] && !v && !*n)
					r = S_objiter;
#endif
				c = key(&n, isptr);
			}
		}
	}
	if (r == S_ok && v) {
		i = v[0] == '!';
		if (i == evalcomp(w, &v[i], k))
			r = S_none;
	}
	return r;
}

static int start(void *closure)
{
	struct wrap *w = closure;
	return w->itf->start ? w->itf->start(w->closure) : MUSTACH_OK;
}

static void stop(void *closure, int status)
{
	struct wrap *w = closure;
	if (w->itf->stop)
		w->itf->stop(w->closure, status);
}

static int write(struct wrap *w, const char *buffer, size_t size, FILE *file)
{
	int r;

	if (w->writecb)
		r = w->writecb(file, buffer, size);
	else
		r = fwrite(buffer, size, 1, file) == 1 ? MUSTACH_OK : MUSTACH_ERROR_SYSTEM;
	return r;
}

static int emit(void *closure, const char *buffer, size_t size, int escape, FILE *file)
{
	struct wrap *w = closure;
	int r;
	size_t s, i;
	char c;

	if (w->emitcb)
		r = w->emitcb(file, buffer, size, escape);
	else if (!escape)
		r = write(w, buffer, size, file);
	else {
		i = 0;
		r = MUSTACH_OK;
		while(i < size && r == MUSTACH_OK) {
			s = i;
			while (i < size && (c = buffer[i]) != '<' && c != '>' && c != '&')
				i++;
			if (i != s)
				r = write(w, &buffer[s], i - s, file);
			if (i < size && r == MUSTACH_OK) {
				switch(c) {
				case '<': r = write(w, "&lt;", 4, file); break;
				case '>': r = write(w, "&gt;", 4, file); break;
				case '&': r = write(w, "&amp;", 5, file); break;
				}
				i++;
			}
		}
	}
	return r;
}

static int enter(void *closure, const char *name)
{
	struct wrap *w = closure;
	enum sel s = sel(w, name);
	return s == S_none ? 0 : w->itf->enter(w->closure, s & S_objiter);
}

static int next(void *closure)
{
	struct wrap *w = closure;
	return w->itf->next(w->closure);
}

static int leave(void *closure)
{
	struct wrap *w = closure;
	return w->itf->leave(w->closure);
}


static int getopt(struct wrap *w, const char *name, struct mustach_sbuf *sbuf)
{
	enum sel s = sel(w, name);
	if (!(s & S_ok))
		return 0;
	return w->itf->get(w->closure, sbuf, s & S_objiter);
}

static int get(void *closure, const char *name, struct mustach_sbuf *sbuf)
{
	struct wrap *w = closure;
	if (getopt(w, name, sbuf) <= 0)
		sbuf->value = "";
	return MUSTACH_OK;
}

#if !defined(NO_INCLUDE_PARTIAL_FALLBACK)
static int get_partial_from_file(const char *name, struct mustach_sbuf *sbuf)
{
	static char extension[] = INCLUDE_PARTIAL_EXTENSION;
	size_t s;
	long pos;
	FILE *file;
	char *path, *buffer;

	/* allocate path */
	s = strlen(name);
	path = malloc(s + sizeof extension);
	if (path == NULL)
		return MUSTACH_ERROR_SYSTEM;

	/* try without extension first */
	memcpy(path, name, s + 1);
	file = fopen(path, "r");
	if (file == NULL) {
		memcpy(&path[s], extension, sizeof extension);
		file = fopen(path, "r");
	}
	free(path);

	/* if file opened */
	if (file != NULL) {
		/* compute file size */
		if (fseek(file, 0, SEEK_END) >= 0
		 && (pos = ftell(file)) >= 0
		 && fseek(file, 0, SEEK_SET) >= 0) {
			/* allocate value */
			s = (size_t)pos;
			buffer = malloc(s + 1);
			if (buffer != NULL) {
				/* read value */
				if (1 == fread(buffer, s, 1, file)) {
					/* force zero at end */
					sbuf->value = buffer;
					buffer[s] = 0;
					sbuf->freecb = free;
					fclose(file);
					return MUSTACH_OK;
				}
				free(buffer);
			}
		}
		fclose(file);
	}
	return MUSTACH_ERROR_SYSTEM;
}

static int partial(void *closure, const char *name, struct mustach_sbuf *sbuf)
{
	if (!getopt(closure, name, sbuf)
	 && get_partial_from_file(name, sbuf) != MUSTACH_OK)
			sbuf->value = "";
	return MUSTACH_OK;
}
#endif

static struct mustach_itf witf = {
	.start = start,
	.put = NULL,
	.enter = enter,
	.next = next,
	.leave = leave,
#if !defined(NO_INCLUDE_PARTIAL_FALLBACK)
	.partial = partial,
#else
	.partial = NULL,
#endif
	.get = get,
	.emit = emit,
	.stop = stop
};

int fmustach_wrap(const char *template, struct mustach_wrap_itf *itf, void *closure, FILE *file)
{
	struct wrap w;
	w.closure = closure;
	w.itf = itf;
	w.emitcb = NULL;
	w.writecb = NULL;
	return fmustach(template, &witf, &w, file);
}

int fdmustach_wrap(const char *template, struct mustach_wrap_itf *itf, void *closure, int fd)
{
	struct wrap w;
	w.closure = closure;
	w.itf = itf;
	w.emitcb = NULL;
	w.writecb = NULL;
	return fdmustach(template, &witf, &w, fd);
}

int mustach_wrap(const char *template, struct mustach_wrap_itf *itf, void *closure, char **result, size_t *size)
{
	struct wrap w;
	w.closure = closure;
	w.itf = itf;
	w.emitcb = NULL;
	w.writecb = NULL;
	return mustach(template, &witf, &w, result, size);
}

int umustach_wrap(const char *template, struct mustach_wrap_itf *itf, void *closure, mustach_wrap_write_cb *writecb, void *writeclosure)
{
	struct wrap w;
	w.closure = closure;
	w.itf = itf;
	w.emitcb = NULL;
	w.writecb = writecb;
	return fmustach(template, &witf, &w, writeclosure);
}

int emustach_wrap(const char *template, struct mustach_wrap_itf *itf, void *closure, mustach_wrap_emit_cb *emitcb, void *emitclosure)
{
	struct wrap w;
	w.closure = closure;
	w.itf = itf;
	w.emitcb = emitcb;
	w.writecb = NULL;
	return fmustach(template, &witf, &w, emitclosure);
}

