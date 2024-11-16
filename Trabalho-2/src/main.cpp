
#include <iostream>

#include "structs.hpp"
#include "utils.hpp"

using namespace std;

int main() {

    MessageRequestCommit rc;
    rc.header.type = request_commit;
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

    return 0;
}
