/*
 Author: José Bollo <jobol@nonadev.net>
 Author: José Bollo <jose.bollo@iot.bzh>

 https://gitlab.com/jobol/mustach

 SPDX-License-Identifier: ISC
*/

#ifndef _mustach_wrap_h_included_
#define _mustach_wrap_h_included_

/*
 * mustach-wrap is intended to make integration of JSON
 * libraries easier by wrapping mustach extensions in a
 * single place.
 * 
 * As before, using mustach and only mustach is possible
 * but does not implement high level features coming
 * with extensions.
 */

/**
 * mustach_wrap_itf - interface for callbacks
 */
struct mustach_wrap_itf {
	int (*start)(void *closure);
	void (*stop)(void *closure, int status);
	int (*compare)(void *closure, const char *value);
	int (*sel)(void *closure, const char *name);
	int (*subsel)(void *closure, const char *name);
	int (*enter)(void *closure, int objiter);
	int (*next)(void *closure);
	int (*leave)(void *closure);
	int (*get)(void *closure, struct mustach_sbuf *sbuf, int key);
};

typedef int mustach_wrap_emit_cb(void *closure, const char *buffer, size_t size, int escape);
typedef int mustach_wrap_write_cb(void *closure, const char *buffer, size_t size);

extern int fmustach_wrap(const char *template, struct mustach_wrap_itf *itf, void *closure, FILE *file);

extern int fdmustach_wrap(const char *template, struct mustach_wrap_itf *itf, void *closure, int fd);

extern int mustach_wrap(const char *template, struct mustach_wrap_itf *itf, void *closure, char **result, size_t *size);

extern int umustach_wrap(const char *template, struct mustach_wrap_itf *itf, void *closure, mustach_wrap_write_cb *writecb, void *writeclosure);

extern int emustach_wrap(const char *template, struct mustach_wrap_itf *itf, void *closure, mustach_wrap_emit_cb *emitcb, void *emitclosure);

#endif

