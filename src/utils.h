/**
 * @file
 * @author: Eduardo Miravalls Sierra
 * @author: David Muelas Recuenco
 * @author: Angel Lopez
 */


#ifndef _UTILS_H_
#define _UTILS_H_

#ifdef  __cplusplus
extern "C" {
#endif


#include <stdio.h>
#include <time.h>
#include <string.h>
#include <stdbool.h>
#include <stdlib.h>
#include <inttypes.h>
#include <errno.h>


#define MAX_DGRAM_SIZE ((uint32_t)((1<<16)-1))

/**
 * @brief compare 2 integer types (doesn't matter their sizes).
 *
 * @details <b><i>may</i> not be portable</b>: uses the fact that GCC uses
 * 1 for true and 0 for false.
 *
 * Note that both integers have to be signed or unsigned
 * in order to work as expected. Comparing integers of different sign
 * could result in unpredictable results.
 *
 * If the integers are of different size, the compiler will promote the
 * smallest type to the largest type.
 *
 * @param i1 integer (of any size).
 * @param i2 integer (of any size).
 *
 * @return < 0 if i1 < i2.
 * @return   0 if i1 == i2.
 * @return > 0 if i1 < i2.
 */
#define int_cmp(i1, i2) (((i1) > (i2)) - ((i1) < (i2)))

/**
 * Modified version of the original macro, to work with struct timespec.
 *
 * @param a pointer to a struct timespec.
 * @param b pointer to a struct timespec.
 * @param CMP any valid comparison operator.
 *
 * @obs may not work as expected with <= and >=.
 *
 * @return the comparison value.
 */
#define timespeccmp(a, b, CMP)                                               \
 (((a)->tv_sec == (b)->tv_sec) ?                                             \
  ((a)->tv_nsec CMP (b)->tv_nsec) :                                          \
  ((a)->tv_sec CMP (b)->tv_sec))

#define NSEC_IN_SEC 1000000000

//diference between two timespecs
 void timespec_diff(struct timespec *start, struct timespec *stop,
                   struct timespec *result);

//calculates time to wait between frames
 void timespec_timeleft(struct timespec *shouldve, struct timespec *start,
                   struct timespec *stop, struct timespec *wait);

 void timespec_add(struct timespec *t1, struct timespec *t2,struct timespec *result);

/**
 * @brief check if both strings are equal.
 *
 * @param s1 a C string.
 * @param s2 another C string.
 *
 * @return != 0 if they are equal.
 * @return 0 if they aren't equal.
 */
#define streq(s1, s2) (0 == strcmp(s1, s2))

/**
 * @brief check if str starts with a given prefix
 *
 * @param str nul terminated string.
 * @param prefix nul terminated string or at least lnger than prefix.
 *
 * @return true or false
 */
int startswith(const char *str, const char *prefix);

#include "LHE_headers.h"
#include "RTP.h"

void printLHEHeader(LHE_packetheader_t ph);
void printRTPHeader(RTP_packetheader_t rtp);


#define min(a,b) (((a)<(b)) ? (a) : (b))
#define max(a,b) (((a)>(b)) ? (a) : (b))

/**
 * @brief print a message to stderr and exit the program
 *
 * @param msg
 */
void die(const char *msg);

/**
 * If condition is != 0, print a message to stderr and exit with status -1.
 *
 * @param condition integer or expression that evaluates to integer.
 * @param msg       C string with message to be printed on error.
 */
#define die_if_condition(condition, msg) do{if (condition) { die(msg); }}while(0)

/**
 * If condition is != 0, call perror and exit with status -1.
 *
 * @param condition integer or expression that evaluates to integer.
 * @param s         C string with message to be printed on error.
 */
#define perror_if_condition(condition, s) do{if (condition) { perror(s); exit(-1); }}while(0)

#if DEBUG
	#define debug_fprintf(f, fmt, ...) fprintf(f, "[ DEBUG ] (%s:%d) " fmt "\n", __FILE__, __LINE__,  ##__VA_ARGS__)
#else
	#define debug_fprintf(f, fmt, ...) (void)0
#endif

#include <sys/stat.h>

/**
 * Returns the size in bytes of a file.
 *
 * @param  filename path to a file.
 *
 * @return          bytes, or -1 if error.
 */
off_t get_file_size(const char *filename);


#ifdef  __cplusplus
}
#endif

#endif /* _UTILS_H_ */
