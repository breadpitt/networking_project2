//
// Created by Nathan Evans on 5/15/18
#include <stdint.h>
#ifndef TCP_PROJECT2_P2P_H
#define TCP_PROJECT2_P2P_H

#define MAX_SEGMENT_SIZE 1400
#define DEBUG 0

/**
 * Header included with every P2P message sent.
 * Includes a type, and length.
 * The length includes the header and all data sent with it.
 */
struct P2PHeader {
    uint16_t type; // Type of message, one of P2PMessageTypes
    uint16_t length; // Length of full message, including this header
    unsigned char msg_hash[32]; // SHA-256 Hash of entire message (excluding this header)
} __attribute__ ((packed));

enum P2PMessageTypes {
    CONTROL_MSG = 777,
    DATA_MSG,
    ERROR_MSG
};

struct ControlMessage {
    struct P2PHeader header;
    uint16_t control_type; // Type of control message, one P2PControlTypes
} __attribute__ ((packed));

struct ErrorMessage {
  struct P2PHeader header;
  uint16_t error_type; // Type of error message, one P2PErrorTypes
} __attribute__ ((packed));

enum P2PErrorTypes {
  INCORRECT_MESSAGE_TYPE = 20,
  INCORRECT_MESSAGE_SIZE,
  INCORRECT_MESSAGE_DIGEST
};

enum P2PControlTypes {
    CONNECT = 1223,
    CONNECT_OK,
    DISCONNECT,
    FIND_PEERS,
    GOSSIP_PEERS
};

struct DataMessage {
    struct P2PHeader header;
    uint16_t data_type; // Type of data message, one P2PDataTypes
} __attribute__ ((packed));;

enum P2PDataTypes {
    SEND_MESSAGE = 1337,
    FORWARD_MESSAGE,
    GET_MESSAGE_HISTORY,
    SEND_MESSAGE_HISTORY
};


struct PeerInfo {
  uint16_t peer_listen_port; // Listen port of this peer
  uint32_t ipv4_address; // ipv4 address of this peer
  uint32_t ipv6_address[4]; // ipv6 address of this peer
} __attribute__ ((packed));

/** BEGIN CONTROL MESSAGE STRUCTURES **/

/**
 * Connect message, sent from one peer to a non-connected
 * other peer in the network. Upon connecting, peers may exchange
 * data messages.
 *
 * ConnectMessage is just a convenience struct; the connect message
 * is just the ControlMessage followed by a PeerInfo
 */
struct ConnectMessage {
    struct ControlMessage control_header; // Control message header
    struct PeerInfo peer_data;
} __attribute__ ((packed));

// Disconnect message is just a ControlMessage with type DISCONNECT
// No additional struct/data is needed.

/**
 * Message sent to find new peers to connect to.
 */
struct FindPeersMessage {
    struct ControlMessage control_header; // Control message header
    uint16_t max_results; // Maximum results to return
    // if set to AF_INET, only return v4 address,
    // if set to AF_INET6 only return v6 addresses,
    // if set to 0, return both types
    uint16_t restrict_results;
}__attribute__ ((packed));

/**
 * Message sent as a response to a FIND_PEERS message with known peers.
 */
struct GossipPeersMessage {
    struct ControlMessage control_header; // Control message header
    uint16_t num_results; // Number of results returned in this message (must fit within MAX_SEGMENT_SIZE)
}__attribute__ ((packed)); // num_results PeerInfo's follow this message



/** END CONTROL MESSAGE STRUCTURES **/

/** BEGIN DATA MESSAGE STRUCTURES **/

struct Message {
  struct PeerInfo sender;
  uint64_t send_time;
  uint16_t nickname_length;
  uint16_t message_length;
}__attribute__ ((packed)); // Actual nickname and message follow this struct

struct SendMessage {
    struct DataMessage data_header;
    struct Message message; // The message
}__attribute__ ((packed));

struct ForwardMessage {
    struct DataMessage data_header;
    struct Message message; // The message
}__attribute__ ((packed));

struct GetHistory {
    struct DataMessage data_header;
    uint16_t request_id;
    uint64_t since_time;
}__attribute__ ((packed));

struct SendHistory {
    struct DataMessage data_header;
    uint16_t request_id;
    uint16_t num_responses;
}__attribute__ ((packed)); // MessageData struct's follow this message


/** END DATA MESSAGE STRUCTURES **/

#endif //TCP_PROJECT2_P2P_H
