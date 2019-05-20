//
// Created by Crochet Christophe on 07.05.2019.
//
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include "random.h"
#include "Message.c"

void printDPKT(dpkt *r, const unsigned char from, const unsigned char from2,char * client, char* act){
    if(r->neg == 1) printf("%s{%d.%d > %s} - payload = (id:%u, topic:%u, data:-%u) \n",client, from, from2, act,r->id, r->topic, r->data);
    else printf("%s{%d.%d > %s} - payload = (id:%u, topic:%u, data:%u) \n",client, from,  from2, act,r->id, r->topic,  r->data);
}

dpkt * generateData(uint8_t ci, int s) {
    if(s == 2) {
        dpkt * res = malloc(sizeof(dpkt));
        int data = random_rand() % 100 + 1;//-100C <-> 100C
        if(data%2 == 1){ res->neg = 1; data /= 2;}
        else res->neg = 0;
        res->data = data;
        res->id = ci;
        res->topic =  2;
        return res;
    }
    else if(s == 1){
        dpkt * res = malloc(sizeof(dpkt));
        int data = random_rand() % 100 + 1;//0-100%
        res->neg =  0;
        res->data =  data;
        res->id = ci;
        res->topic =  1;
        return res;
    }
    else
        return NULL;
}