#undef NDEBUG

#include <stdio.h>
#include <assert.h>
#include <stdlib.h>
#include <ctype.h>
#include <string.h>

#include <iostream>
#include <sstream>
#include <vector>
#include <list>
#include <fstream>
#include <string>
#include <tuple>
#include <queue>

#include <unistd.h>
#include <fcntl.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <pthread.h>
#include <semaphore.h>
#include <sys/types.h>
#include <dirent.h>

using namespace std;

using Thread_Function = void*(void*);

enum Discovery_Type {
    type_request,
    type_response
};

struct Header {
    int type;
    int ttl;
    int last_id;  // NOTE(felipe): used to not retransmit to the sender
};

struct Discovery_Request_Packet {
    Header header;
    string file;
};

constexpr int discovery_request_max_size = 4096;

struct Discovery_Response_Packet {
    Header header;
    string ip;
    uint16_t port;
    int chunk;
};

constexpr int discovery_response_packet_max_size = 4096;

void print(Header* header) {
    cout << "Type: " << header->type << endl;
    cout << "TTL: " << header->ttl << endl;
    cout << "Last id: " << header->last_id << endl;
}

void print(Discovery_Request_Packet* packet) {
    print(&packet->header);
    cout << "File: " << packet->file << endl;
    cout << endl;
}

void print(Discovery_Response_Packet* packet) {
    print(&packet->header);
    cout << "IP: " << packet->ip << endl;
    cout << "Port: " << packet->port << endl;
    cout << "Chunk: " << packet->chunk << endl;
    cout << endl;
}

struct Node {
    int id;
    string ip;
    uint16_t port;
};

struct Data {
    Node this_node;
    int transmission_rate;

    vector<Node> neighbors;

    string metadata_file_name;
    int chunk_count;
    int ttl;

    pthread_t receive_udp_packets_thread;
    pthread_t request_file_thread;
    pthread_t respond_discovery_thread;
    vector<pthread_t> send_chunk_threads;
    vector<pthread_t> receive_chunk_threads;

    Discovery_Request_Packet last_request;
    pthread_mutex_t last_request_lock;

    Discovery_Response_Packet last_response;
    pthread_mutex_t last_response_lock;

    bool request_file = false;
    pthread_mutex_t request_file_lock;

    sem_t notify_request_arrived;
    sem_t notify_response_arrived;
};

struct Chunk {
};

int perror_check(int return_value) {
    if (return_value == -1) {
        perror("ERROR");
        assert(false);
        exit(1);
    }
    return return_value;
}

vector<vector<int>> read_topology() {
    ifstream file("../topologia.txt");
    assert(file.is_open());

    vector<vector<int>> topology;
    while (true) {
        string s;
        std::getline(file, s);

        if (!file) {
            break;
        }

        stringstream ss(s);

        int from;
        ss >> from;

        char colon;
        ss >> colon;

        vector<int> nodes_to;

        bool first = true;
        while (true) {
            if (!first) {
                char comma;
                ss >> comma;
            }
            first = false;

            int to;
            ss >> to;

            if (!ss) {
                break;
            }

            nodes_to.push_back(to);
        }

        topology.push_back(nodes_to);
    }

    return topology;
}

struct Config {
    string ip;
    uint16_t port;
    int transmission_rate;
};

vector<Config> read_config() {
    ifstream file("../config.txt");
    assert(file.is_open());

    vector<Config> configs;
    while (true) {
        string s;
        std::getline(file, s);

        if (!file) {
            break;
        }

        stringstream ss(s);

        int from;
        ss >> from;

        char colon;
        ss >> colon;

        Config config;

        char comma;

        string ip;
        uint16_t port;
        int transmission_rate;
        {
            while (true) {
                char c;
                ss >> c;

                if (!ss) {
                    break;
                }

                if (c == ',') {
                    break;
                }

                ip.push_back(c);
            }

            ss >> port;
            ss >> comma;

            ss >> transmission_rate;
        }

        configs.push_back({ip, port, transmission_rate});
    }

    return configs;
}

void set_nodes(Data* data, int this_id) {
    vector<vector<int>> topology = read_topology();
    vector<Config> configs = read_config();

    vector<Node> neighbors;
    for (int neighbor_id : topology[this_id]) {
        Config* neighbor_config = &configs[neighbor_id];
        neighbors.push_back({neighbor_id, neighbor_config->ip, neighbor_config->port});
    }
    data->neighbors = neighbors;
    data->this_node.ip = configs[data->this_node.id].ip;
    data->this_node.port = configs[data->this_node.id].port;
    data->transmission_rate = configs[data->this_node.id].transmission_rate;
}

void read_and_set_metadata(Data* data) {
    ifstream file("../arquivo.p2p");
    assert(file.is_open());

    file >> data->metadata_file_name;
    file >> data->chunk_count;
    file >> data->ttl;
}

Discovery_Request_Packet unserialize_discovery_request_packet(string* serialized) {
    stringstream all_lines(*serialized);

    string file;
    {
        string s;
        getline(all_lines, s);
        stringstream line(s);

        line >> file;
    }

    return Discovery_Request_Packet{Header{}, file};
}

string serialize_discovery_request_packet(Discovery_Request_Packet* packet) {
    string s;

    s += to_string(packet->header.type);
    s += '\n';

    s += to_string(packet->header.ttl);
    s += '\n';

    s += to_string(packet->header.last_id);
    s += '\n';

    s += packet->file;
    s += '\n';

    return s;
}

Discovery_Response_Packet unserialize_discovery_response_packet(string* serialized) {
    stringstream all_lines(*serialized);

    string ip;
    uint16_t port;
    int chunk;
    {
        string s;
        getline(all_lines, s);
        stringstream line(s);

        line >> ip;
    }
    {
        string s;
        getline(all_lines, s);
        stringstream line(s);

        line >> port;
    }
    {
        string s;
        getline(all_lines, s);
        stringstream line(s);

        line >> chunk;
    }

    return Discovery_Response_Packet{Header{}, ip, port, chunk};
}

string serialize_discovery_response_packet(Discovery_Response_Packet* packet) {
    string s;

    s += to_string(packet->header.type);
    s += '\n';

    s += to_string(packet->header.ttl);
    s += '\n';

    s += to_string(packet->header.last_id);
    s += '\n';

    s += packet->ip;
    s += '\n';

    s += to_string(packet->port);
    s += '\n';

    s += to_string(packet->chunk);
    s += '\n';

    return s;
}

int tcp_create_socket() {
    return perror_check(
        socket(AF_INET, SOCK_STREAM, 0));
}

int udp_create_socket() {
    return perror_check(
        socket(AF_INET, SOCK_DGRAM, 0));
}

void tcp_bind(int this_socket, int address, uint16_t port) {
    sockaddr_in s_addr = {};
    s_addr.sin_family = AF_INET;
    s_addr.sin_addr.s_addr = address;
    s_addr.sin_port = htons(port);
    perror_check(
        bind(this_socket, (const struct sockaddr*)&s_addr, sizeof(s_addr)));

    constexpr int max_connections = 100;
    perror_check(
        listen(this_socket, max_connections));
}

void udp_bind(int this_socket, int address, uint16_t port) {
    sockaddr_in s_addr = {};
    s_addr.sin_family = AF_INET;
    s_addr.sin_addr.s_addr = address;
    s_addr.sin_port = htons(port);
    perror_check(
        bind(this_socket, (const struct sockaddr*)&s_addr, sizeof(s_addr)));
}

void tcp_connect(int this_socket, int address, uint16_t port) {
    sockaddr_in s_addr = {};
    s_addr.sin_family = AF_INET;
    s_addr.sin_addr.s_addr = address;
    s_addr.sin_port = htons(port);
    perror_check(
        connect(this_socket, (sockaddr*)&s_addr, sizeof(s_addr)));
}

string general_receive(int this_socket, int max_bytes, in_addr_t* ip = 0, in_port_t* port = 0) {
    string s;
    s.resize(max_bytes);

    sockaddr_in receiving_address = {};
    socklen_t receiving_size = sizeof(receiving_address);

    int n = recvfrom(
        this_socket, s.data(), s.size(),
        MSG_WAITALL, (struct sockaddr*)&receiving_address, &receiving_size
    );

    perror_check(n);

    if (ip) {
        *ip = receiving_address.sin_addr.s_addr;
    }
    if (port) {
        *port = ntohs(receiving_address.sin_port);
    }

    s.resize(n);

    return s;
}

string tcp_receive(int this_socket, int max_bytes, in_addr_t* ip = 0, in_port_t* port = 0) {
    return general_receive(this_socket, max_bytes, ip, port);
}

string udp_receive(int this_socket, int max_bytes, in_addr_t* ip = 0, in_port_t* port = 0) {
    return general_receive(this_socket, max_bytes, ip, port);
}

void general_send(int this_socket, in_addr_t ip, uint16_t port, string* message) {
    sockaddr_in s_addr = {};
    s_addr.sin_family = AF_INET;
    s_addr.sin_addr.s_addr = ip;
    s_addr.sin_port = htons(port);

    perror_check(
        sendto(
            this_socket, message->c_str(), message->size(),
            MSG_CONFIRM, (sockaddr*)&s_addr, sizeof(sockaddr_in)
        )
    );
}

void tcp_send(int this_socket, in_addr_t ip, uint16_t port, string* message) {
    general_send(this_socket, ip, port, message);
}

void udp_send(int this_socket, in_addr_t ip, uint16_t port, string* message) {
    general_send(this_socket, ip, port, message);
}


// TODO(felipe): retransmit UDP discovery transmission (timeout?)
// TODO(felipe): consider that a chunk may not exist in all nodes

void* send_chunk(void*) {
    //{
    //    int server_sockfd = socket(AF_INET, SOCK_STREAM, 0);

    //    sockaddr_in server_address = {};
    //    server_address.sin_family = AF_INET;
    //    server_address.sin_addr.s_addr = INADDR_ANY;
    //    server_address.sin_port = htons(this_config->port);

    //    constexpr int max_clients = 100;

    //    perror_check(
    //        bind(server_sockfd, (struct sockaddr *)&server_address, sizeof(server_address)));
    //    perror_check(
    //        listen(server_sockfd, max_clients));

    //    int flags = fcntl(server_sockfd, F_GETFL, 0);
    //    fcntl(server_sockfd, F_SETFL, O_NONBLOCK|flags);

    //    vector<int> clients;
    //    while (true) {
    //        struct sockaddr_in client_address = {};
    //        socklen_t client_len = sizeof(client_address);

    //        int client_sockfd = accept(server_sockfd,(struct sockaddr *)&client_address, &client_len);

    //        bool new_client = true;
    //        if (client_sockfd < 0) {
    //            if (errno != EAGAIN && errno != EWOULDBLOCK) {
    //                perror_check(client_sockfd);
    //            }

    //            new_client = false;
    //        }

    //        if (new_client) {
    //            clients.push_back(client_sockfd);
    //        }

    //        //send_chunk(data, &clients);
    //    }
    //}

    return 0;
}

void* receive_chunk(tuple<Discovery_Response_Packet, Chunk>* tuple_arg) {
    Discovery_Response_Packet* packet = &get<0>(*tuple_arg);
    Chunk* chunk = &get<1>(*tuple_arg);

    int this_socket = tcp_create_socket();

    int server_ip = inet_addr(packet->ip.c_str());
    uint16_t server_port = packet->port;
    tcp_connect(this_socket, server_ip, server_port);

    close(this_socket);

    return 0;
}

void* receive_udp_packets(Data* data) {
    int this_socket = udp_create_socket();

    int ip = INADDR_ANY;
    uint16_t port = data->this_node.port;
    udp_bind(this_socket, ip, port);

    while (true) {
        string s = udp_receive(this_socket, discovery_request_max_size);

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
            if (!ss) break;
            s_rest += c;
        }

        Header new_header = {
            header.type,
            header.ttl - 1,
            data->this_node.id
        };

        if (header.type == type_request) {
            Discovery_Request_Packet request = unserialize_discovery_request_packet(&s_rest);
            request.header = header;

            cout << "UDP packets from " << header.last_id << " " << endl;
            print(&request);

            pthread_mutex_lock(&data->last_request_lock);
            data->last_request = request;
            pthread_mutex_unlock(&data->last_request_lock);

            sem_post(&data->notify_request_arrived);

            retransmit_request_packet: {
                Discovery_Request_Packet to_send_packet = request;
                to_send_packet.header = new_header;

                if (to_send_packet.header.ttl >= 0) {
                    string serialized_packet = serialize_discovery_request_packet(&to_send_packet);

                    for (Node& neighbor : data->neighbors) {
                        if (neighbor.id == header.last_id) {
                            continue;
                        }

                        int neighbor_ip = inet_addr(neighbor.ip.c_str());
                        uint16_t neighbor_port = neighbor.port;
                        udp_send(this_socket, neighbor_ip, neighbor_port, &serialized_packet);

                        cout << "Retransmiting to " << neighbor.id << " " << endl;
                        print(&to_send_packet);
                    }
                }
            }
        } else if (header.type == type_response) {
            Discovery_Response_Packet response = unserialize_discovery_response_packet(&s_rest);
            response.header = header;

            cout << "UDP packets from " << header.last_id << " " << endl;
            print(&response);

            pthread_mutex_lock(&data->last_response_lock);
            data->last_response = response;
            pthread_mutex_unlock(&data->last_response_lock);

            pthread_mutex_lock(&data->request_file_lock);
            if (data->request_file) {
                sem_post(&data->notify_response_arrived);
            }
            pthread_mutex_unlock(&data->request_file_lock);

            retransmit_response_packet: {
                Discovery_Response_Packet to_send_packet = response;
                to_send_packet.header = new_header;

                if (to_send_packet.header.ttl >= 0) {
                    string serialized_packet = serialize_discovery_response_packet(&to_send_packet);

                    for (Node& neighbor : data->neighbors) {
                        if (neighbor.id == header.last_id) {
                            continue;
                        }

                        int neighbor_ip = inet_addr(neighbor.ip.c_str());
                        uint16_t neighbor_port = neighbor.port;
                        udp_send(this_socket, neighbor_ip, neighbor_port, &serialized_packet);

                        cout << "Retransmiting to " << neighbor.id << " " << endl;
                        print(&to_send_packet);
                    }
                }

            }
        }
    }

    close(this_socket); 

    return 0;
}

void* respond_discovery(Data* data) {
    while (true) {
        Discovery_Request_Packet received_packet;
        receive_packet: {
            sem_wait(&data->notify_request_arrived);

            pthread_mutex_lock(&data->last_request_lock);
            received_packet = data->last_request;
            pthread_mutex_unlock(&data->last_request_lock);
        }

        sleep(1);

        respond_packet: {
            int this_socket = udp_create_socket();

            {
                char const* file_name = received_packet.file.c_str();
                int file_name_size = received_packet.file.size();

                DIR* directory = opendir(".");
                assert(directory);

                dirent* entry;
                while (true) {
                    entry = readdir(directory);
                    if (!entry) {
                        break;
                    }

                    char const* curr_file = entry->d_name;

                    char const* chunk_suffix = ".ch";
                    int chunk_suffix_size = strlen(chunk_suffix);

                    bool starts_with_file_name = strncmp(curr_file, file_name, file_name_size) == 0;
                    bool ends_with_chunk_suffix = strncmp(curr_file + file_name_size, chunk_suffix, chunk_suffix_size) == 0;

                    if (starts_with_file_name && ends_with_chunk_suffix) {
                        int chunk = atoi(curr_file + file_name_size + chunk_suffix_size);
                        Discovery_Response_Packet response = {
                            Header{
                                type_response,
                                data->ttl - 1,
                                data->this_node.id
                            },
                            data->this_node.ip, data->this_node.port, chunk
                        };

                        string serialized_response = serialize_discovery_response_packet(&response);

                        for (Node& neighbor : data->neighbors) {
                            int neighbor_ip = inet_addr(neighbor.ip.c_str());
                            uint16_t neighbor_port = neighbor.port;
                            udp_send(this_socket, neighbor_ip, neighbor_port, &serialized_response);

                            cout << "Responding to " << neighbor.id << endl;
                            print(&response);
                        }
                    }
                }
                closedir(directory);
            }

            close(this_socket);
        }
    }

    return 0;
}

void* request_file(Data* data) {
    while (true) {
        ask: while (true) {
            cout << "Voce quer requisitar arquivo? [s/n]: ";

            char answer = 0;
            cin >> answer;

            answer = tolower(answer);

            if (answer != 's' && answer != 'n') {
                cout << "Entrada invalida\n";
            }

            if (answer == 's') {
                break;
            }
        }

        request_chunks: {
            Discovery_Request_Packet packet = {
                Header {
                    type_request,
                    data->ttl - 1,
                    data->this_node.id
                },
                data->metadata_file_name,
            };

            string serialized_packet = serialize_discovery_request_packet(&packet);

            int this_socket = udp_create_socket();

            for (Node& neighbor : data->neighbors) {
                int ip = inet_addr(neighbor.ip.c_str());
                uint16_t port = neighbor.port;
                udp_send(this_socket, ip, port, &serialized_packet);

                cout << "Requesting to " << neighbor.id << endl;
                print(&packet);
            }

            close(this_socket);
        }

        pthread_mutex_lock(&data->request_file_lock);
        data->request_file = true;
        pthread_mutex_unlock(&data->request_file_lock);

        usleep(1'000);

        //create_threads_to_receive_chunks: {
        //    list<tuple<Discovery_Response_Packet, Chunk>> threads_storage;
        //        
        //    int chunk_count = 0;
        //    vector<bool> seen_chunks(data->chunk_count, false);
        //    while (chunk_count < data->chunk_count) {
        //        sem_wait(&data->notify_response_arrived);

        //        pthread_mutex_lock(&data->last_response_lock);
        //        Discovery_Response_Packet temp_packet = data->last_response;
        //        pthread_mutex_unlock(&data->last_response_lock);

        //        if (!seen_chunks[temp_packet.chunk]) {
        //            seen_chunks[temp_packet.chunk] = true;

        //            threads_storage.push_back({std::move(temp_packet), Chunk{}});

        //            pthread_t new_thread = 0;
        //            pthread_create(&new_thread, 0, (Thread_Function*)receive_chunk, &threads_storage.back());
        //        }
        //    }
        //}

        pthread_mutex_lock(&data->request_file_lock);
        data->request_file = false;
        pthread_mutex_unlock(&data->request_file_lock);
    }

    return 0;
}

int main(int argc, char** argv) {
    assert(argc >= 2);

    Data data = {};
    data.this_node.id = atoi(argv[1]);

    set_nodes(&data, data.this_node.id);
    read_and_set_metadata(&data);

    init: {
        pthread_create(&data.receive_udp_packets_thread, 0, (Thread_Function*)receive_udp_packets, &data);
        pthread_create(&data.request_file_thread, 0, (Thread_Function*)request_file, &data);
        pthread_create(&data.respond_discovery_thread, 0, (Thread_Function*)respond_discovery, &data);

        pthread_mutex_init(&data.last_request_lock, 0);
        pthread_mutex_init(&data.last_response_lock, 0);

        pthread_mutex_init(&data.request_file_lock, 0);

        sem_init(&data.notify_request_arrived, 0, 0);
        sem_init(&data.notify_response_arrived, 0, 0);
    }

    pthread_join(data.receive_udp_packets_thread, 0);
    pthread_join(data.request_file_thread, 0);
    pthread_join(data.respond_discovery_thread, 0);

    destroy: {
        pthread_mutex_destroy(&data.last_request_lock);
        pthread_mutex_destroy(&data.last_response_lock);

        pthread_mutex_destroy(&data.request_file_lock);

        sem_destroy(&data.notify_request_arrived);
        sem_destroy(&data.notify_response_arrived);
    }

    return 0;
}

