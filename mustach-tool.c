/*
 Author: José Bollo <jobol@nonadev.net>
 Author: José Bollo <jose.bollo@iot.bzh>

 https://gitlab.com/jobol/mustach

 Licensed under the Apache License, Version 2.0 (the "License");
 you may not use this file except in compliance with the License.
 You may obtain a copy of the License at

     http://www.apache.org/licenses/LICENSE-2.0

 Unless required by applicable law or agreed to in writing, software
 distributed under the License is distributed on an "AS IS" BASIS,
 WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 See the License for the specific language governing permissions and
 limitations under the License.
*/

#define _GNU_SOURCE

#include <stdlib.h>
#include <stdio.h>
#include <sys/stat.h>
#include <unistd.h>
#include <fcntl.h>
#include <string.h>
#include <libgen.h>

#include <json-c/json.h>

#include "mustach-json-c.h"

static const char *errors[] = {
	"??? unreferenced ???",
	"system",
	"unexpected end",
	"empty tag",
	"tag too long",
	"bad separators",
	"too depth",
	"closing",
	"bad unescape tag"
};

static void help(char *prog)
{
	printf("usage: %s json-file mustach-templates...\n", basename(prog));
	exit(0);
}

static char *readfile(const char *filename)
{
	struct stat s;
	int f;
	char *result;

	f = open(filename, O_RDONLY);
	fstat(f, &s);
	result = malloc(s.st_size + 1);
	read(f, result, s.st_size);
	close(f);
	result[s.st_size] = 0;
	return result;
}

int main(int ac, char **av)
{
	struct json_object *o;
	char *t;
	char *prog = *av;
	int s;

	if (*++av) {
		if (!strcmp(*av, "-h") || !strcmp(*av, "--help"))
			help(prog);
		o = json_object_from_file(*av);
#if JSON_C_VERSION_NUM >= 0x000D00
		if (json_util_get_last_err() != NULL) {
			fprintf(stderr, "Bad json: %s (file %s)\n", json_util_get_last_err(), *av);
			exit(1);
		}
		else
#endif
		if (o == NULL) {
			fprintf(stderr, "Aborted: null json (file %s)\n", *av);
			exit(1);
		}
		while(*++av) {
			t = readfile(*av);
			s = fmustach_json_c(t, o, stdout);
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

