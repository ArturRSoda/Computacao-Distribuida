#pragma once

#include "structs.hpp"
#include <netinet/in.h>

using namespace std;

DatabaseData get_db_var(string var_name, vector<DatabaseData>* db);
void set_db_value(string var_name, float value, vector<DatabaseData>* db);
void add_db_version(string var_name, vector<DatabaseData>* db);
void print(Header* header);
void print(MessageRequestRead* rr);
void print(MessageResponseRead* rr);
void print(MessageRequestCommit* rc);
void print(MessageResponseCommit* rc);
string serialize_MessageRequestRead(MessageRequestRead* request);
string serialize_MessageResponseRead(MessageResponseRead* response);
string serialize_MessageRequestCommit(MessageRequestCommit* request);
string serialize_MessageResponseCommit(MessageResponseCommit* response);
MessageRequestRead unserialize_MessageRequestRead(string* serialized);
MessageResponseRead unserialize_MessageResponseRead(string* serialized);
MessageRequestCommit unserialize_MessageRequestCommit(string* serialized);
MessageResponseCommit unserialize_MessageResponseCommit(string* serialized);
int perror_check(int return_value);
int tcp_create_socket();
int udp_creat_socket();
void tcp_bind(int this_socket, int address, uint16_t port);
void tcp_listen(int this_socket);
int tcp_accept(int this_socket, in_addr_t* ip);
void udp_bind(int this_socket, int address, uint16_t port);
bool tcp_connect(int this_socket, int address, uint16_t port);
string general_receive(int this_socket, int max_bytes, in_addr_t* ip, in_port_t* port);
string tcp_receive(int this_socket, int max_bytes, in_addr_t* ip, in_port_t* port);
string udp_receive(int this_socket, int max_bytes, in_addr_t* ip, in_port_t* port);
void general_send(int this_socket, in_addr_t ip, uint16_t port, string* message);
void tcp_send(int this_socket, in_addr_t ip, uint16_t port, string* message);
void udp_send(int this_socket, in_addr_t ip, uint16_t port, string* message);
void read_config(Config* config);
timespec my_get_time();
int64_t get_elapsed_time_ms(timespec* initial_time);
void print_time_sec(timespec* initial_time);

