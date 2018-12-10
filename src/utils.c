/**
 * @file
 * @author: Eduardo Miravalls Sierra
 * @author: David Muelas Recuenco
 * @author: Angel Lopez
 */


#include <stdio.h>
#include <stdlib.h>
#include <inttypes.h>
#include <errno.h>

#include "LHE_headers.h"
#include "utils.h"

char buffer[2048];


/* extracted from procesaConexiones's utils module */
// Convierte un struct timespec en un float
inline long double timespec2float(const struct timespec *tv);
inline void print_timespec(FILE *f, const struct timespec *ts);

/* http://stackoverflow.com/a/7618231 */
inline const char *bool2str(bool b);


/**
 * @brief check if str starts with a given prefix
 *
 * @param str nul terminated string.
 * @param prefix nul terminated string or at least lnger than prefix.
 *
 * @return true or false
 */
int startswith(const char *str, const char *prefix)
{
    return strncmp(prefix, str, strlen(prefix)) == 0;
}

/* extracted from procesaConexiones's utils module */
// Convierte un struct timespec en un float
inline long double timespec2float(const struct timespec *tv) {
    if (!tv) return -1;
    long double ret = tv->tv_sec+tv->tv_nsec/1000000000.0L;
    return ret;
}

inline void print_timespec(FILE *f, const struct timespec *ts)
{
	long double ts_float = timespec2float(ts);

	/* conexionesTCP.c:2297 */
	if (ts_float > 1e-9) {
		fprintf(f, "%.9Lf", ts_float);

	} else {
		fprintf(f, "0");
	}
}

void timespec_diff(struct timespec *start, struct timespec *stop,
                   struct timespec *result)
{
    if ((stop->tv_nsec - start->tv_nsec) < 0) {
        result->tv_sec = stop->tv_sec - start->tv_sec - 1;
        result->tv_nsec = stop->tv_nsec - start->tv_nsec + 1000000000;
    } else {
        result->tv_sec = stop->tv_sec - start->tv_sec;
        result->tv_nsec = stop->tv_nsec - start->tv_nsec;
    }
    if(result->tv_sec < 0){
    	result->tv_sec = 0;
    }
    if(result->tv_nsec < 0){
    	result->tv_nsec = 0;
    }

    return;
}

void timespec_timeleft(struct timespec *shouldve, struct timespec *start,
                   struct timespec *stop, struct timespec *wait)
{
	struct timespec has_passed;
	//time that has passed = stop - start
	timespec_diff(start, stop, &has_passed);
	//wait = time that should've passed - time that has passed
	timespec_diff(&has_passed, shouldve, wait);
	/*printf("%lld s %.ld ms should've passed\n", (long long)shouldve->tv_sec, shouldve->tv_nsec/1000000); //debug
	printf("%lld s %.ld ms have passed\n", (long long)has_passed.tv_sec, has_passed.tv_nsec/1000000); //debug
	printf("Waiting for  %lld s %.ld ms ...\n", (long long)wait->tv_sec, wait->tv_nsec/1000000); //debug*/

	return;
}

#define BILLION 1000000000
void timespec_add(struct timespec *t1, struct timespec *t2,struct timespec *result)
{
    result->tv_sec = t2->tv_sec + t1->tv_sec;
    result->tv_nsec = t2->tv_nsec + t1->tv_nsec;
    if (result->tv_nsec >= BILLION) {
        result->tv_nsec -= BILLION;
        result->tv_sec++;
    }

	return;
}


/* http://stackoverflow.com/a/7618231 */
inline const char *bool2str(bool b)
{
	return "false\0true" + sizeof("false") * (!!b);
}


void printLHEHeader(LHE_packetheader_t ph)
{

	fprintf(stdout, "/**************************************************/\n");
	fprintf(stdout, "/                    LHE HEADER                    /\n");
	fprintf(stdout, "/**************************************************/\n");
	fprintf(stdout, "Version:              \t%"PRIu8"\n",  LHE_PACKETHEADER_GET_VERSION(ph));
	fprintf(stdout, "A:                    \t%"PRIx8"\n",  LHE_PACKETHEADER_GET_A(ph));
	fprintf(stdout, "D:                    \t%"PRIx8"\n",  LHE_PACKETHEADER_GET_D(ph));
	fprintf(stdout, "Profile:              \t%"PRIx8"\n",  LHE_PACKETHEADER_GET_PROFILE(ph));
	fprintf(stdout, "Reserved bits:        \t%"PRIx8"\n",  LHE_PACKETHEADER_GET_RESERVED(ph));
	fprintf(stdout, "N Blocks:             \t%"PRIu8"\n",  LHE_PACKETHEADER_GET_NBLOCKS(ph));
	fprintf(stdout, "Block height:         \t%"PRIu16"\n", LHE_PACKETHEADER_GET_BLOCK_H(ph));
	fprintf(stdout, "Block width:          \t%"PRIu16"\n", LHE_PACKETHEADER_GET_BLOCK_W(ph));
	fprintf(stdout, "Image height (blocks):\t%"PRIu16"\n", LHE_PACKETHEADER_GET_IM_H(ph));
	fprintf(stdout, "Image width (blocks): \t%"PRIu16"\n", LHE_PACKETHEADER_GET_IM_W(ph));
	fprintf(stdout, "1st block ID:         \t%"PRIu16"\n", LHE_PACKETHEADER_GET_FB_ID(ph));
	fprintf(stdout, "/**************************************************/\n");

}

void printRTPHeader(RTP_packetheader_t p)
{
	fprintf(stdout, "/**************************************************/\n");
	fprintf(stdout, "/                    RTP HEADER                    /\n");
	fprintf(stdout, "/**************************************************/\n");
	fprintf(stdout, "Version: %"PRIu8"\n", RTP_GET_VERSION(p));
	fprintf(stdout, "P:       %"PRIu8"\n", RTP_GET_P(p));
	fprintf(stdout, "X:       %"PRIu8"\n", RTP_GET_X(p));
	fprintf(stdout, "CC:      %"PRIu8"\n", RTP_GET_CC(p));
	fprintf(stdout, "M:       %"PRIu8"\n", RTP_GET_M(p));
	fprintf(stdout, "PT:      %"PRIu8"\n", RTP_GET_PT(p));
	fprintf(stdout, "SEQ NUM: %"PRIu16"\n", RTP_GET_SEQ_NUM(p));
	fprintf(stdout, "TS:      %"PRIu32"\n", RTP_GET_TS(p));
	fprintf(stdout, "SSRC:    %"PRIu32"\n", RTP_GET_SSRC(p));
	fprintf(stdout, "/**************************************************/\n");
}

void die(const char *msg)
{
	fprintf(stderr, "%s\n", msg);
	exit(-1);
}

off_t get_file_size(const char *filename) {
	struct stat st;

	if (stat(filename, &st) == 0) {
		return st.st_size;
	}

	return -1;
}

