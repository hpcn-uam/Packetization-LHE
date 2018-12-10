/*
 * lhe_codec.h
 *
 *  Created on: May 22, 2017
 *      Author: tobi
 */


#ifndef LHE_CODEC_CORE_H
#define LHE_CODEC_CORE_H

#include <opencv2/imgproc/imgproc.hpp>
#include <opencv2/imgcodecs.hpp>
#include <opencv2/highgui.hpp>

#include <stdint.h>
#include <iostream>
#include <vector>
#include <algorithm>
#include <functional> 
#include <cmath>
#include <cstring> //memcopy

#define DEBUG_ENC false
#define DEBUG_DEC false

#define MIN_HOP1 4
#define MAX_HOP1 10
#define INIT_HOP1 ((MIN_HOP1+MAX_HOP1)/2)

//typedef unsigned char hop_t;

static const unsigned char huff_tree[9][2]={
	#include "../tables/huff_tree.txt"
};
  #define HOP_CODE 0
	#define HOP_LENGTH 1
	#define LARGEST_CODE 7

static const unsigned char quant_values[256][MAX_HOP1-MIN_HOP1+1][8]={
	#include "../tables/quant_values.txt"
};
	#define HOP_n4 0
	#define HOP_n3 1
	#define HOP_n2 2
	#define HOP_n1 3
	#define HOP_0  8
	#define HOP_1  4
	#define HOP_2  5
	#define HOP_3  6
	#define HOP_4  7

static const unsigned char quant_ranges[256][MAX_HOP1-MIN_HOP1+1][8]={
	#include "../tables/quant_ranges.txt"
};
		#define RANGE_n4_n3	0 
	#define RANGE_n3_n2	1
	#define RANGE_n2_n1	2
	#define RANGE_n1_0	3
	#define RANGE_0_1	  4
	#define RANGE_1_2	  5
	#define RANGE_2_3	  6
	#define RANGE_3_4	  7

#define CHROMA_MODE_GRAY 0
#define CHROMA_MODE_YUV420 1
#define CHROMA_MODE_YUV422 2
#define CHROMA_MODE_YUV444 3
#define CHROMA_MODE_BAYER 4

namespace lhe{ 
	std::string get_chroma_mode_name(char id);
	};



uint32_t lhe_encode_core(const cv::Mat& src,uint32_t* lhe_file,char chroma_mode=CHROMA_MODE_YUV444,char mode=0);

void lhe_decode_core(unsigned char* lhe_in_file ,cv::Mat& decode_img,char chroma_mode=CHROMA_MODE_YUV444, char mode =0);

void rgb2yuv(const cv::Mat& src,cv::Mat&  dst,char chroma_mode =CHROMA_MODE_YUV444);

void yuv2rgb(const cv::Mat src, cv::Mat& dst,char chroma_mode =CHROMA_MODE_YUV444);

float prediction_efficacy(uint32_t file_size, int rows, int cols, int channels=1);

float prediction_efficacy(uint32_t *file_size,int rows, int cols, char chroma_mode=CHROMA_MODE_GRAY);


#endif /* LHE_CODEC_CORE_H */


