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
#include <string.h>

#include "lwip/err.h"
#include "lwip/tcp.h"
#if defined (__arm__) || defined (__aarch64__)
#include "xil_printf.h"
#endif

#include "xparameters.h"
#include "netif/xadapter.h"
#include "platform.h"
#include "xil_cache.h"
#include "platform_config.h"
#include "axiVdmaHelper.h"


typedef uint16_t pixel_t;

void lwip_init();

// global variables
static struct netif server_netif;
struct netif *echo_netif;
struct tcp_pcb *globalPcb;
int isFrameSending = 0;
size_t frame_size = 0;
size_t bytes_sent = 0;
size_t MAX_PBUF_SIZE = 10024;

// Define the buffer at a specific address
pixel_t* processed_frame_buffer = (pixel_t*)0x13000000;
pixel_t* send_frame_buffer = (pixel_t*)0x14000000;

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

int transfer_data() {
	return 0;
}

struct tcp_pcb* getGlobalPcb() {
    return globalPcb;
}

struct netif* getGlobalEchoNetif() {
    return echo_netif;
}

void print_app_header()
{
	xil_printf("\n\r\n\r-----lwIPv6 TCP echo server ------\n\r");
	xil_printf("TCP packets sent to port 6001 will be echoed back\n\r");
}

err_t recv_callback(void *arg, struct tcp_pcb *tpcb,
                               struct pbuf *p, err_t err)
{
	/* do not read the packet if we are not in ESTABLISHED state */
	if (!p) {
		tcp_close(tpcb);
		tcp_recv(tpcb, NULL);
		return ERR_OK;
	}

	/* indicate that the packet has been received */
	tcp_recved(tpcb, p->len);

	if (tcp_sndbuf(tpcb) > p->len) {
		err = tcp_write(tpcb, p->payload, p->len, 1);
	} else
		xil_printf("no space in tcp_sndbuf\n\r");

	/* free the received pbuf */
	pbuf_free(p);

	return ERR_OK;
}

err_t sent_callback(void *arg, struct tcp_pcb *tpcb, u16_t len) {

	xil_printf("bytes sent:%d.\n", bytes_sent);
	if (bytes_sent >= frame_size) {
		xil_printf("Frame sent succesfully.\n");
		isFrameSending = 0;
		return ERR_OK;
	}

	size_t send_size = frame_size - bytes_sent;

	if (send_size > MAX_PBUF_SIZE) {
		send_size = MAX_PBUF_SIZE;
	}

    err_t err = tcp_write(tpcb, processed_frame_buffer + bytes_sent, send_size, 0);
    if (err != ERR_OK) {
        xil_printf("Error: Failed to write packet data to TCP connection (err = %d).\n", err);
        return err;
    }

    // Signal lwIP to send the data
    tcp_output(tpcb);

    // Update the number of bytes sent
    bytes_sent += send_size;

    //xil_printf("Packet sent.\n");
    return ERR_OK;
}

err_t accept_callback(void *arg, struct tcp_pcb *newpcb, err_t err)
{
	static int connection = 1;

	/* set the receive callback for this connection */
	tcp_recv(newpcb, recv_callback);

	/* Set the sent callback for this connection */
	tcp_sent(newpcb, sent_callback);

	/* just use an integer number indicating the connection id as the
	   callback argument */
	tcp_arg(newpcb, (void*)(UINTPTR)connection);

	// Set globalPcb to the newly initialized PCB
	globalPcb = newpcb;

	/* increment for subsequent accepted connections */
	connection++;

	return ERR_OK;
}


int start_application()
{
	struct tcp_pcb *pcb;
	err_t err;
	unsigned port = 7;

	/* create new TCP PCB structure */
	pcb = tcp_new_ip_type(IPADDR_TYPE_ANY);
	if (!pcb) {
		xil_printf("Error creating PCB. Out of Memory\n\r");
		return -1;
	}

	/* bind to specified @port */
	err = tcp_bind(pcb, IP_ANY_TYPE, port);
	if (err != ERR_OK) {
		xil_printf("Unable to bind to port %d: err = %d\n\r", port, err);
		return -2;
	}

	/* we do not need any arguments to callback functions */
	tcp_arg(pcb, NULL);

	/* listen for connections */
	pcb = tcp_listen(pcb);
	if (!pcb) {
		xil_printf("Out of memory while tcp_listen\n\r");
		return -3;
	}

	/* specify callback to use for incoming connections */
	tcp_accept(pcb, accept_callback);

	xil_printf("TCP echo server started @ port %d\n\r", port);

	return 0;
}

void lwip_initialization()
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
			return;
		}
		netif_set_default(echo_netif);

		/* now enable interrupts */
		platform_enable_interrupts();

		/* specify that the network if is up */
		netif_set_up(echo_netif);


		print_ip_settings(&ipaddr, &netmask, &gw);
}


void send_frame(pixel_t* frame_buffer) {

	//xil_printf("Entered send frame.\n");
	isFrameSending = 1;
    struct tcp_pcb *pcb = getGlobalPcb();

    // Check if the connection is valid
    if (pcb == NULL || pcb->state != ESTABLISHED) {
        xil_printf("Error: Invalid or not established TCP connection.\n");
        return;
    }

    frame_size = FRAME_WIDTH * FRAME_HEIGHT;
    bytes_sent = 0;

    uint16_t tcp_sndf = tcp_sndbuf(pcb);
    //xil_printf("tcp_sndbuf initial %d\n", tcp_sndf);

    // Send the frame data over TCP
    err_t err = tcp_write(pcb, frame_buffer, MAX_PBUF_SIZE, 0);  // Set the 'apiflags' parameter to 1 for TCP_WRITE_FLAG_COPY
    if (err != ERR_OK) {
        xil_printf("Error: Failed to write packet data to TCP connection (err = %d).\n", err);
        return;
    }

    bytes_sent += MAX_PBUF_SIZE;
    // Signal lwIP to send the data
    tcp_output(pcb);

    //xil_printf("Packet sent successfully.\n");
}

void process_frame(pixel_t* input_buffer) {


    int numOfPixels = 0;
    for (int y = 0; y < FRAME_HEIGHT; y++) {
        for (int x = 0; x < FRAME_WIDTH; x++) {
            // Calculate index for the current pixel
            int idx = y * FRAME_WIDTH + x;

            // Extract color components from the input buffer
            uint8_t red = (input_buffer[idx] >> 11) & 0x1F;
            uint8_t green = (input_buffer[idx] >> 5) & 0x3F;
            uint8_t blue = input_buffer[idx] & 0x1F;

            // Create the current pixel with extracted values
            pixel_t current_pixel = (pixel_t)((red << 11) | (green << 5) | blue);

            // Store the pixel in the specified memory region
            processed_frame_buffer[idx] = current_pixel;
            numOfPixels++;
        }
    }
    xil_printf("number of pixels:%d\n", numOfPixels);


    while (1) {
    	xemacif_input(echo_netif);
    	if (isFrameSending == 0) {
    		xil_printf("\na cked\n");
    		break;
    	}
    }
    memcpy(send_frame_buffer, input_buffer, numOfPixels * 2);
    send_frame(send_frame_buffer);
}
