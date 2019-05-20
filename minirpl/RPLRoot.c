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
AUTOSTART_PROCESSES(&rime_receiver_process,&mini_rpl_process);
/*---------------------------------------------------------------------------*/

/***
 *  ===========================================================================
 *  Instance Variable
 *  ===========================================================================
 */
node client; //represent the current root state

child_node children[MAX_CHILDREN];
static int nb_children = 0;


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
    uint8_t *hops = (uint8_t *) packetbuf_dataptr();
    printf("Root{%d.%d[%d] > RECEIVE} - %u\n",
           from->u8[0], from->u8[1], seqno, *hops);
    int message_code = (int) hops;
    if (message_code == ACK_CHILD){ // Verify if the message is an ACK child message
        if(nb_children < MAX_CHILDREN){
            child_node c;
            c.node_addr.u8[0] = from->u8[0];
            c.node_addr.u8[1] = from->u8[1];
            if(child_exists(c) == -1){
                children[nb_children] = c;
                nb_children++;
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
    printf("Root{%d.%d <> R-BROADCAST}\n",
           from->u8[0], from->u8[1]);

}

static const struct broadcast_callbacks broadcast_call = {broadcast_recv};

/***
 *  ===========================================================================
 *  RECEIVER : Reliable Unicast
 *  ===========================================================================
 */
static struct runicast_conn runicastMQTT;
void
recv_runicastData(struct runicast_conn *c, const rimeaddr_t *from, uint8_t seqno) {
    printDPKT((dpkt *)packetbuf_dataptr(),
              from->u8[0],from->u8[1],"BROKER", "RECEIVE");
    /*rimeaddr_t receiver;
    receiver.u8[0] = from->u8[0];
    receiver.u8[1] = from->u8[1];
    runicast_send(&runicastRPL, &receiver, MAX_RETRANSMISSIONS);*/
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

	if(index != -1){
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

static char rx_buf[SERIAL_BUF_SIZE]; 
static int rx_buf_index; 
static void uart_rx_callback(unsigned char c) { 
    if(c != '\n'){
		rx_buf[rx_buf_index] = c;
	}  
    if(c == '\n' || c == EOF || c == '\0'){ 
        printf("%s\n", (char *)rx_buf);
        packetbuf_clear();
        rx_buf[strcspn ( rx_buf, "\n" )] = '\0';
        packetbuf_copyfrom(rx_buf, strlen(rx_buf));

        //Send the config to all the child nodes
        int i;
        for(i = 0; i < nb_children; i++) {
            runicast_send(&runicastConfig, &children[i].node_addr, MAX_RETRANSMISSIONS);
        }

        memset(rx_buf, 0, rx_buf_index); 
        rx_buf_index = 0; 
    } else { 
        rx_buf_index = rx_buf_index + 1; 
    } 
}       

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

    runicast_open(&runicastRPL, 144, &runicast_callbacks);

    static struct etimer et;

    broadcast_open(&broadcastRPL, 129, &broadcast_call);

    uart0_init(BAUD2UBR(115200)); 
  	uart0_set_input(uart_rx_callback);

    etimer_set(&et,5 *CLOCK_SECOND);
    PROCESS_WAIT_EVENT_UNTIL(etimer_expired(&et));
    packetbuf_copyfrom(0, 1);
    broadcast_send(&broadcastRPL);

    BROADCAST : while(1) { //TODO
        etimer_set(&et,15 *CLOCK_SECOND);
        PROCESS_WAIT_EVENT_UNTIL(etimer_expired(&et));
        packetbuf_copyfrom(0, 1);
        broadcast_send(&broadcastRPL);
    }

    goto BROADCAST;

    PROCESS_END();
}

/***
 *  ===========================================================================
 *  MQTT PROCESS : Receiver
 *  ===========================================================================
 */
PROCESS_THREAD(rime_receiver_process, ev, data) {
    PROCESS_EXITHANDLER(runicast_close(&runicastMQTT));

    PROCESS_BEGIN();
    runicast_open(&runicastMQTT, 145, &runicast_callbacksData);

    PROCESS_END();
}
