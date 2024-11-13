#undef NDEBUG

#include <algorithm>
#include <assert.h>
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include <fstream>
#include <iomanip>
#include <iostream>
#include <list>
#include <queue>
#include <sstream>
#include <string>
#include <tuple>
#include <vector>

#include <arpa/inet.h>
#include <dirent.h>
#include <fcntl.h>
#include <poll.h>
#include <pthread.h>
#include <semaphore.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>

using namespace std;

#include "parse_files.hpp"
#include "structs.hpp"
#include "utils.hpp"

struct Send_Chunk_Args {
  Data *data;
  string file_to_send;
  int server_socket;
  int timeout_milli;
  int chunk;
};

void *send_chunk(void *void_arg) {
  Send_Chunk_Args *p_arg = (Send_Chunk_Args *)void_arg;

  Send_Chunk_Args arg = *p_arg;
  delete p_arg;

  int client_socket = -1;
connect_to_client: {
  in_addr_t client_ip;

  pollfd poll_fd = {};
  poll_fd.fd = arg.server_socket;
  poll_fd.events = POLLIN;

  int ret = perror_check(poll(&poll_fd, 1, arg.timeout_milli));

  if (ret != 0) {
    client_socket = tcp_accept(arg.server_socket, &client_ip);
  }

  close(arg.server_socket);

  if (client_socket < 0) {
    cout << my_get_time() << "Timeout" << endl;
    return 0;
  } else {
    in_addr client_address = {client_ip};
    string client_ip_string = string(inet_ntoa(client_address));
    cout << my_get_time() << "Client " << client_ip_string << " connected"
         << endl;
  }
}

  int file_size = 0;
compute_file_size: {
  ifstream file(arg.file_to_send, ios::ate | ios::binary);
  file_size = file.tellg();
}

  int total_bytes_sent = 0;

  Data *data = arg.data;
  while (true) {
    if (total_bytes_sent >= file_size) {
      break;
    }

    sem_wait(&data->new_send_timeframe);

    int remaining_file_size = file_size - total_bytes_sent;

    int count_of_bytes_to_send = 0;
    pthread_mutex_lock(&data->bytes_to_send_in_timeframe_lock);
    count_of_bytes_to_send =
        std::min(data->bytes_to_send_in_timeframe, remaining_file_size);
    data->bytes_to_send_in_timeframe -= count_of_bytes_to_send;
    pthread_mutex_unlock(&data->bytes_to_send_in_timeframe_lock);

    ifstream file(arg.file_to_send, ios::binary);
    file.seekg(total_bytes_sent);

    for (int i = 0; i < count_of_bytes_to_send; i++) {
      char ch;
      file.get(ch);
      assert(file.good());

      perror_check(write(client_socket, &ch, 1));
    }

    total_bytes_sent += count_of_bytes_to_send;
  }

  close(client_socket);

  cout << my_get_time() << "Done sending chunk " << arg.chunk << endl;

  return 0;
}

struct Receive_Chunk_Args {
  in_addr_t server_ip;
  uint16_t server_port;
  string chunk_file_name;
};

void *receive_chunk(void *void_arg) {
  Receive_Chunk_Args *p_args = (Receive_Chunk_Args *)void_arg;
  Receive_Chunk_Args args = *p_args;
  delete p_args;

  int this_socket = tcp_create_socket();

  int server_ip = args.server_ip;
  uint16_t server_port = args.server_port;
  bool connected = tcp_connect(this_socket, server_ip, server_port);
  assert(connected);

  while (true) {
    char byte;
    int n = perror_check(read(this_socket, &byte, 1));

    if (n == 0)
      break;

    ofstream file(args.chunk_file_name, ios::app | ios::binary);
    assert(file.is_open());

    file.put(byte);
  }

  close(this_socket);

  cout << my_get_time() << "Done receiving " << args.chunk_file_name << endl;

  return 0;
}

void *receive_udp_packets(void *void_arg) {
  Data *data = (Data *)void_arg;

  int this_socket = udp_create_socket();

  int ip = INADDR_ANY;
  uint16_t port = data->this_node.port;
  udp_bind(this_socket, ip, port);

  while (true) {
    string s = udp_receive(this_socket, discovery_packet_max_size);

    stringstream ss(s);

    Header header = {};
    {
      {
        string s;
        getline(ss, s);
        stringstream line(s);

        line >> header.type;
      }
      {
        string s;
        getline(ss, s);
        stringstream line(s);

        line >> header.ttl;
      }
      {
        string s;
        getline(ss, s);
        stringstream line(s);

        line >> header.last_id;
      }
    }

    string s_rest;
    while (true) {
      char c = ss.get();
      if (!ss)
        break;
      s_rest += c;
    }

    Header new_header = {header.type, header.ttl - 1, data->this_node.id};

    if (header.type == type_request) {
      Discovery_Request_Packet request =
          unserialize_discovery_request_packet(&s_rest);
      request.header = header;

      if (request.request_id != data->this_node.id) {
        cout << my_get_time() << "UDP packets from " << header.last_id << " "
             << endl;
        print(&request);

        pthread_mutex_lock(&data->last_request_lock);
        data->last_request = request;
        pthread_mutex_unlock(&data->last_request_lock);

        sem_post(&data->notify_request_arrived);

        pthread_mutex_lock(&data->retransmit_queues_lock);
        Discovery_Request_Packet to_send_packet = request;
        to_send_packet.header = new_header;
        timespec curr_time = my_get_time();
        data->requests_to_retransmit.push(
            {to_send_packet, curr_time, header.last_id});
        pthread_mutex_unlock(&data->retransmit_queues_lock);
      }
    } else if (header.type == type_response) {
      Discovery_Response_Packet response =
          unserialize_discovery_response_packet(&s_rest);
      response.header = header;

      pthread_mutex_lock(&data->received_responses_lock);
      data->received_responses.push_back(response);
      pthread_mutex_unlock(&data->received_responses_lock);

      cout << my_get_time() << "UDP packets from " << header.last_id << " "
           << endl;
      print(&response);
    }
  }

  close(this_socket);

  return 0;
}

void *retransmit_udp_packets(void *void_arg) {
  Data *data = (Data *)void_arg;

  while (true) {
    pthread_mutex_lock(&data->retransmit_queues_lock);

    int this_socket = udp_create_socket();

    timespec curr_time = my_get_time();

    double sec_to_milli = 1000.0;
    double nsec_to_milli = 1.0 / 1000000.0;

    if (data->requests_to_retransmit.size()) {
      auto request_tup = data->requests_to_retransmit.front();

      timespec last_time = get<1>(request_tup);
      double diff_milli =
          sec_to_milli * (curr_time.tv_sec - last_time.tv_sec) +
          nsec_to_milli * (curr_time.tv_nsec - last_time.tv_nsec);

      if (diff_milli > 1000) {
        data->requests_to_retransmit.pop();

        Discovery_Request_Packet to_send_packet = get<0>(request_tup);
        if (to_send_packet.header.ttl >= 0) {
          string serialized_packet =
              serialize_discovery_request_packet(&to_send_packet);

          for (Node &neighbor : data->neighbors) {
            if (neighbor.id == get<2>(request_tup)) {
              continue;
            }

            int neighbor_ip = inet_addr(neighbor.ip.c_str());
            uint16_t neighbor_port = neighbor.port;
            udp_send(this_socket, neighbor_ip, neighbor_port,
                     &serialized_packet);

            cout << curr_time << "Retransmiting to " << neighbor.id << endl;
            print(&to_send_packet);
          }
        }
      }
    }
    close(this_socket);

    pthread_mutex_unlock(&data->retransmit_queues_lock);

    usleep(10000);
  }
}

vector<int> get_chunks(Data *data, char const *file_name, int file_name_size) {
  vector<int> chunks;

  DIR *directory = opendir(".");
  assert(directory);

  dirent *entry = 0;
  while (true) {
    entry = readdir(directory);
    if (!entry) {
      break;
    }

    char const *curr_file = entry->d_name;

    char const *chunk_suffix = ".ch";
    int chunk_suffix_size = strlen(chunk_suffix);

    bool starts_with_file_name =
        strncmp(curr_file, file_name, file_name_size) == 0;
    bool ends_with_chunk_suffix = strncmp(curr_file + file_name_size,
                                          chunk_suffix, chunk_suffix_size) == 0;

    if (starts_with_file_name && ends_with_chunk_suffix) {
      int chunk = atoi(curr_file + file_name_size + chunk_suffix_size);

      bool current_chunk_is_being_received = false;
      pthread_mutex_lock(&data->receiving_chunks_lock);
      if (data->receiving_chunks) {
        current_chunk_is_being_received = data->chunks_being_received[chunk];
      }
      pthread_mutex_unlock(&data->receiving_chunks_lock);

      if (!current_chunk_is_being_received) {
        chunks.push_back(chunk);
      }
    }
  }
  closedir(directory);
  return chunks;
}

void *respond_discovery(void *arg) {
  Data *data = (Data *)arg;
  while (true) {
    Discovery_Request_Packet received_packet;
  receive_packet: {
    sem_wait(&data->notify_request_arrived);

    pthread_mutex_lock(&data->last_request_lock);
    received_packet = data->last_request;
    pthread_mutex_unlock(&data->last_request_lock);
  }

    char const *file_name = received_packet.file.c_str();
    int file_name_size = received_packet.file.size();

    vector<int> chunks = get_chunks(data, file_name, file_name_size);

    uint16_t start_port = data->next_tcp_port;

  create_send_chunk_threads_and_respond: {
    for (int i = 0; i < chunks.size(); i++) {
      uint16_t tcp_port = start_port + i;
      int chunk = chunks[i];

      {
        int server_socket = tcp_create_socket();
        tcp_bind(server_socket, INADDR_ANY, tcp_port);
        tcp_listen(server_socket);

        int timeout_milli = (received_packet.header.ttl + 10) * 1000;

        cout << my_get_time() << "Waiting in port " << tcp_port << endl;

        Send_Chunk_Args *args = new Send_Chunk_Args(
            {data, received_packet.file + ".ch" + to_string(chunk),
             server_socket, timeout_milli, chunk});

        pthread_t new_send_chunk_thread;
        pthread_create(&new_send_chunk_thread, 0, send_chunk, args);

        data->send_chunk_threads.push_back(new_send_chunk_thread);
      }
      {
        int this_socket = udp_create_socket();

        Discovery_Response_Packet response = {
            Header{type_response, received_packet.header.ttl - 1,
                   data->this_node.id},
            data->this_node.id,
            data->this_node.ip,
            tcp_port,
            chunk,
            data->transmission_rate};

        if (response.header.ttl >= 0) {
          string serialized_response =
              serialize_discovery_response_packet(&response);

          int request_ip = inet_addr(received_packet.request_ip.c_str());
          uint16_t request_port = received_packet.request_port;
          udp_send(this_socket, request_ip, request_port, &serialized_response);
          cout << my_get_time() << "Responding to "
               << received_packet.request_id << endl;
          print(&response);

          close(this_socket);
        }
      }
    }
  }

    data->next_tcp_port += chunks.size();
  }

  return 0;
}

void *request_file(void *arg) {
  Data *data = (Data *)arg;
  while (true) {
  ask:
    while (true) {
      cout << "Digite o arquivo a ser requisitado: ";

      string file_name;
      cin >> file_name;

      ifstream file_info_stream(file_name);

      if (!file_info_stream.is_open()) {
        cout << "Nao foi possivel abrir o arquivo" << endl;
        continue;
      }

      File file_info;
      file_info_stream >> file_info.metadata_file_name;
      file_info_stream >> file_info.chunk_count;
      file_info_stream >> file_info.ttl;

      data->file = file_info;

      break;
    }

  request_chunks: {
    Discovery_Request_Packet packet = {
        Header{type_request, data->file.ttl - 1, data->this_node.id},
        data->file.metadata_file_name, data->this_node.id, data->this_node.ip,
        data->this_node.port};

    string serialized_packet = serialize_discovery_request_packet(&packet);

    int this_socket = udp_create_socket();

    for (Node &neighbor : data->neighbors) {
      int ip = inet_addr(neighbor.ip.c_str());
      uint16_t port = neighbor.port;
      udp_send(this_socket, ip, port, &serialized_packet);

      cout << my_get_time() << "Requesting to " << neighbor.id << endl;
      print(&packet);
    }

    close(this_socket);
  }

    pthread_mutex_lock(&data->receiving_chunks_lock);
    data->receiving_chunks = true;
    data->chunks_being_received.assign(data->file.chunk_count, false);
    pthread_mutex_unlock(&data->receiving_chunks_lock);

    vector<pthread_t> receive_chunk_threads;
    int chunk_count = 0;
    vector<vector<Discovery_Response_Packet>> received_responses;
    for (int i = 0; i < data->file.chunk_count; i++)
      received_responses.push_back({});

    vector<bool> seen_chunks(data->file.chunk_count, false);

    char const *file_name = data->file.metadata_file_name.c_str();
    int file_name_size = data->file.metadata_file_name.size();

    vector<int> chunks_already_in_folder =
        get_chunks(data, file_name, file_name_size);
    for (int chunk : chunks_already_in_folder) {
      if (!seen_chunks[chunk]) {
        chunk_count++;
      }
      seen_chunks[chunk] = true;
    }

    int request_timeout = data->file.ttl + 5;
    sleep(request_timeout);

    pthread_mutex_lock(&data->received_responses_lock);
    for (auto response_packet : data->received_responses) {
      if (!seen_chunks[response_packet.chunk]) {
        seen_chunks[response_packet.chunk] = true;
        received_responses[response_packet.chunk].push_back(response_packet);
        chunk_count++;

        pthread_mutex_lock(&data->receiving_chunks_lock);
        data->chunks_being_received[response_packet.chunk] = true;
        pthread_mutex_unlock(&data->receiving_chunks_lock);
      }
    }
    pthread_mutex_unlock(&data->received_responses_lock);

    for (auto v : received_responses) {
      if (not v.empty()) {
        sort(v.begin(), v.end(), compareByTransmissionRate);
        Discovery_Response_Packet chosen_packet = v.back();

        Receive_Chunk_Args *args = new Receive_Chunk_Args{
            inet_addr(chosen_packet.ip.c_str()), chosen_packet.port,
            data->file.metadata_file_name + ".ch" +
                to_string(chosen_packet.chunk)};

        pthread_t new_thread = 0;
        pthread_create(&new_thread, 0, receive_chunk, args);

        receive_chunk_threads.push_back(new_thread);

        cout << my_get_time() << "Receiving chunk " << chosen_packet.chunk
             << " from " << chosen_packet.id << endl;
      }
    }

    for (pthread_t thread : receive_chunk_threads) {
      pthread_join(thread, 0);
    }

    pthread_mutex_lock(&data->receiving_chunks_lock);
    data->receiving_chunks = false;
    pthread_mutex_unlock(&data->receiving_chunks_lock);

    if (chunk_count < data->file.chunk_count) {
      cout << my_get_time() << "Not all chunks received" << endl;
    } else {
    create_original_file: {
      ofstream original_file(data->file.metadata_file_name,
                             ios::binary | ios::trunc);

      for (int i = 0; i < data->file.chunk_count; i++) {
        ifstream chunk_file(
            data->file.metadata_file_name + ".ch" + to_string(i), ios::binary);
        while (true) {
          char ch = 0;
          chunk_file.get(ch);

          if (!chunk_file) {
            break;
          }

          original_file.put(ch);
        }
      }
    }

      cout << my_get_time() << "Received all chunks" << endl;
    }

    pthread_mutex_lock(&data->received_responses_lock);
    data->received_responses.clear();
    pthread_mutex_unlock(&data->received_responses_lock);
  }

  return 0;
}

void *manage_bytes_to_send(void *arg) {
  Data *data = (Data *)arg;
  while (true) {
    sleep(1);

    pthread_mutex_lock(&data->bytes_to_send_in_timeframe_lock);
    data->bytes_to_send_in_timeframe = data->transmission_rate;
    pthread_mutex_unlock(&data->bytes_to_send_in_timeframe_lock);

    sem_post(&data->new_send_timeframe);
  }
}

int main(int argc, char **argv) {
  assert(argc >= 2);

  Data data = {};
  data.this_node.id = atoi(argv[1]);
  data.next_tcp_port = 10000 + (1000 * data.this_node.id);
  data.received_responses = {};

  set_nodes(&data, data.this_node.id);

init: {
  pthread_mutex_init(&data.received_responses_lock, 0);
  sem_init(&data.new_send_timeframe, 0, 0);
  pthread_mutex_init(&data.bytes_to_send_in_timeframe_lock, 0);
  pthread_mutex_init(&data.receiving_chunks_lock, 0);
  pthread_mutex_init(&data.retransmit_queues_lock, 0);
  pthread_mutex_init(&data.last_request_lock, 0);
  sem_init(&data.notify_request_arrived, 0, 0);

  pthread_create(&data.receive_udp_packets_thread, 0, receive_udp_packets,
                 &data);
  pthread_create(&data.retransmit_udp_packets_thread, 0, retransmit_udp_packets,
                 &data);
  pthread_create(&data.request_file_thread, 0, request_file, &data);
  pthread_create(&data.respond_discovery_thread, 0, respond_discovery, &data);
  pthread_create(&data.manage_bytes_to_send_thread, 0, manage_bytes_to_send,
                 &data);
}

  pthread_join(data.receive_udp_packets_thread, 0);
  pthread_join(data.retransmit_udp_packets_thread, 0);
  pthread_join(data.request_file_thread, 0);
  pthread_join(data.respond_discovery_thread, 0);
  pthread_join(data.manage_bytes_to_send_thread, 0);

  for (pthread_t thread : data.send_chunk_threads) {
    pthread_join(thread, 0);
  }

destroy: {
  pthread_mutex_destroy(&data.received_responses_lock);
  sem_destroy(&data.new_send_timeframe);
  pthread_mutex_destroy(&data.bytes_to_send_in_timeframe_lock);
  pthread_mutex_destroy(&data.receiving_chunks_lock);
  pthread_mutex_destroy(&data.retransmit_queues_lock);
  pthread_mutex_destroy(&data.last_request_lock);
  sem_destroy(&data.notify_request_arrived);
}

  return 0;
}
