/*
 Author: José Bollo <jobol@nonadev.net>

 https://gitlab.com/jobol/mustach

 SPDX-License-Identifier: ISC
*/

#define _GNU_SOURCE

#include <stdlib.h>
#include <stdio.h>
#include <sys/stat.h>
#include <unistd.h>
#include <fcntl.h>
#include <string.h>
#include <libgen.h>

#include "mustach-wrap.h"

static const size_t BLOCKSIZE = 8192;

static const char *errors[] = {
	"??? unreferenced ???",
	"system",
	"unexpected end",
	"empty tag",
	"tag too long",
	"bad separators",
	"too depth",
	"closing",
	"bad unescape tag",
	"invalid interface",
	"item not found",
	"partial not found",
	"undefined tag"
};

static const char *errmsg = 0;
static int flags = 0;
static FILE *output = 0;

static void help(char *prog)
{
	char *name = basename(prog);
#define STR_INDIR(x) #x
#define STR(x) STR_INDIR(x)
	printf("%s version %s\n", name, STR(VERSION));
#undef STR
#undef STR_INDIR
	printf(
		"\n"
		"USAGE:\n"
		"    %s [FLAGS] <json-file> <mustach-templates...>\n"
		"\n"
		"FLAGS:\n"
		"    -h, --help     Prints help information\n"
		"    -s, --strict   Error when a tag is undefined\n"
		"\n"
		"ARGS: (if a file is -, read standard input)\n"
		"    <json-file>              JSON file with input data\n"
		"    <mustach-templates...>   Template files to instanciate\n",
		name);
	exit(0);
}

static char *readfile(const char *filename, size_t *length)
{
	int f;
	struct stat s;
	char *result;
	size_t size, pos;
	ssize_t rc;

	result = NULL;
	if (filename[0] == '-' &&  filename[1] == 0)
		f = dup(0);
	else
		f = open(filename, O_RDONLY);
	if (f < 0) {
		fprintf(stderr, "Can't open file: %s\n", filename);
		exit(1);
	}

	fstat(f, &s);
	switch (s.st_mode & S_IFMT) {
	case S_IFREG:
		size = s.st_size;
		break;
	case S_IFSOCK:
	case S_IFIFO:
		size = BLOCKSIZE;
		break;
	default:
		fprintf(stderr, "Bad file: %s\n", filename);
		exit(1);
	}

	pos = 0;
	result = malloc(size + 1);
	do {
		if (result == NULL) {
			fprintf(stderr, "Out of memory\n");
			exit(1);
		}
		rc = read(f, &result[pos], (size - pos) + 1);
		if (rc < 0) {
			fprintf(stderr, "Error while reading %s\n", filename);
			exit(1);
		}
		if (rc > 0) {
			pos += (size_t)rc;
			if (pos > size) {
				size = pos + BLOCKSIZE;
				result = realloc(result, size + 1);
			}
		}
	} while(rc > 0);

	close(f);
	if (length != NULL)
		*length = pos;
	result[pos] = 0;
	return result;
}

static int load_json(const char *filename);
static int process(const char *content, size_t length);
static void close_json();

int main(int ac, char **av)
{
	char *t, *f;
	char *prog = *av;
	int s;
	size_t length;

	(void)ac; /* unused */
	flags = Mustach_With_AllExtensions;
	output = stdout;

	for( ++av ; av[0] && av[0][0] == '-' && av[0][1] != 0 ; av++) {
		if (!strcmp(*av, "-h") || !strcmp(*av, "--help"))
			help(prog);
		if (!strcmp(*av, "-s") || !strcmp(*av, "--strict"))
			flags |= Mustach_With_ErrorUndefined;
	}
	if (*av) {
		f = (av[0][0] == '-' && !av[0][1]) ? "/dev/stdin" : av[0];
		s = load_json(f);
		if (s < 0) {
			fprintf(stderr, "Can't load json file %s\n", av[0]);
			if(errmsg)
				fprintf(stderr, "   reason: %s\n", errmsg);
			exit(1);
		}
		while(*++av) {
			t = readfile(*av, &length);
			s = process(t, length);
			free(t);
			if (s != MUSTACH_OK) {
				s = -s;
				if (s < 1 || s >= (int)(sizeof errors / sizeof * errors))
					s = 0;
				fprintf(stderr, "Template error %s (file %s)\n", errors[s], *av);
			}
		}
		close_json();
	}
	return 0;
}

#define MUSTACH_TOOL_JSON_C  1
#define MUSTACH_TOOL_JANSSON 2
#define MUSTACH_TOOL_CJSON   3

#if TOOL == MUSTACH_TOOL_JSON_C

#include "mustach-json-c.h"

static struct json_object *o;
static int load_json(const char *filename)
{
	o = json_object_from_file(filename);
#if JSON_C_VERSION_NUM >= 0x000D00
	errmsg = json_util_get_last_err();
	if (errmsg != NULL)
		return -1;
#endif
	if (o == NULL) {
		errmsg = "null json";
		return -1;
	}
	return 0;
}
static int process(const char *content, size_t length)
{
	return mustach_json_c_file(content, length, o, flags, output);
}
static void close_json()
{
	json_object_put(o);
}

#elif TOOL == MUSTACH_TOOL_JANSSON

#include "mustach-jansson.h"

static json_t *o;
static json_error_t e;
static int load_json(const char *filename)
{
	o = json_load_file(filename, JSON_DECODE_ANY, &e);
	if (o == NULL) {
		errmsg = e.text;
		return -1;
	}
	return 0;
}
static int process(const char *content, size_t length)
{
	return mustach_jansson_file(content, length, o, flags, output);
}
static void close_json()
{
	json_decref(o);
}

#elif TOOL == MUSTACH_TOOL_CJSON

#include "mustach-cjson.h"

static cJSON *o;
static int load_json(const char *filename)
{
	char *t;
	size_t length;

	t = readfile(filename, &length);
	o = t ? cJSON_ParseWithLength(t, length) : NULL;
	free(t);
	return -!o;
}
static int process(const char *content, size_t length)
{
	return mustach_cJSON_file(content, length, o, flags, output);
}
static void close_json()
{
	cJSON_Delete(o);
}

#else
#error "no defined json library"
#endif
