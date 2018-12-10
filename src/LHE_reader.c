/**
 * @file
 * @author: Eduardo Miravalls Sierra
 * @author: David Muelas Recuenco
 */

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <string.h> /* memset, memcpy */
#include <inttypes.h>
#include <stddef.h> /* ptrdiff_t */

#include "utils.h"
#include "LHE_headers.h"

#include "LHE_reader.h"

struct LHE_Reader {
	FILE *f;
	size_t blocks_read;
	size_t nblocks;
	LHE_packetheader_t ph;
};

/**
 * Initialises a LHE_Reader_t, reading the header from an open file.
 *
 * @param reader
 * @param fin
 *
 * @return 0 if success, != 0 if error
 */
static
int LHE_init_reader(LHE_Reader_t reader, FILE *fin)
{
	size_t expected, got;

	reader->f = fin;

#if NOT_FIRST_BLOCK_ID
	// file does not include the 1st block id field
	expected = sizeof(reader->ph) - sizeof(reader->ph.first_block_id);
#else
	expected = sizeof(reader->ph);
#endif

	got = fread(&reader->ph, 1, expected, reader->f);

	if (expected != got) {
		return -1;
	}

#if DEBUG
	fprintf(stdout, "Size of the image: Width: %"PRIu16"px - Height: %"PRIu16"px\n",
	        LHE_PACKETHEADER_GET_IM_W(reader->ph)*LHE_PACKETHEADER_GET_BLOCK_W(reader->ph),
	        LHE_PACKETHEADER_GET_IM_H(reader->ph)*LHE_PACKETHEADER_GET_BLOCK_H(reader->ph));

	printLHEHeader(reader->ph);
#endif

	reader->nblocks = LHE_PACKETHEADER_GET_NBLOCKS(reader->ph);

	/* if nblocks is 0, assume all blocks are in fin */
	if (!reader->nblocks) {
		reader->nblocks = LHE_PACKETHEADER_GET_IM_H(reader->ph) * LHE_PACKETHEADER_GET_IM_W(reader->ph);
	}

	return 0;
}


LHE_Reader_t LHE_new_reader(FILE *fin)
{
	assert(NULL != fin);

	LHE_Reader_t reader = (LHE_Reader_t) calloc(1, sizeof(*reader));

	if (NULL == reader) {
		return NULL;
	}

	if (LHE_init_reader(reader, fin)) {
		reader->f = (FILE*)(void *)0x1;
		free(reader);
		return NULL;
	}

	return reader;
}

void LHE_free_reader(LHE_Reader_t reader)
{
	if (NULL != reader) {
		reader->f = (FILE*)(void *)0x1;
		free(reader);
	}
}

/* Copies the next block into buffer. Returns the number of bytes
 * (blockheader+block) read.
 *
 * Returns 0 if EOF or there are no more blocks for this reader.
 * Returns <0 on error.
 */
int LHE_get_next(Buffer_t bf, LHE_Reader_t reader)
{
	size_t bytes;

	LHE_blockheader_t bh;

	assert(NULL != bf);
	assert(NULL != reader);

	if (reader->blocks_read >= reader->nblocks) {
		return 0;
	}

	/* read block header */
	bytes = Buffer_fread(bf, sizeof(bh), true, reader->f);

	if (!bytes) {
		/* no more bytes */
		return 0;
	}

	if (bytes < sizeof(bh)) {
		fprintf(stderr, "Error reading block header. Read %zu bytes, less bytes than expected (%zu)!!\n", bytes,  sizeof(bh));
		return -1;
	}

	/* copy the block header IN the buffer TO bh */
	bh = *(LHE_blockheader_t *)(bf->buf + bf->off);

	debug_fprintf(stdout, "Read block header %zu: type_and_len=0x%04"PRIX16", padding=%"PRIu16", length=%"PRIu16,
		LHE_PACKETHEADER_GET_FB_ID(reader->ph) + reader->blocks_read, bh.type_and_len, LHE_BLOCKHEADER_GET_TYPE(bh), LHE_BLOCKHEADER_GET_LEN(bh));

	reader->blocks_read++;
	/* advance the offset so the block doesn't overwrite its header */
	bf->off += bytes;

	bytes = Buffer_fread(bf, LHE_BLOCKHEADER_GET_LEN(bh), true, reader->f);

	if (bytes != LHE_BLOCKHEADER_GET_LEN(bh)) {
		fprintf(stderr, "Error reading block. Read %zu bytes, less bytes than expected (%"PRIu16")!!\n", bytes,  LHE_BLOCKHEADER_GET_LEN(bh));
		return -2;
	}

	/* advance the offset so the next header doesn't overwrite this block */
	bf->off += bytes;
	return bytes + sizeof(bh);
}


int LHE_reader_main(const char *in, size_t initial_buffer,
	header_processor process_header, block_processor process_block, void *aux)
{
	int retval;
	size_t total_bytes;
	size_t fsize;
	FILE *fin;

	die_if_condition(NULL == in, "File is a mandatory argument for reader");

	fsize = get_file_size(in);

	if (-1 == (off_t)fsize) {
		perror(in);
		return -1;
	}

	fin = fopen(in, "r");

	if (NULL == fin) {
		perror(in);
		return -1;
	}

	Buffer_t bf = Buffer_new(initial_buffer);

	if (NULL == bf) {
		fprintf(stderr, "[ ERROR ] Buffer allocation failed\n");
		exit(-1);
	}

	/* iterate over all LHE packets in the file */
	total_bytes = 0;
	retval = 0;

	/* stop when error (retval < 0) or EOF (total_bytes == fsize) */
	while (total_bytes < fsize && 0 <= retval) {
		size_t bytes;
		uint16_t blocks;

		Buffer_clear(bf);
		LHE_Reader_t reader = LHE_new_reader(fin);

		if (NULL == reader) {
			fprintf(stderr, "[ ERROR ] There was a problem reading the header\n");
			perror("LHE_new_reader");
			exit(-1);
		}

		if (NULL != process_header) {
			process_header(&reader->ph, aux);
		}
		blocks = 0;

#if NOT_FIRST_BLOCK_ID
		// file does not include the 1st block id field
		bytes = sizeof(LHE_packetheader_t) - sizeof(uint16_t);
#else
		bytes = sizeof(LHE_packetheader_t);
#endif
		do {
			retval = LHE_get_next(bf, reader);

			/* positive values give how many bytes were copied into bf */
			if (retval > 0) {
				blocks++;
				bytes += retval;

				if (NULL != process_block) {
					process_block(bf->buf + bf->off - (ptrdiff_t)retval, blocks, aux);
				}
			}
			/* negative values indicate errors */
		} while (0 < retval);

		if (retval < 0) {
			fprintf(stderr, "[ ERROR ] Return code was %d\n", retval);
		}

		LHE_free_reader(reader);

		debug_fprintf(stdout, "%zu bytes read in %"PRIu16" blocks\n", bytes, blocks);
		total_bytes += bytes;
	}

	debug_fprintf(stdout, "A total of %zu/%zu bytes read\n", total_bytes, fsize);

	Buffer_free(bf);
	fclose(fin);
	return 0;
}


int copy_block_to_bf(const char *ptr, uint16_t id, void *args[2])
{
	Buffer_t bf = (Buffer_t)args[0];
	LHE_blockheader_t *hdr = (LHE_blockheader_t *)ptr;
	size_t bytes = sizeof(*hdr) + LHE_BLOCKHEADER_GET_LEN(*hdr);

	assert(NULL != ptr);

	(void)id;
	// bytes includes header size!
	debug_fprintf(stdout, "Writing block %"PRIu16" of %zu bytes to buffer", id, bytes - sizeof(*hdr));

	Buffer_append(bf, bytes, true, ptr);
	return 0;
}

int copy_header_to_bf(const LHE_packetheader_t *hdr, void *args[2])
{
	int *pnfiles = (int*) args[1];
	Buffer_t bf = (Buffer_t)args[0];

	assert(NULL != hdr);

	debug_fprintf(stdout, "Writing header %d to buffer", *pnfiles);
#if DEBUG
	printLHEHeader(*hdr);
#endif
	(*pnfiles)++;

	Buffer_append(bf, sizeof(*hdr), true, hdr);
	return 0;
}


int LHE_read_into_buffer(const char *in, Buffer_t bf)
{
	int nfiles = 0;
	int retval;
	void *args[2] = {bf, &nfiles};

	// I have to pass an initial buffer size somehow, so I pass size as initial guess
	// Other option will be adding another parameter or declaring an extern variable
	retval = LHE_reader_main(in, bf->size, (header_processor)copy_header_to_bf,
		(block_processor)copy_block_to_bf, args);

	die_if_condition(retval < 0, "LHE_reader_main failed!");

	return nfiles;
}
