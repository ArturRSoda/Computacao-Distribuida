#include "utils.hpp"
#include <cstdint>
#include <string>

bool compareByTransmissionRate(const Discovery_Response_Packet &a, const Discovery_Response_Packet &b) {
    return a.transmission_rate < b.transmission_rate;
}

timespec my_get_time() {
    timespec curr_time;
    timespec_get(&curr_time, TIME_UTC);
    return curr_time;
}

ostream& operator<<(ostream& os, timespec ts) {
    int precision = 2;
    double time = (double)(ts.tv_sec % 1000) + ((double)ts.tv_nsec / 1000000000 / precision);
    os << "[" << fixed << setprecision(precision) << time << "]: ";
    return os;
}

void print(Header* header) {
    cout << "Type: ";
    if (header->type == type_request) {
        cout << "request" << endl;
    } else if (header->type == type_response) {
        cout << "response" << endl;
    }
    cout << "TTL: " << header->ttl << endl;
    cout << "Last id: " << header->last_id << endl;
}

void print(Discovery_Request_Packet* packet) {
    print(&packet->header);
    cout << "File: " << packet->file << endl;
    cout << "Request id: " << packet->request_id << endl;
    cout << "Request ip: " << packet->request_ip << endl;
    cout << "Request port: " << packet->request_port << endl;
    cout << endl;
}

void print(Discovery_Response_Packet* packet) {
    print(&packet->header);
    cout << "Origin ID " << packet->id << endl;
    cout << "IP: " << packet->ip << endl;
    cout << "Port: " << packet->port << endl;
    cout << "Chunk: " << packet->chunk << endl;
    cout << "Transmission Rate: " << packet->transmission_rate << endl;
    cout << endl;
}

int perror_check(int return_value) {
    if (return_value == -1) {
        perror("ERROR");
        assert(false);
        exit(1);
    }
    return return_value;
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

    int request_id;
    {
        string s;
        getline(all_lines, s);
        stringstream line(s);

        line >> request_id;
    }

    string request_ip;
    {
        string s;
        getline(all_lines, s);
        stringstream line(s);

        line >> request_ip;
    }

    uint16_t request_port;
    {
        string s;
        getline(all_lines, s);
        stringstream line(s);

        line >> request_port;
    }

    return Discovery_Request_Packet{Header{}, file, request_id, request_ip, request_port};
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

    s += packet->request_id;
    s += '\n';

    s += packet->request_ip;
    s += '\n';

    s += to_string(packet->request_port);
    s += '\n';

    return s;
}

Discovery_Response_Packet unserialize_discovery_response_packet(string* serialized) {
    stringstream all_lines(*serialized);

    int id;
    string ip;
    uint16_t port;
    int chunk;
    int transmission_rate;
    {
        string s;
        getline(all_lines, s);
        stringstream line(s);

        line >> id;
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

        line >> chunk;
    }
    {
        string s;
        getline(all_lines, s);
        stringstream line(s);

        line >> transmission_rate;
    }

    return Discovery_Response_Packet{Header{}, id, ip, port, chunk, transmission_rate};
}

string serialize_discovery_response_packet(Discovery_Response_Packet* packet) {
    string s;

    s += to_string(packet->header.type);
    s += '\n';

    s += to_string(packet->header.ttl);
    s += '\n';

    s += to_string(packet->header.last_id);
    s += '\n';

    s += to_string(packet->id);
    s += '\n';

    s += packet->ip;
    s += '\n';

    s += to_string(packet->port);
    s += '\n';

    s += to_string(packet->chunk);
    s += '\n';

    s += to_string(packet->transmission_rate);
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

void tcp_listen(int this_socket) {
    int max_connections = 10;

    perror_check(
        listen(this_socket, max_connections));
}

int tcp_accept(int this_socket, in_addr_t* ip) {
    sockaddr_in client = {};
    socklen_t len = sizeof(client);
    int client_socket = accept(this_socket, (sockaddr*)&client, &len);

    if (client_socket < 0 && errno != EAGAIN && errno != EWOULDBLOCK) {
        perror_check(client_socket);
    }

    if (ip) {
        *ip = client.sin_addr.s_addr;
    }

    return client_socket;
}

void udp_bind(int this_socket, int address, uint16_t port) {
    sockaddr_in s_addr = {};
    s_addr.sin_family = AF_INET;
    s_addr.sin_addr.s_addr = address;
    s_addr.sin_port = htons(port);
    perror_check(
        bind(this_socket, (const struct sockaddr*)&s_addr, sizeof(s_addr)));
}

bool tcp_connect(int this_socket, int address, uint16_t port) {
    sockaddr_in s_addr = {};
    s_addr.sin_family = AF_INET;
    s_addr.sin_addr.s_addr = address;
    s_addr.sin_port = htons(port);
    int result = connect(this_socket, (sockaddr*)&s_addr, sizeof(s_addr));

    bool connected = true;
    if (result < 0) {
        if (errno == ECONNREFUSED) {
            connected = false;
        } else {
            perror_check(result);
        }
    }
    
    return connected;
}

string general_receive(int this_socket, int max_bytes, in_addr_t* ip, in_port_t* port) {
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

string tcp_receive(int this_socket, int max_bytes, in_addr_t* ip, in_port_t* port) {
    return general_receive(this_socket, max_bytes, ip, port);
}

string udp_receive(int this_socket, int max_bytes, in_addr_t* ip, in_port_t* port) {
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

