#include <iostream>
#include <string>
#include <sstream>

#include "utils.hpp"
#include "structs.hpp"

using namespace std;


string serialize_MessageRequestRead(MessageRequestRead* request) {
    string s;

    s += to_string(request->header.type);
    s += '\n';

    s += to_string(request->client_id);
    s += '\n';

    s += request->variable_name;
    s += '\n';

    return s;
}


MessageRequestRead unserialize_MessageRequestRead(string* serialized) {
    stringstream all_lines(*serialized);

    int header_type;
    int client_id;
    string variable_name;

    {
        string s;
        getline(all_lines, s);
        stringstream line(s);
        line >> header_type;
    }

    {
        string s;
        getline(all_lines, s);
        stringstream line(s);
        line >> client_id;
    }

    {
        string s;
        getline(all_lines, s);
        stringstream line(s);
        line >> variable_name;
    }

    return MessageRequestRead{RequestHeader{(RequestType) header_type}, client_id, variable_name};
}

string serialize_MessageResponseRead(MessageResponseRead* response) {
    string s;

    s += to_string(response->value);
    s += '\n';

    s += response->version;
    s += '\n';

    return s;
}


MessageResponseRead unserialize_MessageResponseRead(string* serialized) {
    stringstream all_lines(*serialized);

    float value;
    string version;

    {
        string s;
        getline(all_lines, s);
        stringstream line(s);
        line >> value;
    }

    {
        string s;
        getline(all_lines, s);
        stringstream line(s);
        line >> version;
    }

    return MessageResponseRead{value, version};
}

string serialize_MessageRequestCommit(MessageRequestCommit* request) {
    string s;

    s += to_string(request->header.type);
    s += '\n';

    s += to_string(request->client_id);
    s += '\n';

    s += to_string(request->transaction_id);
    s += '\n';

    s += "<\n";
    for (auto ro : request->rs) {
        s += ro.variable_name;
        s += '\n';
        s += to_string(ro.value);
        s += '\n';
        s += ro.version;
        s += '\n';
    }
    s += ">\n";

    s += "<\n";
    for (auto ro : request->ws) {
        s += ro.variable_name;
        s += '\n';
        s += to_string(ro.value);
        s += '\n';
    }
    s += ">\n";

    return s;
}

MessageRequestCommit unserialize_MessageRequestCommit(string* serialized) {
    stringstream all_lines(*serialized);

    int header_type;
    int client_id;
    int transaction_id;
    vector<ReadOp> rs;
    vector<WriteOp> ws;

    {
        string s;
        getline(all_lines, s);
        stringstream line(s);
        line >> header_type;
    }

    {
        string s;
        getline(all_lines, s);
        stringstream line(s);
        line >> client_id;
    }

    {
        string s;
        getline(all_lines, s);
        stringstream line(s);
        line >> transaction_id;
    }

    {
        string s;
        getline(all_lines, s);
        stringstream line(s);
    }

    while (true) {
        string variable_name;
        float value;
        string version;

        {
            string s;
            getline(all_lines, s);
            stringstream line(s);
            line >> variable_name;
        }

        if (variable_name == ">") break;

        {
            string s;
            getline(all_lines, s);
            stringstream line(s);
            line >> value;
        }
        {
            string s;
            getline(all_lines, s);
            stringstream line(s);
            line >> version;
        }

        rs.push_back(ReadOp{variable_name, value, version});
    }

    {
        string s;
        getline(all_lines, s);
        stringstream line(s);
    }

    while (true) {
        string variable_name;
        float value;

        {
            string s;
            getline(all_lines, s);
            stringstream line(s);
            line >> variable_name;
        }

        if (variable_name == ">") break;


        {
            string s;
            getline(all_lines, s);
            stringstream line(s);
            line >> value;
        }

        ws.push_back(WriteOp{variable_name, value});
    }

    return MessageRequestCommit{RequestHeader{(RequestType) header_type}, client_id, transaction_id, rs, ws};
}

string serialize_MessageResponseCommit(MessageResponseCommit* response) {
    string s;

    s += to_string(response->status);
    s += '\n';

    s += to_string(response->transaction_id);
    s += '\n';

    return s;
}


MessageResponseCommit unserialize_MessageResponseCommit(string* serialized) {
    stringstream all_lines(*serialized);

    int status;
    int transaction_id;

    {
        string s;
        getline(all_lines, s);
        stringstream line(s);
        line >> status;
    }

    {
        string s;
        getline(all_lines, s);
        stringstream line(s);
        line >> transaction_id;
    }

    return MessageResponseCommit{status, transaction_id};
}
