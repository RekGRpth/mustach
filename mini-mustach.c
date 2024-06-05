/*
 Author: José Bollo <jobol@nonadev.net>

 https://gitlab.com/jobol/mustach

 SPDX-License-Identifier: 0BSD
*/

#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif

#include "mini-mustach.h"

#include <string.h>
#include <ctype.h>

/**
 * Accept empty tags
 */
#ifndef MINI_MUSTACH_WITH_EMPTY_TAG
#define MINI_MUSTACH_WITH_EMPTY_TAG 1
#endif

/**
 * Treat colon first character as escaping
 */
#ifndef MINI_MUSTACH_WITH_COLON
#define MINI_MUSTACH_WITH_COLON 1
#endif

/**
 * reporting error line
 */
#ifndef MINI_MUSTACH_REPORT_LINE_ERROR
#define MINI_MUSTACH_REPORT_LINE_ERROR 1
#endif

struct prefix {
	size_t len;
	const char *start;
	struct prefix *prefix;
};

struct iwrap {
	mini_mustach_itf_t itf;
	void *closure;
	int nesting;
};

/*********************************************************
* This section has functions for managing instances of mustach_sbuf_t
*********************************************************/

static inline void mustach_sbuf_reset(mustach_sbuf_t *sbuf)
{
	*sbuf = (mustach_sbuf_t){ NULL, { NULL }, NULL, 0 };
}

static inline void mustach_sbuf_release(mustach_sbuf_t *sbuf)
{
	if (sbuf->releasecb)
		sbuf->releasecb((void*)sbuf->value, sbuf->closure);
}

static inline size_t mustach_sbuf_length(const mustach_sbuf_t *sbuf)
{
	return sbuf->length ?: sbuf->value == NULL ? 0 : strlen(sbuf->value);
}

/*********************************************************
* Function for writing the indentation prefix
*********************************************************/
static int emitprefix(struct iwrap *iwrap, const struct prefix *prefix)
{
	if (prefix == NULL)
		return MUSTACH_OK;
	else if (prefix->len == 0)
		return emitprefix(iwrap, prefix->prefix);
	else {
		int rc = emitprefix(iwrap, prefix->prefix);
		if (rc == MUSTACH_OK)
			rc = iwrap->itf.emit(iwrap->closure, prefix->start, prefix->len, 0);
		return rc;
	}
}

/*********************************************************
* This section is for default implementations of
* optional callbacks that are although required
*********************************************************/
static int process(
		const char *template,
		size_t length,
		struct iwrap *iwrap,
		struct prefix *prefix
) {
	/* initial mustach delimiters */
	static const char defopcl[4] = { '{', '{', '}', '}' };
	const char *opstr = &defopcl[0]; /* open delimiter */
	const char *clstr = &defopcl[2]; /* close delimiter */
	unsigned oplen = 2; /* length of open delimiter */
	unsigned cllen = 2; /* length of close delimiter */

	mustach_sbuf_t sbuf;
	char c;
	const char *beg, *term, *end;
	struct {
		const char *name, *again;
		unsigned length;
		unsigned enabled: 1, entered: 1; } stack[MUSTACH_MAX_DEPTH];
	unsigned len, l;
	int depth, rc, enabled, stdalone;
	struct prefix pref;
	size_t sz;

	pref.prefix = prefix;
	end = &template[length];
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
					rc = iwrap->itf.emit(iwrap->closure, template, l, 0);
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
				if (c == *opstr && end - beg >= oplen) {
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
			if (*term == *clstr && end - term >= cllen) {
				for (l = 1 ; l < cllen && term[l] == clstr[l] ; l++);
				if (l == cllen)
					break;
			}
		}
		template = term + cllen;
		len = (size_t)(term - beg);
		c = *beg;
		switch(c) {
#if MINI_MUSTACH_WITH_COLON
		case ':':
			stdalone = 0;
			goto exclude_first;
#endif
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
#if !MINI_MUSTACH_WITH_EMPTY_TAG
			if (len == 0)
				return MUSTACH_ERROR_EMPTY_TAG;
#endif
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
				return MUSTACH_ERROR_BAD_DELIMITER;
			beg++;
			len -= 2;
			while (len && isspace(*beg))
				beg++, len--;
			while (len && isspace(beg[len - 1]))
				len--;
			for (l = 0; l < len && !isspace(beg[l]) ; l++);
			if (l == len)
				return MUSTACH_ERROR_BAD_DELIMITER;
			oplen = l;
			opstr = beg;
			while (l < len && isspace(beg[l])) l++;
			if (l == len)
				return MUSTACH_ERROR_BAD_DELIMITER;
			cllen = len - l;
			clstr = beg + l;
			break;
		case '^':
		case '#':
			/* begin section */
			if (depth == MUSTACH_MAX_DEPTH)
				return MUSTACH_ERROR_TOO_DEEP;
			rc = enabled;
			if (rc) {
				rc = iwrap->itf.enter(iwrap->closure, beg, len);
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
			if (depth-- == 0
			 || len != stack[depth].length
			 || memcmp(stack[depth].name, beg, len))
				return MUSTACH_ERROR_CLOSING;
			rc = enabled && stack[depth].entered
				? iwrap->itf.next(iwrap->closure) : 0;
			if (rc < 0)
				return rc;
			if (rc) {
				template = stack[depth++].again;
			} else {
				enabled = stack[depth].enabled;
				if (enabled && stack[depth].entered)
					iwrap->itf.leave(iwrap->closure);
			}
			break;
		case '>':
			/* partials */
			if (enabled) {
				if (iwrap->nesting >= MUSTACH_MAX_NESTING)
					rc = MUSTACH_ERROR_TOO_MUCH_NESTING;
				else {
					mustach_sbuf_reset(&sbuf);
					rc = iwrap->itf.partial(iwrap->closure, beg, len, &sbuf);
					if (rc >= 0) {
						iwrap->nesting++;
						sz = mustach_sbuf_length(&sbuf);
						rc = process(sbuf.value, sz, iwrap, &pref);
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
				mustach_sbuf_reset(&sbuf);
				rc = iwrap->itf.get(iwrap->closure, beg, len, &sbuf);
				if (rc < 0)
					return rc;
				sz = mustach_sbuf_length(&sbuf);
				rc = iwrap->itf.emit(iwrap->closure, sbuf.value, sz, c != '&');
				mustach_sbuf_release(&sbuf);
				if (rc < 0)
					return rc;
			}
			break;
		}
	}
}

/*********************************************************
* This section is for the public interface functions
*********************************************************/
int mini_mustach(
	const char *template,
	size_t length,
	const mini_mustach_itf_t *itf,
	void *closure
) {
	struct iwrap iwrap;

	/* check validity */
	if (itf == NULL || !itf->enter || !itf->next || !itf->leave || !itf->get)
		return MUSTACH_ERROR_INVALID_ITF;

	/* init wrap structure */
	iwrap.itf = *itf;
	if (iwrap.itf.partial == NULL)
		iwrap.itf.partial = iwrap.itf.get;
	iwrap.closure = closure;
	iwrap.nesting = 0;

	/* process */
	if (length == 0)
		length = strlen(template);
	return process(template, length, &iwrap, NULL);
}

