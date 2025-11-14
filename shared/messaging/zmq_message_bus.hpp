#pragma once
#include "message_bus.hpp"
#include <zmq.h>
#include <thread>
#include <atomic>
#include <unordered_map>

#include "message.hpp"

class ZmqMessageSubscription : public MessageSubscription {
public:
    ZmqMessageSubscription(std::atomic<bool>& running) 
        : running_(running) {}

    Result<void> unsubscribe() override {
        running_.store(false);
    }
private:
    std::atomic<bool>& running_;
};


class ZmqMessageBus : public MessageBus {
public:
    ZmqMessageBus() {
        context_ = zmq_ctx_new();
    }

    ~ZmqMessageBus() {
        shutdown();
        zmq_ctx_destroy(context_);
    }

    // -------------------------
    // PUBLISH
    // -------------------------
    Result<void> publish(const std::string& topic, const std::string& msg) override {
        void* socket = getOrCreatePubSocket();

        zmq_send(socket, topic.data(), topic.size(), ZMQ_SNDMORE);
        zmq_send(socket, msg.data(), msg.size(), 0);
    }

    // -------------------------
    // SUBSCRIBE
    // -------------------------
    std::unique_ptr<MessageSubscription> subscribe(
        const std::string& topic,
        std::function<void(const message::Message&)> callback) override 
    {
        void* socket = zmq_socket(context_, ZMQ_SUB);
        zmq_setsockopt(socket, ZMQ_SUBSCRIBE, topic.c_str(), topic.size());
        zmq_connect(socket, "tcp://127.0.0.1:5555");

        auto running = std::make_shared<std::atomic<bool>>(true);

        std::thread([=]() {
            while (running->load()) {
                char topic_buf[256];
                char msg_buf[1024];

                int tsize = zmq_recv(socket, topic_buf, sizeof(topic_buf), 0);
                if (tsize < 0) continue;

                int msize = zmq_recv(socket, msg_buf, sizeof(msg_buf), 0);
                if (msize < 0) continue;

                Message m;
                m.topic = std::string(topic_buf, tsize);
                m.payload = std::string(msg_buf, msize);

                callback(m);
            }
            zmq_close(socket);
        }).detach();

        return std::make_unique<ZmqMessageSubscription>(*running);
    }

    // -------------------------
    // REQUEST
    // -------------------------
    std::string request(const std::string& endpoint, const std::string& msg) override {
        void* socket = zmq_socket(context_, ZMQ_REQ);
        zmq_connect(socket, endpoint.c_str());

        zmq_send(socket, msg.data(), msg.size(), 0);

        char buf[1024];
        int size = zmq_recv(socket, buf, sizeof(buf), 0);

        zmq_close(socket);
        return std::string(buf, size);
    }

    // -------------------------
    // REPLY
    // -------------------------
    void reply(const std::string& endpoint,
        std::function<std::string(const std::string&)> handler) override 
    {
        void* socket = zmq_socket(context_, ZMQ_REP);
        zmq_bind(socket, endpoint.c_str());

        std::thread([this, socket, handler]() {
            char buf[1024];

            while (running_.load()) {
                int size = zmq_recv(socket, buf, sizeof(buf), 0);
                if (size < 0) continue;

                std::string response = handler(std::string(buf, size));

                zmq_send(socket, response.data(), response.size(), 0);
            }

            zmq_close(socket);
        }).detach();
    }

private:
    void shutdown() {
        running_.store(false);
    }

    void* getOrCreatePubSocket() {
        if (!pub_socket_) {
            pub_socket_ = zmq_socket(context_, ZMQ_PUB);
            zmq_bind(pub_socket_, "tcp://*:5555");
        }
        return pub_socket_;
    }

private:
    void* context_ = nullptr;
    void* pub_socket_ = nullptr;
    std::atomic<bool> running_{true};
};
