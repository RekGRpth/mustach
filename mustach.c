/*
 Author: José Bollo <jobol@nonadev.net>

 https://gitlab.com/jobol/mustach

 SPDX-License-Identifier: 0BSD
*/

#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif

#include "mustach2.h"
#include "mustach.h"
#include "mustach-helpers.h"

#include <stdint.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <ctype.h>
#ifdef _WIN32
#include <malloc.h>
#endif

#define USING_MINI_MUSTACH  0
#define USING_MUSTACH_V1    1
#define USING_MUSTACH_V2    2

/*
#define MUSTACH_USED USING_MUSTACH_V1
#define MUSTACH_USED USING_MUSTACH_V2
*/
#define MUSTACH_USED USING_MINI_MUSTACH
/**************************************************************************/
/**************************************************************************/
/** USING VERSION 2 *******************************************************/
/**************************************************************************/
/**************************************************************************/
#if MUSTACH_USED == USING_MUSTACH_V2

struct wrap
{
	const struct mustach_itf *itf;
	void *closure;
	FILE *file;
	int flags;
};

static int wrap_emit_esc(void *closure, const char *buffer, size_t size, int escape)
{
	struct wrap *wrap = closure;
	return wrap->itf->emit(wrap->closure, buffer, size, escape, wrap->file);
}

static int wrap_emit_raw(void *closure, const char *buffer, size_t size)
{
	struct wrap *wrap = closure;
	ssize_t r = fwrite(buffer, size, 1, wrap->file);
	return r == 1 ? MUSTACH_OK : MUSTACH_ERROR_SYSTEM;
}

static int wrap_enter(void *closure, const char *name, size_t length)
{
	struct wrap *wrap = closure;
	(void)length;/*make compiler happy #@!%!!*/
	return wrap->itf->enter(wrap->closure, name);
}

static int wrap_next(void *closure)
{
	struct wrap *wrap = closure;
	return wrap->itf->next(wrap->closure);
}

static int wrap_leave(void *closure)
{
	struct wrap *wrap = closure;
	return wrap->itf->leave(wrap->closure);
}

static int wrap_get(void *closure, const char *name, size_t length, struct mustach_sbuf *sbuf)
{
	struct wrap *wrap = closure;
	(void)length;/*make compiler happy #@!%!!*/
	return wrap->itf->get(wrap->closure, name, sbuf);
}

static int wrap_partial_get(void *closure, const char *name, size_t length, mustach_template_t **part)
{
	struct wrap *wrap = closure;
	mustach_sbuf_t sbuf = MUSTACH_SBUF_INIT;
	int rc = wrap->itf->partial(wrap->closure, name, &sbuf);
	(void)length;/*make compiler happy #@!%!!*/
	if (rc == MUSTACH_OK)
		rc = mustach_build_template(
				part,
				(wrap->flags & Mustach_With_AllExtensions) | Mustach_Build_Null_Term_Tag,
				&sbuf,
				NULL,
				0,
				NULL,/*wrap error*/
				NULL);
	return rc;
}

static void wrap_partial_put(void *closure, mustach_template_t *part)
{
	(void)closure;/*make compiler happy #@!%!!*/
	mustach_destroy_template(part, NULL, NULL);
}

int mustach_file(const char *template, size_t length, const struct mustach_itf *itf, void *closure, int flags, FILE *file)
{
	int rc, flags2;
	struct wrap wrap;
	struct mustach_apply_itf itf2;
	mustach_sbuf_t sbuf;
	mustach_template_t *templ;

	/* check validity */
	if (!itf->enter || !itf->next || !itf->leave || !itf->get)
		return MUSTACH_ERROR_INVALID_ITF;

	/* init wrap structure */
	wrap.itf = itf;
	wrap.closure = closure;
	wrap.file = file;
	wrap.flags = flags;

	/* init interface structure */
	memset(&itf2, 0, sizeof itf2);
	itf2.version = MUSTACH_APPLY_ITF_VERSION_1;
	if (itf->partial) {
		itf2.partial_get = wrap_partial_get;
		itf2.partial_put = wrap_partial_put;
	}
	if (itf->emit)
		itf2.emit_esc = wrap_emit_esc;
	else
		itf2.emit_raw = wrap_emit_raw;
	itf2.enter = wrap_enter;
	itf2.next = wrap_next;
	itf2.leave = wrap_leave;
	itf2.get = wrap_get;

	/* process */
	rc = itf->start ? itf->start(closure) : 0;
	if (rc == 0) {
		sbuf.value = template;
		sbuf.length = length;
		sbuf.freecb = NULL;

		flags2 = Mustach_Build_Null_Term_Tag;
		if ((flags & Mustach_With_Colon) != 0)
			flags2 |= Mustach_Build_With_Colon;
		if ((flags & Mustach_With_EmptyTag) != 0)
			flags2 |= Mustach_Build_With_EmptyTag;
		rc = mustach_make_template(&templ, flags2, &sbuf, NULL);
		if(rc == MUSTACH_OK) {
			rc = mustach_apply_template(templ, 0, &itf2, &wrap);
			mustach_destroy_template(templ, NULL, NULL);
		}
	}
	if (itf->stop)
		itf->stop(closure, rc);
	return rc;
}
#endif

/**************************************************************************/
/**************************************************************************/
/** USING LEGACY VERSION 1 ************************************************/
/**************************************************************************/
/**************************************************************************/
#if MUSTACH_USED == USING_MUSTACH_V1

/**
 * Maximum length of tags in mustaches {{...}}
 */
#define MUSTACH_MAX_LENGTH 4096

/**
 * Maximum length of delimitors (2 normally but extended here)
 */
#define MUSTACH_MAX_DELIM_LENGTH 8


struct iwrap {
	int (*emit)(void *closure, const char *buffer, size_t size, int escape, FILE *file);
	void *closure; /* closure for: enter, next, leave, emit, get */
	int (*put)(void *closure, const char *name, int escape, FILE *file);
	void *closure_put; /* closure for put */
	int (*enter)(void *closure, const char *name);
	int (*next)(void *closure);
	int (*leave)(void *closure);
	int (*get)(void *closure, const char *name, struct mustach_sbuf *sbuf);
	int (*partial)(void *closure, const char *name, struct mustach_sbuf *sbuf);
	void *closure_partial; /* closure for partial */
	FILE *file;
	int flags;
	int nesting;
};

struct prefix {
	size_t len;
	const char *start;
	struct prefix *prefix;
};

/*********************************************************
* Function for writing the indentation prefix
*********************************************************/
static int emitprefix(struct iwrap *iwrap, const struct prefix *prefix)
{
	if (prefix->prefix) {
		int rc = emitprefix(iwrap, prefix->prefix);
		if (rc < 0)
			return rc;
	}
	return prefix->len ? iwrap->emit(iwrap->closure, prefix->start, prefix->len, 0, iwrap->file) : 0;
}


/*********************************************************
* This section is for default implementations of
* optional callbacks that are although required
*********************************************************/
static int process(const char *template, size_t length, struct iwrap *iwrap, struct prefix *prefix)
{
	struct mustach_sbuf sbuf;
	char opstr[MUSTACH_MAX_DELIM_LENGTH], clstr[MUSTACH_MAX_DELIM_LENGTH];
	char name[MUSTACH_MAX_LENGTH + 1], c;
	const char *beg, *term, *end;
	struct { const char *name, *again; size_t length; unsigned enabled: 1, entered: 1; } stack[MUSTACH_MAX_DEPTH];
	size_t oplen, cllen, len, l;
	int depth, rc, enabled, stdalone;
	struct prefix pref;

	pref.prefix = prefix;
	end = template + (length ? length : strlen(template));
	opstr[0] = opstr[1] = '{';
	clstr[0] = clstr[1] = '}';
	oplen = cllen = 2;
	stdalone = enabled = 1;
	depth = pref.len = 0;
	for (;;) {
		/* search next openning delimiter */
		for (beg = template ; ; beg++) {
			if (beg == end)
				c = '\n';
			else {
				c = *beg;
				if (c == '\r') {
					if ((beg + 1) != end && beg[1] == '\n')
						beg++;
					c = '\n';
				}
			}
			if (c == '\n') {
				l = (beg != end) + (size_t)(beg - template);
				if (stdalone != 2 && enabled) {
					if (beg != template /* don't prefix empty lines */) {
						rc = emitprefix(iwrap, &pref);
						if (rc < 0)
							return rc;
					}
					rc = iwrap->emit(iwrap->closure, template, l, 0, iwrap->file);
					if (rc < 0)
						return rc;
				}
				if (beg == end) /* no more mustach */
					return depth ? MUSTACH_ERROR_UNEXPECTED_END : MUSTACH_OK;
				template += l;
				stdalone = 1;
				pref.len = 0;
				pref.prefix = prefix;
			}
			else if (!isspace(c)) {
				if (stdalone == 2 && enabled) {
					rc = emitprefix(iwrap, &pref);
					if (rc < 0)
						return rc;
					pref.len = 0;
					stdalone = 0;
					pref.prefix = NULL;
				}
				if (c == *opstr && end - beg >= (ssize_t)oplen) {
					for (l = 1 ; l < oplen && beg[l] == opstr[l] ; l++);
					if (l == oplen)
						break;
				}
				stdalone = 0;
			}
		}

		pref.start = template;
		pref.len = enabled ? (size_t)(beg - template) : 0;
		beg += oplen;

		/* search next closing delimiter */
		for (term = beg ; ; term++) {
			if (term == end)
				return MUSTACH_ERROR_UNEXPECTED_END;
			if (*term == *clstr && end - term >= (ssize_t)cllen) {
				for (l = 1 ; l < cllen && term[l] == clstr[l] ; l++);
				if (l == cllen)
					break;
			}
		}
		template = term + cllen;
		len = (size_t)(term - beg);
		c = *beg;
		switch(c) {
		case ':':
			stdalone = 0;
			if (iwrap->flags & Mustach_With_Colon)
				goto exclude_first;
			goto get_name;
		case '!':
		case '=':
			break;
		case '{':
			for (l = 0 ; l < cllen && clstr[l] == '}' ; l++);
			if (l < cllen) {
				if (!len || beg[len-1] != '}')
					return MUSTACH_ERROR_BAD_UNESCAPE_TAG;
				len--;
			} else {
				if (term[l] != '}')
					return MUSTACH_ERROR_BAD_UNESCAPE_TAG;
				template++;
			}
			c = '&';
			/*@fallthrough@*/
		case '&':
			stdalone = 0;
			/*@fallthrough@*/
		case '^':
		case '#':
		case '/':
		case '>':
exclude_first:
			beg++;
			len--;
			goto get_name;
		default:
			stdalone = 0;
get_name:
			while (len && isspace(beg[0])) { beg++; len--; }
			while (len && isspace(beg[len-1])) len--;
			if (len == 0 && !(iwrap->flags & Mustach_With_EmptyTag))
				return MUSTACH_ERROR_EMPTY_TAG;
			if (len > MUSTACH_MAX_LENGTH)
				return MUSTACH_ERROR_TAG_TOO_LONG;
			memcpy(name, beg, len);
			name[len] = 0;
			break;
		}
		if (stdalone)
			stdalone = 2;
		else if (enabled) {
			rc = emitprefix(iwrap, &pref);
			if (rc < 0)
				return rc;
			pref.len = 0;
			pref.prefix = NULL;
		}
		switch(c) {
		case '!':
			/* comment */
			/* nothing to do */
			break;
		case '=':
			/* defines delimiters */
			if (len < 5 || beg[len - 1] != '=')
				return MUSTACH_ERROR_BAD_SEPARATORS;
			beg++;
			len -= 2;
			while (len && isspace(*beg))
				beg++, len--;
			while (len && isspace(beg[len - 1]))
				len--;
			for (l = 0; l < len && !isspace(beg[l]) ; l++);
			if (l == len || l > MUSTACH_MAX_DELIM_LENGTH)
				return MUSTACH_ERROR_BAD_SEPARATORS;
			oplen = l;
			memcpy(opstr, beg, l);
			while (l < len && isspace(beg[l])) l++;
			if (l == len || len - l > MUSTACH_MAX_DELIM_LENGTH)
				return MUSTACH_ERROR_BAD_SEPARATORS;
			cllen = len - l;
			memcpy(clstr, beg + l, cllen);
			break;
		case '^':
		case '#':
			/* begin section */
			if (depth == MUSTACH_MAX_DEPTH)
				return MUSTACH_ERROR_TOO_DEEP;
			rc = enabled;
			if (rc) {
				rc = iwrap->enter(iwrap->closure, name);
				if (rc < 0)
					return rc;
			}
			stack[depth].name = beg;
			stack[depth].again = template;
			stack[depth].length = len;
			stack[depth].enabled = enabled != 0;
			stack[depth].entered = rc != 0;
			if ((c == '#') == (rc == 0))
				enabled = 0;
			depth++;
			break;
		case '/':
			/* end section */
			if (depth-- == 0 || len != stack[depth].length || memcmp(stack[depth].name, name, len))
				return MUSTACH_ERROR_CLOSING;
			rc = enabled && stack[depth].entered ? iwrap->next(iwrap->closure) : 0;
			if (rc < 0)
				return rc;
			if (rc) {
				template = stack[depth++].again;
			} else {
				enabled = stack[depth].enabled;
				if (enabled && stack[depth].entered)
					iwrap->leave(iwrap->closure);
			}
			break;
		case '>':
			/* partials */
			if (enabled) {
				if (iwrap->nesting >= MUSTACH_MAX_NESTING)
					rc = MUSTACH_ERROR_TOO_MUCH_NESTING;
				else {
					mustach_sbuf_reset(&sbuf);
					rc = iwrap->partial(iwrap->closure_partial, name, &sbuf);
					if (rc >= 0) {
						iwrap->nesting++;
						rc = process(sbuf.value, mustach_sbuf_length(&sbuf), iwrap, &pref);
						mustach_sbuf_release(&sbuf);
						iwrap->nesting--;
					}
				}
				if (rc < 0)
					return rc;
			}
			break;
		default:
			/* replacement */
			if (enabled) {
				rc = iwrap->put(iwrap->closure_put, name, c != '&', iwrap->file);
				if (rc < 0)
					return rc;
			}
			break;
		}
	}
}
/*********************************************************
* This section is for default implementations of
* optional callbacks that are although required
*********************************************************/
static int iwrap_emit(void *closure, const char *buffer, size_t size, int escape, FILE *file)
{
	size_t i, j, r;

	(void)closure; /* unused */

	if (!escape)
		return mustach_fwrite(file, buffer, size);
	return mustach_fwrite_escape(file, buffer, size);
}

static int iwrap_put(void *closure, const char *name, int escape, FILE *file)
{
	struct iwrap *iwrap = closure;
	int rc;
	struct mustach_sbuf sbuf;
	size_t length;

	mustach_sbuf_reset(&sbuf);
	rc = iwrap->get(iwrap->closure, name, &sbuf);
	if (rc >= 0) {
		length = mustach_sbuf_length(&sbuf);
		if (length)
			rc = iwrap->emit(iwrap->closure, sbuf.value, length, escape, file);
		mustach_sbuf_release(&sbuf);
	}
	return rc;
}

static int iwrap_partial(void *closure, const char *name, struct mustach_sbuf *sbuf)
{
	struct iwrap *iwrap = closure;
	int rc;
	FILE *file;
	size_t size;
	char *result;

	result = NULL;
	file = mustach_memfile_open(&result, &size);
	if (file == NULL)
		rc = MUSTACH_ERROR_SYSTEM;
	else {
		rc = iwrap->put(iwrap->closure_put, name, 0, file);
		if (rc < 0)
			mustach_memfile_abort(file, &result, &size);
		else {
			rc = mustach_memfile_close(file, &result, &size);
			if (rc == 0) {
				sbuf->value = result;
				sbuf->freecb = free;
				sbuf->length = size;
			}
		}
	}
	return rc;
}



/*********************************************************
* This section is for the public interface functions
*********************************************************/
int mustach_file(const char *template, size_t length, const struct mustach_itf *itf, void *closure, int flags, FILE *file)
{
	int rc;
	struct iwrap iwrap;

	/* check validity */
	if (itf == NULL || !itf->enter || !itf->next || !itf->leave || (!itf->put && !itf->get))
		return MUSTACH_ERROR_INVALID_ITF;

	/* init wrap structure */
	iwrap.closure = closure;
	if (itf->put) {
		iwrap.put = itf->put;
		iwrap.closure_put = closure;
	} else {
		iwrap.put = iwrap_put;
		iwrap.closure_put = &iwrap;
	}
	if (itf->partial) {
		iwrap.partial = itf->partial;
		iwrap.closure_partial = closure;
	} else if (itf->get) {
		iwrap.partial = itf->get;
		iwrap.closure_partial = closure;
	} else {
		iwrap.partial = iwrap_partial;
		iwrap.closure_partial = &iwrap;
	}
	iwrap.emit = itf->emit ? itf->emit : iwrap_emit;
	iwrap.enter = itf->enter;
	iwrap.next = itf->next;
	iwrap.leave = itf->leave;
	iwrap.get = itf->get;
	iwrap.file = file;
	iwrap.flags = flags;
	iwrap.nesting = 0;

	/* process */
	rc = itf->start ? itf->start(closure) : 0;
	if (rc == 0)
		rc = process(template, length, &iwrap, NULL);
	if (itf->stop)
		itf->stop(closure, rc);
	return rc;
}
#endif

/**************************************************************************/
/**************************************************************************/
/** USING MINI MUSTACH ****************************************************/
/**************************************************************************/
/**************************************************************************/
#if MUSTACH_USED == USING_MINI_MUSTACH
struct wrap
{
	const struct mustach_itf *itf;
	void *closure;
	FILE *file;
};

static int wrap_emit(void *closure, const char *buffer, size_t size, int escape)
{
	struct wrap *wrap = closure;

	if (wrap->itf->emit)
		return wrap->itf->emit(wrap->closure, buffer, size, escape, wrap->file);

	if (!escape)
		return mustach_fwrite(wrap->file, buffer, size);

	return mustach_fwrite_escape(wrap->file, buffer, size);
}

static int wrap_enter(void *closure, const char *name, size_t length)
{
	struct wrap *wrap = closure;
	char copy[length + 1];
	memcpy(copy, name, length);
	copy[length] = 0;
	return wrap->itf->enter(wrap->closure, copy);
}

static int wrap_next(void *closure)
{
	struct wrap *wrap = closure;
	return wrap->itf->next(wrap->closure);
}

static int wrap_leave(void *closure)
{
	struct wrap *wrap = closure;
	return wrap->itf->leave(wrap->closure);
}

static int wrap_get(void *closure, const char *name, size_t length, struct mustach_sbuf *sbuf)
{
	struct wrap *wrap = closure;
	char copy[length + 1];
	memcpy(copy, name, length);
	copy[length] = 0;
	return wrap->itf->get(wrap->closure, copy, sbuf);
}

static int wrap_partial(void *closure, const char *name, size_t length, struct mustach_sbuf *sbuf)
{
	struct wrap *wrap = closure;
	char copy[length + 1];
	memcpy(copy, name, length);
	copy[length] = 0;
	return wrap->itf->partial(wrap->closure, copy, sbuf);
}

int mustach_file(const char *template, size_t length, const struct mustach_itf *itf, void *closure, int flags, FILE *file)
{
	int rc;
	struct wrap wrap;
	struct mini_mustach_itf itf2;

	(void)flags;/*make compiler happy #@!%!!*/

	/* check validity */
	if (!itf->enter || !itf->next || !itf->leave || !itf->get || itf->put)
		return MUSTACH_ERROR_INVALID_ITF;

	/* init wrap structure */
	wrap.itf = itf;
	wrap.closure = closure;
	wrap.file = file;

	/* init interface structure */
	memset(&itf2, 0, sizeof itf2);
	itf2.emit = wrap_emit;
	itf2.get = wrap_get;
	itf2.enter = wrap_enter;
	itf2.next = wrap_next;
	itf2.leave = wrap_leave;
	if (itf->partial)
		itf2.partial = wrap_partial;

	/* process */
	rc = itf->start ? itf->start(closure) : 0;
	if (rc == 0)
		rc = mini_mustach(template, length, &itf2, &wrap);
	if (itf->stop)
		itf->stop(closure, rc);
	return rc;
}
#endif

/**************************************************************************/
/**************************************************************************/
/** COMMON PART ***********************************************************/
/**************************************************************************/
/**************************************************************************/
int mustach_fd(const char *template, size_t length, const struct mustach_itf *itf, void *closure, int flags, int fd)
{
	int rc;
	FILE *file;

	file = fdopen(fd, "w");
	if (file == NULL) {
		rc = MUSTACH_ERROR_SYSTEM;
		errno = ENOMEM;
	} else {
		rc = mustach_file(template, length, itf, closure, flags, file);
		fclose(file);
	}
	return rc;
}

int mustach_mem(const char *template, size_t length, const struct mustach_itf *itf, void *closure, int flags, char **result, size_t *size)
{
	int rc;
	FILE *file;
	size_t s;

	*result = NULL;
	if (size == NULL)
		size = &s;
	file = mustach_memfile_open(result, size);
	if (file == NULL)
		rc = MUSTACH_ERROR_SYSTEM;
	else {
		rc = mustach_file(template, length, itf, closure, flags, file);
		if (rc < 0)
			mustach_memfile_abort(file, result, size);
		else
			rc = mustach_memfile_close(file, result, size);
	}
	return rc;
}

int fmustach(const char *template, const struct mustach_itf *itf, void *closure, FILE *file)
{
	return mustach_file(template, 0, itf, closure, Mustach_With_AllExtensions, file);
}

int fdmustach(const char *template, const struct mustach_itf *itf, void *closure, int fd)
{
	return mustach_fd(template, 0, itf, closure, Mustach_With_AllExtensions, fd);
}

int mustach(const char *template, const struct mustach_itf *itf, void *closure, char **result, size_t *size)
{
	return mustach_mem(template, 0, itf, closure, Mustach_With_AllExtensions, result, size);
}

