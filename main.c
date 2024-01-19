/*
 * Copyright (C) 2009 - 2019 Xilinx, Inc.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without modification,
 * are permitted provided that the following conditions are met:
 *
 * 1. Redistributions of source code must retain the above copyright notice,
 *    this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright notice,
 *    this list of conditions and the following disclaimer in the documentation
 *    and/or other materials provided with the distribution.
 * 3. The name of the author may not be used to endorse or promote products
 *    derived from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR IMPLIED
 * WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT
 * SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT
 * OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING
 * IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY
 * OF SUCH DAMAGE.
 *
 */

#include <stdio.h>

#include "xparameters.h"

#include "netif/xadapter.h"

#include "platform.h"
#include "platform_config.h"
#if defined (__arm__) || defined(__aarch64__)
#include "xil_printf.h"
#endif

#include "lwip/tcp.h"
#include "xil_cache.h"
#include "axiVdmaHelper.h"


// Assuming RGB565 format
typedef uint16_t pixel_t;

/* defined by each RAW mode application */
void print_app_header();
int start_application();
int transfer_data();
void lwip_initialization();
void init_ov7670();
void init_axi_vdma();
struct tcp_pcb* getGlobalPcb();
struct netif* getGlobalEchoNetif();
int check_vdma_status();
void process_frame(pixel_t* input_buffer);

/* missing declaration in lwIP */
void lwip_init();

#if defined (__arm__) && !defined (ARMR5)
#if XPAR_GIGE_PCS_PMA_SGMII_CORE_PRESENT == 1 || XPAR_GIGE_PCS_PMA_1000BASEX_CORE_PRESENT == 1
int ProgramSi5324(void);
int ProgramSfpPhy(void);
#endif
#endif

#ifdef XPS_BOARD_ZCU102
#ifdef XPAR_XIICPS_0_DEVICE_ID
int IicPhyReset(void);
#endif
#endif

int main()
{

	lwip_initialization();

	init_ov7670();

	init_axi_vdma();

	/* start the application (web server, rxtest, txtest, etc..) */
	start_application();

	struct tcp_pcb* pcb;

	struct netif* echo_netif = getGlobalEchoNetif();
	if (echo_netif == NULL) {
		xil_printf("\nFailed to get global netif exiting\n");
	}

	int isConnected = 0;
	while (isConnected < 1) {
		xemacif_input(echo_netif);
		pcb = getGlobalPcb();
		if (pcb->state == ESTABLISHED) {
			xil_printf("Conn established\n");
			isConnected = 1;
		}
	}

	pixel_t* frame_buffer1 = (pixel_t*)FRAME_BUFFER1_ADDR;
	pixel_t* frame_buffer2 = (pixel_t*)FRAME_BUFFER2_ADDR;
	pixel_t* frame_buffer3 = (pixel_t*)FRAME_BUFFER3_ADDR;

	while (1) {
		// frame counter starts at 3 so fb3 should be taken if
		// VDMA is writing to fb1 (frame count = 3)
		switch (check_vdma_status()) {
		case 3:
			// process FB3
			xil_printf("\nProcessing frame 3\n");
			process_frame(frame_buffer3);
		case 2:
			// process FB1
			xil_printf("\nProcessing frame 1\n");
			process_frame(frame_buffer1);
		case 1:
			// process FB2
			xil_printf("\nProcessing frame 2\n");
			process_frame(frame_buffer2);
		}

	}

	/* never reached */
	cleanup_platform();

	return 0;
}
