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
PROCESS(rime_update_process, "RPLupdate implementation");
AUTOSTART_PROCESSES(&mini_rpl_process);
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

int share = 0;

int gc = 0; //nb of good message receive
int k = 4; //some treshold
int tmin = 10;
int tmax = 300;
int tc = 10;

/* OPTIONAL: Sender history.
 * Detects duplicate callbacks at receiving nodes.
 * Duplicates appear when ack messages are lost. */
struct history_entry {
    struct history_entry *next;
    rimeaddr_t addr;
    uint8_t seq;
};
LIST(history_table);
LIST(history_tableRPL);
MEMB(history_mem, struct history_entry, NUM_HISTORY_ENTRIES);
MEMB(history_memRPL, struct history_entry, NUM_HISTORY_ENTRIES);

/***
 *  ===========================================================================
 *  RPL : Reliable Unicast
 *  ===========================================================================
 */
void
recv_runicast(struct runicast_conn *c, const rimeaddr_t *from, uint8_t seqno) {
    /* OPTIONAL: Sender history */
    struct history_entry *e = NULL;
    for(e = list_head(history_tableRPL); e != NULL; e = e->next) {
        if(rimeaddr_cmp(&e->addr, from)) {
            break;
        }
    }
    if(e == NULL) {
        /* Create new history entry */
        e = memb_alloc(&history_memRPL);
        if(e == NULL) {
            e = list_chop(history_tableRPL); /* Remove oldest at full history */
        }
        rimeaddr_copy(&e->addr, from);
        e->seq = seqno;
        list_push(history_tableRPL, e);
    } else {
        /* Detect duplicate callback */
        if(e->seq == seqno) {
            printf("runicast message received from %d.%d, seqno %d (DUPLICATE)\n",
                   from->u8[0], from->u8[1], seqno);
            return;
        }
        /* Update existing history entry */
        e->seq = seqno;
    }
    uint8_t *hops = (uint8_t *) packetbuf_dataptr();
    if (USE_RSSI == 0 && *hops > parent.hop_dist && parent.node_addr.u8[0] != 0) {

    }
    else if(USE_RSSI == 0 && *hops < parent.hop_dist) {
        parent.node_addr.u8[0] = from->u8[0];
        parent.node_addr.u8[1] = from->u8[1];
        parent.hop_dist = *hops;
        client.hop_dist = *hops + 1;
        printf("RPL{RECONFIG2 - PARENT :%d.%d]} - %d : parent.hop\n",
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
        gc = 0;
        tc = tmin;
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
    printf("RPL{%d.%d <> RECEIVE-BROADCAST} - C = %d\n", from->u8[0], from->u8[1],gc);
    gc = gc+1;
    if(parent.node_addr.u8[0] == 0) { //No parent => Choose one
        if (USE_RSSI == 0 && *hops < parent.hop_dist && *hops != 253) {
            parent.node_addr.u8[0] = from->u8[0];
            parent.node_addr.u8[1] = from->u8[1];
            parent.hop_dist = *hops;
            client.hop_dist = *hops + 1;
            printf("RPL{CHOOSE-PARENT:%d.%d]} - parent.hop : %d - %d : client.hop\n",
                   parent.node_addr.u8[0], parent.node_addr.u8[1], parent.hop_dist, client.hop_dist);
            uint8_t * hops = client.hop_dist;
            packetbuf_copyfrom(&hops, 1);
            broadcast_send(&broadcastRPL);
            process_start(&rime_update_process,NULL);
            //Si on utilise des rooting table
            rimeaddr_t receiver;
            receiver.u8[0] = from->u8[0];
            receiver.u8[1] = from->u8[1];
            packetbuf_copyfrom(&hops, 1);
            runicast_send(&runicastRPL, &receiver, MAX_RETRANSMISSIONS);
        }
        if(USE_RSSI == 1 && last_rssi > parent.rssi) {
            parent.node_addr.u8[0] = from->u8[0];
            parent.node_addr.u8[1] = from->u8[1];
            parent.rssi = last_rssi;
            printf("RPL{CHOOSE-PARENT:%d.%d} - parent.hop : %d - %d : client.hop\n",
                   parent.node_addr.u8[0], parent.node_addr.u8[1], parent.hop_dist, client.hop_dist);
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
            gc = 0;
            tc = tmin;
        }
        else if(USE_RSSI == 0 && *hops < parent.hop_dist){ //Find better parent => Replace current
            parent.node_addr.u8[0] = from->u8[0];
            parent.node_addr.u8[1] = from->u8[1];
            parent.hop_dist = *hops;
            client.hop_dist = *hops + 1;
            printf("RPL{RECONFIG - PARENT:%d.%d} - parent.hop : %d - %d : client.hop\n",
                   parent.node_addr.u8[0], parent.node_addr.u8[1], parent.hop_dist, client.hop_dist);
            gc = 0;
            tc = tmin;
        }
        else {
            if(share == 5){
                uint8_t * hops = client.hop_dist;
                packetbuf_copyfrom(&hops, 1);
                broadcast_send(&broadcastRPL);
                share = 0;
            }
            else {
                share = share + 1;
            }
        }
    }
}

static const struct broadcast_callbacks broadcast_call = {broadcast_recv};


/***
 *  ===========================================================================
 *  RECEIVER : Reliable Unicast
 *  ===========================================================================
 */
void
recv_runicastData(struct runicast_conn *c, const rimeaddr_t *from, uint8_t seqno) {
    /* OPTIONAL: Sender history */
    struct history_entry *e = NULL;
    for(e = list_head(history_table); e != NULL; e = e->next) {
        if(rimeaddr_cmp(&e->addr, from)) {
            break;
        }
    }
    if(e == NULL) {
        /* Create new history entry */
        e = memb_alloc(&history_mem);
        if(e == NULL) {
            e = list_chop(history_table); /* Remove oldest at full history */
        }
        rimeaddr_copy(&e->addr, from);
        e->seq = seqno;
        list_push(history_table, e);
    } else {
        /* Detect duplicate callback */
        if(e->seq == seqno) {
            printf("runicast message received from %d.%d, seqno %d (DUPLICATE)\n",
                   from->u8[0], from->u8[1], seqno);
            return;
        }
        /* Update existing history entry */
        e->seq = seqno;
    }
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
 *  RPL PROCESS : SENDER
 *  ===========================================================================
 */
PROCESS_THREAD(mini_rpl_process, ev, data) {
PROCESS_EXITHANDLER(broadcast_close(&broadcastRPL));
PROCESS_EXITHANDLER(runicast_close(&runicastRPL));
PROCESS_EXITHANDLER(runicast_close(&runicastMQTT));
PROCESS_BEGIN();

parent.node_addr.u8[0] = 0;
parent.node_addr.u8[1] = 0;
parent.hop_dist = 254;
parent.rssi = -65534;
/* OPTIONAL: Sender history */
list_init(history_table);
list_init(history_tableRPL);
memb_init(&history_mem);
memb_init(&history_memRPL);
client.node_addr.u8[0] = rimeaddr_node_addr.u8[0];
client.node_addr.u8[1] = rimeaddr_node_addr.u8[1];

runicast_open(&runicastMQTT, 145, &runicast_callbacksData);
runicast_open(&runicastRPL, 144, &runicast_callbacks);
broadcast_open(&broadcastRPL, 129, &broadcast_call);

static struct etimer et;
BROADCAST: while(parent.node_addr.u8[0] != 0) {
int time = random_rand() % 100 + 1; //1 -> 50 sec
etimer_set(&et,(time/2) *CLOCK_SECOND);
PROCESS_WAIT_EVENT_UNTIL(etimer_expired(&et));
process_start(&rime_sender_process,NULL);
}

while (parent.node_addr.u8[0] == 0 && parent.node_addr.u8[1] == 0) {
etimer_set(&et,30 *CLOCK_SECOND);
PROCESS_WAIT_EVENT_UNTIL(etimer_expired(&et));
uint8_t * hops = 253; //DIS message
packetbuf_copyfrom(&hops, 1);
if(parent.node_addr.u8[0] == 0 && parent.node_addr.u8[1] == 0)  {
printf("RPL{DIS-Message}\n");
broadcast_send(&broadcastRPL);
}
}

goto BROADCAST;
PROCESS_END();
}

/***
 *  ===========================================================================
 *  MQTT PROCESS : Sender
 *  ===========================================================================
 */
PROCESS_THREAD(rime_sender_process, ev, data) {
PROCESS_BEGIN();
if(parent.node_addr.u8[0] != 0) {
dpkt * pp = generateData(rimeaddr_node_addr.u8[0],TYPE);
packetbuf_copyfrom((void *) pp, 4);
printDPKT(pp, &parent.node_addr.u8[0],&parent.node_addr.u8[1],"SENDER", "SENT");
runicast_send(&runicastMQTT, &parent.node_addr, MAX_RETRANSMISSIONS);
}
PROCESS_END();
}


PROCESS_THREAD(rime_update_process, ev, data) {
PROCESS_BEGIN();
static struct etimer et;
while(1) {
int randompercentage = random_rand() % 100 + 1;//0-100%
randompercentage = randompercentage/2;//0-50%
int i = (tc/2) + (int) ((double)(1/(double)randompercentage) * tc);
etimer_set(&et,i *CLOCK_SECOND);
printf("RPL UPDATE{TC = %d, C = %d}\n",i,gc);
PROCESS_WAIT_EVENT_UNTIL(etimer_expired(&et));
if(parent.node_addr.u8[0] != 0 && gc < k) {
uint8_t * hops = client.hop_dist;
packetbuf_copyfrom(&hops, 1);
broadcast_send(&broadcastRPL);
}
else {
tc = 2*tc;
if(tc > tmax) tc = tmax;
}
}
PROCESS_END();
}
