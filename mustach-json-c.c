/*
 Author: José Bollo <jobol@nonadev.net>
 Author: José Bollo <jose.bollo@iot.bzh>

 https://gitlab.com/jobol/mustach

 Licensed under the Apache License, Version 2.0 (the "License");
 you may not use this file except in compliance with the License.
 You may obtain a copy of the License at

     http://www.apache.org/licenses/LICENSE-2.0

 Unless required by applicable law or agreed to in writing, software
 distributed under the License is distributed on an "AS IS" BASIS,
 WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 See the License for the specific language governing permissions and
 limitations under the License.
*/

#define _GNU_SOURCE

#include <stdio.h>
#include <string.h>

#include <json-c/json.h>

#include "mustach.h"
#include "mustach-json-c.h"

#define MAX_DEPTH 256

#if defined(NO_EXTENSION_FOR_MUSTACH)
# if !defined(NO_SINGLE_DOT_EXTENSION_FOR_MUSTACH)
#  define NO_SINGLE_DOT_EXTENSION_FOR_MUSTACH
# endif
# if !defined(NO_COLON_EXTENSION_FOR_MUSTACH)
#  define NO_COLON_EXTENSION_FOR_MUSTACH
# endif
# if !defined(NO_EQUAL_VALUE_EXTENSION_FOR_MUSTACH)
#  define NO_EQUAL_VALUE_EXTENSION_FOR_MUSTACH
# endif
# if !defined(NO_JSON_POINTER_EXTENSION_FOR_MUSTACH)
#  define NO_JSON_POINTER_EXTENSION_FOR_MUSTACH
# endif
# if !defined(NO_OBJECT_ITERATION_FOR_MUSTACH)
#  define NO_OBJECT_ITERATION_FOR_MUSTACH
# endif
#endif

#if defined(NO_COLON_EXTENSION_FOR_MUSTACH)
# if !defined(NO_JSON_POINTER_EXTENSION_FOR_MUSTACH)
#  define NO_JSON_POINTER_EXTENSION_FOR_MUSTACH
# endif
#endif

struct expl {
	struct json_object *root;
	mustach_json_c_write_cb writecb;
	int depth;
#if !defined(NO_OBJECT_ITERATION_FOR_MUSTACH)
	int found_objiter;
#endif
	struct {
		struct json_object *cont;
		struct json_object *obj;
#if !defined(NO_OBJECT_ITERATION_FOR_MUSTACH)
		struct json_object_iterator biter, eiter;
		int is_objiter;
#endif
		int index, count;
	} stack[MAX_DEPTH];
};

#if !defined(NO_EQUAL_VALUE_EXTENSION_FOR_MUSTACH)
static char *keyval(char *head, int isptr)
{
	char *w, c;

	c = *(w = head);
	while (c && c != '=') {
#if !defined(NO_JSON_POINTER_EXTENSION_FOR_MUSTACH)
		if (isptr) {
			if (isptr && c == '~' && head[1] == '=')
				c = *++head;
		} else
#endif
		if (!isptr && c == '\\' && head[1] == '=')
			c = *++head;
		*w++ = c;
		c = *++head;
	}
	*w = 0;
	return c == '=' ? ++head : NULL;
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
			while (c && c != '/') {
				if (c == '~')
					switch (i[1]) {
					case '1': c = '/'; /*@fallthrough@*/
					case '0': i++;
					}
				*w++ = c;
				c = *++i;
			}
		else
#endif
		while (c && c != '.') {
			if (c == '\\' && (i[1] == '.' || i[1] == '\\'))
				c = *++i;
			*w++ = c;
			c = *++i;
		}
		*w = 0;
		*head = i + !!c;
	}
	return r;
}

static struct json_object *find(struct expl *e, const char *name)
{
	int i, isptr;
	struct json_object *o, *no;
	char *n, *c, *v;

	n = alloca(1 + strlen(name));
	strcpy(n, name);
	isptr = 0;
#if !defined(NO_JSON_POINTER_EXTENSION_FOR_MUSTACH)
	isptr = n[0] == '/';
	n += isptr;
#endif

	v = NULL;
#if !defined(NO_EQUAL_VALUE_EXTENSION_FOR_MUSTACH)
	v = keyval(n, isptr);
#endif
#if !defined(NO_OBJECT_ITERATION_FOR_MUSTACH)
	e->found_objiter = 0;
#endif
#if !defined(NO_SINGLE_DOT_EXTENSION_FOR_MUSTACH)
	if (n[0] == '.' && !n[1]) {
		/* case of . alone */
		o = e->stack[e->depth].obj;
	} else
#endif
	{
		c = key(&n, isptr);
		if (c == NULL)
			return NULL;
		o = NULL;
		i = e->depth;
		while (i >= 0 && !json_object_object_get_ex(e->stack[i].obj, c, &o))
			i--;
		if (i < 0) {
#if !defined(NO_OBJECT_ITERATION_FOR_MUSTACH)
			o = e->stack[e->depth].obj;
			if (c[0] == '*' && !c[1] && !v && !key(&n, isptr) && json_object_is_type(o, json_type_object)) {
				e->found_objiter = 1;
				return o;
			}
#endif
			return NULL;
		}
		c = key(&n, isptr);
		while(c) {
			if (!json_object_object_get_ex(o, c, &no)) {
#if !defined(NO_OBJECT_ITERATION_FOR_MUSTACH)
				if (c[0] == '*' && !c[1] && !v && !key(&n, isptr) && json_object_is_type(o, json_type_object)) {
					e->found_objiter = 1;
					return o;
				}
#endif
				return NULL;
			}
			o = no;
			c = key(&n, isptr);
		}
	}
	if (v) {
		i = v[0] == '!';
		if (i == !strcmp(&v[i], json_object_get_string(o)))
			o = NULL;
	}
	return o;
}

static int start(void *closure)
{
	struct expl *e = closure;
	e->depth = 0;
	e->stack[0].cont = NULL;
	e->stack[0].obj = e->root;
	e->stack[0].index = 0;
	e->stack[0].count = 1;
	return MUSTACH_OK;
}

static int write(struct expl *e, const char *buffer, size_t size, FILE *file)
{
	return e->writecb(file, buffer, size);
}

static int emituw(void *closure, const char *buffer, size_t size, int escape, FILE *file)
{
	struct expl *e = closure;
	if (!escape)
		write(e, buffer, size, file);
	else
		do {
			switch(*buffer) {
			case '<': write(e, "&lt;", 4, file); break;
			case '>': write(e, "&gt;", 4, file); break;
			case '&': write(e, "&amp;", 5, file); break;
			default: write(e, buffer, 1, file); break;
			}
			buffer++;
		} while(--size);
	return MUSTACH_OK;
}

static const char *item(struct expl *e, const char *name)
{
	struct json_object *o;
	const char *s;

#if !defined(NO_OBJECT_ITERATION_FOR_MUSTACH)
	if (name[0] == '*' && !name[1] && e->stack[e->depth].is_objiter)
		s = json_object_iter_peek_name(&e->stack[e->depth].biter);
	else
		s = (o = find(e, name)) && !e->found_objiter ? json_object_get_string(o) : NULL;
#else
	s = (o = find(e, name)) ? json_object_get_string(o) : NULL;
#endif
	return s;
}

static int enter(void *closure, const char *name)
{
	struct expl *e = closure;
	struct json_object *o = find(e, name);
	if (++e->depth >= MAX_DEPTH)
		return MUSTACH_ERROR_TOO_DEEP;
	if (json_object_is_type(o, json_type_array)) {
		e->stack[e->depth].count = json_object_array_length(o);
		if (e->stack[e->depth].count == 0) {
			e->depth--;
			return 0;
		}
		e->stack[e->depth].cont = o;
		e->stack[e->depth].obj = json_object_array_get_idx(o, 0);
		e->stack[e->depth].index = 0;
#if !defined(NO_OBJECT_ITERATION_FOR_MUSTACH)
		e->stack[e->depth].is_objiter = 0;
	} else if (json_object_is_type(o, json_type_object) && e->found_objiter) {
		e->stack[e->depth].biter = json_object_iter_begin(o);
		e->stack[e->depth].eiter = json_object_iter_end(o);
		if (json_object_iter_equal(&e->stack[e->depth].biter, &e->stack[e->depth].eiter)) {
			e->depth--;
			return 0;
		}
		e->stack[e->depth].obj = json_object_iter_peek_value(&e->stack[e->depth].biter);
		e->stack[e->depth].cont = o;
		e->stack[e->depth].is_objiter = 1;
#endif
	} else if (json_object_is_type(o, json_type_object) || json_object_get_boolean(o)) {
		e->stack[e->depth].count = 1;
		e->stack[e->depth].cont = NULL;
		e->stack[e->depth].obj = o;
		e->stack[e->depth].index = 0;
#if !defined(NO_OBJECT_ITERATION_FOR_MUSTACH)
		e->stack[e->depth].is_objiter = 0;
#endif
	} else {
		e->depth--;
		return 0;
	}
	return 1;
}

static int next(void *closure)
{
	struct expl *e = closure;
	if (e->depth <= 0)
		return MUSTACH_ERROR_CLOSING;
#if !defined(NO_OBJECT_ITERATION_FOR_MUSTACH)
	if (e->stack[e->depth].is_objiter) {
		json_object_iter_next(&e->stack[e->depth].biter);
		if (json_object_iter_equal(&e->stack[e->depth].biter, &e->stack[e->depth].eiter))
			return 0;
		e->stack[e->depth].obj = json_object_iter_peek_value(&e->stack[e->depth].biter);
		return 1;
	}
#endif
	e->stack[e->depth].index++;
	if (e->stack[e->depth].index >= e->stack[e->depth].count)
		return 0;
	e->stack[e->depth].obj = json_object_array_get_idx(e->stack[e->depth].cont, e->stack[e->depth].index);
	return 1;
}

static int leave(void *closure)
{
	struct expl *e = closure;
	if (e->depth <= 0)
		return MUSTACH_ERROR_CLOSING;
	e->depth--;
	return 0;
}

static int get_partial_from_file(const char *name, struct mustach_sbuf *sbuf)
{
	static char extension[] = ".mustache";
	int rc;
	size_t s;
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
		 && (s = (size_t)ftell(file)) >= 0
		 && fseek(file, 0, SEEK_SET) >= 0) {
			/* allocate value */
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
	struct expl *e = closure;
	const char *s;

	s = item(e, name);
	if (s)
		sbuf->value = s;
	else if (get_partial_from_file(name, sbuf) < 0)
		sbuf->value = "";
	return MUSTACH_OK;
}

static int get(void *closure, const char *name, struct mustach_sbuf *sbuf)
{
	struct expl *e = closure;
	const char *s;

	s = item(e, name);
	if (s)
		sbuf->value = s;
	else
		sbuf->value = "";
	return MUSTACH_OK;
}

static struct mustach_itf itf = {
	.start = start,
	.put = NULL,
	.enter = enter,
	.next = next,
	.leave = leave,
	.partial = partial,
	.get = get,
	.emit = NULL
};

static struct mustach_itf itfuw = {
	.start = start,
	.put = NULL,
	.enter = enter,
	.next = next,
	.leave = leave,
	.partial = partial,
	.get = get,
	.emit = emituw
};

int fmustach_json_c(const char *template, struct json_object *root, FILE *file)
{
	struct expl e;
	e.root = root;
	return fmustach(template, &itf, &e, file);
}

int fdmustach_json_c(const char *template, struct json_object *root, int fd)
{
	struct expl e;
	e.root = root;
	return fdmustach(template, &itf, &e, fd);
}

int mustach_json_c(const char *template, struct json_object *root, char **result, size_t *size)
{
	struct expl e;
	e.root = root;
	e.writecb = NULL;
	return mustach(template, &itf, &e, result, size);
}

int umustach_json_c(const char *template, struct json_object *root, mustach_json_c_write_cb writecb, void *closure)
{
	struct expl e;
	e.root = root;
	e.writecb = writecb;
	return fmustach(template, &itfuw, &e, closure);
}

