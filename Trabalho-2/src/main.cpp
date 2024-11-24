
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
        for (auto transaction : config.my_operations) {
            cout << "    transaction:" << endl;
            for (auto x : transaction) {
                cout << "        - " << x.type << " " << x.variable_name << " ";
                if (x.type == "write") {
                    cout << x.value << " ";
                }
                cout << x.time << endl;
            }
        }
    }


    return 0;
}
