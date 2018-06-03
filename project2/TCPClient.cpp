//
// Created by nate on 5/14/18.
//

#include "TCPClient.h"
#include <errno.h>
#include <stdio.h>
#include <iostream>

unsigned int TCPClient::bytes_ready_to_send() {
  return send_buf_offset;
}

unsigned int TCPClient::bytes_ready_to_recv() {
  return recv_buf_offset;
}

int TCPClient::get_fd() {
  return socket_fd;
}

bool TCPClient::add_send_data(char *data, unsigned int data_len) {
  // Don't add more data into send buffer if it's full (or would overflow)
  if (data_len + send_buf_offset > DEFAULT_BUFFER_SIZE) {
    return false;
  } else {
    memcpy(&send_buffer[send_buf_offset], data, data_len);
    send_buf_offset += data_len;
    return true;
  }
}

bool TCPClient::get_recv_data(char *buf, unsigned int buf_len) {
  // Ensure the receive buffer is big enough to hold the data we're getting
  if (buf_len < recv_buf_offset)
    return false;
  else if (recv_buf_offset == 0)
    return false;
  else {
#if DEBUG
    std::cerr << "Copying " << recv_buf_offset << " bytes into recv buffer." << std::endl;
#endif
    memcpy(buf, recv_buffer, recv_buf_offset);
    recv_buf_offset = 0;
    return true;
  }
}

const char *TCPClient::get_printable_address() {
  // Buffer will be big enough for either a v4 or v6 address
  // AND big enough to put :65535 (the port) at the end.
  static char print_buf[NI_MAXHOST + NI_MAXSERV];
  static char host_buf[NI_MAXHOST];
  static char port_buf[NI_MAXSERV];

  int ret;
  // Verify address family is either v4 or v6
  switch (client_addr.ss_family) {
    case AF_INET:
      break;
    case AF_INET6:
      break;
    default:
      return nullptr;
  }

  // If we get here, we're good to go!
  ret = getnameinfo((struct sockaddr *)&client_addr, client_addr_len,
                    host_buf, NI_MAXHOST,
                    port_buf, NI_MAXSERV, NI_NUMERICHOST | NI_NUMERICSERV);
  if (ret != 0) {
    std::cout << "getnameinfo error " << gai_strerror(errno) << std::endl;
    return nullptr;
  }

  strncpy(print_buf, host_buf, NI_MAXHOST);
  print_buf[strlen(host_buf)] = ':';
  strncpy(&print_buf[strlen(host_buf) + 1], port_buf, NI_MAXSERV);

  return print_buf;
}

const char *TCPClient::get_printable_listen_address() {
  // Buffer will be big enough for either a v4 or v6 address
  // AND big enough to put :65535 (the port) at the end.
  static char print_buf[NI_MAXHOST + NI_MAXSERV];
  static char host_buf[NI_MAXHOST];
  static char port_buf[NI_MAXSERV];

  int ret;
  // Verify address family is either v4 or v6
  switch (client_listen_address.ss_family) {
    case AF_INET:
      break;
    case AF_INET6:
      break;
    default:
      return nullptr;
  }

  // If we get here, we're good to go!
  ret = getnameinfo((struct sockaddr *)&client_listen_address, client_listen_address_len,
                    host_buf, NI_MAXHOST,
                    port_buf, NI_MAXSERV, NI_NUMERICHOST | NI_NUMERICSERV);
  if (ret != 0) {
    std::cout << "getnameinfo error " << gai_strerror(errno) << std::endl;
    return nullptr;
  }

  strncpy(print_buf, host_buf, NI_MAXHOST);
  print_buf[strlen(host_buf)] = ':';
  strncpy(&print_buf[strlen(host_buf) + 1], port_buf, NI_MAXSERV);

  return print_buf;
}

bool TCPClient::get_send_data(char *buf, unsigned int buf_len) {
  if (send_buf_offset == 0) // Nothing to send!
  {
    std::cerr << "Asked for send data, but nothing to send!" << std::endl;
    return false;
  }
  if (buf_len < send_buf_offset) {
    std::cerr << "Need to send " << send_buf_offset << " bytes but buffer only " << buf_len << std::endl;
  }
  memcpy(buf, send_buffer, send_buf_offset);
  send_buf_offset = 0;
  return true;
}

bool TCPClient::add_recv_data(char *data, unsigned int data_len) {
  // Don't add more data into send buffer if it's full (or would overflow)
  if (data_len + recv_buf_offset > DEFAULT_BUFFER_SIZE) {
    return false;
  } else {
#if DEBUG
    std::cerr << "add_recv_data copying " << data_len << " bytes into buffer at offset " << recv_buf_offset << std::endl;
#endif
    memcpy(&recv_buffer[recv_buf_offset], data, data_len);
    recv_buf_offset += data_len;
    return true;
  }
}

void TCPClient::add_client_listen_address(struct sockaddr_storage *client_listen_addr,
                                          socklen_t client_listen_addrlen) {

  if (client_listen_addr->ss_family == AF_INET) {
    memcpy(&client_listen_address, client_listen_addr, sizeof(struct sockaddr_in));
    client_listen_address_len = client_listen_addrlen;
  }
  else if (client_listen_addr->ss_family == AF_INET6) {
    memcpy(&client_listen_address, client_listen_addr, sizeof(struct sockaddr_in6));
    client_listen_address_len = client_listen_addrlen;
  } else {
    std::cerr << "Unknown address family when trying to add listen address!" << std::endl;
  }
#if DEBUG
  std::cerr << "Added client listen address " << get_printable_listen_address() << std::endl;
#endif
}

struct sockaddr_storage *TCPClient::get_client_listen_address() {
  static struct sockaddr_storage null_address;
  if (0 == memcmp(&client_listen_address, &null_address, sizeof(struct sockaddr_storage))) {
    return NULL;
  } else {
    return &client_listen_address;
  }
}
