#pragma once

#include <unordered_map>
#include <any>
#include <string>


namespace message {
    
struct Message {
    std::string topic;
    std::unordered_map<std::string, std::any> values;
};

enum class ArgType {
    String,
    Int,
    Float,
    Bool,
    Unknown
};



}
