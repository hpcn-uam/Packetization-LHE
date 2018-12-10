/*
 * lhe_codec.cc
 *
 *  Created on: Jul 28, 2017
 *  Derived from: lhe_encoder.cc and lhe_decoder.cc
 *      Author: tobi
 */

#include "lhe_codec_core.h"
/*
*##################   Codec Utils  ########################
*/

  inline unsigned char clamp(int v){  //cv::saturate_cast<uchar>
    return (unsigned char)((unsigned)v <= 255 ? v : v > 0 ? 255 : 0);
  }

  void rgb2yuv(const cv::Mat& src,cv::Mat&  dst,char chroma_mode){
  	cv::Mat tmp;
  	if(chroma_mode==CHROMA_MODE_GRAY) {
      tmp =cv::Mat::zeros(src.size(),CV_8UC1);
      for (int row = 0; row < src.rows; ++row){
        for (int col = 0; col < src.cols; ++col){
          int32_t r = src.at<cv::Vec3b>(row,col)[2];
          int32_t g = src.at<cv::Vec3b>(row,col)[1];
          int32_t b = src.at<cv::Vec3b>(row,col)[0];

          tmp.at<unsigned char>(row,col)= ((r*76  + g*150  + b*29  +128)>>8)    ; //y
        }

      }
    }else{
      tmp =cv::Mat::zeros(src.size(),src.type());
      for (int row = 0; row < src.rows; ++row){
        for (int col = 0; col < src.cols; ++col){
          int32_t r = src.at<cv::Vec3b>(row,col)[2];
          int32_t g = src.at<cv::Vec3b>(row,col)[1];
          int32_t b = src.at<cv::Vec3b>(row,col)[0];

          tmp.at<cv::Vec3b>(row,col)[0]= ((r*76  + g*150  + b*29  +128)>>8)    ; //y
          tmp.at<cv::Vec3b>(row,col)[1]= ((r*-43 + g*-84  + b*127 +128)>>8)+128; //u
          tmp.at<cv::Vec3b>(row,col)[2]= ((r*127 + g*-106 + b*-21 +128)>>8)+128; //v
        }

      }
    }
    dst=tmp.clone();
  }

  void yuv2rgb(const cv::Mat src, cv::Mat& dst,char chroma_mode){
    //cv::Mat tmp=cv::Mat::zeros(src.size(),src.type());
    int rows=src.rows, cols=src.cols;
    for (int row = 0; row < rows; ++row){
      for (int col = 0; col < cols; ++col){
        int16_t  y = src.at<cv::Vec3b>(row,col)[0];
        int16_t  u = src.at<cv::Vec3b>(row,col)[1]-128;
        int16_t  v = src.at<cv::Vec3b>(row,col)[2]-128;

        dst.at<cv::Vec3b>(row,col)[2]= clamp((y*257 + u     + v*363  +128)/256); //r
        dst.at<cv::Vec3b>(row,col)[1]= clamp((y*257 + u*-89 + v*-183 +128)/256); //g
        dst.at<cv::Vec3b>(row,col)[0]= clamp((y*257 + u*458  +v      +128)/256); //b

        /* approximation of
          tmp.at<cv::Vec3b>(row,col)[2]= clamp(y +    + v*1.13983 );  //r
          tmp.at<cv::Vec3b>(row,col)[1]= clamp(y + u*-.39465 + v*-.5806); //g
          tmp.at<cv::Vec3b>(row,col)[0]= clamp((y + u*2.03211    ));  //b*/

      }

    }
    //tmp.copyTo(dst);
  }



  inline void adapt_hop1(int prev_hop, bool* decrease_if_small_hop,unsigned char* hop1){
    if (prev_hop==HOP_1||prev_hop==HOP_n1 || prev_hop==HOP_0){
      if (*decrease_if_small_hop && *hop1 > MIN_HOP1)
      {
        (*hop1)--;
        *decrease_if_small_hop= false;
      }else{
        *decrease_if_small_hop= true;
      }
    }else{
      *hop1= MAX_HOP1;
      *decrease_if_small_hop= false;
    }
  }

  inline unsigned char& get_value(const cv::Mat &src,int row, int col, int channel){
    return src.data[src.step[0]*row+src.step[1]*col+channel];
  }


  unsigned char lhe_prediction(const cv::Mat quantized_img,int row, int col, int chn,char chroma_mode=CHROMA_MODE_YUV420){
    unsigned char prediction=0;
    int x_jump=1,y_jump=1;
    std::vector<unsigned char> sup_px;

    switch(chroma_mode){
      case CHROMA_MODE_YUV422:
        if(chn!=0) { //if not channel y
          x_jump=2;
        }
        break;

      case CHROMA_MODE_YUV420:
        if(chn!=0) { //if not channel y
          x_jump=2;
          y_jump=2;
        }
        break;

      case CHROMA_MODE_YUV444:
      default: 
        break;
    }
    
    if (row==0 ){
      if (col==0){
        std::cerr<<"lhe_prediction: first pixel it's not predicted"<<std::endl;
      }else{
        prediction= get_value(quantized_img,0,col-x_jump,chn);
      }
    }else{
      if (col==0){
        prediction= get_value(quantized_img,row-y_jump,0,chn);
      }else if (col==quantized_img.cols-x_jump){
        prediction= ( get_value(quantized_img,row,col-x_jump,chn) +
                      get_value(quantized_img,row-y_jump,col,chn))/2;
      }else{
        prediction= ( get_value(quantized_img,row,col-x_jump,chn) +
                      get_value(quantized_img,row-y_jump,col+x_jump,chn))/2;
      }
    }

    return prediction;
  }


  int get_num_of_symbs(int rows, int cols, char chroma_mode){
    int num_symb;
    switch(chroma_mode){
      case CHROMA_MODE_YUV422:
        if(cols%2!=0) {
          std::cerr<<" For this chroma mode the number of columns should be even" <<std::endl;
          num_symb=rows*cols + 2*rows*(cols/2+1) ;
        }else{
          num_symb=rows*cols*2;  
        }
        break;

      case CHROMA_MODE_YUV420:
          

        if(cols%2!=0) {
          if(rows!=1) { //this height is supported cause lhe_encoder takes this into account
            std::cerr<<" For this chroma mode the number of columns should be even" <<std::endl;
          }

          if(rows%2!=0) {
            if(rows!=1){ //better than throwing it to NULL?
              std::cerr<<" For this chroma mode the number of rows should be even" <<std::endl;
            }
            num_symb=rows*cols + (rows/2+1)*(cols/2+1) ;
          }else{
            num_symb=rows*cols + rows*(cols/2+1) ;
          }
        }else{
          if(rows%2!=0) {
            if(rows!=1){ //better than throwing it to NULL?
              std::cerr<<" For this chroma mode the number of rows should be even" <<std::endl;
            }
            num_symb= rows*cols + cols*(rows/2+1);
          }else{
            num_symb= rows*cols*1.5;
          }
        }

        break;
      default:
    	 std::cout<<" Not a valid chroma mode. Switching to YUV444"<<std::endl;
    	 /* no break */
      case CHROMA_MODE_YUV444:
        num_symb=rows*cols*3;
        break;
      case CHROMA_MODE_GRAY:
        num_symb=rows*cols;
        break;
    }

    return num_symb;
  }
/*
*##################   Encoder  ########################
*/

  int choose_quant_value(int prediction,unsigned char pixel,int h1, unsigned char* dst ){ //can be optimized for FPGA
    int h1_idx=h1-MIN_HOP1;
    if (pixel>=quant_ranges[prediction][h1_idx][RANGE_n1_0]){
      if (pixel<=quant_ranges[prediction][h1_idx][RANGE_0_1]){
        *dst=HOP_0;
        return prediction;
      }else{
        if (pixel<=quant_ranges[prediction][h1_idx][RANGE_1_2]){
          *dst= HOP_1;
          return quant_values[prediction][h1_idx][HOP_1];
        }else{
          if (pixel<=quant_ranges[prediction][h1_idx][RANGE_2_3]){
            *dst= HOP_2;
            return quant_values[prediction][h1_idx][HOP_2];
          }else{
            if (pixel<=quant_ranges[prediction][h1_idx][RANGE_3_4]){
              *dst= HOP_3;
              return quant_values[prediction][h1_idx][HOP_3];
            }else{
              *dst= HOP_4;
              return quant_values[prediction][h1_idx][HOP_4];
            }
          }
        }
      }
    }else{
      if (pixel>=quant_ranges[prediction][h1_idx][RANGE_n2_n1]){
        *dst= HOP_n1;
        return quant_values[prediction][h1_idx][HOP_n1];
      }else{
        if (pixel>=quant_ranges[prediction][h1_idx][RANGE_n3_n2]){
          *dst= HOP_n2;
          return quant_values[prediction][h1_idx][HOP_n2];
        }else{
          if (pixel>=quant_ranges[prediction][h1_idx][RANGE_n4_n3]){
            *dst= HOP_n3;
            return quant_values[prediction][h1_idx][HOP_n3];
          }else{
            *dst= HOP_n4;
            return quant_values[prediction][h1_idx][HOP_n4];
          }
        }
      } 
    }
  }


  void LHEquantizer(const cv::Mat& src,unsigned char* dst, char chroma_mode){
    unsigned char prediction, *hop1;
    bool *decrease_if_small_hop;
    cv::Mat quantized_img(src.size(),src.type());

    //variable init
      int num_chn= src.channels();
      hop1 = new unsigned char[num_chn];
      decrease_if_small_hop = new bool[num_chn];

    int dst_idx=0;
    for (int row = 0; row < src.rows; ++row){
      for (int col = 0; col < src.cols; ++col){
        for (int chn = 0; chn < num_chn; ++chn){
          unsigned char channel_value =get_value(src,row,col,chn); 

          if (col==0){
            //reset encoder variables
            hop1[chn]=INIT_HOP1;
            decrease_if_small_hop[chn]=false;

            if (row==0)
            {
              dst[dst_idx++] = channel_value;
              quantized_img.data[chn]=channel_value; // just .data[chn] cause row=col=0
              continue;
            }
          }

          switch(chroma_mode){  //chroma decimation
            case CHROMA_MODE_YUV420:
              if ((col%2==1 ||row%2==1)&& (chn>0) ){ //chn=0 --> Y channel
                continue;
              }
              break;

            case CHROMA_MODE_YUV422:
              if (col%2==1 && (chn>0) ){ //chn=0 --> Y channel
                continue;
              }
              break;
            case CHROMA_MODE_YUV444:
            default:
              break;
          }

          prediction=lhe_prediction(quantized_img,row,col,chn,chroma_mode);
          quantized_img.data[quantized_img.step[0]*row+quantized_img.step[1]*col+chn] 
                =choose_quant_value(prediction,channel_value,hop1[chn],&(dst[dst_idx])); 
          adapt_hop1(dst[dst_idx], &decrease_if_small_hop[chn], &hop1[chn]); //actualize encoder variables
          dst_idx++;
        }
      }
      

    }
    
    if(0){
  	  cvNamedWindow("quantized_img",CV_WINDOW_NORMAL);
  	  cv::imshow("quantized_img",quantized_img(cv::Range(7,9),cv::Range(130,136)));


  	  cvNamedWindow("yuv",CV_WINDOW_NORMAL);
  	  cv::imshow("yuv",src(cv::Range(7,9),cv::Range(130,136)));

  	  std::cout<<"type any key to continue"<<std::endl;

  	  char key;
  	  while(key!='q'){
  		 key= cv::waitKey();
  	  }

    }

    delete[] hop1;
    delete[] decrease_if_small_hop;
  }


  int Huffman(unsigned char* src,uint32_t* dst,int num_symb,int num_chn){
    uint32_t bit_ptr=0,aux=0,dst_idx=0;
    dst_idx=0;

    //store first bytes uncoded
      for (int chn = 0; chn < num_chn; ++chn){
        aux |= src[chn]<<bit_ptr;
        bit_ptr+= 8;

        if (bit_ptr==32){
          bit_ptr = 0;
          dst[dst_idx]=aux;
          dst_idx++;
          aux=0;
        }
      }

    //store the rest encoded
      for (int i = num_chn; i <  num_symb; ++i){
        aux |= huff_tree[src[i]][HOP_CODE]<<bit_ptr;
        bit_ptr+= huff_tree[src[i]][HOP_LENGTH];

        if (bit_ptr>=32){
          dst[dst_idx]=aux;
          bit_ptr -= 32;

          if (bit_ptr==0){
            aux=0;
          }else{
            aux= huff_tree[src[i]][0]>> (huff_tree[src[i]][HOP_LENGTH]-bit_ptr);
          }
          dst_idx++;

        }
      }

    //store what's left in the buffer
      int file_size;
      if (bit_ptr>0){
        dst[dst_idx]=aux;
        file_size= dst_idx*4+ ceil(bit_ptr/8.0);
        //file_size=(dst_idx+1)*4; //without padding is: dst_idx*4+ ceil(bit_ptr/8);
      }else{
        file_size=dst_idx*4;
      }

    return file_size; //dst_idx*4+ ceil(bit_ptr/8); //return size in bytes
  }


  uint32_t lhe_encode_core(const cv::Mat& src,uint32_t* lhe_file,char chroma_mode, char mode){

    int num_chn=src.channels(),num_symb;

    num_symb=get_num_of_symbs(src.rows,src.cols,chroma_mode);

    uint32_t lhe_file_size;
    unsigned char* lhe_img = new unsigned char[src.rows*src.cols*num_chn];
    
    if(num_chn==3){ //I assume it's RGB image 
      cv::Mat yuv;
      if(chroma_mode==CHROMA_MODE_GRAY) {
        num_chn=1;
      }
      rgb2yuv(src, yuv,chroma_mode);
      LHEquantizer(yuv,lhe_img,chroma_mode);
    }else{
      LHEquantizer(src,lhe_img,chroma_mode);
    }



    if(mode==1){
      memcpy(lhe_file,lhe_img,sizeof(lhe_img)*num_symb);
      /*for (int i = 0; i < num_symb; ++i){
        lhe_file[i]=uint32_t(lhe_img[i]);
      }*/
      lhe_file_size=num_symb;
    }else{
      lhe_file_size = Huffman(lhe_img,lhe_file,num_symb,num_chn);
    }
    
    #if DEBUG_ENC
      if (num_chn==3){
        cv::namedWindow("yuv_src",CV_WINDOW_NORMAL);
        cv::imshow("yuv_src",yuv_src);

        char* hop_array= new char[num_symb];
        for (int i = 0; i < num_symb; ++i)
        {
          hop_array[i]=(char)lhe_img[i+2];
        }

        cv::Mat hops(src.size(),src.type(),hop_array);
        cvNamedWindow("enc_hops",CV_WINDOW_NORMAL);
        cv::imshow("enc_hops",hops);
        delete[] hop_array;
      }
    #endif

    delete[] lhe_img;

    return lhe_file_size;
  }



/*
*##################   Decoder  ########################
*/

  constexpr int16_t simbol_mask(char num_of_1){
    switch(num_of_1){
      case 0:
        //this makes no sense
        return 0;
      case 1:
        return 0x01 ;

      case 2:
        return 0x03 ;

      case 3:
        return 0x7 ;

      case 4:
        return 0xf ;

      case 5:
        return 0x1f ;

      case 6:
        return  0x3f;

      case 7:
        return  0x7f;

      case 8:
        return 0xff ;

      default:
        //I need longer masks
        return 0;
    }
  }

  //constexpr std::size_t find_largerst_code(){}


  void inv_Huffman(unsigned char* src,unsigned char* dst,int num_symb,int num_chn){
    uint16_t buffer;
    const int MAX_BIT_PTR =(int)((sizeof buffer)*8-LARGEST_CODE);
    int src_idx=0 ; //i do not keep track if i'm out of range,i assume the file is alright
                          //and keeping track of the number of symbols is enough

    for (int chn = 0; chn < num_chn; ++chn)
    {
      dst[chn]= src[src_idx++];
    }

    buffer= src[src_idx+1]<<8 | src[src_idx]; //can have problems in super small images
    src_idx+=2;
    int bit_ptr= 0;
    

    for (int symbol_idx = num_chn; symbol_idx < num_symb; ++symbol_idx)
    {
      if (bit_ptr>MAX_BIT_PTR)
      {
        buffer= buffer>>8 | src[src_idx]<<8;  
        src_idx++;
        bit_ptr-=8;
      }

      uint16_t aligned_buffer= buffer>>bit_ptr;
      if ((aligned_buffer & simbol_mask(huff_tree[HOP_0][HOP_LENGTH])) == huff_tree[HOP_0][HOP_CODE])
      {
        dst[symbol_idx]=HOP_0;
        bit_ptr+=huff_tree[HOP_0][HOP_LENGTH];
      }else{
        if ((aligned_buffer & simbol_mask(huff_tree[HOP_1][HOP_LENGTH])) == huff_tree[HOP_1][HOP_CODE])
        {
          dst[symbol_idx]=HOP_1;
          bit_ptr+=huff_tree[HOP_1][HOP_LENGTH];
        }else{
          if ((aligned_buffer & simbol_mask(huff_tree[HOP_n1][HOP_LENGTH])) == huff_tree[HOP_n1][HOP_CODE])
          {
            dst[symbol_idx]=HOP_n1;
            bit_ptr+=huff_tree[HOP_n1][HOP_LENGTH];
          }else{
            if ((aligned_buffer & simbol_mask(huff_tree[HOP_2][HOP_LENGTH])) == huff_tree[HOP_2][HOP_CODE])
            {
              dst[symbol_idx]=HOP_2;
              bit_ptr+=huff_tree[HOP_2][HOP_LENGTH];
            }else{
              if ((aligned_buffer & simbol_mask(huff_tree[HOP_n2][HOP_LENGTH])) == huff_tree[HOP_n2][HOP_CODE])
              {
                dst[symbol_idx]=HOP_n2;
                bit_ptr+=huff_tree[HOP_n2][HOP_LENGTH];
              }else{
                if ((aligned_buffer & simbol_mask(huff_tree[HOP_3][HOP_LENGTH])) == huff_tree[HOP_3][HOP_CODE])
                {
                  dst[symbol_idx]=HOP_3;
                  bit_ptr+=huff_tree[HOP_3][HOP_LENGTH];
                }else{
                  if ((aligned_buffer & simbol_mask(huff_tree[HOP_n3][HOP_LENGTH])) == huff_tree[HOP_n3][HOP_CODE])
                  {
                    dst[symbol_idx]=HOP_n3;
                    bit_ptr+=huff_tree[HOP_n3][HOP_LENGTH];
                  }else{
                    if ((aligned_buffer & simbol_mask(huff_tree[HOP_4][HOP_LENGTH])) == huff_tree[HOP_4][HOP_CODE])
                    {
                      dst[symbol_idx]=HOP_4;
                      bit_ptr+=huff_tree[HOP_4][HOP_LENGTH];
                    }else{
                      dst[symbol_idx]=HOP_n4;
                      bit_ptr+=huff_tree[HOP_n4][HOP_LENGTH];                   
                    }
                  }
                }
              }
            }
          }
        }
      }
    }
  }


  inline unsigned char Hops2Value(unsigned char hop,unsigned char prediction,char hop1){
    return (hop==HOP_0)?prediction:quant_values[prediction][hop1-MIN_HOP1][hop];
  }


  void LHE_dequant(unsigned char* lhe_img,cv::Mat& decoded_img,char chroma_mode){
    unsigned char prediction,hop1[4];
    bool decrease_if_small_hop[4]; 
    int src_idx=0;
    int num_chn= chroma_mode==CHROMA_MODE_GRAY? 1 : decoded_img.channels();

    //variable init 
      //hop1= new unsigned char[num_chn];
      //decrease_if_small_hop= new bool[num_chn];
      for (int chn = 0; chn < num_chn; ++chn)
      {
        hop1[chn]=INIT_HOP1;
        decrease_if_small_hop[chn]=false;
      }
    
    for (int row = 0; row < decoded_img.rows; ++row){
      for (int col = 0; col < decoded_img.cols; ++col){
        for (int chn = 0; chn < num_chn; ++chn){

          if (col==0 ){
            //reset encoder variables
            hop1[chn]=INIT_HOP1;
            decrease_if_small_hop[chn]=false;

            if (row==0){
              decoded_img.data[chn]=lhe_img[src_idx++]; // just .data[chn] cause row=col=0
              continue;
            }
          } 

           

          switch(chroma_mode){  //chroma interpolation
            case CHROMA_MODE_YUV420:
              if ((col%2==1 ||row%2==1)&& (chn>0) ){ //chn=0 --> Y channel
                if(col%2==1 && row%2==1 ) {
                  decoded_img.data[decoded_img.step[0]*row+decoded_img.step[1]*col+chn]=
                        decoded_img.data[decoded_img.step[0]*(row-1)+decoded_img.step[1]*(col-1)+chn];
                }else if(col%2==1){
                  decoded_img.data[decoded_img.step[0]*row+decoded_img.step[1]*col+chn]=
                        decoded_img.data[decoded_img.step[0]*(row)+decoded_img.step[1]*(col-1)+chn];
                }else{
                  decoded_img.data[decoded_img.step[0]*row+decoded_img.step[1]*col+chn]=
                        decoded_img.data[decoded_img.step[0]*(row-1)+decoded_img.step[1]*(col)+chn];
                }
                continue;
              }
              break;

            case CHROMA_MODE_YUV422:
              if (col%2==1 && (chn>0) ){ //chn=0 --> Y channel
                decoded_img.data[decoded_img.step[0]*row+decoded_img.step[1]*col+chn]=
                      decoded_img.data[decoded_img.step[0]*(row)+decoded_img.step[1]*(col-1)+chn];
                continue;
              }
              break;
            case CHROMA_MODE_YUV444:
            default:
              break;
          }

          prediction=lhe_prediction(decoded_img,row,col,chn,chroma_mode);
          decoded_img.data[decoded_img.step[0]*row+decoded_img.step[1]*col+chn] =
                                  Hops2Value(lhe_img[src_idx], prediction,hop1[chn]);
          adapt_hop1(lhe_img[src_idx], &decrease_if_small_hop[chn], &hop1[chn]);
          src_idx++;  
        }
      }
    }

  //delete[] hop1;
  //delete[] decrease_if_small_hop;

  }


  //chroma_interpol();



  void lhe_decode_core(unsigned char* lhe_in_file ,cv::Mat& decode_img,char chroma_mode, char mode){
    int num_symb=get_num_of_symbs(decode_img.rows,decode_img.cols,chroma_mode);

    int num_chn= chroma_mode == CHROMA_MODE_GRAY? 1:decode_img.channels();
 

    unsigned char* lhe_img = new unsigned char[num_symb];
    inv_Huffman(lhe_in_file,lhe_img,num_symb,num_chn);

    #if DEBUG_DEC
      {
        cv::Mat dec_hops(decode_img.size(),decode_img.type(),lhe_img);
        cv::namedWindow("dec_hops",CV_WINDOW_NORMAL);
        cv::imshow("dec_hops",dec_hops);
      }
    #endif

    if (num_chn==3 && mode!=1) 
    {
      //cv::Mat yuv_img(decode_img.size(), decode_img.type());
      LHE_dequant(lhe_img,decode_img,chroma_mode);
      #if DEBUG_DEC
        {
          cv::namedWindow("yuv",CV_WINDOW_NORMAL);
          cv::imshow("yuv",yuv_img);
        }
      #endif
         
      yuv2rgb(decode_img,decode_img);

    }else{
      LHE_dequant(lhe_img,decode_img,chroma_mode);
    }
    delete[] lhe_img;
  }


/*
*##################   External Utils ########################
*/

  float prediction_efficacy(uint32_t file_size, int rows, int cols, int channels){
    float max_file_size=(8+(rows*cols-1)*7)/8; //in bytes
    float min_file_size=(8+(rows*cols-1))/8;   //in bytes
    return (1-(file_size/channels-min_file_size)/(max_file_size-min_file_size))*100; // file_size/channels is valid only for YUV444
  }

  float prediction_efficacy(uint32_t *file_size,int rows, int cols, char chroma_mode){
    //pre-computations
      const float max_file_size=(8+(rows*cols-1)*7)/8; //in bytes
      const float min_file_size=(8+(rows*cols-1))/8;   //in bytes
      const float size_range= max_file_size-min_file_size;

    float chn_weight[3];
    int channels;
      switch(chroma_mode){
        case CHROMA_MODE_YUV420:
          chn_weight[0]= 4/6.0;
          chn_weight[1]= 1/6.0;
          chn_weight[2]= 1/6.0;

          channels=3;
          break;
        case CHROMA_MODE_YUV444:
          chn_weight[0]= 1/3.0;
          chn_weight[1]= 1/3.0;
          chn_weight[2]= 1/3.0;

          channels=3;
          break;
        case CHROMA_MODE_BAYER:
          chn_weight[0]= 1/4.0;
          chn_weight[1]= 2/4.0;
          chn_weight[2]= 1/4.0;

          channels=3;
          break;
        default:
          std::cerr<<"test: chroma_mode "<<chroma_mode<<" it's not implemented"
                  <<"assuming CHROMA_MODE_GRAY "<<std::endl;
          return -1;

        case CHROMA_MODE_GRAY:
          chn_weight[0]=1;
          channels=1;
          break;
      };
      
      float prediction_efficacy= 0;
      for (int i = 0; i < channels; ++i){
        prediction_efficacy+= chn_weight[i]*(1-(file_size[i]-min_file_size)/size_range);
      }

      return prediction_efficacy*100; 
  }


  std::string lhe::get_chroma_mode_name(char id){
    std::string out_string;
    switch(id){
      case CHROMA_MODE_GRAY:
        out_string= "GRAY";
        break;
      case CHROMA_MODE_YUV420:
        out_string= "YUV420";
        break;
      case CHROMA_MODE_YUV422:
        out_string= "YUV422";
        break;
      case CHROMA_MODE_YUV444:
        out_string= "YUV444";
        break;
      case CHROMA_MODE_BAYER:
        out_string= "BAYER";
        break;
      default:
        out_string= "Unknown";
        std::cerr<<"test_variables_t: Unknown chroma_mode"<<std::endl;
    }
    return out_string;
  }


