#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>

#include "contiki.h"
#include "net/rime.h"

#include "lib/list.h"
#include "lib/memb.h"

#include "dev/button-sensor.h"
#include "dev/leds.h"

#define MAX_RETRANSMISSIONS 4
#define NUM_HISTORY_ENTRIES 4

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
} node;

node client;
/*---------------------------------------------------------------------------*/
/* Reliable unicast */
static void
recv_runicast(struct runicast_conn *c, const rimeaddr_t *from, uint8_t seqno)
{
	printf("runicast message received from %d.%d, seqno %d\n",
		   from->u8[0], from->u8[1], seqno);
printf("packetbuf_dataptr = %s\n",
		   (char *)packetbuf_dataptr());
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
	printf("Root: broadcast message received from %d.%d: '%s'\n",
		   from->u8[0], from->u8[1], (char *)packetbuf_dataptr());

	rimeaddr_t receiver;
	packetbuf_copyfrom("0", 1);
	receiver.u8[0] = from->u8[0];
	receiver.u8[1] = from->u8[1];
	runicast_send(&runicast, &receiver, MAX_RETRANSMISSIONS);

}
static const struct broadcast_callbacks broadcast_call = {broadcast_recv};
static struct broadcast_conn broadcast;
/*---------------------------------------------------------------------------*/

PROCESS_THREAD(mini_rpl_process, ev, data)
{
	PROCESS_EXITHANDLER(broadcast_close(&broadcast);)
	PROCESS_EXITHANDLER(runicast_close(&runicast);)

	PROCESS_BEGIN();

	client.node_addr.u8[0] = rimeaddr_node_addr.u8[0];
	client.node_addr.u8[1] = rimeaddr_node_addr.u8[1];
	client.hop_dist = 0;

	runicast_open(&runicast, 144, &runicast_callbacks);

	static struct etimer et;

	broadcast_open(&broadcast, 129, &broadcast_call);

	PROCESS_END();
}
