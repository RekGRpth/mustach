/*
 Author: José Bollo <jobol@nonadev.net>

 https://gitlab.com/jobol/mustach

 SPDX-License-Identifier: 0BSD
*/

#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif

#include "mustach-wrap.h"
#include "mustach-helpers.h"

#include <stdlib.h>
#include <stdio.h>
#include <sys/stat.h>
#include <unistd.h>
#include <fcntl.h>
#include <string.h>
#include <libgen.h>

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
	"undefined tag",
	"too much template nesting"
};

static const char *errmsg = 0;
static int flags = 0;
static FILE *output = 0;
static mustach_template_t *templ;

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
		"    %s [FLAGS] <mustach-templates> <json-file...>\n"
		"\n"
		"FLAGS:\n"
		"    -h, --help     Prints help information\n"
		"    -s, --strict   Error when a tag is undefined\n"
		"\n"
		"ARGS: (if a file is -, read standard input)\n"
		"    <mustach-templates...>   Template file\n"
		"    <json-file>              JSON files to render\n",
		name);
	exit(0);
}

static int load_json(const char *filename);
static int apply();
static void close_json();

int main(int ac, char **av)
{
	char *f;
	char *prog = *av;
	int s;
	mustach_sbuf_t tbuf;

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
		/* load template file */
		s = mustach_read_file(av[0], &tbuf);
		if (s < 0) {
			fprintf(stderr, "Can't load file %s\n", av[0]);
			if(errmsg)
				fprintf(stderr, "   reason: %s\n", errmsg);
			exit(1);
		}

		/* prepare template */
		s = mustach_make_template(&templ, 0, &tbuf, av[0]);
		if (s < 0) {
			fprintf(stderr, "Can't prepare template %s\n", av[0]);
			if(errmsg)
				fprintf(stderr, "   reason: %s\n", errmsg);
			exit(1);
		}

		/* process the json files */
		while(*++av) {
			f = (av[0][0] == '-' && !av[0][1]) ? "/dev/stdin" : av[0];
			s = load_json(f);

			s = apply();

			close_json();

			if (s != MUSTACH_OK) {
				s = -s;
				if (s < 1 || s >= (int)(sizeof errors / sizeof * errors))
					s = 0;
				fprintf(stderr, "Error %s when redering %s\n", errors[s], av[0]);
			}
		}
		mustach_destroy_template(templ, NULL, NULL);
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
static int apply()
{
	return mustach_json_c_apply(templ, o, flags, mustach_fwrite_cb, NULL, output);
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
static int apply()
{
	return mustach_jansson_apply(templ, o, flags, mustach_fwrite_cb, NULL, output);
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
	mustach_sbuf_t buf = MUSTACH_SBUF_INIT;
	int s = mustach_read_file(filename, &buf);
	o = s == MUSTACH_OK ? cJSON_ParseWithLength(buf.value, mustach_sbuf_length(&buf)) : NULL;
	mustach_sbuf_release(&buf);
	return -!o;
}
static int apply()
{
	return mustach_cJSON_apply(templ, o, flags, mustach_fwrite_cb, NULL, output);
}
static void close_json()
{
	cJSON_Delete(o);
}

#else
#error "no defined json library"
#endif
