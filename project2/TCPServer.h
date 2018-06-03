//
// Created by nate on 5/14/18.
//

#ifndef CLASS_BASED_TCP_EXAMPLE_TCPServer_H
#define CLASS_BASED_TCP_EXAMPLE_TCPServer_H

#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <string.h>
#include <unistd.h>

#define DEFAULT_BUFFER_SIZE 4096

class TCPServer {
public:
  TCPServer() {
    socket_fd = 0;
    memset(&client_addr, 0, sizeof(struct sockaddr_storage));
    memset(&client_listen_address, 0, sizeof(struct sockaddr_storage));
    memset(send_buffer, 0, DEFAULT_BUFFER_SIZE);
    memset(recv_buffer, 0, DEFAULT_BUFFER_SIZE);
    send_buf_offset = 0;
    recv_buf_offset = 0;
  }

  TCPServer(int fd, struct sockaddr_storage *client_address, socklen_t client_address_len) : TCPServer() {
    socket_fd = fd;
    memcpy(&client_addr, client_address, client_address_len);
    client_addr_len = client_address_len;
  }

  ~TCPServer() {
    close(socket_fd);
  }


    bool create_TCPSocket(char* listen_hostname, char *listen_port);
    bool TCPListen(int server_socket, int max_connections);

  unsigned int bytes_ready_to_send();
  unsigned int bytes_ready_to_recv();
  int get_fd();
  bool add_send_data(char *data, unsigned int data_len);
  bool get_recv_data(char *buf, unsigned int buf_len);
  bool get_send_data(char *buf, unsigned int buf_len);
  bool add_recv_data(char *data, unsigned int data_len);
  /* Get a printable version of the client address. Non-reentrant */
  const char *get_printable_address();
  const char *get_printable_listen_address();
  void add_client_listen_address(struct sockaddr_storage *client_listen_address, socklen_t client_listen_address_length);
  struct sockaddr_storage *get_client_listen_address();

private:
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

    char *listen_hostname = NULL;
    char *listen_port = NULL;

    int server_socket;
    int temp_fd;

    int ret;
    bool stop = false;

    fd_set read_set;
    fd_set write_set;
    int max_fd;
};


#endif //CLASS_BASED_TCP_EXAMPLE_TCPServer_H
