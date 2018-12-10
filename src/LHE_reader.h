/**
 * @file
 * @author: Eduardo Miravalls Sierra
 * @author: David Muelas Recuenco
 */


#ifndef _LHE_READER_H_
#define _LHE_READER_H_

#ifdef  __cplusplus
extern "C" {
#endif

#include <stdio.h>

#include "Buffer.h"
#include "LHE_pack.h"

/**
 * Allocates a new reader.
 *
 * @note The file doesn't necessarily have to be at the beginning,
 * it just have to be at the beginning of a packet header.
 * This allows to read multiple frames stored in the same file.
 *
 * @param  fin an open file descriptor, with READ WRITES.
 *
 * @return     an initialized reader.
 */
LHE_Reader_t LHE_new_reader(FILE *fin);

/**
 * Deallocates a reader. The file WILL NOT BE CLOSED.
 *
 * @param reader
 */
void LHE_free_reader(LHE_Reader_t reader);

/**
 * Read the next block from the file to a buffer.
 *
 * @param  bf
 * @param  reader
 *
 * @return        0 if No more blocks. < 0 if error, > 0 is the number of bytes read.
 */
int LHE_get_next(Buffer_t bf, LHE_Reader_t reader);

/**
 * Reads a LHE file, allowing to process it one block at a time. The function pointers can be NULL to indicate that no operation must be done with blocks and/or headers.
 *
 * @param path path to file.
 * @param initial_buffer size of the read Buffer.
 * @param process_header callback. if != NULL, this function will be called after reading each LHE header.
 * @param process_block callback. if != NULL, this function will be called after reading each LHE block.
 * @param aux argument directly passed to process_X callbacks.
 *
 * @return -1 if error. 0 if success.
 */
int LHE_reader_main(const char *path, size_t initial_buffer,
	header_processor process_header, block_processor process_block, void *aux);

/**
 * Reads the file in path and stores it in a buffer.
 *
 * @param path
 * @param bf
 *
 * @return -1 if an error occurred. If success, the number of lhe frames stored in bf.
 */
int LHE_read_into_buffer(const char *path, Buffer_t bf);

#ifdef  __cplusplus
}
#endif

#endif /* _LHE_READER_H_ */
