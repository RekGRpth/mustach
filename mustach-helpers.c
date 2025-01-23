/*
 Author: José Bollo <jobol@nonadev.net>

 https://gitlab.com/jobol/mustach

 SPDX-License-Identifier: 0BSD
*/

#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif

#include "mustach-helpers.h"

#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <stdint.h>
#ifdef _WIN32
#include <malloc.h>
#endif

/*********************************************************
**********************************************************/
static const char *errtxts[] = {
	"?",
	"system",
	"unexpected end",
	"empty tag",
	"too big",
	"bad delimiter",
	"too deep",
	"closing",
	"bad unescape tag",
	"invalid interface",
	"not found",
	"undefined tag",
	"too much nesting",
	"out of memory",
	"bad input data"
};

const char *mustach_strerror(int code)
{
	int idx = -code;
	if (idx < 0 || idx > (int)(sizeof errtxts / sizeof *errtxts))
		idx = 0;
	return errtxts[idx];
}

/*********************************************************
* This section wraps implementation details of memory files.
* It also takes care of adding a terminating zero to the
* produced text.
* When the function open_memstream exists, use it.
* Otherwise emulate it using a temporary file.
**********************************************************/
#if !defined(NO_OPEN_MEMSTREAM)
FILE *mustach_memfile_open(char **buffer, size_t *size)
{
	return open_memstream(buffer, size);
}
void mustach_memfile_abort(FILE *file, char **buffer, size_t *size)
{
	fclose(file);
	free(*buffer);
	*buffer = NULL;
	*size = 0;
}
int mustach_memfile_close(FILE *file, char **buffer, size_t *size)
{
	int rc;

	/* adds terminating null */
	rc = fputc(0, file) ? MUSTACH_ERROR_SYSTEM : 0;
	fclose(file);
	if (rc == 0)
		/* removes terminating null of the length */
		(*size)--;
	else {
		free(*buffer);
		*buffer = NULL;
		*size = 0;
	}
	return rc;
}
#else
FILE *mustach_memfile_open(char **buffer, size_t *size)
{
	/*
	 * We can't provide *buffer and *size as open_memstream does but
	 * at least clear them so the caller won't get bad data.
	 */
	*buffer = NULL;
	*size = 0;

	return tmpfile();
}
void mustach_memfile_abort(FILE *file, char **buffer, size_t *size)
{
	fclose(file);
	*buffer = NULL;
	*size = 0;
}
static int read_file(FILE *file, mustach_sbuf_t *sbuf);
int mustach_memfile_close(FILE *file, char **buffer, size_t *size)
{
	mustach_sbuf_t buf;
	int rc = read_file(file, &buf);
		*buffer = (char*)buf.value;
		*size = buf.length;
	return rc;
}
#endif

/*********************************************************
* This section is for reading file
**********************************************************/

#ifndef SZINIT
#define SZINIT 4000
#endif
static int read_file(FILE *file, mustach_sbuf_t *sbuf)
{
	int rc;
	char *p, *buffer = NULL;
	size_t rsz, nsz, all, idx = 0, sz = 0;

	/* file opened, compute file size */
	if (fseek(file, 0, SEEK_END) >= 0) {
		long pos = ftell(file);
		if (pos < 0 || fseek(file, 0, SEEK_SET) < 0)
			return MUSTACH_ERROR_SYSTEM;
		sz = (size_t)pos;
		if (pos != (long)sz)/*detecting truncation*/
			return MUSTACH_ERROR_TOO_BIG;
	}

	/* initial size */
	if (sz == 0)
		all = SZINIT;
	else {
		all = 1 + sz;
		if (all < sz)/*detecting truncation*/
			return MUSTACH_ERROR_TOO_BIG;
	}

	/* allocates buffer */
	rc = MUSTACH_ERROR_OUT_OF_MEMORY;
	while ((p = realloc(buffer, all)) != NULL) {
		buffer = p;
		/* read values */
		nsz = all - 1 - idx;
		rsz = fread(&buffer[idx], 1, nsz, file);
		if (rsz != 0)
			idx += rsz;
		else if (feof(file))
			sz = idx;
		else {
			rc = MUSTACH_ERROR_SYSTEM;
			break;
		}
		if (sz == idx) {
			/* read done, resize buffer if needed, init sbuf */
			if (all > idx + 1 && (p = realloc(buffer, idx + 1)) != NULL)
				buffer = p;
			sbuf->value = buffer;
			buffer[sbuf->length = idx] = 0;
			sbuf->freecb = free;
			sbuf->closure = NULL;
			return MUSTACH_OK;
		}
		if (rsz == nsz) {
			/* at end of buffer, growth needed */
			if (all + all > all)
				all = all + all;
			else if (all != SIZE_MAX)
				all = SIZE_MAX;
			else {
				free(buffer);
				return MUSTACH_ERROR_TOO_BIG;
			}
		}
	}
	free(buffer);
	mustach_sbuf_reset(sbuf);
	return rc;
}

int mustach_read_file(const char *path, mustach_sbuf_t *sbuf)
{
	int rc;
	FILE *file;

	if (strcmp(path, "-") == 0)
		return read_file(stdin, sbuf);

	/* open the path */
	file = fopen(path, "r");
	if (file == NULL)
		return MUSTACH_ERROR_NOT_FOUND;
	rc = read_file(file, sbuf);
	fclose(file);
	return rc;
}

/*********************************************************
* This section is for escaping
**********************************************************/

int mustach_escape(
		const char *buffer,
		size_t size,
		int (*emit)(void *, const char *, size_t),
		void *closure
) {
	const char *p;
	int r, n = 0;
	size_t i = 0, j = 0;
	for (;;) {
		if (j == size)
			n = -1;
		else {
			switch(buffer[j]) {
			case '<': p = "&lt;"; n = 4; break;
			case '>': p = "&gt;"; n = 4; break;
			case '&': p = "&amp;"; n = 5; break;
			case '"': p = "&quot;"; n = 6; break;
			default: j++; continue;
			}
		}
		if (i < j) {
			r = emit(closure, &buffer[i], j - i);
			if (r != MUSTACH_OK)
				return r;
		}
		if (n < 0)
			return MUSTACH_OK;
		r = emit(closure, p, (size_t)n);
		if (r != MUSTACH_OK)
			return r;
		i = ++j;
	}
}

int mustach_fwrite(
		FILE *file,
		const char *buffer,
		size_t size
) {
	return fwrite(buffer, 1, size, file) == size
		? MUSTACH_OK : MUSTACH_ERROR_SYSTEM;
}

int mustach_fwrite_cb(
		void *closure,
		const char *buffer,
		size_t size
) {
	FILE *file = (FILE*)closure;
	return mustach_fwrite(file, buffer, size);
}

int mustach_fwrite_escape(
		FILE *file,
		const char *buffer,
		size_t size
) {
	return mustach_escape(buffer, size, mustach_fwrite_cb, file);
}

int mustach_write(
		int fd,
		const char *buffer,
		size_t size
) {
	while (size > 0) {
		ssize_t s = write(fd, buffer, size);
		if (s > 0) {
			size -= (size_t)s;
			buffer += s;
		}
		else if (errno != EINTR)
			return MUSTACH_ERROR_SYSTEM;
	}
	return MUSTACH_OK;
}

int mustach_write_cb(
		void *closure,
		const char *buffer,
		size_t size
) {
	int fd = (int)(intptr_t)closure;
	return mustach_write(fd, buffer, size);
}

int mustach_write_escape(
		int fd,
		const char *buffer,
		size_t size
) {
	void *closure = (void*)(intptr_t)fd;
	return mustach_escape(buffer, size, mustach_write_cb, closure);
}










#define SZBLK  4000


void mustach_stream_init(mustach_stream_t *stream)
{
	*stream = MUSTACH_STREAM_INIT;
}

void mustach_stream_abort(mustach_stream_t *stream)
{
	free(stream->buffer);
}

int mustach_stream_end(
		mustach_stream_t *stream,
		char **buffer,
		size_t *size)
{
	char *buf = realloc(stream->buffer, stream->length + 1);
	if (buf == NULL)
		return MUSTACH_ERROR_OUT_OF_MEMORY;
	*buffer = buf;
	*size = stream->length;
	buf[stream->length] = 0;
	return MUSTACH_OK;
}

int mustach_stream_write(
		mustach_stream_t *stream,
		const char *buffer,
		size_t size
) {
	if (size >0) {
		size_t nlen = stream->length + size;
		if (nlen < size)/*detect overflow*/
			return MUSTACH_ERROR_TOO_BIG;
		if (nlen > stream->avail) {
			size_t nava = nlen + SZBLK;
			void *nbuf;
			if (nava < nlen || nava + 1 < nava)/*avoid overflow*/
				nava = SIZE_MAX - 1;
			nbuf = realloc(stream->buffer, nava + 1);
			if (nbuf == NULL)
				return MUSTACH_ERROR_OUT_OF_MEMORY;
			stream->buffer = nbuf;
			stream->avail = nava;
		}
		memcpy(&stream->buffer[stream->length], buffer, size);
		stream->length = nlen;
	}
	return MUSTACH_OK;
}

int mustach_stream_write_cb(
		void *closure,
		const char *buffer,
		size_t size
) {
	mustach_stream_t *stream = (mustach_stream_t*)closure;
	return mustach_stream_write(stream, buffer, size);
}

