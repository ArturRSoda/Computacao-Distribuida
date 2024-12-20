#include <csignal>
#include <cstdio>
#include <cstdlib>
#include <cassert>
#include <cstdint>
#include <cstring>
#include <ctime>
#include <cstdint>

#include <queue>
#include <sstream>
#include <iostream>
#include <sys/poll.h>
#include <unordered_map>
#include <vector>

#include <netinet/in.h>
#include <poll.h>
#include <unistd.h>
#include <arpa/inet.h>

#include "structs.hpp"
#include "utils.hpp"

#define PACKET_MAX_SIZE 1000000
#define SERVER_BASE_PORT 1100
#define SEQUENCER_PORT 42000

using namespace std;

void client_func(int n_servers, int id, vector<vector<Operation>> my_operations) {
    timespec initial_time = my_get_time();

    print_time_sec(&initial_time);
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
    char buffer[PACKET_MAX_SIZE] = {0};

    // Randomly select a server
    int server_id = rand() % n_servers;
    print_time_sec(&initial_time);
    cout << "server to connect: " << server_id << endl;

    my_socket = tcp_create_socket();
    print_time_sec(&initial_time);
    cout << "tcp socket created successfully" << endl;

    uint16_t port = SERVER_BASE_PORT + server_id*100;
    print_time_sec(&initial_time);
    if (tcp_connect(my_socket, INADDR_ANY, port))
        cout << "Server connected with server " << server_id << " successfully!" << endl;
    else
        cout << "tcp connection failed" << endl;
    cout << endl;

    int next_transaction_sub_id = 0;
    for (auto transaction : my_operations) {
        int transaction_id = id*100 + next_transaction_sub_id;
        next_transaction_sub_id++;

        bool status = true;
        vector<WriteOp> ws = {};
        vector<ReadOp> rs = {};

        print_time_sec(&initial_time);
        cout << "Transaction ID: " << transaction_id << endl;

        for (auto op : transaction) {
            /* NOTE(felipe): wait for operation time */ {
                double sec_to_milli = 1000.0;
                double nsec_to_milli = 1.0 / 1000000.0;

                int64_t elapsed_time = get_elapsed_time_ms(&initial_time);

                if (elapsed_time < op.time) {
                    int64_t sleep_time_ms = op.time - elapsed_time;
                    int64_t seconds = sleep_time_ms / sec_to_milli;
                    int64_t nanoseconds = (sleep_time_ms % (int64_t)sec_to_milli) / nsec_to_milli;
                    timespec sleep_time = {};
                    sleep_time.tv_sec = seconds;
                    sleep_time.tv_nsec = nanoseconds;
                    nanosleep(&sleep_time, 0);
                }
            }

            if (op.type == "write") {
                print_time_sec(&initial_time);
                cout << "- write " << op.variable_name << " " << op.value << endl;

                ws.push_back(WriteOp{op.variable_name, op.value});

                print_time_sec(&initial_time);
                cout << "variable " << op.variable_name << " added to ws" << endl << endl;
            }
            else if (op.type == "read") {
                print_time_sec(&initial_time);
                cout << "- read " << op.variable_name << endl;

                bool already_in = false;
                for (auto wp : ws) {
                    if (wp.variable_name == op.variable_name) {
                        already_in = true;
                        print_time_sec(&initial_time);
                        cout << "- Variable is already in ws vector" << endl << endl;
                        break;
                    }
                }

                if (already_in) continue;

                MessageRequestRead request {
                    Header{request_read},
                    id,
                    op.variable_name
                };

                string request_str = serialize_MessageRequestRead(&request);

                print_time_sec(&initial_time);
                cout << "Sending read request to server" << endl;

                if (send(my_socket, request_str.c_str(), request_str.size(), 0) < 0) {
                    print_time_sec(&initial_time);
                    perror("Failed to send read request to server");
                    continue;
                }

                print_time_sec(&initial_time);
                cout << "Client sent request to read: " << endl;
                print(&request);

                // Receive a response

                print_time_sec(&initial_time);
                cout << "Reading read response from server" << endl;

                memset(buffer, 0, PACKET_MAX_SIZE);
                int valread = read(my_socket, buffer, PACKET_MAX_SIZE);

                if (valread < 0) {
                    perror("Read error");
                    break;
                } else if (valread == 0) {
                    cout << "Server closed the connection" << endl;
                    break;
                }

                string message = buffer;
                stringstream ss(buffer);
                int ht;
                ss >> ht;

                assert(ht == response_read);

                MessageResponseRead response = unserialize_MessageResponseRead(&message);

                print_time_sec(&initial_time);
                cout << "Client read response:" << endl;
                print(&response);

                rs.push_back(ReadOp{response.variable_name, response.value, response.version});
                cout << "variable " << response.variable_name << " added to rs" << endl << endl;
            }
        }

        if (!ws.empty()) {
            // send request commit
            MessageRequestCommit commit{
                Header{request_commit},
                    id,
                    transaction_id,
                    rs,
                    ws
            };
            string commit_str = serialize_MessageRequestCommit(&commit);

            print_time_sec(&initial_time);
            cout << "Sending commit to server" << endl;
            print(&commit);

            if (send(my_socket, commit_str.c_str(), commit_str.size(), 0) < 0) {
                print_time_sec(&initial_time);
                perror("Failed to send commit request to server");
                continue;
            }
            print_time_sec(&initial_time);
            cout << "Client sent commit successfully " << endl;

            // Receive  response
            print_time_sec(&initial_time);
            cout << "Reading commit response from server" << endl;

            memset(buffer, 0,PACKET_MAX_SIZE);
            int valread = read(my_socket, buffer, PACKET_MAX_SIZE);
            if (valread < 0) {
                perror("Read error");
                break;
            } else if (valread == 0) {
                cout << "Server closed the connection" << endl;
                break;
            }

            string message = buffer;
            stringstream ss(buffer);
            int ht;
            ss >> ht;

            assert(ht == response_commit);

            MessageResponseCommit response = unserialize_MessageResponseCommit(&message);
            print_time_sec(&initial_time);
            cout << "Client received the commit response:" << endl;
            print(&response);

            status = response.status;
            if (!response.status) {
                print_time_sec(&initial_time);
                cout << "The transaction was aborted!" << endl;
            }

        }

        print_time_sec(&initial_time);
        cout << "Final rs vector: {var_name, value, version}" << endl;
        for (auto op: rs){
            cout << "    - { " << op.variable_name << ", " << op.value << ", " << op.version << "}" << endl;
        }
        cout << endl;

        print_time_sec(&initial_time);
        cout << "Final ws vector: {var_name, value}" << endl;
        for (auto op: ws){
            cout << "    - { " << op.variable_name << ", " << op.value << "}" << endl;
        }
        cout << endl;

        print_time_sec(&initial_time);
        cout << "Transaction ended ";
        if (status)
            cout << "with success!" << endl;
        else
            cout << "with fail" << endl;
    }
    close(my_socket);
}


void server_func(int id, vector<DatabaseData> dataBase) {
    timespec initial_time = my_get_time();

    print_time_sec(&initial_time);
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

    uint16_t tcp_port = SERVER_BASE_PORT + id*100;
    char buffer[PACKET_MAX_SIZE] = {0};

    int client_socket;
    client_socket = tcp_create_socket();
    tcp_bind(client_socket, INADDR_ANY, tcp_port);
    tcp_listen(client_socket);

    int sequencer_socket = tcp_create_socket();
    print_time_sec(&initial_time);
    cout << "tcp socket created successfully" << endl;

    print_time_sec(&initial_time);
    if (tcp_connect(sequencer_socket, INADDR_ANY, SEQUENCER_PORT))
        cout << "Server connected with sequencer successfully!" << endl << endl;
    else
        cout << "tcp connection failed" << endl;

    vector<ProcessingCommitRequests> requests_to_respond;

    vector<pollfd> fds;
    fds.push_back({client_socket, POLLIN, 0});
    fds.push_back({sequencer_socket, POLLIN, 0});

    string tcp_buffer = "";
    while (true) {
        int poll_count = poll(fds.data(), fds.size(), 10000);
        if (poll_count == 0) {
            print_time_sec(&initial_time);
            cout << "Poll timeout: no activity detected" << endl;
            continue;
        }
        else if (poll_count < 0) {
            print_time_sec(&initial_time);
            perror("Poll error");
            break;
        }

        for (int i = 0; i < fds.size(); ++i) {
            if (fds[i].revents & POLLIN) {
                if (fds[i].fd == client_socket) {
                    // New connection to client
                    int new_client_socket;
                    in_addr_t client_ip;

                    new_client_socket = tcp_accept(client_socket, &client_ip);
                    print_time_sec(&initial_time);
                    cout << "Server accepted a connection" << endl << endl;

                    fds.push_back({new_client_socket, POLLIN, 0});
                }
                else if (fds[i].fd == sequencer_socket) {
                    print_time_sec(&initial_time);
                    cout << "Reading from sequencer socket" << endl;

                    memset(buffer, 0,PACKET_MAX_SIZE);
                    int valread = read(fds[i].fd, buffer, PACKET_MAX_SIZE);

                    if (valread < 0) {
                        perror("Read error");
                        continue;
                    } else if (valread == 0) {
                        cout << "Lost connection with sequencer" << endl;
                        close(sequencer_socket);
                        close(client_socket);
                        return;
                    }

                    tcp_buffer.append(buffer, valread);

                    while (!tcp_buffer.empty()) {
                        int end_of_package = tcp_buffer.find('$');
                        if (end_of_package == string::npos)
                            break; // already read all availables pakages

                        string message = tcp_buffer.substr(0, end_of_package);
                        tcp_buffer.erase(0, end_of_package+1);

                        stringstream ss(message);
                        int ht;
                        ss >> ht;

                        // receive broadcast from sequencer
                        if (ht == request_commit) {
                            MessageRequestCommit request = unserialize_MessageRequestCommit(&message);

                            print_time_sec(&initial_time);
                            cout << "Server received request commit from sequencer: " << endl;
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

                            print_time_sec(&initial_time);
                            cout << "Sending commit response to sequencer" << endl;

                            if (send(fds[i].fd, response_str.c_str(), response_str.size(), 0) < 0) {
                                print_time_sec(&initial_time);
                                perror("Failed to send the commit response to the sequencer");
                                continue;
                            }

                            print_time_sec(&initial_time);
                            cout << "Server respond the commit request to the sequencer successfully:" << endl;
                            print(&response);
                        }
                        else if (ht == response_commit) {
                            MessageResponseCommit response = unserialize_MessageResponseCommit(&message);

                            print_time_sec(&initial_time);
                            cout << "Server received response commit from sequencer" << endl;
                            print(&response);

                            for (auto it = requests_to_respond.rbegin(); it != requests_to_respond.rend(); ++it) {
                                if (it->package.transaction_id == response.transaction_id) {
                                    print_time_sec(&initial_time);
                                    cout << "Sending response commit to client" << endl;

                                    if (send(it->socket, message.c_str(), message.size(), 0) < 0) {
                                        print_time_sec(&initial_time);
                                        perror("Failed to send commit response to client");
                                        continue;
                                    }

                                    print_time_sec(&initial_time);
                                    cout << "Server sent response commit to client successfully" << endl;
                                    print(&response);

                                    requests_to_respond.erase(std::next(it).base());
                                }
                            }
                        }
                    }
                }
                else {
                    // Request read or request commit from client

                    print_time_sec(&initial_time);
                    cout << "Reading from some client socket" << endl;

                    memset(buffer, 0,PACKET_MAX_SIZE);
                    int valread = read(fds[i].fd, buffer, PACKET_MAX_SIZE);

                    if (valread < 0) {
                        perror("Read Error");
                        continue;
                    } else if (valread <= 0) {
                        print_time_sec(&initial_time);
                        cout << "Client disconnected" << endl;
                        close(fds[i].fd);
                        fds.erase(fds.begin() + i);
                        continue;
                    }

                    string message = buffer;
                    stringstream ss(buffer);
                    int ht;
                    ss >> ht;

                    if (ht == request_read) {
                        MessageRequestRead request = unserialize_MessageRequestRead(&message);

                        print_time_sec(&initial_time);
                        cout << "Server a read request from client: " << endl;
                        print(&request);

                        DatabaseData data = get_db_var(request.variable_name, &dataBase);

                        MessageResponseRead response {
                            Header{response_read},
                                data.variable_name,
                                data.value,
                                data.version
                        };
                        string response_str = serialize_MessageResponseRead(&response);

                        print_time_sec(&initial_time);
                        cout << "Sending read response to client" << endl;

                        if (send(fds[i].fd, response_str.c_str(), response_str.size(), 0) < 0) {
                            print_time_sec(&initial_time);
                            perror("Failed to respond the read request from client");
                            continue;
                        }

                        print_time_sec(&initial_time);
                        cout << "Server respond the read request successfully: " << endl;
                        print(&response);
                    }

                    else if (ht == request_commit) {
                        MessageRequestCommit req_package = unserialize_MessageRequestCommit(&message);

                        print_time_sec(&initial_time);
                        cout << "Server commit request from the client. " << endl;

                        if (send(sequencer_socket, message.c_str(), message.size(), 0) < 0) {
                            print_time_sec(&initial_time);
                            perror("Failed to send the request to sequencer");
                            continue;
                        }

                        print_time_sec(&initial_time);
                        cout << "Request passed to sequencer successfully" << endl;
                        print(&req_package);

                        ProcessingCommitRequests req {
                            req_package,
                            fds[i].fd
                        };
                        requests_to_respond.push_back(req);
                    }
                }
            }
        }
    }
    close(client_socket);
    close(sequencer_socket);
}


void sequencer_func() {
    timespec initial_time = my_get_time();
    char buffer[PACKET_MAX_SIZE] = {0};

    int my_socket;
    my_socket = tcp_create_socket();
    tcp_bind(my_socket, INADDR_ANY, SEQUENCER_PORT);
    tcp_listen(my_socket);

    vector<pollfd> fds;
    fds.push_back({my_socket, POLLIN, 0});
    vector<int> servers_sockets;

    queue<ProcessingCommitRequests> commit_queue;
    unordered_map<int, string> tcp_buffers;

    while (true) {
        int poll_count = poll(fds.data(), fds.size(), 10000);
        if (poll_count < 0) {
            print_time_sec(&initial_time);
            perror("Poll error");
            break;
        } else if (poll_count == 0) {
            print_time_sec(&initial_time);
            cout << "Poll timeout: no activity detected" << endl;
            continue;
        }

        for (size_t i = 0; i < fds.size(); ++i) {
            if (fds[i].revents & POLLIN) {
                if (fds[i].fd == my_socket) {
                    // New server connection
                    int server_socket;
                    in_addr_t server_ip;

                    server_socket = tcp_accept(my_socket, &server_ip);
                    print_time_sec(&initial_time);
                    cout << "Sequencer accepted a connection" << endl << endl;

                    fds.push_back({server_socket, POLLIN, 0});
                    servers_sockets.push_back(server_socket);
                    tcp_buffers[server_socket] = "";
                }
                else {
                    print_time_sec(&initial_time);
                    cout << "Reading from some server socket" << endl << endl;

                    memset(buffer, 0,PACKET_MAX_SIZE);
                    int valread = read(fds[i].fd, buffer, PACKET_MAX_SIZE);

                    if (valread <= 0) {
                        print_time_sec(&initial_time);
                        cout << "Server disconnected" << endl;

                        close(fds[i].fd);
                        tcp_buffers.erase(fds[i].fd);
                        fds.erase(fds.begin() + i);
                        continue;
                    }

                    tcp_buffers[fds[i].fd].append(buffer, valread);

                    while (!tcp_buffers[fds[i].fd].empty()) {
                        int end_of_package = tcp_buffers[fds[i].fd].find('$');
                        if (end_of_package == string::npos)
                            break; // already read all availables pakages

                        string message = tcp_buffers[fds[i].fd].substr(0, end_of_package);
                        tcp_buffers[fds[i].fd].erase(0, end_of_package+1);

                        stringstream ss(message);
                        int ht;
                        ss >> ht;

                        assert (ht == request_commit);
                        MessageRequestCommit package = unserialize_MessageRequestCommit(&message);

                        print_time_sec(&initial_time);
                        cout << "Sequencer received a commit request:" << endl;
                        print(&package);

                        ProcessingCommitRequests commit {
                            package,
                                fds[i].fd
                        };

                        commit_queue.push(commit);
                    }
                }
            }
        }

        // Process commit queue if not empty
        while (!commit_queue.empty()) {

            ProcessingCommitRequests commit_to_broadcast = commit_queue.front();
            commit_queue.pop();

            MessageRequestCommit commit_package = commit_to_broadcast.package;
            string commit_message = serialize_MessageRequestCommit(&commit_package);

            print_time_sec(&initial_time);
            cout << "Sending commit request broadcast" << endl << endl;

            for (int server_socket : servers_sockets) {
                if (send(server_socket, commit_message.c_str(), commit_message.size(), 0) < 0) {
                    print_time_sec(&initial_time);
                    perror("Failed to broadcast to all the servers");
                    continue;
                }
            }

            print_time_sec(&initial_time);
            cout << "Broadcasted commit request to servers successfully" << endl;
            print(&commit_package);

            // wait responses from servers
            vector<MessageResponseCommit> responses;
            int responses_needed = servers_sockets.size();
            while (responses.size() < responses_needed) {
                int poll_count = poll(fds.data(), fds.size(), 10000);
                if (poll_count < 0) {
                    print_time_sec(&initial_time);
                    perror("Poll error");
                    break;
                } else if (poll_count == 0) {
                    print_time_sec(&initial_time);
                    cout << "Poll timeout: no activity detected while waiting for servers broadcast responses" << endl;
                    continue;
                }

                for (size_t i = 0; i < fds.size(); ++i) {
                    if (fds[i].revents & POLLIN) {
                        print_time_sec(&initial_time);
                        cout << "Reading from some server socket" << endl;

                        memset(buffer, 0,PACKET_MAX_SIZE);
                        int valread = read(fds[i].fd, buffer, PACKET_MAX_SIZE);

                        if (valread <= 0) {
                            print_time_sec(&initial_time);
                            cout << "Nothing was read" << endl;
                            continue;
                        }

                        tcp_buffers[fds[i].fd].append(buffer, valread);

                        while (!tcp_buffers[fds[i].fd].empty()) {
                            int end_of_package = tcp_buffers[fds[i].fd].find('$');
                            if (end_of_package == string::npos)
                                break; // already read all availables pakages

                            string response_message = tcp_buffers[fds[i].fd].substr(0, end_of_package);
                            tcp_buffers[fds[i].fd].erase(0, end_of_package+1);

                            stringstream ss(response_message);
                            int ht;
                            ss >> ht;

                            if (ht == response_commit) {
                                MessageResponseCommit response = unserialize_MessageResponseCommit(&response_message);

                                print_time_sec(&initial_time);
                                cout << "Received commit response from server " << i-1 << endl << endl;

                                responses.push_back(response);
                            }
                            else if (ht == request_commit) {

                                MessageRequestCommit req_commit = unserialize_MessageRequestCommit(&response_message);

                                print_time_sec(&initial_time);
                                cout << "Sequencer received a commit request" << endl;
                                print(&req_commit);

                                ProcessingCommitRequests commit{
                                    req_commit,
                                        fds[i].fd
                                };

                                commit_queue.push(commit);
                            }
                        }
                    }
                }
            }

            print_time_sec(&initial_time);
            cout << "Collected all responses from servers" << endl;

            // Determine commit success
            bool success = true;
            for (auto& response : responses) {
                if (!response.status) {
                    success = false;
                    break;
                }
            }

            // Respond to the original client
            MessageResponseCommit final_response{
                Header{response_commit},
                success,
                commit_to_broadcast.package.transaction_id
            };

            string final_response_str = serialize_MessageResponseCommit(&final_response);
            if (send(commit_to_broadcast.socket, final_response_str.c_str(), final_response_str.size(), 0) < 0) {
                print_time_sec(&initial_time);
                perror("Failed to send commit response to server");
                continue;
            }

            print_time_sec(&initial_time);
            cout << "Sequencer sent final commit response to server" << endl;
            print(&final_response);
        }
    }
    close(my_socket);
}

int main(int argc, char **argv) {
    assert(argc >= 2);

    srand(time(0));

    Config config;
    config.clientORserver = argv[1];
    read_config(&config);

    cout << "n_clients = " << config.n_clients << endl;
    cout << "n_servers = " << config.n_servers << endl;

    if (config.clientORserver == "client") {
        config.id = atoi(argv[2]);
        client_func(config.n_servers, config.id, config.all_operations[config.id]);
    }
    else if (config.clientORserver == "server") {
        config.id = atoi(argv[2]);
        server_func(config.id, config.dataBase);
    }
    else if (config.clientORserver == "sequencer") {
        sequencer_func();
    }

    return 0;
}
