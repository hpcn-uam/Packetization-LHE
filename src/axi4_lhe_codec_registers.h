#ifndef _LHE_CODEC_REGISTERS_
#define _LHE_CODEC_REGISTERS_

#define LHE_SETTINGS_BASEADDR                                    0x43C00000
#define LHE_SETTINGS_HIGHADDR                                    0x43C0FFFF

#define LHE_SETTINGS_DATA_CHROMA_SUBSAMPLIG                      0x00
#define LHE_SETTINGS_RESOLUTION                                  0x04
#define LHE_SETTINGS_QUALITY_LEVEL                               0x0C
#define LHE_SETTINGS_GOP                                         0x10
#define LHE_SETTINGS_BLOCK_SIZE                                  0x14
#define LHE_SETTINGS_NUMBER_BLOCK                                0x18
#define LHE_SETTINGS_MAX_NUM_OF_BLOCKS                           0x1C
#define LHE_SETTINGS_BYTES_RESERVED_PER_BLOCK_HW                 0x20
#define LHE_SETTINGS_SOFTWARE_AVAILABLE                          0x24
#define LHE_SETTINGS_PTR_HW                                      0x28
#define LHE_SETTINGS_PTR_SW                                      0x2C
#define LHE_SETTINGS_RELATIVE_FPS                                0x30
#define LHE_SETTINGS_PTR_VALID_BLOCK                             0x34
#define LHE_SETTINGS_NUM_OF_ERROR_TRANS                          0x38
#define LHE_SETTINGS_SOFTWARE_UPDATING                           0x3C
#define LHE_SETTINGS_TIME_COUNTER_LSB                            0x50
#define LHE_SETTINGS_TIME_COUNTER_MSB                            0x54
#define LHE_SETTINGS_STATE_WRITE_MNG                             0x6C
#define LHE_SETTINGS_STATE_DATA_CHECKER                          0x70
#define LHE_SETTINGS_RESET                                       0x78
#define LHE_SETTINGS_VERSION                                     0x7C

#define IRQ_MASK                                                 0x1

#define LHE_SETTINGS_RANGE                                       LHE_SETTINGS_HIGHADDR - LHE_SETTINGS_BASEADDR

#define NUMBER_BLOCKS                                            900

#define CHROMA_SUB_400                                           0
#define CHROMA_SUB_420                                           1
#define CHROMA_SUB_422                                           2

/*  The max block size is a very importan variable because defines
    the amount of memory needed for each block.

    It is calculate as follow (in the worst scenario 4:4:4) 
    block of 128x8 pixels 1024= .

    64 bits for the headers.
    24 bits for the absulute values of the first pixel
    three_components * longest_huffman * amount_remaining_pixels
    3 * 7 * 1023 = 21483 bits

    Result  
        - 21571 bits
        - 2696.375 bytes
        - 337.04 words of 64 bits

        **** 338 words of 64 bits****
*/        
 
#define MAX_BLOCK_SIZE                      170
/*
 * LHE coding image size in bytes
*/        
#endif