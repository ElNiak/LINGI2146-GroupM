#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>

#include "contiki.h"
#include "net/rime.h"

#include "lib/list.h"
#include "lib/memb.h"

#include "dev/button-sensor.h"
#include "dev/leds.h"
#include "dev/uart0.h"
#include "dev/serial-line.h"

#include "DataGenerator.c"

/*---------------------------------------------------------------------------*/
PROCESS(mini_rpl_process, "RPLRoot implementation");
PROCESS(rime_receiver_process, "MQTTReceiver implementation");
/*---------------------------------------------------------------------------*/

/***
 *  ===========================================================================
 *  Instance Variable
 *  ===========================================================================
 */
node client; //represent the current root state

child_node children[MAX_CHILDREN];
static int nb_children = 0;


int gc = 0; //nb of good message receive
int k = 4; //some treshold
int tmin = 10;
int tmax = 60 * 5;
int tc = 10;

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
static struct runicast_conn runicastRPL;
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
    uint8_t hops = (uint8_t) atoi(payload);
    int message_code = atoi(payload);
    printf("Root{%d.%d[%d] > RECEIVE} - %d\n",
           from->u8[0], from->u8[1], seqno, message_code);
    if (message_code == ACK_CHILD){ // Verify if the message is an ACK child message
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
    }
}

void
sent_runicast(struct runicast_conn *c, const rimeaddr_t *to, uint8_t retransmissions) {
    printf("Root{%d.%d <> RETRANSMIT:%d}\n",
           to->u8[0], to->u8[1], retransmissions);
}

void
timedout_runicast(struct runicast_conn *c, const rimeaddr_t *to, uint8_t retransmissions) {
    printf("Root{%d.%d <> TIMEOUT:%d}\n",
           to->u8[0], to->u8[1], retransmissions);

    child_node child;
    child.node_addr.u8[0] = to->u8[0];
    child.node_addr.u8[1] = to->u8[1];
    int index = child_exists(child);
    if(index != -1){
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
static struct broadcast_conn broadcastRPL;
void
broadcast_recv(struct broadcast_conn *c, const rimeaddr_t *from) {
    uint16_t last_rssi = packetbuf_attr(PACKETBUF_ATTR_RSSI);
    char * payload = (char *) packetbuf_dataptr();
    uint8_t hops = (uint8_t) atoi(payload);
    gc++;
    if(USE_RSSI == 0 && hops == 253) { //Receive a request DIS from a node
        uint8_t hops = 0;
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
    }
    //printf("Root{%d.%d <> R-BROADCAST}\n",from->u8[0], from->u8[1]);
}

static const struct broadcast_callbacks broadcast_call = {broadcast_recv};

/***
 *  ===========================================================================
 *  RECEIVER : Reliable Unicast from root node
 *  ===========================================================================
 */
static struct runicast_conn runicastConfig;

static void
configuration_recv_runicast(struct runicast_conn *c, const rimeaddr_t *from, uint8_t seqno) {
    printf("SENDER{%d.%d[%d] > FORWARD > %d CHILDREN}\n", from->u8[0], from->u8[1], seqno, nb_children);
}

static void
configuration_sent_runicast(struct runicast_conn *c, const rimeaddr_t *to, uint8_t retransmissions) {
    printf("SENDER{%d.%d <> RETRANSMIT:%d}\n", to->u8[0], to->u8[1],retransmissions);
}

static void
configuration_timedout_runicast(struct runicast_conn *c, const rimeaddr_t *to, uint8_t retransmissions) {
    printf("SENDER{%d.%d <> TIMEOUT:%d}\n", to->u8[0], to->u8[1], retransmissions);

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
 *  RECEIVER : Transmit configuration messages to child nodes
 *  ===========================================================================
 */

// static char rx_buf[SERIAL_BUF_SIZE];
// static int rx_buf_index;
// static void uart_rx_callback(unsigned char c) {
//     if(c != '\n'){
//         rx_buf[rx_buf_index] = c;
//     }
//     if(c == '\n' || c == EOF || c == '\0'){
//         printf("%s\n", (char *)rx_buf);
//         packetbuf_clear();
//         rx_buf[strcspn ( rx_buf, "\n" )] = '\0';
//         packetbuf_copyfrom(rx_buf, strlen(rx_buf));

//         //Send the config to all the child nodes
//         int i;
//         for(i = 0; i < nb_children; i++) {
//             runicast_send(&runicastConfig, &children[i].node_addr, MAX_RETRANSMISSIONS);
//         }

//         memset(rx_buf, 0, rx_buf_index);
//         rx_buf_index = 0;
//     } else {
//         rx_buf_index = rx_buf_index + 1;
//     }
// }


/***
 *  ===========================================================================
 *  RPL PROCESS : ROOT
 *  ===========================================================================
 */
PROCESS_THREAD(mini_rpl_process, ev, data) {
    PROCESS_EXITHANDLER(broadcast_close(&broadcastRPL));
    PROCESS_EXITHANDLER(runicast_close(&runicastRPL));

    PROCESS_BEGIN();

    client.node_addr.u8[0] = rimeaddr_node_addr.u8[0];
    client.node_addr.u8[1] = rimeaddr_node_addr.u8[1];
    client.hop_dist = 0;
    list_init(history_tableRPL);
    memb_init(&history_memRPL);
    runicast_open(&runicastRPL, 144, &runicast_callbacks);

    static struct etimer et;

    broadcast_open(&broadcastRPL, 129, &broadcast_call);

    // uart0_init(BAUD2UBR(115200));
    // uart0_set_input(uart_rx_callback);

    etimer_set(&et,5 *CLOCK_SECOND);
    PROCESS_WAIT_EVENT_UNTIL(etimer_expired(&et));
    packetbuf_clear();
    char buf[8];
    snprintf(buf, sizeof(buf), "%d", 0);
    packetbuf_copyfrom(&buf, strlen(buf));
    broadcast_send(&broadcastRPL);
    packetbuf_clear();

    BROADCAST : while(1) {
        int randompercentage = random_rand() % 100 + 1;//0-100%
        randompercentage = randompercentage/2;//0-50%
        int i = (tc/2) + (int) ((double)(1/(double)randompercentage) * tc);
        //printf("TRICKLE-TIMER{T = %d}\n",i);
        etimer_set(&et,i *CLOCK_SECOND);
        PROCESS_WAIT_EVENT_UNTIL(etimer_expired(&et));
        if(gc < k) {
            packetbuf_clear();
            char buf[8];
            snprintf(buf, sizeof(buf), "%d", 0);
            packetbuf_copyfrom(&buf, strlen(buf));
            broadcast_send(&broadcastRPL);
            packetbuf_clear();
        } else {
            tc = 2*tc;
            if(tc > tmax) tc = tmax;
        }
    }

    goto BROADCAST;

    PROCESS_END();
}

/***
 *  ===========================================================================
 *  RECEIVER : Reliable Unicast
 *  ===========================================================================
 */
static struct runicast_conn runicastMQTT;
void
recv_runicastData(struct runicast_conn *c, const rimeaddr_t *from, uint8_t seqno) {
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
    const int size = packetbuf_datalen()/sizeof(dpkt);
    dpkt* array = packetbuf_dataptr();
    int i = 0;
    for(; i < size;i++){
        dpkt* pp = &array[i];
        printDPKT(pp,from->u8[0],from->u8[2],"BROKER", "RECEIVE");
        if(pp->neg == 1){ //negative data (e.g -5 degree)
            printf("# %u %u -%u \n", pp->id, pp->topic, pp->data);
        }
        else { //positive value -> reason why this : difficult to produce negative random value
            printf("# %u %u %u \n", pp->id, pp->topic, pp->data);
        }
    }
}

void
sent_runicastData(struct runicast_conn *c, const rimeaddr_t *to, uint8_t retransmissions) {
    printf("BROKER{%d.%d <> RETRANSMIT:%d}\n",
           to->u8[0], to->u8[1], retransmissions);
}

void
timedout_runicastData(struct runicast_conn *c, const rimeaddr_t *to, uint8_t retransmissions) {
    printf("BROKER{%d.%d <> TIMEOUT:%d}\n",
           to->u8[0], to->u8[1], retransmissions);
}

static const struct runicast_callbacks runicast_callbacksData = {recv_runicastData,
                                                                 sent_runicastData,
                                                                 timedout_runicastData};

/***
 *  ===========================================================================
 *  MQTT PROCESS : Receiver
 *  ===========================================================================
 */
PROCESS_THREAD(rime_receiver_process, ev, data) {
    PROCESS_EXITHANDLER(runicast_close(&runicastMQTT));
    PROCESS_BEGIN();
    list_init(history_table);
    memb_init(&history_mem);
    runicast_open(&runicastMQTT, 145, &runicast_callbacksData);
    PROCESS_END();
}

PROCESS(listen_gateway, "Listening messages from the gateway");

PROCESS_THREAD(listen_gateway, ev, data){
    PROCESS_EXITHANDLER(runicast_close(&runicastConfig));
    PROCESS_BEGIN();
    runicast_open(&runicastConfig, 146, &configuration_runicast_callbacks);
    for(;;) {
        PROCESS_YIELD();
        if(ev == serial_line_event_message) {
            char *d = (char *)data;
            printf("received line : %s. Send to %d\n", d, nb_children);
            packetbuf_copyfrom(d, strlen(d));
            //Send the config to all the child nodes
            int i;
            for(i = 0; i < nb_children; i++) {
                printf("Child %d, (%d,%d)", i, children[i].node_addr.u8[0], children[i].node_addr.u8[1]);
                runicast_send(&runicastConfig, &children[i].node_addr, MAX_RETRANSMISSIONS);
            }
            packetbuf_clear();
        }
    }
    PROCESS_END();
}

AUTOSTART_PROCESSES(&rime_receiver_process,&mini_rpl_process, &listen_gateway);