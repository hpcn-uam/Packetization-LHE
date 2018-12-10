/**
 * @file
 *
 * Program to add first_block_id to all LHE_headers in a file
 */

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <inttypes.h>

#if VERSION_AVAILABLE
#include "version.h"
#endif

#include "utils.h"
#include "LHE_pack.h"
#include "LHE_headers.h"
#include "LHE_reader.h"

/**
 * Global variables
 */
const char *in = NULL;
const char *out = NULL;
size_t initial_buffer_size = 1500;
FILE *fout = NULL;

void
usage(const char *prog_name, bool print_try)
{
	printf("Usage: %s IN OUT\n", prog_name);
	printf("  or:  %s [OPTIONS]\n", prog_name);

	if (print_try) {
		printf("Try \'%s --help\' for more information.\n", prog_name);
	}
}

void
help(const char *prog_name)
{
	usage(prog_name, false);
	printf("\n");
	printf("Adds the first_block_id field to all LHE packets in a file.\n");
	printf("\n");
	printf("OPTIONS:\n");
	printf("    --help, -h         Display this help.\n");
	printf("    --in,  -f IN       input file.\n");
	printf("    --out, -o OUT      ouput file.\n");
	printf("    --buffer, -b SIZE  set initial read buffer size in bytes. Default %zu.\n", initial_buffer_size);
#if VERSION_AVAILABLE
	printf("\n");
	printf("Compiled from commit %s on %s.\n", COMMIT, BUILD_TIMESTAMP);
#endif
}

/**
 * Check there is another argument after option, exit otherwise.
 * Substracts 1 from argc.
 *
 * @param pargc  pointer to argc.
 * @param option string starting with "--" or "-".
 * @param prog_name
 */
void check_value_after_option(int *pargc, const char *option, const char *prog_name)
{
	(*pargc)--;

	if (*pargc <= 0) {
		fprintf(stderr, "Missing mandatory argument after \"%s\"\n", option);
		usage(prog_name, true);
		exit(-1);
	}
}

int
parse_args(int argc, const char *argv[])
{
	const char *prog_name = argv[0];

	while (*++argv != NULL) {
		argc--;

		// Displays help
		if (streq(*argv, "--help") || streq(*argv, "-h")) {
			help(prog_name);
			return 1;

		// Sets input file
		} else if (streq(*argv, "--in") || streq(*argv, "-f")) {
			check_value_after_option(&argc, *argv, prog_name);
			in = *++argv;

		// Sets output file
		} else if (streq(*argv, "--out") || streq(*argv, "-o")) {
			check_value_after_option(&argc, *argv, prog_name);
			out = *++argv;

		// Sets buffer size
		} else if (streq(*argv, "--buffer") || streq(*argv, "-b")) {
			check_value_after_option(&argc, *argv, prog_name);
			initial_buffer_size = strtol(*++argv, NULL, 10);

		/* if the first argument doesn't start with --, assume it's
		 * a path to input file
		 */
		} else if (NULL == in && **argv != '-' && (*argv)[1] != '-') {
			in = *argv;

		/* if the second argument doesn't start with --, assume it's
		 * a path to output file
		 */
		} else if (NULL == out && **argv != '-' && (*argv)[1] != '-') {
			out = *argv;

		} else {
			fprintf(stderr, "Unrecognized parameter: \"%s\".\n", *argv);
			usage(prog_name, true);
			return -1;
		}
	}

	if (NULL == in || NULL == out) {
		fprintf(stderr, "Missing mandatory argument\n");
		usage(prog_name, true);
		return -1;
	}

	return 0;
}

int write_block(const char *ptr, uint16_t id)
{
	LHE_blockheader_t *hdr = (LHE_blockheader_t *)ptr;
	size_t bytes = sizeof(*hdr) + LHE_BLOCKHEADER_GET_LEN(*hdr);

	assert(NULL != ptr);

	(void)id;
	debug_fprintf(stdout, "Writing block %"PRIu16" to %s of %zu bytes", id, out, bytes);

	return fwrite(ptr, 1, bytes, fout);
}

int write_header(const LHE_packetheader_t *hdr)
{
	debug_fprintf(stdout, "Writing header to %s", out);

	assert(NULL != hdr);

	return fwrite(hdr, 1, sizeof(*hdr), fout);
}

int main(int argc, char const *argv[])
{
	int retval;

	if (parse_args(argc, argv)) {
		exit(-1);
	}

	fout = fopen(out, "w");

	if (NULL == fout) {
		perror("fopen");
		exit(-1);
	}

	retval = LHE_reader_main(in, initial_buffer_size,
		(header_processor)write_header, (block_processor)write_block);
	fclose(fout);
	return retval;
}