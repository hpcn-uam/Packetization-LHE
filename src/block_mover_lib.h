#ifndef _BLOCK_MOVER_LIB_
#define _BLOCK_MOVER_LIB_

#include <stdint.h>
//#include <asm/outercache.h>
#include "axi4_lhe_codec_registers.h"

#define DATA_PHY_ADDRESS 			0x1F800000

#define CHROMA_SUB_400 				0
#define CHROMA_SUB_420 				1
#define CHROMA_SUB_422 				2

#define FPS_15 						0
#define FPS_30 						1
#define FPS_60 						2
#define FPS_90 						3

#define OPEN_LHE_PARAMETERS_MASK    0x1
#define OPEN_IMAGE_BUFFER_MASK      0x2

#define MAX_BLOCK_SIZE              170
#define MAX_BLOCK_SIZE_32           MAX_BLOCK_SIZE*2
#define MAX_BLOCK_BYTES             MAX_BLOCK_SIZE*8

#define MAX_IMAGE_WIDTH  		 	1280
#define MAX_IMAGE_HEIGHT  		 	720
#define MAX_BLOCK_WIDTH  		 	MAX_IMAGE_WIDTH
#define MAX_BLOCK_HEIGHT  		 	16

#define  LHE_GLOBAL_HEADER_32_SIZE  2
#define  LHE_LOCAL_HEADER_32_SIZE   2

#define  MAX_NUMBER_PIXEL_PER_BLOCK 1280

//default codec config
  #define DEF_CHROMA_SUBSAMPLING   (CHROMA_SUB_420)
  #define DEF_IMAGE_WIDTH   1280         // measured in pixels
  #define DEF_IMAGE_HEIGHT   720        // measured in pixels
  #define DEF_BLOCK_WIDTH   128         // number of horizontal pixels in a block
  #define DEF_BLOCK_HEIGHT   8        // number of vertical   pixels in a block
  #define DEF_RELATIVE_FPS   1        // How many input frames are needed to get one valid output frame
  #define DEF_NUMBER_OF_BLOCKS   (DEF_IMAGE_WIDTH)/(DEF_BLOCK_WIDTH)*(DEF_IMAGE_HEIGHT)/(DEF_BLOCK_HEIGHT)

typedef struct lhe_parameters
{
  uint8_t  chroma_subsampling;
  uint16_t image_width;         // measured in pixels
  uint16_t image_height;        // measured in pixels
  uint16_t block_width;         // number of horizontal pixels in a block
  uint16_t block_height;        // number of vertical   pixels in a block
  uint8_t  relative_fps;        // How many input frames are needed to get one valid output frame
  
  uint16_t number_of_blocks;
  /*
  uint32_t quality_level,   // Not used yet
  uint32_t group_of_pixels  // Not used yet differential or not
*/
} lhe_parameters_s;



struct lhe_global_header_hw {     //version 1
  uint8_t profile:4;
  uint8_t fps:4;

  uint8_t reserved;
  uint8_t blk_height; //(block height) -1
  uint8_t blk_width;  //(block width)/16 - 1

  uint8_t img_height_h; 
  uint8_t img_height_l;
  uint8_t img_width_h;
  uint8_t img_width_l;
}__attribute__((packed));

struct lhe_local_header_hw {  //version 1

  uint32_t block_size:13;   // block size
  uint32_t gop:3;       // diferential

  uint32_t unused:16;

  uint32_t timestamp;

}__attribute__((packed));

/**
* @brief This function opens the device. If there is not driver it will open the /dev/mem.
*
*/
int open_device(uint8_t open_flags);

/**
* @brief This function assosiate the memory region of the image.
*
*/
int open_imagen_buffer_device(int fd);

/**
* @brief This function assosiate the memory region of the parameters.
*
*/
int open_LHE_parameters_device(int fd);

/**
* @brief This function closes the device.
*
*/

void close_device(void);

/**
 * @brief      Starts a video streaming.
 */

void start_video_streaming(void);

/**
 * @brief      Stops a video streaming.
 */

void stop_video_streaming(void);

/**
 * @brief      { function_description }
 */

void reset_hardware(void);

/**
 * @brief      { function_description }
 *
 * @param      core_version  The core version
 */
void get_core_version(uint16_t *core_version);

/**
 * @brief      Gets the lhe global header.
 * @param      ptr_next_data  The pointer next data
 *
 * @return     
 */

int get_lhe_global_header(struct lhe_global_header_hw *lhe_global_header);

/**
 * @brief      Gets the next block. Not blocking funtion
 *
 * @param      ptr_next_data  The pointer next data
 *
 * @return     Return operation code.
 * 				0 -> New block available @ pointer data
 * 				1 -> No new block available @ pointer data
 */
int get_next_block(uint32_t **ptr_next_data, struct lhe_local_header_hw *lhe_local_header ,uint16_t *block_number, uint16_t *frame_number);

/**
 * @brief      Sets the parameters.
 *
 * @param[in]  current_parameters  The current parameters
 *
 * @return     { description_of_the_return_value }
 */

int set_parameters(lhe_parameters_s *current_parameters);


int set_register(uint32_t new_value,unsigned int reg_addr);


int set_register_not_check(uint32_t new_value,unsigned int reg_addr );


uint32_t get_register(unsigned int reg_addr);


/**
 * @brief      Gets the parameters.
 *
 * @param      current_parameters  The current parameters
 *
 * @return     The parameters.
 */
int get_parameters(lhe_parameters_s *hardware_parameters);

int get_codec_state() ;

uint64_t get_hw_timer ();

int check_register_address(unsigned int reg_addr);

#endif