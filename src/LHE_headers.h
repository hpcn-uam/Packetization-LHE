/**
 * @file
 * @author: Eduardo Miravalls Sierra
 * @author: David Muelas Recuenco
 * @author: Angel Lopez
 */


#ifndef _LHE_HEADERS_H_
#define _LHE_HEADERS_H_

#ifdef  __cplusplus
extern "C" {
#endif

#include <arpa/inet.h>
#include <stdlib.h>
#include <inttypes.h>
#include <stdio.h>
#include <string.h>

#define MAX_LEN_IP_DATAGRAM 65535 //Max length of buffer
#define MAX_BLOCK_LEN 1450

/**
\verbatim

   LHE Block header

 0                   1           
 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 
+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
| Type|         BlockLen        |
+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+

\endverbatim
*/

struct LHE_blockheader {
	uint16_t type_and_len;
} __attribute__((packed));

typedef struct LHE_blockheader LHE_blockheader_t;

// These macros get each of the fields in a LHE_blockheader_t
#define LHE_BLOCKHEADER_GET_TYPE(s) (ntohs((s).type_and_len) >> 13)
#define LHE_BLOCKHEADER_GET_LEN(s)     (ntohs((s).type_and_len) & 0x1FFF)


static inline
void LHE_blockheader_set_length(LHE_blockheader_t *b, uint16_t len)
{
	uint16_t tmp = ntohs(b->type_and_len);
	b->type_and_len = htons((tmp & 0xE000) | (len & 0x1FFF));
}

static inline
void LHE_blockheader_set_type(LHE_blockheader_t *b, uint16_t t)
{
	uint16_t tmp = ntohs(b->type_and_len);
	b->type_and_len = htons((tmp & 0x1FFF) | (t << 13));
}

// These macros set each of the fields in a LHE_blockheader_t
#define LHE_BLOCKHEADER_SET_LEN(s, len) LHE_blockheader_set_length(&(s), len)
#define LHE_BLOCKHEADER_SET_TYPE(s, t) LHE_blockheader_set_type(&(s), t)

/**
\verbatim

   LHE Header V0

    0                   1                   2                   3
    0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1
   +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
   |V=0|A|D|PROFILE| R |  #BLOCKS  |      BH       |      BW       |
   +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
   |               IH              |               IW              |
   +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
   |         1ST BLOCK id.         |            PAYLOAD            |
   +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+

\endverbatim
*/

/**
\verbatim

   LHE Header V1
    0                   1                   2                   3  
    0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1
   +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
   |V=1|A|R|Profile| C |  #NBlocks |      BH       |      BW       |
   +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
   |               IH              |               IW              |
   +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
   |           1stBlockID          |            Payload            |
   +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+

\endverbatim
*/

/**
   We use the same structure to represent both headers

*/
#define MAX_BLOCKS_PER_CHUNK 63

struct LHE_packetheader {

	uint8_t flags;               // Version, A, D, and profile fields
	uint8_t reserved_and_blocks; // Reserved bits and number of blocks in packet
	uint16_t block_dim;          // Block dimensions (H x W)
	uint16_t im_height;          // Image height in blocks
	uint16_t im_width;           // Image width in blocks
	uint16_t first_block_id;     // ID of the first block

} __attribute__((packed));

typedef struct LHE_packetheader LHE_packetheader_t;

/**
   Getters for all fields in defined versions of LHE over RTP headers
*/

/**
 * @param s pointer to the packet header.
 * @return Version (V) field in header.
 */
static inline
uint8_t LHE_packetheader_get_version(LHE_packetheader_t *s)
{
	return (uint8_t)((s)->flags >> 6);
}

/**
 * @param s pointer to the packet header.
 * @return A Audio bit in header.
 */
static inline
uint8_t LHE_packetheader_get_audio(LHE_packetheader_t *s)
{

	return (uint8_t)(((s)->flags & 0x3F) >> 5);
}

/**
 * @param s pointer to the packet header.
 * @return D Differential bit in header.
 */
static inline
uint8_t LHE_packetheader_get_differential(LHE_packetheader_t *s)
{
	uint8_t ret_aux = 0;
	switch(LHE_packetheader_get_version(s)) {
		case 0: ret_aux = (uint8_t)(((s)->flags & 0x1F) >> 4); break;
		default: ret_aux = 0; break;
	}

	return ret_aux;
}

/**
 * @param s pointer to the packet header.
 * @return P color Profile field in header.
 */
static inline
uint8_t LHE_packetheader_get_profile(LHE_packetheader_t *s)
{
	return (uint8_t)((s)->flags & 0x0F);
}

/**
 * @param s pointer to the packet header.
 * @return R Reserved bits in the packet header.
 */
static inline
uint8_t LHE_packetheader_get_reserved(LHE_packetheader_t *s)
{

	uint8_t ret_aux = 0;
	switch(LHE_packetheader_get_version(s)) {
		case 0: ret_aux = (uint8_t)((s)->reserved_and_blocks >> 6); break;
		default: ret_aux = 0; break;
	}

	return ret_aux;
}

/**
 * @param s pointer to the packet header.
 * @return C Codec bits in the packet header.
 */
static inline
uint8_t LHE_packetheader_get_codec(LHE_packetheader_t *s)
{

	uint8_t ret_aux = 0;
	switch(LHE_packetheader_get_version(s)) {
		case 1: ret_aux = (uint8_t)((s)->reserved_and_blocks >> 6); break;
		default: ret_aux = 0; break;
	}

	return ret_aux;
}


/**
 * @param s pointer to the packet header.
 * @return N number of blocks in the packet header.
 */
static inline
uint8_t LHE_packetheader_get_nblocks(LHE_packetheader_t *s)
{
	return (uint8_t)((s)->reserved_and_blocks & 0x3F);
}

/**
 * @brief this getter will take care of formatting this field according
 * to the specification.
 *
 * @param s pointer to the packet header.
 * @return BH block height IN PIXELS.
 */
static inline
uint16_t LHE_packetheader_get_block_height(LHE_packetheader_t *s)
{

	uint16_t ret_aux = 0;
	switch(LHE_packetheader_get_version(s)) {
		case 0: ret_aux = 2*(uint16_t)((uint8_t *)(&(s->block_dim)))[0]; break;
		//case 1: ret_aux = (uint16_t)((((uint8_t *)(&(s->block_dim)))[0] >> 4)& 0x0F) + 1; break;
		case 1: ret_aux = (uint16_t)((uint8_t *)(&(s->block_dim)))[0] + 1; break;
		default: ret_aux = 0; break;
	}

	return ret_aux;
}

/**
 * @param s pointer to the packet header.
 * @return BH block width IN PIXELS.
 */
static inline
uint16_t LHE_packetheader_get_block_width(LHE_packetheader_t *s)
{
	uint16_t ret_aux = 0;
	switch(LHE_packetheader_get_version(s)) {
		case 0: ret_aux = 2*(uint16_t)((uint8_t *)(&(s->block_dim)))[1]; break;
		case 1: ret_aux = 16*(uint16_t)(((uint8_t *)(&(s->block_dim)))[1] + 1); break;
		default: ret_aux = 0; break;
	}

	return ret_aux;
}

/**
 * @param s pointer to the packet header.
 * @return IH Image Height IN PIXELS IN HBO.
 */
static inline
uint16_t LHE_packetheader_get_im_height(LHE_packetheader_t *s)
{
	return ntohs((s)->im_height);
}

/**
 * @param s pointer to the packet header.
 * @return IW Image Width IN PIXELS IN HBO.
 */
static inline
uint16_t LHE_packetheader_get_im_width(LHE_packetheader_t *s)
{
	return ntohs((s)->im_width);
}

/**
 * @param s pointer to the packet header.
 * @return ID the block ID of the first block IN HBO.
 */
static inline
uint16_t LHE_packetheader_get_1st_block_ID(LHE_packetheader_t *s)
{
	return ntohs((s)->first_block_id);
}

// These macros get each of the fields in a LHE_packetheader_t
#define LHE_PACKETHEADER_GET_VERSION(s) (LHE_packetheader_get_version(&(s)))
#define LHE_PACKETHEADER_GET_A(s) (LHE_packetheader_get_audio(&(s)))
#define LHE_PACKETHEADER_GET_D(s) (LHE_packetheader_get_differential(&(s)))
#define LHE_PACKETHEADER_GET_PROFILE(s) (LHE_packetheader_get_profile(&(s)))
#define LHE_PACKETHEADER_GET_RESERVED(s) (LHE_packetheader_get_reserved(&(s)))
#define LHE_PACKETHEADER_GET_CODEC(s) (LHE_packetheader_get_codec(&(s)))
#define LHE_PACKETHEADER_GET_NBLOCKS(s) (LHE_packetheader_get_nblocks(&(s)))
#define LHE_PACKETHEADER_GET_BLOCK_H(s) (LHE_packetheader_get_block_height(&(s)))
#define LHE_PACKETHEADER_GET_BLOCK_W(s) (LHE_packetheader_get_block_width(&(s)))
#define LHE_PACKETHEADER_GET_IM_H(s) (LHE_packetheader_get_im_height(&(s)))
#define LHE_PACKETHEADER_GET_IM_W(s) (LHE_packetheader_get_im_width(&(s)))
#define LHE_PACKETHEADER_GET_FB_ID(s) (LHE_packetheader_get_1st_block_ID(&(s)))
#define LHE_PACKETHEADER_GET_IM_SIZE(s) (LHE_PACKETHEADER_GET_BLOCK_H(s)*LHE_PACKETHEADER_GET_BLOCK_W(s))
#define LHE_PACKETHEADER_GET_IM_NBLOCKS(s) (LHE_PACKETHEADER_GET_IM_H(s)*LHE_PACKETHEADER_GET_IM_W(s))

/**
   Setters for all fields in defined versions of LHE over RTP headers
*/

/**
 * @param s pointer to the packet header.
 * @param V an integer with a version number, like the one LHE_PACKETHEADER_GET_VERSION(s) would return.
 */
static inline
void LHE_packetheader_set_version(LHE_packetheader_t *s, uint8_t V)
{
	s->flags &= 0x3F;
	s->flags |= (V << 6);
}

/**
 * @param s pointer to the packet header.
 * @param A Audio bit. MUST BE 1 (audio) or 0 (video).
 */
static inline
void LHE_packetheader_set_audio(LHE_packetheader_t *s, uint8_t A)
{
	s->flags &= 0xDF;
	s->flags |= ((A & 0x01) << 5);
}

/**
 * @param s pointer to the packet header.
 * @param D Differential bit. MUST BE 1 (differential) or 0 (absolute).
 */
static inline
void LHE_packetheader_set_differential(LHE_packetheader_t *s, uint8_t D)
{

	switch(LHE_packetheader_get_version(s)) {
		case 0:
			s->flags &= 0xEF;
			s->flags |= ((D & 0x01) << 4);
			break;
		default: break;
	}
}

/**
 * @param s pointer to the packet header.
 * @param P color Profile.
 */
static inline
void LHE_packetheader_set_profile(LHE_packetheader_t *s, uint8_t P)
{
	s->flags &= 0xF0;
	s->flags |= (P & 0x0F);
}

/**
 * @param s pointer to the packet header.
 * @param R Reserved bits. MUST BE SET TO 0
 */
static inline
void LHE_packetheader_set_reserved(LHE_packetheader_t *s, uint8_t R)
{
	switch(LHE_packetheader_get_version(s)) {
		case 0:
			s->reserved_and_blocks &= 0x3F;
			s->reserved_and_blocks |= (R << 6);
			break;
		default: break;
	}
}

/**
 * @param s pointer to the packet header.
 * @param C Codec bits in the packet header.
 */
static inline
void LHE_packetheader_set_codec(LHE_packetheader_t *s, uint8_t R)
{
	switch(LHE_packetheader_get_version(s)) {
		case 1:
			s->reserved_and_blocks &= 0x3F;
			s->reserved_and_blocks |= (R << 6);
			break;
		default: break;
	}
}

/**
 * @param s pointer to the packet header.
 * @param N number of blocks in the packet.
 */
static inline
void LHE_packetheader_set_nblocks(LHE_packetheader_t *s, uint8_t N)
{
	s->reserved_and_blocks &= 0xC0;
	s->reserved_and_blocks |= (N & 0x3F);
}

/**
 * @brief this setter will take care of formatting this field according
 * to the specification.
 *
 * @param s pointer to the packet header.
 * @param BH block height IN PIXELS.
 */
static inline
void LHE_packetheader_set_block_height(LHE_packetheader_t *s, uint16_t BH)
{
	switch(LHE_packetheader_get_version(s)) {
		case 0: ((uint8_t *)(&(s->block_dim)))[0] = (uint8_t)(BH / 2); break;
//		case 1: ((uint8_t *)(&(s->block_dim)))[0] |= (((uint8_t) BH -1) << 4); break;
		case 1: ((uint8_t *)(&(s->block_dim)))[0] = (uint8_t)(BH - 1); break;
		default: break;
	}
}

static inline
void LHE_packetheader_copy_block_height(LHE_packetheader_t *s, uint16_t BH)
{
	((uint8_t *)(&(s->block_dim)))[0] = (uint8_t)BH;
}

/**
 * @param s pointer to the packet header.
 * @param BH block width IN PIXELS.
 */
static inline
void LHE_packetheader_set_block_width(LHE_packetheader_t *s, uint16_t BW)
{
	switch(LHE_packetheader_get_version(s)) {
		case 0: ((uint8_t *)(&(s->block_dim)))[1] = (uint8_t)(BW / 2); break;
//		case 1: s->block_dim |= htons((BW /16) -1); break;
		case 1: ((uint8_t *)(&(s->block_dim)))[1] = (uint8_t)((BW / 16)-1); break;
		default: break;
	}
}

static inline
void LHE_packetheader_copy_block_width(LHE_packetheader_t *s, uint16_t BW)
{
	((uint8_t *)(&(s->block_dim)))[1] = (uint8_t)BW;
}

/**
 * @param s pointer to the packet header.
 * @param IH Image Height IN PIXELS IN HBO.
 */
static inline
void LHE_packetheader_set_im_height(LHE_packetheader_t *s, uint16_t IH)
{
	s->im_height = htons(IH);
}

/**
 * @param s pointer to the packet header.
 * @param IW Image Width IN PIXELS IN HBO.
 */
static inline
void LHE_packetheader_set_im_width(LHE_packetheader_t *s, uint16_t IW)
{
	s->im_width = htons(IW);
}

/**
 * @param s pointer to the packet header.
 * @param ID the block ID of the first block IN HBO.
 */
static inline
void LHE_packetheader_set_1st_block_ID(LHE_packetheader_t *s, uint16_t ID)
{
	s->first_block_id = htons(ID);
}


// These macros set each of the fields of a packet header
#define LHE_PACKETHEADER_SET_VERSION(s, V)  LHE_packetheader_set_version(&(s), V)
#define LHE_PACKETHEADER_SET_A(s, A)        LHE_packetheader_set_audio(&(s), A)
#define LHE_PACKETHEADER_SET_D(s, D)        LHE_packetheader_set_differential(&(s), D)
#define LHE_PACKETHEADER_SET_PROFILE(s, P)  LHE_packetheader_set_profile(&(s), P)
#define LHE_PACKETHEADER_SET_RESERVED(s, R) LHE_packetheader_set_reserved(&(s), R)
#define LHE_PACKETHEADER_SET_CODEC(s, C)    LHE_packetheader_set_codec(&(s), C)
#define LHE_PACKETHEADER_SET_NBLOCKS(s, N)  LHE_packetheader_set_nblocks(&(s), N)
#define LHE_PACKETHEADER_SET_BLOCK_H(s, BH) LHE_packetheader_set_block_height(&(s), BH)
#define LHE_PACKETHEADER_SET_BLOCK_W(s, BW) LHE_packetheader_set_block_width(&(s), BW)
#define LHE_PACKETHEADER_SET_IM_H(s, IH)    LHE_packetheader_set_im_height(&(s), IH)
#define LHE_PACKETHEADER_SET_IM_W(s, IW)    LHE_packetheader_set_im_width(&(s), IW)
#define LHE_PACKETHEADER_SET_FB_ID(s, ID)   LHE_packetheader_set_1st_block_ID(&(s), ID)

static inline
void LHE_header_change_version(LHE_packetheader_t * original, LHE_packetheader_t * converted, uint8_t new_version)
{

	LHE_packetheader_t aux_h = {0};
	memcpy(&aux_h, original, sizeof(LHE_packetheader_t));

	LHE_PACKETHEADER_SET_VERSION(*converted, new_version);
	LHE_PACKETHEADER_SET_A(*converted, LHE_PACKETHEADER_GET_A(aux_h));
	LHE_PACKETHEADER_SET_D(*converted, LHE_PACKETHEADER_GET_D(aux_h));
	LHE_PACKETHEADER_SET_PROFILE(*converted, LHE_PACKETHEADER_GET_PROFILE(aux_h));
	LHE_PACKETHEADER_SET_RESERVED(*converted, LHE_PACKETHEADER_GET_RESERVED(aux_h));
	LHE_PACKETHEADER_SET_CODEC(*converted, LHE_PACKETHEADER_GET_CODEC(aux_h));
	LHE_PACKETHEADER_SET_NBLOCKS(*converted, LHE_PACKETHEADER_GET_NBLOCKS(aux_h));
	LHE_PACKETHEADER_SET_BLOCK_H(*converted, LHE_PACKETHEADER_GET_BLOCK_H(aux_h));
	LHE_PACKETHEADER_SET_BLOCK_W(*converted, LHE_PACKETHEADER_GET_BLOCK_W(aux_h));
	LHE_PACKETHEADER_SET_IM_H(*converted, LHE_PACKETHEADER_GET_IM_H(aux_h));
	LHE_PACKETHEADER_SET_IM_W(*converted, LHE_PACKETHEADER_GET_IM_W(aux_h));
	LHE_PACKETHEADER_SET_FB_ID(*converted, LHE_PACKETHEADER_GET_FB_ID(aux_h));
}

static inline
void print_LHE_header(LHE_packetheader_t * h)
{

	printf("/****************************************/\n");
	printf("Version: %" PRIu8 "\n",LHE_PACKETHEADER_GET_VERSION(*h));
	printf("A: %" PRIx8 "\n",LHE_PACKETHEADER_GET_A(*h));
	printf("D: %" PRIx8 "\n",LHE_PACKETHEADER_GET_D(*h));
	printf("PROFILE: %" PRIx8 "\n",LHE_PACKETHEADER_GET_PROFILE(*h));
	printf("RESERVED: %" PRIx8 "\n",LHE_PACKETHEADER_GET_RESERVED(*h));
	printf("CODEC: %" PRIx8 "\n",LHE_PACKETHEADER_GET_CODEC(*h));
	printf("NBLOCKS: %" PRIu8 "\n",LHE_PACKETHEADER_GET_NBLOCKS(*h));
	printf("BH: %" PRIu16 "\n",LHE_PACKETHEADER_GET_BLOCK_H(*h));
	printf("BW: %" PRIu16 "\n",LHE_PACKETHEADER_GET_BLOCK_W(*h));
	printf("IMH: %" PRIu16 "\n",LHE_PACKETHEADER_GET_IM_H(*h));
	printf("IMW: %" PRIu16 "\n",LHE_PACKETHEADER_GET_IM_W(*h));
	printf("/****************************************/\n");

}


#ifdef  __cplusplus
}
#endif

#endif /* _LHE_HEADERS_H_ */
