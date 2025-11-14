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
#include "worker.hpp"
#include "message_helper.hpp"
#include "subscriber.hpp"


class ZmqPollSubscriber : virtual public ISubscriber {

public:
    ~ZmqPollSubscriber()
    {
        stop();
    }

    Result<void> init() {
        context_ = zmq_ctx_new();
        if (!context_) {
            return Error(ResultCode::InternalError, "Failed to create ZMQ context");
        }

        running_.store(true);

        //thread_ = std::thread(&ZmqPollSubscriber::pollLoop, this);

        return OK();
    }

    Result<void> start() {

    }

    Result<void> stop() {
        bool expected = true;
        if (running_.compare_exchange_strong(expected, false)) {
            // wake up poll by setting a very short linger and closing after join
            if (thread_.joinable()) {
                thread_.join();
            }
            zmq_close(sub_socket_);
            zmq_ctx_term(context_);
        }
        return OK();
    }

    Result<void> addBus(const std::string& bus) {
        std::lock_guard<std::mutex> lock(mutex_);
        
        auto socket = zmq_socket(context_, ZMQ_SUB);
        if (!socket) {
            zmq_ctx_term(context_);
            return Error(ResultCode::SocketError, "create socket error");
        }

        if (zmq_connect(socket, bus.c_str()) != 0) {
            zmq_close(socket);
            zmq_ctx_term(context_);
            return Error(ResultCode::ConnectionFail, "Failed to connect socket");
        }
        sockets_.push_back(socket);
        items_.push_back({ socket, 0, ZMQ_POLLIN, 0 });
        items_dirty.store(true);
    }

    Result<void> subscribe(SubscribeDescriptor desc) { 
        {
            std::lock_guard<std::mutex> lock(callbacks_mutex_);
            auto it = callbacks_.find(desc.topic);
            if(it != callbacks_.end()) {
                auto list = it->second;
                list.push_back(std::move(desc));
            }
        }
    }

    Result<void> unsubscribe(const std::string& topic) {

    }

    

    Result<void> addMessageBus(const std::string& endpoint) {
        
    }



private:
    void pollLoop()
    {
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

    }

private:
    class WorkerImpl : public task::Worker {
    public:
        WorkerImpl(ZmqPollSubscriber& parent) : parent_(parent) {
            task::WorkerDescriptor wd;
            wd.name = "Subscriber";
            wd.type = task::WorkerType::Loop;
            wd.loop_sleep_ms = 2;        

            auto r = Worker::init(wd);
            if (!r) {
                LOGE("Subscriber inner thread init failed: {}", r.error());
            }
        }
        
    protected:
        Result<void> run() override {
            parent_.pollLoop();
        }

    private:
        ZmqPollSubscriber& parent_;
    };

    void* context_     = nullptr;
    void* sub_socket_  = nullptr;

    std::mutex mutex_;
    std::atomic<bool> running_{false};
    std::thread thread_;

    std::vector<void*> sockets_;
    std::vector<zmq_pollitem_t> items_;

    std::unordered_map<std::string, std::vector<SubscribeDescriptor>> callbacks_;
    std::mutex callbacks_mutex_;

    std::atomic<bool> items_dirty{false};

    WorkerImpl worker_;
};
