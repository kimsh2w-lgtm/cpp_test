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

class ZmqPollSubscriber {
public:
    using Callback = std::function<Result<void>(const message::Message&)>;

    Result<void> init(const std::string& endpoint)
    {
        context_ = zmq_ctx_new();
        if (!context_) {
            throw std::runtime_error("Failed to create ZMQ context");
        }

        sub_socket_ = zmq_socket(context_, ZMQ_SUB);
        if (!sub_socket_) {
            zmq_ctx_term(context_);
            throw std::runtime_error("Failed to create ZMQ_SUB socket");
        }

        if (zmq_connect(sub_socket_, endpoint.c_str()) != 0) {
            zmq_close(sub_socket_);
            zmq_ctx_term(context_);
            throw std::runtime_error("Failed to connect SUB socket");
        }

        running_.store(true);
        thread_ = std::thread(&ZmqPollSubscriber::pollLoop, this);

        return OK();
    }

    ~ZmqPollSubscriber()
    {
        stop();
    }

    void stop()
    {
        bool expected = true;
        if (running_.compare_exchange_strong(expected, false)) {
            // wake up poll by setting a very short linger and closing after join
            if (thread_.joinable()) {
                thread_.join();
            }
            zmq_close(sub_socket_);
            zmq_ctx_term(context_);
        }
    }

    Result<void> addMessageBus(const std::string& endpoint) {
        std::lock_guard<std::mutex> lock(mutex_);
        
        auto socket = zmq_socket(context_, ZMQ_SUB);
        if (!socket) {
            zmq_ctx_term(context_);
            return Error(ResultCode::InternalError, "create socket error");
        }

        if (zmq_connect(socket, endpoint.c_str()) != 0) {
            zmq_close(socket);
            zmq_ctx_term(context_);
            return Error(ResultCode::ConnectionFail, "Failed to connect socket");
        }
        sockets_.push_back(socket);
        items_.push_back({ socket, 0, ZMQ_POLLIN, 0 });
        items_dirty.store(true);
    }

    // Subscribe to a topic (exact prefix match)
    void subscribe(const std::string& topic, Callback cb)
    {
        {
            std::lock_guard<std::mutex> lock(callbacks_mutex_);
            callbacks_[topic] = std::move(cb);
        }

        // Add filter to SUB socket
        int rc = zmq_setsockopt(sub_socket_, ZMQ_SUBSCRIBE,
                                topic.data(), static_cast<int>(topic.size()));
        if (rc != 0) {
            std::cerr << "ZMQ_SUBSCRIBE failed for topic: " << topic << "\n";
        }
    }

    // (optional) unsubscribe for a topic (ZeroMQ supports this from 3.2+)
    void unsubscribe(const std::string& topic)
    {
        {
            std::lock_guard<std::mutex> lock(callbacks_mutex_);
            callbacks_.erase(topic);
        }

        int rc = zmq_setsockopt(sub_socket_, ZMQ_UNSUBSCRIBE,
                                topic.data(), static_cast<int>(topic.size()));
        if (rc != 0) {
            std::cerr << "ZMQ_UNSUBSCRIBE failed for topic: " << topic << "\n";
        }
    }

private:
    void pollLoop()
    {
        // poll item for our single SUB socket
        zmq_pollitem_t items[] = {
            { sub_socket_, 0, ZMQ_POLLIN, 0 }
        };

        while (running_.load()) {
            if (items_dirty) {
                std::lock_guard<std::mutex> lock(items_mutex);
                rebuildItemsVector();  // items를 새롭게 구성
                items_dirty = false;
            }

            // timeout in ms (e.g., 100ms) – lets us check running_ periodically
            int rc = zmq_poll(items, 1, 100);
            if (rc < 0) {
                // interrupted or error
                if (!running_.load())
                    break;
                continue;
            }

            if (items[0].revents & ZMQ_POLLIN) {
                receiveAndDispatch();
            }
        }
    }

    void receiveAndDispatch()
    {
        // ZeroMQ pub/sub: frame 0 = topic, frame 1 = payload
        char topic_buf[256];
        char msg_buf[4096];

        int tsize = zmq_recv(sub_socket_, topic_buf, sizeof(topic_buf), 0);
        if (tsize <= 0) return;

        int msize = zmq_recv(sub_socket_, msg_buf, sizeof(msg_buf), 0);
        if (msize <= 0) return;

        message::Message m;
        m.topic   = std::string(topic_buf, tsize);
        m.payload = std::string(msg_buf, msize);

        Callback cb;

        {
            std::lock_guard<std::mutex> lock(callbacks_mutex_);
            auto it = callbacks_.find(m.topic);
            if (it != callbacks_.end()) {
                cb = it->second; // copy out to call outside lock
            }
        }

        if (cb) {
            cb(m);
        }
        // else: topic subscribed at ZeroMQ level but no callback registered
    }

private:
    void* context_     = nullptr;
    void* sub_socket_  = nullptr;

    std::mutex mutex_;
    std::atomic<bool> running_{false};
    std::thread thread_;

    std::vector<void*> sockets_;
    std::vector<zmq_pollitem_t> items_;

    std::unordered_map<std::string, Callback> callbacks_;
    std::mutex callbacks_mutex_;

    std::atomic<bool> items_dirty{false};
};
