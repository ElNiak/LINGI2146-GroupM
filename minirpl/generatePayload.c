//
// Created by Crochet Christophe on 07.05.2019.
//
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
//#include <math.h> //gcc -lm
//gcc minirpl/generatePayload.c -o  generate -lm

char * generateDataT(char * ci) { // -50d - 80d
    int data = (-50.0) + rand() % (80+1 -(-50));
   // double roundedData = (data * 100) /100;
    int i = 0;
    if(data < 0)
        i = 1;
    char outData[i+3]; //"-"+"44"+'\0'
    snprintf(outData, i+3, "%d", data);
    char * res = (char *) malloc(sizeof(ci) + 1 + sizeof(outData)+1);
    strcat(res,ci);
    strcat(res,"2");
    strcat(res,outData);
    return res;
}

char * generateDataH(char * ci) { // 0 - 100%
    int data = (0) + rand() % (100+1 -(0));
   // double roundedData = (data * 100) /100;
    int i = 0;
    if(data < 0)
        i = 1;
    char outData[i+3]; //"-"+"44"+'\0'
    snprintf(outData, i+3, "%d", data);
    char * res = (char *) malloc(sizeof(ci) + 1 + sizeof(outData)+1);
    strcat(res,ci);
    strcat(res,"1");
    strcat(res,outData);
    return res;
}
