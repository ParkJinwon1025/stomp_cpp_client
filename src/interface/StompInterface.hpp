#pragma once
#include "StompCore.hpp"
#include <mutex>
#include <atomic>
#include <vector>
#include <functional>
#include <string>

class StompInterface
{
public:
    struct Handlers
    {
        std::function<void()> onConnect;
        std::function<void()> onDisconnect;
    };

    static StompInterface &getInstance()
    {
        static StompInterface instance;
        return instance;
    }

    StompInterface(const StompInterface &) = delete;
    StompInterface &operator=(const StompInterface &) = delete;

    void init(Handlers handlers);

    void Connection_Start(const std::string &url);
    void Connection_End();
    bool Connection_IsAlive() const;

    void Message_Publish(const std::string &destination, const std::string &body);
    void Message_Subscribe(const std::string &topic, std::function<void(const std::string &, const std::string &)> callback = nullptr);

private:
    StompInterface();
    ~StompInterface();

    Handlers handlers;
    StompCore core;
    std::string host;
    std::atomic<bool> stopRequested{false};

    struct Subscription
    {
        std::string topic;
        std::function<void(const std::string &, const std::string &)> callback;
    };
    std::vector<Subscription> subscriptions;
    std::mutex subMutex;

    std::string Frame_BuildConnect(const std::string &host) const;
    std::string Frame_BuildPub(const std::string &destination, const std::string &body) const;
    std::string Frame_BuildSub(const std::string &topic, const std::string &subId) const;

    void Message_Parse(const std::string &payload);
    void Message_Route(const std::string &destination, const std::string &body);
};