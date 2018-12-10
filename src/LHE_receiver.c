/**
 * @file
 * @author: Eduardo Miravalls Sierra
 * @author: David Muelas Recuenco
 * @author: Angel Lopez
 */

#include <arpa/inet.h>
#include <assert.h>
#include <errno.h>
#include <fcntl.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <time.h>
#include <unistd.h>

#include "Buffer.h"
#include "LHE_headers.h"
#include "utils.h"

#define TS_MAX 65535
#define IMAGE_BSIZE_MAX 2880 //for an image of size 1280x720 coded with blocks of 320 pixels

static
volatile bool stop = false;

volatile uint64_t packets_received = 0;
volatile uint64_t blocks_lost = 0, total_blocks, blocks_received = 0;

static
void sigint_handler(int signo)
{
	(void)signo; // ignore warning
	
	//packets received and block loss percentage output in csv style, for testbench
	blocks_lost = total_blocks - blocks_received;
	if(total_blocks == 0){
		total_blocks++; //0 div
	}
	fprintf(stdout, "%"PRIu64", %.3f", packets_received, 100*(blocks_lost/total_blocks));

	//fprintf(stderr, "Received SIGINT!\n"); 
	stop = true;
}

int LHE_receiver(int socket, struct sockaddr_in si_other, const char *out, size_t initial_buffer_size)
{
	int retval;
	socklen_t slen = sizeof(si_other);
	Buffer_t bf;
	FILE *fout;
#if STATIC_BUFFER
	/*
	 * We can allocate a static buffer compiling with CEXTRAFLAGS=-DSTATIC_BUFFER=1
	 * WATCH OUT! All calls MUST HAVE growable=false, maybe move "growable" to
	 * Buffer_new() and keep as internal variable?
	 */
	static char static_buffer[MAX_DGRAM_SIZE];
#endif

	die_if_condition(NULL == out, "File is a mandatory argument for receiver");

	// ensure valid port number
	//die_if_condition(0 == port, "Port must be non zero!");
	/*	Port 0 trick:
			"On both Windows and Linux, if you bind a socket to port 0,
	 		the kernel will assign it a free port number somewhere above 1024."
		--> port 0 atcually is the most ensured port number
	*/


	/* Set handler for SIGINT */
	struct sigaction act;

	act.sa_handler = sigint_handler;
	sigemptyset (&act.sa_mask);
	act.sa_flags = 0;
	sigaction(SIGINT, &act, NULL);

	debug_fprintf(stderr, "SIGINT captured");

	debug_fprintf(stderr, "Allocating receiver resources");

#if !STATIC_BUFFER
	bf = Buffer_new(initial_buffer_size);
	die_if_condition(NULL == bf, "Buffer allocation failed!");
#else
	bf = Buffer_new(0);
	die_if_condition(NULL == bf, "Buffer allocation failed!");
	bf->buf = static_buffer;
	bf->size = sizeof(static_buffer);
#endif

	/* remove pipe in case it already exists */
	unlink(out);
	/* create named pipe */
	retval = mkfifo(out, 0666);
	perror_if_condition(retval < 0, "mkfifo");

	debug_fprintf(stderr, "Pipe %s created", out);

	/* open named pipe for writing
	 * WARNING: pipes may be blocking until there is another process listening!
	 */
	fout = fopen(out, "w");
	perror_if_condition(NULL == fout, "fopen");

	debug_fprintf(stderr, "Pipe %s opened", out);

	debug_fprintf(stderr, "Starting receiver loop");

	// Keep listening for data
	while(!stop) {
		RTP_packetheader_t *rtp;

		Buffer_clear(bf);

		// you can read from a socket
#if STATIC_BUFFER
		retval = bf(Buffer_recvfrom, MAX_DGRAM_SIZE, false, socket, (struct sockaddr *)&si_other, &slen);
#else
		retval = Buffer_recvfrom(bf, MAX_DGRAM_SIZE, true, socket, (struct sockaddr *)&si_other, &slen);
#endif

		// TODO: How do we verify what we got is indeed RTP + LHE data?
			//check payload type
		// TODO: Did we get the data from whom we expected?
#if DEBUG
	LHE_packetheader_t *ph;
	if (-1 != retval) {
		//print details of the client/peer and the data received
		debug_fprintf(stdout, "Received packet from %s:%d of %d bytes", inet_ntoa(si_other.sin_addr), ntohs(si_other.sin_port), retval);

		// Lets see what we got... guess its what we expect
		rtp = (RTP_packetheader_t *)bf->buf;
		ph = (LHE_packetheader_t *)(bf->buf + sizeof(*rtp));

		if ((size_t)retval >= sizeof(*rtp)) {
			printRTPHeader(*rtp);
		}
		if ((size_t)retval >= (sizeof(*ph) + sizeof(*rtp))) {
			printLHEHeader(*ph);
		}
	}
#endif

		// read/recv call may have been interrupted by SIGINT
		if (stop) {
			break;
		}

		if (-1 == retval) {
			perror("read");

		} else {
			// send it down the pipe WITH the RTP header
			//fwrite(bf->buf, 1, bf->used, fout); //WITH
			fwrite(bf->buf + sizeof(*rtp), 1, bf->used - sizeof(*rtp), fout); //WITHOUT
			//fwrite(bf->buf + sizeof(*rtp) + sizeof(*ph), 1, bf->used - sizeof(*rtp) - sizeof(*ph), fout); //WITHOUT RTP && WITHOUT LHE
			packets_received++;
		}
	}

	fprintf(stderr, "Received %"PRIu64" packets\n", packets_received);
	debug_fprintf(stderr, "Freeing receiver resources");

	close(socket);

#if STATIC_BUFFER
	/* dont call free on static buffer */
	bf->buf = NULL;
#endif
	Buffer_free(bf);
	fclose(fout);
	unlink(out); // tell OS to delete named pipe when the other process closes it
	debug_fprintf(stderr, "Receiver out");
	return 0;
}

//writes blocks in bf_w to fout
void squeeze_into_file(uint8_t bf_w[IMAGE_BSIZE_MAX][MAX_BLOCK_LEN], FILE *fout, uint64_t real_nblocks){
	uint64_t i;
	LHE_blockheader_t *bh;
	size_t block_len;

	for(i = 0; i < real_nblocks; i++){
		bh = (LHE_blockheader_t *)bf_w[i];
		block_len = LHE_BLOCKHEADER_GET_LEN(*bh);
		fwrite(bf_w[i], 1, sizeof(LHE_blockheader_t) + block_len, fout); //write payload
	}
}

int LHE_receiver2(int socket, struct sockaddr_in si_other, const char *out, size_t initial_buffer_size)
{
	socklen_t slen = sizeof(si_other);
	int retval, i; 

	uint64_t ts_aux = 0;
	uint32_t ts_current = 0;
	uint32_t ts_prev = 0;
	uint32_t ts_step = 0;

	uint16_t frames_received = 0;
	uint64_t packets_dropped = 0, packets_lost = 0, total_packets = 0;
	uint64_t blocks_dropped = 0;

	uint16_t current_seqnum, previous_seqnum, next_seqnum;
	uint32_t current_session;

	uint8_t *rover;
	uint8_t *rover_w;

	uint8_t blocks_in_paq;

	Buffer_t bf; //reception buffer
	uint8_t bf_w[IMAGE_BSIZE_MAX][MAX_BLOCK_LEN]; //write buffer 

	FILE *fout;

	bool first = true;
	bool startup = true;

	uint8_t LHE_header_file_version = 1;

	RTP_packetheader_t *rtp;
	LHE_packetheader_t *ph;
	LHE_blockheader_t *bh;
	size_t block_len;
	
	char filename_out[1000]; //TO DO!!
	const char *out_path;

#if TIMING
  char timing_filename[1000];
  const char *filename;
  clockid_t clk_id = CLOCK_MONOTONIC;
  struct timespec frame_in, frame_depaq, frame_writen, depaq_time, write_time;
  frame_in.tv_sec = 0;
  frame_in.tv_nsec = 0;
  frame_depaq.tv_sec = 0;
  frame_depaq.tv_nsec = 0;
  frame_writen.tv_sec = 0;
  frame_writen.tv_nsec = 0;
  depaq_time.tv_sec = 0;
  depaq_time.tv_nsec = 0;
  write_time.tv_sec = 0;
  write_time.tv_nsec = 0;

  FILE *flogout; //timing log file

  printf("TIMING!!\n");
  sprintf(timing_filename, "performance_timing/receiver_timing_%ld.log", time (NULL));
  filename = timing_filename;
  flogout = fopen(filename, "w+");
  if(NULL == flogout){
    printf("ERROR: couldn't open receiver_timing.log\n");
    stop = true;
  }else{
  	fprintf(flogout, "depaq_time.sec\tdepaq_time.nsec\twrite_time.sec\twrite_time.nsec\n"); //column names
  }
#endif /*TIMING*/

#if STATIC_BUFFER
	/*
	 * We can allocate a static buffer compiling with CEXTRAFLAGS=-DSTATIC_BUFFER=1
	 * WATCH OUT! All calls MUST HAVE growable=false, maybe move "growable" to
	 * Buffer_new() and keep as internal variable?
	 */
	static char static_buffer[MAX_DGRAM_SIZE];
#endif

	die_if_condition(NULL == out, "Directory is a mandatory argument for receiver2");

	// ensure valid port number
	//die_if_condition(0 == port, "Port must be non zero!");
	/*	Port 0 trick:
			"On both Windows and Linux, if you bind a socket to port 0,
	 		the kernel will assign it a free port number somewhere above 1024."
		--> port 0 atcually is the most ensured port number
	*/

	/* Set handler for SIGINT */
	struct sigaction act;

	act.sa_handler = sigint_handler;
	sigemptyset (&act.sa_mask);
	act.sa_flags = 0;
	sigaction(SIGINT, &act, NULL);

	debug_fprintf(stderr, "SIGINT captured");

	debug_fprintf(stderr, "Allocating receiver resources");


#if !STATIC_BUFFER
	bf = Buffer_new(initial_buffer_size);
	die_if_condition(NULL == bf, "Buffer allocation failed!");
#else
	bf = Buffer_new(0);
	die_if_condition(NULL == bf, "Buffer allocation failed!");
	bf->buf = static_buffer;
	bf->size = sizeof(static_buffer);
#endif
	
	debug_fprintf(stderr, "Starting receiver loop");

	// Keep listening for data
	while(!stop) {
		Buffer_clear(bf);

		// you can read from a socket
#if STATIC_BUFFER
		retval = bf(Buffer_recvfrom, MAX_DGRAM_SIZE, false, socket, (struct sockaddr *)&si_other, &slen);
#else
		retval = Buffer_recvfrom(bf, MAX_DGRAM_SIZE, true, socket, (struct sockaddr *)&si_other, &slen);
#endif
		// read/recv call may have been interrupted by SIGINT
		if (stop && !first) { //if stop is called during the transmission of a file
			//create file
			fout = fopen(out_path, "w+");
			perror_if_condition(NULL == fout, "fopen");
			//printf("Output file: %s\n", out_path);//debug

			//write modified LHE header (in a file, #blocks is 0)
			//print_LHE_header(ph);
			LHE_header_change_version(ph, ph, LHE_header_file_version);
			//print_LHE_header(ph);
			LHE_packetheader_set_nblocks(ph, 0); 
			fwrite(bf->buf + sizeof(*rtp), 1, sizeof(*ph) - sizeof(uint16_t), fout); //write LHE header without "1st block id" field -> UNFIXED FILE!
			//write payload (write buffer content)
			squeeze_into_file(bf_w, fout, LHE_PACKETHEADER_GET_IM_NBLOCKS(*ph));

			fclose(fout); //close file

			frames_received++;
			total_blocks += LHE_PACKETHEADER_GET_IM_NBLOCKS(*ph);

			break;
		}
		if (-1 == retval) {
			perror("read");
		} else {
#if TIMING
        if(first){
        	clock_gettime(clk_id, &frame_in);
        }
#endif /*TIMING*/

			rtp = (RTP_packetheader_t *)bf->buf; //RTP header
			ph = (LHE_packetheader_t *)(bf->buf + sizeof(*rtp)); //LHE header
			
			previous_seqnum = current_seqnum;
			current_seqnum = RTP_GET_SEQ_NUM(*rtp);

			if(startup){
				//init (0000000000.lhe)
				sprintf(filename_out, "%s/%010"PRIu64".lhe", out, ts_aux);
				out_path = filename_out;
				current_session = RTP_GET_SSRC(*rtp);
				next_seqnum = current_seqnum;
				startup = false;
			}
			if(current_session != RTP_GET_SSRC(*rtp)){ //if a new session is starting
				next_seqnum = current_seqnum;
			}

			//determine packet loss
			if(!startup && (current_session == RTP_GET_SSRC(*rtp))){ //if not on startup of receiver or on the start of a new session
				if(next_seqnum < current_seqnum){ //seqnum skips forward -> loss
					packets_lost += current_seqnum - next_seqnum;
					next_seqnum = current_seqnum;
				}else if(next_seqnum > current_seqnum){ //late packet or skip over rollover?
					if((65535 - next_seqnum + current_seqnum) < (next_seqnum - current_seqnum)){ //if the leap is smaller forwards than backwards -> loss 
						packets_lost += 65535 - next_seqnum + current_seqnum +1;
						next_seqnum = current_seqnum;
					}//else -> late packet 
						//+1 packet recovered -1 packet not coming at its time = +/-0 losses
				} 
			}
			next_seqnum++;

			//adress packet loss/out of order arrivals
			if(!first && RTP_GET_TS(*rtp) != ts_current){ 
				if(current_seqnum < previous_seqnum){ //a prior frame
					packets_dropped++; //too late, drop it
					blocks_dropped += LHE_PACKETHEADER_GET_NBLOCKS(*ph);
					continue;  
				} 
				else{ //if(current_seqnum < previous_seqnum){ //the next frame, dispatch current frame as is and get ready
					
					//create file
					fout = fopen(out_path, "w+");
					perror_if_condition(NULL == fout, "fopen");
					//printf("Output file: %s\n", out_path);//debug

					//write modified LHE header (in a file, #blocks is 0)
					//print_LHE_header(ph);
					LHE_header_change_version(ph, ph, LHE_header_file_version);
					//print_LHE_header(ph);
					LHE_packetheader_set_nblocks(ph, 0); 
					fwrite(bf->buf + sizeof(*rtp), 1, sizeof(*ph) - sizeof(uint16_t), fout); //write LHE header without "1st block id" field -> UNFIXED FILE!
					//write payload (write buffer content)
					squeeze_into_file(bf_w, fout, LHE_PACKETHEADER_GET_IM_NBLOCKS(*ph));

					fclose(fout); //close file

					frames_received++;
					total_blocks += LHE_PACKETHEADER_GET_IM_NBLOCKS(*ph);

					first = true; //next packet is first packet of next frame
				} 
			}

			if(ts_step > 0 && ((RTP_GET_TS(*rtp) - ts_current) > 2 * ts_step)){
				//printf("FRAME LOSS! (%d)\n", ((RTP_GET_TS(*rtp) - ts_current)/ts_step)-1); //frame loss OR frame rate has been reduced
			}

			if(first){ //first packet of a frame -> init
				//get TS
				ts_prev = ts_current;
				ts_current = RTP_GET_TS(*rtp); 
				
				//get file name (out)/(RTP timestamp).lhe
				if(ts_current >= ts_prev){
					ts_step = (ts_current - ts_prev);
				}else{
					ts_step = (TS_MAX - ts_prev) + ts_current;
				}
				ts_aux += ts_step;
				sprintf(filename_out, "%s/%010"PRIu64".lhe", out, ts_aux);
				out_path = filename_out;

				//next is not first
				first = false;
			}

			//copy blocks from reception buffer to write buffer
			rover = (uint8_t *)bf->buf + sizeof(*rtp) + sizeof(*ph); //points to first block
			blocks_in_paq = LHE_PACKETHEADER_GET_NBLOCKS(*ph); 

			for(i = 0; i < blocks_in_paq; i++){

				// grab a new block
				bh = (LHE_blockheader_t *)rover;
				block_len = LHE_BLOCKHEADER_GET_LEN(*bh);

				//write buffer position
				rover_w =bf_w[(LHE_PACKETHEADER_GET_FB_ID(*ph) + i)];
				//printf("writing block %d\n", LHE_PACKETHEADER_GET_FB_ID(*ph) + i);//debug

				//delete block from write buffer
				memset(rover_w, 0, MAX_BLOCK_LEN);

				//write block header + block from reception buffer to write buffer
				memcpy(rover_w, rover, sizeof(*bh) + block_len);
				
				//advance read pointer to end of block
				rover += sizeof(*bh) + block_len;
			}

			packets_received++;
			blocks_received += blocks_in_paq;

			if(1 == RTP_GET_M(*rtp)){ //if marker --> last packet of frame
									
#if TIMING
        		clock_gettime(clk_id, &frame_depaq);
#endif /*TIMING*/

				//create file
				fout = fopen(out_path, "w+");
				perror_if_condition(NULL == fout, "fopen");
				//printf("Output file: %s\n", out_path);//debug

				//write modified LHE header (in a file, #blocks is 0)
				LHE_header_change_version(ph, ph, LHE_header_file_version);
				//print_LHE_header(ph);
				LHE_packetheader_set_nblocks(ph, 0);
				fwrite(bf->buf + sizeof(*rtp), 1, sizeof(*ph) - sizeof(uint16_t), fout); //write LHE header without "1st block id" field -> UNFIXED FILE!
				//write payload (write buffer content)
				squeeze_into_file(bf_w, fout, LHE_PACKETHEADER_GET_IM_NBLOCKS(*ph));

				fclose(fout); //close file

#if TIMING
          		clock_gettime(clk_id, &frame_writen);
          		timespec_diff(&frame_in, &frame_depaq, &depaq_time);
                timespec_diff(&frame_depaq, &frame_writen, &write_time);
                fprintf(flogout, "%lld\t%ld\t%lld\t%ld\n", (long long)depaq_time.tv_sec, depaq_time.tv_nsec, (long long)write_time.tv_sec, write_time.tv_nsec);
#endif /*TIMING*/

				frames_received++;
				total_blocks += LHE_PACKETHEADER_GET_IM_NBLOCKS(*ph);

				first = true; //next packet is first packet of next frame
			}
		}
	}
	total_packets = packets_received + packets_lost + packets_dropped;

	blocks_lost = total_blocks - blocks_received;

	if(!stop){
		printf("Something wrong happened (?)\n");
	}
	if(total_blocks == 0){
		total_blocks++; //0 div
	}
	if(total_packets == 0){
		total_packets++; //0 div
	}
	fprintf(stderr, "\nreceived:\t%"PRIu64"\n", packets_received);
	fprintf(stderr, "loss:\t%"PRIu64"/%"PRIu64"\t%.3f%%\n", packets_lost, total_packets, 100*((float)packets_lost/(float)total_packets));
	fprintf(stderr, "block loss: %.3f%%\n", 100*((float)blocks_lost/(float)total_blocks));
	fprintf(stderr, "Frames received:\t%"PRIu16"\n", frames_received);
	/*fprintf(stderr, "Frames received:\t%"PRIu16"\n", frames_received);
	if(frames_received > 0){
		fprintf(stderr, "Packet loss at net level:\t%"PRIu64"/%"PRIu64"\t%.3f%%\n", packets_lost, total_packets, 100*((float)packets_lost/(float)total_packets));
		fprintf(stderr, "Block loss at net level:\t%"PRIu64"/%"PRIu64"\t%.3f%%\n", blocks_lost, total_blocks, 100*((float)blocks_lost/(float)total_blocks));
		fprintf(stderr, "Packet loss at app level:\t%"PRIu64"/%"PRIu64" (%"PRIu64" dropped)\t%.3f%%\n", (packets_lost - packets_dropped), total_packets, packets_dropped,100*((float)(packets_lost - packets_dropped)/(float)total_packets));
		fprintf(stderr, "Block loss at app level:\t%"PRIu64"/%"PRIu64" (%"PRIu64" dropped)\t%.3f%%\n", (blocks_lost - blocks_dropped), total_blocks, blocks_dropped,100*((float)(blocks_lost - blocks_dropped)/(float)total_blocks));
	}
	debug_fprintf(stderr, "Freeing receiver resources");
*/
	close(socket);

#if STATIC_BUFFER
	/* dont call free on static buffer */
	bf->buf = NULL;
#endif

#if TIMING
  if(NULL != flogout){
    fclose(flogout);
  }
#endif /*TIMING*/

	Buffer_free(bf);
	debug_fprintf(stderr, "receiver2 out");
	return packets_received;
}

int LHE_receiver3(int socket, struct sockaddr_in si_other, const char *out, size_t initial_buffer_size)
{
	socklen_t slen = sizeof(si_other);
	int retval, i; 

	uint32_t ts_current = 0;
	uint32_t ts_prev = 0;
	uint32_t ts_step = 0;

	uint16_t frames_received = 0;
	uint64_t packets_dropped = 0, packets_lost = 0, total_packets = 0;
	uint64_t blocks_dropped = 0;

	uint16_t current_seqnum, previous_seqnum, next_seqnum;
	uint32_t current_session;

	uint8_t *rover;

	int fout;

	uint8_t blocks_in_paq;

	uint16_t block_n;

	Buffer_t bf; //reception buffer

	bool first = true;
	bool startup = true;

	uint8_t LHE_header_file_version = 1;

	RTP_packetheader_t *rtp;
	LHE_packetheader_t *ph;
	LHE_blockheader_t *bh;
	size_t block_len;
	//special blockheader zero for signaling end of frame 
		LHE_blockheader_t *bz;
		bz = (LHE_blockheader_t *)malloc(sizeof(*bz));
		LHE_BLOCKHEADER_SET_LEN(*bz, 0);
		LHE_BLOCKHEADER_SET_TYPE(*bz, 0);

#if TIMING
  char timing_filename[1000];
  const char *filename;
  clockid_t clk_id = CLOCK_REALTIME;
  struct timespec frame_in, frame_out;
  frame_in.tv_sec = 0;
  frame_in.tv_nsec = 0;
  frame_out.tv_sec = 0;
  frame_out.tv_nsec = 0;

  FILE *flogout; //timing log file

  printf("TIMING!!\n");
  sprintf(timing_filename, "performance_timing/receiver_timing_%ld.log", time (NULL));
  filename = timing_filename;
  flogout = fopen(filename, "w+");
  if(NULL == flogout){
    printf("ERROR: couldn't open receiver_timing.log\n");
    stop = true;
  }else{
  	fprintf(flogout, "frame_in.sec\tframe_in.nsec\tframe_out.sec\tframe_out.nsec\n"); //column names
  }
#endif /*TIMING*/

#if STATIC_BUFFER
	/*
	 * We can allocate a static buffer compiling with CEXTRAFLAGS=-DSTATIC_BUFFER=1
	 * WATCH OUT! All calls MUST HAVE growable=false, maybe move "growable" to
	 * Buffer_new() and keep as internal variable?
	 */
	static char static_buffer[MAX_DGRAM_SIZE];
#endif

	die_if_condition(NULL == out, "Directory is a mandatory argument for receiver2");

	// ensure valid port number
	//die_if_condition(0 == port, "Port must be non zero!");
	/*	Port 0 trick:
			"On both Windows and Linux, if you bind a socket to port 0,
	 		the kernel will assign it a free port number somewhere above 1024."
		--> port 0 atcually is the most ensured port number
	*/

	/* Set handler for SIGINT */
	struct sigaction act;

	act.sa_handler = sigint_handler;
	sigemptyset (&act.sa_mask);
	act.sa_flags = 0;
	sigaction(SIGINT, &act, NULL);

	debug_fprintf(stderr, "SIGINT captured");

	debug_fprintf(stderr, "Allocating receiver resources");


#if !STATIC_BUFFER
	bf = Buffer_new(initial_buffer_size);
	die_if_condition(NULL == bf, "Buffer allocation failed!");
#else
	bf = Buffer_new(0);
	die_if_condition(NULL == bf, "Buffer allocation failed!");
	bf->buf = static_buffer;
	bf->size = sizeof(static_buffer);
#endif
	
	debug_fprintf(stderr, "Starting receiver loop");

	//Create pipe
		fout = open(out, O_WRONLY);
		if(fout == -1){
			perror("opening pipe");
			return -1;
		}
  	fcntl(fout, F_SETPIPE_SZ, 1048576);
  	//fcntl(fout, F_SETPIPE_SZ, 0);

	// Keep listening for data
	while(!stop) {
		Buffer_clear(bf);

		// you can read from a socket
#if STATIC_BUFFER
		retval = bf(Buffer_recvfrom, MAX_DGRAM_SIZE, false, socket, (struct sockaddr *)&si_other, &slen);
#else
		retval = Buffer_recvfrom(bf, MAX_DGRAM_SIZE, true, socket, (struct sockaddr *)&si_other, &slen);
#endif
		// read/recv call may have been interrupted by SIGINT
		if (stop && !first) { //if stop is called during the transmission of a file
			frames_received++;
			total_blocks += LHE_PACKETHEADER_GET_IM_NBLOCKS(*ph);
			break;
		}
		if (-1 == retval) {
			perror("read");
			return -1;
		} else {
#if TIMING
        if(first){
        	clock_gettime(clk_id, &frame_in);
        }
#endif /*TIMING*/

			rtp = (RTP_packetheader_t *)bf->buf; //RTP header
			ph = (LHE_packetheader_t *)(bf->buf + sizeof(*rtp)); //LHE header
			
			previous_seqnum = current_seqnum;
			current_seqnum = RTP_GET_SEQ_NUM(*rtp);

			if(startup){
				current_session = RTP_GET_SSRC(*rtp);
				next_seqnum = current_seqnum;
				startup = false;
			}
			if(current_session != RTP_GET_SSRC(*rtp)){ //if a new session is starting
				next_seqnum = current_seqnum;
			}

			//determine packet loss
			if(!startup && (current_session == RTP_GET_SSRC(*rtp))){ //if not on startup of receiver or on the start of a new session
				if(next_seqnum < current_seqnum){ //seqnum skips forward -> loss
					packets_lost += current_seqnum - next_seqnum;
					next_seqnum = current_seqnum;
				}else if(next_seqnum > current_seqnum){ //late packet or skip over rollover?
					if((65535 - next_seqnum + current_seqnum) < (next_seqnum - current_seqnum)){ //if the leap is smaller forwards than backwards -> loss 
						packets_lost += 65535 - next_seqnum + current_seqnum +1;
						next_seqnum = current_seqnum;
					}//else -> late packet 
						//+1 packet recovered -1 packet not coming at its time = +/-0 losses
				} 
			}
			next_seqnum++;

			//adress packet loss/out of order arrivals
			if(!first && RTP_GET_TS(*rtp) != ts_current){ 
				if(current_seqnum < previous_seqnum){ //a prior frame
					packets_dropped++; //too late, drop it
					blocks_dropped += LHE_PACKETHEADER_GET_NBLOCKS(*ph);
					continue;  
				} 
				else{ //if(current_seqnum < previous_seqnum){ //the next frame, dispatch current frame as is and get ready
							
					frames_received++;
					total_blocks += LHE_PACKETHEADER_GET_IM_NBLOCKS(*ph);

					//write block header with size 0 to pipe to signal end of frame
						retval = write(fout, bz, sizeof(*bz));
						if (-1 == retval) {
							perror("write");
							return -1;
						}
					first = true; //next packet is first packet of next frame
				} 
			}

			if(ts_step > 0 && ((RTP_GET_TS(*rtp) - ts_current) > 2 * ts_step)){
				//printf("FRAME LOSS! (%d)\n", ((RTP_GET_TS(*rtp) - ts_current)/ts_step)-1); //frame loss OR frame rate has been reduced
			}

			if(first){ //first packet of a frame -> init
				//get TS
				ts_prev = ts_current;
				ts_current = RTP_GET_TS(*rtp); 
				
				//get file name (out)/(RTP timestamp).lhe
				if(ts_current >= ts_prev){
					ts_step = (ts_current - ts_prev);
				}else{
					ts_step = (TS_MAX - ts_prev) + ts_current;
				}
				//write lhe header to pipe
					LHE_header_change_version(ph, ph, LHE_header_file_version);
					retval = write(fout, bf->buf + sizeof(*rtp), sizeof(*ph));
					if (-1 == retval) {
						perror("write");
						return -1;
					}
				first = false; //next is not first
			}

			//copy blocks from reception buffer to write buffer
			rover = (uint8_t *)bf->buf + sizeof(*rtp) + sizeof(*ph); //points to first block
			blocks_in_paq = LHE_PACKETHEADER_GET_NBLOCKS(*ph); 
			
			block_n = LHE_PACKETHEADER_GET_FB_ID(*ph);

			for(i = 0; i < blocks_in_paq; i++){
				// grab a new block
				bh = (LHE_blockheader_t *)rover;
				block_len = LHE_BLOCKHEADER_GET_LEN(*bh);

				//write block header from reception buffer to pipe
					retval = write(fout, rover, sizeof(*bh));
					if (-1 == retval) {
						perror("write");
						return -1;
					}
				//write block number to pipe
					
					retval = write(fout, &block_n, sizeof(uint16_t));
					if (-1 == retval) {
						perror("write");
						return -1;
					}
				//write block from reception buffer to pipe
					retval = write(fout, rover + sizeof(*bh), block_len);
					if (-1 == retval) {
						perror("write");
						return -1;
					}

				//advance read pointer to end of block
				rover += sizeof(*bh) + block_len;
				block_n++;
			}

			packets_received++;
			blocks_received += blocks_in_paq;

			if(1 == RTP_GET_M(*rtp)){ //if marker --> last packet of frame
									
#if TIMING
        		clock_gettime(clk_id, &frame_out);
          		fprintf(flogout, "%lld\t%ld\t%lld\t%ld\n", (long long)frame_in.tv_sec, frame_in.tv_nsec, (long long)frame_out.tv_sec, frame_out.tv_nsec);
#endif /*TIMING*/

				frames_received++;
				total_blocks += LHE_PACKETHEADER_GET_IM_NBLOCKS(*ph);

				//write block header with size 0 to pipe to signal end of frame
					retval = write(fout, bz, sizeof(*bz));
					if (-1 == retval) {
						perror("write");
						return -1;
					}

				first = true; //next packet is first packet of next frame
			}
		}
	}
	total_packets = packets_received + packets_lost + packets_dropped;

	blocks_lost = total_blocks - blocks_received;

	if(!stop){
		fprintf(stderr, "Something wrong happened (?)\n");
	}
	if(total_blocks == 0){
		total_blocks++; //0 div
	}
	if(total_packets == 0){
		total_packets++; //0 div
	}
	fprintf(stderr, "\nreceived:\t%"PRIu64"\n", packets_received);
	fprintf(stderr, "loss:\t%"PRIu64"/%"PRIu64"\t%.3f%%\n", packets_lost, total_packets, 100*((float)packets_lost/(float)total_packets));
	fprintf(stderr, "block loss: %.3f%%\n", 100*((float)blocks_lost/(float)total_blocks));
	fprintf(stderr, "Frames received:\t%"PRIu16"\n", frames_received);
	/*fprintf(stderr, "Frames received:\t%"PRIu16"\n", frames_received);
	if(frames_received > 0){
		fprintf(stderr, "Packet loss at net level:\t%"PRIu64"/%"PRIu64"\t%.3f%%\n", packets_lost, total_packets, 100*((float)packets_lost/(float)total_packets));
		fprintf(stderr, "Block loss at net level:\t%"PRIu64"/%"PRIu64"\t%.3f%%\n", blocks_lost, total_blocks, 100*((float)blocks_lost/(float)total_blocks));
		fprintf(stderr, "Packet loss at app level:\t%"PRIu64"/%"PRIu64" (%"PRIu64" dropped)\t%.3f%%\n", (packets_lost - packets_dropped), total_packets, packets_dropped,100*((float)(packets_lost - packets_dropped)/(float)total_packets));
		fprintf(stderr, "Block loss at app level:\t%"PRIu64"/%"PRIu64" (%"PRIu64" dropped)\t%.3f%%\n", (blocks_lost - blocks_dropped), total_blocks, blocks_dropped,100*((float)(blocks_lost - blocks_dropped)/(float)total_blocks));
	}
	debug_fprintf(stderr, "Freeing receiver resources");
*/
	close(socket);

#if STATIC_BUFFER
	/* dont call free on static buffer */
	bf->buf = NULL;
#endif

#if TIMING
  if(NULL != flogout){
    fclose(flogout);
  }
#endif /*TIMING*/

	//close pipe
      if(-1 == close(fout)){
        perror("closing pipe");
        return -1;
      }
    free(bz);
	Buffer_free(bf);
	debug_fprintf(stderr, "receiver3 out");
	return packets_received;
}
