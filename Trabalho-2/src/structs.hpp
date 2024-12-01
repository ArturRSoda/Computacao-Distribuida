#pragma once

#include <string>
#include <vector>

using namespace std;

enum HeaderType {
    request_read,
    response_read,
    request_commit,
    response_commit,
};


struct Header {
    HeaderType type;
};


struct Operation {
    string type;
    string variable_name;
    float value;
    float version;
    int time;
};

struct ReadOp {
    string variable_name;
    float value;
    float version;
};

struct WriteOp {
    string variable_name;
    float value;
};

struct MessageRequestRead {
    Header header;
    int client_id;
    string variable_name;
};

struct MessageResponseRead {
    Header header;
    string variable_name;
    float value;
    float version;
};

struct MessageRequestCommit {
    Header header;
    int client_id;
    int transaction_id;
    vector<ReadOp> rs;
    vector<WriteOp> ws;
};

struct MessageResponseCommit {
    Header header;
    int status;
    int transaction_id;
};


struct ProcessingCommitRequests {
    MessageRequestCommit package;
    int socket;
};


struct DatabaseData {
    string variable_name;
    float value;
    float version;
};

struct Config {
    string clientORserver;
    int id;
    int n_clients;
    int n_servers;
    vector<DatabaseData> dataBase;
    vector<vector<vector<Operation>>> all_operations;
};
