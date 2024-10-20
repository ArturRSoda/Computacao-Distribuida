#include "parse_files.hpp"

vector<vector<int>> read_topology() {
    ifstream file("../topologia.txt");
    assert(file.is_open());

    vector<vector<int>> topology;
    while (true) {
        string s;
        std::getline(file, s);

        if (!file) {
            break;
        }

        stringstream ss(s);

        int from;
        ss >> from;

        char colon;
        ss >> colon;

        vector<int> nodes_to;

        bool first = true;
        while (true) {
            if (!first) {
                char comma;
                ss >> comma;
            }
            first = false;

            int to;
            ss >> to;

            if (!ss) {
                break;
            }

            nodes_to.push_back(to);
        }

        topology.push_back(nodes_to);
    }

    return topology;
}

vector<Config> read_config() {
    ifstream file("../config.txt");
    assert(file.is_open());

    vector<Config> configs;
    while (true) {
        string s;
        std::getline(file, s);

        if (!file) {
            break;
        }

        stringstream ss(s);

        int from;
        ss >> from;

        char colon;
        ss >> colon;

        Config config;

        char comma;

        string ip;
        uint16_t port;
        int transmission_rate;
        {
            while (true) {
                char c;
                ss >> c;

                if (!ss) {
                    break;
                }

                if (c == ',') {
                    break;
                }

                ip.push_back(c);
            }

            ss >> port;
            ss >> comma;

            ss >> transmission_rate;
        }

        configs.push_back({ip, port, transmission_rate});
    }

    return configs;
}

void set_nodes(Data* data, int this_id) {
    vector<vector<int>> topology = read_topology();
    vector<Config> configs = read_config();

    vector<Node> neighbors;
    for (int neighbor_id : topology[this_id]) {
        Config* neighbor_config = &configs[neighbor_id];
        neighbors.push_back({neighbor_id, neighbor_config->ip, neighbor_config->port});
    }
    data->neighbors = neighbors;
    data->this_node.ip = configs[data->this_node.id].ip;
    data->this_node.port = configs[data->this_node.id].port;
    data->transmission_rate = configs[data->this_node.id].transmission_rate;
}

