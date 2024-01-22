#include "axiVdmaRegisters.h"


int vdma_setup(vdma_handle *handle,
			   unsigned int baseAddr,
			   int width,
			   int height,
			   int pixelLength,
			   unsigned int fb1Addr,
			   unsigned int fb2Addr,
			   unsigned int fb3Addr) {
    handle->width=width;
    handle->height=height;
    handle->pixelLength=pixelLength;
    handle->fbLength=pixelLength*width*height;

    handle->vdmaVirtualAddress = (unsigned int *)baseAddr;
    handle->fb1PhysicalAddress = (uint16_t *)fb1Addr;
    handle->fb2PhysicalAddress = (uint16_t *)fb2Addr;
    handle->fb3PhysicalAddress = (uint16_t *)fb3Addr;
    return 0;
}


unsigned int vdma_get(vdma_handle *handle, int num) {
    return handle->vdmaVirtualAddress[num>>2];
}

void vdma_set(vdma_handle *handle, int num, unsigned int val) {
    handle->vdmaVirtualAddress[num>>2]=val;
}

void vdma_start_s2mm(vdma_handle *handle) {
    // Reset VDMA
    vdma_set(handle, OFFSET_VDMA_S2MM_CONTROL_REGISTER, VDMA_CONTROL_REGISTER_RESET);

    // Wait for reset to finish
    while((vdma_get(handle, OFFSET_VDMA_S2MM_CONTROL_REGISTER) & VDMA_CONTROL_REGISTER_RESET)==4);

    // Clear all error bits in status register
    vdma_set(handle, OFFSET_VDMA_S2MM_STATUS_REGISTER, 0);

    // Do not mask interrupts
    vdma_set(handle, OFFSET_VDMA_S2MM_IRQ_MASK, 0xf);

    int interrupt_frame_count = 3;

    // Start up buffer number
    vdma_set(handle, OFFSET_VDMA_S2MM_CONTROL_REGISTER,
        (interrupt_frame_count << 16) |
        VDMA_CONTROL_REGISTER_START |
        VDMA_CONTROL_REGISTER_GENLOCK_ENABLE |
        VDMA_CONTROL_REGISTER_INTERNAL_GENLOCK |
        VDMA_CONTROL_REGISTER_CIRCULAR_PARK);



    while((vdma_get(handle, 0x30)&1)==0 || (vdma_get(handle, 0x34)&1)==1);

    // Extra register index, use first 16 frame pointer registers
    vdma_set(handle, OFFSET_VDMA_S2MM_REG_INDEX, 0);

    // Write physical addresses to control register
    vdma_set(handle, OFFSET_VDMA_S2MM_FRAMEBUFFER1, (unsigned int)handle->fb1PhysicalAddress);
    vdma_set(handle, OFFSET_VDMA_S2MM_FRAMEBUFFER2, (unsigned int)handle->fb2PhysicalAddress);
    vdma_set(handle, OFFSET_VDMA_S2MM_FRAMEBUFFER3, (unsigned int)handle->fb3PhysicalAddress);

    // Write Park pointer register
    vdma_set(handle, OFFSET_PARK_PTR_REG, 0);

    // Frame delay and stride (bytes)
    vdma_set(handle, OFFSET_VDMA_S2MM_FRMDLY_STRIDE, handle->width*handle->pixelLength);

    // Write horizontal size (bytes)
    vdma_set(handle, OFFSET_VDMA_S2MM_HSIZE, handle->width*handle->pixelLength);

    // Write vertical size (lines), this actually starts the transfer
    vdma_set(handle, OFFSET_VDMA_S2MM_VSIZE, handle->height);
    vdma_set(handle, OFFSET_VDMA_S2MM_STATUS_REGISTER, 0);
    sleep(5);
}

int vdma_running(vdma_handle *handle) {
    // Check whether VDMA is running, that is ready to start transfers
    return (vdma_get(handle, 0x34)&1)==1;
}


int vdma_idle(vdma_handle *handle) {
    // Check whtether VDMA is transferring
    return (vdma_get(handle, OFFSET_VDMA_S2MM_STATUS_REGISTER) & VDMA_STATUS_REGISTER_FrameCountInterrupt)!=0;
}

void vdma_status_dump(int status) {
    if (status & VDMA_STATUS_REGISTER_HALTED) xil_printf(" halted"); else xil_printf("running");
    //if (status & VDMA_STATUS_REGISTER_VDMAInternalError) xil_printf(" vdma-internal-error");
    //if (status & VDMA_STATUS_REGISTER_VDMASlaveError) xil_printf(" vdma-slave-error");
    //if (status & VDMA_STATUS_REGISTER_VDMADecodeError) xil_printf(" vdma-decode-error");
    //if (status & VDMA_STATUS_REGISTER_StartOfFrameEarlyError) xil_printf(" start-of-frame-early-error");
    //if (status & VDMA_STATUS_REGISTER_EndOfLineEarlyError) xil_printf(" end-of-line-early-error");
    //if (status & VDMA_STATUS_REGISTER_StartOfFrameLateError) xil_printf(" start-of-frame-late-error");
    if (status & VDMA_STATUS_REGISTER_FrameCountInterrupt) xil_printf(" frame-count-interrupt");
    //if (status & VDMA_STATUS_REGISTER_DelayCountInterrupt) xil_printf(" delay-count-interrupt");
    //if (status & VDMA_STATUS_REGISTER_ErrorInterrupt) xil_printf(" error-interrupt");
    //if (status & VDMA_STATUS_REGISTER_EndOfLineLateError) xil_printf(" end-of-line-late-error");
    xil_printf(" frame-count:%d", (status & VDMA_STATUS_REGISTER_FrameCount) >> 16);
    xil_printf(" delay-count:%d", (status & VDMA_STATUS_REGISTER_DelayCount) >> 24);
    xil_printf("\n");
}

void vdma_s2mm_status_dump(vdma_handle *handle) {
    int status = vdma_get(handle, OFFSET_VDMA_S2MM_STATUS_REGISTER);
    xil_printf("S2MM status register (%08x):", status);
    vdma_status_dump(status);
}
