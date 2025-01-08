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
#include "mustach-helpers.h"

#include <stdlib.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#ifdef _WIN32
#include <malloc.h>
#endif

#define USING_MINI_MUSTACH  0
#define USING_MUSTACH_V2    2

#define MUSTACH_USED USING_MUSTACH_V2
/*
#define MUSTACH_USED USING_MINI_MUSTACH
*/



/*
* It was stated that allowing to include files
* through template is not safe when the mustache
* template is open to any value because it could
* create leaks (example: {{>/etc/passwd}}).
*/
#if MUSTACH_SAFE
# undef MUSTACH_LOAD_TEMPLATE
#elif !defined(MUSTACH_LOAD_TEMPLATE)
# define MUSTACH_LOAD_TEMPLATE 1
#endif

#if !defined(INCLUDE_PARTIAL_EXTENSION)
# define INCLUDE_PARTIAL_EXTENSION ".mustache"
#endif

/* global hook for partials */
int (*mustach_wrap_get_partial)(const char *name, struct mustach_sbuf *sbuf) = NULL;

/* internal structure for wrapping */
struct wrap {
	/* original interface */
	const struct mustach_wrap_itf *itf;

	/* original closure */
	void *closure;

	/* flags */
	int flags;

	/* emiter callback */
	mustach_emit_cb_t *emitcb;

	/* write callback */
	mustach_write_cb_t *writecb;

	/* extra closure for emiter or write callback */
	void *wrclosure;

	/* the main template */
	mustach_template_t *templ;
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

static enum comp getcomp(char *head, int sflags)
{
	return (head[0] == '=' && (sflags & Mustach_With_Equal)) ? C_eq
		: (head[0] == '<' && (sflags & Mustach_With_Compare)) ? (head[1] == '=' ? C_le : C_lt)
		: (head[0] == '>' && (sflags & Mustach_With_Compare)) ? (head[1] == '=' ? C_ge : C_gt)
		: C_no;
}

static char *keyval(char *head, int sflags, enum comp *comp)
{
	char *w, car, escaped;
	enum comp k;

	k = C_no;
	w = head;
	car = *head;
	escaped = (sflags & Mustach_With_EscFirstCmp) && (getcomp(head, sflags) != C_no);
	while (car && (escaped || (k = getcomp(head, sflags)) == C_no)) {
		if (escaped)
			escaped = 0;
		else
			escaped = ((sflags & Mustach_With_JsonPointer) ? car == '~' : car == '\\')
			    && (getcomp(head + 1, sflags) != C_no);
		if (!escaped)
			*w++ = car;
		head++;
		car = *head;
	}
	*w = 0;
	*comp = k;
	return k == C_no ? NULL : &head[k & 3];
}

static char *getkey(char **head, int sflags)
{
	char *result, *iter, *write, car;

	car = *(iter = *head);
	if (!car)
		result = NULL;
	else {
		result = write = iter;
		if (sflags & Mustach_With_JsonPointer)
		{
			while (car && car != '/') {
				if (car == '~')
					switch (iter[1]) {
					case '1': car = '/'; /*@fallthrough@*/
					case '0': iter++;
					}
				*write++ = car;
				car = *++iter;
			}
			*write = 0;
			while (car == '/')
				car = *++iter;
		}
		else
		{
			while (car && car != '.') {
				if (car == '\\' && (iter[1] == '.' || iter[1] == '\\'))
					car = *++iter;
				*write++ = car;
				car = *++iter;
			}
			*write = 0;
			while (car == '.')
				car = *++iter;
		}
		*head = iter;
	}
	return result;
}

static enum sel sel(struct wrap *w, const char *name, size_t length)
{
	enum sel result;
	int i, j, sflags, scmp;
	char *key, *value;
	enum comp k;

	/* make a local writeable copy */
	char buffer[1 + length];
	char *copy = buffer;
	memcpy(copy, name, length);
	copy[length] = 0;

	/* check if matches json pointer selection */
	sflags = w->flags;
	if (sflags & Mustach_With_JsonPointer) {
		if (copy[0] == '/')
			copy++;
		else
			sflags ^= Mustach_With_JsonPointer;
	}

	/* extract the value, translate the key and get the comparator */
	if (sflags & (Mustach_With_Equal | Mustach_With_Compare))
		value = keyval(copy, sflags, &k);
	else {
		k = C_no;
		value = NULL;
	}

	/* case of . alone if Mustach_With_SingleDot? */
	if (copy[0] == '.' && copy[1] == 0 /*&& (sflags & Mustach_With_SingleDot)*/)
		/* yes, select current */
		result = w->itf->sel(w->closure, NULL) ? S_ok : S_none;
	else
	{
		/* not the single dot, extract the first key */
		key = getkey(&copy, sflags);
		if (key == NULL)
			return 0;

		/* select the root item */
		if (w->itf->sel(w->closure, key))
			result = S_ok;
		else if (key[0] == '*'
		      && !key[1]
		      && !value
		      && !*copy
		      && (w->flags & Mustach_With_ObjectIter)
		      && w->itf->sel(w->closure, NULL))
			result = S_ok_or_objiter;
		else
			result = S_none;
		if (result == S_ok) {
			/* iterate the selection of sub items */
			key = getkey(&copy, sflags);
			while(result == S_ok && key) {
				if (w->itf->subsel(w->closure, key))
					/* nothing */;
				else if (key[0] == '*'
				      && !key[1]
				      && !value
				      && !*copy
				      && (w->flags & Mustach_With_ObjectIter))
					result = S_objiter;
				else
					result = S_none;
				key = getkey(&copy, sflags);
			}
		}
	}
	/* should it be compared? */
	if (result == S_ok && value) {
		if (!w->itf->compare)
			result = S_none;
		else {
			i = value[0] == '!';
			scmp = w->itf->compare(w->closure, &value[i]);
			switch (k) {
			case C_eq: j = scmp == 0; break;
			case C_lt: j = scmp < 0; break;
			case C_le: j = scmp <= 0; break;
			case C_gt: j = scmp > 0; break;
			case C_ge: j = scmp >= 0; break;
			default: j = i; break;
			}
			if (i == j)
				result = S_none;
		}
	}
	return result;
}

static int enter_cb(void *closure, const char *name, size_t length)
{
	struct wrap *w = closure;
	enum sel s = sel(w, name, length);
	return s == S_none ? 0 : w->itf->enter(w->closure, s & S_objiter);
}

static int next_cb(void *closure)
{
	struct wrap *w = closure;
	return w->itf->next(w->closure);
}

static int leave_cb(void *closure)
{
	struct wrap *w = closure;
	return w->itf->leave(w->closure);
}

static int getoptional(struct wrap *w, const char *name, size_t length, struct mustach_sbuf *sbuf)
{
	enum sel s = sel(w, name, length);
	if (!(s & S_ok))
		return 0;
	return w->itf->get(w->closure, sbuf, s & S_objiter);
}

static int get_cb(void *closure, const char *name, size_t length, struct mustach_sbuf *sbuf)
{
	struct wrap *w = closure;
	if (getoptional(w, name, length, sbuf) <= 0) {
		if (w->flags & Mustach_With_ErrorUndefined)
			return MUSTACH_ERROR_UNDEFINED_TAG;
		sbuf->value = "";
	}
	return MUSTACH_OK;
}

#if MUSTACH_LOAD_TEMPLATE
static int get_partial_from_file(const char *name, size_t length, struct mustach_sbuf *sbuf)
{
	static char extension[] = INCLUDE_PARTIAL_EXTENSION;
	char path[length + sizeof extension];
	int rc;

	/* try without extension first */
	memcpy(path, name, length);
	path[length] = 0;
	rc = mustach_read_file(path, sbuf);
	if (rc != MUSTACH_OK) {
		memcpy(&path[length], extension, sizeof extension);
		rc = mustach_read_file(path, sbuf);
	}
	return rc == MUSTACH_OK ? rc : MUSTACH_ERROR_NOT_FOUND;
}
#endif

static int get_partial_buf(
		struct wrap *w,
		const char *name,
		size_t length,
		struct mustach_sbuf *sbuf
) {
	int rc;
	if (mustach_wrap_get_partial != NULL) {
		char path[length + 1];
		memcpy(path, name, length);
		path[length] = 0;
		rc = mustach_wrap_get_partial(path, sbuf);
		if (rc != MUSTACH_ERROR_NOT_FOUND) {
			if (rc != MUSTACH_OK)
				sbuf->value = "";
			return rc;
		}
	}
#if MUSTACH_LOAD_TEMPLATE
	if (w->flags & Mustach_With_PartialDataFirst) {
		if (getoptional(w, name, length, sbuf) > 0)
			rc = MUSTACH_OK;
		else
			rc = get_partial_from_file(name, length, sbuf);
	}
	else {
		rc = get_partial_from_file(name, length, sbuf);
		if (rc != MUSTACH_OK &&  getoptional(w, name, 0, sbuf) > 0)
			rc = MUSTACH_OK;
	}
#else
	rc = getoptional(w, name, length, sbuf) > 0 ?  MUSTACH_OK : MUSTACH_ERROR_NOT_FOUND;
#endif
	if (rc != MUSTACH_OK)
		sbuf->value = "";
	return MUSTACH_OK;
}

static int start_cb(void *closure)
{
	struct wrap *w = closure;
	return w->itf->start ? w->itf->start(w->closure) : MUSTACH_OK;
}

static void stop_cb(void *closure, int status)
{
	struct wrap *w = closure;
	if (w->itf->stop)
		w->itf->stop(w->closure, status);
}

static int emit_raw_cb(void *closure, const char *buffer, size_t size)
{
	struct wrap *w = closure;
	return w->writecb != NULL
		? w->writecb(w->wrclosure, buffer, size)
		: w->emitcb(w->wrclosure, buffer, size, 0);
}

static int emit_esc_cb(void *closure, const char *buffer, size_t size, int escape)
{
	struct wrap *w = closure;
	return w->emitcb != NULL
		? w->emitcb(w->wrclosure, buffer, size, escape)
		: escape
			? mustach_escape(buffer, size, w->writecb, w->wrclosure)
			: w->writecb(w->wrclosure, buffer, size);
}

static int partial_get_cb(
		void *closure,
		const char *name,
		size_t length,
		mustach_template_t **partial
) {
	struct wrap *w = closure;
	struct mustach_sbuf sbuf = MUSTACH_SBUF_INIT;
	int rc = get_partial_buf(w, name, length, &sbuf);
	if (rc == MUSTACH_OK)
		rc = mustach_make_template(partial, 0, &sbuf, NULL);
	return rc;
}

static void partial_put_cb(void *closure, mustach_template_t *partial)
{
	(void)closure;/*make compiler happy #@!%!!*/
	mustach_destroy_template(partial, NULL, NULL);
}

static const struct mustach_apply_itf itfw = {
	.version = MUSTACH_APPLY_ITF_VERSION_1,
	.error = NULL,
	.start = start_cb,
	.stop = stop_cb,
	.emit_raw = emit_raw_cb,
	.emit_esc = emit_esc_cb,
	.get = get_cb,
	.enter = enter_cb,
	.next = next_cb,
	.leave = leave_cb,
	.partial_get = partial_get_cb,
	.partial_put = partial_put_cb
};

int mustach_wrap_apply(
		mustach_template_t *templstr,
		const struct mustach_wrap_itf *itf,
		void *closure,
		int flags,
		mustach_write_cb_t *writecb,
		mustach_emit_cb_t *emitcb,
		void *wrclosure
) {
	int afl;
	struct wrap wrap;

	/* init the wrap data */
	if (flags & Mustach_With_Compare)
		flags |= Mustach_With_Equal;
	wrap.templ = templstr;
	wrap.itf = itf;
	wrap.closure = closure;
	wrap.flags = flags;
	wrap.emitcb = emitcb;
	wrap.writecb = writecb;
	wrap.wrclosure = wrclosure;

	/* apply the template */
	afl = 0;
	if ((flags & Mustach_With_PartialDataFirst) == 0)
		afl |= Mustach_Apply_GlobalPartialFirst;
	return mustach_apply_template(wrap.templ, afl, &itfw, &wrap);
}

/**************************************************************************/
/**************************************************************************/
/** USING VERSION 2 *******************************************************/
/**************************************************************************/
/**************************************************************************/
#if MUSTACH_USED == USING_MUSTACH_V2

static int get_template(mustach_template_t **templ, int flags, const char *templstr, size_t length)
{
	int flags2;
	mustach_sbuf_t sbuf;

	sbuf.value = templstr;
	sbuf.length = length;
	sbuf.freecb = NULL;

	flags2 = 0;
	if ((flags & Mustach_With_Colon) != 0)
		flags2 |= Mustach_Build_With_Colon;
	if ((flags & Mustach_With_EmptyTag) != 0)
		flags2 |= Mustach_Build_With_EmptyTag;

	return mustach_make_template(templ, flags2, &sbuf, NULL);
}

static int dowrap(
		const char *templstr,
		size_t length,
		const struct mustach_wrap_itf *itf,
		void *closure,
		int flags,
		mustach_write_cb_t *writecb,
		mustach_emit_cb_t *emitcb,
		void *wrclosure
) {
	int rc;
	mustach_template_t *templ;

	/* prepare the template */
	rc = get_template(&templ, flags, templstr, length);
	if (rc == MUSTACH_OK) {
		rc = mustach_wrap_apply(templ, itf, closure, flags, writecb, emitcb, wrclosure);
		mustach_destroy_template(templ, NULL, NULL);
	}
	return rc;
}
#endif
/**************************************************************************/
/**************************************************************************/
/** USING MINI MUSTACH ****************************************************/
/**************************************************************************/
/**************************************************************************/
#if MUSTACH_USED == USING_MINI_MUSTACH

static int emit_cb(void *closure, const char *buffer, size_t size, int escape)
{
	struct wrap *wrap = closure;

	if (wrap->emitcb)
		return wrap->emitcb(wrap->wrclosure, buffer, size, escape);

	if (!escape)
		return wrap->writecb(wrap->wrclosure, buffer, size);

	return mustach_escape(buffer, size, wrap->writecb, wrap->wrclosure);
}

static int partial_cb(void *closure, const char *name, size_t length, struct mustach_sbuf *sbuf)
{
	struct wrap *w = closure;
	return get_partial_buf(w, name, length, sbuf);
}

static const mini_mustach_itf_t mini_itf = {
	.emit = emit_cb,
	.get = get_cb,
	.enter = enter_cb,
	.next = next_cb,
	.leave = leave_cb,
	.partial = partial_cb,
};

static int dowrap(
		const char *templstr,
		size_t length,
		const struct mustach_wrap_itf *itf,
		void *closure,
		int flags,
		mustach_write_cb_t *writecb,
		mustach_emit_cb_t *emitcb,
		void *wrclosure
) {
	int rc;
	struct wrap wrap;

	/* init the wrap data */
	if (flags & Mustach_With_Compare)
		flags |= Mustach_With_Equal;
	wrap.itf = itf;
	wrap.closure = closure;
	wrap.flags = flags;
	wrap.emitcb = emitcb;
	wrap.writecb = writecb;
	wrap.wrclosure = wrclosure;

	/* apply the template */
	rc = wrap.itf->start == NULL ? MUSTACH_OK : wrap.itf->start(wrap.closure);
	if (rc == MUSTACH_OK)
		rc = mini_mustach(templstr, length, &mini_itf, &wrap);
	if (wrap.itf->stop)
		wrap.itf->stop(wrap.closure, rc);
	return rc;
}
#endif

int mustach_wrap_file(const char *templstr, size_t length, const struct mustach_wrap_itf *itf, void *closure, int flags, FILE *file)
{
	return dowrap(templstr, length, itf, closure, flags, mustach_fwrite_cb, NULL, file);
}

int mustach_wrap_fd(const char *templstr, size_t length, const struct mustach_wrap_itf *itf, void *closure, int flags, int fd)
{
	return dowrap(templstr, length, itf, closure, flags, mustach_write_cb, NULL, (void*)(intptr_t)fd);
}

int mustach_wrap_mem(const char *templstr, size_t length, const struct mustach_wrap_itf *itf, void *closure, int flags, char **result, size_t *size)
{
	mustach_stream_t stream = MUSTACH_STREAM_INIT;
	int rc = dowrap(templstr, length, itf, closure, flags, mustach_stream_write_cb, NULL, &stream);
	if (rc == MUSTACH_OK)
		mustach_stream_end(&stream, result, size);
	else
		mustach_stream_abort(&stream);
	return rc;
}

int mustach_wrap_write(const char *templstr, size_t length, const struct mustach_wrap_itf *itf, void *closure, int flags, mustach_write_cb_t *writecb, void *writeclosure)
{
	return dowrap(templstr, length, itf, closure, flags, writecb, NULL, writeclosure);
}

int mustach_wrap_emit(const char *templstr, size_t length, const struct mustach_wrap_itf *itf, void *closure, int flags, mustach_emit_cb_t *emitcb, void *emitclosure)
{
	return dowrap(templstr, length, itf, closure, flags, NULL, emitcb, emitclosure);
}

