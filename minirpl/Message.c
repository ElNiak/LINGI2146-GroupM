#include <stdio.h>
#include <stdlib.h>

#define MAX_RETRANSMISSIONS 4 // 0 because no doublon detector
#define NUM_HISTORY_ENTRIES 4
#define USE_RSSI 0

struct dpkt;
typedef struct dpkt
{
    uint8_t data;
    uint8_t id;
    uint8_t topic;
    uint8_t neg; //moins de byte avec neg et data que un int de 32bit
} dpkt;

struct ru_node;
typedef struct ru_node
{
    rimeaddr_t node_addr;
    uint8_t hop_dist; //rank
    uint16_t rssi;
} node;

