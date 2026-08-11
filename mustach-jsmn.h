/*
 Author: Georgy Shelkovy

 https://gitlab.com/jobol/mustach

 SPDX-License-Identifier: 0BSD
*/

#ifndef _mustach_jsmn_h_included_
#define _mustach_jsmn_h_included_

/*
 * mustach-jsmn is intended to make integration of the jsmn
 * library by providing integrated functions.
 *
 * Unlike other backends, jsmn doesn't build a tree of objects:
 * jsmn_parse only fills a flat array of tokens ('jsmntok_t') that
 * index into the original json text. So instead of a single root
 * object, the "root" of a jsmn document is the pair made of the
 * original json text and its parsed tokens array.
 *
 * jsmn.h is a single-header library. This header only pulls its
 * type declarations (JSMN_HEADER, no function bodies) so it can be
 * safely included from several translation units. If your code
 * needs to call jsmn_parse itself (instead of using the
 * mustach_jsmn_parse helper below), include "jsmn.h" the usual way
 * (defining JSMN_STATIC, or providing one non-static instantiation
 * project wide).
 */
#ifndef JSMN_HEADER
#define JSMN_HEADER
#endif
#include "jsmn.h"

#include "mustach-wrap.h"

/**
 * Wrap interface used internally by mustach jsmn functions.
 * Can be used for overriding behaviour.
 */
extern const struct mustach_wrap_itf mustach_jsmn_wrap_itf;

/**
 * mustach_jsmn_parse - Parses the 'json' text of 'length' bytes and
 * returns in 'tokens' a freshly allocated array of parsed tokens
 * (the root of the document is always tokens[0]).
 *
 * @json:    the json text to parse
 * @length:  length of the json text
 * @tokens:  pointer receiving the allocated tokens array when 0 is returned
 * @count:   pointer receiving the count of tokens when 0 is returned
 *
 * Returns 0 in case of success. The caller then owns '*tokens' and must
 * free() it. Returns MUSTACH_ERROR_SYSTEM in case of allocation failure,
 * or MUSTACH_ERROR_BAD_DATA if the json text is invalid or incomplete.
 */
extern int mustach_jsmn_parse(const char *json, size_t length, jsmntok_t **tokens, int *count);

/**
 * mustach_jsmn_file - Renders the mustache 'templstr' in 'file' for the
 * document made of 'json' and its parsed 'tokens'.
 *
 * @templstr: the template string to instantiate
 * @length:   length of the template or zero if unknown and template null terminated
 * @json:     the json text that was parsed
 * @tokens:   the tokens array produced by parsing 'json' (see mustach_jsmn_parse)
 * @file:     the file where to write the result
 *
 * Returns 0 in case of success, -1 with errno set in case of system error
 * a other negative value in case of error.
 */
extern int mustach_jsmn_file(const char *templstr, size_t length, const char *json, jsmntok_t *tokens, int flags, FILE *file);

/**
 * mustach_jsmn_fd - Renders the mustache 'templstr' in 'fd' for the
 * document made of 'json' and its parsed 'tokens'.
 *
 * @templstr: the template string to instantiate
 * @length:   length of the template or zero if unknown and template null terminated
 * @json:     the json text that was parsed
 * @tokens:   the tokens array produced by parsing 'json' (see mustach_jsmn_parse)
 * @fd:       the file descriptor number where to write the result
 *
 * Returns 0 in case of success, -1 with errno set in case of system error
 * a other negative value in case of error.
 */
extern int mustach_jsmn_fd(const char *templstr, size_t length, const char *json, jsmntok_t *tokens, int flags, int fd);

/**
 * mustach_jsmn_mem - Renders the mustache 'templstr' in 'result' for the
 * document made of 'json' and its parsed 'tokens'.
 *
 * @templstr: the template string to instantiate
 * @length:   length of the template or zero if unknown and template null terminated
 * @json:     the json text that was parsed
 * @tokens:   the tokens array produced by parsing 'json' (see mustach_jsmn_parse)
 * @result:   the pointer receiving the result when 0 is returned
 * @size:     the size of the returned result
 *
 * Returns 0 in case of success, -1 with errno set in case of system error
 * a other negative value in case of error.
 */
extern int mustach_jsmn_mem(const char *templstr, size_t length, const char *json, jsmntok_t *tokens, int flags, char **result, size_t *size);

/**
 * mustach_jsmn_write - Renders the mustache 'templstr' for the document
 * made of 'json' and its parsed 'tokens' to custom writer 'writecb' with
 * 'closure'.
 *
 * @templstr: the template string to instantiate
 * @length:   length of the template or zero if unknown and template null terminated
 * @json:     the json text that was parsed
 * @tokens:   the tokens array produced by parsing 'json' (see mustach_jsmn_parse)
 * @writecb:  the function that write values
 * @closure:  the closure for the write function
 *
 * Returns 0 in case of success, -1 with errno set in case of system error
 * a other negative value in case of error.
 */
extern int mustach_jsmn_write(const char *templstr, size_t length, const char *json, jsmntok_t *tokens, int flags, mustach_write_cb_t *writecb, void *closure);

/**
 * mustach_jsmn_emit - Renders the mustache 'templstr' for the document
 * made of 'json' and its parsed 'tokens' to custom emiter 'emitcb' with
 * 'closure'.
 *
 * @templstr: the template string to instantiate
 * @length:   length of the template or zero if unknown and template null terminated
 * @json:     the json text that was parsed
 * @tokens:   the tokens array produced by parsing 'json' (see mustach_jsmn_parse)
 * @emitcb:   the function that emit values
 * @closure:  the closure for the write function
 *
 * Returns 0 in case of success, -1 with errno set in case of system error
 * a other negative value in case of error.
 */
extern int mustach_jsmn_emit(const char *templstr, size_t length, const char *json, jsmntok_t *tokens, int flags, mustach_emit_cb_t *emitcb, void *closure);

extern int mustach_jsmn_apply(
		mustach_template_t *templstr,
		const char *json,
		jsmntok_t *tokens,
		int flags,
		mustach_write_cb_t *writecb,
		mustach_emit_cb_t *emitcb,
		void *closure
);

#endif
