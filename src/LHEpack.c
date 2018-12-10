/**
 * @file
 * @author: Eduardo Miravalls Sierra
 * @author: David Muelas Recuenco
 * @author: Angel Lopez
 */

#include "block_mover_lib.h"
#include "Buffer.h"
#include "LHE_headers.h"
#include "LHE_reader.h"
#include "RTP.h"
#include "RTSP/rtsp_lhe.h"
#include "utils.h"

#include <dirent.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/dir.h>
#include <time.h>
#include <unistd.h>

#if VERSION_AVAILABLE
#include "version.h"
#endif

/**
 * Global variables
 */

#define STEP 3000 //for 30 FPS

const char *f = NULL;
const char *d = NULL;
const char *p = NULL;
size_t initial_buffer_size = MAX_DGRAM_SIZE;
FILE *fout = NULL;
bool memory = false;
bool sender = false;
bool receiver = false;
bool receiver2 = false;
bool receiver3 = false;
bool rtsp_server = false;
bool rtsp_client = false;
struct sockaddr_in rtp_si_me = {0}; // initialise to 0
struct sockaddr_in rtp_si_remote = {0}; // initialise to 0
char * remote_ip = NULL;
uint16_t port = 0;
/*	Port 0 trick:
		"On both Windows and Linux, if you bind a socket to port 0,
		the kernel will assign it a free port number somewhere above 1024."
	--> default ports = 0
*/

size_t max_chunk_size = 1480 - sizeof(LHE_packetheader_t) - sizeof(RTP_packetheader_t); //for SHORTEST IP header possible

lhe_parameters_s system_settings;

static
volatile bool stop = false;

static
void sigint_handler(int signo)
{
	(void)signo; // ignore warning

	stop = true;
	fprintf(stderr, "Received SIGINT!\n");
}

void
usage(const char *prog_name, bool print_try)
{
	printf("Usage: %s [OPTIONS]\n", prog_name);

	if (print_try) {
		printf("Try \'%s --help\' for more information.\n", prog_name);
	}
}

void
help(const char *prog_name)
{
	usage(prog_name, false);
	printf("\n");
	printf("OPTIONS:\n");
	printf("    --help, -h\n\tDisplay this help.\n\n");
	printf("    --file, -f FILE\n\t Input/output file.\n\n");
	printf("    --directory, -d DIRECTORY\n\t Specifies the directory from/where files will be read/writen (one file per frame).\n\n");
	printf("    --pipe, -pp PIPE\n\t Specifies the named pipe where received frames will be writen.\n\n");
	printf("    --memory, -m\n\t Gets LHE data from memory(HW) instead of .lhe files.\n\n");
	printf("    --buffer, -b SIZE\n\tSet initial read buffer size in bytes. Default %zu.\n\n", initial_buffer_size);
	printf("    --port, -p PORT\n\tMandatory argument for receiver and sender modes.\n\n");
	printf("    --sender, -s IPv4\n\tRun in sender mode. IPv4 address in dotted-decimal format so sender can send the data.\n\n");
	printf("    --receiver, -r [IPv4]\n\tRun in receiver mode (all frames to a single file). IPv4 is an optional parameter that the receiver can use to only accept traffic from that address.\n\n");
	printf("    --receiver2, -r2 [IPv4]\n\tRun in receiver2 mode (each frame in a file). IPv4 is an optional parameter that the receiver2 can use to only listen traffic from that address.\n\n");
	printf("    --receiver3, -r3 [IPv4]\n\tRun in receiver3 mode (all frames to a FIFO pipe). IPv4 is an optional parameter that the receiver2 can use to only listen traffic from that address.\n\n");	
	printf("    --rtsp-server, -rs\n\tStarts RTSP server.\n\n");
	printf("    --rtsp-client, -rc [IPv4]\n\tStarts RTSP client. Calls server @ IPv4. Specify output directory with -d option.\n\n");
	printf("    --max-chunk-size, -c BYTES\n\tMaximum number of bytes a packet should have. Default: %zu.\n\n", max_chunk_size);
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

int filter_files(const struct dirent *entry){ //filter for scandir
  //filters out everything but files
  if(entry->d_type == DT_REG || entry->d_type == DT_UNKNOWN && strcmp(entry->d_name,".") == 1 && strcmp(entry->d_name,"..") == 1) 
    return 1;
  return 0;
}

int parse_args(int argc, const char *argv[])
{
	const char *prog_name = argv[0];
	uint32_t other_ip;
	int retval;

	if (argc < 2) {
		usage(prog_name, true);
		exit(0);
	}

	while (*++argv != NULL) {
		argc--;

		// Displays help
		if (streq(*argv, "--help") || streq(*argv, "-h")) {
			help(prog_name);
			return 1;

		// Sets input/output file
		} else if (streq(*argv, "--file") || streq(*argv, "-f")) {
			check_value_after_option(&argc, *argv, prog_name);
			f = *++argv;

		// Sets input directory
		} else if (streq(*argv, "--directory") || streq(*argv, "-d")) {
			check_value_after_option(&argc, *argv, prog_name);
			d = *++argv;

		// Sets output pipe
		} else if (streq(*argv, "--pipe") || streq(*argv, "-pp")) {
			check_value_after_option(&argc, *argv, prog_name);
			p = *++argv;

		// Sets memory as input
		} else if (streq(*argv, "--memory") || streq(*argv, "-m")) {
			memory = true;

		// Sets sender mode
		} else if (streq(*argv, "--sender") || streq(*argv, "-s")) {
			sender = true;
			check_value_after_option(&argc, *argv, prog_name);
			remote_ip = *++argv;
			// Check format
			retval = inet_pton(AF_INET, remote_ip, &other_ip);
			die_if_condition(0 == retval, "Supplied IP address has invalid format");

		// Sets receiver mode
		} else if (streq(*argv, "--receiver") || streq(*argv, "-r")) {
			receiver = true;
			// check if optional IPv4 value is present
			if (argc > 1 && '-' != *(argv[1])) {
				remote_ip = *++argv;
				argc--;
				// Check format
				retval = inet_pton(AF_INET, remote_ip, &other_ip);
				die_if_condition(0 == retval, "Supplied IP address has invalid format");
			}

		// Sets receiver2 mode 
		} else if (streq(*argv, "--receiver2") || streq(*argv, "-r2")) {
			receiver2 = true;
			// check if optional IPv4 value is present
			if (argc > 1 && '-' != *(argv[1])) {
				remote_ip = *++argv;
				argc--;
				// Check format
				retval = inet_pton(AF_INET, remote_ip, &other_ip);
				die_if_condition(0 == retval, "Supplied IP address has invalid format");
			}

		// Sets receiver3 mode 
		} else if (streq(*argv, "--receiver3") || streq(*argv, "-r3")) {
			receiver3 = true;
			// check if optional IPv4 value is present
			if (argc > 1 && '-' != *(argv[1])) {
				remote_ip = *++argv;
				argc--;
				// Check format
				retval = inet_pton(AF_INET, remote_ip, &other_ip);
				die_if_condition(0 == retval, "Supplied IP address has invalid format");
			}
			
		// Sets port where the receiver is listening
		} else if (streq(*argv, "--port") || streq(*argv, "-p")) {
			check_value_after_option(&argc, *argv, prog_name);
			port = strtol(*++argv, NULL, 10);

		// Sets buffer size
		} else if (streq(*argv, "--buffer") || streq(*argv, "-b")) {
			check_value_after_option(&argc, *argv, prog_name);
			initial_buffer_size = strtol(*++argv, NULL, 10);

		// Sets packet size
		} else if (streq(*argv, "--max-chunk-size") || streq(*argv, "-c")) {
			check_value_after_option(&argc, *argv, prog_name);
			max_chunk_size = strtol(*++argv, NULL, 10);

		// Starts RTSP server 
		} else if (streq(*argv, "--rtsp-server") || streq(*argv, "-rs")) {
			rtsp_server = true;

		// Starts RTSP client 
		} else if (streq(*argv, "--rtsp-client") || streq(*argv, "-rc")) {
			//Get RTSP server IP
			check_value_after_option(&argc, *argv, prog_name);
			remote_ip = *++argv;
			// Check format
			retval = inet_pton(AF_INET, remote_ip, &other_ip);
			die_if_condition(0 == retval, "Supplied IP address has invalid format");

			rtsp_client = true;

		} else {
			fprintf(stderr, "Unrecognized parameter: \"%s\".\n", *argv);
			usage(prog_name, true);
			return -1;
		}
	}

	return 0;
}

int get_udp_sender_socket(const char *remote_ip, uint16_t remote_port, uint16_t * local_port){
//gets an available port for the sender and gets the socket ready
	
	socklen_t len_me = sizeof(rtp_si_me);
	//socklen_t len_remote = sizeof(rtp_si_remote);
	int s, retval;
	
	// Create a UDP socket
	s = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
	perror_if_condition(-1 == s, "socket");
	rtp_si_me.sin_family = AF_INET;
	rtp_si_me.sin_port = htons(*local_port);
	rtp_si_me.sin_addr.s_addr = htonl(INADDR_ANY);

	// Allow to reuse port. See C99 compound literal
	int aux_int = {1};
	retval = setsockopt(s, SOL_SOCKET, SO_REUSEADDR, &aux_int, sizeof(int));
	perror_if_condition(-1 == retval, "setsockopt SO_REUSEADDR");

	//bind before connect: set the local port
	retval = bind(s, (struct sockaddr*)&rtp_si_me, sizeof(rtp_si_me));
	perror_if_condition(-1 == retval, "bind");

	if(*local_port == 0){ //if port == 0, an available port will be asigned by the OS --> get that port #
		if(getsockname(s, (struct sockaddr *)&rtp_si_me, &len_me) == -1){
    		perror("getsockname");
    	}
		*local_port = ntohs(rtp_si_me.sin_port);
	}

	if(remote_ip != NULL){
		//check remote ip
		rtp_si_remote.sin_family = AF_INET;
		rtp_si_remote.sin_port = htons(remote_port);
		retval = inet_pton(AF_INET, remote_ip, &rtp_si_remote.sin_addr);
		// inet_pton reports success with 1
		die_if_condition(0 == retval, "Supplied IP address has invalid format");
		// set the address to which datagrams are sent by default
		retval = connect(s, &rtp_si_remote, sizeof(rtp_si_remote));
		perror_if_condition(-1 == retval, "connect");
	}

	return s;
}

int get_udp_receiver_socket(const char *remote_ip, uint16_t * local_port){
	//same as get_udp_sender_socket, but remote_port is not known yet (waiting for RTSP SETUP reply)

	return get_udp_sender_socket(remote_ip, 0, local_port);
}

int LHE_receive_file(const char * file, int socket, size_t initial_buffer_size, uint32_t initial_SSRC){
	uint64_t retval;

	retval = LHE_receiver(socket, rtp_si_remote, file, initial_buffer_size); 

	return retval;
}

int LHE_receive_dir(int socket, size_t initial_buffer_size, uint32_t initial_SSRC){
	uint64_t retval;

	retval = LHE_receiver2(socket, rtp_si_remote, d, initial_buffer_size); 

	return retval;
}

int LHE_receive_pipe(int socket, size_t initial_buffer_size, uint32_t initial_SSRC){
	uint64_t retval;

	retval = LHE_receiver3(socket, rtp_si_remote, p, initial_buffer_size); 

	return retval;
}

int LHE_send_file(const char * file, int socket, size_t initial_buffer_size, uint16_t initial_seq_num, uint32_t initial_ts, uint32_t initial_SSRC)
{
	//TO DO: does this still work??

	uint64_t retval;
	
	die_if_condition(file == NULL, "Input file not specified");

	Buffer_t bf = Buffer_new(initial_buffer_size);
	die_if_condition(NULL == bf, "Buffer allocation failed");
	retval = LHE_read_into_buffer(file, bf);
	//die_if_condition(retval < 0, "Error loading lhe file");

	retval = LHE_sender(socket, initial_buffer_size, initial_seq_num, initial_ts, 
				initial_SSRC, (uint8_t *)bf->buf, retval, max_chunk_size);

	Buffer_free(bf);
	close(socket);
		
	return retval;
}

int LHE_send_dir(const char * dir, int socket, size_t initial_buffer_size, uint16_t initial_seq_num, uint32_t initial_ts, uint32_t initial_SSRC)
{
	int n = 0, i = 0;
	struct dirent **namelist;
	char filename[1000]; //!
	int frames_sent = 0;
	clockid_t clk_id = CLOCK_MONOTONIC;
	uint64_t retval, packets_sent = 0;

	struct timespec start, now, wait, unit, shouldve;
	
	start.tv_sec = 0;
	start.tv_nsec = 0; 
	now.tv_sec = 0;
	now.tv_nsec = 0; 
	unit.tv_sec = 0;
	unit.tv_nsec = 33333333; //hardcoded for 30 FPS
	shouldve.tv_sec = 0;
	shouldve.tv_nsec = 0; 

	die_if_condition(dir == NULL, "Input directory not specified");

	n = scandir(dir, &namelist, filter_files, alphasort); //n = # of files
	die_if_condition(n < 0, "Directory empty");
			
	//printf("Sending %d frames...\n", n); //debug

	//transmission start time
	clock_gettime(clk_id, &start);
		
	while (i < n && !stop){
		if (stop){
			break;
		}

		i++;

		sprintf(filename, "%s/%s", dir, namelist[i-1]->d_name);
		f = filename;
		//printf("file: %s\n", filename); //debug

		Buffer_t bf = Buffer_new(initial_buffer_size);
		die_if_condition(NULL == bf, "Buffer allocation failed");
	
		retval = LHE_read_into_buffer(f, bf);
		//die_if_condition(retval < 0, "Error loading lhe file");
	
		//wait for FPS
		if(frames_sent > 0){
			clock_gettime(clk_id, &now);
			timespec_timeleft(&shouldve, &start, &now, &wait);
			nanosleep(&wait, NULL); //WARNING: implement error control!					
		}
		
		retval = LHE_sender(socket, initial_buffer_size, initial_seq_num, initial_ts,
					initial_SSRC, (uint8_t *)bf->buf, retval, max_chunk_size);

		if(retval == 0){
			stop = true;
		}

		Buffer_free(bf);
	
		//update RTP header values
		initial_seq_num += retval; //retval is # of packets sent
		initial_ts += STEP;
		packets_sent += retval;
	
		timespec_add(&shouldve, &unit, &shouldve);
		//printf("shouldve:  %lld s %.ld ms \n", (long long)shouldve.tv_sec, shouldve.tv_nsec/1000000); //debug
		frames_sent++;

		free(namelist[i-1]);
	}
	free(namelist);
	close(socket);


	//printf("Sent %d frames!\n", frames_sent); //debug
	fprintf(stdout,"%"PRIu64"", packets_sent);

	if (stop){
		stop = false;
	}

	return packets_sent;
}

int LHE_send_mem(int socket, size_t initial_buffer_size, uint16_t initial_seq_num, uint32_t initial_ts, uint32_t initial_SSRC)
{
	uint64_t retval;
	
	retval = LHE_send_from_memory(socket, initial_buffer_size, initial_seq_num, initial_ts, initial_SSRC, max_chunk_size);			
			
	close(socket);

	return retval;
}

int main(int argc, const char *argv[])
{
	srand(time(NULL));
	uint16_t initial_seq_num = rand();
	uint32_t initial_ts = 0;
	uint32_t initial_SSRC = rand();
	uint64_t retval, packets_sent = 0;
	int socket;

	/* Set handler for SIGINT */
	struct sigaction act;

	act.sa_handler = sigint_handler;
	sigemptyset (&act.sa_mask);
	act.sa_flags = 0;
	sigaction(SIGINT, &act, NULL);

	debug_fprintf(stderr, "SIGINT captured");

	if (parse_args(argc, argv)) {
		exit(-1);
	}

	if(rtsp_server){
		if(rtsp_client || receiver || receiver2 || receiver3 || sender){
			die("Only one mode can be selected!!");
		}

	    /* Call open device function. It will initialize a pointer to data*/
	    if (open_device(OPEN_LHE_PARAMETERS_MASK | OPEN_IMAGE_BUFFER_MASK) != 0) {
	        perror("Error opening device");
	        return -1;
	    }

		retval = rtsp_server_start(port);

		close_device(); /* Free the pointers */

	} else if (rtsp_client){
		if(rtsp_server || receiver || receiver2 || receiver3 || sender){
			die("Only one mode can be selected!!");
		}

		die_if_condition(NULL == p, "Please specify the pipe");

		retval = rtsp_client_start(port, remote_ip);

	} else if (receiver){
		if(rtsp_server || rtsp_client || receiver2 || receiver3 || sender){
			die("Only one mode can be selected!!");
		}

		die_if_condition(NULL == f, "Please specify the file");

		debug_fprintf(stderr, "Starting receiver");

		socket = get_udp_receiver_socket(remote_ip, &port);
		retval = LHE_receiver(socket, rtp_si_remote, f, initial_buffer_size);

		return retval;

	} else if (receiver2){
		if(rtsp_server || rtsp_client || receiver || receiver3 || sender){
			die("Only one mode can be selected!!");
		}

		die_if_condition(NULL == d, "Please specify the directory");

		debug_fprintf(stderr, "Starting receiver2");

		socket = get_udp_receiver_socket(remote_ip, &port);
		retval = LHE_receiver2(socket, rtp_si_remote, d, initial_buffer_size); 
		//printf("%"PRIu64"", retval); //packets received
		return retval;

	} else if (receiver3){
		if(rtsp_server || rtsp_client || receiver || receiver2 || sender){
			die("Only one mode can be selected!!");
		}

		die_if_condition(NULL == p, "Please specify the pipe");

		debug_fprintf(stderr, "Starting receiver3");

		socket = get_udp_receiver_socket(remote_ip, &port);
		retval = LHE_receiver3(socket, rtp_si_remote, p, initial_buffer_size); 
		//printf("%"PRIu64"", retval); //packets received
		return retval;

	} else if (sender){
		if(rtsp_server || rtsp_client || receiver || receiver2 || receiver3){
			die("Only one mode can be selected!!");
		}

		if(memory){ //send from camera
			die_if_condition((NULL != d || NULL != f), "Select either an input file/directory or memory input, not both!");

			uint16_t local_port = 0;

		    /* Call open device function. It will initialize a pointer to data*/
		    if (open_device(OPEN_LHE_PARAMETERS_MASK | OPEN_IMAGE_BUFFER_MASK) != 0) {
		        perror("Error opening device");
		        return -1;
		    }

			socket = get_udp_sender_socket(remote_ip, port, &local_port);
			
			retval = LHE_send_mem(socket, initial_buffer_size, initial_seq_num, initial_ts, initial_SSRC);
			close_device(); /* Free the pointers */
			return retval;

		}else if(NULL != f){ //sends one file with all the frames (stream)
			//TO DO: does this still work??

			die_if_condition(NULL != d, "Select either an input file or an input directory, not both!");

			uint16_t local_port = 0;

			socket = get_udp_sender_socket(remote_ip, port, &local_port);
			
			retval = LHE_send_file(f, socket, initial_buffer_size, initial_seq_num, initial_ts, initial_SSRC);
			
			return retval;

		}else if(NULL != d){ //sends all the files in a directory, where each file is a frame 
			uint16_t local_port = 0;

			socket = get_udp_sender_socket(remote_ip, port, &local_port);
			
			packets_sent = LHE_send_dir(d, socket, initial_buffer_size, initial_seq_num, initial_ts, initial_SSRC);

			return packets_sent;

		}else{
			die("No input file nor input directory specified!");
		}

	} else {
		debug_fprintf(stderr, "Start reading file");
		return LHE_reader_main(f, initial_buffer_size, NULL, NULL, NULL);
	}
}

