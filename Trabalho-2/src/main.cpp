#include <cstdio>
#include <cstdlib>
#include <sstream>
#include <cassert>
#include <cstdint>
#include <iostream>
#include <netinet/in.h>
#include <poll.h>
#include <iostream>
#include <cstring>
#include <vector>
#include <poll.h>
#include <unistd.h>
#include <arpa/inet.h>

#include "structs.hpp"
#include "utils.hpp"

using namespace std;

void client_func(int n_servers, int id, vector<vector<Operation>> my_operations) {
    cout << "clientORserver = client" << endl;
    cout << "my_id: " << id << endl;
    cout << "my_operations:" << endl;
    for (auto transaction : my_operations) {
        cout << "    transaction:" << endl;
        for (auto x : transaction) {
            cout << "        - " << x.type << " " << x.variable_name << " ";
            if (x.type == "write") {
                cout << x.value << " ";
            }
            cout << x.time << endl;
 }
    }

    int my_socket;
    char buffer[packet_max_size] = {0};
    vector<WriteOp> ws = {};
    vector<ReadOp> rs = {};

    // Randomly select a server
    int server_id = rand() % n_servers;
    cout << "server to connect: " << server_id << endl;

    my_socket = tcp_create_socket();
    cout << "tcp socket created successfully" << endl;

    uint16_t port = 1000 + (server_id+1)*100;
    if (tcp_connect(my_socket, INADDR_ANY, port)) {
        cout << "client connected with server " << server_id << " successfully!" << endl;
    } else {
        cout << "tcp connection failed" << endl;
    }

    cout << endl;

    /* AINDA NAO CONSIDERA O TEMPO DAS OPERACOES */
    for (auto transaction : my_operations) {
        int transaction_id = rand() % 10000;
        cout << "Transaction ID: " << transaction_id << endl;

        for (auto op : transaction) {
            if (op.type == "write") {
                cout << "- write " << op.variable_name << " " << op.value << endl;
                ws.push_back(WriteOp{op.variable_name, op.value});
                cout << "variable " << op.variable_name << " added to ws" << endl << endl;
            }
            else if (op.type == "read") {
                cout << "- read " << op.variable_name << endl;

                bool already_in = false;
                for (auto wp : ws) {
                    if (wp.variable_name == op.variable_name) {
                        already_in = true;
                        cout << "- Variable is already in ws vector" << endl << endl;
                        break;
                    }
                }

                if (!already_in) {
                    MessageRequestRead request {
                        Header{request_read},
                            id,
                            op.variable_name
                    };

                    string request_str = serialize_MessageRequestRead(&request);
                    send(my_socket, request_str.c_str(), request_str.size(), 0);
                    cout << "Client sent: " << endl;
                    print(&request);

                     // Receive a response
                    memset(buffer, 0, packet_max_size);
                    int valread = read(my_socket, buffer, packet_max_size);
                    if (valread > 0) {
                        string message = buffer;
                        stringstream ss(buffer);
                        int ht;
                        ss >> ht;
                        assert(ht == response_read);

                        MessageResponseRead response = unserialize_MessageResponseRead(&message);
                        cout << "Client received:" << endl;
                        print(&response);

                        rs.push_back(ReadOp{response.variable_name, response.value, response.version});
                        cout << "variable " << response.variable_name << " added to rs" << endl << endl;
                    }
                }
            }
        }

        cout << "End of Transaction " << transaction_id << endl << endl;

        /* TODO  fazer difusao atomica */

    }

    close(my_socket);
}


void server_func(int id, vector<DatabaseData> dataBase) {
    cout << "clientORserver = client" << endl;
    cout << "my_id: " << id << endl;
    cout << "Database:" << endl;
    for (auto x : dataBase) {
        cout << "    - " << x.variable_name << " " << x.value << " " << x.version << endl;
    }

    int lastCommitted = 0;
    for (int i = 0; i < dataBase.size(); i++) {
        dataBase[i].version = 0.0;
    }

    int my_socket;
    uint16_t tcp_port = 1000 + (id+1)*100;
    char buffer[packet_max_size] = {0};


    my_socket = tcp_create_socket();
    tcp_bind(my_socket, INADDR_ANY, tcp_port);
    tcp_listen(my_socket);

    vector<pollfd> fds;
    fds.push_back({my_socket, POLLIN, 0});

    while (true) {
        int poll_count = poll(fds.data(), fds.size(), -1);
        if (poll_count < 0) {
            perror("Poll error");
            break;
        }

        for (size_t i = 0; i < fds.size(); ++i) {
            if (fds[i].revents & POLLIN) {
                if (fds[i].fd == my_socket) {
                    // New connection
                    int client_socket;
                    in_addr_t client_ip;

                    client_socket = tcp_accept(my_socket, &client_ip);
                    cout << "Server accepted a connection" << endl;

                    fds.push_back({client_socket, POLLIN, 0});
                }
                else {
                    // Incoming data
                    int valread = read(fds[i].fd, buffer, packet_max_size);
                    if (valread > 0) {
                        string message = buffer;
                        stringstream ss(buffer);
                        int ht;
                        ss >> ht;

                        cout << "received something" << endl;

                        if (ht == request_read) {
                            MessageRequestRead request = unserialize_MessageRequestRead(&message);

                            cout << "server received: " << endl;
                            print(&request);

                            DatabaseData data = get_db_var(request.variable_name, &dataBase);

                            MessageResponseRead response {
                                Header{response_read},
                                    data.variable_name,
                                    data.value,
                                    data.version
                            };

                            string response_str = serialize_MessageResponseRead(&response);
                            send(fds[i].fd, response_str.c_str(), response_str.size(), 0);

                            cout << "server sent: " << endl;
                            print(&response);
                        }
                        else if (ht == request_commit) {
                            MessageRequestCommit request = unserialize_MessageRequestCommit(&message);
                            cout << "server received: " << endl;
                            print(&request);

                            bool abort = false;
                            for (auto read_op : request.rs) {
                                DatabaseData data = get_db_var(read_op.variable_name, &dataBase);

                                if (data.version > read_op.version) {
                                    abort = true;
                                    break;
                                }
                            }

                            if (!abort) {
                                lastCommitted++;
                                for (auto write_op : request.ws) {
                                    add_db_version(write_op.variable_name, &dataBase);
                                    set_db_value(write_op.variable_name, write_op.value, &dataBase);
                                }
                            }

                            MessageResponseCommit response {
                                Header {response_commit},
                                !abort,
                                request.transaction_id
                            };

                            string response_str = serialize_MessageResponseCommit(&response);
                            send(fds[i].fd, response_str.c_str(), response_str.size(), 0);

                            cout << "server sent: " << endl;
                            print(&response);
                        }

                    }
                    else {
                        cout << "Client disconnected" << endl;
                        close(fds[i].fd);
                        fds.erase(fds.begin() + i);
                    }
                }
            }
        }
    }

    close(my_socket);

}

int main(int argc, char **argv) {
    assert(argc >= 2);

    Config config;
    config.clientORserver = argv[1];
    config.id = atoi(argv[2]);
    read_config(&config);

    cout << "n_clients = " << config.n_clients << endl;
    cout << "n_servers = " << config.n_servers << endl;

    if (config.clientORserver == "client") {
        client_func(config.n_servers, config.id, config.all_operations[config.id]);
    }
    else if (config.clientORserver == "server") {
        server_func(config.id, config.dataBase);
    }

    return 0;
}
