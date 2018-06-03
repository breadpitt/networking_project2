#include <iostream>
#include <sys/socket.h>
#include <sys/select.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <vector>
#include <sys/types.h>
#include <netdb.h>

#include "TCPServer.h"
#include "P2P.h"

bool TCPListen(int server_socket, int max_connections){ // need to use scope resolution :: ?
        int ret; 
        ret = listen(server_socket, 50);
        if (ret != 0){
            perror("listen failure");
            close(server_socket);
            return false;
        }
        return true;
}

bool create_TCPSocket(char* listen_hostname, char*listen_port){
    
    server_socket = socket(AF_INET, SOCK_STREAM, 0);
    // Make sure socket was created successfully, or exit.
    if (server_socket == -1) {
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
    ret = getaddrinfo(listen_hostname, listen_port, &hints, &results);
    if (ret != 0) {
        std::cerr << "Getaddrinfo failed with error " << ret << std::endl;
        perror("getaddrinfo");
        return false;
    }

    results_it = results;
    ret = -1;

    while (results_it != NULL) {
        ret = bind(server_socket, results_it->ai_addr,
                   results_it->ai_addrlen);
        if (ret == 0) {
            break;
        }
        perror("bind");
        results_it = results_it->ai_next;
    }
    // Always free the result of calling getaddrinfo
    freeaddrinfo(results);

    if (ret != 0) {
        std::cerr << "Failed to bind to any addresses. Be sure to specify a local address/hostname, and an unused port?"
                  << std::endl;
        return false;
    } 
    max_fd = 0;

    while (stop == false) {
        FD_ZERO(&read_set);
        FD_ZERO(&write_set);

        // Mark the server_socket in the read set
        // If this is then set, it means we need to accept a new connection.
        FD_SET(server_socket, &read_set);

        if (server_socket > max_fd)
            max_fd = server_socket + 1;

        // For each client, set the appropriate descriptors in the select sets
        for (int i = 0; i < client_list.size(); i++) {
            // Lazy-legacy check. If we don't remove a client immediately set the vector entry to NULL
            if (client_list[i] == NULL) {
                continue;
            }

            // Always check if the client has sent us data
            FD_SET(client_list[i]->get_fd(), &read_set);

            // Check if client has data to send. If so, add it to the write_set
            // If there isn't data to write, don't set it (prevents pegging CPU)
            if (client_list[i]->bytes_ready_to_send() > 0) {
                FD_SET(client_list[i]->get_fd(), &write_set);
            }

            if (client_list[i]->get_fd() > max_fd)
                max_fd = client_list[i]->get_fd() + 1;
        }

        // If select hasn't returned after 5 seconds, return anyways so other asynchronous events can be triggered
        // HINT: send a find_peer request?
        timeout.tv_sec = 5;
        timeout.tv_usec = 0;

        ret = select(max_fd + 1, &read_set, &write_set, NULL, &timeout);

        if (ret == -1) {
            perror("select");
            continue;
        }

        // Check if server_socket is in the read set. If so, a new client has connected to us!
        if (FD_ISSET(server_socket, &read_set)) {
            temp_fd = accept(server_socket, (struct sockaddr *) &incoming_client, &incoming_client_len);
            if (temp_fd == -1) {
                perror("accept");
                continue;
            }
            // Create a new TCPClient from the connection
            temp_client = new TCPClient(temp_fd, &incoming_client, incoming_client_len);
            // Add the new client to the list of clients we have
            client_list.push_back(temp_client);
        }

        for (int i = 0; i < client_list.size(); i++) {
            // Lazy-legacy check. If we don't remove a client immediately set the vector entry to NULL
            if (client_list[i] == NULL) {
                continue;
            }

            // Check if this client has sent us data
            if (FD_ISSET(client_list[i]->get_fd(), &read_set)) {
                ret = recv(client_list[i]->get_fd(), recv_buf, DEFAULT_BUFFER_SIZE, 0);
                if (ret == -1) {
                    perror("recv");
                    // On error, something bad bad has happened to this client. Remove.
                    close(client_list[i]->get_fd());
                    client_list.erase(client_list.begin() + i);
                    return false;
                    break;
                } else if (ret == 0) {
                    // On 0 return, client has initiated connection shutdown.
                    close(client_list[i]->get_fd());
                    client_list.erase(client_list.begin() + i);
                    return false;
                    break;
                } else {
                    // Add the newly received data to the client buffer
                    client_list[i]->add_recv_data(recv_buf, ret);
                }
            }

            // Check if this client has sent us data
            if ((client_list[i]->bytes_ready_to_send() > 0) && (FD_ISSET(client_list[i]->get_fd(), &write_set))) {
                // Store how many bytes this client has ready to send
                int bytes_to_send = client_list[i]->bytes_ready_to_send();
                // Copy send bytes into our local send buffer
                client_list[i]->get_send_data(send_buf, DEFAULT_BUFFER_SIZE);
                // Finally, send the data to the client.
                ret = send(client_list[i]->get_fd(), send_buf, bytes_to_send, 0);
                if (ret == -1) {
                    perror("send");
                    // On error, something bad bad has happened to this client. Remove.
                    close(client_list[i]->get_fd());
                    client_list.erase(client_list.begin() + i);
                    return false;
                    break;
                }
            }

            // Finally, process any incoming client data. For this silly example, if we have received data
            // just add it to the same client's send buffer.
            if (client_list[i]->bytes_ready_to_recv() > 0) {
                // Store how many bytes are ready to be handled
                int bytes_to_process = client_list[i]->bytes_ready_to_recv();

                // Read the data into a temporary buffer
                client_list[i]->get_recv_data(scratch_buf, DEFAULT_BUFFER_SIZE);
                // Add the data we received from this client into the send_buffer
                // Next time through the while loop, this will be detected and the data
                // will get sent out.
                client_list[i]->add_send_data(scratch_buf, bytes_to_process);
            }
        }
    }
    return true;
}

