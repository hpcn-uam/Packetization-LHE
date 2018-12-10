/*
 * @author: Angel Lopez
 */

/*This refactored version of the LHE player has more code duplication, but aims to have better performance.*/
#include <algorithm>
#include <cinttypes>
#include <dirent.h>
#include <fcntl.h>
#include <fstream>
#include <future>
#include <inttypes.h>
#include <iostream>
#include <opencv2/highgui.hpp>
#include <opencv2/imgcodecs.hpp>
#include <opencv2/imgproc/imgproc.hpp>
#include <signal.h>
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <thread>
#include <time.h>
#include <unistd.h>
#include "LHE_codec/src/lhe_codec.h"
#include "../src/LHE_headers.h"
//#include "hptimelib/include/hptl.h" //TO DO

#define __STDC_FORMAT_MACROS 1
#define A_BUFFER_SIZE 1 //player will wait until there are A_BUFFER_SIZE files in dir to start playing

static
volatile bool stop = false;

uint8_t LHE_header_file_version = 1;

static
void sigint_handler(int signo)
{
  (void)signo; // ignore warning

  stop = true;
  fprintf(stderr, "Received SIGINT!\n");
}

int filter_files(const struct dirent *entry){ //filter for scandir
  //filters out everything but files
  if(entry->d_type == DT_REG || entry->d_type == DT_UNKNOWN && strcmp(entry->d_name,".") == 1 && strcmp(entry->d_name,"..") == 1) 
    return 1;
  return 0;
}

void timespec_diff(struct timespec *start, struct timespec *stop,
                   struct timespec *result)
{
  //result = stop - start
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

void usage(){
  printf("\nLHEplayer usage:\n");
  printf("\tLHEplayer [MODE] ([INPUT]) ([OUTPUT])\n");
}

void help(){
   printf("\nOPTIONS:\n");
   printf("\t--decode, -d          [INPUT_DIRECTORY] [OUTPUT_DIRECTORY]   Decodes.lhe files in input dir. to .png files in output dir.\n");
   printf("\t--play, -p            [INPUT_DIRECTORY]                      Plays -lhe video in input dir.\n");
   printf("\t--step, -s            [INPUT_DIRECTORY]                      Plays frame by frame -lhe video in input dir.\n");
   printf("\t--play-fstream, -pfs  [INPUT_DIRECTORY]                      Plays stream video @ input dir (file stream).\n");
   printf("\t--play-pstream, -pps  [INPUT_PIPE]                           Plays stream video @ input pipe(pipe stream).\n");
   printf("\t--help, -h                                                   Display this help.\n");
}

int decode(const char *d_in, const char *d_out){
  int n = 0, i=0;
  uint64_t j=1;
  struct dirent **namelist;
  char filename_in[1000]; //TO DO
  char filename_out[1000]; //TO DO
  const char *filename;
  cv::Mat decode_img;

  //Set handler for SIGINT
    struct sigaction act;
    act.sa_handler = sigint_handler;
    sigemptyset (&act.sa_mask);
    act.sa_flags = 0;
    sigaction(SIGINT, &act, NULL);

  //Read input directory
    n = scandir(d_in, &namelist, filter_files, alphasort); //n = # of files

  //decode all files in directory
    for(i = 0; i < n; i++){
      if(stop){
        break;
      }
      
      sprintf(filename_in, "%s/%s", d_in, namelist[i]->d_name);
      filename = filename_in;
      
      decode_img.release();
      if (lhe_decoder(filename_in,decode_img)){
        std::cerr<<"there's been an error in trying to decode the image"<<std::endl;
      }else{
        //write .png's with same name as .lhe's:
          //strcpy(aux, namelist[i]->d_name);
          //aux[strlen(aux)-4] = '\0'; //dirty way to get the .lhe extension out, but works
          //sprintf(filename_out, "%s/%s.png", d_out, aux);
  
        //write .png's with consecutive names (for ffmpeg):
          sprintf(filename_out, "%s/%010" PRIu64 ".png", d_out, j);
          j++;
  
        imwrite(filename_out, decode_img);
      }
    }

  //Free resources
    while (n--) {
      free(namelist[n]);
    }
    free(namelist);
    decode_img.release();

  return 1;
}

int play(const char *d_in){
  int n = 0, i = 0;
  struct dirent **namelist;
  char filename_in[1000]; //TO DO
  char filename_out[1000]; //TO DO
  const char *filename;
  cv::Mat decode_img;

  //Set handler for SIGINT
    struct sigaction act;
    act.sa_handler = sigint_handler;
    sigemptyset (&act.sa_mask);
    act.sa_flags = 0;
    sigaction(SIGINT, &act, NULL);

  //FPS timing
    uint64_t current_ts, previous_ts = 0; //RTP TS on names
    clockid_t clk_id = CLOCK_MONOTONIC;
    struct timespec previous, current, dif1, dif2, unit;
  
    previous.tv_sec = 0;
    previous.tv_nsec = 0;
    dif1.tv_sec = 0;
    dif1.tv_nsec = 0;
    dif2.tv_sec = 0;
    dif2.tv_nsec = 0;
  
    //90000 Hz unit
    unit.tv_sec = 0;
    unit.tv_nsec = 11111;

  //Usage
    printf("\n\tControls:\n");
    printf("\t  Exit : esc\n\n");

  //Open show window
    cvNamedWindow("LHE_Player", CV_WINDOW_NORMAL);
    cv::waitKey(1); //Window is not actually shown until waitkey is called. Doesn't actually wait 1ms. 

  //Read input directory
    n = scandir(d_in, &namelist, filter_files, alphasort); //n = # of files

  //decode all files in directory
    for(i = 0; i < n; i++){
      if(stop){
        break;
      }
      
      sprintf(filename_in, "%s/%s", d_in, namelist[i]->d_name);
      filename = filename_in;
      
      decode_img.release();
      if (lhe_decoder(filename_in,decode_img)){
        std::cerr<<"there's been an error in trying to decode the image"<<std::endl;
      }else{
        //Show decoded frame
          cv::imshow("LHE_Player",decode_img);
          if(27 == cv::waitKey(1)){ //Window is not actually shown until waitkey is called. Doesn't actually wait 1ms. Closes if ESC is pressed.
            stop = true;
            break;
          }

        //Wait (for appropiate FPS) 
          //get what time has passed since last frame 
            clock_gettime(clk_id, &current);
            timespec_diff(&previous, &current, &dif1);
          //get what time should have passed since last frame 
            current_ts = strtoul(namelist[i]->d_name, NULL, 10); //get ts of current file
            dif2.tv_nsec = (((current_ts - previous_ts) * unit.tv_nsec) - dif1.tv_nsec);
          //if it has been less than it should, wait that difference 
            if(dif2.tv_nsec > 0){
              //printf("%" PRIu64 " ms\tVS\t%.ld ms\t; Waiting for\t%.ld ms ...\n",(current_ts - previous_ts)*11111/1000000,dif1.tv_nsec/1000000, dif2.tv_nsec/1000000);//debug
              nanosleep(&dif2, NULL); //TO DO: implement error control!
            } 
          //current -> previous
            previous = current;
            previous_ts = current_ts;
      }
    }

  //Free resources
    while (n--) {
      free(namelist[n]);
    }
    free(namelist);
    decode_img.release();

  return 1;
}

int step(const char *d_in){
  int n = 0, i = 0;
  struct dirent **namelist;
  char filename_in[1000]; //TO DO
  char filename_out[1000]; //TO DO
  const char *filename;
  cv::Mat decode_img;

  //Set handler for SIGINT
    struct sigaction act;
    act.sa_handler = sigint_handler;
    sigemptyset (&act.sa_mask);
    act.sa_flags = 0;
    sigaction(SIGINT, &act, NULL);

  int key = 0;

  //Showname function
    bool showname = false;
    int showname_c = 0;
    int showname_r = 255;
    int showname_g = 0;
    int showname_b = 0;

  //Usage
    printf("\n\tControls:\n");
    printf("\t  Exit : esc\n");
    printf("\t  Print filename : up arrow\n");
    printf("\t  Show/hide filename on screen : down arrow\n");
    printf("\t  Toggle font color : spacebar\n");
    printf("\t  Go back one frame : left arrow\n");
    printf("\t  Advance one frame : any other key\n\n");

  //Open show window
    cvNamedWindow("LHE_Player", CV_WINDOW_NORMAL);
    cv::waitKey(1); //Window is not actually shown until waitkey is called. Doesn't actually wait 1ms. 

  //Read input directory
    n = scandir(d_in, &namelist, filter_files, alphasort); //n = # of files

  //decode all files in directory
    while(!stop){
      if(stop){
        break;
      }
      
      sprintf(filename_in, "%s/%s", d_in, namelist[i]->d_name);
      filename = filename_in;
      
      decode_img.release();
      if (lhe_decoder(filename_in,decode_img)){
        std::cerr<<"there's been an error in trying to decode the image"<<std::endl;
      }else{
        //Show filename if option is selected
          if(showname){
            cv::putText(decode_img, namelist[i]->d_name, cvPoint(30,30), cv::FONT_HERSHEY_COMPLEX_SMALL, 1.0, cvScalar(showname_r,showname_g,showname_b), 1, CV_AA);
          }

        //Show decoded frame
          cv::imshow("LHE_Player",decode_img);
          key = cv::waitKey(); //wait for key press

        //React to key press
          if(key == 27){ //exit : esc
            stop = true;
            break;
          }else if(key == 82){ //print filename : up arrow
            printf("\t%s\n", filename_in);
          }else if(key == 84){ //show filename : down arrow
            showname = !showname;
          }else if(key == 32){ //change font color : spacebar
            showname_c++;
            if(showname_c == 4){
              showname_c = 0; //loop
            }
            switch(showname_c){ //TO DO: poor style...
              case 0:
              showname_r = 255;
              showname_g = 0;
              showname_b = 0;
              break;
              case 1:
              showname_r = 0;
              showname_g = 255;
              showname_b = 0;
              break;
              case 2:
              showname_r = 0;
              showname_g = 0;
              showname_b = 255;
              break;
              case 3:
              showname_r = 255;
              showname_g = 255;
              showname_b = 255;
              break;
            } 
          }else if(key == 81){ //back : left arrow 
            if(i > 0){
              i--;  
            }
          }else{
            if(i < n-1){ //if not the end
              i++; //advance one frame
            }
          }
      }
    }

  //Free resources
    while(n--){
      free(namelist[n]);
    }
    free(namelist);
    decode_img.release();

  return 1;
}

int play_fstream(const char *d_in){
  int n = 0, i = 0;
  struct dirent **namelist;
  char filename_in[1000]; //TO DO
  char filename_out[1000]; //TO DO
  const char *filename;
  cv::Mat decode_img;

  //Performance timing 
#ifdef TIMING
  clockid_t clk_id = CLOCK_MONOTONIC;
  char timing_filename[1000]; //TO DO
  struct timespec frame_in, frame_decoded, frame_displayed, decode_time, display_time;
  frame_in.tv_sec = 0;
  frame_in.tv_nsec = 0;
  frame_decoded.tv_sec = 0;
  frame_decoded.tv_nsec = 0;
  frame_displayed.tv_sec = 0;
  frame_displayed.tv_nsec = 0;
  decode_time.tv_sec = 0;
  decode_time.tv_nsec = 0;
  display_time.tv_sec = 0;
  display_time.tv_nsec = 0;

  FILE *fout; //timing log file
#endif /*TIMING*/

  //Set handler for SIGINT
    struct sigaction act;
    act.sa_handler = sigint_handler;
    sigemptyset (&act.sa_mask);
    act.sa_flags = 0;
    sigaction(SIGINT, &act, NULL);

  //Usage
    printf("\n\tControls:\n");
    printf("\t  Exit : esc\n\n");

  //Performance timing file
#if TIMING
  printf("TIMING!!\n"); 
  sprintf(timing_filename, "../performance_timing/player_timing_%ld.log", time (NULL));
  filename = timing_filename;
  fout = fopen(filename, "w+");
  if(NULL == fout){
    printf("ERROR: couldn't open sender_timing.log\n");
    stop = true;
  }else{
    fprintf(fout, "decode_time.sec\tdecode_time.nsec\tdisplay_time.sec\tdisplay_time.nsec\n"); //column names
  }
#endif /*TIMING*/

  //Open show window
    cvNamedWindow("LHE_Player", CV_WINDOW_NORMAL);
    cv::waitKey(1); //Window is not actually shown until waitkey is called. Doesn't actually wait 1ms. 


  //Player loop
    while(!stop){
      if(stop){
        break;
      }
      
      //Read input directory
        n = scandir(d_in, &namelist, filter_files, alphasort); //n = # of files

      if(27 == cv::waitKey(1)){ //closes if ESC
        stop = true;
        break;
      }

      if(n >= 1 + A_BUFFER_SIZE){
        //Play only the latest frame: get rid of the rest
          if(n > 1 + A_BUFFER_SIZE){
            for(i = 0; i < n-(1 + A_BUFFER_SIZE); i++){
              sprintf(filename_in, "%s/%s", d_in, namelist[i]->d_name);
              filename = filename_in;
              if(unlink(filename)){
                std::cerr<<"Error: couldn't delete image"<<std::endl;
                stop = true;
              }
              free(namelist[i]);
            }
          }
           
        sprintf(filename_in, "%s/%s", d_in, namelist[(n - A_BUFFER_SIZE)]->d_name);
        filename = filename_in;
        
        decode_img.release();
#if TIMING
          clock_gettime(clk_id, &frame_in); //time before decoding
#endif /*TIMING*/
        if (lhe_decoder(filename_in,decode_img)){
          std::cerr<<"there's been an error in trying to decode the image"<<std::endl;
        }else{
#if TIMING
              clock_gettime(clk_id, &frame_decoded); //time after decoding
#endif /*TIMING*/

          //Show decoded frame
            cv::imshow("LHE_Player",decode_img);
            if(27 == cv::waitKey(1)){ //Window is not actually shown until waitkey is called. Doesn't actually wait 1ms. Closes if ESC is pressed.
              stop = true;
            }
#if TIMING
            clock_gettime(clk_id, &frame_displayed); //time after displaying
            timespec_diff(&frame_in, &frame_decoded, &decode_time);
            timespec_diff(&frame_decoded, &frame_displayed, &display_time);
            fprintf(fout, "%lld\t%ld\t%lld\t%ld\n", (long long)decode_time.tv_sec, decode_time.tv_nsec, (long long)display_time.tv_sec, display_time.tv_nsec);
#endif /*TIMING*/

          //Once played, delete frame
            free(namelist[(n - A_BUFFER_SIZE)]);
            if(unlink(filename_in)){
              std::cerr<<"Error: couldn't delete image"<<std::endl;
              stop = true;
              break;
            }
          //No FPS waiting: play as early as possible
        }
      }
    }

  //Free resources
    for(i = A_BUFFER_SIZE; i > 0; i--){
      free(namelist[n - i]);
      sprintf(filename_in, "%s/%s", d_in, namelist[(n - i)]->d_name);
      filename = filename_in;
      if(unlink(filename_in)){
        std::cerr<<"Error: couldn't delete image"<<std::endl;
        break;
      }
    }
    free(namelist);
    decode_img.release();

  //Close performance timing file
#if TIMING
  if(NULL != fout){
    fclose(fout);
  }
#endif /*TIMING*/

  return 1;
}


#define THREAD_N 5 //min 2, or change join protection for single thread

static cv::Mat frame_buffer; 

//arguments for threads
struct arg_struct {
    uint16_t blk_row;
    uint16_t blk_height;
    uint8_t chroma_mode;
    char codec_mode;
    uint8_t lhe_block_data[MAX_BLOCK_LEN];
    cv::Mat block;
    //int block_n; //only for debugging, not necessary
};

//threads
void *decode_block(void *arguments){
  struct arg_struct *args = (struct arg_struct *)arguments;

  if(args->blk_height==1 && args->chroma_mode==CHROMA_MODE_YUV420 && args->blk_row%2!=0) {
    lhe_decode_core((unsigned char*)args->lhe_block_data, args->block, CHROMA_MODE_GRAY, args->codec_mode);
  }else{
    lhe_decode_core((unsigned char*)args->lhe_block_data, args->block, args->chroma_mode, args->codec_mode);
  }
}



int play_pstream(const char *d_in){
  int retval; //for error control
  int fin; //pipe
  int i = 0; //for loops
  bool eof = false; //end of frame

  LHE_packetheader_t ph; //LHE header
  LHE_blockheader_t bh; //block header
  uint16_t block_n; //block number
  uint8_t lhe_block_data[MAX_BLOCK_LEN]; //actual block data 
  
  uint16_t rows = 0; //# of rows (in blocks)
  uint16_t cols = 0; //# of cols (in blocks)
  size_t block_len; //block length

  uint16_t blk_row, blk_col;
  uint16_t blk_height;
  uint16_t blk_width;
  uint8_t chroma_mode = 0;
  char codec_mode;

  pthread_t some_threads[THREAD_N]; //thread pool
  struct arg_struct args[THREAD_N]; //thread's arguments
  int next_thread = 1; //round robin thread counter
  bool startup = true; //for join thread protection

  //Performance timing 
#ifdef TIMING
    clockid_t clk_id = CLOCK_REALTIME;
    const char *filename;
    char timing_filename[1000]; //TO DO
    struct timespec frame_in, frame_decoded, frame_displayed;
    frame_in.tv_sec = 0;
    frame_in.tv_nsec = 0;
    frame_decoded.tv_sec = 0;
    frame_decoded.tv_nsec = 0;
    frame_displayed.tv_sec = 0;
    frame_displayed.tv_nsec = 0;
  
    FILE *fout; //timing log file
#endif /*TIMING*/

  //Set handler for SIGINT
    struct sigaction act;
    act.sa_handler = sigint_handler;
    sigemptyset (&act.sa_mask);
    act.sa_flags = 0;
    sigaction(SIGINT, &act, NULL);

  //Usage
    printf("\nLHE pipe-stream player:\n");
    printf("\n\tConnecting with LHE receiver...\n\n");

  //Performance timing file
#if TIMING
    printf("TIMING!!\n"); 
    sprintf(timing_filename, "../performance_timing/pipe_player_timing_%ld.log", time (NULL));
    filename = timing_filename;
    fout = fopen(filename, "w+");
    if(NULL == fout){
      printf("ERROR: couldn't open sender_timing.log\n");
      stop = true;
    }else{
      fprintf(fout, "frame_in.sec\tframe_in.nsec\tframe_decoded.sec\tframe_decoded.nsec\tframe_displayed.sec\tframe_displayed.nsec\n"); //column names
    }
#endif /*TIMING*/

  //Open show window
    cvNamedWindow("LHE_Player", CV_WINDOW_NORMAL);
    cvResizeWindow("LHE_Player", 640, 480);
    //cv::waitKey(1); //Window is not actually shown until waitkey is called. Doesn't actually wait 1ms. 
  
  //Create pipe
    unlink(d_in);
    retval = mkfifo(d_in, 0666); //create named pipe : mkfifo(<pathname>, <permission>)
    if(retval == -1){
      perror("mkfifo"); fflush(stderr);
      return -1;
    }

  //open pipe
    fin = open(d_in, O_RDONLY);
    if(fin == -1){
      perror("opening pipe"); fflush(stderr);
      return -1;
    }
    fcntl(fin, F_SETPIPE_SZ, 1048576); //set pipe size
    //fcntl(fin, F_SETPIPE_SZ, 0);

    //READ FRAME LOOP
    while(!stop){
      eof = false;
      retval = read(fin, &ph, sizeof(ph)); //read LHE_header
      if(retval < 0){ //error
        perror("read"); fflush(stderr);
        break;
      }else if(retval == 0){
        printf("\n\tTransmission finished (?)\n\n"); fflush(stdout);
        stop = true;
        break;
      }else if(retval != sizeof(ph)){
        printf("Unexpected lhe_h read size (%d, expected %zu)\n", retval, sizeof(ph)); fflush(stdout);
        stop = true;
        break;
      }else if(retval > 0){ //getting new frame...
#if TIMING
        clock_gettime(clk_id, &frame_in); //time before decoding
#endif /*TIMING*/

        LHE_header_change_version(&ph, &ph, LHE_header_file_version);
        blk_height = LHE_PACKETHEADER_GET_BLOCK_H(ph);
        blk_width = LHE_PACKETHEADER_GET_BLOCK_W(ph);

        //if frame config is different, update frame_buffer 
          if(cols != LHE_PACKETHEADER_GET_IM_W(ph) * LHE_PACKETHEADER_GET_BLOCK_W(ph) 
              || rows != LHE_PACKETHEADER_GET_IM_H(ph) * LHE_PACKETHEADER_GET_BLOCK_H(ph) 
              || chroma_mode != LHE_PACKETHEADER_GET_PROFILE(ph))
          {
            cols = LHE_PACKETHEADER_GET_IM_W(ph) * LHE_PACKETHEADER_GET_BLOCK_W(ph);
            rows = LHE_PACKETHEADER_GET_IM_H(ph) * LHE_PACKETHEADER_GET_BLOCK_H(ph);
            chroma_mode = LHE_PACKETHEADER_GET_PROFILE(ph);

            frame_buffer.release();
            if (chroma_mode==CHROMA_MODE_GRAY){
              frame_buffer=cv::Mat::zeros(rows,cols,CV_MAKETYPE(CV_8U,1));
            }else{
              frame_buffer=cv::Mat::zeros(rows,cols,CV_MAKETYPE(CV_8U,3));
            }
          } 
          codec_mode = (chroma_mode==CHROMA_MODE_YUV420 && blk_height==1)? 1 : 0;
        
        //READ BLOCK LOOP
        while(!stop && !eof){ 
          retval = read(fin, &bh, sizeof(bh)); //read block header
          if(retval < 0){ //error
            perror("read"); fflush(stderr);
            stop = true;
            break;
          }else if(retval != sizeof(bh)){
            printf("Unexpected b_h read size (%d, expected %zu)\n", retval, sizeof(bh)); fflush(stdout);
            stop = true;
            break;
          }else if(retval > 0){ //check block size
            block_len = (ntohs((bh).type_and_len) & 0x1FFF); //LHE_BLOCKHEADER_GET_LEN(*bh);
            if(block_len == 0){ //new frame coming
              //wait for all threads to finish
                for(i = 0; i < THREAD_N; i++){
                  pthread_join(some_threads[i], NULL);
                }

              //interpol 
                if(chroma_mode==CHROMA_MODE_YUV420 && blk_height==1) {
                  //interpol
                  interpol_yuv440(frame_buffer,frame_buffer);
                  yuv2rgb(frame_buffer,frame_buffer);
                }

#if TIMING
                clock_gettime(clk_id, &frame_decoded); //time after decoding
#endif /*TIMING*/

              //print frame
                cv::imshow("LHE_Player",frame_buffer);
                cv::waitKey(1); //Window is not actually shown until waitkey is called. Doesn't actually wait 1ms. Closes if ESC is pressed.
        
#if TIMING
                clock_gettime(clk_id, &frame_displayed); //time after displaying
                fprintf(fout, "%lld\t%ld\t%lld\t%ld\t%lld\t%ld\n", (long long)frame_in.tv_sec, frame_in.tv_nsec, (long long)frame_decoded.tv_sec, frame_decoded.tv_nsec, (long long)frame_displayed.tv_sec, frame_displayed.tv_nsec);
#endif /*TIMING*/

                next_thread = 1; //activate join thread protection 
                startup = true;
                eof = true; //end of frame
                break; //back to read frame loop

            }else if(block_len > MAX_BLOCK_LEN){
              printf("\tUnexpected block size %zu\n", block_len);
              stop = true;
              break;
            }else{ //read block
              retval = read(fin, &block_n, sizeof(uint16_t)); //read block number
              if(retval < 0){ //error
                perror("read"); fflush(stderr);
                stop = true;
                break;
              }
              retval = read(fin, lhe_block_data, block_len); //read block
              if(retval < 0){ //error
                perror("read"); fflush(stderr);
                stop = true;
                break;
              }else if(retval != block_len){
                printf("\tUnexpected block read size (%d, expected %zu)\n", retval, block_len); fflush(stdout);
                stop = true;
                break;
              }else{
                //DECODE BLOCK
                //get block coordinates
                  blk_row = block_n / LHE_PACKETHEADER_GET_IM_W(ph);
                  blk_col = block_n % LHE_PACKETHEADER_GET_IM_W(ph);

                  //wait thread before re-launching it, but don't wait for a thread that hasn't ever been launched
                    if(!startup){
                      pthread_join(some_threads[next_thread], NULL);
                    }else if(next_thread == 0){
                      startup = false;
                    }

                  //update thread's arguments
                    cv::Mat block=frame_buffer(cv::Range(blk_row*blk_height,(blk_row+1)*blk_height),
                                               cv::Range(blk_col*blk_width,(blk_col+1)*blk_width));
                    args[next_thread].blk_row = blk_row; 
                    args[next_thread].blk_height = blk_height; 
                    args[next_thread].chroma_mode = chroma_mode; 
                    args[next_thread].codec_mode = codec_mode; 
                    memcpy(args[next_thread].lhe_block_data, lhe_block_data, MAX_BLOCK_LEN);
                    args[next_thread].block = block;
                    //args[next_thread].block_n = block_n; //only for debugging, not necessary

                  //launch thread
                    if(0 != pthread_create(&some_threads[next_thread], NULL, &decode_block, (void *)&args[next_thread])){ 
                      perror("threads");
                    }

                  //update round robin thread counter
                    next_thread++;
                    next_thread = next_thread%THREAD_N;

              } //DECODE BLOCK
            } //read block
          } //block header
        }//READ BLOCK LOOP
      } //getting new frame...
    } //READ FRAME LOOP

    //free resources
      frame_buffer.release();
      if(-1 == close(fin)){
        perror("closing pipe"); fflush(stderr);
        return -1;
      }
      if(-1 == unlink(d_in)){ // tell OS to delete named pipe when the other process closes it
        perror("unlink pipe"); fflush(stderr);
      } 

  return 1;
}

/*
int test_server(const char *d_in){
  int retval;
  int fin;

  char str1[80];
  int i = 0;

  //Set handler for SIGINT
    struct sigaction act;
    act.sa_handler = sigint_handler;
    sigemptyset (&act.sa_mask);
    act.sa_flags = 0;
    sigaction(SIGINT, &act, NULL);

  //Create pipe
    unlink(d_in);
    retval = mkfifo(d_in, 0666); //create named pipe : mkfifo(<pathname>, <permission>)
    if(retval == -1){
      perror("mkfifo");
      return -1;
    }

    fin = open(d_in, O_WRONLY);
    if(fin == -1){
      perror("opening pipe");
      return -1;
    }

    while(!stop){
      sprintf(str1, "%d\n", i);
      i++;
      retval = write(fin, str1, 80);
    }

    if(-1 == close(fin)){
      perror("closing pipe");
      //break;
      return -1;
    }
    if(-1 == unlink(d_in)){ // tell OS to delete named pipe when the other process closes it
      perror("unlink pipe");
    } 

  return 1;
}

int test_client(const char *d_in){
  int retval;
  int fin;

  char str1[80];

  //Set handler for SIGINT
    struct sigaction act;
    act.sa_handler = sigint_handler;
    sigemptyset (&act.sa_mask);
    act.sa_flags = 0;
    sigaction(SIGINT, &act, NULL);

    fin = open(d_in, O_RDONLY);
    if(fin == -1){
      perror("opening pipe");
      return -1;
    }

    while(!stop){
      retval = read(fin, str1, 80);
      printf("%s", str1);
    }

    if(-1 == close(fin)){
      perror("closing pipe");
      return -1;
    }
    if(-1 == unlink(d_in)){ // tell OS to delete named pipe when the other process closes it
      perror("unlink pipe");
    } 


  return 1;
}
*/

int main(int argc, char** argv ){
  const char *d_in = NULL;
  const char *d_out = NULL;

  if(argc<2 || argc>4){
    std::cout<<"Wrong number of arguments (need --help ?)"<<std::endl;
    usage();
    return 1;
  }

  if(strcmp(argv[1], "--help") == 0 || strcmp(argv[1], "-h") == 0) { //help menu
      help();
  } else if (strcmp(argv[1], "--decode") == 0 || strcmp(argv[1], "-d") == 0) { //decodes .lhe files
      if(argc<4){
        fprintf(stderr, "Missing mandatory arguments after \"%s\":\n\tLHEplayer %s [INPUT_DIRECTORY] [OUTPUT_DIRECTORY]\n", argv[1], argv[1]);
        return -1;
      } else {
        d_in = argv[2];
        d_out = argv[3];
        
        return decode(d_in, d_out);
      }
  } else if (strcmp(argv[1], "--play") == 0|| strcmp(argv[1], "-p") == 0) { //plays video
      if(argc<3){
      fprintf(stderr, "Missing mandatory arguments after \"%s\":\n\tLHEplayer %s [INPUT_DIRECTORY]\n", argv[1], argv[1]);
      return -1;
    } else {
      d_in = argv[2];
              
      return play(d_in);
    }
  } else if (strcmp(argv[1], "--step") == 0|| strcmp(argv[1], "-s") == 0) { //plays video frame by frame
      if(argc<3){
      fprintf(stderr, "Missing mandatory arguments after \"%s\":\n\tLHEplayer %s [INPUT_DIRECTORY]\n", argv[1], argv[1]);
      return -1;
    } else {
      d_in = argv[2];
              
      return step(d_in);
    }
  } else if (strcmp(argv[1], "--play-fstream") == 0|| strcmp(argv[1], "-pfs") == 0) { //plays file stream
      if(argc<3){
      fprintf(stderr, "Missing mandatory arguments after \"%s\":\n\tLHEplayer %s [INPUT_DIRECTORY]\n", argv[1], argv[1]);
      return -1;
    } else {
      d_in = argv[2];
              
      return play_fstream(d_in);
      //return test_server(d_in);
    }
  } else if (strcmp(argv[1], "--play-pstream") == 0|| strcmp(argv[1], "-pps") == 0) { //plays pipe stream
      if(argc<3){
      fprintf(stderr, "Missing mandatory arguments after \"%s\":\n\tLHEplayer %s [INPUT_PIPE]\n", argv[1], argv[1]);
      return -1;
    } else {
      d_in = argv[2];
              
      return play_pstream(d_in);
      //return test_client(d_in);
    }
  } else {
      fprintf(stderr, "Unrecognized parameter: \"%s\". (need --help ?)\n", argv[1]);
      return -1;
  }  

}