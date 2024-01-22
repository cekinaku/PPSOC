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
#include "netif/xadapter.h"

struct tcp_pcb * pcbGlobal;
u32_t bytes_to_send;
int last_packet_sent;
void *send_head;
struct netif *globalNetif;
int packets_sent = 0;

int transfer_data() {
	return 0;
}

void print_app_header()
{
#if (LWIP_IPV6==0)
	xil_printf("\n\r\n\r-----lwIP TCP echo server ------\n\r");
#else
	xil_printf("\n\r\n\r-----lwIPv6 TCP echo server ------\n\r");
#endif
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

	/* echo back the payload */
	/* in this case, we assume that the payload is < TCP_SND_BUF */
	if (tcp_sndbuf(tpcb) > p->len) {
		err = tcp_write(tpcb, p->payload, p->len, 1);
		xil_printf("txperf: status on tcp_write: %d\r\n", err);
	} else
		xil_printf("no space in tcp_sndbuf\n\r");

	/* free the received pbuf */
	pbuf_free(p);

	return ERR_OK;
}

err_t sent_callback(void *arg, struct tcp_pcb *tpcb, u16_t len) {

	xil_printf("Entered Sent callback\n");
	//uint16_t tcp_sndf = tcp_sndbuf(tpcb);
	//xil_printf("tcp_sndbuf cb %d\n", tcp_sndf);

	uint32_t max_bytes = 10024;
	uint32_t packet_size = bytes_to_send;
	err_t status;
	//xil_printf("bytes to send start %d\n", bytes_to_send);
	//xil_printf("initial packer size %d\n", packet_size);

	packets_sent++;
	if (bytes_to_send == 0) {
		//xil_printf("bytes to send is 0");
		//xil_printf("Packets per frame: %d", packets_sent);
		last_packet_sent = 1;
		return ERR_OK;
	}

	if (packet_size > max_bytes) {
		//xil_printf("packet size is larger than max bytes limit \n");
		packet_size = max_bytes;
		//xil_printf("packet_size %d\n", packet_size);
	}

	status = tcp_write(tpcb, send_head, packet_size, 0);
	if (status != ERR_OK) {
		xil_printf("txperf: Error on tcp_write: %d\r\n", status);
		return status;
	}

	//xil_printf("compariosn packetSize %d\n", packet_size);

	if (packet_size == bytes_to_send) {
		bytes_to_send = 0;
	} else {
		bytes_to_send -= packet_size;
	}

	//xil_printf("bytes to send reduced %d\n", bytes_to_send);

	send_head += packet_size;

	status = tcp_output(tpcb);
	if (status != ERR_OK) {
		xil_printf("txperf: Error on tcp_output: %d\r\n", status);
		return status;
	}


    return status;
}

err_t accept_callback(void *arg, struct tcp_pcb *newpcb, err_t err)
{
	static int connection = 1;

	/* set the receive callback for this connection */
	tcp_recv(newpcb, recv_callback);

	/* just use an integer number indicating the connection id as the
	   callback argument */
	tcp_arg(newpcb, (void*)(UINTPTR)connection);

	tcp_sent(newpcb, sent_callback);

	// set global pcb
	pcbGlobal = newpcb;
	if (newpcb->state == ESTABLISHED) {
		xil_printf("Conn established\n");
	}


	/* increment for subsequent accepted connections */
	connection++;

	return ERR_OK;
}


int start_application(struct netif *netif)
{
	globalNetif = netif;
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

void send_data(struct tcp_pcb *pcb, uint32_t *dataAddress, u32_t dataSize){

	void *copied_value = (void*)0x13000000;
	memcpy(copied_value, dataAddress, dataSize);
	uint32_t packetSize;
	bytes_to_send = dataSize;
	err_t status;
	uint32_t max_bytes = 10024;
	last_packet_sent = 0;

	send_head = copied_value;
	if (!pcb){
		xil_printf("\n pcb not initialized  \n");
	}

	packetSize = max_bytes;

	status = tcp_write(pcb, send_head, packetSize, 0);
	if (status != ERR_OK) {
		xil_printf("txperf: Error on tcp_write: %d\r\n", status);
		return;
	}

	send_head += packetSize;
	bytes_to_send -= packetSize;

	status = tcp_output(pcb);
	if (status != ERR_OK) {
		xil_printf("txperf: Error on tcp_output: %d\r\n", status);
		return;
	}
	packets_sent++;

	while (1) {
		xemacif_input(globalNetif);
		if (last_packet_sent == 1) {
			xil_printf("\na ACKED\n");
			return;
		}
	}
}

struct tcp_pcb * getGlobalPcb() {
	return pcbGlobal;
}
