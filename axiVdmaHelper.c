#include "axiVdmaHelper.h"

XAxiVdma vdma;

int check_vdma_status() {
    u32 status = XAxiVdma_ReadReg(vdma.BaseAddr, XAXIVDMA_SR_OFFSET + XAXIVDMA_RX_OFFSET);

    // Extract the frame count using the mask
    u32 frame_count = (status & XAXIVDMA_FRMCNT_MASK) >> XAXIVDMA_FRMCNT_SHIFT;

    return frame_count;
}

int check_axi_vdma_configuration(XAxiVdma *vdma) {
    u32 status = XAxiVdma_ReadReg(vdma->BaseAddr, XAXIVDMA_SR_OFFSET + XAXIVDMA_RX_OFFSET);

    // Check if VDMA is halted
    if (status & XAXIVDMA_SR_HALTED_MASK) {
        xil_printf("Error: VDMA is halted.\n");
        return XST_FAILURE;
    }

    // Check if VDMA is idle
    if ((status & XAXIVDMA_SR_IDLE_MASK)) {
        xil_printf("Error: VDMA is idle, not running.\n");
        return XST_FAILURE;
    }

    // Check for errors in the VDMA
     if (status & XAXIVDMA_SR_ERR_ALL_MASK) {
         xil_printf("Error: VDMA has errors (SR = 0x%x).\n", status);
         // will print EOL error and interrupt error probbably due to mismatch in timing(clock missmatch)
     }

    xil_printf("AXI VDMA configuration is correct.\n");
    return XST_SUCCESS;
}

void init_axi_vdma() {
    XAxiVdma_Config *vdma_config;
    int status;

    // Initialize AXI VDMA
    vdma_config = XAxiVdma_LookupConfig(VDMA_DEVICE_ID);
    if (!vdma_config) {
        // Handle configuration error
        xil_printf("Error: Could not find VDMA configuration.\n");
        return;
    }

    status = XAxiVdma_CfgInitialize(&vdma, vdma_config, vdma_config->BaseAddress);
    if (status != XST_SUCCESS) {
        // Handle initialization errSor
        xil_printf("Error: VDMA initialization failed with status %d.\n", status);
        return;
    }

    // reset AXI_VDMA
    XAxiVdma_WriteReg(vdma.BaseAddr, XAXIVDMA_RX_OFFSET, XAXIVDMA_CR_RESET_MASK);

    // wait for reset to finish
    while((XAxiVdma_ReadReg(vdma.BaseAddr, XAXIVDMA_RX_OFFSET) & XAXIVDMA_CR_RESET_MASK)==4);

    // Do not mask interrupts
    XAxiVdma_WriteReg(vdma.BaseAddr, XAXIVDMA_S2MM_DMA_IRQ_MASK_OFFSET, 0xf);

    XAxiVdma_WriteReg(vdma.BaseAddr, XAXIVDMA_RX_OFFSET,
    	(NUM_FRAMES << 16) |
		XAXIVDMA_CR_RUNSTOP_MASK |
		XAXIVDMA_CR_SYNC_EN_MASK |
		XAXIVDMA_CR_GENLCK_SRC_MASK |
		XAXIVDMA_CR_TAIL_EN_MASK);

    // wait for start
    while((XAxiVdma_ReadReg(vdma.BaseAddr, XAXIVDMA_RX_OFFSET) & XAXIVDMA_CR_RUNSTOP_MASK)==0 || (XAxiVdma_ReadReg(vdma.BaseAddr, XAXIVDMA_RX_OFFSET + XAXIVDMA_SR_OFFSET) & XAXIVDMA_SR_HALTED_MASK)==1);

    // Configure frame buffer addresses
    XAxiVdma_SetFrmStore(&vdma, NUM_FRAMES, XAXIVDMA_WRITE);
    XAxiVdma_WriteReg(vdma.BaseAddr, S2MM_REG_INDEX, 0);


    // Set the buffer addresses, assuming contiguous physical memory
    XAxiVdma_WriteReg(vdma.BaseAddr, S2MM_START_ADDRESS, FRAME_BUFFER1_ADDR);
    XAxiVdma_WriteReg(vdma.BaseAddr, S2MM_START_ADDRESS + 4, FRAME_BUFFER2_ADDR);
    XAxiVdma_WriteReg(vdma.BaseAddr, S2MM_START_ADDRESS + 8, FRAME_BUFFER3_ADDR);

    // Configure AXI VDMA for triple buffering
    XAxiVdma_WriteReg(vdma.BaseAddr, XAXIVDMA_FRMSTORE_OFFSET, NUM_FRAMES - 1);

    // Write Park pointer register
    XAxiVdma_WriteReg(vdma.BaseAddr, XAXIVDMA_PARKPTR_OFFSET, 0);

    // Frame delay and stride (bytes)
    XAxiVdma_WriteReg(vdma.BaseAddr, OFFSET_VDMA_S2MM_FRMDLY_STRIDE, FRAME_WIDTH * 2);


    // Set frame dimensions
    XAxiVdma_WriteReg(vdma.BaseAddr, XAXIVDMA_S2MM_ADDR_OFFSET + XAXIVDMA_HSIZE_OFFSET, FRAME_WIDTH * 2);
    XAxiVdma_WriteReg(vdma.BaseAddr, XAXIVDMA_S2MM_ADDR_OFFSET + XAXIVDMA_VSIZE_OFFSET, FRAME_HEIGHT);


    // Print success message
    xil_printf("VDMA initialized successfully.\n");
    // Check AXI VDMA configuration
    status = check_axi_vdma_configuration(&vdma);
    if (status != XST_SUCCESS) {
        // Handle configuration error
        return;
    }
}
