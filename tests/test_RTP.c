/**
 * @file
 *
 *  Test macros in RTP.h
 *  hex got from:https://wiki.wireshark.org/SampleCaptures
 *  sample: h263-over-rtp.pcap
 */

#include <assert.h>
#include <stdio.h>

#include "RTP.h"

#include "utils.h"

void test_packet_5(void)
{
	char pckt5[] = {
		0x80,0x22,0xd2,0xc5,
		0x24,0x27,0x6e,0x4a,
		0x54,0x82,0xec,0xe0
	};
	RTP_packetheader_t p = *(RTP_packetheader_t *)&pckt5;

	printf("%s:  ", __PRETTY_FUNCTION__);

	assert(2 == RTP_GET_VERSION(p));
	assert(0 == RTP_GET_P(p));
	assert(0 == RTP_GET_X(p));
	assert(0 == RTP_GET_CC(p));
	assert(0 == RTP_GET_M(p));
	assert(34 == RTP_GET_PT(p));
	assert(53957 == RTP_GET_SEQ_NUM(p));
	assert(606563914 == RTP_GET_TS(p));
	assert(1417866464 == RTP_GET_SSRC(p));

	printf("PASSED!!\n");
}

void test_packet_13(void)
{
	char pckt13[] = {
		0x80,0xa2,0xd2,0xcd,
		0x24,0x27,0x6e,0x4a,
		0x54,0x82,0xec,0xe0
	};
	RTP_packetheader_t p = *(RTP_packetheader_t *)&pckt13;

	printf("%s: ", __PRETTY_FUNCTION__);

	assert(2 == RTP_GET_VERSION(p));
	assert(0 == RTP_GET_P(p));
	assert(0 == RTP_GET_X(p));
	assert(0 == RTP_GET_CC(p));
	assert(1 == RTP_GET_M(p));
	assert(34 == RTP_GET_PT(p));
	assert(53965 == RTP_GET_SEQ_NUM(p));
	assert(606563914 == RTP_GET_TS(p));
	assert(1417866464 == RTP_GET_SSRC(p));

	printf("PASSED!!\n");
}

void test_set_version_zeros(void)
{
	char zeros[] = {
		0x00,0x00,0x00,0x00,
		0x00,0x00,0x00,0x00,
		0x00,0x00,0x00,0x00
	};
	RTP_packetheader_t p = *(RTP_packetheader_t *)&zeros;

	printf("%s: ", __PRETTY_FUNCTION__);

	RTP_SET_VERSION(p, 1);
	assert(1 == RTP_GET_VERSION(p));
	assert(0 == RTP_GET_P(p));
	assert(0 == RTP_GET_X(p));
	assert(0 == RTP_GET_CC(p));
	assert(0 == RTP_GET_M(p));
	assert(0 == RTP_GET_PT(p));
	assert(0 == RTP_GET_SEQ_NUM(p));
	assert(0 == RTP_GET_TS(p));
	assert(0 == RTP_GET_SSRC(p));

	RTP_SET_VERSION(p, 2);
	assert(2 == RTP_GET_VERSION(p));
	assert(0 == RTP_GET_P(p));
	assert(0 == RTP_GET_X(p));
	assert(0 == RTP_GET_CC(p));
	assert(0 == RTP_GET_M(p));
	assert(0 == RTP_GET_PT(p));
	assert(0 == RTP_GET_SEQ_NUM(p));
	assert(0 == RTP_GET_TS(p));
	assert(0 == RTP_GET_SSRC(p));

	RTP_SET_VERSION(p, 3);
	assert(3 == RTP_GET_VERSION(p));
	assert(0 == RTP_GET_P(p));
	assert(0 == RTP_GET_X(p));
	assert(0 == RTP_GET_CC(p));
	assert(0 == RTP_GET_M(p));
	assert(0 == RTP_GET_PT(p));
	assert(0 == RTP_GET_SEQ_NUM(p));
	assert(0 == RTP_GET_TS(p));
	assert(0 == RTP_GET_SSRC(p));
	printf("PASSED!!\n");
}

void test_set_version_ones(void)
{
	char ones[] = {
		0xFF,0xFF,0xFF,0xFF,
		0xFF,0xFF,0xFF,0xFF,
		0xFF,0xFF,0xFF,0xFF
	};
	RTP_packetheader_t p = *(RTP_packetheader_t *)&ones;

	printf("%s: ", __PRETTY_FUNCTION__);

	RTP_SET_VERSION(p, 0);
	assert(0x0 == RTP_GET_VERSION(p));
	assert(0x1 == RTP_GET_P(p));
	assert(0x1 == RTP_GET_X(p));
	assert(0x0F == RTP_GET_CC(p));
	assert(0x1 == RTP_GET_M(p));
	assert(0x7F == RTP_GET_PT(p));
	assert(0xFFFF == RTP_GET_SEQ_NUM(p));
	assert(0xFFFFFFFF == RTP_GET_TS(p));
	assert(0xFFFFFFFF == RTP_GET_SSRC(p));

	RTP_SET_VERSION(p, 1);
	assert(1 == RTP_GET_VERSION(p));
	assert(0x1 == RTP_GET_P(p));
	assert(0x1 == RTP_GET_X(p));
	assert(0x0F == RTP_GET_CC(p));
	assert(0x1 == RTP_GET_M(p));
	assert(0x7F == RTP_GET_PT(p));
	assert(0xFFFF == RTP_GET_SEQ_NUM(p));
	assert(0xFFFFFFFF == RTP_GET_TS(p));
	assert(0xFFFFFFFF == RTP_GET_SSRC(p));

	RTP_SET_VERSION(p, 2);
	assert(2 == RTP_GET_VERSION(p));
	assert(0x1 == RTP_GET_P(p));
	assert(0x1 == RTP_GET_X(p));
	assert(0x0F == RTP_GET_CC(p));
	assert(0x1 == RTP_GET_M(p));
	assert(0x7F == RTP_GET_PT(p));
	assert(0xFFFF == RTP_GET_SEQ_NUM(p));
	assert(0xFFFFFFFF == RTP_GET_TS(p));
	assert(0xFFFFFFFF == RTP_GET_SSRC(p));
	printf("PASSED!!\n");
}

void test_set_P_zeros(void)
{
	char zeros[] = {
		0x00,0x00,0x00,0x00,
		0x00,0x00,0x00,0x00,
		0x00,0x00,0x00,0x00
	};
	RTP_packetheader_t p = *(RTP_packetheader_t *)&zeros;

	printf("%s: ", __PRETTY_FUNCTION__);

	RTP_SET_P(p, 1);
	assert(0 == RTP_GET_VERSION(p));
	assert(1 == RTP_GET_P(p));
	assert(0 == RTP_GET_X(p));
	assert(0 == RTP_GET_CC(p));
	assert(0 == RTP_GET_M(p));
	assert(0 == RTP_GET_PT(p));
	assert(0 == RTP_GET_SEQ_NUM(p));
	assert(0 == RTP_GET_TS(p));
	assert(0 == RTP_GET_SSRC(p));
	printf("PASSED!!\n");
}

void test_set_P_ones(void)
{
	char ones[] = {
		0xFF,0xFF,0xFF,0xFF,
		0xFF,0xFF,0xFF,0xFF,
		0xFF,0xFF,0xFF,0xFF
	};
	RTP_packetheader_t p = *(RTP_packetheader_t *)&ones;

	printf("%s: ", __PRETTY_FUNCTION__);

	RTP_SET_P(p, 0);
	assert(0x3 == RTP_GET_VERSION(p));
	assert(0x0 == RTP_GET_P(p));
	assert(0x1 == RTP_GET_X(p));
	assert(0x0F == RTP_GET_CC(p));
	assert(0x1 == RTP_GET_M(p));
	assert(0x7F == RTP_GET_PT(p));
	assert(0xFFFF == RTP_GET_SEQ_NUM(p));
	assert(0xFFFFFFFF == RTP_GET_TS(p));
	assert(0xFFFFFFFF == RTP_GET_SSRC(p));
	printf("PASSED!!\n");
}

void test_set_X_zeros(void)
{
	char zeros[] = {
		0x00,0x00,0x00,0x00,
		0x00,0x00,0x00,0x00,
		0x00,0x00,0x00,0x00
	};
	RTP_packetheader_t p = *(RTP_packetheader_t *)&zeros;

	printf("%s: ", __PRETTY_FUNCTION__);

	RTP_SET_X(p, 1);
	assert(0 == RTP_GET_VERSION(p));
	assert(0 == RTP_GET_P(p));
	assert(1 == RTP_GET_X(p));
	assert(0 == RTP_GET_CC(p));
	assert(0 == RTP_GET_M(p));
	assert(0 == RTP_GET_PT(p));
	assert(0 == RTP_GET_SEQ_NUM(p));
	assert(0 == RTP_GET_TS(p));
	assert(0 == RTP_GET_SSRC(p));
	printf("PASSED!!\n");
}

void test_set_X_ones(void)
{
	char ones[] = {
		0xFF,0xFF,0xFF,0xFF,
		0xFF,0xFF,0xFF,0xFF,
		0xFF,0xFF,0xFF,0xFF
	};
	RTP_packetheader_t p = *(RTP_packetheader_t *)&ones;

	printf("%s: ", __PRETTY_FUNCTION__);

	RTP_SET_X(p, 0);
	assert(0x3 == RTP_GET_VERSION(p));
	assert(0x1 == RTP_GET_P(p));
	assert(0 == RTP_GET_X(p));
	assert(0x0F == RTP_GET_CC(p));
	assert(0x1 == RTP_GET_M(p));
	assert(0x7F == RTP_GET_PT(p));
	assert(0xFFFF == RTP_GET_SEQ_NUM(p));
	assert(0xFFFFFFFF == RTP_GET_TS(p));
	assert(0xFFFFFFFF == RTP_GET_SSRC(p));
	printf("PASSED!!\n");
}

void test_set_CC_zeros(void)
{
	char zeros[] = {
		0x00,0x00,0x00,0x00,
		0x00,0x00,0x00,0x00,
		0x00,0x00,0x00,0x00
	};
	RTP_packetheader_t p = *(RTP_packetheader_t *)&zeros;

	printf("%s: ", __PRETTY_FUNCTION__);

	RTP_SET_CC(p, 10);
	assert(0 == RTP_GET_VERSION(p));
	assert(0 == RTP_GET_P(p));
	assert(0 == RTP_GET_X(p));
	assert(10 == RTP_GET_CC(p));
	assert(0 == RTP_GET_M(p));
	assert(0 == RTP_GET_PT(p));
	assert(0 == RTP_GET_SEQ_NUM(p));
	assert(0 == RTP_GET_TS(p));
	assert(0 == RTP_GET_SSRC(p));
	printf("PASSED!!\n");
}

void test_set_CC_ones(void)
{
	char ones[] = {
		0xFF,0xFF,0xFF,0xFF,
		0xFF,0xFF,0xFF,0xFF,
		0xFF,0xFF,0xFF,0xFF
	};
	RTP_packetheader_t p = *(RTP_packetheader_t *)&ones;

	printf("%s: ", __PRETTY_FUNCTION__);

	RTP_SET_CC(p, 5);
	assert(0x3 == RTP_GET_VERSION(p));
	assert(0x1 == RTP_GET_P(p));
	assert(0x1 == RTP_GET_X(p));
	assert(5 == RTP_GET_CC(p));
	assert(0x1 == RTP_GET_M(p));
	assert(0x7F == RTP_GET_PT(p));
	assert(0xFFFF == RTP_GET_SEQ_NUM(p));
	assert(0xFFFFFFFF == RTP_GET_TS(p));
	assert(0xFFFFFFFF == RTP_GET_SSRC(p));
	printf("PASSED!!\n");
}

void test_set_M_zeros(void)
{
	char zeros[] = {
		0x00,0x00,0x00,0x00,
		0x00,0x00,0x00,0x00,
		0x00,0x00,0x00,0x00
	};
	RTP_packetheader_t p = *(RTP_packetheader_t *)&zeros;

	printf("%s: ", __PRETTY_FUNCTION__);

	RTP_SET_M(p, 1);
	assert(0 == RTP_GET_VERSION(p));
	assert(0 == RTP_GET_P(p));
	assert(0 == RTP_GET_X(p));
	assert(0 == RTP_GET_CC(p));
	assert(1 == RTP_GET_M(p));
	assert(0 == RTP_GET_PT(p));
	assert(0 == RTP_GET_SEQ_NUM(p));
	assert(0 == RTP_GET_TS(p));
	assert(0 == RTP_GET_SSRC(p));
	printf("PASSED!!\n");
}

void test_set_M_ones(void)
{
	char ones[] = {
		0xFF,0xFF,0xFF,0xFF,
		0xFF,0xFF,0xFF,0xFF,
		0xFF,0xFF,0xFF,0xFF
	};
	RTP_packetheader_t p = *(RTP_packetheader_t *)&ones;

	printf("%s: ", __PRETTY_FUNCTION__);

	RTP_SET_M(p, 0);
	assert(0x3 == RTP_GET_VERSION(p));
	assert(0x1 == RTP_GET_P(p));
	assert(0x1 == RTP_GET_X(p));
	assert(0x0F == RTP_GET_CC(p));
	assert(0 == RTP_GET_M(p));
	assert(0x7F == RTP_GET_PT(p));
	assert(0xFFFF == RTP_GET_SEQ_NUM(p));
	assert(0xFFFFFFFF == RTP_GET_TS(p));
	assert(0xFFFFFFFF == RTP_GET_SSRC(p));
	printf("PASSED!!\n");
}

void test_set_PT_zeros(void)
{
	char zeros[] = {
		0x00,0x00,0x00,0x00,
		0x00,0x00,0x00,0x00,
		0x00,0x00,0x00,0x00
	};
	RTP_packetheader_t p = *(RTP_packetheader_t *)&zeros;

	printf("%s: ", __PRETTY_FUNCTION__);

	RTP_SET_PT(p, 34);
	assert(0 == RTP_GET_VERSION(p));
	assert(0 == RTP_GET_P(p));
	assert(0 == RTP_GET_X(p));
	assert(0 == RTP_GET_CC(p));
	assert(0 == RTP_GET_M(p));
	assert(34 == RTP_GET_PT(p));
	assert(0 == RTP_GET_SEQ_NUM(p));
	assert(0 == RTP_GET_TS(p));
	assert(0 == RTP_GET_SSRC(p));
	printf("PASSED!!\n");
}

void test_set_PT_ones(void)
{
	char ones[] = {
		0xFF,0xFF,0xFF,0xFF,
		0xFF,0xFF,0xFF,0xFF,
		0xFF,0xFF,0xFF,0xFF
	};
	RTP_packetheader_t p = *(RTP_packetheader_t *)&ones;

	printf("%s: ", __PRETTY_FUNCTION__);

	RTP_SET_PT(p, 34);
	assert(0x3 == RTP_GET_VERSION(p));
	assert(0x1 == RTP_GET_P(p));
	assert(0x1 == RTP_GET_X(p));
	assert(0x0F == RTP_GET_CC(p));
	assert(0x1 == RTP_GET_M(p));
	assert(34 == RTP_GET_PT(p));
	assert(0xFFFF == RTP_GET_SEQ_NUM(p));
	assert(0xFFFFFFFF == RTP_GET_TS(p));
	assert(0xFFFFFFFF == RTP_GET_SSRC(p));
	printf("PASSED!!\n");
}

void test_set_seq_num_zeros(void)
{
	char zeros[] = {
		0x00,0x00,0x00,0x00,
		0x00,0x00,0x00,0x00,
		0x00,0x00,0x00,0x00
	};
	RTP_packetheader_t p = *(RTP_packetheader_t *)&zeros;

	printf("%s: ", __PRETTY_FUNCTION__);

	RTP_SET_SEQ_NUM(p, 0xDEAD);
	assert(0 == RTP_GET_VERSION(p));
	assert(0 == RTP_GET_P(p));
	assert(0 == RTP_GET_X(p));
	assert(0 == RTP_GET_CC(p));
	assert(0 == RTP_GET_M(p));
	assert(0 == RTP_GET_PT(p));
	assert(0xDEAD == RTP_GET_SEQ_NUM(p));
	assert(0 == RTP_GET_TS(p));
	assert(0 == RTP_GET_SSRC(p));
	printf("PASSED!!\n");
}

void test_set_seq_num_ones(void)
{
	char ones[] = {
		0xFF,0xFF,0xFF,0xFF,
		0xFF,0xFF,0xFF,0xFF,
		0xFF,0xFF,0xFF,0xFF
	};
	RTP_packetheader_t p = *(RTP_packetheader_t *)&ones;

	printf("%s: ", __PRETTY_FUNCTION__);

	RTP_SET_SEQ_NUM(p, 0xCAFE);
	assert(0x3 == RTP_GET_VERSION(p));
	assert(0x1 == RTP_GET_P(p));
	assert(0x1 == RTP_GET_X(p));
	assert(0x0F == RTP_GET_CC(p));
	assert(0x1 == RTP_GET_M(p));
	assert(0x7F == RTP_GET_PT(p));
	assert(0xCAFE== RTP_GET_SEQ_NUM(p));
	assert(0xFFFFFFFF == RTP_GET_TS(p));
	assert(0xFFFFFFFF == RTP_GET_SSRC(p));
	printf("PASSED!!\n");
}

void test_set_ts_zeros(void)
{
	char zeros[] = {
		0x00,0x00,0x00,0x00,
		0x00,0x00,0x00,0x00,
		0x00,0x00,0x00,0x00
	};
	RTP_packetheader_t p = *(RTP_packetheader_t *)&zeros;

	printf("%s: ", __PRETTY_FUNCTION__);

	RTP_SET_TS(p, 0xDEADBABE);
	assert(0 == RTP_GET_VERSION(p));
	assert(0 == RTP_GET_P(p));
	assert(0 == RTP_GET_X(p));
	assert(0 == RTP_GET_CC(p));
	assert(0 == RTP_GET_M(p));
	assert(0 == RTP_GET_PT(p));
	assert(0 == RTP_GET_SEQ_NUM(p));
	assert(0xDEADBABE == RTP_GET_TS(p));
	assert(0 == RTP_GET_SSRC(p));
	printf("PASSED!!\n");
}

void test_set_ts_ones(void)
{
	char ones[] = {
		0xFF,0xFF,0xFF,0xFF,
		0xFF,0xFF,0xFF,0xFF,
		0xFF,0xFF,0xFF,0xFF
	};
	RTP_packetheader_t p = *(RTP_packetheader_t *)&ones;

	printf("%s: ", __PRETTY_FUNCTION__);

	RTP_SET_TS(p, 0xDEADBABE);
	assert(0x3 == RTP_GET_VERSION(p));
	assert(0x1 == RTP_GET_P(p));
	assert(0x1 == RTP_GET_X(p));
	assert(0x0F == RTP_GET_CC(p));
	assert(0x1 == RTP_GET_M(p));
	assert(0x7F == RTP_GET_PT(p));
	assert(0xFFFF == RTP_GET_SEQ_NUM(p));
	assert(0xDEADBABE == RTP_GET_TS(p));
	assert(0xFFFFFFFF == RTP_GET_SSRC(p));
	printf("PASSED!!\n");
}

void test_set_SSRC_zeros(void)
{
	char zeros[] = {
		0x00,0x00,0x00,0x00,
		0x00,0x00,0x00,0x00,
		0x00,0x00,0x00,0x00
	};
	RTP_packetheader_t p = *(RTP_packetheader_t *)&zeros;

	printf("%s: ", __PRETTY_FUNCTION__);

	RTP_SET_SSRC(p, 0xDEADBABE);
	assert(0 == RTP_GET_VERSION(p));
	assert(0 == RTP_GET_P(p));
	assert(0 == RTP_GET_X(p));
	assert(0 == RTP_GET_CC(p));
	assert(0 == RTP_GET_M(p));
	assert(0 == RTP_GET_PT(p));
	assert(0 == RTP_GET_SEQ_NUM(p));
	assert(0 == RTP_GET_TS(p));
	assert(0xDEADBABE == RTP_GET_SSRC(p));
	printf("PASSED!!\n");
}

void test_set_SSRC_ones(void)
{
	char ones[] = {
		0xFF,0xFF,0xFF,0xFF,
		0xFF,0xFF,0xFF,0xFF,
		0xFF,0xFF,0xFF,0xFF
	};
	RTP_packetheader_t p = *(RTP_packetheader_t *)&ones;

	printf("%s: ", __PRETTY_FUNCTION__);

	RTP_SET_SSRC(p, 0xDEADBABE);
	assert(0x3 == RTP_GET_VERSION(p));
	assert(0x1 == RTP_GET_P(p));
	assert(0x1 == RTP_GET_X(p));
	assert(0x0F == RTP_GET_CC(p));
	assert(0x1 == RTP_GET_M(p));
	assert(0x7F == RTP_GET_PT(p));
	assert(0xFFFF == RTP_GET_SEQ_NUM(p));
	assert(0xFFFFFFFF == RTP_GET_TS(p));
	assert(0xDEADBABE == RTP_GET_SSRC(p));
	printf("PASSED!!\n");
}



int main(int argc, char const *argv[])
{
	/* suppress warnings */
	(void)argc;
	(void)argv;

	test_packet_5();
	test_packet_13();
	test_set_version_zeros();
	test_set_version_ones();
	test_set_P_zeros();
	test_set_P_ones();
	test_set_X_zeros();
	test_set_X_ones();
	test_set_CC_zeros();
	test_set_CC_ones();
	test_set_M_zeros();
	test_set_M_ones();
	test_set_PT_zeros();
	test_set_PT_ones();
	test_set_seq_num_zeros();
	test_set_seq_num_ones();
	test_set_ts_zeros();
	test_set_ts_ones();
	test_set_SSRC_zeros();
	test_set_SSRC_ones();
	/* if it didn't crash... */
	printf("ALL TESTS PASSED!\n");
	return 0;
}