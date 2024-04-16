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
#include "mustach-cjson.h"

#include <stdio.h>
#include <string.h>
#include <stdlib.h>

struct expl {
	cJSON null;
	cJSON *root;
	cJSON *selection;
	int depth;
	struct {
		cJSON *cont;
		cJSON *obj;
		cJSON *next;
		int is_objiter;
	} stack[MUSTACH_MAX_DEPTH];
};

static int start(void *closure)
{
	struct expl *e = closure;
	e->depth = 0;
	memset(&e->null, 0, sizeof e->null);
	e->null.type = cJSON_NULL;
	e->selection = &e->null;
	e->stack[0].cont = NULL;
	e->stack[0].obj = e->root;
	return MUSTACH_OK;
}

static int compare(void *closure, const char *value)
{
	struct expl *e = closure;
	cJSON *o = e->selection;
	double d;

	if (cJSON_IsNumber(o)) {
		d = o->valuedouble - atof(value);
		return d < 0 ? -1 : d > 0 ? 1 : 0;
	} else if (cJSON_IsString(o)) {
		return strcmp(o->valuestring, value);
	} else if (cJSON_IsTrue(o)) {
		return strcmp("true", value);
	} else if (cJSON_IsFalse(o)) {
		return strcmp("false", value);
	} else if (cJSON_IsNull(o)) {
		return strcmp("null", value);
	} else {
		return 1;
	}
}

static int sel(void *closure, const char *name)
{
	struct expl *e = closure;
	cJSON *o;
	int i, r;

	if (name == NULL) {
		o = e->stack[e->depth].obj;
		r = 1;
	} else {
		i = e->depth;
		while (i >= 0 && !(o = cJSON_GetObjectItemCaseSensitive(e->stack[i].obj, name)))
			i--;
		if (i >= 0)
			r = 1;
		else {
			o = &e->null;
			r = 0;
		}
	}
	e->selection = o;
	return r;
}

static int subsel(void *closure, const char *name)
{
	struct expl *e = closure;
	cJSON *o = NULL;
	int r = 0;

	if (cJSON_IsObject(e->selection)) {
		o = cJSON_GetObjectItemCaseSensitive(e->selection, name);
		r = o != NULL;
	}
	else if (cJSON_IsArray(e->selection) && *name) {
		char *end;
		int idx = (int)strtol(name, &end, 10);
		if (!*end && idx >= 0 && idx < cJSON_GetArraySize(e->selection)) {
			o = cJSON_GetArrayItem(e->selection, idx);
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
	cJSON *o;

	if (++e->depth >= MUSTACH_MAX_DEPTH)
		return MUSTACH_ERROR_TOO_DEEP;

	o = e->selection;
	e->stack[e->depth].is_objiter = 0;
	if (objiter) {
		if (! cJSON_IsObject(o))
			goto not_entering;
		if (o->child == NULL)
			goto not_entering;
		e->stack[e->depth].obj = o->child;
		e->stack[e->depth].next = o->child->next;
		e->stack[e->depth].cont = o;
		e->stack[e->depth].is_objiter = 1;
	} else if (cJSON_IsArray(o)) {
		if (o->child == NULL)
			goto not_entering;
		e->stack[e->depth].obj = o->child;
		e->stack[e->depth].next = o->child->next;
		e->stack[e->depth].cont = o;
	} else if ((cJSON_IsObject(o) && o->child != NULL)
	        || cJSON_IsTrue(o)
	        || (cJSON_IsString(o) && cJSON_GetStringValue(o)[0] != '\0')
	        || (cJSON_IsNumber(o) && cJSON_GetNumberValue(o) != 0)) {
		e->stack[e->depth].obj = o;
		e->stack[e->depth].cont = NULL;
		e->stack[e->depth].next = NULL;
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
	cJSON *o;

	if (e->depth <= 0)
		return MUSTACH_ERROR_CLOSING;

	o = e->stack[e->depth].next;
	if (o == NULL)
		return 0;

	e->stack[e->depth].obj = o;
	e->stack[e->depth].next = o->next;
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
				s = e->stack[d].obj->string;
				break;
			}
	}
	else if (cJSON_IsString(e->selection))
		s = e->selection->valuestring;
	else if (cJSON_IsNull(e->selection))
		s = "";
	else {
		s = cJSON_PrintUnformatted(e->selection);
		if (s == NULL)
			return MUSTACH_ERROR_SYSTEM;
		sbuf->freecb = cJSON_free;
	}
	sbuf->value = s;
	return 1;
}

const struct mustach_wrap_itf mustach_cJSON_wrap_itf = {
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

int mustach_cJSON_file(const char *template, size_t length, cJSON *root, int flags, FILE *file)
{
	struct expl e;
	e.root = root;
	return mustach_wrap_file(template, length, &mustach_cJSON_wrap_itf, &e, flags, file);
}

int mustach_cJSON_fd(const char *template, size_t length, cJSON *root, int flags, int fd)
{
	struct expl e;
	e.root = root;
	return mustach_wrap_fd(template, length, &mustach_cJSON_wrap_itf, &e, flags, fd);
}

int mustach_cJSON_mem(const char *template, size_t length, cJSON *root, int flags, char **result, size_t *size)
{
	struct expl e;
	e.root = root;
	return mustach_wrap_mem(template, length, &mustach_cJSON_wrap_itf, &e, flags, result, size);
}

int mustach_cJSON_write(const char *template, size_t length, cJSON *root, int flags, mustach_write_cb_t *writecb, void *closure)
{
	struct expl e;
	e.root = root;
	return mustach_wrap_write(template, length, &mustach_cJSON_wrap_itf, &e, flags, writecb, closure);
}

int mustach_cJSON_emit(const char *template, size_t length, cJSON *root, int flags, mustach_emit_cb_t *emitcb, void *closure)
{
	struct expl e;
	e.root = root;
	return mustach_wrap_emit(template, length, &mustach_cJSON_wrap_itf, &e, flags, emitcb, closure);
}

