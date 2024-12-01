#include <cstdio>
#include <iostream>
#include <string>
#include <sstream>
#include <fstream>
#include <cstdint>
#include <cassert>
#include <assert.h>
#include <iomanip>

#include <arpa/inet.h>

#include "utils.hpp"
#include "structs.hpp"

using namespace std;


DatabaseData get_db_var(string var_name, vector<DatabaseData>* db) {
    DatabaseData dbd;
    for (auto v : *db) {
        if (v.variable_name == var_name) {
            dbd = v;
            break;
        }
    }
    return dbd;
}

void set_db_value(string var_name, float value, vector<DatabaseData>* db) {
    for (int i = 0; i < db->size(); i++) {
        if (db->at(i).variable_name == var_name) {
            db->at(i).value = value;
            return;
        }
    }
}

void add_db_version(string var_name, vector<DatabaseData>* db) {
    for (int i = 0; i < db->size(); i++) {
        if (db->at(i).variable_name == var_name) {
            db->at(i).version++;
            return;
        }
    }

}


void print(Header* header) {
    cout << "Type: ";
    if (header->type == request_read) {
        cout << "request_read" << endl;
    } else if (header->type == response_read) {
        cout << "response_read" << endl;
    } else if (header->type == request_commit) {
        cout << "request_commit" << endl;
    } else if (header->type == response_commit) {
        cout << "response_commit" << endl;
    }
}

void print(MessageRequestRead* rr) {
    print(&rr->header);
    cout << "Client ID: " << rr->client_id << endl;
    cout << "Var name: " << rr->variable_name << endl;
    cout << endl;
}

void print(MessageResponseRead* rr) {
    print(&rr->header);
    cout << "Var name: " << rr->variable_name << endl;
    cout << "Value: " << rr->value << endl;
    cout << "Version: " << rr->version << endl;
    cout << endl;
}

void print(MessageRequestCommit* rc) {
    print(&rc->header);
    cout << "Client ID: " << rc->client_id << endl;
    cout << "Transaction ID: " << rc->transaction_id << endl;
    cout << "RS (var_name, value, version): " << endl;
    for (auto x : rc->rs) {
        cout << "    - " << x.variable_name << " " << x.value << " " << x.version << endl;
    }
    cout << "WS (var_name, value): " << endl;
    for (auto x : rc->ws) {
        cout << "    - " << x.variable_name << " " << x.value << endl;
    }
    cout << endl;
}

void print(MessageResponseCommit* rc) {
    print(&rc->header);
    cout << "Status: " << rc->status << endl;
    cout << "Transaction ID: " << rc->transaction_id << endl;
    cout << endl;
}

string serialize_MessageRequestRead(MessageRequestRead* request) {
    string s;

    s = to_string(request->header.type);
    s += " ";
    s += to_string(request->client_id);
    s += " ";
    s += request->variable_name;
    s += '\n';
    s += '$';

    return s;
}


MessageRequestRead unserialize_MessageRequestRead(string* serialized) {
    stringstream all_lines(*serialized);
    string s;

    int header_type;
    int client_id;
    string variable_name;

    getline(all_lines, s);
    stringstream ss(s);

    ss >> header_type;
    ss >> client_id;
    ss >> variable_name;

    return MessageRequestRead{Header{(HeaderType) header_type}, client_id, variable_name};
}

string serialize_MessageResponseRead(MessageResponseRead* response) {
    string s;

    s = to_string(response->header.type);
    s += " ";
    s += response->variable_name;
    s += " ";
    s += to_string(response->value);
    s += " ";
    s += to_string(response->version);
    s += '\n';
    s += '$';

    return s;
}


MessageResponseRead unserialize_MessageResponseRead(string* serialized) {
    stringstream all_lines(*serialized);
    string s;

    int header_type;
    string variable_name;
    float value;
    float version;

    getline(all_lines, s);
    stringstream ss(s);

    ss >> header_type;
    ss >> variable_name;
    ss >> value;
    ss >> version;

    return MessageResponseRead{Header{(HeaderType) header_type}, variable_name, value, version};
}

string serialize_MessageRequestCommit(MessageRequestCommit* request) {
    string s;

    s = to_string(request->header.type);
    s += " ";
    s += to_string(request->client_id);
    s += " ";
    s += to_string(request->transaction_id);
    s += '\n';

    s += "<\n";
    for (auto ro : request->rs) {
        s += ro.variable_name;
        s += " ";
        s += to_string(ro.value);
        s += " ";
        s += to_string(ro.version);
        s += '\n';
    }
    s += ">\n";

    s += "<\n";
    for (auto ro : request->ws) {
        s += ro.variable_name;
        s += " ";
        s += to_string(ro.value);
        s += '\n';
    }
    s += ">\n";
    s += '$';

    return s;
}

MessageRequestCommit unserialize_MessageRequestCommit(string* serialized) {
    stringstream all_lines(*serialized);
    string s;

    int header_type;
    int client_id;
    int transaction_id;
    vector<ReadOp> rs;
    vector<WriteOp> ws;

    getline(all_lines, s);
    stringstream ss(s);
    ss >> header_type;
    ss >> client_id;
    ss >> transaction_id;

    getline(all_lines, s);
    while (true) {
        string variable_name;
        float value;
        float version;

        getline(all_lines, s);
        if (s == ">") break;

        stringstream ss(s);
        ss >> variable_name;
        ss >> value;
        ss >> version;

        rs.push_back(ReadOp{variable_name, value, version});
    }


    getline(all_lines, s);
    while (true) {
        string variable_name;
        float value;

        getline(all_lines, s);
        if (s == ">") break;

        stringstream ss(s);
        ss >> variable_name;
        ss >> value;

        ws.push_back(WriteOp{variable_name, value});
    }

    return MessageRequestCommit{Header{(HeaderType) header_type}, client_id, transaction_id, rs, ws};
}

string serialize_MessageResponseCommit(MessageResponseCommit* response) {
    string s;

    s = to_string(response->header.type);
    s += " ";
    s += to_string(response->status);
    s += " ";
    s += to_string(response->transaction_id);
    s += '\n';
    s += '$';

    return s;
}


MessageResponseCommit unserialize_MessageResponseCommit(string* serialized) {
    stringstream all_lines(*serialized);
    string s;

    int header_type;
    int status;
    int transaction_id;

    getline(all_lines, s);
    stringstream ss(s);
    ss >> header_type;
    ss >> status;
    ss >> transaction_id;

    return MessageResponseCommit{Header{(HeaderType) header_type}, status, transaction_id};
}

int perror_check(int return_value) {
    if (return_value == -1) {
        perror("ERROR");
        assert(false);
        exit(1);
    }
    return return_value;
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

void read_config(Config* config) {
    string s;
    vector<Operation> transaction;
    vector<vector<Operation>> operations;
    bool read_db = false;
    bool read_client_op = false;
    bool read_transaction = false;

    ifstream file("./system_config.txt");
    assert(file.is_open()); 

    while (getline(file, s)) {
        string header;
        stringstream ss(s);

        ss >> header;
        if (header == "n_clients:") {
            ss >> config->n_clients;
        }
        else if (header == "n_servers:") {
            ss >> config->n_servers;
        }
        else if (header == "data_base:") {
            read_db = true;
        }
        else if (header == "client_op:") {
            read_client_op = true;
        }
        else if (header == "transaction:") {
            read_transaction = true;
        }
        else if (header == "-") {
            if (read_db) {
                DatabaseData data;
                ss >> data.variable_name;
                ss >> data.value;
                ss >> data.version;
                config->dataBase.push_back(data);
            }
            else if (read_transaction) {
                Operation op;
                ss >> op.type;
                ss >> op.variable_name;

                if (op.type == "write") {
                    ss >> op.value;
                }

                ss >> op.time;

                transaction.push_back(op);
            }
        }
        else if (header == "end") {
            if (read_db) {
                read_db = false;
            }
            else if (read_client_op) {
                if (read_transaction) {
                    read_transaction = false;
                    vector<Operation> v(transaction);
                    operations.push_back(v);
                    transaction.clear();
                }
                else {
                    read_client_op = false;
                    vector<vector<Operation>> ops;
                    for (auto v : operations) {
                        vector<Operation> v_(v);
                        ops.push_back(v_);
                    }
                    config->all_operations.push_back(ops);
                    operations.clear();
                }
            }
        }
    }
}

timespec my_get_time() {
    timespec curr_time;
    timespec_get(&curr_time, TIME_UTC);
    return curr_time;
}

int64_t get_elapsed_time_ms(timespec* initial_time) {
    double sec_to_milli = 1000.0;
    double nsec_to_milli = 1.0 / 1000000.0;

    timespec curr_time = my_get_time();

    int64_t elapsed_time =
        sec_to_milli * (curr_time.tv_sec - initial_time->tv_sec) +
        nsec_to_milli * (curr_time.tv_nsec - initial_time->tv_nsec);

    return elapsed_time;
}

void print_time_sec(timespec* initial_time) {
    int64_t elapsed_time = get_elapsed_time_ms(initial_time);

    cout << "[" << setfill('0') << setw(4) << elapsed_time / 1000 << "." << setfill('0') << setw(3) << elapsed_time % 1000 << "] ";
}
