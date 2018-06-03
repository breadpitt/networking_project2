#include <iostream>
#include <sys/socket.h>
#include <sys/select.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <vector>
#include <sys/types.h>
#include <netdb.h>
#include <openssl/sha.h>
#include "TCPClient.h"
#include "P2P.h"

/* TODO: 
- Update makefile
- Debug getaddrinfo
- Your program must also connect to other peers using connect (e.g., act as a client)
- Your peer must be able to send/receive messages from other peers, regardless of how the connection was initiated
- Your program must use select to enable support for multiple connections DONE
- Upon request, your node must respond to find peer requests 
    (node must store known nodes by server IP address and PORT, and send them to other peers upon request)
- On startup, your peer will connect to a "seed node". 
    Within 30 seconds of startup after connecting to this node, 
        your peer must send a peer lookup request to this seed node to find new peers to connect to
- Your peer must periodically send peer lookup requests to other connected peers to attempt to find new peers to connect to
- Your peer must forward messages received to other connected peers
- Your program should initially take four arguments: 
    SERVER_HOST PORT SEED_HOST SEED_PORT, where SERVER_HOST is the hostname or IP address of your node, 
        PORT is the listen port for your node, SEED_HOST is the hostname or port of the initial peer you'll connect to
             and SEED_PORT is the server port of the seed node
- Upon startup your program needs to ask the user to enter a nickname to be used when sending messages
- After initial startup, your program needs to monitor stdin (fd 0)
     as part of its select routine to take in chat messages from the user
- When messages are received from the user, they will be encapsulated and sent to all connected peers
- When previously unseen messages are received from other peers, they will be displayed on stdout
- When a peer disconnects, your client should remove it from its list of connected and known peers
*/

int checkConnectMessage(char *buf); // parses the connect message header recieved

int main(int argc, char *argv[])
{

    int client_port = 5555;
    sockaddr_in dest_addr;
    socklen_t client_addr_len;

    TCPClient *client;

    struct sockaddr_storage incoming_client;
    socklen_t incoming_client_len;
    std::vector<TCPClient *> client_list;
    TCPClient *temp_client;
    char recv_buf[DEFAULT_BUFFER_SIZE];
    char send_buf[DEFAULT_BUFFER_SIZE];
    char scratch_buf[DEFAULT_BUFFER_SIZE];
    struct timeval timeout;

    struct addrinfo hints;
    struct addrinfo *results;
    struct addrinfo *results_it;

    char *server_hostname = NULL;
    char *server_port = NULL;
    char *seed_hostname = NULL;
    char *seed_port = NULL;
    char *temp_server_hostname = NULL;
    int server_socket;
    int temp_fd;

    int ret;
    bool stop = false;

    fd_set read_set;
    fd_set write_set;
    int max_fd;

    if ((argc != 5))
    {
        std::cerr << "Specify LISTEN_HOST server_port as first two arguments (test network is: lincoln.cs.du.edu 17777)." << std::endl;
        return 1;
    }

    server_hostname = argv[1];
    server_port = argv[2];
    seed_hostname = argv[3];
    seed_port = argv[4];

    // Create TCP sockets.
    // AF_INET is the address family used for IPv4 addresses
    // SOCK_STREAM indicates creation of a TCP socket
    server_socket = socket(AF_INET, SOCK_STREAM, 0);
    int temp_client_socket = socket(AF_INET, SOCK_STREAM, 0);
    // Make sure socket was created successfully, or exit.
    if (temp_client_socket == -1)
    {
        std::cerr << "Failed to create tcp socket!" << std::endl;
        std::cerr << strerror(errno) << std::endl;
        return 1;
    }

    if (server_socket == -1)
    {
        std::cerr << "Failed to create tcp socket!" << std::endl;
        std::cerr << strerror(errno) << std::endl;
        return 1;
    }

    memset(&hints, 0, sizeof(struct addrinfo));
    hints.ai_addr = NULL;
    hints.ai_canonname = NULL;
    hints.ai_family = AF_INET;
    hints.ai_protocol = 0;
    hints.ai_flags = AI_PASSIVE;
    hints.ai_socktype = SOCK_STREAM;
    ret = getaddrinfo(server_hostname, server_port, &hints, &results);
    if (ret != 0)
    {
        std::cerr << "Getaddrinfo failed with error " << ret << std::endl;
        perror("getaddrinfo");
        return 1;
    }
    results_it = results;
    ret = -1;

    while (results_it != NULL)
    {
        ret = bind(server_socket, results_it->ai_addr,
                   results_it->ai_addrlen);
        if (ret == 0)
        {
            break;
        }
        perror("bind");
        results_it = results_it->ai_next;
    }
    // Always free the result of calling getaddrinfo
    freeaddrinfo(results);

    if (ret != 0)
    {
        std::cerr << "Failed to bind to any addresses. Be sure to specify a local address/hostname, and an unused port?"
                  << std::endl;
        return 1;
    }

    // Listen on the server socket with a max of 50 outstanding connections.
    ret = listen(server_socket, 50);

    if (ret != 0)
    {
        perror("listen");
        close(server_socket);
        return 1;
    }
    max_fd = 0;

    memset(&hints, 0, sizeof(struct addrinfo));
    hints.ai_addr = NULL;
    hints.ai_canonname = NULL;
    hints.ai_family = AF_INET;
    hints.ai_protocol = 0;
    hints.ai_flags = AI_PASSIVE;
    hints.ai_socktype = SOCK_STREAM;
    ret = getaddrinfo(seed_hostname, seed_port, &hints, &results);
    if (ret != 0)
    {
        std::cerr << "Getaddrinfo failed with error " << gai_strerror(ret) << std::endl;
        perror("getaddrinfo");
        return 1;
    }
    results_it = results;
    ret = -1;
    freeaddrinfo(results);

    // Connect to init peer
    ret = connect(temp_client_socket, results_it->ai_addr, results_it->ai_addrlen);
    // Always free the result of calling getaddrinfo
    if (ret == 0)
    {
        std::cerr << "Successful connect to init peer!" << std::endl;
    }
    else
    {
        std::cerr << "Failed to connect to init peer!" << std::endl;
        std::cerr << strerror(errno) << std::endl;
        return 1;
    }

    client = new TCPClient(temp_client_socket, (struct sockaddr_storage *)&results->ai_addr, sizeof(results->ai_addr));
    client_list.push_back(client);

    // Structure to fill in with connect message info
    struct ConnectMessage connect_message;
    // Zero out structure
    memset(&connect_message, 0, sizeof(struct ConnectMessage));
    connect_message.control_header.header.type = htons(CONTROL_MSG);
    connect_message.control_header.header.length = htons(sizeof(struct ConnectMessage));
    connect_message.control_header.control_type = htons(CONNECT);
    ret = inet_pton(server_socket, server_hostname, temp_server_hostname); // ret 1 on success
    connect_message.peer_data.peer_listen_port = client_port;

    if (seed_hostname != NULL)
    { // initially our server is it's own peer?
        memcpy(&connect_message.peer_data.ipv4_address, &temp_server_hostname, sizeof(struct in_addr));
    }

    unsigned char *temp_ptr = (unsigned char *)&connect_message;

    uint16_t send_size;
    send_size = sizeof(ConnectMessage);

    // Create hash after filling in rest of message.
    //update_message_digest(&connect_message.control_header.header);
    SHA256(&temp_ptr[sizeof(P2PHeader)], send_size - sizeof(P2PHeader), connect_message.control_header.header.msg_hash);

    std::cout << "Size of struct Connect Message: " << sizeof(connect_message) << std::endl;
    ControlMessage testcontrol;
    std::cout << "Size of struct Control Message: " << sizeof(testcontrol) << std::endl;

    P2PHeader testheader;
    std::cout << "Size of struct P2P header: " << sizeof(testheader) << std::endl;

    if (client->add_send_data((char *)&connect_message, sizeof(connect_message)) != true)
    {
        std::cerr << "Failed to add send data to client!" << std::endl;
    }

    while (stop == false)
    {
        FD_ZERO(&read_set);
        FD_ZERO(&write_set);

        // Mark the server_socket in the read set
        // If this is then set, it means we need to accept a new connection.
        FD_SET(server_socket, &read_set);
        std::cout << "success1\n";

        if (server_socket > max_fd)
            max_fd = server_socket + 1;

        // For each client, set the appropriate descriptors in the select sets
        for (int i = 0; i < client_list.size(); i++)
        {
            // Lazy-legacy check. If we don't remove a client immediately set the vector entry to NULL

            if (client_list[i] == NULL)
            {
                continue;
            }

            // Always check if the client has sent us data
            FD_SET(client_list[i]->get_fd(), &read_set);

            // Check if client has data to send. If so, add it to the write_set
            // If there isn't data to write, don't set it (prevents pegging CPU)
            if (client_list[i]->bytes_ready_to_send() > 0)
            {
                FD_SET(client_list[i]->get_fd(), &write_set);
            }

            if (client_list[i]->get_fd() > max_fd)
                max_fd = client_list[i]->get_fd() + 1;
        }
        std::cout << "success3\n";

        // If select hasn't returned after 5 seconds, return anyways so other asynchronous events can be triggered
        // HINT: send a find_peer request?
        timeout.tv_sec = 5;
        timeout.tv_usec = 0;

        ret = select(max_fd + 1, &read_set, &write_set, NULL, &timeout);

        if (ret == -1)
        {
            perror("select");
            continue;
        }

        // Check if server_socket is in the read set. If so, a new client has connected to us!
        if (FD_ISSET(server_socket, &read_set))
        {
            temp_fd = accept(server_socket, (struct sockaddr *)&incoming_client, &incoming_client_len);
            if (temp_fd == -1)
            {
                perror("accept");
                continue;
            }
            // Create a new TCPClient from the connection
            temp_client = new TCPClient(temp_fd, &incoming_client, incoming_client_len);
            // Add the new client to the list of clients we have
            client_list.push_back(temp_client);
        }

        for (int i = 0; i < client_list.size(); i++)
        {
            // Lazy-legacy check. If we don't remove a client immediately set the vector entry to NULL
            if (client_list[i] == NULL)
            {
                continue;
            }

            // Check if this client has sent us data
            if (FD_ISSET(client_list[i]->get_fd(), &read_set))
            {
                ret = recv(client_list[i]->get_fd(), recv_buf, DEFAULT_BUFFER_SIZE, 0);
                if (ret == -1)
                {
                    perror("recv");
                    // On error, something bad bad has happened to this client. Remove.
                    close(client_list[i]->get_fd());
                    client_list.erase(client_list.begin() + i);
                    break;
                }
                else if (ret == 0)
                {
                    // On 0 return, client has initiated connection shutdown.
                    close(client_list[i]->get_fd());
                    client_list.erase(client_list.begin() + i);
                    break;
                }
                else
                {
                    // Add the newly received data to the client buffer
                    client_list[i]->add_recv_data(recv_buf, ret);
                }
            }

            // Check if this client has sent us data
            if ((client_list[i]->bytes_ready_to_send() > 0) && (FD_ISSET(client_list[i]->get_fd(), &write_set)))
            {
                // Store how many bytes this client has ready to send
                int bytes_to_send = client_list[i]->bytes_ready_to_send();
                // Copy send bytes into our local send buffer
                client_list[i]->get_send_data(send_buf, DEFAULT_BUFFER_SIZE);
                // Finally, send the data to the client.
                ret = send(client_list[i]->get_fd(), send_buf, bytes_to_send, 0);
                std::cout << "Bytes sent: " << ret << "\n";

                if (ret == -1)
                {
                    perror("send");
                    // On error, something bad bad has happened to this client. Remove.
                    close(client_list[i]->get_fd());
                    client_list.erase(client_list.begin() + i);
                    break;
                }
            }

            // Finally, process any incoming client data. For this silly example, if we have received data
            // just add it to the same client's send buffer.
            if (client_list[i]->bytes_ready_to_recv() > 0)
            {
                // Store how many bytes are ready to be handled
                int bytes_to_process = client_list[i]->bytes_ready_to_recv();

                // Read the data into a temporary buffer
                client_list[i]->get_recv_data(scratch_buf, DEFAULT_BUFFER_SIZE);
                checkConnectMessage(scratch_buf);
                // Add the data we received from this client into the send_buffer
                // Next time through the while loop, this will be detected and the data
                // will get sent out.
                client_list[i]->add_send_data(scratch_buf, bytes_to_process);
            }
        }
    }
    /*
    */
}

int checkConnectMessage(char *buf)
{   
    ConnectMessage connect_message;
    ControlMessage control_header;
    P2PHeader p2pheader; // 36 bytes
    PeerInfo peerdata;
    uint16_t control_type;
    uint16_t p2ptype;   // p2p message type
    uint16_t p2plength; // length of message including this header
    uint16_t listenport;
    //uint32_t ipv4addr;

    memcpy(&control_header, &buf[0], sizeof(control_header)); // copy control header from the connect message buffer
    memcpy(&p2pheader, &control_header.header, sizeof(p2pheader));
    memcpy(&control_type, &control_header.control_type, sizeof(control_type));

    memcpy(&peerdata, &buf[sizeof(control_header)], sizeof(peerdata));
    memcpy(&listenport, &peerdata.peer_listen_port, sizeof(peerdata.peer_listen_port));
    //memcpy(&ipv4addr, &peerdata.ipv4_address, sizeof(peerdata.ipv4_address));

    switch(ntohs(control_type)){
        case 1223 : std::cout << "Connect message: CONNECT\n";
                    break;
        case 1224 : std::cout << "Connect message: CONNECT_OK\n";
                    break;
        case 1225 : std::cout << "Connect message: DISCONNECT\n";
                    break;  
        case 1226 : std::cout << "Connect message: FIND_PEERS\n";
                    break;
        case 1227 : std::cout << "Connect message: GOSSIP_PEERS\n";
                    break;                  
    }
    //std::cout << "Connect message: " << ntohs(control_type) << "!\n";

    std::cout << "peer listen port: " << ntohs(listenport) << "!\n";
    //std::cout << "peer ipv4address: " << ntohl(ipv4addr) << "!\n";

    memcpy(&p2ptype, &p2pheader, sizeof(p2ptype));
    memcpy(&p2plength, &p2pheader, sizeof(p2plength));

    switch(ntohs(p2ptype)){
        case 777 : std::cout << "P2P type: CONTROL_MSG\n";
                    break;
        case 778 : std::cout << "P2P type: CONTROL_MSG\n";
                    break;
        case 779 : std::cout << "P2P type: CONTROL_MSG\n";
                    break;    
    }
    std::cout << "p2p message type: " << ntohs(p2ptype) << "!\n";
    std::cout << "p2p message length: " << ntohs(p2plength) << "!\n";
    
    connect_message.peer_data.ipv4_address = ntohl(peerdata.ipv4_address);
    struct in_addr ipv4_address = {connect_message.peer_data.ipv4_address};

    char *address;
    address = inet_ntoa(ipv4_address);
    std::cout << "address: " << address << "!\n";
    
    /*
    
enum P2PMessageTypes {
    CONTROL_MSG = 777,
    DATA_MSG,
    ERROR_MSG
};


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



enum P2PDataTypes {
    SEND_MESSAGE = 1337,
    FORWARD_MESSAGE,
    GET_MESSAGE_HISTORY,
    SEND_MESSAGE_HISTORY
};

    
    
    
    
    if (ntohs(control_type) != CONNECT_OK){
        std::cout << "Connect message recieve error\n";
    } else {
        std::cout << "Connect message OK!\n";
    }*/

    return 0;
}
