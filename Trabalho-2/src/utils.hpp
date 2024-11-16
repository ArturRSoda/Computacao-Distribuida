#pragma once

#include "structs.hpp"

using namespace std;

string serialize_MessageRequestRead(MessageRequestRead* request);
string serialize_MessageResponseRead(MessageRequestRead* response);
string serialize_MessageRequestCommit(MessageRequestCommit* request);
string serialize_MessageResponseCommit(MessageResponseCommit* response);
MessageRequestRead unserialize_MessageRequestRead(string* serialized);
MessageResponseRead unserialize_MessageResponseRead(string* serialized);
MessageRequestCommit unserialize_MessageRequestCommit(string* serialized);
MessageResponseCommit unserialize_MessageResponseCommit(string* serialized);

