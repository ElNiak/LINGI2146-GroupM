#include <stdio.h>
#include <stdlib.h>

#include "contiki.h"
#include "net/rime.h"

#include "lib/list.h"
#include "lib/memb.h"

#include "dev/button-sensor.h"
#include "dev/leds.h"

#define MAX_RETRANSMISSIONS 4
#define NUM_HISTORY_ENTRIES 4
#define USE_RSSI 0

/*---------------------------------------------------------------------------*/
PROCESS(mini_rpl_process, "Mini-rpl implementation");
AUTOSTART_PROCESSES(&mini_rpl_process);
/*---------------------------------------------------------------------------*/
struct packet;

typedef struct packet
{
	uint8_t id;
	uint8_t nb_hops;
} pkt;

struct ru_node;

typedef struct ru_node
{
	rimeaddr_t node_addr;
	uint8_t hop_dist;
	uint16_t rssi;
} node;

node parent;
node client;
/*---------------------------------------------------------------------------*/
/* Reliable unicast */
static void
recv_runicast(struct runicast_conn *c, const rimeaddr_t *from, uint8_t seqno)
{
	printf("runicast message received from %d.%d, seqno %d\n",
		   from->u8[0], from->u8[1], seqno);

	/* If we receive a direct communication from the broadcast we sent, 
	we should update the parent node if the sender is closer */
	uint16_t last_rssi = packetbuf_attr(PACKETBUF_ATTR_RSSI);
	char *payload = (char *)packetbuf_dataptr();
	uint8_t hops = (uint8_t)atoi(payload);
	printf("nb hops received : %d\n", hops);
	if (USE_RSSI == 0 && hops < parent.hop_dist)
	{
		parent.node_addr.u8[0] = from->u8[0];
		parent.node_addr.u8[1] = from->u8[1];
		parent.hop_dist = hops;
		client.hop_dist = hops + 1;
		printf("Change parent to %d.%d with %d hop", parent.node_addr.u8[0], parent.node_addr.u8[1], parent.hop_dist);
	} else if(USE_RSSI == 1 && last_rssi > parent.rssi) {
		parent.node_addr.u8[0] = from->u8[0];
		parent.node_addr.u8[1] = from->u8[1];
		parent.rssi = last_rssi;
		printf("Change parent to %d.%d with %d hop", parent.node_addr.u8[0], parent.node_addr.u8[1], parent.hop_dist);
	}
}

static void
sent_runicast(struct runicast_conn *c, const rimeaddr_t *to, uint8_t retransmissions)
{
	printf("runicast message sent to %d.%d, retransmissions %d\n",
		   to->u8[0], to->u8[1], retransmissions);
}

static void
timedout_runicast(struct runicast_conn *c, const rimeaddr_t *to, uint8_t retransmissions)
{
	printf("runicast message timed out when sending to %d.%d, retransmissions %d\n",
		   to->u8[0], to->u8[1], retransmissions);

	parent.node_addr.u8[0] = 0;
	parent.hop_dist = 254;
}
static const struct runicast_callbacks runicast_callbacks = {recv_runicast,
															 sent_runicast,
															 timedout_runicast};
static struct runicast_conn runicast;
/*---------------------------------------------------------------------------*/
/* Broadcast */
static void
broadcast_recv(struct broadcast_conn *c, const rimeaddr_t *from)
{
	// printf("broadcast message received from %d.%d: '%s'\n",
	// 	   from->u8[0], from->u8[1], (char *)packetbuf_dataptr());

	if (parent.node_addr.u8[0] != 0)
	{
		while(runicast_is_transmitting(&runicast)){}
		rimeaddr_t receiver;
		char *hops;
		packetbuf_copyfrom(&hops, sizeof(client.hop_dist));
		receiver.u8[0] = from->u8[0];
		receiver.u8[1] = from->u8[1];
		runicast_send(&runicast, &receiver, MAX_RETRANSMISSIONS);
	}
}
static const struct broadcast_callbacks broadcast_call = {broadcast_recv};
static struct broadcast_conn broadcast;
/*---------------------------------------------------------------------------*/

PROCESS_THREAD(mini_rpl_process, ev, data)
{
	PROCESS_EXITHANDLER(broadcast_close(&broadcast);)
	PROCESS_EXITHANDLER(runicast_close(&runicast);)

	PROCESS_BEGIN();

	parent.node_addr.u8[0] = 0;
	parent.node_addr.u8[1] = 0;
	parent.hop_dist = 254;
	parent.rssi = -65534;

	client.node_addr.u8[0] = rimeaddr_node_addr.u8[0];
	client.node_addr.u8[1] = rimeaddr_node_addr.u8[1];


	runicast_open(&runicast, 144, &runicast_callbacks);
	broadcast_open(&broadcast, 129, &broadcast_call);

	static struct etimer et;

	BROADCAST:while (parent.node_addr.u8[0] == 0 && parent.node_addr.u8[1] == 0)
	{
		etimer_set(&et, 4 * CLOCK_SECOND);
		PROCESS_WAIT_EVENT_UNTIL(etimer_expired(&et));
		//packetbuf_copyfrom("Hello", 6);
		broadcast_send(&broadcast);
		//printf("broadcast message sent\n");
	}

	while(parent.node_addr.u8[0] != 0)
	{
		PROCESS_WAIT_EVENT_UNTIL(0);
	}

	goto BROADCAST;

	PROCESS_END();
}