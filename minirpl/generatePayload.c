//
// Created by Crochet Christophe on 07.05.2019.
//
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <assert.h>
#include <math.h> //gcc -lm

char * generateData(char * ci, char * topic) {
    assert(strlen(topic) == 1);
    double data = (-50.0) + (rand()/(RAND_MAX/(90.0 - (-50.0))));
    double roundedData = round(data * 100) /100;
    int i = 0;
    if(roundedData < 0)
        i = 1;
    char outData[i+5+1]; //"-"+"44.44"+'\0'
    snprintf(outData, i+6, "%f", roundedData);
    char * res = (char *) malloc(sizeof(ci) + 1 + sizeof(outData)+1);
    strcat(res,ci);
    strcat(res,topic);
    strcat(res,outData);
    return res;
}

int main( int argc, const char* argv[]) {
    for(int i = 0 ; i < 10; i ++)
        printf("%d : %s\n",i,generateData("1","2"));
}
