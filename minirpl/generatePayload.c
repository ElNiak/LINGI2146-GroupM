//
// Created by Crochet Christophe on 07.05.2019.
//
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
//#include <math.h> //gcc -lm
//gcc minirpl/generatePayload.c -o  generate -lm

char * generateData(char * ci, char * topic) {
    if(strlen(topic) != 1)
	return NULL;
    int data = (-50.0) + rand() % (80+1 -(-50));
   // double roundedData = (data * 100) /100;
    int i = 0;
    if(data < 0)
        i = 1;
    char outData[i+3]; //"-"+"44"+'\0'
    snprintf(outData, i+3, "%d", data);
    char * res = (char *) malloc(sizeof(ci) + 1 + sizeof(outData)+1);
    strcat(res,ci);
    strcat(res,topic);
    strcat(res,outData);
    return res;
}
