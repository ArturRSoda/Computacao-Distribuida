
#include <cassert>
#include <iostream>

#include "structs.hpp"
#include "utils.hpp"

using namespace std;

int main(int argc, char **argv) {
    assert(argc >= 2);

    Config config;
    config.clientORserver = argv[1];
    config.id = atoi(argv[2]);
    read_config(&config);

    cout << "clientORserver = " << config.clientORserver << endl;
    cout << "id = " << config.id << endl;
    cout << "n_clients = " << config.n_clients << endl;
    cout << "n_servers = " << config.n_servers << endl;
    cout << "Database:" << endl;
    for (auto x : config.dataBase) {
        cout << "    - " << x.variable_name << " " << x.value << " " << x.version << endl;
    }

    if (config.clientORserver == "client") {
        config.my_operations = config.all_operations[config.id];

        cout << "my_operations:" << endl;
        for (auto x : config.my_operations) {
            cout << "    - " << x.type << " " << x.variable_name << " ";
            if (x.type == "read") {
                cout << x.version << " ";
            }
            else if (x.type == "write") {
                cout << x.value << " ";
            }
            cout << x.time << endl;
        }
    }

    /*
    MessageRequestRead rr;
    rr.header.type = request_read;
    rr.client_id = 123;
    rr.variable_name = "x";

    string s = serialize_MessageRequestRead(&rr);
    cout << s << endl;

    rr = unserialize_MessageRequestRead(&s);
    cout << rr.header.type << " " << rr.client_id << " " << rr.variable_name << endl;
    */

    /*
    MessageResponseRead rr;
    rr.value = 123;
    rr.version = "1.0";

    string s = serialize_MessageResponseRead(&rr);
    cout << s << endl;

    rr = unserialize_MessageResponseRead(&s);
    cout << rr.value << " " << rr.version << endl;
    */

    /*
    MessageRequestCommit rc;
    rc.header.type = request_read;
    rc.client_id = 1;
    rc.transaction_id = 2;
    rc.rs.push_back(ReadOp{"x", 3, "1.0"});
    rc.rs.push_back(ReadOp{"w", 5, "1.1"});
    rc.ws.push_back(WriteOp{"y", 4});
    rc.ws.push_back(WriteOp{"z", 6});

    string s;
    s = serialize_MessageRequestCommit(&rc);
    cout << s << endl;
    rc = unserialize_MessageRequestCommit(&s);
    cout << endl;
    cout << rc.header.type << endl;
    cout << rc.client_id << endl;
    cout << rc.transaction_id << endl;
    for (auto x : rc.rs) {
        cout << x.variable_name << " " << x.value << " " << x.version << endl;
    }
    for (auto x : rc.ws) {
        cout << x.variable_name << " " << x.value << endl;
    }
    */

    /*
    MessageResponseCommit rc;
    rc.status = 1;
    rc.transaction_id = 3;
    string s = serialize_MessageResponseCommit(&rc);
    cout << s << endl;
    rc = unserialize_MessageResponseCommit(&s);
    cout << rc.status << " " << rc.transaction_id << endl;
    */

    return 0;
}
