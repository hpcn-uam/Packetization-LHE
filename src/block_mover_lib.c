#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <unistd.h>     // for sleep function
#include <sys/mman.h>   // for mmap
#include <fcntl.h>      // For open function
#include <inttypes.h>

#include "block_mover_lib.h"

#define  BLK_WR_BEFORE_RD   0

/**
 * Global pointer to the LHE Global Header
 */

//static uint32_t * volatile ptr_copy_data;  
static uint32_t * ptr_copy_data;  
//static uint32_t * volatile ptr_lhe_codec_registers;
static uint32_t * ptr_lhe_codec_registers;
static uint32_t number_blocks,new_number_blocks;  // number_blocks is update to new_number_blocks @ the start of frame
static int32_t number_of_available_blocks=0;
static uint32_t starting_hw_ptr;
static uint8_t  flag=0;

static uint16_t current_frame = 1;
static uint16_t current_block = 0;
const uint32_t page_size_copy_data = 8 * 1024 * 1024;

static uint8_t open_flags_r = 0;
/**
* @brief This function opens the device. If there is not driver it will open the /dev/mem.
*        The open_flags are used to map differents memory region such as: LHE_PARAMETERS and
 *        IMAGEN_BUFFER. Only one programme should open IMAGEN_BUFFER  
*/
int open_device(uint8_t open_flags){
  #ifndef UIO_DRIVER
  /* Open /dev/mem file */
    int fd;
    fd = open ("/dev/mem", O_RDWR);
    if (fd < 1) {
        perror("Error: opening /dev/mem");
        return -1;
    }
  #else   
    // TODO uio driver
  #endif

  if (open_flags & OPEN_LHE_PARAMETERS_MASK){
    open_LHE_parameters_device(fd);
  }

  if (open_flags & OPEN_IMAGE_BUFFER_MASK){
    open_imagen_buffer_device(fd);

    lhe_parameters_s DEF_codec_config= {DEF_CHROMA_SUBSAMPLING,DEF_IMAGE_WIDTH,DEF_IMAGE_HEIGHT,
                                        DEF_BLOCK_WIDTH,DEF_BLOCK_HEIGHT,DEF_RELATIVE_FPS,DEF_NUMBER_OF_BLOCKS };
    set_parameters(&DEF_codec_config);
  }

  open_flags_r = open_flags;
  return 0;

}

int open_LHE_parameters_device(int fd){
  #ifndef UIO_DRIVER 

    /* mmap AXI4-Lite register into a memory */
    uint32_t page_addr_LHE_SETTINGS = (LHE_SETTINGS_BASEADDR & (~(LHE_SETTINGS_RANGE-1)));
    #ifdef DEBUG
      printf("Data Mover Settings\n");
      printf("Page size datamover    :   %d Bytes\n",LHE_SETTINGS_RANGE);
      printf("Page address datamover :   0x%X\n",page_addr_LHE_SETTINGS);
    #endif  

    ptr_lhe_codec_registers = mmap(NULL, LHE_SETTINGS_RANGE, PROT_READ|PROT_WRITE, MAP_SHARED, fd, page_addr_LHE_SETTINGS);
  #else   
    // TODO uio driver
  #endif 
  return 0;  
}

int open_imagen_buffer_device(int fd){
  #ifndef UIO_DRIVER

    /* mmap the device into memory */ 

    uint32_t page_addr_copy_data = (DATA_PHY_ADDRESS & (~(page_size_copy_data-1)));

    #ifdef DEBUG
      printf("Reading data\n");
      printf("Page size copy data    :   0x%X\n",page_size_copy_data);
      printf("Page address copy data :   0x%X\n",page_addr_copy_data);
    #endif 

    ptr_copy_data = mmap(NULL, page_size_copy_data, PROT_READ|PROT_WRITE, MAP_SHARED, fd, page_addr_copy_data);

  #else   
    // TODO uio driver
  #endif 

  return 0;
}


void close_device(void){
  number_of_available_blocks=0;

  if (open_flags_r & OPEN_LHE_PARAMETERS_MASK){
    munmap(ptr_lhe_codec_registers  , LHE_SETTINGS_RANGE);
  }
  if (open_flags_r & OPEN_IMAGE_BUFFER_MASK){
    munmap(ptr_copy_data            , page_size_copy_data);
  }
}


void reset_hardware(void){
    set_register_not_check(1,LHE_SETTINGS_RESET);
}


void start_video_streaming(void){
  number_of_available_blocks=0;

  starting_hw_ptr       = get_register(LHE_SETTINGS_PTR_HW);
  uint16_t hw_frame_ptr =((starting_hw_ptr>>16)&0xffff);

  //update sw pointers
  current_frame = hw_frame_ptr +1;
  current_block = 0;
  uint32_t sw_ptr = (((uint32_t)current_frame)<< 16) & 0xFFFF0000 ;
  set_register(sw_ptr,LHE_SETTINGS_PTR_SW);

  #ifdef DEBUG
    printf("Lib: start_streaming: starting_hw_ptr: %" PRIu32 ",sw_ptr: %" PRIu32 ", ",starting_hw_ptr,sw_ptr);

    uint32_t starting_hw_ptr_valid = get_register(LHE_SETTINGS_PTR_VALID_BLOCK);
    printf("   valid pointer: %" PRIu32 "\n",starting_hw_ptr_valid);

  #endif

  set_register(1,LHE_SETTINGS_SOFTWARE_AVAILABLE); //start streaming
 
}


void stop_video_streaming(void){
  number_of_available_blocks=0;

  set_register(0,LHE_SETTINGS_SOFTWARE_AVAILABLE);
}


void get_core_version(uint16_t *core_version){
    *core_version = (uint16_t) get_register(LHE_SETTINGS_VERSION);
}


int get_lhe_global_header(struct lhe_global_header_hw *lhe_global_header){
  static int processed_frames_cnt=0;

  // check in there is a new frame or if there are lost frames
    int32_t hw_ptr = get_register(LHE_SETTINGS_PTR_VALID_BLOCK);
    int16_t hw_write_frame = (hw_ptr >> 16) & 0xFFFF;                      // current frame copied
    int16_t hw_write_block = hw_ptr & 0xFFFF;                      // current frame copied

  #ifdef DEBUG
    static int print_flag = 1;

    //if(print_flag) {
      printf("LIB: GET_LGH -> hw_chk_frame : %" PRIu16 ",hw_chk_block: %" PRIu16 ", ",
                                                                                  hw_write_frame , hw_write_block  );
      printf(",   sw_frame: %" PRIu16 ",sw_block: %" PRIu16 ", ",current_frame, current_block);
      printf("\t current time: %" PRIu64 "\n",get_hw_timer());
      print_flag=0;

      uint32_t starting_hw_ptr_valid = get_register(LHE_SETTINGS_PTR_HW);
      printf("   hw_nochk: %" PRIu32 "",starting_hw_ptr_valid);

      uint32_t state_writer = get_register(LHE_SETTINGS_STATE_WRITE_MNG);
      printf("   state_writer: %" PRIu32 "",state_writer);
      uint32_t state_checker = get_register(LHE_SETTINGS_STATE_DATA_CHECKER);
      printf("   state_checker: %" PRIu32 "\n",state_checker);
    //}
      
  #endif

  int frame_ptr_diff = hw_write_frame - current_frame;
  frame_ptr_diff = frame_ptr_diff<-1? frame_ptr_diff+ 0x10000 :frame_ptr_diff; // check if there is an hw_ptr overflow

  if( (hw_write_block > (BLK_WR_BEFORE_RD + 1) && frame_ptr_diff ==0) || frame_ptr_diff > 0){
    if(frame_ptr_diff > 1) { 

      #ifdef DEBUG
        printf(" Lib: get_lhe_global_header : Frames Droped !! current_frame: %" PRIu16 " , hw_write_frame: %" PRIu16 " , "
                                ,current_frame,hw_write_frame  );
        printf("\t current time: %" PRIu64 "\n",get_hw_timer());
      #endif

      processed_frames_cnt+=frame_ptr_diff;
      current_frame = hw_write_frame /*+1*/;
      uint32_t sw_ptr = (((uint32_t)current_frame)<< 16) & 0xFFFF0000;
      set_register(sw_ptr, LHE_SETTINGS_PTR_SW);
      return -2;
    }
   
  }else{
    return 0;
  }

  // This point is only reached if frame_ptr_diff is "0" or "1"

  #ifdef DEBUG
    print_flag = 1;
    printf("### New GH ### \t current time: %" PRIu64 "\n",get_hw_timer());
  #endif

  uint16_t aux_height;
  uint16_t aux_width;


  number_of_available_blocks= frame_ptr_diff ==0? hw_write_block:number_blocks; 
  
  *lhe_global_header = (*(struct lhe_global_header_hw*) ptr_copy_data);         // cast the global header to the software structure

  aux_height = ((lhe_global_header->img_height_h << 8) & 0XFF00) + lhe_global_header->img_height_l;
  aux_width  = ((lhe_global_header->img_width_h << 8) & 0XFF00) + lhe_global_header->img_width_l;

  current_block      = 0;                // reset current_block
  number_blocks = aux_height * aux_width;


  int num_of_frame_processed_by_hw= processed_frames_cnt +1;
  processed_frames_cnt=0;
  return num_of_frame_processed_by_hw;
}

int get_next_block(uint32_t **ptr_next_data, struct lhe_local_header_hw *lhe_local_header ,uint16_t *block_number, uint16_t *frame_number){

  uint32_t hw_ptr;
  uint16_t hw_block_ptr;
  uint16_t hw_frame_ptr;
  uint32_t sw_ptr;

  //check if there is new data
    if(number_of_available_blocks<1) {
      hw_ptr = get_register(LHE_SETTINGS_PTR_VALID_BLOCK);
      hw_block_ptr = hw_ptr & 0xffff;
      hw_frame_ptr = (hw_ptr>>16)&0xffff;

      int frame_ptr_diff = hw_frame_ptr - current_frame;
      frame_ptr_diff = frame_ptr_diff<-1? frame_ptr_diff+ 0x10000 :frame_ptr_diff; // check if there is an hw_ptr overflow
      number_of_available_blocks = frame_ptr_diff > 0 ? number_blocks - current_block : hw_block_ptr - current_block;

 
      #ifdef DEBUG
        sw_ptr = ((((int32_t)current_frame)<< 16) & 0xFFFF0000) + current_block;
        printf("Lib: get_next_block :  hw_frame_ptr: %" PRIu16 ",hw_block_ptr: %" PRIu16 "",(hw_ptr>>16)&0xffff,hw_block_ptr  );
        printf(", current_frame: %" PRIu16 ", current_block: %" PRIu16 ",",current_frame,current_block  );
        printf(",  hw_ptr: %" PRIu16 ", sw_ptr: %" PRIu16 ",",hw_ptr,sw_ptr  );
        printf("\t current time: %" PRIu64 "\n",get_hw_timer());
      #endif

      if(number_of_available_blocks <= 0 ) {//sw is up to date
        return -1;
      }
    }
  

  number_of_available_blocks--;


  //return data
  *lhe_local_header = (*(struct lhe_local_header_hw*) (ptr_copy_data + LHE_GLOBAL_HEADER_32_SIZE + MAX_BLOCK_SIZE_32*current_block));
  *ptr_next_data    = ptr_copy_data + LHE_LOCAL_HEADER_32_SIZE + LHE_GLOBAL_HEADER_32_SIZE + MAX_BLOCK_SIZE_32*current_block; // Assign the correspondent block
  *block_number     = current_block;      
  *frame_number     = current_frame;


  //update current_frame and current_block
  if (current_block == number_blocks - 1) {
      current_block = 0;
      current_frame++;
  } else{
      current_block++;
  }

  //update sw pointer reg
  sw_ptr = ((((int32_t)current_frame)<< 16) & 0xFFFF0000) + current_block;
  set_register(sw_ptr,LHE_SETTINGS_PTR_SW);
  // *(ptr_lhe_codec_registers+LHE_SETTINGS_PTR_SW/4) = sw_ptr;
  return 0;


}

/*
 * Return error codes
 * -1 : Chroma subsamplig not support
 * -2 : Image width is bigger than allowed
 * -3 : Image height is bigger than allowed
 * -4 : Block width is bigger than allowed OR is not module of 16
 * -5 : Block height is bigger than allowed or is (no 1 and no CHROMA_SUB_400)
 * -6 : Number of columns is not integer
 * -7 : Number of rows is not integer
 * -8 : Number of pixel per block not supported
 * -9 : Number of blocks not supported
 */

int set_parameters(lhe_parameters_s *current_parameters){
    // Check if the parameters are inside the range 
    uint32_t block_resolution;
    uint32_t pixel_resolution;
    uint32_t block_columns;
    uint32_t block_rows;
    uint32_t max_number_block_hw;

    if (current_parameters->chroma_subsampling > CHROMA_SUB_422) {
      fprintf(stderr, "ERROR: Chroma subsamplig not support %d\n",current_parameters->chroma_subsampling);
      return -1;
    }

    if(current_parameters->image_width > MAX_IMAGE_WIDTH) {
      fprintf(stderr, "ERROR: Image width is bigger than allowed %u < %u\n",MAX_IMAGE_WIDTH,current_parameters->image_width);
      return -2;
    }

    if(current_parameters->image_height > MAX_IMAGE_HEIGHT) {
      fprintf(stderr, "ERROR: Image height is bigger than allowed %u < %u\n",MAX_IMAGE_HEIGHT,current_parameters->image_height);
      return -3;
    }

    if((current_parameters->block_width % 16 != 0) &&
       (current_parameters->block_width <= MAX_BLOCK_WIDTH)){
      fprintf(stderr, "ERROR: Block width (%" PRIu16 ") is bigger than allowed (%" PRIu16 ") OR is not module of 16 (remm = %" PRIu16 ")\n", current_parameters->block_width, MAX_BLOCK_WIDTH, (current_parameters->block_width % 16));
      return -4;
    }

    if(current_parameters->block_height > MAX_BLOCK_HEIGHT){
      fprintf(stderr, "ERROR: Block height is bigger than allowed %u < %u\n",MAX_BLOCK_WIDTH,current_parameters->block_width);
      return -5;
    }

    if ((current_parameters->block_height > 1) && (current_parameters->block_height % 2 != 0) && current_parameters->chroma_subsampling != CHROMA_SUB_400) {
      fprintf(stderr, "ERROR: Block height can not be odd for this chroma subsamplig\n");
      return -5;
    }

    if (current_parameters->image_width % current_parameters->block_width !=0) {
      fprintf(stderr, "ERROR: Number of columns is not integer %.2f\n", (current_parameters->image_width / (float)current_parameters->block_width));
      return -6;
    }

    if (current_parameters->image_height % current_parameters->block_height !=0) {
      fprintf(stderr, "ERROR: Number of rows is not integer %.2f\n", (current_parameters->image_height / (float) current_parameters->block_height));
      return -7;
    }    

    if (current_parameters->block_width * current_parameters->block_height > MAX_NUMBER_PIXEL_PER_BLOCK) {
      fprintf(stderr, "ERROR: Number of pixels per block not supported %d MAX=%d\n", current_parameters->block_width * current_parameters->block_height,MAX_NUMBER_PIXEL_PER_BLOCK);
      return -8;
    }


    block_columns = (current_parameters->image_width  /  current_parameters->block_width);
    block_rows    = (current_parameters->image_height / current_parameters->block_height);

    new_number_blocks       = block_columns * block_rows;
    max_number_block_hw = get_register(LHE_SETTINGS_MAX_NUM_OF_BLOCKS);

    if (new_number_blocks > max_number_block_hw){
      fprintf(stderr, "ERROR: Number of blocks (%" PRIu32 ") not supported. Block supported %" PRIu32 "\n",new_number_blocks ,max_number_block_hw);
      return -9;
    }

    block_resolution = ((((uint32_t) block_rows) << 16) & 0xFFFF0000) + block_columns;
    pixel_resolution = ((((uint32_t) current_parameters->block_height) << 16) & 0xFFFF0000) + current_parameters->block_width;

    /* Write the information to the hardware */
    set_register(1, LHE_SETTINGS_SOFTWARE_UPDATING); // Set register to keep old settings in hardware

    set_register( new_number_blocks                                 , LHE_SETTINGS_NUMBER_BLOCK);
    set_register( ((uint32_t)current_parameters->chroma_subsampling), LHE_SETTINGS_DATA_CHROMA_SUBSAMPLIG);
    set_register( block_resolution                                  , LHE_SETTINGS_RESOLUTION);
    set_register( pixel_resolution                                  , LHE_SETTINGS_BLOCK_SIZE);
    set_register( ((uint32_t)current_parameters->relative_fps)      , LHE_SETTINGS_RELATIVE_FPS);

    set_register(0 , LHE_SETTINGS_SOFTWARE_UPDATING); // Clear register to update settings in hardware

    current_parameters->number_of_blocks = new_number_blocks; // return the number of blocks computed

    return 0;
}


int check_register_address(unsigned int reg_addr){
    if ((reg_addr % 4) !=0){
        fprintf(stderr, "ERROR: Register address is not 32-bits aligned (%.2f). Address must be multiple of 4\n",(reg_addr/4.0));	
        fprintf(stderr, "In your case it could be %d or %d\n",(reg_addr/4)*4, ((reg_addr/4)+1)*4);	
    	return -1;
    }

    return 0;
}
/**
 * @brief      Gets the parameters.
 *
 * @param      current_parameters  The current parameters
 *
 * @return     The parameters.
 */

int set_register(uint32_t new_value,unsigned int reg_addr ){	
    if (check_register_address(reg_addr) != 0) {
	return -1;
    }    

    *(ptr_lhe_codec_registers + reg_addr /4)   = new_value;

    if(*(ptr_lhe_codec_registers + reg_addr /4)  != new_value) {
        return -2;
    }
    return 0;
}

int set_register_not_check(uint32_t new_value,unsigned int reg_addr ){
    if (check_register_address(reg_addr) != 0) {
         return -1;
    }    

    *(ptr_lhe_codec_registers + reg_addr /4)   = new_value;

    return 0;
}

uint32_t get_register(unsigned int reg_addr){
    return *(ptr_lhe_codec_registers + reg_addr/4);
}

int get_parameters(lhe_parameters_s *hardware_parameters){

    uint32_t image_resolution;
    uint32_t block_resolution;

    image_resolution              = get_register(             LHE_SETTINGS_RESOLUTION);
    block_resolution              = get_register(             LHE_SETTINGS_BLOCK_SIZE);
    
    hardware_parameters->chroma_subsampling  = (uint8_t) get_register( LHE_SETTINGS_DATA_CHROMA_SUBSAMPLIG);
    hardware_parameters->relative_fps        = (uint8_t) get_register(             LHE_SETTINGS_RELATIVE_FPS);
    
    hardware_parameters->block_width         = (uint16_t) block_resolution & 0xFFFF;
    hardware_parameters->block_height        = (uint16_t)((block_resolution >> 16) & 0xFFFF);

    hardware_parameters->image_width         = (uint16_t) ((image_resolution & 0xFFFF) * hardware_parameters->block_width);
    hardware_parameters->image_height        = (uint16_t) (((image_resolution >> 16) & 0xFFFF) * hardware_parameters->block_height);

}


int get_codec_state() {

  uint32_t resolution    = get_register(LHE_SETTINGS_RESOLUTION);
  uint32_t block_size    = get_register(LHE_SETTINGS_BLOCK_SIZE);
  uint32_t core_ver      = get_register(LHE_SETTINGS_VERSION);
  uint32_t hw_pointer    = get_register(LHE_SETTINGS_PTR_HW);
  uint32_t sw_pointer    = get_register(LHE_SETTINGS_PTR_SW);
  uint32_t valid_pointer = get_register(LHE_SETTINGS_PTR_VALID_BLOCK);

    
  printf(" LHE_SETTINGS_DATA_CHROMA_SUBSAMPLIG  %3d -> %d \n",
                  LHE_SETTINGS_DATA_CHROMA_SUBSAMPLIG, get_register(LHE_SETTINGS_DATA_CHROMA_SUBSAMPLIG));
  printf(" LHE_SETTINGS_RESOLUTION              %3d -> blocks height: %3d | width: %3d \n",
                  LHE_SETTINGS_RESOLUTION, resolution >> 16 , resolution&0xFFFF);
//  printf(" LHE_SETTINGS_QUALITY_LEVEL           %3d -> %d \n", 
//                  LHE_SETTINGS_QUALITY_LEVEL,get_register(LHE_SETTINGS_QUALITY_LEVEL));
//  printf(" LHE_SETTINGS_GOP                     %3d -> %d \n",
//                  LHE_SETTINGS_GOP,get_register(LHE_SETTINGS_GOP));
  printf(" LHE_SETTINGS_BLOCK_SIZE              %3d -> pixels height: %3d | width: %3d \n",
                  LHE_SETTINGS_BLOCK_SIZE , block_size>>16 , block_size&0xFFFF);
  printf(" LHE_SETTINGS_NUMBER_BLOCK            %3d -> %d \n",
                  LHE_SETTINGS_NUMBER_BLOCK,get_register(LHE_SETTINGS_NUMBER_BLOCK));
  printf(" LHE_SETTINGS_MAX_NUM_OF_BLOCKS       %3d -> %d \n",
                  LHE_SETTINGS_MAX_NUM_OF_BLOCKS,get_register(LHE_SETTINGS_MAX_NUM_OF_BLOCKS));
  printf(" LHE_SETTINGS_SOFTWARE_AVAILABLE      %3d -> %d \n",
                  LHE_SETTINGS_SOFTWARE_AVAILABLE,get_register(LHE_SETTINGS_SOFTWARE_AVAILABLE));
  printf(" LHE_SETTINGS_PTR_HW                  %3d -> f:%" PRIu16 " blk:%" PRIu16 " \n",
                  LHE_SETTINGS_PTR_HW, hw_pointer>>16 , hw_pointer&0xFFFF);
  printf(" LHE_SETTINGS_PTR_SW                  %3d -> f:%" PRIu16 " blk:%" PRIu16 " \n",
                  LHE_SETTINGS_PTR_SW, sw_pointer>>16, sw_pointer&0xFFFF);
  printf(" LHE_SETTINGS_RELATIVE_FPS            %3d -> %d \n",
                  LHE_SETTINGS_RELATIVE_FPS,get_register(LHE_SETTINGS_RELATIVE_FPS));
  printf(" LHE_SETTINGS_PTR_VALID_BLOCK         %3d -> f:%" PRIu16 " blk:%" PRIu16 " \n",
                  LHE_SETTINGS_PTR_VALID_BLOCK ,valid_pointer>>16 ,valid_pointer&0xFFFF);
  printf(" LHE_SETTINGS_NUM_OF_ERROR_TRANS      %3d -> %u \n",
                  LHE_SETTINGS_NUM_OF_ERROR_TRANS,get_register(LHE_SETTINGS_NUM_OF_ERROR_TRANS));
  printf(" LHE_SETTINGS_TIME_COUNTER_LSB        %3d -> %u \n",
                  LHE_SETTINGS_TIME_COUNTER_LSB,get_register(LHE_SETTINGS_TIME_COUNTER_LSB));
  printf(" LHE_SETTINGS_STATE_WRITE_MNG         %3d -> %d \n",
                  LHE_SETTINGS_STATE_WRITE_MNG,get_register(LHE_SETTINGS_STATE_WRITE_MNG));
  printf(" LHE_SETTINGS_STATE_DATA_CHECKER      %3d -> %d \n",
                  LHE_SETTINGS_STATE_DATA_CHECKER,get_register(LHE_SETTINGS_STATE_DATA_CHECKER));
  printf(" LHE_SETTINGS_RESET                   %3d -> %d \n",
                  LHE_SETTINGS_RESET,get_register(LHE_SETTINGS_RESET));
  printf(" LHE_SETTINGS_VERSION                 %3d -> %2d.%d \n",
                  LHE_SETTINGS_VERSION,core_ver >> 8 , core_ver & 0xFF);
  return 0;
}

uint64_t get_hw_timer (){

  uint64_t hw_time;
  uint32_t hw_time_lsb;
  uint32_t hw_time_msb;

  hw_time_msb = get_register(LHE_SETTINGS_TIME_COUNTER_MSB);
  hw_time_lsb = get_register(LHE_SETTINGS_TIME_COUNTER_LSB);

  hw_time = ((((uint64_t) (hw_time_msb)) << 32) & 0xFFFF0000) +  hw_time_lsb;

  return hw_time;
}
