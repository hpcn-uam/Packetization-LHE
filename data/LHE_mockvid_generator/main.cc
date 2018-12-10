/*
 * lhe_enc_dec.cc
 *
 *  Created on: May 17, 2017
 *      Author: Angel Lopez
 */

#include <opencv2/imgproc/imgproc.hpp>
#include <opencv2/imgcodecs.hpp>
#include <opencv2/highgui.hpp>

#include <iostream>
#include <fstream>
#include <algorithm>

#include <dirent.h>

#include "LHE_codec/src/lhe_codec.h"

//#define CHROMA_MODE_GRAY 0
//#define CHROMA_MODE_YUV420 1
//#define CHROMA_MODE_YUV422 2
//#define CHROMA_MODE_YUV444 3
//#define CHROMA_MODE_BAYER 4


int filter_files(const struct dirent *entry){ //filter for scandir
  if(entry->d_type == DT_REG || entry->d_type == DT_UNKNOWN && strcmp(entry->d_name,".") == 1 && strcmp(entry->d_name,"..") == 1) //filters out everything but files
    return 1;
  return 0;
}

int main(int arg, char** argv)
{
  int n, i = 0;
  int block_width, block_height;
  struct dirent **namelist;
  char filename[1000];
  char outputname[1000];
  const char *f = NULL;
  char yuv_profile = 4;
  
  cv::Mat img_orig;

  if(arg<6){
      std::cout<<"Not enough arguments: input_directory output_directory block_width block_height yuv_profile"<<std::endl;
      return 1;
  }
  
  n = scandir(argv[1], &namelist, filter_files, alphasort); //n = # of files
  if(n<1){
      std::cout<<"No files for decoding!"<<std::endl;
      return 1;
  }
  block_width = atoi(argv[3]);
  block_height = atoi(argv[4]);
  yuv_profile = atoi(argv[5]);
  //std::cout<<"block_width "<<block_width<<"  block_height "<<block_height<<std::endl;
        
  while (i < n) {
    i++;
    /////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////7
    //CAMBIO PORQUE NO SE VE BIEEEEN!!
    /////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////7
    sprintf(filename, "%s/%s", argv[1], namelist[i-1]->d_name);
    
    //sprintf(filename, "%s/%03d.png", argv[1], i);
    //std::cout<<"input file "<<filename<<std::endl;
    f = filename;
    sprintf(outputname, "%s/%05d.lhe", argv[2], i);
    //std::cout<<"output file "<<outputname<<std::endl;
    //printf("in %s\n out %s\n", filename, outputname); 

    //action
    img_orig = cv::imread( f );
    if(img_orig.empty()){
      std::cout<<"empty file"<<std::endl;
      return 2;
    }

    if (lhe_encoder(img_orig, outputname, block_width, block_height, yuv_profile) <= 0)    {
      std::cerr<<"there's been an error in trying to encode the image"<<std::endl;
    }
    //


    free(namelist[i-1]);
  }
  free(namelist);


  return 0;
}