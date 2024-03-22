/*
 Author: José Bollo <jobol@nonadev.net>

 https://gitlab.com/jobol/mustach

 SPDX-License-Identifier: 0BSD
*/

#ifndef _mustach2_h_included_
#define _mustach2_h_included_
/*
 * since version 2, minimustach is the core seed, let include it.
 */
#include "mini-mustach.h"
/*
 * Predeclaration of essential structs and their short names.
 *
 * The type 'mustach_template_t' is for an opaque structure.
 */
typedef struct mustach_template mustach_template_t;
/**
 * The types 'mustach_build_itf_t' and 'mustach_apply_itf_t'
 * are for implementation callbacks and are defines below.
 */
typedef struct mustach_build_itf mustach_build_itf_t;
typedef struct mustach_apply_itf mustach_apply_itf_t;


/**
 * Flags specific to mustach builder
 */
#define Mustach_Build_With_Colon          1
#define Mustach_Build_With_EmptyTag       2
#define Mustach_Build_Null_Term_Tag       4
#define Mustach_Build_Null_Term_Text      8
#define Mustach_Build_All_Flags_Mask     15

/**
 * Flags specific to mustach applier
 */
#define Mustach_Apply_GlobalPartialFirst  1

/*
 * Interfaces in version 2 are differing from interfaces of
 * version 1 and older in that it also holds in more than
 * function pointers:
 *  - a version value for allowing extension
 *  - the closure
 *  - the flags
 * And so the interface itself serves as closure to the
 * implementation functions.
 */

#define MUSTACH_BUILD_ITF_VERSION_1     1
#define MUSTACH_BUILD_ITF_VERSION_CUR   MUSTACH_BUILD_ITF_VERSION_1
#define MUSTACH_BUILD_ITF_VERSION_MIN   MUSTACH_BUILD_ITF_VERSION_1
#define MUSTACH_BUILD_ITF_VERSION_MAX   MUSTACH_BUILD_ITF_VERSION_1

struct mustach_build_itf {
	int version;
	void (*error)(
		void *closure,
		int code,
		const char *desc);
	void *(*alloc)(
		size_t size,
		void *closure);
	void (*dealloc)(
		void *item,
		void *closure);
};

#define MUSTACH_APPLY_ITF_VERSION_1      1
#define MUSTACH_APPLY_ITF_VERSION_CUR    MUSTACH_APPLY_ITF_VERSION_1
#define MUSTACH_APPLY_ITF_VERSION_MIN    MUSTACH_APPLY_ITF_VERSION_1
#define MUSTACH_APPLY_ITF_VERSION_MAX    MUSTACH_APPLY_ITF_VERSION_1

struct mustach_apply_itf {
	int version;
	void (*error)(
		void *closure,
		int code,
		const char *desc);
	int (*start)(
		void *closure);
	void (*stop)(
		void *closure,
		int status);
	int (*emit_raw)(
		void *closure,
		const char *buffer,
		size_t size);
	int (*emit_esc)(
		void *closure,
		const char *buffer,
		size_t size,
		int escape);
	int (*get)(
		void *closure,
		const char *name,
		size_t length,
		mustach_sbuf_t *sbuf);
	int (*enter)(
		void *closure,
		const char *name,
		size_t length);
	int (*next)(
		void *closure);
	int (*leave)(
		void *closure);
	int (*partial_get)(
		void *closure,
		const char *name,
		size_t length,
		mustach_template_t **partial);
	void (*partial_put)(
		void *closure,
		mustach_template_t *partial);
};


extern
void mustach_destroy_template(
		mustach_template_t *templ,
		const mustach_build_itf_t *itf,
		void *closure);

extern
int mustach_build_template(
		mustach_template_t **templ,
		int flags,
		const mustach_sbuf_t *sbuf,
		const char *name,
		size_t namelen,
		const mustach_build_itf_t *itf,
		void *closure);

extern
int mustach_make_template(
		mustach_template_t **templ,
		int flags,
		const mustach_sbuf_t *sbuf,
		const char *name);

extern
int mustach_apply_template(
		mustach_template_t *templ,
		int flags,
		const mustach_apply_itf_t *itf,
		void *closure);

#define MUSTACHE_DATA_COUNT_MIN 2

extern
unsigned mustach_get_template_data_count(
		mustach_template_t *templ);

extern
void mustach_set_template_data(
		mustach_template_t *templ,
		unsigned index,
		void *value);

extern
void *mustach_get_template_data(
		mustach_template_t *templ,
		unsigned index);

extern
const char *mustach_get_template_name(
		mustach_template_t *templ,
		size_t *length);

extern
int mustach_get_template_flags(
		mustach_template_t *templ);

#endif

