/*
 * @author: Angel Lopez
 */

 /*This original version of the LHE player has way less code duplication, but has poorer performance.*/

#include <algorithm>
#include <dirent.h>
#include <fstream>
#include <inttypes.h>
#include <iostream>
#include <opencv2/highgui.hpp>
#include <opencv2/imgcodecs.hpp>
#include <opencv2/imgproc/imgproc.hpp>
#include <signal.h>
#include <string.h>
#include <unistd.h>
#include <time.h>
#include "LHE_codec/src/lhe_codec.h"

#define __STDC_FORMAT_MACROS 1
#define A_BUFFER_SIZE 1 //player will wait until there are A_BUFFER_SIZE files in dir to start playing
#define MAX_LATENCY 3 //in purge mode, all files in dir will be deleted if there are more than MAX_LATENCY files

static
volatile bool stop = false;

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

int main(int argc, char** argv )
{
  uint64_t n, i = 0, read = 0, j = 1, k = 0;
  struct dirent **namelist;
  char filename_in[1000];
  char filename_out[1000];
  const char *filename;

  const char *d_in = NULL;
  const char *d_out = NULL;

  int key = 0;

  bool play = false;
  bool decode = false;
  bool write = false;
  bool step = false;
  bool keep_open = false;
  bool purge = false;

  bool showname = false;
  int showname_c = 0;
  int showname_r = 255;
  int showname_g = 0;
  int showname_b = 0;

  uint64_t current_ts, previous_ts = 0; //RTP TS on names
  clockid_t clk_id = CLOCK_MONOTONIC;
  struct timespec previous, current, dif1, dif2, unit;
  
  cv::Mat decode_img;

  previous.tv_sec = 0;
  previous.tv_nsec = 0;
  dif1.tv_sec = 0;
  dif1.tv_nsec = 0;
  dif2.tv_sec = 0;
  dif2.tv_nsec = 0;

  //90000 Hz unit
  unit.tv_sec = 0;
  unit.tv_nsec = 11111;

#ifdef TIMING
  char timing_filename[1000];
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

  if(argc<2){
      std::cout<<"Not enough args (need --help ?)"<<std::endl;
      return 1;
  }

  while (*++argv != NULL) {
    argc--;
    if(strcmp(*argv, "--help") == 0) { //help menu
      printf("\nOPTIONS:\n");
      printf("    --help\n\tDisplay this help.\n\n");
      printf("    --decode INPUT_DIRECTORY\n\t Decodes all .lhe files in input directory.\n\n");
      printf("    --play\n\t Plays decoded video as it is decoded.\n\n");
      printf("    --step\n\t Plays decoded video frame by frame (any key to advance).\n\n");
      printf("    --write OUTPUT_DIRECTORY\n\t Writes all decoded frames as .png files in output directory.\n\n");
      printf("    --keepopen \n\t Leaves player open waiting for new frames\n\n");
      printf("    --purge \n\t Deletes frame after decoding it, and keeps a max latency of %d frames.\n\n", MAX_LATENCY);

    } else if (strcmp(*argv, "--decode") == 0) { //decodes .lhe files
      if(argc<=0){
        fprintf(stderr, "Missing mandatory argument after --decode (output directory)\n");
        exit(-1);
      } else {
        decode = true;
        d_in = *++argv;
      }
    } else if (strcmp(*argv, "--play") == 0) { //plays video
        play = true;
        printf("\n\tControls:\n");
        printf("\t  Exit : esc\n\n");
    } else if(strcmp(*argv, "--step") == 0) { //plays video frame-by-frame
        step = true;
        printf("\n\tControls:\n");
        printf("\t  Exit : esc\n");
        printf("\t  Print filename : up arrow\n");
        printf("\t  Show/hide filename on screen : down arrow\n");
        printf("\t  Toggle font color : spacebar\n");
        printf("\t  Go back one frame : left arrow\n");
        printf("\t  Advance one frame : any other key\n\n");
    } else if (strcmp(*argv, "--write") == 0) { //writes decoded frames as .png files
      if(argc<=0){
        fprintf(stderr, "Missing mandatory argument after --write (output directory)\n");
        exit(-1);
      }else{
        write = true;
        d_out = *++argv;
      }
    } else if (strcmp(*argv, "--keepopen") == 0) { //plays video
        keep_open = true;
    } else if (strcmp(*argv, "--purge") == 0) { //deletes frames after decoding them
        purge = true;
    } else {
      fprintf(stderr, "Unrecognized parameter: \"%s\". (need --help ?)\n", *argv);
      return -1;
    }
  }

  if(play && step){
    fprintf(stderr, "Can't play and step at same time!\n");
    return 0;
  }

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

  if(decode){
    /* Set handler for SIGINT */
    struct sigaction act;

    act.sa_handler = sigint_handler;
    sigemptyset (&act.sa_mask);
    act.sa_flags = 0;
    sigaction(SIGINT, &act, NULL);

    if(play){
      //open show window
      cvNamedWindow("LHE_Player", CV_WINDOW_AUTOSIZE);
      cv::waitKey(1); //waits 1ms so window doesn't close
    }

    while (!stop) { //keeps waiting for new frames if keepopen
      if(stop){
        break;
      }

        n = scandir(d_in, &namelist, filter_files, alphasort); //n = # of files
        //printf("%s : %" PRIu64 " files\n", d_in, n);

        if(27 == cv::waitKey(1)){ //closes if ESC
          stop = true;
          break;
        }
        if(n){
          if(purge && n > MAX_LATENCY){ //mega purge
            //printf("\tMegapurge:\t%d\n", (int)n);
            for(i = 0; i < n-1; i++){
              sprintf(filename_in, "%s/%s", d_in, namelist[i]->d_name);
              filename = filename_in;
              if(unlink(filename)){
                std::cerr<<"Error: couldn't delete image"<<std::endl;
                stop = true;
              }
            }
            continue;
          }
        }

        //if ((keep_open && (read + A_BUFFER_SIZE) < n) || (!keep_open && read < n)){ //if there are more frames than frames read
        if ((read + A_BUFFER_SIZE) < n){ 
          i = 0;
          while ((i + read) < n && !stop){ //reads only unread frames
            if(stop){
              break;
            }
            //printf("i:\t%010" PRIu64 "\tread:\t%010" PRIu64 "\tn:\t%010" PRIu64 "\t\n", i, read, n);
            sprintf(filename_in, "%s/%s", d_in, namelist[(i + read)]->d_name);
            filename = filename_in;
            //printf("Decoding file: %s ...\n", filename_in);
            decode_img.release();

#if TIMING
          clock_gettime(clk_id, &frame_in);
#endif /*TIMING*/

            if (lhe_decoder(filename_in,decode_img))
            {
              std::cerr<<"there's been an error in trying to decode the image"<<std::endl;
            }else{
#if TIMING
              clock_gettime(clk_id, &frame_decoded);
#endif /*TIMING*/
              if(play || step){
                if(step && showname){
                  cv::putText(decode_img, namelist[(i + read)]->d_name, cvPoint(30,30), cv::FONT_HERSHEY_COMPLEX_SMALL, 1.0, cvScalar(showname_r,showname_g,showname_b), 1, CV_AA);
                }
                cv::imshow("LHE_Player",decode_img);
#if TIMING
                clock_gettime(clk_id, &frame_displayed);
                timespec_diff(&frame_in, &frame_decoded, &decode_time);
                timespec_diff(&frame_decoded, &frame_displayed, &display_time);
                fprintf(fout, "%lld\t%ld\t%lld\t%ld\n", (long long)decode_time.tv_sec, decode_time.tv_nsec, (long long)display_time.tv_sec, display_time.tv_nsec);
#endif /*TIMING*/
              }
              if(write){
                //strcpy(aux, namelist[(i + read)]->d_name);
                //aux[strlen(aux)-4] = '\0'; //dirty way to get the .lhe extension out, but works
                //sprintf(filename_out, "%s/%s.png", d_out, aux);
                //note: this gets original name without .lhe extension, 
                //but ffmpeg takes consecutive filenames, whereas these have a TS step (3000)
            
                //this writes files with consecutive names, for ffmpeg
                sprintf(filename_out, "%s/%010" PRIu64 ".png", d_out, j);
                j++;
                imwrite(filename_out, decode_img);            
              }

            if(step){
                key = cv::waitKey(); //wait for key press
              }else if(play){
                if(27 == cv::waitKey(1)){ //waits 1ms so window doesn't close, closes if ESC
                  stop = true;
                  break;
                }
              }
            }
           
            //wait for FPS
            if(play && !purge){
              current_ts = strtoul(namelist[(i + read)]->d_name, NULL, 10); //get ts of current file
              clock_gettime(clk_id, &current);
              timespec_diff(&previous, &current, &dif1);
              dif2.tv_nsec = (((current_ts - previous_ts) * unit.tv_nsec) - dif1.tv_nsec);
              if(dif2.tv_nsec > 0){
                //printf("%" PRIu64 " ms\tVS\t%.ld ms\t; Waiting for\t%.ld ms ...\n",(current_ts - previous_ts)*11111/1000000,dif1.tv_nsec/1000000, dif2.tv_nsec/1000000);//debug
                nanosleep(&dif2, NULL); //WARNING: implement error control!
              } 
              //current -> previous
              clock_gettime(clk_id, &previous); //start previous timer
              previous_ts = current_ts;
            }

            //printf("keycode: %d\n", key); //debug
            //printf("i: %" PRIu64 "\n", i); //debug
            if(step){
              if(key == 27){ //exit : esc
                stop = true;
                break;
              }else if(key == 82){ //print filename : up arrow
                printf("\t%s\n", filename_in);
              }else if(key == 84){ //show filename : down arrow
                showname = !showname;
              }else if(key == 32){ //change font color : spacebar
                showname_c++;
                switch(showname_c){
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
                  case 4:
                  showname_r = 255;
                  showname_g = 0;
                  showname_b = 0;
                  showname_c = 0;
                  break;
                } 
              }else if(key == 81){ //back : left arrow 
                if(i > 0){
                  i--;  
                }
              }else{
                i++; //new frames read this time
              }
            }else{
              free(namelist[(i + read)]);
              i++; //new frames read this time
            }
  
            if(purge){
              //printf("PURGING FILE %s\n", filename); //debug
              if(unlink(filename_in)){
                std::cerr<<"Error: couldn't delete image"<<std::endl;
                break;
              }
            }
          }

          if(!purge){
            read += i; //total frames read
          }
          
          free(namelist);
          
        }else{
          while (n--) {
            free(namelist[n]);
           }
          free(namelist);
        }
      if(!keep_open){
        printf("\n\tNo more frames.\n\n");
        break; //stop waiting for new frames if keepopen has not been setted
      } 
    }
  }
  decode_img.release();

#if TIMING
  if(NULL != fout){
    fclose(fout);
  }
#endif /*TIMING*/

  return 0;
}
