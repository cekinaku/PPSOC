#include <xaxivdma.h>
#include <xil_printf.h>
#include "xparameters.h"

#define VDMA_DEVICE_ID        XPAR_AXIVDMA_0_DEVICE_ID
#define FRAME_BUFFER1_ADDR    0x10000000
#define FRAME_BUFFER2_ADDR    0x11000000
#define FRAME_BUFFER3_ADDR    0x12000000
#define FRAME_WIDTH           640
#define FRAME_HEIGHT          480
#define NUM_FRAMES            3
#define S2MM_REG_INDEX		  0x44
#define S2MM_START_ADDRESS    0xac
#define OFFSET_VDMA_S2MM_FRMDLY_STRIDE 0xa8
