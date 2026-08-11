#define JSMN_STATIC
#include "jsmn.h"

#include "mustach.h"
#include "mustach-wrap.h"
#include "mustach-jsmn.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

struct frame {
    int container;   /* token index of the array being iterated, or -1 */
    int is_objiter;
    int index;
    int count;
    int value;        /* token index of the current value */
    int key;          /* token index of the current key (objiter only) */
};

struct expl {
    const char *json;
    jsmntok_t *tokens;
    int selection;    /* token index, or -1 for "no value" */
    int depth;
    struct frame stack[MUSTACH_MAX_DEPTH];
};

static int tok_len(jsmntok_t *t) { return t->end - t->start; }

static int tok_eq(const char *json, jsmntok_t *tokens, int idx, const char *name, size_t namelen) {
    jsmntok_t *t = &tokens[idx];
    return (size_t) tok_len(t) == namelen && !memcmp(json + t->start, name, namelen);
}

/* Skip past the whole subtree rooted at tokens[i], returning the index of
 * the token right after it. Relies on jsmn's convention that an object's
 * or array's `size` is its number of direct children (for an object, a
 * member key's own `size` is 1, standing for its value), and that a
 * subtree's tokens always immediately follow its own token in document
 * order -- the standard jsmn traversal idiom. */
static int skip(jsmntok_t *tokens, int i) {
    int end = i + 1, j, n = tokens[i].size;
    for (j = 0; j < n; j++)
        end = skip(tokens, end);
    return end;
}

int mustach_jsmn_find(const char *json, jsmntok_t *tokens, int container, const char *name) {
    size_t namelen = strlen(name);
    int idx = container + 1, j, n = tokens[container].size;
    for (j = 0; j < n; j++) {
        int key = idx, value = key + 1;
        if (tok_eq(json, tokens, key, name, namelen)) return value;
        idx = skip(tokens, value);
    }
    return -1;
}

int mustach_jsmn_index(jsmntok_t *tokens, int container, int n) {
    int idx = container + 1, j;
    for (j = 0; j < n; j++) idx = skip(tokens, idx);
    return idx;
}

static int is_zero_number(const char *s, int len) {
    int i;
    for (i = 0; i < len; i++)
        if (s[i] >= '1' && s[i] <= '9')
            return 0;
    return 1;
}

static int is_truthy(struct expl *e, int idx) {
    jsmntok_t *t;
    int len;
    const char *s;
    if (idx < 0) return 0;
    t = &e->tokens[idx];
    len = tok_len(t);
    s = e->json + t->start;
    switch (t->type) {
        case JSMN_OBJECT: case JSMN_ARRAY: return t->size > 0;
        case JSMN_STRING: return len > 0;
        case JSMN_PRIMITIVE:
            if (len == 4 && !memcmp(s, "true", 4)) return 1;
            if (len == 5 && !memcmp(s, "false", 5)) return 0;
            if (len == 4 && !memcmp(s, "null", 4)) return 0;
            return !is_zero_number(s, len);
        default: return 0;
    }
}

const char *mustach_jsmn_string(const char *json, jsmntok_t *t, size_t *outlen, int *alloc) {
    const char *s = json + t->start;
    int len = tok_len(t), i;
    char *out, *o;
    *alloc = 0;
    if (len == 0) {
        /* struct mustach_sbuf convention: length 0 with a non-NULL value
         * means "value is NUL-terminated, take its strlen()". A slice
         * into 'json' at this point isn't NUL-terminated, so returning
         * it here would make callers read past the intended empty
         * string into whatever follows in the json buffer. */
        *outlen = 0;
        return "";
    }
    for (i = 0; i < len; i++)
        if (s[i] == '\\')
            break;
    if (i == len) {
        *outlen = (size_t) len;
        return s;
    }
    out = malloc((size_t) len ? (size_t) len : 1);
    if (!out) { *outlen = 0; return ""; }
    o = out;
    for (i = 0; i < len; i++) {
        if (s[i] != '\\' || i + 1 >= len) { *o++ = s[i]; continue; }
        i++;
        switch (s[i]) {
            case '"': *o++ = '"'; break;
            case '\\': *o++ = '\\'; break;
            case '/': *o++ = '/'; break;
            case 'b': *o++ = '\b'; break;
            case 'f': *o++ = '\f'; break;
            case 'n': *o++ = '\n'; break;
            case 'r': *o++ = '\r'; break;
            case 't': *o++ = '\t'; break;
            case 'u': {
                unsigned cp = 0, k;
                if (i + 4 < len) {
                    for (k = 1; k <= 4; k++) {
                        char c = s[i + k];
                        cp <<= 4;
                        if (c >= '0' && c <= '9') cp |= (unsigned) (c - '0');
                        else if (c >= 'a' && c <= 'f') cp |= (unsigned) (c - 'a' + 10);
                        else if (c >= 'A' && c <= 'F') cp |= (unsigned) (c - 'A' + 10);
                    }
                    i += 4;
                    if (cp >= 0xd800 && cp <= 0xdbff && i + 6 < len && s[i + 1] == '\\' && s[i + 2] == 'u') {
                        unsigned lo = 0;
                        for (k = 3; k <= 6; k++) {
                            char c = s[i + k];
                            lo <<= 4;
                            if (c >= '0' && c <= '9') lo |= (unsigned) (c - '0');
                            else if (c >= 'a' && c <= 'f') lo |= (unsigned) (c - 'a' + 10);
                            else if (c >= 'A' && c <= 'F') lo |= (unsigned) (c - 'A' + 10);
                        }
                        if (lo >= 0xdc00 && lo <= 0xdfff) {
                            cp = 0x10000 + ((cp - 0xd800) << 10) + (lo - 0xdc00);
                            i += 6;
                        }
                    }
                }
                if (cp < 0x80) *o++ = (char) cp;
                else if (cp < 0x800) {
                    *o++ = (char) (0xC0 | (cp >> 6));
                    *o++ = (char) (0x80 | (cp & 0x3F));
                } else if (cp < 0x10000) {
                    *o++ = (char) (0xE0 | (cp >> 12));
                    *o++ = (char) (0x80 | ((cp >> 6) & 0x3F));
                    *o++ = (char) (0x80 | (cp & 0x3F));
                } else {
                    *o++ = (char) (0xF0 | (cp >> 18));
                    *o++ = (char) (0x80 | ((cp >> 12) & 0x3F));
                    *o++ = (char) (0x80 | ((cp >> 6) & 0x3F));
                    *o++ = (char) (0x80 | (cp & 0x3F));
                }
                break;
            }
            default: *o++ = s[i]; break;
        }
    }
    *outlen = (size_t) (o - out);
    *alloc = 1;
    return out;
}

static int start(void *closure) {
    struct expl *e = closure;
    e->depth = 0;
    e->stack[0].value = 0; /* token 0 is always the root */
    e->selection = 0;
    return MUSTACH_OK;
}

static int compare(void *closure, const char *value) {
    struct expl *e = closure;
    jsmntok_t *t;
    const char *s;
    size_t slen;
    int alloc, c;
    size_t vlen, minlen;
    if (e->selection < 0) return strcmp("", value);
    t = &e->tokens[e->selection];
    switch (t->type) {
        case JSMN_PRIMITIVE:
            s = e->json + t->start;
            if (tok_len(t) == 4 && !memcmp(s, "true", 4)) return strcmp("true", value);
            if (tok_len(t) == 5 && !memcmp(s, "false", 5)) return strcmp("false", value);
            if (tok_len(t) == 4 && !memcmp(s, "null", 4)) return strcmp("null", value);
            { double d = atof(s) - atof(value); return d < 0 ? -1 : d > 0 ? 1 : 0; }
        case JSMN_STRING:
            s = mustach_jsmn_string(e->json, t, &slen, &alloc);
            vlen = strlen(value);
            minlen = slen < vlen ? slen : vlen;
            c = minlen ? memcmp(s, value, minlen) : 0;
            if (c == 0) c = (int) slen - (int) vlen;
            if (alloc) free((void *) s);
            return c < 0 ? -1 : c > 0 ? 1 : 0;
        default:
            return 1;
    }
}

static int sel(void *closure, const char *name) {
    struct expl *e = closure;
    int i, r = 0, o = -1;
    if (name == NULL) {
        o = e->stack[e->depth].value;
        r = 1;
    } else {
        for (i = e->depth; i >= 0 && !r; i--) {
            int cur = e->stack[i].value;
            if (cur >= 0 && e->tokens[cur].type == JSMN_OBJECT) {
                o = mustach_jsmn_find(e->json, e->tokens, cur, name);
                r = o >= 0;
            }
        }
    }
    e->selection = o;
    return r;
}

static int subsel(void *closure, const char *name) {
    struct expl *e = closure;
    jsmntok_t *t;
    int o = -1, r = 0;
    if (e->selection >= 0) {
        t = &e->tokens[e->selection];
        if (t->type == JSMN_OBJECT) {
            o = mustach_jsmn_find(e->json, e->tokens, e->selection, name);
            r = o >= 0;
        } else if (t->type == JSMN_ARRAY && *name) {
            char *end;
            long idx = strtol(name, &end, 10);
            if (!*end && idx >= 0 && idx < t->size) {
                o = mustach_jsmn_index(e->tokens, e->selection, (int) idx);
                r = 1;
            }
        }
    }
    if (r) e->selection = o;
    return r;
}

static int enter(void *closure, int objiter) {
    struct expl *e = closure;
    int o = e->selection;
    struct frame *f;
    if (++e->depth >= MUSTACH_MAX_DEPTH) return MUSTACH_ERROR_TOO_DEEP;
    f = &e->stack[e->depth];
    f->is_objiter = 0;
    f->container = -1;
    if (objiter) {
        if (o < 0 || e->tokens[o].type != JSMN_OBJECT || e->tokens[o].size == 0) goto not_entering;
        f->is_objiter = 1;
        f->container = o;
        f->index = 0;
        f->count = e->tokens[o].size;
        f->key = o + 1;
        f->value = f->key + 1;
    } else if (o >= 0 && e->tokens[o].type == JSMN_ARRAY) {
        if (e->tokens[o].size == 0) goto not_entering;
        f->container = o;
        f->index = 0;
        f->count = e->tokens[o].size;
        f->value = o + 1;
    } else if (is_truthy(e, o)) {
        f->value = o;
    } else
        goto not_entering;
    return 1;
not_entering:
    e->depth--;
    return 0;
}

static int next(void *closure) {
    struct expl *e = closure;
    struct frame *f;
    if (e->depth <= 0) return MUSTACH_ERROR_CLOSING;
    f = &e->stack[e->depth];
    if (f->is_objiter) {
        int nk = skip(e->tokens, f->value);
        if (++f->index >= f->count) return 0;
        f->key = nk;
        f->value = nk + 1;
        return 1;
    }
    if (f->container >= 0) {
        int ne = skip(e->tokens, f->value);
        if (++f->index >= f->count) return 0;
        f->value = ne;
        return 1;
    }
    return 0;
}

static int leave(void *closure) {
    struct expl *e = closure;
    if (e->depth <= 0) return MUSTACH_ERROR_CLOSING;
    e->depth--;
    return 0;
}

static int get(void *closure, struct mustach_sbuf *sbuf, int key) {
    struct expl *e = closure;
    jsmntok_t *t;
    const char *s;
    size_t slen;
    int alloc;
    if (key) {
        int d, k = -1;
        for (d = e->depth; d >= 0; d--)
            if (e->stack[d].is_objiter) { k = e->stack[d].key; break; }
        if (k >= 0) {
            s = mustach_jsmn_string(e->json, &e->tokens[k], &slen, &alloc);
            sbuf->value = s;
            sbuf->length = slen;
            if (alloc) sbuf->freecb = free;
        } else {
            sbuf->value = "";
            sbuf->length = 0;
        }
        return 1;
    }
    if (e->selection < 0) {
        sbuf->value = "";
        sbuf->length = 0;
        return 1;
    }
    t = &e->tokens[e->selection];
    switch (t->type) {
        case JSMN_STRING:
            s = mustach_jsmn_string(e->json, t, &slen, &alloc);
            sbuf->value = s;
            sbuf->length = slen;
            if (alloc) sbuf->freecb = free;
            break;
        case JSMN_PRIMITIVE:
            if (tok_len(t) == 4 && !memcmp(e->json + t->start, "null", 4)) {
                sbuf->value = "";
                sbuf->length = 0;
                break;
            }
            /* fall through */
        case JSMN_OBJECT:
        case JSMN_ARRAY:
            sbuf->value = e->json + t->start;
            sbuf->length = (size_t) tok_len(t);
            break;
        default:
            sbuf->value = "";
            sbuf->length = 0;
            break;
    }
    return 1;
}

const struct mustach_wrap_itf mustach_jsmn_wrap_itf = {
    .start = start,
    .stop = NULL,
    .compare = compare,
    .sel = sel,
    .subsel = subsel,
    .enter = enter,
    .next = next,
    .leave = leave,
    .get = get
};

int mustach_jsmn_parse(const char *json, size_t length, jsmntok_t **tokens, int *count) {
    jsmn_parser p;
    jsmntok_t *toks;
    int ntok;

    if (!length) { json = "{}"; length = 2; }

    jsmn_init(&p);
    ntok = jsmn_parse(&p, json, length, NULL, 0);
    if (ntok < 0) return MUSTACH_ERROR_BAD_DATA;
    if (!(toks = malloc((size_t) (ntok ? ntok : 1) * sizeof(*toks)))) return MUSTACH_ERROR_SYSTEM;

    jsmn_init(&p);
    if (jsmn_parse(&p, json, length, toks, (unsigned) ntok) < 0) { free(toks); return MUSTACH_ERROR_BAD_DATA; }

    *tokens = toks;
    *count = ntok;
    return MUSTACH_OK;
}

int mustach_jsmn_file(const char *templstr, size_t length, const char *json, jsmntok_t *tokens, int flags, FILE *file) {
    struct expl e;
    e.json = json;
    e.tokens = tokens;
    return mustach_wrap_file(templstr, length, &mustach_jsmn_wrap_itf, &e, flags, file);
}

int mustach_jsmn_fd(const char *templstr, size_t length, const char *json, jsmntok_t *tokens, int flags, int fd) {
    struct expl e;
    e.json = json;
    e.tokens = tokens;
    return mustach_wrap_fd(templstr, length, &mustach_jsmn_wrap_itf, &e, flags, fd);
}

int mustach_jsmn_mem(const char *templstr, size_t length, const char *json, jsmntok_t *tokens, int flags, char **result, size_t *size) {
    struct expl e;
    e.json = json;
    e.tokens = tokens;
    return mustach_wrap_mem(templstr, length, &mustach_jsmn_wrap_itf, &e, flags, result, size);
}

int mustach_jsmn_write(const char *templstr, size_t length, const char *json, jsmntok_t *tokens, int flags, mustach_write_cb_t *writecb, void *closure) {
    struct expl e;
    e.json = json;
    e.tokens = tokens;
    return mustach_wrap_write(templstr, length, &mustach_jsmn_wrap_itf, &e, flags, writecb, closure);
}

int mustach_jsmn_emit(const char *templstr, size_t length, const char *json, jsmntok_t *tokens, int flags, mustach_emit_cb_t *emitcb, void *closure) {
    struct expl e;
    e.json = json;
    e.tokens = tokens;
    return mustach_wrap_emit(templstr, length, &mustach_jsmn_wrap_itf, &e, flags, emitcb, closure);
}

int mustach_jsmn_apply(
        mustach_template_t *templstr,
        const char *json,
        jsmntok_t *tokens,
        int flags,
        mustach_write_cb_t *writecb,
        mustach_emit_cb_t *emitcb,
        void *closure
) {
    struct expl e;
    e.json = json;
    e.tokens = tokens;
    return mustach_wrap_apply(templstr, &mustach_jsmn_wrap_itf, &e, flags, writecb, emitcb, closure);
}
