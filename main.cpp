#undef NDEBUG

#include <stdio.h>
#include <assert.h>
#include <stdlib.h>
#include <ctype.h>

#include <iostream>
#include <sstream>
#include <vector>
#include <list>
#include <fstream>
#include <string>
#include <tuple>

#include <unistd.h>
#include <fcntl.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <pthread.h>

using std::vector;
using std::stringstream;
using std::ifstream;
using std::string;
using std::to_string;
using std::cout;
using std::cin;
using std::list;
using std::move;
using std::tuple;
using std::get;

using Thread_Function = void*(void*);

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

    pthread_t request_file_thread;
    pthread_t retransmit_request_thread;
    vector<pthread_t> send_chunk_threads;
    vector<pthread_t> receive_chunk_threads;
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

struct Discovery_Request_Package {
    string file;
    int ttl;
    int last_id;  // NOTE(felipe): used to not retransmit to the receiver
};

constexpr int discovery_request_max_size = 4096;

Discovery_Request_Package unserialize_discovery_request_package(string* serialized) {
    stringstream all_lines(*serialized);

    string file;
    int ttl;
    int id;
    {
        string s;
        getline(all_lines, s);
        stringstream line(s);

        line >> file;
    }
    {
        string s;
        getline(all_lines, s);
        stringstream line(s);
        
        line >> ttl;
    }
    {
        string s;
        getline(all_lines, s);
        stringstream line(s);
        
        line >> id;
    }

    return Discovery_Request_Package{file, ttl, id};
}

string serialize_discovery_request_package(Discovery_Request_Package* package) {
    string s;

    s += package->file;
    s += '\n';

    s += to_string(package->ttl);
    s += '\n';

    s += to_string(package->last_id);
    s += '\n';

    return s;
}

struct Discovery_Response_Package {
    string ip;
    uint16_t port;
    int chunk;
};

constexpr int discovery_response_package_max_size = 4096;

Discovery_Response_Package unserialize_discovery_response_package(string* serialized) {
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

    return Discovery_Response_Package{ip, port, chunk};
}

string serialize_discovery_response_package(Discovery_Response_Package* package) {
    string s;

    s += package->ip;
    s += '\n';

    s += package->port;
    s += '\n';

    s += package->chunk;
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

void* receive_chunk(tuple<Discovery_Response_Package, Chunk>* tuple_arg) {
    Discovery_Response_Package* package = &get<0>(*tuple_arg);
    Chunk* chunk = &get<1>(*tuple_arg);

    int this_socket = tcp_create_socket();

    int server_ip = inet_addr(package->ip.c_str());
    uint16_t server_port = package->port;
    tcp_connect(this_socket, server_ip, server_port);

    close(this_socket);

    return 0;
}

void* retransmit_request(Data* data) {
    Discovery_Request_Package received_package;
    receive_package: {
        int this_socket = udp_create_socket();

        int ip = INADDR_ANY;
        uint16_t port = data->this_node.port;
        udp_bind(this_socket, ip, port);

        string s = udp_receive(this_socket, discovery_request_max_size);

        received_package = unserialize_discovery_request_package(&s);

        close(this_socket); 
    }

    sleep(1);

    retransmit_package: {
        int this_socket = udp_create_socket();

        Discovery_Request_Package to_send_package = received_package;
        to_send_package.ttl--;
        to_send_package.last_id = data->this_node.id;

        if (to_send_package.ttl > 0) {
            string serialized_package = serialize_discovery_request_package(&to_send_package);

            for (Node& neighbor : data->neighbors) {
                if (neighbor.id == received_package.last_id) {
                    continue;
                }

                int neighbor_ip = inet_addr(neighbor.ip.c_str());
                uint16_t neighbor_port = neighbor.port;
                udp_send(this_socket, neighbor_ip, neighbor_port, &serialized_package);
            }
        }

        close(this_socket); 
    }

    response_package: {
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
            Discovery_Request_Package package = {
                data->metadata_file_name,
                data->ttl,
                data->this_node.id
            };
            package.ttl--;

            string serialized_package = serialize_discovery_request_package(&package);

            int this_socket = udp_create_socket();

            for (Node& neighbor : data->neighbors) {
                int ip = inet_addr(neighbor.ip.c_str());
                uint16_t port = neighbor.port;
                udp_send(this_socket, ip, port, &serialized_package);
            }

            close(this_socket);
        }

        usleep(1'000);

        create_threads_to_receive_chunks: {
            int this_socket = udp_create_socket();
            udp_bind(this_socket, INADDR_ANY, data->this_node.port);

            list<tuple<Discovery_Response_Package, Chunk>> threads_storage;
                
            int chunk_count = 0;
            vector<bool> seen_chunks(data->chunk_count, false);
            while (chunk_count < data->chunk_count) {
                in_addr_t ip = 0;
                in_port_t port = 0;
                string serialized_package = udp_receive(this_socket, discovery_response_package_max_size, &ip, &port);

                Discovery_Response_Package temp_package = unserialize_discovery_response_package(&serialized_package);

                if (!seen_chunks[temp_package.chunk]) {
                    seen_chunks[temp_package.chunk] = true;

                    threads_storage.push_back({std::move(temp_package), Chunk{}});

                    pthread_t new_thread = 0;
                    pthread_create(&new_thread, 0, (Thread_Function*)receive_chunk, &threads_storage.back());
                }
            }

            close(this_socket);
        }
    }
    return 0;
}

int main(int argc, char** argv) {
    assert(argc >= 2);

    Data data = {};
    data.this_node.id = atoi(argv[1]);

    set_nodes(&data, data.this_node.id);
    read_and_set_metadata(&data);

    init_threads: {
        pthread_create(&data.request_file_thread, 0, (Thread_Function*)request_file, &data);
        pthread_create(&data.retransmit_request_thread, 0, (Thread_Function*)retransmit_request, &data);
    }

    pthread_join(data.request_file_thread, 0);
    pthread_join(data.retransmit_request_thread, 0);

    return 0;
}

