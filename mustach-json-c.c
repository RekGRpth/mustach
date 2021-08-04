/*
 Author: José Bollo <jobol@nonadev.net>

 https://gitlab.com/jobol/mustach

 SPDX-License-Identifier: ISC
*/

#define _GNU_SOURCE

#include <stdio.h>
#include <string.h>

#include "mustach.h"
#include "mustach-wrap.h"
#include "mustach-json-c.h"

struct expl {
	struct json_object *root;
	struct json_object *selection;
	int depth;
	struct {
		struct json_object *cont;
		struct json_object *obj;
		struct json_object_iterator iter;
		struct json_object_iterator enditer;
		int is_objiter;
		int index, count;
	} stack[MUSTACH_MAX_DEPTH];
};

static int start(void *closure)
{
	struct expl *e = closure;
	e->depth = 0;
	e->selection = NULL;
	e->stack[0].cont = NULL;
	e->stack[0].obj = e->root;
	e->stack[0].index = 0;
	e->stack[0].count = 1;
	return MUSTACH_OK;
}

static int compare(void *closure, const char *value)
{
	struct expl *e = closure;
	struct json_object *o = e->selection;
	double d;
	int64_t i;

	switch (json_object_get_type(o)) {
	case json_type_double:
		d = json_object_get_double(o) - atof(value);
		return d < 0 ? -1 : d > 0 ? 1 : 0;
	case json_type_int:
		i = json_object_get_int64(o) - (int64_t)atoll(value);
		return i < 0 ? -1 : i > 0 ? 1 : 0;
	default:
		return strcmp(json_object_get_string(o), value);
	}
}

static int sel(void *closure, const char *name)
{
	struct expl *e = closure;
	struct json_object *o;
	int i, r;

	if (name == NULL) {
		o = e->stack[e->depth].obj;
		r = 1;
	} else {
		i = e->depth;
		while (i >= 0 && !json_object_object_get_ex(e->stack[i].obj, name, &o))
			i--;
		if (i >= 0)
			r = 1;
		else {
			o = NULL;
			r = 0;
		}
	}
	e->selection = o;
	return r;
}

static int subsel(void *closure, const char *name)
{
	struct expl *e = closure;
	struct json_object *o;
	int r;

	r = json_object_object_get_ex(e->selection, name, &o);
	if (r)
		e->selection = o;
	return r;
}

static int enter(void *closure, int objiter)
{
	struct expl *e = closure;
	struct json_object *o;

	if (++e->depth >= MUSTACH_MAX_DEPTH)
		return MUSTACH_ERROR_TOO_DEEP;

	o = e->selection;
	e->stack[e->depth].is_objiter = 0;
	if (objiter) {
		if (!json_object_is_type(o, json_type_object))
			goto not_entering;

		e->stack[e->depth].iter = json_object_iter_begin(o);
		e->stack[e->depth].enditer = json_object_iter_end(o);
		if (json_object_iter_equal(&e->stack[e->depth].iter, &e->stack[e->depth].enditer))
			goto not_entering;
		e->stack[e->depth].obj = json_object_iter_peek_value(&e->stack[e->depth].iter);
		e->stack[e->depth].cont = o;
		e->stack[e->depth].is_objiter = 1;
	} else if (json_object_is_type(o, json_type_array)) {
		e->stack[e->depth].count = json_object_array_length(o);
		if (e->stack[e->depth].count == 0)
			goto not_entering;
		e->stack[e->depth].cont = o;
		e->stack[e->depth].obj = json_object_array_get_idx(o, 0);
		e->stack[e->depth].index = 0;
	} else if (json_object_is_type(o, json_type_object) || json_object_get_boolean(o)) {
		e->stack[e->depth].count = 1;
		e->stack[e->depth].cont = NULL;
		e->stack[e->depth].obj = o;
		e->stack[e->depth].index = 0;
	} else
		goto not_entering;
	return 1;

not_entering:
	e->depth--;
	return 0;
}

static int next(void *closure)
{
	struct expl *e = closure;

	if (e->depth <= 0)
		return MUSTACH_ERROR_CLOSING;

	if (e->stack[e->depth].is_objiter) {
		json_object_iter_next(&e->stack[e->depth].iter);
		if (json_object_iter_equal(&e->stack[e->depth].iter, &e->stack[e->depth].enditer))
			return 0;
		e->stack[e->depth].obj = json_object_iter_peek_value(&e->stack[e->depth].iter);
		return 1;
	}

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

static int get(void *closure, struct mustach_sbuf *sbuf, int key)
{
	struct expl *e = closure;
	const char *s;

	if (key)
		s = e->stack[e->depth].is_objiter
			? json_object_iter_peek_name(&e->stack[e->depth].iter)
			: "";
	else
		switch (json_object_get_type(e->selection)) {
		case json_type_string:
			s = json_object_get_string(e->selection);
			break;
		case json_type_null:
			s = "";
			break;
		default:
			s = json_object_to_json_string_ext(e->selection, 0);
			break;
		}
	sbuf->value = s;
	return 1;
}

const struct mustach_wrap_itf mustach_json_c_wrap_itf = {
	.start = start,
	.stop = NULL,
	.compare = compare,
	.sel = sel,
	.subsel = subsel,
	.enter = enter,
	.next = next,
	.leave = leave,
	.get = get
};

int mustach_json_c_file(const char *template, size_t length, struct json_object *root, int flags, FILE *file)
{
	struct expl e;
	e.root = root;
	return mustach_wrap_file(template, length, &mustach_json_c_wrap_itf, &e, flags, file);
}

int mustach_json_c_fd(const char *template, size_t length, struct json_object *root, int flags, int fd)
{
	struct expl e;
	e.root = root;
	return mustach_wrap_fd(template, length, &mustach_json_c_wrap_itf, &e, flags, fd);
}

int mustach_json_c_mem(const char *template, size_t length, struct json_object *root, int flags, char **result, size_t *size)
{
	struct expl e;
	e.root = root;
	return mustach_wrap_mem(template, length, &mustach_json_c_wrap_itf, &e, flags, result, size);
}

int mustach_json_c_write(const char *template, size_t length, struct json_object *root, int flags, mustach_write_cb_t *writecb, void *closure)
{
	struct expl e;
	e.root = root;
	return mustach_wrap_write(template, length, &mustach_json_c_wrap_itf, &e, flags, writecb, closure);
}

int mustach_json_c_emit(const char *template, size_t length, struct json_object *root, int flags, mustach_emit_cb_t *emitcb, void *closure)
{
	struct expl e;
	e.root = root;
	return mustach_wrap_emit(template, length, &mustach_json_c_wrap_itf, &e, flags, emitcb, closure);
}

int fmustach_json_c(const char *template, struct json_object *root, FILE *file)
{
	return mustach_json_c_file(template, 0, root, -1, file);
}

int fdmustach_json_c(const char *template, struct json_object *root, int fd)
{
	return mustach_json_c_fd(template, 0, root, -1, fd);
}

int mustach_json_c(const char *template, struct json_object *root, char **result, size_t *size)
{
	return mustach_json_c_mem(template, 0, root, -1, result, size);
}

int umustach_json_c(const char *template, struct json_object *root, mustach_write_cb_t *writecb, void *closure)
{
	return mustach_json_c_write(template, 0, root, -1, writecb, closure);
}


