#include <cassert>
#include <condition_variable>
#include <iostream>
#include <string>
#include <sstream>
#include <fstream>

#include "utils.hpp"
#include "structs.hpp"

using namespace std;


string serialize_MessageRequestRead(MessageRequestRead* request) {
    string s;

    s = to_string(request->header.type);
    s += " ";
    s += to_string(request->client_id);
    s += " ";
    s += request->variable_name;
    s += '\n';

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

    return MessageRequestRead{RequestHeader{(RequestType) header_type}, client_id, variable_name};
}

string serialize_MessageResponseRead(MessageResponseRead* response) {
    string s;

    s = to_string(response->value);
    s += " ";
    s += response->version;
    s += '\n';

    return s;
}


MessageResponseRead unserialize_MessageResponseRead(string* serialized) {
    stringstream all_lines(*serialized);
    string s;

    float value;
    string version;

    getline(all_lines, s);
    stringstream ss(s);

    ss >> value;
    ss >> version;

    return MessageResponseRead{value, version};
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
        s += ro.version;
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
        string version;

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

    return MessageRequestCommit{RequestHeader{(RequestType) header_type}, client_id, transaction_id, rs, ws};
}

string serialize_MessageResponseCommit(MessageResponseCommit* response) {
    string s;

    s += to_string(response->status);
    s += " ";
    s += to_string(response->transaction_id);
    s += '\n';

    return s;
}


MessageResponseCommit unserialize_MessageResponseCommit(string* serialized) {
    stringstream all_lines(*serialized);
    string s;

    int status;
    int transaction_id;

    getline(all_lines, s);
    stringstream ss(s);
    ss >> status;
    ss >> transaction_id;

    return MessageResponseCommit{status, transaction_id};
}

void read_config(Config* config) {
    string s;
    vector<Operation> operations;
    bool read_db = false;
    bool read_client_op = false;

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
        else if (header == "-") {
            if (read_db) {
                DatabaseData data;
                ss >> data.variable_name;
                ss >> data.value;
                ss >> data.version;
                config->dataBase.push_back(data);
            }
            else if (read_client_op) {
                Operation op;
                ss >> op.type;
                ss >> op.variable_name;

                if (op.type == "read") {
                    ss >> op.version;
                }
                else if (op.type == "write") {
                    ss >> op.value;
                }
                ss >> op.time;

                operations.push_back(op);
            }
        }
        else if (header == "end") {
            if (read_db) {
                read_db = false;
            }
            else if (read_client_op) {
                read_client_op = false;
                vector<Operation> v(operations);
                config->all_operations.push_back(v);
                operations.clear();
            }
        }
    }
}
