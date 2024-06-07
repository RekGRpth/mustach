/*
 Author: José Bollo <jobol@nonadev.net>

 https://gitlab.com/jobol/mustach

 SPDX-License-Identifier: 0BSD
*/

#ifndef _mustach_helpers_h_included_
#define _mustach_helpers_h_included_


#include "mustach2.h"

#include <stdio.h>
#include <string.h>

/*********************************************************
* This section has functions for managing instances of mustach_sbuf_t
*********************************************************/
#define MUSTACH_SBUF_INIT ((mustach_sbuf_t){ NULL, { NULL }, NULL, 0 })

static inline void mustach_sbuf_reset(mustach_sbuf_t *sbuf)
{
	*sbuf = MUSTACH_SBUF_INIT;
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
* This section wraps implementation details of memory files.
* It also takes care of adding a terminating zero to the
* produced text.
* When the function open_memstream exists, use it.
* Otherwise emulate it using a temporary file.
**********************************************************/
extern FILE *mustach_memfile_open(char **buffer, size_t *size);
extern void mustach_memfile_abort(FILE *file, char **buffer, size_t *size);
extern int mustach_memfile_close(FILE *file, char **buffer, size_t *size);

/*********************************************************
* This section is for reading file
**********************************************************/
extern int mustach_read_file(const char *path, mustach_sbuf_t *sbuf);

/*********************************************************
* This section is for escaping
**********************************************************/
extern int mustach_escape(
	const char *buffer,
	size_t size,
	int (*write)(void *, const char *, size_t),
	void *closure
);

extern int mustach_fwrite(FILE *file, const char *buffer, size_t size);
extern int mustach_fwrite_cb(void *file, const char *buffer, size_t size);
extern int mustach_fwrite_escape(FILE *file, const char *buffer, size_t size);

extern int mustach_write(int fd, const char *buffer, size_t size);
extern int mustach_write_cb(void *fd, const char *buffer, size_t size);
extern int mustach_write_escape(int fd, const char *buffer, size_t size);

/*********************************************************
* This section is for managing memory stream
*********************************************************/
typedef
struct mustach_stream {
	char *buffer;
	size_t length;
	size_t avail;
}
	mustach_stream_t;

#define MUSTACH_STREAM_INIT ((mustach_stream_t){ NULL, 0, 0 })

extern void mustach_stream_init(mustach_stream_t *stream);
extern void mustach_stream_abort(mustach_stream_t *stream);
extern int mustach_stream_end(
		mustach_stream_t *stream,
		char **buffer,
		size_t *size);
extern int mustach_stream_write(
		mustach_stream_t *stream,
		const char *buffer,
		size_t size);
extern int mustach_stream_write_cb(
		void *closure,
		const char *buffer,
		size_t size);

#endif

