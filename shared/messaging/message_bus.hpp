#pragma once
#include <memory>
#include <string>
#include <functional>
#include <vector>

#include "result.h"


class MessageSubscription {
public:
    virtual ~MessageSubscription() = default;
    virtual Result<void> unsubscribe() = 0;
};

class MessageBus {
public:
    virtual ~MessageBus() = default;

    // PUB
    virtual Result<void> publish(const std::string& topic, const std::string& msg) = 0;

    // SUB
    virtual std::unique_ptr<MessageSubscription> subscribe(
        const std::string& topic,
        std::function<void(const Message&)> callback) = 0;

    // REQ/REP
    virtual std::string request(const std::string& endpoint, const std::string& msg) = 0;
    virtual void reply(const std::string& endpoint,
        std::function<std::string(const std::string&)> handler) = 0;
};
