#pragma once

#include <cstdint>
#include <pthread.h>
#include <queue>
#include <semaphore.h>
#include <string>

using namespace std;

constexpr int discovery_packet_max_size = 4096;

enum Discovery_Type { type_request, type_response };

struct Header {
  int type;
  int ttl;
  int last_id; // NOTE(felipe): used to not retransmit to the sender
};

struct Discovery_Request_Packet {
  Header header;
  string file;
  int request_id;
  string request_ip;
  uint16_t request_port;
};

struct Discovery_Response_Packet {
  Header header;
  int id;
  string ip;
  uint16_t port;
  int chunk;
  int transmission_rate;
};

struct Node {
  int id;
  string ip;
  uint16_t port;
};

struct File {
  string metadata_file_name;
  int chunk_count;
  int ttl;
};

struct Data {
  Node this_node;
  int transmission_rate;

  vector<Node> neighbors;

  File file;

  // TCP {
  uint16_t next_tcp_port;

  int bytes_to_send_in_timeframe;
  sem_t new_send_timeframe;
  pthread_mutex_t bytes_to_send_in_timeframe_lock;
  // } TCP

  bool receiving_chunks;
  pthread_mutex_t receiving_chunks_lock;
  vector<bool> chunks_being_received;

  pthread_t receive_udp_packets_thread;
  pthread_t retransmit_udp_packets_thread;
  pthread_t request_file_thread;
  pthread_t respond_discovery_thread;
  pthread_t manage_bytes_to_send_thread;
  vector<pthread_t> send_chunk_threads;

  // NOTE(felipe): packet, time of receive, last_id
  queue<tuple<Discovery_Request_Packet, timespec, int>> requests_to_retransmit;

  vector<Discovery_Response_Packet> received_responses;
  pthread_mutex_t received_responses_lock;

  pthread_mutex_t retransmit_queues_lock;

  Discovery_Request_Packet last_request;
  pthread_mutex_t last_request_lock;

  sem_t notify_request_arrived;
};
