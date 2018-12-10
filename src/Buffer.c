/**
 * @file
 * @author: Eduardo Miravalls Sierra
 * @author: David Muelas Recuenco
 */

#include <assert.h>
#include <stdlib.h>
#include <string.h> /* memset, memmove */
#include <unistd.h> /* read() */

#include "Buffer.h"
#include "utils.h"

/* GROW STEP can be changed at compile time */
#ifndef GROW_STEP
	#define GROW_STEP 1000
#endif

Buffer_t Buffer_new(size_t size)
{
	Buffer_t bf = calloc(1, sizeof(*bf));

	if (NULL == bf) {
		return NULL;
	}

	if (size > 0 && Buffer_resize(bf, size)) {
		free(bf);
		return NULL;
	}

	return bf;
}

static
int ensure_enough_space_from_off(Buffer_t bf, size_t B, bool growable)
{
	/* check if buffer has enough size from offset */
	size_t expected_new_used = bf->off + B;

	if (expected_new_used > bf->size) {
		/* grow buffer at least GROW_STEP bytes to reduce number of memory allocations */
		size_t new_size = max(bf->size + GROW_STEP, expected_new_used);

		if (!growable || Buffer_resize(bf, new_size)) {
			return -1;
		}
	}

	return 0;
}

int Buffer_fread(Buffer_t bf, size_t B, bool growable, FILE *f)
{
	size_t bytes;

	assert(NULL != bf);
	assert(NULL != f);
	assert(bf->off <= bf->size);

	if (ensure_enough_space_from_off(bf, B, growable)) {
		return -1;
	}

	bytes = fread(bf->buf + bf->off, 1, B, f);
	bf->used = bf->off + bytes;
	return bytes;
}

int Buffer_read(Buffer_t bf, size_t B, bool growable, int fd)
{
	size_t bytes;

	assert(NULL != bf);
	assert(bf->off <= bf->size);

	if (ensure_enough_space_from_off(bf, B, growable)) {
		return -1;
	}

	bytes = read(fd, bf->buf + bf->off, B);
	bf->used = bf->off + bytes;
	return bytes;
}

int Buffer_recvfrom(Buffer_t bf, size_t B, bool growable, int fd, struct sockaddr *src_addr, socklen_t *addrlen)
{
	ssize_t bytes;

	assert(NULL != bf);
	assert(bf->off <= bf->size);

	if (ensure_enough_space_from_off(bf, B, growable)) {
		return -1;
	}

	bytes = recvfrom(fd, bf->buf + bf->off, B, 0, src_addr, addrlen);
	bf->used = bf->off + bytes;
	return bytes;
}

int Buffer_resize(Buffer_t bf, size_t new_size)
{
	char * tmp;

	assert(NULL != bf);

	tmp = realloc(bf->buf, new_size);

	if (NULL == tmp) {
		return -1;
	}

	bf->buf = tmp;
	/* keep offset within buffers limits */
	bf->off = min(bf->off, new_size-1);
	/* keep used within buffers limits */
	bf->used = min(bf->used, new_size);
	bf->size = new_size;
	return 0;
}

void Buffer_clear(Buffer_t bf)
{
	assert(NULL != bf);

#if DEBUG || POISON
	if (NULL != bf->buf) {
		memset(bf->buf, 0xff, bf->size);
	}
#endif

	bf->off = 0;
	bf->used = 0;
}

void Buffer_free(Buffer_t bf)
{
	assert(NULL != bf);

#if DEBUG || POISON
	if (NULL != bf->buf) {
		memset(bf->buf, 0xff, bf->size);
	}
#endif

	free(bf->buf);

#if DEBUG || POISON
	bf->buf = NULL;
	bf->off =  -3;
	bf->used = -2;
	bf->size = -1;
#endif

	free(bf);
}

void Buffer_move(Buffer_t bf, size_t to, size_t from, size_t bytes)
{
	assert((from + bytes) <= bf->size);
	assert((to + bytes) <= bf->size);

	bytes = min(bf->size - from, bytes);
	memmove(bf->buf + to, bf->buf + from, bytes);

	bf->used = min(bf->used, to+bytes);
}

int Buffer_append(Buffer_t bf, size_t B, bool growable, const void *src)
{
	size_t old_off;
	assert(bf != NULL);
	assert(src != NULL);

	// set off to used, so we can reuse this auxiliary function
	old_off = bf->off;
	bf->off = bf->used;

	if (ensure_enough_space_from_off(bf, B, growable)) {
		return -1;
	}

	bf->off = old_off;

	memmove(bf->buf + bf->used, src, B);
	bf->used += B;
	return 0;
}

