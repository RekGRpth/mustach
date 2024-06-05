/*
 Author: José Bollo <jobol@nonadev.net>

 https://gitlab.com/jobol/mustach

 SPDX-License-Identifier: 0BSD
*/
#ifndef _mini_mustach_h_included_
#define _mini_mustach_h_included_
/*
 * Some standard definitions
 */
#include <stddef.h>
/*
 * Current version of mustach and its derivates
 */
#define MUSTACH_VERSION 200
#define MUSTACH_VERSION_MAJOR (MUSTACH_VERSION / 100)
#define MUSTACH_VERSION_MINOR (MUSTACH_VERSION % 100)
/*
 * Predeclaration of used struct types.
 * The difinition is made below.
 */
typedef struct mini_mustach_itf mini_mustach_itf_t;
typedef struct mustach_sbuf mustach_sbuf_t;
/*
 * mini_mustach - Renders the mustache 'template' of 'length'
 *                with 'itf' and 'closure'.
 *
 * This is the main and only function offered by mini-mustach.
 * The template string 'templ' does not need to be zero terminated if
 * the 'length' if not zero.
 *
 * When the given length is zero then the 'templ' string must be
 * zero terminated and its length is computed using 'strlen'.
 *
 * During processing of the template, the callbacks of the interface 'itf'
 * are invoked for accomplishing some of the actions that the generic
 * renderer can't do. The interface and these callbacks are documented
 * below.
 *
 * The function 'mini_mustach' doesn't allocate any memory. It works
 * using the plain template string. It is intended for being used in
 * embedded environment where memory is limited or when efficiency is not
 * an issue.
 *
 * In current version, the function 'mini_mustach' doesn't implement
 * inheritance (blocs and parent tags).
 *
 * @templ:    the template string to instantiate
 * @length:   length of the template or zero for automatic computation
 * @itf:      the callbacks invoked by 'mini_mustach' during processing
 * @closure:  the closure to pass to interface's callbacks
 *
 * Returns:
 *
 * The function returns MUSTACH_OK in case of success.
 *
 * When it doesn't return MUSTACH_OK it generally returns
 * one of the standard error code defined below.
 *
 * But it can also be a user defined value returned by some
 * of the callback of the interface 'itf'.
 *
 * By itself, 'mini_mustach' can produce the following errors:
 *  - MUSTACH_ERROR_INVALID_ITF
 *  - MUSTACH_ERROR_UNEXPECTED_END
 *  - MUSTACH_ERROR_BAD_UNESCAPE_TAG
 *  - MUSTACH_ERROR_EMPTY_TAG
 *  - MUSTACH_ERROR_BAD_DELIMITER
 *  - MUSTACH_ERROR_TOO_DEEP
 *  - MUSTACH_ERROR_CLOSING
 *  - MUSTACH_ERROR_TOO_MUCH_NESTING
 */
extern int mini_mustach(
		const char *templ,
		size_t length,
		const mini_mustach_itf_t *itf,
		void *closure);
/*
 * Definition of status codes returned by mustach:
 * 
 * - MUSTACH_OK: no error status
 * 
 * - MUSTACH_ERROR_SYSTEM: predefined error code for any error of the
 *   operating system, normally completed by errno and some context
 * 
 * - MUSTACH_ERROR_UNEXPECTED_END: end of the template string in some
 *   started section
 *   
 * - MUSTACH_ERROR_EMPTY_TAG: the tag is empty
 * 
 * - MUSTACH_ERROR_TOO_BIG: something in the template is too big to
 *   be processed
 * 
 * - MUSTACH_ERROR_BAD_DELIMITER: the definition of new delimiters is
 *   invalid
 * 
 * - MUSTACH_ERROR_TOO_DEEP: the depth of imbricated sections
 *   is too big (see limit MUSTACH_MAX_DEPTH)
 * 
 * - MUSTACH_ERROR_CLOSING: the closing tag doesn't match the opening one
 * 
 * - MUSTACH_ERROR_BAD_UNESCAPE_TAG: the unescape tag {{{ }}} is
 *   syntacticly invalid
 * 
 * - MUSTACH_ERROR_INVALID_ITF: the callback interface doesn't match the
 *   requirements
 * 
 * - MUSTACH_ERROR_NOT_FOUND: a partial or parent is not found
 * 
 * - MUSTACH_ERROR_UNDEFINED_TAG: an expected tag is not defined in the
 *   model
 * 
 * - MUSTACH_ERROR_TOO_MUCH_NESTING: the depth of partial imbrication
 *   is too big (see limit MUSTACH_MAX_NESTING) probably reached because
 *   of recursivity
 * 
 * - MUSTACH_ERROR_OUT_OF_MEMORY: memory exhausted
 */
#define MUSTACH_OK                       0
#define MUSTACH_ERROR_SYSTEM            -1
#define MUSTACH_ERROR_UNEXPECTED_END    -2
#define MUSTACH_ERROR_EMPTY_TAG         -3
#define MUSTACH_ERROR_TOO_BIG           -4
#define MUSTACH_ERROR_BAD_DELIMITER     -5
#define MUSTACH_ERROR_TOO_DEEP          -6
#define MUSTACH_ERROR_CLOSING           -7
#define MUSTACH_ERROR_BAD_UNESCAPE_TAG  -8
#define MUSTACH_ERROR_INVALID_ITF       -9
#define MUSTACH_ERROR_NOT_FOUND         -11
#define MUSTACH_ERROR_UNDEFINED_TAG     -12
#define MUSTACH_ERROR_TOO_MUCH_NESTING  -13
#define MUSTACH_ERROR_OUT_OF_MEMORY     -14
/*
 * You can use definition below for user specific error
 *
 * The macro MUSTACH_ERROR_USER is involutive so for any value
 *   value = MUSTACH_ERROR_USER(MUSTACH_ERROR_USER(value))
 *
 * For example you can define the error 22 using the macro
 * MUSTACH_ERROR_USER(22) that gives the vlaue -122. And you
 * can query the user defined code of the user error -122 using
 * MUSTACH_ERROR_USER(-122) that gives 22.
 */
#define MUSTACH_ERROR_USER_BASE         -100
#define MUSTACH_ERROR_USER(x)           (MUSTACH_ERROR_USER_BASE-(x))
#define MUSTACH_IS_ERROR_USER(x)        (MUSTACH_ERROR_USER(x) >= 0)
/*
 * Definition of some limits. When using libraries, you should not
 * try to overwrite these definitions. But when you include mustach
 * your own compilation of mustach, you can tune the values to your
 * expected requirement.
 *
 * - MUSTACH_MAX_DEPTH is the maximum depth allowed for imbrication of
 *   sections in a template. It is possible to use partials for go
 *   beyond the limit.
 *
 * - MUSTACH_MAX_NESTING is the maximum level of nesting of partials.
 *   This value is for avoiding infinite recusivity.
 */
#ifndef MUSTACH_MAX_DEPTH
#define MUSTACH_MAX_DEPTH    32
#endif
#ifndef MUSTACH_MAX_NESTING
#define MUSTACH_MAX_NESTING  32
#endif
/*
 * mustach_sbuf - Interface for handling zero terminated strings
 *
 * That structure is used for returning zero terminated strings
 * -in 'value'- to mustach. The callee can provide a function
 * for releasing the returned 'value'. Three methods for releasing
 * the string are possible.
 *
 *  1. no release: set either 'freecb' or 'releasecb' with NULL
 *     (done by default)
 *  2. release without closure: set 'freecb' to its expected value
 *  3. release with closure: set 'releasecb' and 'closure'
 *     to their expected values
 *
 * CAUTION: callbacks require CDECL calling convention because with
 * that convention, calling the full featured releasecb is possible
 * even if freecb is expected.
 *
 * @value: The value of the string.
 *         That value is not changed by mustach -const-.
 *
 * @freecb: The function to call for freeing the value without closure.
 *          For convenience, signature of that callback is
 *          compatible with 'free'.
 *          Can be NULL.
 *
 * @releasecb: The function to release with closure.
 *             Can be NULL.
 *
 * @closure: The closure to use for 'releasecb'.
 *
 * @length: Length of the value or zero if unknown and value null
 *          terminated. To return the empty string, it is possible
 *          to let length be 0 and value be NULL.
 */
struct mustach_sbuf
{
	const char *value;
	union {
		void (*freecb)(void *value);
		void (*releasecb)(void *value, void *closure);
	};
	void *closure;
	size_t length;
};
/*
 * mini_mustach_itf - interface for callbacks
 *
 * The functions enter and next should return 0 or 1.
 *
 * All other functions should normally return MUSTACH_OK (zero).
 *
 * CAUTION! strings are not zero terminated
 *
 * When any callback returns value not expected, the rendering process
 * is stopped and the unexpected value is returned.
 *
 * Mini mustach rendering process also detects error that can interrupt
 * the rendering process. The macros MUSTACH_ERROR_USER and
 * MUSTACH_IS_ERROR_USER could help to avoid clashes in values.
 *
 * @emit: Called for emitting a 'text' of 'length' with 'escaping' or not.
 *
 * @get: Returns in 'sbuf' the value for 'name' of 'length'.
 *       CAUTION! 'name' are not zero terminated
 *
 * @enter: Conditionaly enters the section having 'name' of 'length'.
 *         CAUTION! 'name' are not zero terminated
 *
 *         Must return 1 if the section exists and can be entered,
 *         or must return 0 otherwise.
 *
 *         When the section is entered (1 is returned), the callback
 *         must activate the first item of the section of 'name'.
 *         Then the callback 'next' may be called and the callback 'leave'
 *         will always be called at end.
 *
 *         Conversely 'leave' is never called when enter did not return 1.
 *
 * @next: Activates the next item of the last entered section if it exists.
 *        Musts return 1 when the next item is activated.
 *        Musts return 0 when there is no item to activate.
 *
 * @leave: Leaves the last entered section.
 *
 * @partial: If defined (can be NULL), returns in 'sbuf' the content
 *           of the partial for 'name' of length.
 *           CAUTION! 'name' are not zero terminated
 *           If NULL, 'get' callback is called instead.
 */
struct mini_mustach_itf
{
	int (*emit)(
		void *closure,
		const char *text, /* not zero terminated */
		size_t length,
		int escaping);

	int (*get)(
		void *closure,
		const char *name, /* not zero terminated */
		size_t length,
		mustach_sbuf_t *sbuf);

	int (*enter)(
		void *closure,
		const char *name, /* not zero terminated */
		size_t length);

	int (*next)(void *closure);

	int (*leave)(void *closure);

	int (*partial)(
		void *closure,
		const char *name, /* not zero terminated */
		size_t length,
		mustach_sbuf_t *sbuf);
};

#endif

