/**
 * @file
 * @author: Eduardo Miravalls Sierra
 * @author: David Muelas Recuenco
 */


#ifndef _BUFFER_H_
#define _BUFFER_H_

#ifdef  __cplusplus
extern "C" {
#endif

#include <stdio.h>
#include <stdbool.h>
#include <sys/types.h>
#include <sys/socket.h>

typedef struct buffer {
	char  *buf;  /**< memory */
	size_t off;  /**< offset in buffer */
	size_t used; /**< bytes used in buf */
	size_t size; /**< size in bytes of buf. used <= size */
} *Buffer_t;

/**
 * Allocates a new Buffer.
 *
 * @param size initial size of buf in bytes.
 *
 * @return NULL if allocation failed.
 */
Buffer_t Buffer_new(size_t size);

/**
 * Reads upto B bytes in the Buffer. If off is not 0, it will try to write
 * the data starting at that offset. If growing is allowed, the buffer will grow
 * to at least bytes+off, otherwise the call fails.
 *
 * @param  bf
 * @param  B        number of bytes to read.
 * @param  growable allows resizing if buffer doesn't have enough space.
 * @param  f        file descriptor.
 *
 * @return <= B if not enough bytes read, -1 if not enough memory or
 * allocation failed
 */
int Buffer_fread(Buffer_t bf, size_t B, bool growable, FILE *f);

/**
 * Reads upto B bytes in the Buffer. If off is not 0, it will try to write
 * the data starting at that offset. If growing is allowed, the buffer will grow
 * to at least bytes+off, otherwise the call fails.
 *
 * @param  bf
 * @param  B        number of bytes to read.
 * @param  growable allows resizing if buffer doesn't have enough space.
 * @param  fd       file descriptor.
 *
 * @return <= B if not enough bytes read, -1 if not enough memory or
 * allocation failed
 */
int Buffer_read(Buffer_t bf, size_t B, bool growable, int fd);

/**
 * Calls recvfrom using bf as buffer.
 *
 * @param  bf
 * @param  B        maximum number of bytes to recv.
 * @param  growable allows resizing if buffer doesn't have enough space.
 * @param  fd       socket fd.
 * @param  src_addr see recvfrom()
 * @param  addrlen  see recvfrom()
 *
 * @return          -1 if not enough space. Otherwise, what recvfrom returned.
 */
int Buffer_recvfrom(Buffer_t bf, size_t B, bool growable, int fd, struct sockaddr *src_addr, socklen_t *addrlen);

/**
 * Resizes a Buffer. If reallocation failed, the bf is unchanged.
 *
 * @param bf
 * @param new_size
 *
 * @return 0 if success, != 0 if reallocation failed.
 */
int Buffer_resize(Buffer_t bf, size_t new_size);

/**
 * Clears a buffer as if it was new.
 *
 * @param bf
 *
 * @return 0 for success, != 0 for error.
 */
void Buffer_clear(Buffer_t bf);

/**
 * Frees a Buffer.
 *
 * @param bf
 */
void Buffer_free(Buffer_t bf);

/**
 * Move some bytes in the buffer to another part. to+bytes and from+bytes MUST
 * BE less than the size of the buffer.
 *
 * @param bf
 * @param to    offset
 * @param from  offset
 * @param bytes how many bytes to move
 */
void Buffer_move(Buffer_t bf, size_t to, size_t from, size_t bytes);

/**
 * Move some bytes starting in bf->off. bf->off will be updated to "to".
 *
 * @param  bf
 * @param  to
 * @param  bytes
 */
#define Buffer_move_from_off(bf, to, bytes) do{ Buffer_move(bf, to, bf->off, bytes); bf->off=to;} while(0)

/**
 * Copy B bytes from src to end of buffer. The end of buffer is determined
 * by the used variable.
 *
 * @param bf
 * @param B        how many bytes to copy from src
 * @param growable indicates if buffer can be grown if not enough capacity.
 * @param src      bytes will be copied from here.
 *
 * @return -1 if not enough space. 0 if success.
 */
int Buffer_append(Buffer_t bf, size_t B, bool growable, const void *src);


#ifdef  __cplusplus
}
#endif

#endif /* _BUFFER_H_ */

