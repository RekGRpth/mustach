/*
 Author: José Bollo <jobol@nonadev.net>

 https://gitlab.com/jobol/mustach

 SPDX-License-Identifier: 0BSD
*/

#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif

#include "mustach.h"
#include "mustach-wrap.h"
#include "mustach-jansson.h"

#include <stdio.h>
#include <string.h>

struct expl {
	json_t *root;
	json_t *selection;
	int depth;
	struct {
		json_t *cont;
		json_t *obj;
		void *iter;
		int is_objiter;
		size_t index, count;
	} stack[MUSTACH_MAX_DEPTH];
};

static int start(void *closure)
{
	struct expl *e = closure;
	e->depth = 0;
	e->selection = json_null();
	e->stack[0].cont = NULL;
	e->stack[0].obj = e->root;
	e->stack[0].index = 0;
	e->stack[0].count = 1;
	return MUSTACH_OK;
}

static int compare(void *closure, const char *value)
{
	struct expl *e = closure;
	json_t *o = e->selection;
	double d;
	json_int_t i;

	switch (json_typeof(o)) {
	case JSON_REAL:
		d = json_number_value(o) - atof(value);
		return d < 0 ? -1 : d > 0 ? 1 : 0;
	case JSON_INTEGER:
		i = (json_int_t)json_integer_value(o) - (json_int_t)atoll(value);
		return i < 0 ? -1 : i > 0 ? 1 : 0;
	case JSON_STRING:
		return strcmp(json_string_value(o), value);
	case JSON_TRUE:
		return strcmp("true", value);
	case JSON_FALSE:
		return strcmp("false", value);
	case JSON_NULL:
		return strcmp("null", value);
	default:
		return 1;
	}
}

static int sel(void *closure, const char *name)
{
	struct expl *e = closure;
	json_t *o;
	int i, r;

	if (name == NULL) {
		o = e->stack[e->depth].obj;
		r = 1;
	} else {
		i = e->depth;
		while (i >= 0 && !(o = json_object_get(e->stack[i].obj, name)))
			i--;
		if (i >= 0)
			r = 1;
		else {
			o = json_null();
			r = 0;
		}
	}
	e->selection = o;
	return r;
}

static int subsel(void *closure, const char *name)
{
	struct expl *e = closure;
	json_t *o = NULL;
	int r = 0;

	if (json_is_object(e->selection)) {
		o = json_object_get(e->selection, name);
		r = o != NULL;
	}
	else if (json_is_array(e->selection)) {
		char *end;
		size_t idx = (size_t)strtol(name, &end, 10);
		if (!*end && idx < json_array_size(e->selection)) {
			o = json_array_get(e->selection, idx);
			r = 1;
		}
	}
	if (r)
		e->selection = o;
	return r;
}

static int enter(void *closure, int objiter)
{
	struct expl *e = closure;
	json_t *o;

	if (++e->depth >= MUSTACH_MAX_DEPTH)
		return MUSTACH_ERROR_TOO_DEEP;

	o = e->selection;
	e->stack[e->depth].is_objiter = 0;
	if (objiter) {
		if (!json_is_object(o))
			goto not_entering;
		e->stack[e->depth].iter = json_object_iter(o);
		if (e->stack[e->depth].iter == NULL)
			goto not_entering;
		e->stack[e->depth].obj = json_object_iter_value(e->stack[e->depth].iter);
		e->stack[e->depth].cont = o;
		e->stack[e->depth].is_objiter = 1;
	} else if (json_is_array(o)) {
		e->stack[e->depth].count = json_array_size(o);
		if (e->stack[e->depth].count == 0)
			goto not_entering;
		e->stack[e->depth].cont = o;
		e->stack[e->depth].obj = json_array_get(o, 0);
		e->stack[e->depth].index = 0;
	} else if ((json_is_object(o) && json_object_size(o))
                || json_is_true(o)
	        || (json_is_string(o) && json_string_length(o) > 0)
	        || (json_is_integer(o) && json_integer_value(o) != 0)
	        || (json_is_real(o) && json_real_value(o) != 0)) {
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
		e->stack[e->depth].iter = json_object_iter_next(e->stack[e->depth].cont, e->stack[e->depth].iter);
		if (e->stack[e->depth].iter == NULL)
			return 0;
		e->stack[e->depth].obj = json_object_iter_value(e->stack[e->depth].iter);
		return 1;
	}

	e->stack[e->depth].index++;
	if (e->stack[e->depth].index >= e->stack[e->depth].count)
		return 0;

	e->stack[e->depth].obj = json_array_get(e->stack[e->depth].cont, e->stack[e->depth].index);
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
	int d;

	if (key) {
		s = "";
		for (d = e->depth ; d >= 0 ; d--)
			if (e->stack[d].is_objiter) {
				s = json_object_iter_key(e->stack[d].iter);
				break;
			}
	}
	else if (json_is_string(e->selection))
		s = json_string_value(e->selection);
	else if (json_is_null(e->selection))
		s = "";
	else {
		s = json_dumps(e->selection, JSON_ENCODE_ANY | JSON_COMPACT);
		if (s == NULL)
			return MUSTACH_ERROR_SYSTEM;
		sbuf->freecb = free;
	}
	sbuf->value = s;
	return 1;
}

const struct mustach_wrap_itf mustach_jansson_wrap_itf = {
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

int mustach_jansson_file(const char *templstr, size_t length, json_t *root, int flags, FILE *file)
{
	struct expl e;
	e.root = root;
	return mustach_wrap_file(templstr, length, &mustach_jansson_wrap_itf, &e, flags, file);
}

int mustach_jansson_fd(const char *templstr, size_t length, json_t *root, int flags, int fd)
{
	struct expl e;
	e.root = root;
	return mustach_wrap_fd(templstr, length, &mustach_jansson_wrap_itf, &e, flags, fd);
}

int mustach_jansson_mem(const char *templstr, size_t length, json_t *root, int flags, char **result, size_t *size)
{
	struct expl e;
	e.root = root;
	return mustach_wrap_mem(templstr, length, &mustach_jansson_wrap_itf, &e, flags, result, size);
}

int mustach_jansson_write(const char *templstr, size_t length, json_t *root, int flags, mustach_write_cb_t *writecb, void *closure)
{
	struct expl e;
	e.root = root;
	return mustach_wrap_write(templstr, length, &mustach_jansson_wrap_itf, &e, flags, writecb, closure);
}

int mustach_jansson_emit(const char *templstr, size_t length, json_t *root, int flags, mustach_emit_cb_t *emitcb, void *closure)
{
	struct expl e;
	e.root = root;
	return mustach_wrap_emit(templstr, length, &mustach_jansson_wrap_itf, &e, flags, emitcb, closure);
}

int mustach_jansson_apply(
		mustach_template_t *templstr,
		json_t *root,
		int flags,
		mustach_write_cb_t *writecb,
		mustach_emit_cb_t *emitcb,
		void *closure
) {
	struct expl e;
	e.root = root;
	return mustach_wrap_apply(templstr, &mustach_jansson_wrap_itf, &e, flags, writecb, emitcb, closure);
}
