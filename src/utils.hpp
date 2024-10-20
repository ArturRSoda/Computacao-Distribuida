#pragma once

#include <time.h>
#include <iostream>
#include <iomanip>
#include <assert.h>

#include <arpa/inet.h>

#include "structs.hpp"

using namespace std;

timespec my_get_time();
ostream& operator<<(ostream& os, timespec ts);
void print(Header* header);
void print(Discovery_Request_Packet* packet);
void print(Discovery_Response_Packet* packet);
int perror_check(int return_value);
Discovery_Request_Packet unserialize_discovery_request_packet(string* serialized);
string serialize_discovery_request_packet(Discovery_Request_Packet* packet);
Discovery_Response_Packet unserialize_discovery_response_packet(string* serialized);
string serialize_discovery_response_packet(Discovery_Response_Packet* packet);
int tcp_create_socket();
int udp_create_socket();
void tcp_bind(int this_socket, int address, uint16_t port);
void tcp_listen(int this_socket);
int tcp_accept(int this_socket, in_addr_t* ip = 0);
void udp_bind(int this_socket, int address, uint16_t port);
bool tcp_connect(int this_socket, int address, uint16_t port);
string general_receive(int this_socket, int max_bytes, in_addr_t* ip = 0, in_port_t* port = 0);
string tcp_receive(int this_socket, int max_bytes, in_addr_t* ip = 0, in_port_t* port = 0);
string udp_receive(int this_socket, int max_bytes, in_addr_t* ip = 0, in_port_t* port = 0);
void general_send(int this_socket, in_addr_t ip, uint16_t port, string* message);
void tcp_send(int this_socket, in_addr_t ip, uint16_t port, string* message);
void udp_send(int this_socket, in_addr_t ip, uint16_t port, string* message);
