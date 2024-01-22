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

#include "xiicps.h"
#include "axiVdmaRegisters.h"
#include "cameraRegisters.h"

typedef uint32_t pixel_t;

/* defined by each RAW mode application */
void print_app_header();
int start_application(struct netif *netif);
int transfer_data();
void tcp_fasttmr(void);
void tcp_slowtmr(void);
void send_data(struct tcp_pcb *pcb, uint16_t *dataAddress, u32_t dataSize);
struct tcp_pcb * getGlobalPcb();
void init_ov7670();


/* missing declaration in lwIP */
void lwip_init();

static struct netif server_netif;
struct netif *echo_netif;

void
print_ip(char *msg, ip_addr_t *ip)
{
	print(msg);
	xil_printf("%d.%d.%d.%d\n\r", ip4_addr1(ip), ip4_addr2(ip),
			ip4_addr3(ip), ip4_addr4(ip));
}

void
print_ip_settings(ip_addr_t *ip, ip_addr_t *mask, ip_addr_t *gw)
{

	print_ip("Board IP: ", ip);
	print_ip("Netmask : ", mask);
	print_ip("Gateway : ", gw);
}

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
	ip_addr_t ipaddr, netmask, gw;

	/* the mac address of the board. this should be unique per board */
	unsigned char mac_ethernet_address[] =
	{ 0x00, 0x0a, 0x35, 0x00, 0x01, 0x02 };

	echo_netif = &server_netif;
	init_platform();

	/* initialize IP addresses to be used */
	IP4_ADDR(&ipaddr,  192, 168,   1, 10);
	IP4_ADDR(&netmask, 255, 255, 255,  0);
	IP4_ADDR(&gw,      192, 168,   1,  1);
	print_app_header();

	lwip_init();

	/* Add network interface to the netif_list, and set it as default */
	if (!xemac_add(echo_netif, &ipaddr, &netmask,
						&gw, mac_ethernet_address,
						PLATFORM_EMAC_BASEADDR)) {
		xil_printf("Error adding N/W interface\n\r");
		return -1;
	}
	netif_set_default(echo_netif);

	/* now enable interrupts */
	platform_enable_interrupts();

	/* specify that the network if is up */
	netif_set_up(echo_netif);

	print_ip_settings(&ipaddr, &netmask, &gw);

    xil_printf("Start Initialize Camera Module, reset before usage\n");
    init_ov7670();

    xil_printf("\nSetUp VMDA\n");
    vdma_handle handle_s2mm;
    vdma_setup(&handle_s2mm, VDMA_S2MM, WIDTH, HEIGHT, 4, FRAME_BUFFER_1, FRAME_BUFFER_2, FRAME_BUFFER_3);
    vdma_s2mm_status_dump(&handle_s2mm);

    xil_printf("\nStart VMDA\n");
    vdma_start_s2mm(&handle_s2mm);
    vdma_s2mm_status_dump(&handle_s2mm);
    sleep(5);
    vdma_s2mm_status_dump(&handle_s2mm);
    vdma_set(&handle_s2mm, OFFSET_VDMA_S2MM_STATUS_REGISTER, 0);
    vdma_s2mm_status_dump(&handle_s2mm);

	/* start the application (web server, rxtest, txtest, etc..) */


    struct tcp_pcb *pcb;
    start_application(echo_netif);
    int idx = 0;

	/* receive and process packets */
    int connected = 0;
	while (connected != 1) {
		xemacif_input(echo_netif);
		pcb = getGlobalPcb();
		if (pcb->state == ESTABLISHED) {
				xil_printf("Conn established\n");
				connected = 1;
			}
	}
	uint32_t frame_size = HEIGHT * WIDTH * sizeof(pixel_t);

    while(1) {
    	xil_printf("\nSend %d frame\n", idx++);
    	int status = vdma_get(&handle_s2mm, OFFSET_VDMA_S2MM_STATUS_REGISTER);

    	switch ((status & VDMA_STATUS_REGISTER_FrameCount) >> 16) {
    		case 1:
    			send_data(pcb, handle_s2mm.fb3PhysicalAddress, frame_size);
    			break;
    		case 2:
    			send_data(pcb, handle_s2mm.fb1PhysicalAddress, frame_size);
    			break;
    		case 3:
    			send_data(pcb, handle_s2mm.fb2PhysicalAddress, frame_size);
    			break;
    		default:
    	}
    }

	/* never reached */
	cleanup_platform();

	return 0;
}
