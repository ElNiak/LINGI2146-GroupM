#include <stdio.h>
#include <stdlib.h>

#include "contiki.h"
#include "net/rime.h"

#include "lib/list.h"
#include "lib/memb.h"

#include "dev/button-sensor.h"
#include "dev/leds.h"

#include "DataGenerator.c"

// #define TYPE 1 // Humidity
// #define TYPE 2 //Temperature

/*---------------------------------------------------------------------------*/
PROCESS(mini_rpl_process, "RPLSender implementation");
PROCESS(rime_sender_process, "MQTTSender implementation");
PROCESS(rime_update_process, "RPLupdate implementation");
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
static struct runicast_conn runicastConfig;

int share = 0; //Use to limit the number of response to broadcast message

//Trickle timer for broadcasting
int gc = 0; //nb of good message receive
int k = 4; //some treshold
int tmin = 10; //10 sec
int tmax = 300; //5 min
int tc = 10; //time current = T

int type = 0;

/* Sender history :
 * Detects duplicate callbacks at receiving nodes.
 * Duplicates appear when ack messages are lost.
*/
struct history_entry {
    struct history_entry *next;
    rimeaddr_t addr;
    uint8_t seq;
};
LIST(history_table);
LIST(history_tableRPL);
MEMB(history_mem, struct history_entry, NUM_HISTORY_ENTRIES);
MEMB(history_memRPL, struct history_entry, NUM_HISTORY_ENTRIES);


int maxAggregate = 1;
LIST(aggregate_data);
LIST(last_aggregate_data);

static uint8_t last_sent_data;

/* Behaviour of the sensor node :
   - 0 : data is sent periodically (every x seconds)
   - 1 : data is sent only when there is a change
   - 2 : data is not sent since there are no subscribers for temperature.
   - 3 : data is not sent since there are no subscribers for humidity.
   - 4 : data is not sent since there are no subscribers at all.
*/
static uint8_t config_sensor_data = 1;

child_node children[MAX_CHILDREN];
static int nb_children = 0;

/***
 *  ===========================================================================
 *  RPL : Handle child nodes
 *  ===========================================================================
 */

static void
send_child_ack() {
    /* Wait for the runicast channel to be available*/
    while(runicast_is_transmitting(&runicastRPL)){}
    packetbuf_clear();
    int max = ACK_CHILD;
    char buf[16];
    snprintf(buf, sizeof(buf), "%d", max);
    packetbuf_copyfrom(&buf, strlen(buf));
    runicast_send(&runicastRPL, &parent.node_addr, MAX_RETRANSMISSIONS);
    packetbuf_clear();
}

static void
send_child_remove(rimeaddr_t old_parent) {
    /* Wait for the runicast channel to be available*/
    while(runicast_is_transmitting(&runicastRPL)){}
    packetbuf_clear();
    int max = RM_CHILD;
    char buf[16];
    snprintf(buf, sizeof(buf), "%d", max);
    packetbuf_copyfrom(&buf, strlen(buf));
    runicast_send(&runicastRPL, &old_parent, MAX_RETRANSMISSIONS);
    packetbuf_clear();
}


static int
child_exists(child_node c){
    int exists = -1;
    int i;
    for(i = 0; i < nb_children; i++){
        if(children[i].node_addr.u8[0] == c.node_addr.u8[0] && children[i].node_addr.u8[1] == c.node_addr.u8[1]){
            return i;
        }
    }
    return exists;
}

void
remove_child(int index){
    int i;
    for (i = index; i < nb_children - 1; i++){
        children[i] = children[i + 1];
    }
    nb_children--;
}

/***
 *  ===========================================================================
 *  RPL : Reliable Unicast
 *  ===========================================================================
 */
void
recv_runicast(struct runicast_conn *c, const rimeaddr_t *from, uint8_t seqno) {
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
    char * payload = (char *) packetbuf_dataptr();
    uint8_t hops = (uint8_t *) atoi(payload);
    int message_code = atoi(payload);
    if (message_code == ACK_CHILD) {
        if(nb_children < MAX_CHILDREN){
            child_node child;
            child.node_addr.u8[0] = from->u8[0];
            child.node_addr.u8[1] = from->u8[1];
            if(child_exists(child) == -1){
                children[nb_children] = child;
                nb_children++;
                printf("%d - nb_child: %d \n", client.node_addr.u8[0], nb_children);
            }
        }
    } else if (message_code == RM_CHILD) {
        if(nb_children > 0){
            child_node child;
            child.node_addr.u8[0] = from->u8[0];
            child.node_addr.u8[1] = from->u8[1];
            int idx = child_exists(child);
            if(idx != -1) {
                remove_child(idx);
            }
        }
    } else {
        uint16_t last_rssi = packetbuf_attr(PACKETBUF_ATTR_RSSI);
        child_node child;
        child.node_addr.u8[0] = from->u8[0];
        child.node_addr.u8[1] = from->u8[1];
        if (USE_RSSI == 1 && last_rssi < parent.rssi && child_exists(child) == -1) {
            send_child_remove(parent.node_addr);
            parent.node_addr.u8[0] = from->u8[0];
            parent.node_addr.u8[1] = from->u8[1];
            parent.rssi = last_rssi;
            printf("RPL{RECONFIG2 - PARENT :%d.%d]} - %d : parent.hop\n",
                   parent.node_addr.u8[0], parent.node_addr.u8[1], parent.hop_dist);

            packetbuf_clear();
            char buf[8];
            snprintf(buf, sizeof(buf), "%d", client.hop_dist);
            packetbuf_copyfrom(&buf, strlen(buf));
            broadcast_send(&broadcastRPL);
            packetbuf_clear();
            /*rimeaddr_t receiver;
            receiver.u8[0] = from->u8[0];
            receiver.u8[1] = from->u8[1];
            hops = client.hop_dist;
            packetbuf_copyfrom(&hops, 1);
            runicast_send(&runicastRPL, &receiver, MAX_RETRANSMISSIONS);
            */
            send_child_ack();
            gc = 0;
            tc = tmin;
        }
        else if(USE_RSSI == 0 && hops < parent.hop_dist && child_exists(child) == -1) {
            send_child_remove(parent.node_addr);
            parent.node_addr.u8[0] = from->u8[0];
            parent.node_addr.u8[1] = from->u8[1];
            parent.hop_dist = hops;
            client.hop_dist = hops + 1;
            maxAggregate = client.hop_dist;
            if(maxAggregate < 1) maxAggregate = 1;
            printf("RPL{RECONFIG2 - PARENT :%d.%d]} - %d : parent.hop\n",
                   parent.node_addr.u8[0], parent.node_addr.u8[1], parent.hop_dist);
            send_child_ack();
            packetbuf_clear();
            char buf[8];
            snprintf(buf, sizeof(buf), "%d", client.hop_dist);
            packetbuf_copyfrom(&buf, strlen(buf));
            broadcast_send(&broadcastRPL);
            packetbuf_clear();
            // rimeaddr_t receiver;
            // receiver.u8[0] = from->u8[0];
            // receiver.u8[1] = from->u8[1];
            // hops = client.hop_dist;
            // packetbuf_copyfrom(&hops, 1);
            // runicast_send(&runicastRPL, &receiver, MAX_RETRANSMISSIONS);
            gc = 0;
            tc = tmin;
        }
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
    child_node child;
    child.node_addr.u8[0] = to->u8[0];
    child.node_addr.u8[1] = to->u8[1];
    int index = child_exists(child);

    if(parent.node_addr.u8[0] != 1) { //No reconfiguration for node directly connected to the root
        parent.node_addr.u8[0] = 0;
        parent.hop_dist = 254;
    } else if(index != -1) {
        remove_child(index);
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
    char * payload = (char *) packetbuf_dataptr();
    uint8_t hops = (uint8_t) atoi(payload);
    /* If we receive a direct communication from the broadcast we sent,
    we should update the parent node if the sender is closer */
    //printf("RPL{%d.%d <> RECEIVE-BROADCAST} - C = %d\n", from->u8[0], from->u8[1],gc);
    gc = gc+1;
    if(parent.node_addr.u8[0] == 0 && parent.node_addr.u8[1] == 0) { //No parent => Choose one
        if (USE_RSSI == 0 && hops < parent.hop_dist && hops != 253) {
            parent.node_addr.u8[0] = from->u8[0];
            parent.node_addr.u8[1] = from->u8[1];
            parent.hop_dist = hops;
            client.hop_dist = hops + 1;
            maxAggregate = client.hop_dist;
            if(maxAggregate < 1) maxAggregate = 1;
            printf("RPL{CHOOSE-PARENT:%d.%d]} - parent.hop : %d - %d : client.hop\n",
                   parent.node_addr.u8[0], parent.node_addr.u8[1], parent.hop_dist, client.hop_dist);
            send_child_ack();
            packetbuf_clear();
            char buf[8];
            snprintf(buf, sizeof(buf), "%d", client.hop_dist);
            packetbuf_copyfrom(&buf, strlen(buf));
            broadcast_send(&broadcastRPL);
            packetbuf_clear();
            process_start(&rime_update_process,NULL);
        } else if(USE_RSSI == 1 && last_rssi > parent.rssi) {
            parent.node_addr.u8[0] = from->u8[0];
            parent.node_addr.u8[1] = from->u8[1];
            parent.rssi = last_rssi;
            printf("RPL{CHOOSE-PARENT:%d.%d} - parent.hop : %d - %d : client.hop\n",
                   parent.node_addr.u8[0], parent.node_addr.u8[1], parent.hop_dist, client.hop_dist);
            send_child_ack();
            packetbuf_clear();
            char buf[8];
            snprintf(buf, sizeof(buf), "%d", client.hop_dist);
            packetbuf_copyfrom(&buf, strlen(buf));
            broadcast_send(&broadcastRPL);
            packetbuf_clear();
            process_start(&rime_update_process,NULL);
        }
    }
    else { //Already have parent
        if(USE_RSSI == 0 && hops == 253) { //Receive a request DIS from a node
            uint8_t hops = client.hop_dist;
            packetbuf_clear();
            char buf[8];
            snprintf(buf, sizeof(buf), "%d", hops);
            packetbuf_copyfrom(&buf, strlen(buf));
            rimeaddr_t receiver;
            receiver.u8[0] = from->u8[0];
            receiver.u8[1] = from->u8[1];
            runicast_send(&runicastRPL, &receiver, MAX_RETRANSMISSIONS);
            packetbuf_clear();
            gc = 0;
            tc = tmin;

        } else if(USE_RSSI == 0 && hops < parent.hop_dist){ //Find better parent => Replace current
            send_child_remove(parent.node_addr);
            parent.node_addr.u8[0] = from->u8[0];
            parent.node_addr.u8[1] = from->u8[1];
            parent.hop_dist = hops;
            client.hop_dist = hops + 1;
            maxAggregate = client.hop_dist;
            if(maxAggregate < 1) maxAggregate = 1;
            printf("RPL{RECONFIG - PARENT:%d.%d} - parent.hop : %d - %d : client.hop\n",
                   parent.node_addr.u8[0], parent.node_addr.u8[1], parent.hop_dist, client.hop_dist);
            gc = 0;
            tc = tmin;
            send_child_ack();
        } else if(USE_RSSI == 1 && last_rssi > parent.rssi) {
            send_child_remove(parent.node_addr);
            parent.node_addr.u8[0] = from->u8[0];
            parent.node_addr.u8[1] = from->u8[1];
            parent.rssi = last_rssi;
            printf("RPL{RECONFIG - PARENT:%d.%d} - parent.rssi : %d \n",
                   parent.node_addr.u8[0], parent.node_addr.u8[1], parent.rssi);
            send_child_ack();
            packetbuf_clear();
            char buf[8];
            snprintf(buf, sizeof(buf), "%d", client.hop_dist);
            packetbuf_copyfrom(&buf, strlen(buf));
            broadcast_send(&broadcastRPL);
            packetbuf_clear();
            process_start(&rime_update_process,NULL);
        } else {
            if(share == 5){ //Use to limit the number of response to broadcast message
                uint8_t hops = client.hop_dist;
                packetbuf_clear();
                char buf[8];
                snprintf(buf, sizeof(buf), "%d", hops);
                packetbuf_copyfrom(&buf, strlen(buf));
                broadcast_send(&broadcastRPL);
                packetbuf_clear();
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
        printf("SENDER{%d.%d[%d] > FORWARD > %u.%u}\n", from->u8[0], from->u8[1], seqno, parent.node_addr.u8[0], parent.node_addr.u8[1]);
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
 *  RECEIVER : Reliable Unicast from root node
 *  ===========================================================================
 */

static void
relay_config_data(char * s_payload) {
    while(runicast_is_transmitting(&runicastConfig)){}
    int len = strlen(s_payload);
    char buf[len];
    snprintf(buf, sizeof(buf), "%s", s_payload);
    packetbuf_clear();
    packetbuf_copyfrom(&buf, strlen(buf));
    int i;
    for(i = 0; i < nb_children; i++) {
        runicast_send(&runicastConfig, &children[i].node_addr, MAX_RETRANSMISSIONS);
    }
    packetbuf_clear();
}

static void
configuration_recv_runicast(struct runicast_conn *c, const rimeaddr_t *from, uint8_t seqno) {
    printf("SENDER{%d.%d[%d] > FORWARD > %d CHILDREN}\n", from->u8[0], from->u8[1], seqno, nb_children);

    char * payload = (char *) packetbuf_dataptr();
    uint8_t config = (uint8_t) atoi(payload);
    printf("CONFIG %d", config);
    if(config >= 0 && config <= 4){
        config_sensor_data = config;
        printf("New config for node %d", config);
        relay_config_data(payload);
    }
}

static void
configuration_sent_runicast(struct runicast_conn *c, const rimeaddr_t *to, uint8_t retransmissions) {
    printf("SENDER{%d.%d <> RETRANSMIT:%d}\n",
           to->u8[0], to->u8[1],retransmissions);

}

static void
configuration_timedout_runicast(struct runicast_conn *c, const rimeaddr_t *to, uint8_t retransmissions) {
    printf("SENDER{%d.%d <> TIMEOUT:%d}\n",
           to->u8[0], to->u8[1], retransmissions);

    child_node child;
    child.node_addr.u8[0] = to->u8[0];
    child.node_addr.u8[1] = to->u8[1];
    int index = child_exists(child);

    if(index != -1) {
        remove_child(index);
    }
}

static const struct runicast_callbacks configuration_runicast_callbacks = {configuration_recv_runicast,
                                                                           configuration_sent_runicast,
                                                                           configuration_timedout_runicast};


/***
 *  ===========================================================================
 *  RPL PROCESS : SENDER
 *  ===========================================================================
 */
PROCESS_THREAD(mini_rpl_process, ev, data) {
PROCESS_EXITHANDLER(broadcast_close(&broadcastRPL));
PROCESS_EXITHANDLER(runicast_close(&runicastRPL));
PROCESS_EXITHANDLER(runicast_close(&runicastMQTT));
PROCESS_EXITHANDLER(runicast_close(&runicastConfig));
PROCESS_BEGIN();

parent.node_addr.u8[0] = 0;
parent.node_addr.u8[1] = 0;
parent.hop_dist = 254;
parent.rssi = (signed) -65534;

client.node_addr.u8[0] = rimeaddr_node_addr.u8[0];
client.node_addr.u8[1] = rimeaddr_node_addr.u8[1];

list_init(history_table);
list_init(history_tableRPL);
list_init(aggregate_data);
memb_init(&history_mem);
memb_init(&history_memRPL);

runicast_open(&runicastMQTT, 145, &runicast_callbacksData);
runicast_open(&runicastRPL, 144, &runicast_callbacks);
broadcast_open(&broadcastRPL, 129, &broadcast_call);
runicast_open(&runicastConfig, 146, &configuration_runicast_callbacks);

static struct etimer et;
BROADCAST: while(parent.node_addr.u8[0] != 0) {
int time = random_rand() % 100 + 1; //1 -> 50 sec
etimer_set(&et,(time/2) *CLOCK_SECOND);
PROCESS_WAIT_EVENT_UNTIL(etimer_expired(&et));
process_start(&rime_sender_process,NULL);
}

while (parent.node_addr.u8[0] == 0 && parent.node_addr.u8[1] == 0) {
etimer_set(&et, 30 * CLOCK_SECOND);
PROCESS_WAIT_EVENT_UNTIL(etimer_expired(&et));
packetbuf_clear();
uint8_t hops = 253; //DIS message
char buf[8];
snprintf(buf, sizeof(buf), "%d", hops);
packetbuf_copyfrom(&buf, strlen(buf));
if(parent.node_addr.u8[0] == 0 && parent.node_addr.u8[1] == 0)  {
printf("RPL{DIS-Message}\n");
broadcast_send(&broadcastRPL);
}
packetbuf_clear();
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
int rd = random_rand() % 100 + 1;
if(rd % 2 == 0) type = 1;
else type = 2 ;
dpkt * pp = generateData(rimeaddr_node_addr.u8[0],type);
if (config_sensor_data == 0){
if(list_length(aggregate_data) < maxAggregate){
list_push(aggregate_data,pp);
}
else {
list_push(aggregate_data, pp);
last_sent_data = pp->data;
last_aggregate_data = aggregate_data;
const int size = list_length(aggregate_data);
dpkt array[size];// = malloc(sizeof(dpkt)*size);
int i = 0;
for(; i<size; i++){
dpkt *ppp = (dpkt *) list_chop(aggregate_data);
array[i] = *ppp;
printDPKT(&array[i] , parent.node_addr.u8[0],parent.node_addr.u8[1],"PRINT", "PUSH-SEND");
}
//printf("SIZE : %d - SIZE-Byte : %d\n",size, sizeof(array));
packetbuf_copyfrom(array,sizeof(array));
runicast_send(&runicastMQTT, &parent.node_addr, MAX_RETRANSMISSIONS);
list_init(aggregate_data);
}
} else if (config_sensor_data == 1){
if (pp->data != last_sent_data){
last_sent_data = pp->data;
dpkt array[1];
array[0] = *pp;
packetbuf_copyfrom(array,sizeof(array));
printDPKT(&array[0] , parent.node_addr.u8[0],parent.node_addr.u8[1],"PRINT", "SEND");
runicast_send(&runicastMQTT, &parent.node_addr, MAX_RETRANSMISSIONS);
}
} else if ((config_sensor_data == 2 && type == 2) || (config_sensor_data == 3 && type == 1) || config_sensor_data == 4){
// Do nothing because there are no subscribers
}
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
//printf("TRICKLE-TIMER{T = %d}\n",i);
PROCESS_WAIT_EVENT_UNTIL(etimer_expired(&et));
if(parent.node_addr.u8[0] != 0 && gc < k) {
packetbuf_clear();
char buf[8];
snprintf(buf, sizeof(buf), "%d", client.hop_dist);
packetbuf_copyfrom(&buf, strlen(buf));
broadcast_send(&broadcastRPL);
packetbuf_clear();
} else {
tc = 2*tc;
if(tc > tmax) tc = tmax;
}
}
PROCESS_END();
}


AUTOSTART_PROCESSES(&mini_rpl_process);