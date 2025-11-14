// zmq_subscriber.hpp
#pragma once

#include <zmq.h>
#include <thread>
#include <atomic>
#include <unordered_map>
#include <functional>
#include <mutex>
#include <stdexcept>
#include <iostream>

#include "result.h"
#include "message.hpp"

struct SubscribeDescriptor {
    std::string topic;
    std::function<Result<void>(const message::Message&)> callback;
};

class ISubscriber {
    using Callback = std::function<Result<void>(const message::Message&)>;

public:
    virtual ~ISubscriber() = default;

    virtual Result<void> init() = 0;
    virtual Result<void> start() = 0;
    virtual Result<void> stop() = 0;

    virtual Result<void> addBus(const std::string& bus) = 0;

    // Subscribe to a topic (exact prefix match)
    virtual Result<void> subscribe(SubscribeDescriptor desc) = 0;
    virtual Result<void> unsubscribe(const std::string& topic) = 0;    
    
   
};
