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

#define MUSTACH_TOOL_JSON_C  1
#define MUSTACH_TOOL_JANSSON 2
#define MUSTACH_TOOL_CJSON   3

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
	"partial not found"
};

static void help(char *prog)
{
	char *name = basename(prog);
#define STR(x) #x
	printf("%s version %s\n", name, STR(VERSION));
#undef STR
	printf("usage: %s json-file mustach-templates...\n", name);
	exit(0);
}

static char *readfile(const char *filename)
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
	result[pos] = 0;
	return result;
}

#if TOOL == MUSTACH_TOOL_JSON_C

#include "mustach-json-c.h"

int main(int ac, char **av)
{
	struct json_object *o;
	char *t, *f;
	char *prog = *av;
	int s;

	(void)ac; /* unused */

	if (*++av) {
		if (!strcmp(*av, "-h") || !strcmp(*av, "--help"))
			help(prog);
		f = (av[0][0] == '-' && !av[0][1]) ? "/dev/stdin" : av[0];
		o = json_object_from_file(f);
#if JSON_C_VERSION_NUM >= 0x000D00
		if (json_util_get_last_err() != NULL) {
			fprintf(stderr, "Bad json: %s (file %s)\n", json_util_get_last_err(), av[0]);
			exit(1);
		}
		else
#endif
		if (o == NULL) {
			fprintf(stderr, "Aborted: null json (file %s)\n", av[0]);
			exit(1);
		}
		while(*++av) {
			t = readfile(*av);
			s = mustach_json_c_file(t, o, Mustach_With_ALL, stdout);
			if (s != 0) {
				s = -s;
				if (s < 1 || s >= (int)(sizeof errors / sizeof * errors))
					s = 0;
				fprintf(stderr, "Template error %s (file %s)\n", errors[s], *av);
			}
			free(t);
		}
		json_object_put(o);
	}
	return 0;
}

#elif TOOL == MUSTACH_TOOL_JANSSON

#include "mustach-jansson.h"

int main(int ac, char **av)
{
	json_t *o;
	json_error_t e;
	char *t, *f;
	char *prog = *av;
	int s;

	(void)ac; /* unused */

	if (*++av) {
		if (!strcmp(*av, "-h") || !strcmp(*av, "--help"))
			help(prog);
		f = (av[0][0] == '-' && !av[0][1]) ? "/dev/stdin" : av[0];
		o = json_load_file(f, JSON_DECODE_ANY, &e);
		if (o == NULL) {
			fprintf(stderr, "Bad json: %s (file %s)\n", e.text, av[0]);
			exit(1);
		}
		while(*++av) {
			t = readfile(*av);
			s = mustach_jansson_file(t, o, Mustach_With_ALL, stdout);
			if (s != 0) {
				s = -s;
				if (s < 1 || s >= (int)(sizeof errors / sizeof * errors))
					s = 0;
				fprintf(stderr, "Template error %s (file %s)\n", errors[s], *av);
			}
			free(t);
		}
		json_decref(o);
	}
	return 0;
}

#elif TOOL == MUSTACH_TOOL_CJSON

#include "mustach-cjson.h"

int main(int ac, char **av)
{
	cJSON *o;
	char *t, *f;
	char *prog = *av;
	int s;

	(void)ac; /* unused */

	if (*++av) {
		if (!strcmp(*av, "-h") || !strcmp(*av, "--help"))
			help(prog);
		f = (av[0][0] == '-' && !av[0][1]) ? "/dev/stdin" : av[0];
		t = readfile(f);
		o = f ? cJSON_Parse(t) : NULL;
		free(t);
		if (o == NULL) {
			fprintf(stderr, "Can't Load JSON file %s\n", f);
			exit(1);
		}
		while(*++av) {
			t = readfile(*av);
			s = mustach_cJSON_file(t, o, Mustach_With_ALL, stdout);
			if (s != 0) {
				s = -s;
				if (s < 1 || s >= (int)(sizeof errors / sizeof * errors))
					s = 0;
				fprintf(stderr, "Template error %s (file %s)\n", errors[s], *av);
			}
			free(t);
		}
		cJSON_Delete(o);
	}
	return 0;
}

#else
#error "no defined json library"
#endif
