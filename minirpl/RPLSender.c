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
static struct runicast_conn runicastConfig;

int share = 0;

int c = 0;
int k = 0.5;

uint8_t last_sent_data;

/* Behaviour of the sensor node :
   - 0 : data is sent periodically (every x seconds)
   - 1 : data is sent only when there is a change
   - 2 : data is not sent since there are no subscribers for its subject.
*/
uint8_t config_sensor_data = 0;

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
	uint16_t max = ACK_CHILD;
	char buf[16];
	snprintf(buf, sizeof(buf), "%d", max);
	packetbuf_copyfrom(&buf, strlen(buf));
	runicast_send(&runicastRPL, &parent.node_addr, MAX_RETRANSMISSIONS);
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
    uint8_t *hops = (uint8_t *) packetbuf_dataptr();
    int message_code = (int) hops;
    if (message_code == ACK_CHILD) {
        if(nb_children < MAX_CHILDREN){
            child_node c;
            c.node_addr.u8[0] = from->u8[0];
            c.node_addr.u8[1] = from->u8[1];
            if(child_exists(c) == -1){
                children[nb_children] = c;
                nb_children++;
            }
        }
    } else {
        uint16_t last_rssi = packetbuf_attr(PACKETBUF_ATTR_RSSI);
        child_node c;
        c.node_addr.u8[0] = from->u8[0];
        c.node_addr.u8[1] = from->u8[1];
        if (USE_RSSI == 1 && last_rssi < parent.rssi && child_exists(c) == -1) {
            parent.node_addr.u8[0] = from->u8[0];
            parent.node_addr.u8[1] = from->u8[1];
            parent.rssi = last_rssi;
            printf("RPL{RECONFIG2 - PARENT :%d.%d]} - %d : parent.hop\n",
                parent.node_addr.u8[0], parent.node_addr.u8[1], parent.hop_dist);
            /*
            uint8_t * hops = client.hop_dist;
            packetbuf_copyfrom(&hops, 1);
            broadcast_send(&broadcastRPL);
            rimeaddr_t receiver;
            receiver.u8[0] = from->u8[0];
            receiver.u8[1] = from->u8[1];
            hops = client.hop_dist;
            packetbuf_copyfrom(&hops, 1);
            runicast_send(&runicastRPL, &receiver, MAX_RETRANSMISSIONS);
            */
            send_child_ack();
        }
        else if(USE_RSSI == 0 && *hops < parent.hop_dist && child_exists(c) == -1) {
            parent.node_addr.u8[0] = from->u8[0];
            parent.node_addr.u8[1] = from->u8[1];
            parent.hop_dist = *hops;
            client.hop_dist = *hops + 1;
            printf("RPL{RECONFIG2 - PARENT :%d.%d]} - %d : parent.hop\n",
                parent.node_addr.u8[0], parent.node_addr.u8[1], parent.hop_dist);

            send_child_ack();

            uint8_t hops = client.hop_dist;
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
    uint8_t *hops = (uint8_t *) packetbuf_dataptr();
    /* If we receive a direct communication from the broadcast we sent,
    we should update the parent node if the sender is closer */
    printf("RPL{%d.%d <> RECEIVE-BROADCAST}\n", from->u8[0], from->u8[1]);
    if(parent.node_addr.u8[0] == 0) { //No parent => Choose one
        if (USE_RSSI == 0 && *hops < parent.hop_dist && *hops != 253) {
            parent.node_addr.u8[0] = from->u8[0];
            parent.node_addr.u8[1] = from->u8[1];
            parent.hop_dist = *hops;
            client.hop_dist = *hops + 1;
            printf("RPL{CHOOSE-PARENT:%d.%d]} - parent.hop : %d - %d : client.hop\n",
                   parent.node_addr.u8[0], parent.node_addr.u8[1], parent.hop_dist, client.hop_dist);
            uint8_t hops = client.hop_dist;
            packetbuf_copyfrom(&hops, 1);
            broadcast_send(&broadcastRPL);
            //Si on utilise des rooting table
            /*rimeaddr_t receiver;
            receiver.u8[0] = from->u8[0];
            receiver.u8[1] = from->u8[1];
            packetbuf_copyfrom(&hops, 1);
            runicast_send(&runicastRPL, &receiver, MAX_RETRANSMISSIONS);*/
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
            uint8_t hops = client.hop_dist;
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
            printf("RPL{RECONFIG - PARENT:%d.%d} - parent.hop : %d - %d : client.hop\n",
                   parent.node_addr.u8[0], parent.node_addr.u8[1], parent.hop_dist, client.hop_dist);
        }
        else {
            if(share == 5){
                uint8_t hops = client.hop_dist;
                packetbuf_copyfrom(&hops, 1);
                broadcast_send(&broadcastRPL);
                share = 0;
            }
            else {
                share++;
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
	if(config >= 0 && config <= 2){
		config_sensor_data = config;
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
        uint8_t hops = 253; //DIS message
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
        if (config_sensor_data == 0){
            last_sent_data = pp->data;
            packetbuf_copyfrom((void *) pp, 4);
            printDPKT(pp, parent.node_addr.u8[0],parent.node_addr.u8[1],"SENDER", "SENT");
            runicast_send(&runicastMQTT, &parent.node_addr, MAX_RETRANSMISSIONS);
        } else if (config_sensor_data == 1){
            if (pp->data != last_sent_data){
                last_sent_data = pp->data;
                packetbuf_copyfrom((void *) pp, 4);
                printDPKT(pp, parent.node_addr.u8[0],parent.node_addr.u8[1],"SENDER", "SENT");
                runicast_send(&runicastMQTT, &parent.node_addr, MAX_RETRANSMISSIONS);
            }
        } else if (config_sensor_data == 2){
            // Do nothing because there are no subscribers
        }
    }
    PROCESS_END();
}

