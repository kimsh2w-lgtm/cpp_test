

#include <nlohmann/json.hpp>
using json = nlohmann::json;

#include "message.hpp"

namespace message
{

std::string serialize(const Message& msg) {
    json j;

    for (auto& [key, val] : msg.values) {
        if (val.type() == typeid(int)) {
            j[key] = std::any_cast<int>(val);
        }
        else if (val.type() == typeid(double)) {
            j[key] = std::any_cast<double>(val);
        }
        else if (val.type() == typeid(bool)) {
            j[key] = std::any_cast<bool>(val);
        }
        else if (val.type() == typeid(std::string)) {
            j[key] = std::any_cast<std::string>(val);
        }
        else {
            // 지원하지 않는 타입 (원한다면 base64 또는 binary 처리)
            throw std::runtime_error("Unsupported type in Message::values");
        }
    }

    return j.dump(); // JSON 문자열로 변환
}


Message deserialize(const std::string& topic, const std::string& payload) {
    Message msg;
    msg.topic = topic;

    json j = json::parse(payload);

    for (auto& [key, val] : j.items()) {
        if (val.is_number_integer()) {
            msg.values[key] = val.get<int>();
        }
        else if (val.is_number_float()) {
            msg.values[key] = val.get<double>();
        }
        else if (val.is_boolean()) {
            msg.values[key] = val.get<bool>();
        }
        else if (val.is_string()) {
            msg.values[key] = val.get<std::string>();
        }
    }

    return msg;
}

} // namespace message

