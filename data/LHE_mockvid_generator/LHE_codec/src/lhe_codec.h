/*
 * lhe_pack.h
 *
 *  Created on: Jul 21, 2017
 *      Author: Tobi
 */

#ifndef LHE_CODEC_H
#define LHE_CODEC_H

#include "../../LHE_codec_core/src/lhe_codec_core.h"
#include <opencv2/imgproc/imgproc.hpp> //cv::Mat


#include <stdint.h>
#include <iostream>
#include <fstream>

struct lhe_global_header {
  uint8_t profile:4;
  uint8_t reserved:1;
  uint8_t audio:1;
  uint8_t version :2;

  uint8_t num_blocks  :6;
  uint8_t codec: 2;


  uint8_t blk_height; //(block height) -1
  uint8_t blk_width;  //(block width)/16 - 1
  uint8_t img_height_h; 
  uint8_t img_height_l;
  uint8_t img_width_h;
  uint8_t img_width_l;
}__attribute__((packed));


struct lhe_block_header {
  uint8_t size_h :5; //in bytes
  uint8_t padding  :3;

  uint8_t size_l;
}__attribute__((packed));

int lhe_encoder(const cv::Mat& src_img,char* out_file,int block_width=128,int block_height=8, char chroma_samp=0);

int lhe_decoder(char* in_file,cv::Mat &dst_img);

void interpol_yuv440(const cv::Mat& src,cv::Mat&  dst);

#endif /* LHE_CODEC_H */
