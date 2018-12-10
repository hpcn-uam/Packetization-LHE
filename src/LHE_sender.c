/**
 * @file
 * @author: Eduardo Miravalls Sierra
 * @author: David Muelas Recuenco
 * @author: Angel Lopez
 */

#include <arpa/inet.h>
#include <assert.h>
#include <errno.h>
#include <inttypes.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <time.h>
#include <unistd.h>     // for sleep function
#include "stdint.h"

#include "block_mover_lib.h"
#include "Buffer.h"
#include "LHE_headers.h"
#include "utils.h"


#define GLOBAL_HEADER_SIZE 8
#define BLOCK_HEADER_SIZE 2
#define DMA_WORD_SIZE 2 // size of DMA transactions in 32 bit words
#define DEFAULT_FPS 110 //fps configured on camera
#define TS_STEP (90000/DEFAULT_FPS) //timestamp step for DEFAULT_FPS (real step = TS_STEP*fps_div)
uint8_t fps_div = 5; //relative fps (real fps = DEFAULT_FPS/fps_div)

lhe_parameters_s system_settings;

static
volatile bool stop = false;

static
void sigint_handler(int signo)
{
	(void)signo; // ignore warning

	fprintf(stderr, "Sender received SIGINT!\n");
	stop = true;
}

int LHE_sender(int socket, size_t initial_buffer_size,
	uint16_t initial_seq_num, uint32_t initial_ts, uint32_t initial_SSRC,
	uint8_t *in, size_t number_frames, size_t max_chunk_size)
{
	int retval;
	uint8_t *rover;
	uint32_t blocks_in_chunk, first_block_id, blocks_read;
	uint32_t blocks_in_image;
	size_t packets_sent, frames_sent;
	clockid_t clk_id = CLOCK_MONOTONIC;
	struct timespec start, now, dif;
	dif.tv_sec = 0;

	uint8_t version_LHE_header = 1;

	Buffer_t bf;

	die_if_condition(30 > max_chunk_size, "Chunk size should be at least 30");
	//assert(NULL != in);

	/* Set handler for SIGINT */
	struct sigaction act;

	act.sa_handler = sigint_handler;
	sigemptyset (&act.sa_mask);
	act.sa_flags = 0;
	sigaction(SIGINT, &act, NULL);

	debug_fprintf(stderr, "SIGINT captured");

	/* Allocate send buffer
	 * NOTE: the buffer can be statically allocated by setting initial size to 0
	 * and manually setting buf to point to a static buffer and update size to correct length,
	 * however, keep in mind that before calling Buffer_free() one has to set buf back to NULL.
	 * On the plus side, this allows us to check for memory errors with valgrind.
	 */
	bf = Buffer_new(initial_buffer_size);
	die_if_condition(NULL == bf, "Buffer allocation failed!");

	// initialize RTP header
	RTP_packetheader_t *rtp = (RTP_packetheader_t *)bf->buf;
	bf->off = -1;
	bf->used = sizeof(*rtp);

	/* strategy:
	 * bf->used is going to indicate how many bytes are in the buffer
	 * and bf->off is not going to be used
	 */

	RTP_set_version(rtp, 2);
	RTP_set_P(rtp, 0);
	RTP_set_X(rtp, 0);
	RTP_set_CC(rtp, 0); 
	RTP_set_M(rtp, 0); 
	RTP_set_PT(rtp, 124);
	RTP_set_seq_num(rtp, initial_seq_num); 
	RTP_set_ts(rtp, initial_ts); 
	RTP_set_SSRC(rtp, initial_SSRC); 

	// assume "in" points to LHE Packet Header
	LHE_packetheader_t *in_ph = (LHE_packetheader_t *)in;
	LHE_packetheader_t *ph = (LHE_packetheader_t *)(bf->buf + bf->used);

	// Set sending header values
	// We get all fields in original header (agnostic to version)
	// and set them in the traffic stream as requested by the call
	//print_LHE_header(in_ph);
	LHE_header_change_version(in_ph, ph, version_LHE_header);
	//print_LHE_header(ph);
	rover = in + sizeof(*in_ph);
	bf->used += sizeof(*in_ph);
	first_block_id = 0; // value of first_block_id that will go in chunk
	blocks_in_image = LHE_PACKETHEADER_GET_IM_H(*in_ph) * LHE_PACKETHEADER_GET_IM_W(*in_ph);
	
	frames_sent = 0;
	packets_sent = 0;
	blocks_read = 0;
	debug_fprintf(stderr, "Start sending frame %zu of %"PRIu32" blocks", frames_sent, blocks_in_image);

	debug_fprintf(stdout, "Sender starting loop");

	/* WHATCH OUT: assuming bf is not resized! After Buffer_resize() all pointers
	 * to the buffer are invalid and need to be updated!
	 * WATCH OUT: assumes "in" is a gigantic linear buffer!
	 *
	 * TODO: what do I do If memory is not aligned?
	 */

	//transmission start time
	clock_gettime(clk_id, &start);

	while (!stop) {
		if (stop){
			break;
		}
		LHE_blockheader_t *bh;
		assert(blocks_read <= first_block_id);

		for(blocks_in_chunk = 0; bf->used < max_chunk_size && blocks_read < blocks_in_image && blocks_in_chunk < MAX_BLOCKS_PER_CHUNK; blocks_in_chunk++, blocks_read++) {
			size_t block_len;
			size_t new_used;

			// grab a new block
			bh = (LHE_blockheader_t *)rover;

			block_len = LHE_BLOCKHEADER_GET_LEN(*bh);
			new_used = bf->used + sizeof(*bh) + block_len;

			if (new_used > max_chunk_size && blocks_in_chunk > 0) {
				// don't copy a block if it doesn't fit in the buffer
				// BUT, copy at least one block!
				break;
			}

			if(sizeof(LHE_blockheader_t *) + block_len > max_chunk_size){
				printf("ERROR: block too big! (%zu)\n", block_len);
				return packets_sent;
			}

			// copy block header
			assert(bf->used < bf->size);
			assert((bf->used + sizeof(*bh)) <= bf->size);
			memcpy(bf->buf + bf->used, bh, sizeof(*bh));
			bf->used += sizeof(*bh);

			// advance pointer in "in"
			rover += sizeof(*bh); //new//bh is also on the file, so advance

			// copy block
			assert(bf->used < bf->size);
			assert((bf->used + block_len) <= bf->size);
			memcpy(bf->buf + bf->used, rover, block_len);
			bf->used += block_len;

			// advance pointer in "in"
			//old//rover += sizeof(*bh) + block_len; //bh is also in original file, so this sends bh twice and omits last two bytes of each block
			rover += block_len; //new//block read, so advance
		}

		assert(blocks_in_chunk != 0);

		LHE_packetheader_set_nblocks(ph, blocks_in_chunk);
		LHE_packetheader_set_1st_block_ID(ph, first_block_id);
		first_block_id += blocks_in_chunk;

		if (first_block_id == blocks_in_image) {
			RTP_SET_M(*rtp, 1); // signal last chunk
		}

		debug_fprintf(stdout, "%zu: Sending chunk of frame %zu with %"PRIu32" blocks and %zu bytes.\n", packets_sent, frames_sent, blocks_in_chunk, bf->used);
		
		// Send the chunk
		retval = send(socket, bf->buf, bf->used, 0);


		// have I been interrupted?
		if (stop) {
			break;

		} else {
			perror_if_condition(-1 == retval, "sendto()");
			packets_sent++;
		}

		// ignore everything in buffer BUT the RTP+LHE headers
		bf->used = sizeof(*rtp) + sizeof(*ph);
		// update sequence number
		RTP_SET_SEQ_NUM(*rtp, RTP_GET_SEQ_NUM(*rtp) + 1);

		if (first_block_id >= blocks_in_image) { 
			
			//wait for FPS
			clock_gettime(clk_id, &now);

			dif.tv_sec = ((33333333 * frames_sent)/1000000000) - (now.tv_sec - start.tv_sec);
			if(dif.tv_sec < 0){
				dif.tv_sec = 0;
			}
			dif.tv_nsec = (33333333 * frames_sent) - (now.tv_nsec - start.tv_nsec);
			if(dif.tv_nsec < 0){
				dif.tv_nsec = 0;
			}
			
			if(dif.tv_sec > 0 || dif.tv_nsec > 0){
				//printf("Waiting for  %.ld s %.ld ms ...\n", dif.tv_sec, dif.tv_nsec/1000000); //debug
				nanosleep(&dif, NULL); //WARNING: implement error control!					
			}


			// Start new frame
			frames_sent++;
			// TODO: workaround to only having a finite number of lhe files in buffer
			if (frames_sent >= number_frames) {
				break;
			}

			// assume header is contiguous to last block
			in_ph = (LHE_packetheader_t *)rover;
			// just image size changed case it changed
			*ph = *in_ph;


			blocks_in_image = LHE_PACKETHEADER_GET_IM_H(*in_ph) * LHE_PACKETHEADER_GET_IM_W(*in_ph);
			debug_fprintf(stderr, "Start sending frame %zu of %"PRIu32" blocks", frames_sent, blocks_in_image);

			rover += sizeof(*ph);
			first_block_id = 0;
			blocks_read = 0;

			/*
			 * TODO: update timestamp: how do I get FPS?
			 * ts is in 90000Hz units
			 *  24 FPS: new_ts = old_ts + 3750
			 *  25 FPS: new_ts = old_ts + 3600
			 *  30 FPS: new_ts = old_ts + 3000
			 *  50 FPS: new_ts = old_ts + 1800
			 *  60 FPS: new_ts = old_ts + 1500
			 * 120 FPS: new_ts = old_ts +  750
			 */

			RTP_SET_TS(*rtp, RTP_GET_TS(*rtp) + TS_STEP);
			RTP_SET_M(*rtp, 0);
		}
	}

	debug_fprintf(stderr, "%zu frames sent in %"PRIu64" packets", frames_sent, packets_sent);
	Buffer_free(bf);

	if (stop){
		stop = false;
		return 0;
	}



	return packets_sent;
}

int LHE_send_from_memory(int socket, size_t initial_buffer_size, 
	uint16_t initial_seq_num, uint32_t initial_ts, uint32_t initial_SSRC, size_t max_chunk_size)
{

	int retval;
	uint32_t blocks_in_chunk, first_block_id, blocks_read;
	uint32_t blocks_in_image;
	size_t packets_sent, frames_sent;
	
	bool first = true;
	bool setup = true;
	bool block_queued = false;
	bool get_id = true;
	bool get_timer = true;

	srand(time(NULL));

	Buffer_t bf;

    uint32_t *ptr_to_data;
    struct lhe_global_header_hw global_header;
	struct lhe_local_header_hw local_header;
    uint16_t received_block = 0;
    uint16_t received_frame = 0;
	uint16_t block_len;
	size_t new_used;
    uint8_t type;

#ifdef TIMING
    uint32_t timer_in = 0, timer_out = 0, hw_ts_in = 0;
    char timing_filename[1000];
      const char *filename;

	FILE *fout; //timing log file
#endif /*TIMING*/

	die_if_condition(30 > max_chunk_size, "Chunk size should be at least 30");
	//assert(NULL != in);

	/* Set handler for SIGINT */
	struct sigaction act;

	act.sa_handler = sigint_handler;
	sigemptyset (&act.sa_mask);
	act.sa_flags = 0;
	sigaction(SIGINT, &act, NULL);

	debug_fprintf(stderr, "SIGINT captured");

	/* Allocate send buffer
	 * NOTE: the buffer can be statically allocated by setting initial size to 0
	 * and manually setting buf to point to a static buffer and update size to correct length,
	 * however, keep in mind that before calling Buffer_free() one has to set buf back to NULL.
	 * On the plus side, this allows us to check for memory errors with valgrind.
	 */
	bf = Buffer_new(initial_buffer_size);
	die_if_condition(NULL == bf, "Buffer allocation failed!");

	// initialize RTP header
	RTP_packetheader_t *rtp = (RTP_packetheader_t *)bf->buf;
	bf->off = -1;
	bf->used = sizeof(*rtp);

	/* strategy:
	 * bf->used is going to indicate how many bytes are in the buffer
	 * and bf->off is not going to be used
	 */

	RTP_set_version(rtp, 2);
	RTP_set_P(rtp, 0);
	RTP_set_X(rtp, 0);
	RTP_set_CC(rtp, 0); 
	RTP_set_M(rtp, 0); 
	RTP_set_PT(rtp, 124);
	RTP_set_seq_num(rtp, initial_seq_num); 
	RTP_set_ts(rtp, initial_ts); 
	RTP_set_SSRC(rtp, initial_SSRC); 

	//init LHE header
	LHE_packetheader_t *ph = (LHE_packetheader_t *)(bf->buf + bf->used);
	bf->used += sizeof(*ph);
	
	LHE_packetheader_set_version(ph, 1);
	LHE_packetheader_set_audio(ph, 0);
	LHE_packetheader_set_differential(ph, 0);
	LHE_packetheader_set_profile(ph, 0);
	LHE_packetheader_set_reserved(ph, 0);
	LHE_packetheader_set_codec(ph, 0);
	LHE_packetheader_set_nblocks(ph, 0);
	LHE_packetheader_set_block_height(ph, 0);
	LHE_packetheader_set_block_width(ph, 0);
	LHE_packetheader_set_im_height(ph, 0);
	LHE_packetheader_set_im_width(ph, 0);
	LHE_packetheader_set_1st_block_ID(ph, 0);

    blocks_read = 0;
    first_block_id = 0;
	frames_sent = 0;
	packets_sent = 0;
	first = true;
	setup = true;
	stop = false;
	block_queued = false;
	get_id = true;

#if TIMING
printf("TIMING!!\n");	
	sprintf(timing_filename, "performance_timing/sender_timing_%ld.log", time (NULL));
  	filename = timing_filename;
  	fout = fopen(filename, "w+");
	if(NULL == fout){
		printf("ERROR: couldn't open sender_timing.log\n");
		stop = true;
	}else{
		fprintf(fout, "max_fps\tchroma\tim_w\tim_h\tb_w\tb_h\trel_fps\thw_ts_in\ttimer_in\ttimer_out\n"); //column names
	}
#endif /*TIMING*/

    /* Start moving data from the PL to PS */
    reset_hardware();
/*
 	system_settings.chroma_subsampling  = CHROMA_SUB_400;
	system_settings.image_width         = 1280;   
	system_settings.image_height        = 720;   
	system_settings.block_width         = 128;   // Block width  size in pixels
	system_settings.block_height        = 8;     // Block height size in pixels   
    system_settings.relative_fps        = fps_div;
*/
	if(0 > get_parameters(&system_settings)){ //set HW configuration parameters
		stop = true; //error
	}

    start_video_streaming();

	while(true){
		if (stop){
			break;
		}

		//process a frame:
		if(first){ //first block of a frame

			//build packet lhe header
			/* Get the first LHE global header */
			//get_codec_state();
        	while(get_lhe_global_header(&global_header) < 1){ // Wait until Global header is ready 
        		if (stop){
					break;
				}
			}
		
			if(0 > get_parameters(&system_settings)){ //get HW configuration parameters (might've been changed by other SW)
				stop = true; //error
			}
        	//printf("[FRAME %zu]\n", frames_sent);//debug
        
        	/*
        	printf("relative fps: %d  - profile: %d  -  blk_height: %d  -  blk_width: %d  -  img_heigh: %d  -  img_width: %d\n",
        	    global_header.fps,global_header.profile,global_header.blk_height,global_header.blk_width,
        	    ((uint16_t)global_header.img_height_h)<<8|global_header.img_height_l,((uint16_t)global_header.img_width_h)<<8|global_header.img_width_l);  
        	//*/ //debug

			//get data from lhe global header (on memory) to the packet lhe header
			//	LHE Global Header
			//	   0                   1                   2                   3  
			//	   0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1
			//	  +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
			//	  |  FPS  |Profile|   Reserved    |      BH       |      BW       |
			//	  +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
			//	  |               IH              |               IW              |
			//	  +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+

			LHE_packetheader_set_profile(ph, global_header.profile);
			LHE_packetheader_copy_block_height(ph, global_header.blk_height);
			LHE_packetheader_copy_block_width(ph, global_header.blk_width);
			LHE_packetheader_set_im_height(ph, ((uint16_t)global_header.img_height_h)<<8|global_header.img_height_l);
			LHE_packetheader_set_im_width(ph, ((uint16_t)global_header.img_width_h)<<8|global_header.img_width_l);		
			fps_div = global_header.fps;

			//get # of blocks in frame		
			blocks_in_image = LHE_PACKETHEADER_GET_IM_NBLOCKS(*ph);
			
			first = false;

			if(!setup){ //first RTP header is not updated
				//setup rtp header
				RTP_SET_M(*rtp, 0);
				//TO DO: update TS (get FPS field from memory header) 
				RTP_SET_TS(*rtp, RTP_GET_TS(*rtp) + (TS_STEP * fps_div));
			}else{
				setup = false;
			}
		}
		
		debug_fprintf(stderr, "Start sending frame %zu of %"PRIu32" blocks\n", frames_sent, blocks_in_image);

		for(blocks_in_chunk = 0; bf->used < max_chunk_size && blocks_read < blocks_in_image && blocks_in_chunk < MAX_BLOCKS_PER_CHUNK; blocks_in_chunk++, blocks_read++){
			//read a block from memory

			if(stop){
				break;
			}

			if(block_queued == false){ //if there is no block pending send
       			if(stop){
					break;
				}else{
					/* Get the pointer to the current block and its ID */
	       			while(get_next_block(&ptr_to_data, &local_header, &received_block, &received_frame) != 0){ // Wait until local header is ready
						if(stop){
							break;
						}
	       			}					
				}
#if TIMING
				if(get_timer){ //for performance timing log
	        		timer_in = get_register(LHE_SETTINGS_TIME_COUNTER_LSB);
	            	hw_ts_in = local_header.timestamp;
	            	get_timer = false;
				}
#endif /*TIMING*/
			}


            /* Compute size of the current block (13-bits) and padding (3-bits) */ 
            block_len = local_header.block_size;
            type      = local_header.gop;
            

            if(get_id == true){
            	first_block_id = received_block;
            	LHE_packetheader_set_1st_block_ID(ph, first_block_id);
				get_id = false;
            }

            if(blocks_read != received_block){
            	printf("ERROR: Received an unexpected block!");
            	stop = true;
            	break;
            }
            	
       		new_used = bf->used + sizeof(LHE_blockheader_t *) + block_len;

			if (new_used > max_chunk_size && blocks_in_chunk > 0 && !block_queued) {
				// don't copy a block if it doesn't fit in the buffer
				// BUT, copy at least one block!
				block_queued = true; //the block will be sent on next packet
				break;
			}else{
				block_queued = false;
			}

			// copy block header
			LHE_BLOCKHEADER_SET_LEN(*(LHE_blockheader_t *)(bf->buf + bf->used), block_len);
			LHE_BLOCKHEADER_SET_TYPE(*(LHE_blockheader_t *)(bf->buf + bf->used), type);
			bf->used += sizeof(LHE_blockheader_t);

			// copy block
			assert(bf->used < bf->size);
			assert((bf->used + block_len) <= bf->size);

			/* Check if the read size is lower than the maximum */
     		if(block_len > MAX_BLOCK_BYTES){ 
     		    printf("Error read block size larger than allowed. Block[%d]. Size = %u\n",received_block,block_len);
     		}else{
				memcpy(bf->buf + bf->used, ptr_to_data, block_len);
				bf->used += block_len;				
     		}
		}

		if (stop){
			break;
		}

		LHE_packetheader_set_nblocks(ph, blocks_in_chunk);
		RTP_SET_SEQ_NUM(*rtp, RTP_GET_SEQ_NUM(*rtp) + 1); //update RTP seqnum
		get_id = true;

		if(blocks_read == blocks_in_image){ //last block of a frame
			//printf("read:\t%d\ttotal:\t%d\n", blocks_read, blocks_in_image);
			RTP_SET_M(*rtp, 1); //set M flag on RTP header
			first = true; //next is first
			first_block_id = 0; //next block is first block of next frame
			blocks_read = 0;
			frames_sent++; //kinda

#if TIMING
			//get timer for performance timing log
	        timer_out = get_register(LHE_SETTINGS_TIME_COUNTER_LSB);
        	fprintf(fout, "%d\t%"PRIu8"\t%"PRIu16"\t%"PRIu16"\t%"PRIu16"\t%"PRIu16"\t%"PRIu8"\t%"PRIu32"\t%"PRIu32"\t%"PRIu32"\n", DEFAULT_FPS, system_settings.chroma_subsampling, system_settings.image_width, system_settings.image_height, system_settings.block_width, system_settings.block_height, system_settings.relative_fps, hw_ts_in, timer_in, timer_out);
			get_timer = true;
#endif /*TIMING*/
		}

		debug_fprintf(stdout, "%zu: Sending chunk of frame %zu with %"PRIu32" blocks and %zu bytes.\n", packets_sent, frames_sent, blocks_in_chunk, bf->used);

		retval = send(socket, bf->buf, bf->used, 0); //send chunk

		if(-1 == retval){
			printf("ERROR: couldn't send!\n");
		}

		packets_sent++;
		
		// ignore everything in buffer BUT the RTP+LHE headers
		bf->used = sizeof(*rtp) + sizeof(*ph);
	}

	debug_fprintf(stderr, "%zu frames sent in %"PRIu64" packets\n", frames_sent, packets_sent);

  	//stop_video_streaming(); 
    //reset_hardware();
	//set_parameters(&system_settings);
	Buffer_free(bf);
#if TIMING
	if(fout != NULL){
		fclose(fout);
	}
#endif /*TIMING*/
	printf("sender out\n");
	return packets_sent;
}