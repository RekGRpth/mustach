/*
 Author: José Bollo <jobol@nonadev.net>

 https://gitlab.com/jobol/mustach

 SPDX-License-Identifier: 0BSD
*/

#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif

#include "mustach2.h"
#include "mustach-helpers.h"

#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#ifdef _WIN32
#include <malloc.h>
#endif

/* set the data count per template */
#ifndef DATA_COUNT
# define DATA_COUNT MUSTACHE_DATA_COUNT_MIN
#endif
#if DATA_COUNT < MUSTACHE_DATA_COUNT_MIN
# error "Unexpected DATA_COUNT < MUSTACHE_DATA_COUNT_MIN"
#endif

/* default guard is for memory manager: 2 pointers */
#ifndef GUARD_SIZE
# define GUARD_SIZE    (2 * sizeof(void*))
#endif

/* a simple print buffer for text of error */
#ifndef PBUFLEN
# define PBUFLEN 400
#endif

/* the print buffer is a basic buffer */
typedef
struct {
	/* the write position */
	unsigned pos;
	/* the write buffer */
	char buffer[PBUFLEN];
}
	pbuf_t;

/*
* The template is encoded using a code in the 4 lower
* bits of words. The 28 upper bits are used either for
* length, or, for jump address, or, not used at all.
* The code generally need more argument words (text,
* tag, address)
*/

/* operators */
typedef
enum op {
	/* 0. terminate the template: STOP(HERE) STOP(HERE) */
	op_stop,

	/* 1. set current line: LINE(LINE) */
	op_line,

	/* 2. text from the template: TEXT(LENTXT) TXT */
	op_text,

	/* 3. replace without escaping: REPLRAW(LENTAG) TAG */
	op_repl_raw,

	/* 4. replace with escaping: REPLESC(LENTAG) TAG */
	op_repl_esc,

	/* 5. replace with partial: PART(LENTAG) TAG */
	op_partial,

	/* 6. positive section: WHILE(LENTAG) TAG ADDR */
	op_while,

	/* 7. loop on next value: NEXT(ADDR) */
	op_next,

	/* 8. negative section: UNLESS(LENTAG) TAG ADDR */
	op_unless,

	/* 9. replace parent: PARENT(LENTAG) TAG ADDR */
	op_parent,

	/* 10. block: BLOCK(LENTAG) TAG ADDR */
	op_block,

	/* 11. end of parent or block: END */
	op_end,

	/* 12. prefix text from template: PREF(LENTXT) TXT */
	op_prefix,

	/* 13. TODO */
	op_unprefix,
}
	op_t;

/*
* The template is encoded using words of 32 bits.
* This is faily enough for almost any templating
* situation. Offsets in the template are using
* the full 32 bits and length a bit less (28).
*/

/* word type */
typedef uint32_t word_t;

/* word max value */
#define WORD_MAX UINT32_MAX

/*
* codes are made of 4 lower bits coding the operator
* and 28 upper bits coding the argument.
*/

/* bit count of operators */
#define WOPBITS      4
/* make an operator word */
#define MKW(op,val)  ((word_t)(op) | ((word_t)(val) << WOPBITS))
/* operator of an operator word */
#define WOP(x)       ((op_t)((word_t)(x) & ((1 << WOPBITS) - 1)))
/* value of an operator word */
#define WVAL(x)      ((word_t)(x) >> WOPBITS)
/* maximum value of an operator word */
#define WVAL_MAX     WVAL(WORD_MAX)

/*
* Memory is allocated using big blocks.
*
* If the template were encoded using small structures
* referencing themselves with pointers, the use of memory
* had be huge: pointer for reference are 64 or 32 bits
* and the memory allocator generally add data for management.
*
* So using no pointers but big array of words greatly reduce
* the requirement of memory.
*
* To achieve it, big templates are using more than one
* big array of words but several, each called a block
* (don't confused with blocks of templates {{$...}}).
*
* Address for referencing location in the template are
* using the lower 10 bits for offset in the block and
* the 22 upper bits for the index of the block.
*
* Note that no specific attention is made at the moment
* quickly access blocks, the search is linear. So for
* very big templates it could be optimized.
*/

/* bit count of block offset */
#define BOFFBITS   10
/* make an adress */
#define MKA(blk,off) (((word_t)(blk) << BOFFBITS) | (word_t)(off))
/* block of an adress */
#define ABLK(addr)   (((word_t)(addr)) >> BOFFBITS)
/* offset of an address */
#define AOFF(addr)   ((addr) & ((1 << BOFFBITS) - 1))

/*
* Blocks are double chained. It holds the count of words
* in the array words.
*
* The array of words is not of fixed size and is
* allocated with the block when the size is known.
*
* If integrated in an other structure, the block must
* be the last component of the structure.
*/

/* a block of code words */
typedef struct block block_t;
struct block {
	/* next block or NULL */
	block_t *next;
	/* previous block or NULL */
	block_t *prev;
	/* count of words in words */
	uint32_t count;
	/* the words of the block, as allocated */
	word_t words[];
};

/*
* A prepared template (main template or partial)
* is made of 2 pieces: the original template as a
* string, hold using the SBUF feature, and, the
* first encoded block.
*
* The first encoded block is allocated with the structure
*/

/* a prepared template for mains or partials */
struct mustach_template {
	/* the reference text */
	mustach_sbuf_t sbuf;
	/* flags */
	int flags;
	/* length of the name (without nul) */
	word_t length;
	/* null terminated name or NULL */
	const char *name;
	/* some user data */
	void *data[DATA_COUNT];
	/* the first block */
	block_t first_block;
};

/* structure for prefixing */
typedef struct pref pref_t;
struct pref {
	/* is it for adding ? */
	unsigned add;
	/* length of the prefix string */
	unsigned len;
	/* the prefix string */
	const char *start;
	/* link to the next prefix */
	pref_t *next;
};

/* structure for applying */
typedef struct ap ap_t;
struct ap {
	/* base of the text of the template */
	const char *base;
	/* current code block (copy of blk->words) */
	const word_t *words;
	/* apply's flag */
	int aflags;
	/* template's flag */
	int tflags;
	/* current index of code in 'words' */
	unsigned off;
	/* count of words in 'words' (copy of blk->count) */
	unsigned count;
	/* block index of 'words' */
	unsigned iblk;
	/* current line in template */
	unsigned line;
	/* is at begin of line ? */
	unsigned beoflin;
	/* nesting count */
	unsigned nesting;
	/* origin address of block list in a parent */
	unsigned orig;
	/* current prefix */
	pref_t *prefix;
	/* current block */
	const block_t *blk;
	/* interface */
	const mustach_apply_itf_t *itf;
	/* closure */
	void *closure;
	/* calling parent if any */
	ap_t *parent;
	/* the template if any */
	mustach_template_t *templ;
};

/* structure for extraction */
typedef struct ex ex_t;
struct ex {
	/* the reference text */
	mustach_sbuf_t sbuf;

	/* the interface */
	const mustach_build_itf_t *itf;
	/* the closure */
	void *closure;
	/* the flags */
	int flags; 

	/* indicates a standalone tag */
	unsigned alone;
	/* line number of the start of the text */
	word_t tline;
	/* indicates a pending text */
	unsigned pending;
	/* length of text before last EOL */
	word_t length1;
	/* length of text after last EOL */
	word_t length2;
	/* the pending text base */
	const char *text;

	/* last line encoded */
	word_t curline;
	/* not zero if in parent (used for tracking blocks only) */
	word_t inpar;
	/* current write offset */
	word_t curoff;
	/* count of available words */
	word_t offmax;
	/* index of the current block of words */
	word_t curblk;
	/* previous block of words or NULL */
	block_t *prvblk;

	/* next top stack index */
	unsigned stacktop;

	/* created template if any */
	mustach_template_t *templ;

	/* reserved size */
	unsigned reserved;
	/* length of possible name */
	unsigned lename;
	/* possible name */
	const char *name;

	/* array of coded words */
	word_t words[1 << BOFFBITS];

	/* the stack for tracking sections */
	word_t stack[3 * MUSTACH_MAX_DEPTH];
};

int (*mustach_global_partial_get)(
		const char *name,
		size_t length,
		struct mustach_sbuf *sbuf);

/*******************************************************************/
/*******************************************************************/
/** PART interface  ************************************************/
/*******************************************************************/
/*******************************************************************/

static void *alloc(
		size_t size,
		const mustach_build_itf_t *itf,
		void *closure
) {
	if (itf == NULL || itf->alloc == NULL)
		return malloc(size);
	return itf->alloc(size, closure);
}

static void dealloc(
		void *item,
		const mustach_build_itf_t *itf,
		void *closure
) {
	if (itf == NULL || itf->dealloc == NULL)
		return free(item);
	return itf->dealloc(item, closure);
}

/*******************************************************************/
/*******************************************************************/
/** PART print buffer **********************************************/
/*******************************************************************/
/*******************************************************************/

/* init pbuf */
static void pbuf_init(pbuf_t *pbuf)
{
	pbuf->pos = 0;
}

/* get the zero termiated string of pbuf*/
static const char *pbuf_text(pbuf_t *pbuf)
{
	if (pbuf->pos < sizeof pbuf->buffer)
		pbuf->buffer[pbuf->pos] = 0;
	else
		memcpy(&pbuf->buffer[sizeof pbuf->buffer - 4], "...", 4);
	return pbuf->buffer;
}

/* append a character to pbuf */
static void pbuf_put_char(pbuf_t *pbuf, char c)
{
	if (pbuf->pos < sizeof pbuf->buffer)
		pbuf->buffer[pbuf->pos++] = c;
}

/* append an unsigned integer to pbuf */
static void pbuf_put_unsigned(pbuf_t *pbuf, unsigned x)
{
	char c = (char)('0' + x % 10);
	x /= 10;
	if (x > 0)
		pbuf_put_unsigned(pbuf, x);
	pbuf_put_char(pbuf, c);
}

/* append a signed integer to pbuf */
#if 0
static void pbuf_put_signed(pbuf_t *pbuf, signed x)
{
	if (x < 0) {
		pbuf_put_char(pbuf, '-');
		x = -x;
	}
	pbuf_put_unsigned(pbuf, (unsigned)x);
}
#endif

/* append a string of length to pbuf */
static void pbuf_put_str_len(pbuf_t *pbuf, const char *str, size_t length)
{
	unsigned pbeg = pbuf->pos;
	if (pbeg < sizeof pbuf->buffer) {
		size_t tend = length + pbeg;
		unsigned pend = (unsigned)(tend > sizeof pbuf->buffer
					? sizeof pbuf->buffer : tend);
		memcpy(&pbuf->buffer[pbeg], str, pend - pbeg);
		pbuf->pos = pend;
	}
}

/* append a nul terminated string to pbuf */
static void pbuf_put_str(pbuf_t *pbuf, const char *str)
{
	pbuf_put_str_len(pbuf, str, strlen(str));
}

/*******************************************************************/
/*******************************************************************/
/** PART extraction / build  ***************************************/
/*******************************************************************/
/*******************************************************************/

/* basic error report */
static int exerr(ex_t *ex, int err, const char *text)
{
	if (ex->itf != NULL && ex->itf->error != NULL)
		ex->itf->error(ex->closure, err, text ?: "error");
	return err;
}

/* basic error report at line */
static int exerrl(ex_t *ex, int err, const char *text, word_t line)
{
	pbuf_t pbuf;
	pbuf_init(&pbuf);
	pbuf_put_str(&pbuf, "line ");
	pbuf_put_unsigned(&pbuf, line);
	pbuf_put_str(&pbuf, ": ");
	pbuf_put_str(&pbuf, text);
	return exerr(ex, err, pbuf_text(&pbuf));
}

/* report an out of memory error */
static int exerr_oom(ex_t *ex)
{
	return exerr(ex,
		MUSTACH_ERROR_OUT_OF_MEMORY,
		"out of memory");
}

/* report a too big template error */
static int exerr_too_big(ex_t *ex)
{
	return exerr(ex,
		MUSTACH_ERROR_TOO_BIG,
		"template is too big to be encoded");
}

/* report a too deep template error */
static int exerr_too_deep(ex_t *ex)
{
	return exerr(ex,
		MUSTACH_ERROR_TOO_DEEP,
		"section too deeply nested in template");
}

/* report an unmatched section closing error */
static int exerr_unmatched_closing(ex_t *ex)
{
	return exerr(ex,
		MUSTACH_ERROR_CLOSING,
		"unmatched closing in template");
}

/* report an mismatch section closing error */
static int exerr_mismatch_closing(ex_t *ex)
{
	return exerr(ex,
		MUSTACH_ERROR_CLOSING,
		"mismatch closing in template");
}

/* report an premature end error */
static int exerr_bad_end(ex_t *ex)
{
	return exerr(ex,
		MUSTACH_ERROR_UNEXPECTED_END,
		"unexpected end in template");
}

/* report a bad delimiter spec error */
static int exerr_bad_delimiter(ex_t *ex, word_t line)
{
	return exerrl(ex,
		MUSTACH_ERROR_BAD_DELIMITER,
		"bad delimiter specification",
		line);
}

/* report a bad unescape tag error */
static int exerr_bad_unesc(ex_t *ex, word_t line)
{
	return exerrl(ex,
		MUSTACH_ERROR_BAD_UNESCAPE_TAG,
		"badly formed unscape tag",
		line);
}

/* report an empty tag error */
static int exerr_empty_tag(ex_t *ex, word_t line)
{
	return exerrl(ex,
		MUSTACH_ERROR_EMPTY_TAG,
		"empty tag",
		line);
}

/* set the reserved size and compute the maximum word offset
 * the reserved size is the size of the structure
 * and name if any without words */
static void set_reserved(
		ex_t *ex,
		size_t size
) {
	/* available is computed such that the finally
	 * allocated size fits the maximum block length
	 * as given by (sizeof ex->words).
	 * Note that it is assumed that few last words of
	 * ex->words are never used. Can be optimized but
	 * the lose is just sizeof(struct block) = 3 words
	 * for 32 bits arch or 5 words in 64 bits arch */
	size_t avail = (sizeof ex->words) - GUARD_SIZE - size;
	ex->reserved = (unsigned)size;
	ex->offmax = (word_t)(avail / sizeof ex->words[0]);
}

/* initialisation of extraction structure */
static void initex(
		ex_t *ex,
		int flags,
		const mustach_sbuf_t *sbuf,
		const char *name,
		unsigned namelen,
		const mustach_build_itf_t *itf,
		void *closure
) {
	/* record parameters */
	ex->flags = flags;
	ex->sbuf = *sbuf;
	ex->name = name;
	ex->lename = namelen;
	ex->itf = itf;
	ex->closure = closure;

	/* nothing created */
	ex->templ = NULL;

	/* initial state */
	ex->pending = 0;
	ex->curline = 0;
	ex->inpar = 0;
	ex->curoff = 0;
	ex->curblk = 0;
	ex->prvblk = NULL;
	ex->stacktop = 0;

	/* compute sizes */
	namelen = name == NULL ? 0 : 1 + namelen;
	set_reserved(ex, namelen + sizeof *ex->templ);
}

/* Create a block from the current state
 * The block is either a template or a block */
static int store(ex_t *ex)
{
	char *name;
	block_t *blk;
	word_t count;
	size_t size;
	void *ptr;

	/* count of words in the created item */
	count = ex->curoff;

	/* compute size */
	size = ex->reserved + count * sizeof(word_t);

	/* allocate */
	ptr = alloc(size, ex->itf, ex->closure);
	if (ptr == NULL)
		return exerr_oom(ex);

	/* initialize */
	if (ex->templ != NULL)
		blk = ptr;
	else {
		mustach_template_t *templ = ptr;
		ex->templ = templ;
		/* copy the buffer */
		templ->sbuf = ex->sbuf;
		templ->flags = ex->flags;
		/* copy the name */
		if (ex->name == NULL) {
			templ->name = NULL;
			templ->length = 0;
		}
		else {
			name = (void*)&templ->first_block.words[count];
		       	memcpy(name, ex->name, ex->lename);
			name[ex->lename] = 0;
			templ->name = name;
			templ->length = ex->lename;
		}
		/* user data */
		memset(templ->data, 0 , sizeof ex->templ->data);
		/* get the block */
		blk = &templ->first_block;
	}

	/* initialize the block */
	blk->next = NULL;
	blk->prev = ex->prvblk;
	if (ex->prvblk != NULL)
		ex->prvblk->next = blk;
	blk->count = count;
	memcpy(blk->words, ex->words, count * sizeof(word_t));

	/* prepare new block */
	ex->curblk++;
	ex->curoff = 0;
	ex->prvblk = blk;

	/* set reserved size */
	set_reserved(ex, sizeof *ex->prvblk);

	return MUSTACH_OK;
}

/* advance write pointer, flush the block at block end */
static int nextput(ex_t *ex)
{
	return ++ex->curoff < ex->offmax ? MUSTACH_OK : store(ex);
}

/* write the word to the code at current write position */
static int put_word(ex_t *ex, word_t word)
{
	ex->words[ex->curoff] = word;
	return nextput(ex);
}

/* write a operator word at write position */
static int put_op(ex_t *ex, op_t op, unsigned value)
{
	return value > WVAL_MAX
		? exerr_too_big(ex)
		: put_word(ex, MKW(op, value));
}

/* return the address of the nex write
 * address is a couple of block index and offset in the block */
static word_t get_put_addr(ex_t *ex)
{
	return MKA(ex->curblk, ex->curoff);
}

/* put an invalid value for current line
 * this has the effect to enforce next
 * put_line to effectively put the line */
static void invalid_line(ex_t *ex)
{
	ex->curline = 0;
}

/* encode the current line if needed */
static int put_line(ex_t *ex, word_t line)
{
	/* check if line already set or not */
	if (ex->curline == line)
	       return MUSTACH_OK;
	/* record it and code it */
	ex->curline = line;
	return put_op(ex, op_line, line);
}

/* encode a reference to a text in the template text */
static int put_text_ref(
		ex_t *ex,
		const char *text,
		word_t length,
		op_t op
) {
	int rc;
	word_t offset = (word_t)(text - ex->sbuf.value);

	/* check the length */
	if (length > WVAL_MAX)
		return exerr_too_big(ex);

	/* code the text reference */
	rc = put_word(ex, MKW(op, length));
	if (rc == MUSTACH_OK)
		rc = put_word(ex, offset);
	return rc;
}

/* encode a copy of a text of the template text */
static int put_text_copy(
		ex_t *ex,
		const char *text,
		word_t length,
		op_t op
) {
	int rc;
	word_t nrw, *pw;
	size_t size;
	block_t *blk;

	/* check the length */
	if (length > WVAL_MAX)
		return exerr_too_big(ex);

	/* code the text length */
	rc = put_word(ex, MKW(op, length));
	if (rc != MUSTACH_OK)
		return rc;

	/* copy the text */
	nrw = 1 + (length / sizeof(word_t));

	/* check available size */
	if (nrw <= ex->offmax - ex->curoff) {
		/* enough for copying */
		pw = &ex->words[ex->curoff];
		ex->curoff += nrw;
	}
	else {
		/* not enough, store current work */
		rc = store(ex);
		if (rc != MUSTACH_OK)
			return rc;

		/* is there enough spece now?
		 * (it is asserted that ex->curoff==0) */
		if (nrw <= ex->offmax) {
			/* yes, copy in the ex building buffer */
			pw = &ex->words[ex->curoff];
			ex->curoff += nrw;
		}
		else {
			/* no, allocates a block only for the text */
			size = sizeof *blk + nrw * sizeof(word_t);
			blk = alloc(size, ex->itf, ex->closure);
			if (blk == NULL)
				return exerr_oom(ex);

			/* initialize */
			blk->next = NULL;
			blk->prev = ex->prvblk;
			if (ex->prvblk != NULL)
				ex->prvblk->next = blk;
			blk->count = nrw;
			ex->prvblk = blk;
			ex->curblk++;

			/* copy in created block */
			pw = blk->words;
		}
	}

	/* copy the text */
	pw[nrw - 1] = 0; /* terminating nul */
	memcpy(pw, text, length);
	return ex->curoff < ex->offmax ? MUSTACH_OK : store(ex);
}

/* code the text of a mustach tag */
static int put_tag(
		ex_t *ex,
		const char *tag,
		word_t length,
		op_t op
) {
	return ((ex->flags & Mustach_Build_Null_Term_Tag) != 0)
		? put_text_copy(ex, tag, length, op)
		: put_text_ref(ex, tag, length, op);
}

/* code the text of the mustach template */
static int put_text(
		ex_t *ex,
		const char *text,
		word_t length,
		op_t op
) {
	return ((ex->flags & Mustach_Build_Null_Term_Text) != 0)
		? put_text_copy(ex, text, length, op)
		: put_text_ref(ex, text, length, op);
}

/* push a value to the stack */
static int ex_push(ex_t *ex, word_t value)
{
	/* detect stack full */
	if (ex->stacktop == sizeof ex->stack / sizeof *ex->stack)
		return exerr_too_deep(ex);
	ex->stack[ex->stacktop++] = value;
	return MUSTACH_OK;
}

/* pop a value from the stack */
static int ex_pop(ex_t *ex, word_t *value)
{
	/* detect stack empty */
	if (ex->stacktop == 0)
		return exerr_unmatched_closing(ex);
	*value = ex->stack[--ex->stacktop];
	return MUSTACH_OK;
}

/* code the begin of a section */
static int put_begin(
		ex_t *ex,
		const char *tag,
		word_t length,
		op_t op
) {
	/* record start encoding address */
	word_t addr = get_put_addr(ex);

	/* encode the tag */
	int rc = put_tag(ex, tag, length, op);
	if (rc != MUSTACH_OK)
		return rc;

	/* reserve a word for end section address */
	rc = nextput(ex);
	if (rc != MUSTACH_OK)
		return rc;

	/* manage "inpar" state if needed */
	if (op == op_block || op == op_parent) {
		/* save current state on stack */
		rc = ex_push(ex, ex->inpar);
		if (rc != MUSTACH_OK)
			return rc;
		/* set current state */
		ex->inpar = op == op_parent;
	}

	/* save current line on stack */
	rc = ex_push(ex, ex->curline);
	if (rc != MUSTACH_OK)
		return rc;

	/* ensure that section block will encode the line */
	invalid_line(ex);

	/* save start encoding address on stack */
	return ex_push(ex, addr);
}

/* code the end of a section */
static int put_end(ex_t *ex, const char *tag, word_t length)
{
	int i, rc;

	/* poped values */
	word_t addr, begline;

	/* iterator on section's begin */
	block_t *it;
	word_t blk, off, wcnt, *words;

	/* values of section's begin */
	op_t op;
	const char *txtptr;
	word_t txtlen, *pjend;

	/* retrieve saved addr of section's begin */
	rc = ex_pop(ex, &addr);
	if (rc != MUSTACH_OK)
		return rc;

	/* retrieve saved line of section's begin */
	rc = ex_pop(ex, &begline);
	if (rc != MUSTACH_OK)
		return rc;

	/* point on begin */
	blk = ABLK(addr);
	off = AOFF(addr);
	if (blk == ex->curblk) {
		/* currently build block */
		it = NULL;
		wcnt = ex->offmax;
		words = ex->words;
	}
	else {
		/* existing stored block */
		it = ex->prvblk;
		while(++blk != ex->curblk)
			it = it->prev;
		blk = ABLK(addr);
		wcnt = it->count;
		words = it->words;
	}

	/* read the 2 first words (op+length and offset),
	 * get a pointer on third word (address of address of end)
	 * and point on the address of fourth word (begin of section) */
	for(i = 0 ; i < 3 ; i++) {
		switch(i) {
		case 0:
			/* get op and length */
			op = WOP(words[off]);
			txtlen = WVAL(words[off]);
			break;
		case 1:
			/* get text */
			if ((ex->flags & Mustach_Build_Null_Term_Tag) == 0)
				txtptr = &ex->sbuf.value[words[off]];
			else {
				txtptr = (const char*)&words[off];
				off += (length / sizeof(word_t));
			}
			break;
		case 2:
			/* address of end */
			pjend = &words[off];
			break;
		}
		/* compute next address */
		if (++off == wcnt) {
			/* at start of the next block,
			 * it is assumed that it != NULL */
			off = 0;
			blk++;
			it = it->next;
			if (it == NULL) {
				wcnt = ex->offmax;
				words = ex->words;
			}
			else {
				wcnt = it->count;
				words = it->words;
			}
		}
	}

	/* check similarity */
	if (txtlen != length || memcmp(tag, txtptr, txtlen))
		return exerr_mismatch_closing(ex);

	/* set next or end op if needed */
	switch (op) {
	case op_while:
		/* put next op at end of the section
		 * it points to the begin of the section */
		addr = MKA(blk, off);
		rc = put_op(ex, op_next, addr);
		if (rc != MUSTACH_OK)
			return rc;
		break;
	case op_block:
	case op_parent:
		/* put end op at end of the section */
		rc = put_op(ex, op_end, 0);
		if (rc != MUSTACH_OK)
			return rc;
		/* restore previous inpar value */
		rc = ex_pop(ex, &ex->inpar);
		if (rc != MUSTACH_OK)
			return rc;
		break;
	case op_unless:
	default:
		/* invisible section's end */
		break;
	}

	/* record the jump to end address */
	*pjend = get_put_addr(ex);

	/* ensure line is set on continuation */
	invalid_line(ex);
	return MUSTACH_OK;
}

/* set the standalone flag */
static void ex_alone(ex_t *ex)
{
	ex->alone = 1;
}

/* record a pending text whose length until last CRLF
 * is length1 and whose length since last CRLF is length2 */
static int ex_text(
		ex_t *ex,
		word_t line,
		const char *text,
		word_t length1,
		word_t length2
) {
	ex->pending = 1;
	ex->alone = 0;
	ex->tline = line;
	ex->text = text;
	ex->length1 = length1;
	ex->length2 = length2;
	return MUSTACH_OK;
}

/* encode the pending text if any */
static int ex_code_pending_text(ex_t *ex)
{
	int rc = MUSTACH_OK;
	if (ex->pending) {
		word_t length = ex->length1;
		if (!ex->alone)
			length += ex->length2;
		if (length > 0)
			rc = put_text(ex, ex->text, length, op_text);
		ex->pending = 0;
	}
	return rc;
}

/* encode a simple tag: replacement with or without escaping
 * and partials */
static int ex_simple(
		ex_t *ex,
		word_t line,
		const char *txt,
		word_t txtlen,
		op_t op
) {
	int rc = ex_code_pending_text(ex);
	if (rc == MUSTACH_OK) {
		rc = put_line(ex, line);
		if (rc == MUSTACH_OK)
			rc = put_tag(ex, txt, txtlen, op);
	}
	return rc;
}

/* encode the begin of a section */
static int ex_begin(
		ex_t *ex,
		word_t line,
		const char *txt,
		word_t txtlen,
		op_t op
) {
	int rc = ex_code_pending_text(ex);
	if (rc == MUSTACH_OK) {
		rc = put_line(ex, line);
		if (rc == MUSTACH_OK)
			rc = put_begin(ex, txt, txtlen, op);
	}
	return rc;
}

/* encode the begin of a section */
static int ex_end(ex_t *ex, word_t line, const char *txt, word_t txtlen)
{
	(void)line;/*make compiler happy #@!%!!*/
	int rc = ex_code_pending_text(ex);
	if (rc == MUSTACH_OK)
		rc = put_end(ex, txt, txtlen);
	return rc;
}

/* encode a prefix if any */
static int ex_prefix(ex_t *ex)
{
	int rc;
	const char *text;
	word_t length;

	/* compute prefix */
	if (ex->pending && ex->alone) {
		text = &ex->text[ex->length1];
		length = ex->length2;
	}
	else {
		text = NULL;
		length = 0;
	}

	/* encode pending */
	rc = ex_code_pending_text(ex);
	if (rc == MUSTACH_OK && length != 0)
		/* encode the computed prefix */
		rc = put_text(ex, text, length, op_prefix);

	return rc;
}

/* encode a replacement tag with or without escaping */
static int ex_repl(
		ex_t *ex,
		word_t line,
		const char *txt,
		word_t txtlen,
		int escape
) {
	op_t op = escape ? op_repl_esc : op_repl_raw;
	return ex_simple(ex, line, txt, txtlen, op);
}

/* encode partial */
static int ex_partial(
		ex_t *ex,
		word_t line,
		const char *txt,
		word_t txtlen
) {
	int rc = ex_prefix(ex);
	if (rc == MUSTACH_OK)
		rc = ex_simple(ex, line, txt, txtlen, op_partial);
	return rc;
}

/* encode begin of parent */
static int ex_parent(
		ex_t *ex,
		word_t line,
		const char *txt,
		word_t txtlen
) {
	int rc = ex_prefix(ex);
	if (rc == MUSTACH_OK)
		rc = ex_begin(ex, line, txt, txtlen, op_parent);
	return rc;
}

/* scan the template definition and encode it in blocks */
static int ex_build(ex_t *ex)
{
	/* initial mustach delimiters */
	static const char defopcl[4] = { '{', '{', '}', '}' };
	const char *opstr = &defopcl[0]; /* open delimiter */
	const char *clstr = &defopcl[2]; /* close delimiter */
	unsigned oplen = 2; /* length of open delimiter */
	unsigned cllen = 2; /* length of close delimiter */

	/* the template text */
	size_t len;         /* length of the template */
	const char *templ;  /* iterator on template */
	const char *end;    /* end of template test */

	/* recording current tag */
	const char *tag;
	word_t ltag;

	char c;
	const char *beg, *term, *bop;
	int rc, stdalone;
	word_t l, line, lincptr;

	/* check length */
	len = mustach_sbuf_length(&ex->sbuf);
	if (len > WORD_MAX)
		return exerr_too_big(ex);

	/* init */
	templ = ex->sbuf.value;
	end = &templ[len];
	lincptr = 1;
	stdalone = 1;

	/* iterate over template text */
	for (;;) {
		/* search next opening delimiter */
		beg = bop = templ;
		line = lincptr;
		for (;;) {
			/* check not at end */
			if (beg == end) {
				/* add the text segment */
				rc = ex_text(ex, line, templ,
						(word_t)(bop - templ),
						(word_t)(beg - bop));
				return rc;
			}

			/* inspect first character */
			c = *beg++;
			switch (c) {
			case '\r':
				/* special case of CR or CRLF */
				if (beg != end && *beg == '\n')
					beg++;
				/*@fallthrough@*/
			case '\n':
				/* end of line */
				bop = beg;
				stdalone = 1;
				lincptr++;
				continue;
			case ' ':
			case '\f':
			case '\t':
			case '\v':
				/* ignore spaces */
				continue;
			default:
				/* is it an opening delimiter? */
				if (c == *opstr
				 && (end - beg) >= (ssize_t)(oplen - 1)
				 && ((oplen - 1) == 0
				  || !memcmp(beg, &opstr[1], oplen - 1)))
					break;
				stdalone = 0;
				continue;
			}
			break;
		}

		/* add the text segment */
		if (!ex->inpar) {
			rc = ex_text(ex, line, templ,
					(word_t)(bop - templ),
					(word_t)(beg - bop) - 1);
			if (rc != MUSTACH_OK)
				return rc;
		}
		line = lincptr;

		/* at this point, this is an opening delimiter */
		tag = &beg[oplen - 1];

		/* search next closing delimiter */
		term = tag;
		for (;;) {
			if (term == end)
				return exerr_bad_end(ex);
			if (*term++ == *clstr
			 && (end - term) >= (ssize_t)(cllen - 1)
			 && ((cllen - 1) == 0
			  || !memcmp(term, &clstr[1], cllen - 1)))
				break;
		}

		/* content continue after the closing delimiter */
		templ = term + (cllen - 1);

		/* length of the directive and its first character */
		ltag = (word_t)(term - tag) - 1;
		c = *tag;
		switch(c) {
		case '!': /* comment */
			break;
		case '=': /* define delimiters */
			if (ltag < 5 || tag[ltag - 1] != '=')
				return exerr_bad_delimiter(ex, line);
			tag++;
			ltag -= 2;
			break;
		case '{': /* unescaped replacement */
			/* check if closing delimiter is made of } */
			for (l = 0 ; l < cllen && clstr[l] == '}' ; l++);
			if (l < cllen) {
				/* closing delimiter isn't full of } */
				if (ltag == 0 || tag[ltag-1] != '}')
					return exerr_bad_unesc(ex, line);
				ltag--;
			} else {
				/* closing delimiter full made of } */
				if (templ == end || *templ != '}')
					return exerr_bad_unesc(ex, line);
				templ++;
			}
			c = '&';
			/*@fallthrough@*/
		case '&': /* unescaped replacement */
		case ':': /* unescaped replacement */
			stdalone = 0;
			if (c == ':'
			 && !(ex->flags & Mustach_Build_With_Colon))
				break;
			/*@fallthrough@*/
		case '^': /* negative case */
		case '#': /* positive case */
		case '/': /* end of case or block */
		case '>': /* partial */
		case '$': /* replacement block */
		case '<': /* parent block */
			/* removes the first character */
			tag++;
			ltag--;
			break;
		default: /* escaped replacement */
			stdalone = 0;
			break;
		}

		/* remove surrounding spaces */
		while (ltag && tag[0] == ' ') { tag++; ltag--; }
		while (ltag && tag[ltag-1] == ' ') ltag--;

		/* check name length */
		if (ltag == 0 && !(ex->flags & Mustach_Build_With_EmptyTag))
			return exerr_empty_tag(ex, line);

		/* check if standalone on line */
		if (stdalone) {
			beg = templ;
			for (;;) {
				if (beg == end) {
					templ = beg;
					break;
				}
				switch (*beg++) {
				case '\r':
					if (beg != end && *beg == '\n')
						beg++;
					/*@fallthrough@*/
				case '\n':
					lincptr++;
					templ = beg;
					break;
				case ' ':
				case '\f':
				case '\t':
				case '\v':
					/* ignore spaces */
					continue;
				default:
					if (c != '<')
						stdalone = 0;
					break;
				}
				break;
			}
			if (stdalone)
				ex_alone(ex);
		}

		switch(c) {
		case '!': /* comment */
			rc = ex_code_pending_text(ex);
			break;
		case '=': /* defines delimiters */
			/* search internal space */
			for (l = 0; l < ltag && tag[l] != ' ' ; l++);
			if (l == ltag)
				return exerr_bad_delimiter(ex, line);
			/* record new delimiters */
			oplen = l;
			opstr = tag;
			while (l < ltag && tag[l] == ' ') l++;
			cllen = ltag - l;
			clstr = tag + l;
			rc = ex_code_pending_text(ex);
			break;
		case '^':
			/* negative section */
			if (!ex->inpar)
				rc = ex_begin(ex, line, tag, ltag,
						op_unless);
			break;
		case '#':
			/* positive section */
			if (!ex->inpar)
				rc = ex_begin(ex, line, tag, ltag,
						op_while);
			break;
		case '$':
			/* block */
			rc = ex_begin(ex, line, tag, ltag, op_block);
			stdalone = 1;
			break;
		case '<':
			/* parent */
			if (!ex->inpar)
				rc = ex_parent(ex, line, tag, ltag);
			stdalone = 1;
			break;
		case '/':
			/* end section */
			rc = ex_end(ex, line, tag, ltag);
			break;
		case '>':
			/* partials */
			if (!ex->inpar)
				rc = ex_partial(ex, line, tag, ltag);
			break;
		default:
			/* replacement */
			if (!ex->inpar)
				rc = ex_repl(ex, line, tag, ltag,
						c != '&');
			break;
		}
		if (rc != MUSTACH_OK)
			return rc;
	}
}

/* scan the template text and build the expected prepared item:
 * full template or partial */
static int ex_make(ex_t *ex)
{
	/* run extraction */
	int rc = ex_build(ex);
	if (rc != MUSTACH_OK)
		return rc;

	/* force emission of text */
	rc = ex_code_pending_text(ex);
	if (rc != MUSTACH_OK)
		return rc;

	/* check pending section */
	if (ex->stacktop != 0)
		return exerr_bad_end(ex);

	/* add two stop op, the first is for
	 * stopping and the second for setting
	 * a last word avoiding pointing nowhere
	 * at end of read */
	word_t addr = get_put_addr(ex);
	rc = put_op(ex, op_stop, addr);
	if (rc != MUSTACH_OK)
		return rc;
	rc = put_op(ex, op_stop, addr);
	if (rc != MUSTACH_OK)
		return rc;

	/* store everything */
	return store(ex);
}

/*******************************************************************/
/*******************************************************************/
/** PART application of template  **********************************/
/*******************************************************************/
/*******************************************************************/

/* predeclaration of the 2 evaluation primitives */
static int ap_single(ap_t *ap);
static int ap_loop(ap_t *ap);

/* extract the word at current read position and
 * advance the read position to the next word to be read.
 * in order to remain simple, this function requires that
 * the end marker is not the last word */ 
static word_t get_word(ap_t *ap)
{
	/* get word at current read position */
	unsigned off = ap->off;
	word_t r = ap->words[off];

	/* compute next read position */
	if (++off < ap->count)
		ap->off = off;
	else {
		ap->iblk++;
		ap->blk = ap->blk->next;
		ap->words = ap->blk->words;
		ap->count = ap->blk->count;
		ap->off = 0;
	}
	return r;
}

/* read the offset and return corresponding text of the template */
static const char *get_text_ref(ap_t *ap)
{
	unsigned off = get_word(ap);
	return &ap->base[off];
}

static const char *get_text_copy(ap_t *ap, word_t length)
{
	/* get the string */
	unsigned off = ap->off;
	const char *result = (const char*)&ap->words[off];
	
	/* compute next read position */
	off += 1 + (length / sizeof(word_t));
	if (off < ap->count)
		ap->off = off;
	else {
		ap->iblk++;
		ap->blk = ap->blk->next;
		ap->words = ap->blk->words;
		ap->count = ap->blk->count;
		ap->off = 0;
	}
	return result;
}

/* get the text of given length */
static const char *get_text(ap_t *ap, word_t length)
{
	return ((ap->tflags & Mustach_Build_Null_Term_Text) != 0)
		? get_text_copy(ap, length)
		: get_text_ref(ap);
}

/* get the tag of given length */
static const char *get_tag(ap_t *ap, word_t length)
{
	return ((ap->tflags & Mustach_Build_Null_Term_Tag) != 0)
		? get_text_copy(ap, length)
		: get_text_ref(ap);
}

/* move the read position to the given address (iblk+offset) */
static void ap_goto(ap_t *ap, unsigned addr)
{
	unsigned iblk = ABLK(addr);
	/* move to offset */
	ap->off = AOFF(addr);
	/* move to block */
	if (iblk != ap->iblk) {
		/* search the block */
		if (iblk < ap->iblk)
			do {
				ap->blk = ap->blk->prev;
			} while (iblk < --ap->iblk);
		else
			do {
				ap->blk = ap->blk->next;
			} while (iblk < ++ap->iblk);
		/* update copies */
		ap->words = ap->blk->words;
		ap->count = ap->blk->count;		
	}
}

/* emit unescaped text */
static int ap_emit_raw(ap_t *ap, const char *text, size_t length)
{
	return ap->itf->emit_raw != NULL
		? ap->itf->emit_raw(ap->closure, text, length)
		: ap->itf->emit_esc(ap->closure, text, length, 0);
}

/* emit escaped text */
static int ap_emit_esc(ap_t *ap, const char *text, size_t length, int esc)
{
	if (ap->itf->emit_esc != NULL)
		return ap->itf->emit_esc(ap->closure, text, length, esc);
	if (esc == 0)
		return ap->itf->emit_raw(ap->closure, text, length);

	return mustach_escape(text, length, ap->itf->emit_raw, ap->closure);
}

/* emit the prefixes if it is required */
static int ap_emit_pref(ap_t *ap)
{
	/* check if at begin of line */
	if (ap->beoflin) {
		const pref_t *pref  = ap->prefix;
		for ( ; pref != NULL ; pref = pref->next) {
			int rc = ap_emit_raw(ap, pref->start, pref->len);
			if (rc != MUSTACH_OK)
				return rc;
		}
		ap->beoflin = 0;
	}
	return MUSTACH_OK;
}

/* emit the text with a prefix */
static int ap_pref_text(ap_t *ap, const char *text, size_t length, int esc)
{
	int rc = MUSTACH_OK;
	size_t base = 0, idx = 0;

	while (rc == MUSTACH_OK && idx != length) {
		/* avance until end of line */
		base = idx;
		while (idx != length
		    && text[idx] != '\n' && text[idx] != '\r')
			idx++;

		/* prefix non empty lines */
		if (idx != base)
			ap_emit_pref(ap);

		/* search end of end of line */
		if (idx == length)
			ap->beoflin = 0;
		else {
			ap->beoflin = 1;
			while (idx != length
                           && (text[idx] == '\n' || text[idx] == '\r'))
				idx++;
		}

		/* emit the line */
		rc = ap_emit_esc(ap, &text[base], idx - base, esc);
		if (rc > 0)
			rc = MUSTACH_OK;
	}
	return rc;
}

/* emit a text anyhow */
static int ap_any_text(
		ap_t *ap,
		const char *text,
		size_t length,
		int esc,
		int prefin
) {
	if (length == 0)
		return MUSTACH_OK;
	if (ap->prefix != NULL) {
		if (prefin)
			return ap_pref_text(ap, text, length, esc);
		if (ap->beoflin)
			ap_emit_pref(ap);
	}
	ap->beoflin = text[length - 1] == '\n' || text[length - 1] == '\r';
	return ap_emit_esc(ap, text, length, esc);
}

/* emit the text */
static int ap_text(ap_t *ap, word_t length)
{
	const char *text = get_text(ap, length);
	return ap_any_text(ap, text, length, 0, 1);
}

/* emit the value of the tag with or without escaping */
static int ap_repl(ap_t *ap, word_t length, int escape)
{
	mustach_sbuf_t sbuf = SBUF_INITIALIZER;
	const char *tag = get_tag(ap, length);
	int rc = ap->itf->get(ap->closure, tag, length, &sbuf);
	if (rc == MUSTACH_OK) {
		size_t length = mustach_sbuf_length(&sbuf);
		rc = ap_any_text(ap, sbuf.value, length, escape, 0);
		mustach_sbuf_release(&sbuf);
	}
	return rc;
}

/* append the prefix to the list of prefixes
 * and eval a single item with it */
static int ap_prefix(ap_t *ap, word_t length, unsigned add)
{
	int rc;
	pref_t prefix = {
		.add = add,
		.len = length,
		.start = get_text(ap, length),
		.next = NULL
	};
	pref_t **lppref = &ap->prefix;
	while (*lppref != NULL)
		lppref = &(*lppref)->next;
	*lppref = &prefix;
	rc = ap_single(ap);
	*lppref = NULL;
	return rc;
}

/* try to enter a section */
static int ap_enter(ap_t *ap, word_t length)
{
	const char *tag = get_tag(ap, length);
	return ap->itf->enter(ap->closure, tag, length);
}

/* leave an entered section */
static int ap_leave(ap_t *ap)
{
	return ap->itf->leave(ap->closure);
}

/* check unless case */
static int ap_unless(ap_t *ap, word_t length)
{
	/* try enter the tag */
	int rc = ap_enter(ap, length);

	/* read address of continuation */
	word_t addr = get_word(ap);
	if (rc > 0) {
		/* if entered, leave and go to continuation */
		ap_goto(ap, addr);
		rc = ap_leave(ap);
	}
	return rc;
}

/* check while case */
static int ap_while(ap_t *ap, word_t length)
{
	/* try enter the tag */
	int rc = ap_enter(ap, length);

	/* read address of continuation */
	word_t addr = get_word(ap);
	if (rc == 0)
		/* unless entered go to continuation */
		ap_goto(ap, addr);
	return rc < 0 ? rc : MUSTACH_OK;
}

/* check next case of while */
static int ap_next(ap_t *ap, unsigned addr)
{
	/* try enter next item of the section */
	int rc = ap->itf->next(ap->closure);
	if (rc > 0) {
		/* if entered, go to evaluation of section */
		ap_goto(ap, addr);
		rc = MUSTACH_OK;
	}
	else if (rc == 0)
		/* not entered, leave the section */
		rc = ap_leave(ap);
	return rc;
}

/* read the current partial name as a tag of given length
 * and query interface for retriving it */
static int ap_make_partial(
		ap_t *ap,
		const char *tag,
		word_t length,
		mustach_template_t **part
) {
	mustach_sbuf_t sbuf = SBUF_INITIALIZER;
	int rc = ap->itf->get(ap->closure, tag, length, &sbuf);
	if (rc == MUSTACH_OK)
		rc = mustach_build_template(
				part,
				0,/*flag*/
				&sbuf,
				NULL,
				0,
				NULL,/*wrap error*/
				NULL);
	return rc;
}

static void ap_unmake_partial(ap_t *ap, mustach_template_t *part)
{
	(void)ap;/*make compiler happy #@!%!!*/
	mustach_destroy_template(part, NULL, NULL);
}

/* read the current partial name as a tag of given length
 * and query interface for retriving it */
static int ap_partial_get(
		ap_t *ap,
		word_t length,
		mustach_template_t **part
) {
	/* get the name */
	const char *name = get_tag(ap, length);

	/* try to get it */
	return ap->itf->partial_get != NULL
		? ap->itf->partial_get(ap->closure, name, length, part)
		: ap_make_partial(ap, name, length, part);
}

/* read the current partial name as a tag of given length
 * and query interface for retriving it */
static void ap_partial_put(ap_t *ap, mustach_template_t *part)
{
	/* try to get it */
	if (ap->itf->partial_put != NULL)
		ap->itf->partial_put(ap->closure, part);
	else
		ap_unmake_partial(ap, part);
}

/* apply the partial 'part' of given 'parent' */
static int ap_partial_eval(
		ap_t *pap,
		mustach_template_t *part,
		ap_t *parent
) {
	int rc;
	ap_t ap;

	/* check nesting depth */
	if (pap->nesting >= MUSTACH_MAX_NESTING)
		return MUSTACH_ERROR_TOO_MUCH_NESTING;

	/* init local application structure */
	ap.base = part->sbuf.value;
	ap.blk = &part->first_block;
	ap.count = ap.blk->count;
	ap.words = ap.blk->words;
	ap.off = 0;
	ap.iblk = 0;
	ap.line = 1;
	ap.nesting = pap->nesting + 1;
	ap.orig = 0;
	ap.aflags = pap->aflags;
	ap.prefix = pap->prefix;
	ap.itf = pap->itf;
	ap.closure = pap->closure;
	ap.parent = parent;
	ap.tflags = part->flags;
	ap.templ = part;

	/* apply the partial */
	ap.beoflin = pap->beoflin;
	rc = ap_loop(&ap);
	pap->beoflin = ap.beoflin;
	return rc;
}

/* apply the partial */
static int ap_partial(ap_t *ap, word_t length)
{
	mustach_template_t *part;
	int rc = ap_partial_get(ap, length, &part);
	if (rc == MUSTACH_OK) {
		rc = ap_partial_eval(ap, part, ap->parent);
		ap_partial_put(ap, part);
	}
	return rc;
}

/* apply the parent */
static int ap_parent(ap_t *ap, word_t length)
{
	mustach_template_t *part;
	int rc = ap_partial_get(ap, length, &part);
	if (rc == MUSTACH_OK) {
		word_t addr = get_word(ap);
		ap->orig = MKA(ap->iblk, ap->off);
		rc = ap_partial_eval(ap, part, ap);
		ap_goto(ap, addr);
		ap_partial_put(ap, part);
	}
	return rc;
}

static ap_t *ap_block_parent(ap_t *pap, const char *text, word_t length)
{
	unsigned txtlen;
	const char *txt;
	ap_t *r;

	if (pap == NULL)
		return NULL;

	r = ap_block_parent(pap->parent, text, length);
	if (r != NULL)
		return r;

	ap_goto(pap, pap->orig);
	for (;;) {
		word_t code = get_word(pap);
		switch (WOP(code)) {
		case op_line:
			pap->line = WVAL(code);
			break;
		case op_block:
			txtlen = WVAL(code);
			txt = get_tag(pap, txtlen);
			word_t next = get_word(pap);
			if (txtlen == length
			 && !memcmp(txt, text, txtlen))
				return pap;
			ap_goto(pap, next);
			break;
		default:
			return NULL;
		}
	}
}

static int ap_block(ap_t *ap, word_t length)
{
	int rc;
	const char *text = get_tag(ap, length);
	word_t addr = get_word(ap);
	ap_t *parent = ap_block_parent(ap->parent, text, length);
	if (parent == NULL)
		rc = ap_loop(ap);
	else {
		rc = ap_loop(parent);
		ap_goto(ap, addr);
	}
	return rc;
}

/* evaluate application of one operation */
static int ap_single(ap_t *ap)
{
	word_t code = get_word(ap);
	unsigned arg = WVAL(code);
	switch (WOP(code)) {
	case op_line:
		ap->line = arg;
		return ap_single(ap);
	case op_text:
		return ap_text(ap, arg);
	case op_repl_raw:
		return ap_repl(ap, arg, 0);
	case op_repl_esc:
		return ap_repl(ap, arg, 1);
	case op_partial:
		return ap_partial(ap, arg);
	case op_while:
		return ap_while(ap, arg);
	case op_next:
		return ap_next(ap, arg);
	case op_unless:
		return ap_unless(ap, arg);
	case op_block:
		return ap_block(ap, arg);
	case op_parent:
		return ap_parent(ap, arg);
	case op_prefix:
		return ap_prefix(ap, arg, 1);
	case op_unprefix:
		return ap_prefix(ap, arg, 0);
	case op_stop:
	case op_end:
	default:
		return MUSTACH_OK + 1;
	}
}

static int ap_loop(ap_t *ap)
{
	int rc;
	do { rc = ap_single(ap); } while (rc == MUSTACH_OK);
	return rc < MUSTACH_OK ? rc : MUSTACH_OK;
}

/*******************************************************************/
/*******************************************************************/
/** PART public functions  *****************************************/
/*******************************************************************/
/*******************************************************************/

/* see header file */
void mustach_destroy_template(
		mustach_template_t *templ,
		const mustach_build_itf_t *itf,
		void *closure
) {
	if (templ != NULL) {
		/* destroy blocks of the template */
		block_t *blk = templ->first_block.next;
		while (blk != NULL) {
			block_t *b = blk;
			blk = blk->next;
			dealloc(b, itf, closure);
		}
		/* destroy main of the template */
		mustach_sbuf_release(&templ->sbuf);
		dealloc(templ, itf, closure);
	}
}

/* see header file */
int mustach_build_template(
		mustach_template_t **templ,
		int flags,
		const mustach_sbuf_t *sbuf,
		const char *name,
		size_t namelen,
		const mustach_build_itf_t *itf,
		void *closure
) {
	int rc;
	struct ex ex;

	/* check interface validity */
	if (itf != NULL && itf->version != MUSTACH_BUILD_ITF_VERSION_1)
		return MUSTACH_ERROR_INVALID_ITF;

	/* setup extraction structure */
	if (name == NULL)
		namelen = 0;
	else if (namelen == 0)
		namelen = strlen(name);
	initex(&ex, flags, sbuf, name, (unsigned)namelen, itf, closure);

	/* build the template using the extractor */
	rc = ex_make(&ex);
	if (rc == MUSTACH_OK)
		*templ = ex.templ;
	else {
		if (ex.templ != NULL)
			mustach_destroy_template(ex.templ, itf, closure);
		else
			mustach_sbuf_release(&ex.sbuf);
		*templ = NULL;
	}
	return rc;
}

/* see header file */
int mustach_make_template(
		mustach_template_t **templ,
		int flags,
		const mustach_sbuf_t *sbuf,
		const char *name
) {
	return mustach_build_template(templ, flags, sbuf, name,
						0, NULL, NULL);
}

void mustach_set_template_data(
		mustach_template_t *templ,
		unsigned index,
		void *value
) {
	if (index < (unsigned)(sizeof templ->data / sizeof templ->data[0]))
		templ->data[index] = value;
}

void *mustach_get_template_data(
		mustach_template_t *templ,
		unsigned index
) {
	return index < (unsigned)(sizeof templ->data / sizeof templ->data[0])
		? templ->data[index] : NULL;
}

unsigned mustach_get_template_data_count(
		mustach_template_t *templ
) {
	return (unsigned)(sizeof templ->data / sizeof templ->data[0]);
}

const char *mustach_get_template_name(
		mustach_template_t *templ,
		size_t *length
) {
	if (length != NULL)
		*length = templ->length;
	return templ->name;
}

int mustach_get_template_flags(
		mustach_template_t *templ
) {
	return templ->flags;
}

/* see header file */
int mustach_apply_template(
		mustach_template_t *templ,
		int flags,
		const mustach_apply_itf_t *itf,
		void *closure
) {
	ap_t ap;
	int rc;

	/* check interface validity */
	if (itf == NULL
	 || itf->version != MUSTACH_APPLY_ITF_VERSION_1
	 || (itf->emit_raw == NULL && itf->emit_esc == NULL)
	 || itf->get == NULL
	 || itf->enter == NULL
	 || itf->next == NULL
	 || itf->leave == NULL
	 || ((itf->partial_get == NULL) != (itf->partial_put == NULL)))
		return MUSTACH_ERROR_INVALID_ITF;

	/* process */
	rc = itf->start == NULL ? MUSTACH_OK : itf->start(closure);
	if (rc == MUSTACH_OK) {
		ap.aflags = flags;
		ap.base = templ->sbuf.value;
		ap.blk = &templ->first_block;
		ap.count = ap.blk->count;
		ap.words = ap.blk->words;
		ap.off = 0;
		ap.iblk = 0;
		ap.line = 1;
		ap.beoflin = 1;
		ap.nesting = 0;
		ap.orig = 0;
		ap.prefix = NULL;
		ap.itf = itf;
		ap.closure = closure;
		ap.parent = NULL;
		ap.templ = templ;
		ap.tflags = templ->flags;
		rc = ap_loop(&ap);
	}
	if (itf->stop)
		itf->stop(closure, rc);
	return rc;
}


