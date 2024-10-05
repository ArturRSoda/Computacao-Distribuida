#undef NDEBUG

#include <stdio.h>
#include <assert.h>
#include <stdlib.h>
#include <ctype.h>

#include <iostream>
#include <sstream>
#include <vector>
#include <fstream>
#include <string>

#include <unistd.h>
#include <fcntl.h>
#include <arpa/inet.h>
#include <sys/socket.h>

using std::vector;
using std::stringstream;
using std::ifstream;
using std::string;
using std::to_string;
using std::cout;
using std::cin;

struct Config {
    string ip;
    uint16_t port;
    int transmission_rate;
};

struct Data {
    int id;
    vector<int> neighbors;
    vector<Config> configs;
    string metadata_file_name;
    int chunk_count;
    int ttl;
};

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

                // TODO(felipe): this code breaks if the comma does not come immediately after the ip
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

void read_and_set_metadata(Data* data) {
    ifstream file("../arquivo.p2p");
    assert(file.is_open());

    file >> data->metadata_file_name;
    file >> data->chunk_count;
    file >> data->ttl;
}

int perror_check(int return_value) {
    if (return_value == -1) {
        perror("ERROR");
        assert(false);
        exit(1);
    }
    return return_value;
}

struct Discovery_Package {
    string file;
    vector<int> chunks;
    int ttl;
    string ip;
    uint16_t port;
    int request_id;
};

constexpr int discovery_package_max_size = 4096;

Discovery_Package unserialize_discovery_package(string* serialized) {
    stringstream all_lines(*serialized);

    string file;
    vector<int> chunks;
    int ttl;
    string ip;
    uint16_t port;
    int request_id;
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

        while (true) {
            int chunk;
            line >> chunk;
            if (!line) {
                break;
            }

            chunks.push_back(chunk);
        }
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
        
        line >> request_id;
    }

    return Discovery_Package{file, chunks, ttl, ip, port, request_id};
}

string serialize_discovery_package(Discovery_Package* package) {
    string s;

    s += package->file;
    s += '\n';

    bool first = true;
    for (int chunk : package->chunks) {
        if (!first) {
            s += " ";
        }
        first = false;
        s += to_string(chunk);
    }
    s += "\n";

    s += to_string(package->ttl);
    s += '\n';

    s += package->ip;
    s += '\n';

    s += to_string(package->port);
    s += '\n';

    s += to_string(package->request_id);
    s += '\n';

    return s;
}

int create_non_blocking_udp_socket() {
    int this_socket = perror_check(
        socket(AF_INET, SOCK_DGRAM, 0));

    int flags = fcntl(this_socket, F_GETFL, 0);
    fcntl(this_socket, F_SETFL, O_NONBLOCK|flags);

    return this_socket;
}

int create_blocking_udp_socket() {
    return perror_check(
        socket(AF_INET, SOCK_DGRAM, 0));
}

void my_bind(int this_socket, int address, uint16_t port) {
    sockaddr_in s_addr = {};
    s_addr.sin_family = AF_INET;
    s_addr.sin_addr.s_addr = address;
    s_addr.sin_port = htons(port);
    perror_check(
        bind(this_socket, (const struct sockaddr*)&s_addr, sizeof(s_addr)));
}

string udp_receive(int this_socket, int max_bytes) {
    string s;
    s.resize(max_bytes);

    sockaddr_in other_address = {};
    socklen_t other_address_len = sizeof(other_address);
    int n = recvfrom(
            this_socket, s.data(), s.size(),
            MSG_WAITALL, (struct sockaddr*)&other_address, &other_address_len
    );

    if (n < 0) {
        if (errno == EAGAIN || errno == EWOULDBLOCK) {
            return {};
        } else {
            perror_check(n);
        }
    }

    s.resize(n);

    return s;
}

void udp_send(int this_socket, in_addr_t ip, uint16_t port, string* message) {
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

// TODO(felipe): retransmit UDP discovery transmission (timeout?)
void peer_to_peer_request_file(Data* data) {
    vector<int> chunks;
    for (int i = 0; i < data->chunk_count; i++) {
        chunks.push_back(i);
    }

    Config* this_config = &data->configs[data->id];

    Discovery_Package package = {
        data->metadata_file_name,
        chunks,
        data->ttl,
        this_config->ip,
        this_config->port,
        data->id
    };
    package.ttl--;

    string serialized_package = serialize_discovery_package(&package);

    int this_socket = create_blocking_udp_socket();

    for (int neighbor : data->neighbors) {
        Config* neighbor_config = &data->configs[neighbor];

        in_addr_t ip = inet_addr(neighbor_config->ip.c_str());
        uint16_t port = neighbor_config->port;
        udp_send(this_socket, ip, port, &serialized_package);
    }

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

    //        //receive_chunks(data, &clients);
    //    }
    //}
}

// TODO(felipe): consider that a chunk may not exist in all nodes

void peer_to_peer_respond_file(Data* data) {
    Config* this_config = &data->configs[data->id];
    Discovery_Package discovery_package;

    int this_socket = create_blocking_udp_socket();

    {
        int this_socket = create_blocking_udp_socket();

        int ip = inet_addr("127.0.0.1");
        uint16_t port = this_config->port;
        my_bind(this_socket, ip, port);

        // TODO: blocking
        string s = udp_receive(this_socket, discovery_package_max_size);

        discovery_package = unserialize_discovery_package(&s);
        cout << s;
    }
    // TODO: add retransmit time
    {
        discovery_package.ttl--;

        if (discovery_package.ttl > 0) {
            string serialized_package = serialize_discovery_package(&discovery_package);

            for (int neighbor : data->neighbors) {
                if (neighbor == discovery_package.request_id) {
                    continue;
                }

                Config* neighbor_config = &data->configs[neighbor];

                in_addr_t ip = inet_addr(neighbor_config->ip.c_str());
                uint16_t port = neighbor_config->port;
                udp_send(this_socket, ip, port, &serialized_package);
            }
        }
    }

    close(this_socket); 
}

int main(int argc, char** argv) {
    assert(argc >= 2);

    Data data = {};
    data.id = atoi(argv[1]);

    vector<vector<int>> topology = read_topology();
    vector<Config> configs = read_config();

    data.neighbors = topology[data.id];
    data.configs = configs;

    read_and_set_metadata(&data);

    bool receive;
    {
        cout << "Voce quer requisitar arquivo? [s/n]: ";

        char answer;
        cin >> answer;

        answer = tolower(answer);

        assert(answer == 's' || answer == 'n');
        receive = (answer == 's');
    }
    
    if (receive) {
        peer_to_peer_request_file(&data);
    } else {
        peer_to_peer_respond_file(&data);
    }
}

