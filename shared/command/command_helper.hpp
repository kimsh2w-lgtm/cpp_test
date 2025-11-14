#pragma once
#include <string>
#include <any>
#include "command_def.hpp"

#include "message.hpp"

namespace command {

class CommandHelper {
public:
    static bool validate(const CommandInfo& info, const message::Message& args,
                         std::string& error) 
    {
        for (auto& [key, typeSpec] : info.arg_types) {

            // argument 존재 여부 체크
            auto it = args.values.find(key);
            if (it == args.values.end()) {
                error = "Missing required argument: " + key;
                return false;
            }

            // 타입 검사
            if (!validateType(it->second, typeSpec)) {
                error = "Type mismatch for argument: " + key;
                return false;
            }
        }
        return true;
    }

    template<typename T>
    static T get(const message::Message& args, const std::string& key) {
        return std::any_cast<T>(args.values.at(key));
    }

    template<typename T>
    static T getOr(const message::Message& args, const std::string& key, const T& defaultValue) {
        auto it = args.values.find(key);
        if (it == args.values.end()) return defaultValue;
        return std::any_cast<T>(it->second);
    }

private:
    static bool validateType(const std::any& value, message::ArgType t) {
        switch (t) {
            case message::ArgType::String:
                return value.type() == typeid(std::string);
            case message::ArgType::Int:
                return value.type() == typeid(int);
            case message::ArgType::Float:
                return value.type() == typeid(float) ||
                       value.type() == typeid(double);
            case message::ArgType::Bool:
                return value.type() == typeid(bool);
            default:
                return true;
        }
    }
};

} // namespace command
