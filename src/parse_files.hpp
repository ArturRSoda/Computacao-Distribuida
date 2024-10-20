#pragma once

#include <vector>
#include <string>
#include <fstream>
#include <assert.h>
#include <sstream>

#include "structs.hpp"

using namespace std;

vector<vector<int>> read_topology();

struct Config {
    string ip;
    uint16_t port;
    int transmission_rate;
};

vector<Config> read_config();
void set_nodes(Data* data, int this_id);
