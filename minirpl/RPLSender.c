#include <stdio.h>
#include <stdlib.h>

#include "contiki.h"
#include "net/rime.h"

#include "lib/list.h"
#include "lib/memb.h"

#include "dev/button-sensor.h"
#include "dev/leds.h"

#include "DataGenerator.c"

#define TYPE 2 //TODO

/*---------------------------------------------------------------------------*/
PROCESS(mini_rpl_process, "RPLSender implementation");
PROCESS(rime_sender_process, "MQTTSender implementation");
AUTOSTART_PROCESSES(&rime_sender_process,&mini_rpl_process);
/*---------------------------------------------------------------------------*/

/***
 *  ===========================================================================
 *  Instance Variable
 *  ===========================================================================
 */
node parent;
node client;

static struct runicast_conn runicastRPL;
static struct broadcast_conn broadcastRPL;
static struct runicast_conn runicastMQTT;

int c = 0;
int k = 0.5;

/***
 *  ===========================================================================
 *  RPL : Reliable Unicast
 *  ===========================================================================
 */
void
recv_runicast(struct runicast_conn *c, const rimeaddr_t *from, uint8_t seqno) {
    uint8_t *hops = (uint8_t *) packetbuf_dataptr();
    if (USE_RSSI == 0 && *hops > parent.hop_dist && parent.node_addr.u8[0] != 0) {

    }
    else if(USE_RSSI == 0 && *hops < parent.hop_dist) {
        parent.node_addr.u8[0] = from->u8[0];
        parent.node_addr.u8[1] = from->u8[1];
        parent.hop_dist = *hops;
        client.hop_dist = *hops + 1;
        printf("RPL{RECONFIG - CPARENT :%d.%d]} - %d : hop\n", 
            parent.node_addr.u8[0], parent.node_addr.u8[1], parent.hop_dist);
        uint8_t * hops = client.hop_dist;
        packetbuf_copyfrom(&hops, 1);
        broadcast_send(&broadcastRPL);
        rimeaddr_t receiver;
        receiver.u8[0] = from->u8[0];
        receiver.u8[1] = from->u8[1];
        hops = client.hop_dist;
        packetbuf_copyfrom(&hops, 1);
        runicast_send(&runicastRPL, &receiver, MAX_RETRANSMISSIONS);
    }
}

void
sent_runicast(struct runicast_conn *c, const rimeaddr_t *to, uint8_t retransmissions) {
    printf("RPL{%d.%d <> RETRANSMIT:%d}\n", 
            to->u8[0], to->u8[1], retransmissions);
}

void
timedout_runicast(struct runicast_conn *c, const rimeaddr_t *to, uint8_t retransmissions) {
    printf("RPL{%d.%d <> TIMEOUT:%d}\n", 
            to->u8[0], to->u8[1], retransmissions);
    if(parent.node_addr.u8[0] != 1) { //No reconfiguration for node directly connected to the root
        parent.node_addr.u8[0] = 0;
        parent.hop_dist = 254;
    }
}
static const struct runicast_callbacks runicast_callbacks = {recv_runicast,
                                                             sent_runicast,
                                                             timedout_runicast};

/***
 *  ===========================================================================
 *  RPL : Broadcast
 *  ===========================================================================
 */
void
broadcast_recv(struct broadcast_conn *c, const rimeaddr_t *from) {
    uint16_t last_rssi = packetbuf_attr(PACKETBUF_ATTR_RSSI);
    uint8_t *hops = (uint8_t *) packetbuf_dataptr();
    /* If we receive a direct communication from the broadcast we sent,
    we should update the parent node if the sender is closer */
    printf("RPL{%d.%d <> R-BROADCAST} - %u\n", from->u8[0], from->u8[1], *hops);
    if(parent.node_addr.u8[0] == 0) { //No parent => Choose one
        if (USE_RSSI == 0 && *hops < parent.hop_dist && *hops != 253) { 
            parent.node_addr.u8[0] = from->u8[0];
            parent.node_addr.u8[1] = from->u8[1];
            parent.hop_dist = *hops;
            client.hop_dist = *hops + 1;
            printf("RPL{CPARENT:%d.%d]} - %d : hop\n", parent.node_addr.u8[0], parent.node_addr.u8[1], parent.hop_dist);

            uint8_t * hops = client.hop_dist;
            packetbuf_copyfrom(&hops, 1);
            broadcast_send(&broadcastRPL);

            uint8_t * hops2 = client.hop_dist;
            packetbuf_copyfrom(&hops2, 1);
            broadcast_send(&broadcastRPL);

            //Si on utilise des rooting table
            /*rimeaddr_t receiver;
            receiver.u8[0] = from->u8[0];
            receiver.u8[1] = from->u8[1];
            packetbuf_copyfrom(&hops, 1);
            runicast_send(&runicastRPL, &receiver, MAX_RETRANSMISSIONS);*/
            process_poll(&rime_sender_process);

        }
        if(USE_RSSI == 1 && last_rssi > parent.rssi) {
            parent.node_addr.u8[0] = from->u8[0];
            parent.node_addr.u8[1] = from->u8[1];
            parent.rssi = last_rssi;
            printf("RPL{CPARENT:%d.%d]} - %d : hop\n", parent.node_addr.u8[0], parent.node_addr.u8[1], parent.hop_dist);
        }
    }
    else { //Already have parent
        if(USE_RSSI == 0 && *hops == 253) { //Receive a request DIS from a node
            uint8_t * hops = client.hop_dist;
            packetbuf_copyfrom(&hops, 1);
            rimeaddr_t receiver;
            receiver.u8[0] = from->u8[0];
            receiver.u8[1] = from->u8[1];
            runicast_send(&runicastRPL, &receiver, MAX_RETRANSMISSIONS);
        }
        else if(USE_RSSI == 0 && *hops < parent.hop_dist){ //Find better parent => Replace current
            parent.node_addr.u8[0] = from->u8[0];
            parent.node_addr.u8[1] = from->u8[1];
            parent.hop_dist = *hops;
            client.hop_dist = *hops + 1;
            printf("RPL{RECONFIG - CPARENT:%d.%d]} - %d : hop\n", parent.node_addr.u8[0], parent.node_addr.u8[1], parent.hop_dist);
        }
    }
}

static const struct broadcast_callbacks broadcast_call = {broadcast_recv};

/***
 *  ===========================================================================
 *  RPL PROCESS : SENDER
 *  ===========================================================================
 */
PROCESS_THREAD(mini_rpl_process, ev, data) {
    PROCESS_EXITHANDLER(broadcast_close(&broadcastRPL));
    PROCESS_EXITHANDLER(runicast_close(&runicastRPL));

    PROCESS_BEGIN();

    parent.node_addr.u8[0] = 0;
    parent.node_addr.u8[1] = 0;
    parent.hop_dist = 254;
    parent.rssi = -65534;

    client.node_addr.u8[0] = rimeaddr_node_addr.u8[0];
    client.node_addr.u8[1] = rimeaddr_node_addr.u8[1];


    runicast_open(&runicastRPL, 144, &runicast_callbacks);
    broadcast_open(&broadcastRPL, 129, &broadcast_call);

    static struct etimer et;
    BROADCAST:while (parent.node_addr.u8[0] == 0 && parent.node_addr.u8[1] == 0) {
        etimer_set(&et, 30 * CLOCK_SECOND);
        PROCESS_WAIT_EVENT_UNTIL(etimer_expired(&et));
        uint8_t * hops = 253; //DIS message
        packetbuf_copyfrom(&hops, 1);
        broadcast_send(&broadcastRPL);
    }

    while(parent.node_addr.u8[0] != 0) {
        int time = random_rand() % 100 + 1; //1 -> 50 sec
        etimer_set(&et,(time/2) *CLOCK_SECOND);
        PROCESS_WAIT_EVENT_UNTIL(etimer_expired(&et));
        process_poll(&rime_sender_process); //Then the sender process some time in standby mode => to test
        //TODO
        //etimer_set(&et, 30 * CLOCK_SECOND);
        //PROCESS_WAIT_EVENT_UNTIL(etimer_expired(&et));
        uint8_t * hops = client.hop_dist;
        packetbuf_copyfrom(&hops, 1);
        broadcast_send(&broadcastRPL);
    }

    goto BROADCAST;
    PROCESS_END();
}


/***
 *  ===========================================================================
 *  RECEIVER : Reliable Unicast
 *  ===========================================================================
 */
void
recv_runicastData(struct runicast_conn *c, const rimeaddr_t *from, uint8_t seqno) {
    if (parent.node_addr.u8[0] != 0) {
        printf("SENDER{%d.%d[%d] > FOWARD > %u.%u}\n", from->u8[0], from->u8[1], seqno, parent.node_addr.u8[0], parent.node_addr.u8[1]);
        runicast_send(&runicastMQTT, &parent.node_addr, MAX_RETRANSMISSIONS);
    }
}

void
sent_runicastData(struct runicast_conn *c, const rimeaddr_t *to, uint8_t retransmissions) {
    printf("SENDER{%d.%d <> RETRANSMIT:%d}\n",
           to->u8[0], to->u8[1],retransmissions);
}

void
timedout_runicastData(struct runicast_conn *c, const rimeaddr_t *to, uint8_t retransmissions) {
    printf("SENDER{%d.%d <> TIMEOUT:%d}\n",
           to->u8[0], to->u8[1], retransmissions);
    if(parent.node_addr.u8[0] != 1) {
        parent.node_addr.u8[0] = 0;
        parent.hop_dist = 254;
    }
}

static const struct runicast_callbacks runicast_callbacksData = {recv_runicastData,
                                                                 sent_runicastData,
                                                                 timedout_runicastData};
/***
 *  ===========================================================================
 *  MQTT PROCESS : Sender
 *  ===========================================================================
 */
PROCESS_THREAD(rime_sender_process, ev, data) {
    PROCESS_EXITHANDLER(runicast_close(&runicastMQTT));
    PROCESS_BEGIN();
    runicast_open(&runicastMQTT, 145, &runicast_callbacksData);

    //PROCESS_YIELD();

    if(rimeaddr_node_addr.u8[0] == 1 && rimeaddr_node_addr.u8[1] == 0) {
        printf(">> DEVICE NOT INIT << \n");
        PROCESS_WAIT_EVENT_UNTIL(0);
    }

    if(parent.node_addr.u8[0] == 0 && parent.node_addr.u8[1] == 0) {
        printf(">> PARENT NOT INIT << \n");
        PROCESS_WAIT_EVENT_UNTIL(0);
    }

    while(1) {
        PROCESS_YIELD();
        if(!runicast_is_transmitting(&runicastMQTT) && parent.node_addr.u8[0] != 0) {
            dpkt * pp = generateData(rimeaddr_node_addr.u8[0],TYPE);
            packetbuf_copyfrom((void *) pp, 4);
            printDPKT(pp, &parent.node_addr.u8[0],&parent.node_addr.u8[1],"SENDER", "SENT");
            runicast_send(&runicastMQTT, &parent.node_addr, MAX_RETRANSMISSIONS);
        }
    }
    PROCESS_END();
}
