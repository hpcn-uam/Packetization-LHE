/**
 * @file
 * @author: Eduardo Miravalls Sierra
 * @author: David Muelas Recuenco
 */

#ifndef _RTP_H_
#define _RTP_H_

#ifdef  __cplusplus
extern "C" {
#endif

#include <arpa/inet.h>
#include <inttypes.h>

/**
\verbatim
  RTP Header

    0                   1                   2                   3
    0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1
   +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
   |V=2|P|X|  CC   |M|     PT      |       sequence number         |
   +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
   |                           timestamp                           |
   +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
   |           synchronization source (SSRC) identifier            |
   +=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+
   |            contributing source (CSRC) identifiers             |
   |                             ....                              |
   +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+

\endverbatim
*/

struct RTP_packetheader {
	uint8_t  flags;
	uint8_t  m_pt;
	uint16_t seq_num;
	uint32_t ts;
	uint32_t SSRC;
} __attribute__((packed));

typedef struct RTP_packetheader RTP_packetheader_t;

// These macros get each of the fields
#define RTP_GET_VERSION(p) ((uint8_t)((p).flags & 0xC0)>>6)
#define RTP_GET_P(p)       ((uint8_t)((p).flags & 0x20)>>5)
#define RTP_GET_X(p)       ((uint8_t)((p).flags & 0x10)>>4)
#define RTP_GET_CC(p)      ((p).flags & 0x0F)
#define RTP_GET_M(p)       ((uint8_t)((p).m_pt & 0x80)>>7)
#define RTP_GET_PT(p)      ((p).m_pt & 0x7F)
#define RTP_GET_SEQ_NUM(p) ntohs((p).seq_num)
#define RTP_GET_TS(p)      ntohl((p).ts)
#define RTP_GET_SSRC(p)    ntohl((p).SSRC)


#define RTP_SET_VERSION(rtp, V)       RTP_set_version(&(rtp), V)
#define RTP_SET_P(rtp, P)             RTP_set_P(&(rtp), P)
#define RTP_SET_X(rtp, X)             RTP_set_X(&(rtp), X)
#define RTP_SET_CC(rtp, CC)           RTP_set_CC(&(rtp), CC)
#define RTP_SET_M(rtp, M)             RTP_set_M(&(rtp), M)
#define RTP_SET_PT(rtp, PT)           RTP_set_PT(&(rtp), PT)
#define RTP_SET_SEQ_NUM(rtp, seq_num) RTP_set_seq_num(&(rtp), seq_num)
#define RTP_SET_TS(rtp, ts)           RTP_set_ts(&(rtp), ts)
#define RTP_SET_SSRC(rtp, SSRC)       RTP_set_SSRC(&(rtp), SSRC)


/**
 * setter for Version field. V must fit within 2 bits.
 *
 * @param rtp
 * @param V
 */
static inline
void RTP_set_version(RTP_packetheader_t *rtp, uint8_t V)
{
	rtp->flags &= 0x3F;
	rtp->flags |= ((V<<6) & 0xC0);
}

/**
 * setter for Padding bit.
 *
 * @param rtp
 * @param P   MUST be 1 or 0
 */
static inline
void RTP_set_P(RTP_packetheader_t *rtp, uint8_t P)
{
	rtp->flags &= 0xDF;
	rtp->flags |= ((P & 0x01)<<5);
}

/**
 * setter for Extension bit.
 *
 * @param rtp
 * @param X   MUST be 1 or 0.
 */
static inline
void RTP_set_X(RTP_packetheader_t *rtp, uint8_t X)
{
	rtp->flags &= 0xEF;
	rtp->flags |= ((X & 0x01)<<4);
}

/**
 * setter for CSRC Count
 *
 * @param rtp
 * @param CC  MUST be an integer less than 16
 */
static inline
void RTP_set_CC(RTP_packetheader_t *rtp, uint8_t CC)
{
	rtp->flags &= 0xF0;
	rtp->flags |= (CC & 0x0F);
}

/**
 * setter for Marker bit
 *
 * @param rtp
 * @param M   MUST be 1 or 0
 */
static inline
void RTP_set_M(RTP_packetheader_t *rtp, uint8_t M)
{
	rtp->m_pt &= 0x7F;
	rtp->m_pt |= ((M & 0x01) << 7);
}

/**
 * setter for Payload Type field.
 *
 * @param rtp
 * @param PT  Payload Type
 */
static inline
void RTP_set_PT(RTP_packetheader_t *rtp, uint8_t PT)
{
	rtp->m_pt &= 0x80;
	rtp->m_pt |= (PT & 0x7F);
}

/**
 * setter for the sequence number field.
 *
 * @param rtp
 * @param seq_num
 */
static inline
void RTP_set_seq_num(RTP_packetheader_t *rtp, uint16_t seq_num)
{
	rtp->seq_num = htons(seq_num);
}

/**
 * setter for TimeStamp.
 *
 * @param rtp
 * @param ts  a timestamp.
 */
static inline
void RTP_set_ts(RTP_packetheader_t *rtp, uint32_t ts)
{
	rtp->ts = htonl(ts);
}

/**
 * setter for the Synchronization Source identifier
 *
 * @param rtp
 * @param SSRC
 */
static inline
void RTP_set_SSRC(RTP_packetheader_t *rtp, uint32_t SSRC)
{
	rtp->SSRC = htonl(SSRC);
}


#ifdef  __cplusplus
}
#endif

#endif /* _RTP_H_ */

