#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>

#include "contiki.h"
#include "net/rime.h"

#include "lib/list.h"
#include "lib/memb.h"

#include "dev/button-sensor.h"
#include "dev/leds.h"

#include "mini_rpl.c"
#include "../contiki/core/contiki.h"
#include "../contiki/core/net/rime/broadcast.h"
#include "generatePayload.c"

#define MAX_RETRANSMISSIONS 4
#define NUM_HISTORY_ENTRIES 4
#define USE_RSSI 0

/*---------------------------------------------------------------------------*/
PROCESS(rime_sender_process, "rime sender implementation");
AUTOSTART_PROCESSES(&rime_sender_process);
/*---------------------------------------------------------------------------*/
struct packetdata;
typedef struct packetdata
{
	uint8_t id;
	uint8_t nb_hops;
    char * data;
} pktd;

/*---------------------------------------------------------------------------*/
/* Reliable unicast */
static void
recv_runicast(struct runicast_conn *c, const rimeaddr_t *from, uint8_t seqno)
{
	printf("runicast message received from %d.%d, seqno %d\n",
		   from->u8[0], from->u8[1], seqno);

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


PROCESS_THREAD(mini_rpl_process, ev, data)
{
	PROCESS_EXITHANDLER(runicast_close(&runicast);)

	PROCESS_BEGIN();

	runicast_open(&runicast, 144, &runicast_callbacks);

	if(rimeaddr_node_addr.u8[0] == 1 &&
           rimeaddr_node_addr.u8[1] == 0) {
            PROCESS_WAIT_EVENT_UNTIL(0);
	}

	while(1)
	{
        static struct etimer et;
        etimer_set(&et, 10*CLOCK_SECOND);
        PROCESS_WAIT_EVENT_UNTIL(etimer_expired(&et));
        if(!runicast_is_transmitting(&runicast)) {
            rimeaddr_t recv;
            int nb = (char *) ((rand() % (2 + 1 - 1)) + 1);
            char outData[2]; //"-"+"44.44"+'\0'
            snprintf(outData, 2, "%d", nb);
            char * pp = generateData("1", outData);
            packetbuf_copyfrom(pp, sizeof(pp));
            recv.u8[0] = 1;
            recv.u8[1] = 0;
            printf("%u.%u: sending runicast to address %u.%u\n",
                   rimeaddr_node_addr.u8[0],
                   rimeaddr_node_addr.u8[1],
                   recv.u8[0],
                   recv.u8[1]);
            runicast_send(&runicast, &recv, MAX_RETRANSMISSIONS);
        }
	}

	PROCESS_END();
}