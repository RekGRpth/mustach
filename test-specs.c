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
#include <errno.h>

#include "mustach-wrap.h"

#define TEST_JSON_C  1
#define TEST_JANSSON 2
#define TEST_CJSON   3

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

const char *mustach_error_string(int status)
{
	return status >= 0 ? "no error"
		: errors[status <= -(int)(sizeof errors / sizeof * errors) ? 0 : -status];
}

static const char *errmsg = 0;
static int flags = 0;
static FILE *output = 0;

static void help(char *prog)
{
	char *name = basename(prog);
#define STR(x) #x
	printf("%s version %s\n", name, STR(VERSION));
#undef STR
	printf("usage: %s test-files...\n", name);
	exit(0);
}

#if TEST == TEST_CJSON

static const size_t BLOCKSIZE = 8192;

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
#endif

const char partialdir[] = "tmp-partials";
int in_partials = 0;

int enter_partials()
{
	if (in_partials)
		return 0;
	if (chdir(partialdir) < 0) {
		fprintf(stderr, "can't enter directory %s: %s", partialdir, strerror(errno));
		exit(1);
	}
	in_partials = 1;
	return 0;
}

int leave_partials()
{
	if (!in_partials)
		return 0;
	if (chdir("..") < 0)
		return -1;
	in_partials = 0;
	return 0;
}

int add_partial(const char *name, const char *value)
{
	FILE *f;

	enter_partials();
	f = fopen(name, "w");
	if (f == NULL) {
		fprintf(stderr, "can't create the partial file %s\n", name);
		return -1;
	}
	fprintf(f, "%s", value);
	fclose(f);
	return 0;
}

int remove_partial(const char *name)
{
	enter_partials();
	unlink(name);
	return 0;
}

typedef struct {
	unsigned nerror;
	unsigned ndiffers;
	unsigned nsuccess;
	unsigned ninvalid;
} counters;

static int load_json(const char *filename);
static int process(counters *c);
static void close_json();

int main(int ac, char **av)
{
	char *f;
	char *prog = *av;
	int s;
	counters c;

	(void)ac; /* unused */
	flags = Mustach_With_SingleDot | Mustach_With_IncPartial;
	output = stdout;

	memset(&c, 0, sizeof c);
	while (*++av) {
		if (!strcmp(*av, "-h") || !strcmp(*av, "--help"))
			help(prog);
		f = (av[0][0] == '-' && !av[0][1]) ? "/dev/stdin" : av[0];
		fprintf(output, "\nloading %s\n", f);
		s = load_json(f);
		if (s < 0) {
			fprintf(stderr, "error when loading %s!\n", f);
			if(errmsg)
				fprintf(stderr, "   reason: %s\n", errmsg);
			exit(1);
		}
		fprintf(output, "processing file %s\n", f);
		s = process(&c);
		if (s < 0) {
			fprintf(stderr, "error bad test file %s!\n", f);
			exit(1);
		}
		close_json();
	}
	fprintf(output, "\nsummary:\n");
	if (c.ninvalid)
		fprintf(output, "  invalid %u\n", c.ninvalid);
	fprintf(output, "  error   %u\n", c.nerror);
	fprintf(output, "  differ  %u\n", c.ndiffers);
	fprintf(output, "  success %u\n", c.nsuccess);
	if (c.nerror)
		return 2;
	if (c.ndiffers)
		return 1;
	return 0;
}

void emit(FILE *f, const char *s)
{
	for(;;s++) {
		switch(*s) {
		case 0: return;
		case '\\': fprintf(f, "\\\\"); break;
		case '\t': fprintf(f, "\\t"); break;
		case '\n': fprintf(f, "\\n"); break;
		case '\r': fprintf(f, "\\r"); break;
		default: fprintf(f, "%c", *s); break;
		}
	}
}

#if TEST == TEST_JSON_C

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
static int process(counters *c)
{
	const char *t, *e, *partname, *partval;
	char *got;
	unsigned i, n;
	size_t length;
	int s;
	json_object *tests, *unit, *name, *desc, *data, *template, *expected, *partials;
	struct json_object_iterator itobj, endobj;

	if (!json_object_object_get_ex(o, "tests", &tests) || json_object_get_type(tests) != json_type_array)
		return -1;

	i = 0;
	n = (unsigned)json_object_array_length(tests);
	while (i < n) {
		unit = json_object_array_get_idx(tests, i);
		if (json_object_get_type(unit) != json_type_object
		 || !json_object_object_get_ex(unit, "name", &name)
		 || !json_object_object_get_ex(unit, "desc", &desc)
		 || !json_object_object_get_ex(unit, "data", &data)
		 || !json_object_object_get_ex(unit, "template", &template)
		 || !json_object_object_get_ex(unit, "expected", &expected)
		 || json_object_get_type(name) != json_type_string
		 || json_object_get_type(desc) != json_type_string
		 || json_object_get_type(template) != json_type_string
		 || json_object_get_type(expected) != json_type_string) {
			fprintf(stderr, "invalid test %u\n", i);
			c->ninvalid++;
		}
		else {
			fprintf(output, "[%u] %s\n", i, json_object_get_string(name));
			fprintf(output, "\t%s\n", json_object_get_string(desc));
			if (!json_object_object_get_ex(unit, "partials", &partials))
				partials = NULL;
			if (partials && json_object_get_type(partials) == json_type_object) {
				enter_partials();
				itobj = json_object_iter_begin(partials);
				endobj = json_object_iter_end(partials);
				while (!json_object_iter_equal(&itobj, &endobj)) {
					partname = json_object_iter_peek_name(&itobj);
					partval = json_object_get_string(json_object_iter_peek_value(&itobj));
					add_partial(partname, partval);
					json_object_iter_next(&itobj);
				}
			}
			t = json_object_get_string(template);
			e = json_object_get_string(expected);
			s = mustach_json_c_mem(t, 0, data, flags, &got, &length);
			if (s == 0 && strcmp(got, e) == 0) {
				fprintf(output, "\t=> SUCCESS\n");
				c->nsuccess++;
			}
			else {
				if (s < 0) {
					fprintf(output, "\t=> ERROR %s\n", mustach_error_string(s));
					c->nerror++;
				}
				else {
					fprintf(output, "\t=> DIFFERS\n");
					c->ndiffers++;
				}
				if (partials)
					fprintf(output, "\t.. PARTIALS[%s]\n", json_object_to_json_string_ext(partials, 0));
				fprintf(output, "\t..     DATA[%s]\n", json_object_to_json_string_ext(data, 0));
				fprintf(output, "\t.. TEMPLATE[");
				emit(output, t);
				fprintf(output, "]\n");
				fprintf(output, "\t.. EXPECTED[");
				emit(output, e);
				fprintf(output, "]\n");
				if (s == 0) {
					fprintf(output, "\t..      GOT[");
					emit(output, got);
					fprintf(output, "]\n");
				}
			}
			if (partials && json_object_get_type(partials) == json_type_object) {
				itobj = json_object_iter_begin(partials);
				endobj = json_object_iter_end(partials);
				while (!json_object_iter_equal(&itobj, &endobj)) {
					partname = json_object_iter_peek_name(&itobj);
					remove_partial(partname);
					json_object_iter_next(&itobj);
				}
			}
			leave_partials();
			free(got);
		}
		i++;
	}
	return 0;
}
static void close_json()
{
	json_object_put(o);
}

#elif TEST == TEST_JANSSON

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
static int process(counters *c)
{
	const char *t, *e, *partname, *partval;
	char *got, *tmp;
	int i, n;
	size_t length;
	int s;
	json_t *tests, *unit, *name, *desc, *data, *template, *expected, *partials;
	void *itobj;

	tests = json_object_get(o, "tests");
	if (!tests || json_typeof(tests) != JSON_ARRAY)
		return -1;

	i = 0;
	n = json_array_size(tests);
	while (i < n) {
		unit = json_array_get(tests, i);
		if (!unit || json_typeof(unit) != JSON_OBJECT
		 || !(name = json_object_get(unit, "name"))
		 || !(desc = json_object_get(unit, "desc"))
		 || !(data = json_object_get(unit, "data"))
		 || !(template = json_object_get(unit, "template"))
		 || !(expected = json_object_get(unit, "expected"))
		 || json_typeof(name) != JSON_STRING
		 || json_typeof(desc) != JSON_STRING
		 || json_typeof(template) != JSON_STRING
		 || json_typeof(expected) != JSON_STRING) {
			fprintf(stderr, "invalid test %u\n", i);
			c->ninvalid++;
		}
		else {
			fprintf(output, "[%u] %s\n", i, json_string_value(name));
			fprintf(output, "\t%s\n", json_string_value(desc));
			partials = json_object_get(unit, "partials");
			if (partials && json_typeof(partials) == JSON_OBJECT) {
				enter_partials();
				itobj = json_object_iter(partials);
				while (itobj) {
					partname = json_object_iter_key(itobj);
					partval = json_string_value(json_object_iter_value(itobj));
					add_partial(partname, partval);
					itobj = json_object_iter_next(partials, itobj);
				}
			}
			t = json_string_value(template);
			e = json_string_value(expected);
			s = mustach_jansson_mem(t, 0, data, flags, &got, &length);
			if (s == 0 && strcmp(got, e) == 0) {
				fprintf(output, "\t=> SUCCESS\n");
				c->nsuccess++;
			}
			else {
				if (s < 0) {
					fprintf(output, "\t=> ERROR %s\n", mustach_error_string(s));
					c->nerror++;
				}
				else {
					fprintf(output, "\t=> DIFFERS\n");
					c->ndiffers++;
				}
				if (partials) {
					tmp = json_dumps(partials, JSON_ENCODE_ANY | JSON_COMPACT);
					fprintf(output, "\t.. PARTIALS[%s]\n", tmp);
					free(tmp);
				}
				tmp = json_dumps(data, JSON_ENCODE_ANY | JSON_COMPACT);
				fprintf(output, "\t..     DATA[%s]\n", tmp);
				free(tmp);
				fprintf(output, "\t.. TEMPLATE[");
				emit(output, t);
				fprintf(output, "]\n");
				fprintf(output, "\t.. EXPECTED[");
				emit(output, e);
				fprintf(output, "]\n");
				if (s == 0) {
					fprintf(output, "\t..      GOT[");
					emit(output, got);
					fprintf(output, "]\n");
				}
			}
			if (partials && json_typeof(partials) == JSON_OBJECT) {
				itobj = json_object_iter(partials);
				while (itobj) {
					partname = json_object_iter_key(itobj);
					remove_partial(partname);
					itobj = json_object_iter_next(partials, itobj);
				}
			}
			leave_partials();
			free(got);
		}
		i++;
	}
	return 0;
}
static void close_json()
{
	json_decref(o);
}

#elif TEST == TEST_CJSON

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
static int process(counters *c)
{
	const char *t, *e, *partname, *partval;
	char *got, *tmp;
	int i, n;
	size_t length;
	int s;
	cJSON *tests, *unit, *name, *desc, *data, *template, *expected, *partials, *itobj;

	tests = cJSON_GetObjectItemCaseSensitive(o, "tests");
	if (!tests || tests->type != cJSON_Array)
		return -1;

	i = 0;
	n = cJSON_GetArraySize(tests);
	while (i < n) {
		unit = cJSON_GetArrayItem(tests, i);
		if (!unit || unit->type != cJSON_Object
		 || !(name = cJSON_GetObjectItemCaseSensitive(unit, "name"))
		 || !(desc = cJSON_GetObjectItemCaseSensitive(unit, "desc"))
		 || !(data = cJSON_GetObjectItemCaseSensitive(unit, "data"))
		 || !(template = cJSON_GetObjectItemCaseSensitive(unit, "template"))
		 || !(expected = cJSON_GetObjectItemCaseSensitive(unit, "expected"))
		 || name->type != cJSON_String
		 || desc->type != cJSON_String
		 || template->type != cJSON_String
		 || expected->type != cJSON_String) {
			fprintf(stderr, "invalid test %u\n", i);
			c->ninvalid++;
		}
		else {
			fprintf(output, "[%u] %s\n", i, name->valuestring);
			fprintf(output, "\t%s\n", desc->valuestring);
			partials = cJSON_GetObjectItemCaseSensitive(unit, "partials");
			if (partials && partials->type == cJSON_Object) {
				enter_partials();
				itobj = partials->child;
				while (itobj) {
					partname = itobj->string;
					partval = itobj->valuestring;
					add_partial(partname, partval);
					itobj = itobj->next;
				}
			}
			t = template->valuestring;
			e = expected->valuestring;
			s = mustach_cJSON_mem(t, 0, data, flags, &got, &length);
			if (s == 0 && strcmp(got, e) == 0) {
				fprintf(output, "\t=> SUCCESS\n");
				c->nsuccess++;
			}
			else {
				if (s < 0) {
					fprintf(output, "\t=> ERROR %s\n", mustach_error_string(s));
					c->nerror++;
				}
				else {
					fprintf(output, "\t=> DIFFERS\n");
					c->ndiffers++;
				}
				if (partials) {
					tmp = cJSON_PrintUnformatted(partials);
					fprintf(output, "\t.. PARTIALS[%s]\n", tmp);
					free(tmp);
				}
				tmp = cJSON_PrintUnformatted(data);
				fprintf(output, "\t..     DATA[%s]\n", tmp);
				free(tmp);
				fprintf(output, "\t.. TEMPLATE[");
				emit(output, t);
				fprintf(output, "]\n");
				fprintf(output, "\t.. EXPECTED[");
				emit(output, e);
				fprintf(output, "]\n");
				if (s == 0) {
					fprintf(output, "\t..      GOT[");
					emit(output, got);
					fprintf(output, "]\n");
				}
			}
			if (partials && partials->type == cJSON_Object) {
				itobj = partials->child;
				while (itobj) {
					partname = itobj->string;
					remove_partial(partname);
					itobj = itobj->next;
				}
			}
			leave_partials();
			free(got);
		}
		i++;
	}
	return 0;
}
static void close_json()
{
	cJSON_Delete(o);
}

#else
#error "no defined json library"
#endif
