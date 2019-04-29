//
// Created by Crochet Christophe on 29.04.2019.
// http://www.steves-internet-guide.com/python-mqttsn-client/
// http://www.steves-internet-guide.com/mqtt-sn/
// /!\ http://www.mqtt.org/new/wp-content/uploads/2009/06/MQTT-SN_spec_v1.2.pdf

#ifndef LINGI2146_GROUPM_DATA_MESSAGE_H
#define LINGI2146_GROUPM_DATA_MESSAGE_H

#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <stdint.h>

// All message type possible
#define ADVERTISE 0x00
#define GWINFO 0x02
#define CONNECT 0x04
#define WILLTOPICREQ 0x06
#define WILLMSGREQ 0x08
#define REGISTER 0x0A
#define PUBLISH 0x0C
#define PUBCOMP 0x0E
#define PUBREL 0x10
#define SUBSCRIBE 0x12
#define UNSUBSCRIBE 0x14
#define PINGREQ 0x16
#define DISCONNECT 0x18
#define WILLTOPICUPD 0x1A
#define WILLMSGUPD 0x1C
#define SEARCHGW 0x01
#define CONNACK 0x05
#define WILLTOPIC 0x07
#define WILLMSG 0x09
#define REGACK 0x0B
#define PUBACK 0x0D
#define PUBREC 0x0F
#define SUBACK 0x13
#define UNSUBACK 0x15
#define PINGRESP 0x17
#define WILLTOPICRESP 0x1B
#define WILLMSGRESP 0x1D

//Return code
#define ACCEPTED 0x00
#define REJECTED_C 0x01
#define REJECTED_ITID 0x02
#define REJECTED_NS 0x03



//GENERAL FORMAT
struct mqttsn_message_format {
    //=== header ===
    uint8_t message_type; //cf define
    uint8_t message_len; //1 byte => maximum len = 256 bytes

    //=== variable part ===
    char * clientid; //size [1,23]
    uint16_t duration; //specifies the duration of a time period in seconds => max = 18h
    char * payload; //payload of an MQTT PUBLISH message => variable length

    // => flags
    uint8_t topic_id_type : 2; // * message contains a normal topic id (set to “0b00”) * a pre-defined topic id (set to “0b01”) * or a short topic name (set to “0b10”)
    uint8_t clean_session : 1;
    uint8_t will : 1; //unused
    uint8_t retain : 1;
    uint8_t qos : 2;
    uint8_t dup : 1; //1 bit => "0" = message is sent for the first time & “1” if retransmitted
    // <= flags

    uint8_t gateway_addr; // TODO : can be delete ? since 1 gateway for us
    uint8_t gateway_id;

    uint16_t messageid;
    uint8_t protocolid;
    uint8_t radius; // indicates the value of the broadcast radius, 0x00 = broadcast to all nodes in the network
    uint8_t returncode;

    uint8_t topicID; //0x01 => Temperature , 0x02 => Humidity
    char * topicname;
};

struct adv_message {
    //=== header ===
    uint8_t message_type; //cf define
    uint8_t message_len; //1 byte => maximum len = 256 bytes

    uint8_t gateway_id;
    uint16_t duration;
};

struct searchGW_message { //May be unecessary if all data about the GW cached in the sensors
    //=== header ===
    uint8_t message_type; //cf define
    uint8_t message_len; //1 byte => maximum len = 256 bytes

    uint8_t radius;
};

struct gwInfo_message {
    //=== header ===
    uint8_t message_type; //cf define
    uint8_t message_len; //1 byte => maximum len = 256 bytes

    uint8_t gateway_addr;
    uint8_t gateway_id;
};


//register/connect message usefull ?

struct publish_message {
    //=== header ===
    uint8_t message_type; //cf define
    uint8_t message_len; //1 byte => maximum len = 256 bytes

    // => flags
    uint8_t topic_id_type : 2; // * message contains a normal topic id (set to “0b00”) * a pre-defined topic id (set to “0b01”) * or a short topic name (set to “0b10”)
    uint8_t clean_session : 1;
    uint8_t will : 1; //unused
    uint8_t retain : 1;
    uint8_t qos : 2;
    uint8_t dup : 1; //1 bit => "0" = message is sent for the first time & “1” if retransmitted
    // <= flags

    uint8_t topicID; //0x01 => Temperature , 0x02 => Humidity
    uint16_t messageid;
    char * topicname;
};

struct publishack_message {
    //=== header ===
    uint8_t message_type; //cf define
    uint8_t message_len; //1 byte => maximum len = 256 bytes

    uint16_t messageid;
    char * topicname;
    uint8_t returncode;
};




#endif //LINGI2146_GROUPM_DATA_MESSAGE_H
