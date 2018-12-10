/**
 * @file
 * @author: Eduardo Miravalls Sierra
 * @author: David Muelas Recuenco
 * @author: Angel Lopez
 */


#ifndef _LHE_PACK_H_
#define _LHE_PACK_H_

#ifdef  __cplusplus
extern "C" {
#endif

#include <inttypes.h>
#include <dirent.h>
#include <sys/socket.h>

typedef struct LHE_Reader *LHE_Reader_t;

typedef int (*block_processor)(const void *ptr, uint16_t id, void *aux);
typedef int (*header_processor)(const void *ptr, void *aux);

int filter_files(const struct dirent *entry);

//TO DO: check SSRC on receivers
int LHE_receiver(int socket, struct sockaddr_in si_other, const char *out, size_t initial_buffer_size);
int LHE_receiver2(int socket, struct sockaddr_in si_other, const char *out, size_t initial_buffer_size);
int LHE_receiver3(int socket, struct sockaddr_in si_other, const char *out, size_t initial_buffer_size);

int LHE_sender(int socket, size_t initial_buffer_size,
	uint16_t initial_seq_num, uint32_t initial_ts, uint32_t initial_SSRC,
	uint8_t *in, size_t number_frames, size_t max_chunk_size);

int LHE_send_from_memory(int socket, size_t initial_buffer_size, 
	uint16_t initial_seq_num, uint32_t initial_ts, uint32_t initial_SSRC, size_t max_chunk_size);

//
int LHE_receive_file(const char * file, int socket, size_t initial_buffer_size, uint32_t initial_SSRC);
int LHE_receive_dir(int socket, size_t initial_buffer_size, uint32_t initial_SSRC);
int LHE_receive_pipe(int socket, size_t initial_buffer_size, uint32_t initial_SSRC);

int LHE_send_file(const char * file, int socket, size_t initial_buffer_size, uint16_t initial_seq_num, uint32_t initial_ts, uint32_t initial_SSRC);
int LHE_send_dir(const char * dir, int socket, size_t initial_buffer_size, uint16_t initial_seq_num, uint32_t initial_ts, uint32_t initial_SSRC);
int LHE_send_mem(int socket, size_t initial_buffer_size, uint16_t initial_seq_num, uint32_t initial_ts, uint32_t initial_SSRC);

int get_udp_sender_socket(const char *remote_ip, uint16_t remote_port, uint16_t * local_port);
int get_udp_receiver_socket(const char *remote_ip, uint16_t * local_port);
//

#ifdef  __cplusplus
}
#endif

#endif /* _LHE_PACK_H_ */

