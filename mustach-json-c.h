/*
 Author: José Bollo <jobol@nonadev.net>

 https://gitlab.com/jobol/mustach

 SPDX-License-Identifier: 0BSD
*/

#ifndef _mustach_json_c_h_included_
#define _mustach_json_c_h_included_

/*
 * mustach-json-c is intended to make integration of json-c
 * library by providing integrated functions.
 */

#include <json-c/json.h>
#include "mustach-wrap.h"

/**
 * Wrap interface used internally by mustach json-c functions.
 * Can be used for overriding behaviour.
 */
extern const struct mustach_wrap_itf mustach_json_c_wrap_itf;

/**
 * mustach_json_c_file - Renders the mustache 'templstr' in 'file' for 'root'.
 *
 * @templstr: the template string to instantiate
 * @length:   length of the template or zero if unknown and template null terminated
 * @root:     the root json object to render
 * @file:     the file where to write the result
 *
 * Returns 0 in case of success, -1 with errno set in case of system error
 * a other negative value in case of error.
 */
extern int mustach_json_c_file(const char *templstr, size_t length, struct json_object *root, int flags, FILE *file);

/**
 * mustach_json_c_fd - Renders the mustache 'templstr' in 'fd' for 'root'.
 *
 * @templstr: the template string to instantiate
 * @length:   length of the template or zero if unknown and template null terminated
 * @root:     the root json object to render
 * @fd:       the file descriptor number where to write the result
 *
 * Returns 0 in case of success, -1 with errno set in case of system error
 * a other negative value in case of error.
 */
extern int mustach_json_c_fd(const char *templstr, size_t length, struct json_object *root, int flags, int fd);

/**
 * mustach_json_c_mem - Renders the mustache 'templstr' in 'result' for 'root'.
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
extern int mustach_json_c_mem(const char *templstr, size_t length, struct json_object *root, int flags, char **result, size_t *size);

/**
 * mustach_json_c_write - Renders the mustache 'templstr' for 'root' to custom writer 'writecb' with 'closure'.
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
extern int mustach_json_c_write(const char *templstr, size_t length, struct json_object *root, int flags, mustach_write_cb_t *writecb, void *closure);

/**
 * mustach_json_c_emit - Renders the mustache 'templstr' for 'root' to custom emiter 'emitcb' with 'closure'.
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
extern int mustach_json_c_emit(const char *templstr, size_t length, struct json_object *root, int flags, mustach_emit_cb_t *emitcb, void *closure);

extern int mustach_json_c_apply(
		mustach_template_t *templstr,
		struct json_object *root,
		int flags,
		mustach_write_cb_t *writecb,
		mustach_emit_cb_t *emitcb,
		void *closure
);

/***************************************************************************
* compatibility with version before 1.0
*/

/**
 * OBSOLETE use mustach_json_c_file
 *
 * fmustach_json_c - Renders the mustache 'templstr' in 'file' for 'root'.
 *
 * @templstr: the template string to instantiate
 * @root:     the root json object to render
 * @file:     the file where to write the result
 *
 * Returns 0 in case of success, -1 with errno set in case of system error
 * a other negative value in case of error.
 */

DEPRECATED_MUSTACH(extern int fmustach_json_c(const char *templstr, struct json_object *root, FILE *file));

/**
 * OBSOLETE use mustach_json_c_fd
 *
 * fdmustach_json_c - Renders the mustache 'templstr' in 'fd' for 'root'.
 *
 * @templstr: the template string to instantiate
 * @root:     the root json object to render
 * @fd:       the file descriptor number where to write the result
 *
 * Returns 0 in case of success, -1 with errno set in case of system error
 * a other negative value in case of error.
 */

DEPRECATED_MUSTACH(extern int fdmustach_json_c(const char *templstr, struct json_object *root, int fd));

/**
 * OBSOLETE use mustach_json_c_mem
 *
 * mustach_json_c - Renders the mustache 'templstr' in 'result' for 'root'.
 *
 * @templstr: the template string to instantiate
 * @root:     the root json object to render
 * @result:   the pointer receiving the result when 0 is returned
 * @size:     the size of the returned result
 *
 * Returns 0 in case of success, -1 with errno set in case of system error
 * a other negative value in case of error.
 */

DEPRECATED_MUSTACH(extern int mustach_json_c(const char *templstr, struct json_object *root, char **result, size_t *size));

/**
 * OBSOLETE use mustach_json_c_write
 *
 * umustach_json_c - Renders the mustache 'templstr' for 'root' to custom writer 'writecb' with 'closure'.
 *
 * @templstr: the template string to instantiate
 * @root:     the root json object to render
 * @writecb:  the function that write values
 * @closure:  the closure for the write function
 *
 * Returns 0 in case of success, -1 with errno set in case of system error
 * a other negative value in case of error.
 */
typedef mustach_write_cb_t *mustach_json_write_cb;
DEPRECATED_MUSTACH(extern int umustach_json_c(const char *templstr, struct json_object *root, mustach_write_cb_t *writecb, void *closure));

#endif
