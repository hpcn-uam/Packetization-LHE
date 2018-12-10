/*
 * lhe_codec.cc
 *
 *  Created on: Jul 21, 2017
 *      Author: Tobi
 */

#include "lhe_codec.h"

void interpol_yuv440(const cv::Mat& src,cv::Mat&  dst){

  for (int row = 0; row < src.rows; row+=2){
    for (int col = 0; col < src.cols; ++col){
      dst.at<cv::Vec3b>(row+1,col)[1]= src.at<cv::Vec3b>(row,col)[1] ; //u
      dst.at<cv::Vec3b>(row+1,col)[2]= src.at<cv::Vec3b>(row,col)[2] ; //v
    }
  }

}
int lhe_encoder(const cv::Mat& src_img,char* out_file,int block_width,int block_height, char chroma_mode){

  //input validation
    if(block_height>16) {
      std::cerr<<"lhe_encoder: "<<block_height
            <<" it's not a supported blk height. It must be smaller or equal to 256"<<std::endl;
      return -1;
    }

    if(block_width<1 || block_width%16!=0) { // the upper bound is 65536 
      std::cerr<<"lhe_encoder: "<<block_width
            <<" it's not a supported blk width. It must be higher than 0 and 16 must be a factor of it"<<std::endl;
      return -1;
    }

    //get number of image blocks
      uint16_t blk_rows,blk_cols; //image height and width in number of blocks

      if(src_img.rows%block_height==0){
        blk_rows=src_img.rows/block_height;
      }else{
        std::cerr<<"lhe_encoder: unsupported image height("<<src_img.rows<<"). It must be multiple of block height ("
                 <<block_height<<")"<<std::endl;
        return -2;
      }

      if(src_img.cols%block_width==0){
        blk_cols=src_img.cols/block_width;
      }else{
        std::cerr<<"lhe_encoder :unsupported image width("<<src_img.cols<<"). It must be multiple of block width ("
                 <<block_width<<")"<<std::endl;
        return -2;
      }

    
  //save file header
    struct lhe_global_header header;
    header.profile= chroma_mode;
    header.reserved=0 ; // it really doesn't matter
    header.audio=0 ;
    header.version=1 ;
    header.num_blocks =0 ; //blk_rows*blk_cols doesn't fit for full image          
    header.blk_height = block_height-1;
    header.blk_width = block_width/16 -1;
    header.img_height_h =blk_rows>>8; 
    header.img_height_l =blk_rows&0xFF; 
    header.img_width_h = blk_cols>>8;
    header.img_width_l = blk_cols&0xFF;

    std::ofstream lhe_out_file(out_file,std::ios::binary); //out file
    lhe_out_file.write((char*)&(header),sizeof(header));

  //generate blocks
    cv::Mat block;

    int compress_img_size=0;
    uint32_t* block_buffer;
    uint32_t max_output_size=((block_height*block_width*3)/(sizeof (*block_buffer))+1); //for no chroma sub-sampling
    block_buffer = new uint32_t[max_output_size];

    for (uint16_t blk_row = 0; blk_row < blk_rows; ++blk_row) {
      for (uint16_t blk_col = 0; blk_col < blk_cols; ++blk_col) {
        block=src_img(cv::Range(blk_row*block_height,(blk_row+1)*block_height),
                      cv::Range(blk_col*block_width,(blk_col+1)*block_width)); //map a portion of the input image to a block
        uint32_t lhe_file_out_size;
        if(block_height==1 && chroma_mode==CHROMA_MODE_YUV420){
          if (blk_row%2!=0) {
            lhe_file_out_size=lhe_encode_core(block,block_buffer,CHROMA_MODE_GRAY);
          }else{
            lhe_file_out_size=lhe_encode_core(block,block_buffer,CHROMA_MODE_YUV422);
          }
        }else{
         lhe_file_out_size=lhe_encode_core(block,block_buffer,chroma_mode);
        }

        compress_img_size+= (int)lhe_file_out_size;
        //store block header
          struct lhe_block_header block_header;
          block_header.padding = 0;
          block_header.size_h = (lhe_file_out_size)>>8;
          block_header.size_l = (lhe_file_out_size)&0xFF;
          lhe_out_file.write((char*)&(block_header),sizeof(block_header));

        lhe_out_file.write((char*)block_buffer,lhe_file_out_size); //store block data
      }
    }


    lhe_out_file.close();
    delete[] block_buffer;
    return compress_img_size;
}

int lhe_decoder(char* in_file,cv::Mat &dst_img){
  //open lhe-coded image
    std::ifstream lhe_in_file(in_file, std::ios::binary );

  //extract file header
    struct lhe_global_header header;
    lhe_in_file.read((char*)&header,sizeof(header));

    uint32_t cols,rows,blk_rows,blk_cols,blk_height,blk_width;

    blk_rows=header.img_height_h<<8 | header.img_height_l;
    blk_cols=header.img_width_h<<8  | header.img_width_l;
    blk_height=header.blk_height+1;
    blk_width=(header.blk_width+1)*16;
    rows=blk_rows*blk_height;
    cols=blk_cols*blk_width;
    char chroma_mode = (char)header.profile;


  //variable initiation for decoder loop
    char* lhe_block_data;
    uint32_t max_block_data_size=(blk_height*blk_width*3);
    lhe_block_data = new char[max_block_data_size];

  //loop to decode every block
    if (header.profile==CHROMA_MODE_GRAY){
      dst_img=cv::Mat::zeros(rows,cols,CV_MAKETYPE(CV_8U,1));
    }else{
      dst_img=cv::Mat::zeros(rows,cols,CV_MAKETYPE(CV_8U,3));
    }
    
    cv::Mat block;
    char codec_mode= (chroma_mode==CHROMA_MODE_YUV420 && blk_height==1)? 1 : 0;
    

    for (uint16_t blk_row = 0; blk_row < blk_rows; ++blk_row) {
      for (uint16_t blk_col = 0; blk_col < blk_cols; ++blk_col) {
        //map a range of full image to block (they share image data)
          block=dst_img(cv::Range(blk_row*blk_height,(blk_row+1)*blk_height),
                           cv::Range(blk_col*blk_width,(blk_col+1)*blk_width));

        //get block id and length of the generated binary
          struct lhe_block_header block_header;
          lhe_in_file.read((char*)&(block_header),sizeof(block_header));


        //get lhe binary
          lhe_in_file.read(lhe_block_data, block_header.size_h<<8|block_header.size_l);

        if(blk_height==1 && chroma_mode==CHROMA_MODE_YUV420 && blk_row%2!=0) {
          lhe_decode_core((unsigned char*)lhe_block_data,block,CHROMA_MODE_GRAY,codec_mode);
        }else{
          lhe_decode_core((unsigned char*)lhe_block_data,block,chroma_mode,codec_mode);
        }

      }
    }

    if(chroma_mode==CHROMA_MODE_YUV420 && blk_height==1) {
      //interpol
      interpol_yuv440(dst_img,dst_img);
      yuv2rgb(dst_img,dst_img);
    }

  delete[] lhe_block_data;
  return 0;
}
