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
#define ADVERTISE 0x00    //broadcasted periodically by a gateway to advertise its presence
#define SEARCHGW 0x01     //broadcasted by a client when it searches for a GW
#define GWINFO 0x02       //response to a SEARCHGW message using the broadcast service of the underlying layer
#define CONNECT 0x04      //sent by a client to setup a connection
#define CONNACK 0x05      //sent by the server in response to a connection request from a client
#define WILLTOPICREQ 0x06 //sent by the GW to request a client for sending the Will topic name
#define WILLTOPIC 0x07    //sent by a client as response to the WILLTOPICREQ message for transferring its Will topic name to the GW
#define WILLMSGREQ 0x08   //sent by the GW to request a client for sending the Will message
#define WILLMSG 0x09      //sent by a client as response to a WILLMSGREQ for transferring its Will message to the GW
#define REGISTER 0x0A     //sent by a client to a GW for requesting a topic id value for the included topic name
#define REGACK 0x0B       //sent by a client or by a GW as an acknowledgment to the receipt and processing of a REGISTER message.
#define PUBLISH 0x0C      //used by both clients and gateways to publish data for a certain topic
#define PUBACK 0x0D       //is sent by a gateway or a client as an acknowledgment to the receipt
#define SUBSCRIBE 0x12    //used by a client to subscribe to a certain topic name
#define SUBACK 0x13       //sent by a gateway to a client as an acknowledgment to SUBSCRIBE
#define UNSUBSCRIBE 0x14  //sent by the client to the GW to unsubscribe from named topics
#define UNSUBACK 0x15     //sent by a GW to acknowledge the receipt and processing
#define PINGREQ 0x16      //”are you alive” message that is sent from or received by a connected client
#define PINGRESP 0x17     //”yes I am alive”
#define DISCONNECT 0x18
#define WILLTOPICUPD 0x1A //sent by a client to update its Will topic name stored in the GW/server
#define WILLMSGUPD 0x1C   //sent by a client to update its Will message stored in the GW/server
#define WILLTOPICRESP 0x1B//sent by a GW to acknowledge
#define WILLMSGRESP 0x1D  //sent by a GW to acknowledge the receipt and processing

//Return code
#define ACCEPTED 0x00
#define REJECTED_C 0x01
#define REJECTED_ITID 0x02
#define REJECTED_NS 0x03

typedef struct connect_message connect;
typedef struct connack_message connectack;
typedef struct willtopicreq_message willtreq;
typedef struct willtopic_message willt;
typedef struct willmsgreq_message willmreq;
typedef struct willmsg_message willm;
typedef struct register_message regist;
typedef struct regack_message registack;
typedef struct publish_message publish;
typedef struct publishack_message puback;
typedef struct subscribe_message suscrib;
typedef struct suback_message suback;
typedef struct unsubscribe_message unsuscrib;
typedef struct unsuback_message unsuback;
typedef struct disconnect_message disconnect;

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

    uint16_t messageid;
    uint8_t protocolid;
    uint8_t radius; // indicates the value of the broadcast radius, 0x00 = broadcast to all nodes in the network
    uint8_t returncode;

    uint8_t topicID; //0x01 => Temperature , 0x02 => Humidity
    char * topicname;
    char * willmsg;
    char * willtopic;
};

//register/connect message usefull ?

struct connect_message {
    //=== header ===
    uint8_t message_type;// = CONNECT; //cf define
    uint8_t message_len; //1 byte => maximum len = 256 bytes

    // => flags
    uint8_t topic_id_type : 2; // * message contains a normal topic id (set to “0b00”) * a pre-defined topic id (set to “0b01”) * or a short topic name (set to “0b10”)
    uint8_t clean_session : 1;
    uint8_t will : 1; //unused
    uint8_t retain : 1;
    uint8_t qos : 2;
    uint8_t dup : 1; //1 bit => "0" = message is sent for the first time & “1” if retransmitted
    // <= flags

    uint8_t protocolid;
    uint16_t duration; //specifies the duration of a time period in seconds => max = 18h
    char * clientid; //size [1,23]
};

struct connack_message {
    //=== header ===
    uint8_t message_type;// = CONNACK; //cf define
    uint8_t message_len; //1 byte => maximum len = 256 bytes
    uint8_t returncode;
};

struct willtopicreq_message {
    //=== header ===
    uint8_t message_type;// = WILLTOPICREQ; //cf define
    uint8_t message_len; //1 byte => maximum len = 256 bytes
};

struct willtopic_message {
    //=== header ===
    uint8_t message_type;// = WILLTOPIC; //cf define
    uint8_t message_len; //1 byte => maximum len = 256 bytes

    // => flags
    uint8_t topic_id_type : 2; // * message contains a normal topic id (set to “0b00”) * a pre-defined topic id (set to “0b01”) * or a short topic name (set to “0b10”)
    uint8_t clean_session : 1;
    uint8_t will : 1; //unused
    uint8_t retain : 1;
    uint8_t qos : 2;
    uint8_t dup : 1; //1 bit => "0" = message is sent for the first time & “1” if retransmitted
    // <= flags

    char * willtopic;
};

struct willmsgreq_message {
    //=== header ===
    uint8_t message_type;// = WILLMSGREQ; //cf define
    uint8_t message_len; //1 byte => maximum len = 256 bytes
};

struct willmsg_message {
    //=== header ===
    uint8_t message_type;// = WILLMSG; //cf define
    uint8_t message_len; //1 byte => maximum len = 256 bytes

    char * willmsg;
};

struct register_message {
    //=== header ===
    uint8_t message_type;// = REGISTER; //cf define
    uint8_t message_len; //1 byte => maximum len = 256 bytes

    uint8_t topicID; //0x01 => Temperature , 0x02 => Humidity
    uint16_t messageid;
    char * topicname;
};

struct regack_message {
    //=== header ===
    uint8_t message_type;// = REGACK; //cf define
    uint8_t message_len; //1 byte => maximum len = 256 bytes

    uint8_t topicID; //0x01 => Temperature , 0x02 => Humidity
    uint16_t messageid;
    uint8_t returncode;
};

struct publish_message {
    //=== header ===
    uint8_t message_type;// = PUBLISH; //cf define
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
    char * payload; //payload of an MQTT PUBLISH message => variable length
};

struct publishack_message {
    //=== header ===
    uint8_t message_type;// = PUBACK; //cf define
    uint8_t message_len; //1 byte => maximum len = 256 bytes

    uint8_t topicID; //0x01 => Temperature , 0x02 => Humidity
    uint16_t messageid;
    uint8_t returncode;
};

struct subscribe_message {
    //=== header ===
    uint8_t message_type;// = SUBSCRIBE; //cf define
    uint8_t message_len; //1 byte => maximum len = 256 bytes

    // => flags
    uint8_t topic_id_type : 2; // * message contains a normal topic id (set to “0b00”) * a pre-defined topic id (set to “0b01”) * or a short topic name (set to “0b10”)
    uint8_t clean_session : 1;
    uint8_t will : 1; //unused
    uint8_t retain : 1;
    uint8_t qos : 2;
    uint8_t dup : 1; //1 bit => "0" = message is sent for the first time & “1” if retransmitted
    // <= flags

    uint16_t messageid;
    uint8_t topicID; //0x01 => Temperature , 0x02 => Humidity
};

struct suback_message {
    //=== header ===
    uint8_t message_type;// = SUBACK; //cf define
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
    uint8_t returncode;
};

struct unsubscribe_message {
    //=== header ===
    uint8_t message_type;// = UNSUBSCRIBE; //cf define
    uint8_t message_len; //1 byte => maximum len = 256 bytes

    // => flags
    uint8_t topic_id_type : 2; // * message contains a normal topic id (set to “0b00”) * a pre-defined topic id (set to “0b01”) * or a short topic name (set to “0b10”)
    uint8_t clean_session : 1;
    uint8_t will : 1; //unused
    uint8_t retain : 1;
    uint8_t qos : 2;
    uint8_t dup : 1; //1 bit => "0" = message is sent for the first time & “1” if retransmitted
    // <= flags

    uint16_t messageid;
    uint8_t topicID; //0x01 => Temperature , 0x02 => Humidity
};

struct unsuback_message {
    //=== header ===
    uint8_t message_type;// = UNSUBACK; //cf define
    uint8_t message_len; //1 byte => maximum len = 256 bytes

    uint16_t messageid;
};

struct disconnect_message {
    //=== header ===
    uint8_t message_type;// = DISCONNECT; //cf define
    uint8_t message_len; //1 byte => maximum len = 256 bytes
};

publish * generateRandomPublishMessage();

#endif //LINGI2146_GROUPM_DATA_MESSAGE_H
