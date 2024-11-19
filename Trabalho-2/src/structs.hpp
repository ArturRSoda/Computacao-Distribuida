#pragma once

#include <string>
#include <vector>

using namespace std;

enum RequestType {
    request_read,
    request_commit
};


struct RequestHeader {
    RequestType type;
};


struct Operation {
    string type;
    string variable_name;
    float value;
    string  version;
    int time;
};

struct ReadOp {
    string variable_name;
    float value;
    string version;
};

struct WriteOp {
    string variable_name;
    float value;
};

struct MessageRequestRead {
    RequestHeader header;
    int client_id;
    string variable_name;
};

struct MessageResponseRead {
    float value;
    string version;
};

struct MessageRequestCommit {
    RequestHeader header;
    int client_id;
    int transaction_id;
    vector<ReadOp> rs;
    vector<WriteOp> ws;
};

struct MessageResponseCommit {
    int status;
    int transaction_id;
};

struct DatabaseData {
    string variable_name;
    float value;
    string version;
};

struct Config {
    string clientORserver;
    int id;
    int n_clients;
    int n_servers;
    vector<DatabaseData> dataBase;
    vector<vector<Operation>> all_operations;
    vector<Operation> my_operations;
};
