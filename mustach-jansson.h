/*
 Author: José Bollo <jobol@nonadev.net>

 https://gitlab.com/jobol/mustach

 SPDX-License-Identifier: 0BSD
*/

#ifndef _mustach_jansson_h_included_
#define _mustach_jansson_h_included_

/*
 * mustach-jansson is intended to make integration of jansson
 * library by providing integrated functions.
 */

#include <jansson.h>
#include "mustach-wrap.h"

/**
 * Wrap interface used internally by mustach jansson functions.
 * Can be used for overriding behaviour.
 */
extern const struct mustach_wrap_itf mustach_jansson_wrap_itf;

/**
 * mustach_jansson_file - Renders the mustache 'templstr' in 'file' for 'root'.
 *
 * @templstr: the template string to instantiate
 * @length:   length of the template or zero if unknown and template null terminated
 * @root:     the root json object to render
 * @file:     the file where to write the result
 *
 * Returns 0 in case of success, -1 with errno set in case of system error
 * a other negative value in case of error.
 */
extern int mustach_jansson_file(const char *templstr, size_t length, json_t *root, int flags, FILE *file);

/**
 * mustach_jansson_fd - Renders the mustache 'templstr' in 'fd' for 'root'.
 *
 * @templstr: the template string to instantiate
 * @length:   length of the template or zero if unknown and template null terminated
 * @root:     the root json object to render
 * @fd:       the file descriptor number where to write the result
 *
 * Returns 0 in case of success, -1 with errno set in case of system error
 * a other negative value in case of error.
 */
extern int mustach_jansson_fd(const char *templstr, size_t length, json_t *root, int flags, int fd);


/**
 * mustach_jansson_mem - Renders the mustache 'templstr' in 'result' for 'root'.
 *
 * @templstr: the template string to instantiate
 * @length:   length of the template or zero if unknown and template null terminated
 * @root:     the root json object to render
 * @result:   the pointer receiving the result when 0 is returned
 * @size:     the size of the returned result
 *
 * Returns 0 in case of success, -1 with errno set in case of system error
 * a other negative value in case of error.
 */
extern int mustach_jansson_mem(const char *templstr, size_t length, json_t *root, int flags, char **result, size_t *size);

/**
 * mustach_jansson_write - Renders the mustache 'templstr' for 'root' to custom writer 'writecb' with 'closure'.
 *
 * @templstr: the template string to instantiate
 * @length:   length of the template or zero if unknown and template null terminated
 * @root:     the root json object to render
 * @writecb:  the function that write values
 * @closure:  the closure for the write function
 *
 * Returns 0 in case of success, -1 with errno set in case of system error
 * a other negative value in case of error.
 */
extern int mustach_jansson_write(const char *templstr, size_t length, json_t *root, int flags, mustach_write_cb_t *writecb, void *closure);

/**
 * mustach_jansson_emit - Renders the mustache 'templstr' for 'root' to custom emiter 'emitcb' with 'closure'.
 *
 * @templstr: the template string to instantiate
 * @length:   length of the template or zero if unknown and template null terminated
 * @root:     the root json object to render
 * @emitcb:   the function that emit values
 * @closure:  the closure for the write function
 *
 * Returns 0 in case of success, -1 with errno set in case of system error
 * a other negative value in case of error.
 */
extern int mustach_jansson_emit(const char *templstr, size_t length, json_t *root, int flags, mustach_emit_cb_t *emitcb, void *closure);

extern int mustach_jansson_apply(
		mustach_template_t *templstr,
		json_t *root,
		int flags,
		mustach_write_cb_t *writecb,
		mustach_emit_cb_t *emitcb,
		void *closure
);


#endif

