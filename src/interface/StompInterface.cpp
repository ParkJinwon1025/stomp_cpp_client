#include "StompInterface.hpp"

StompInterface::StompInterface(
    const std::string &url,
    Handlers handlers)
    : handlers(std::move(handlers)),
      core(url,
           {[this]()
            {
                connected = true;
                if (this->handlers.onConnect)
                    this->handlers.onConnect();
            },
            [this]()
            {
                connected = false;
                if (this->handlers.onDisconnect)
                    this->handlers.onDisconnect();
            },
            [this](const std::string &destination, const std::string &body)
            {
                onMessageHandler(destination, body);
            }})
{
}

StompInterface::~StompInterface()
{
    stop();
}

void StompInterface::start()
{
    stopRequested = false;
    core.start();
}

void StompInterface::stop()
{
    if (stopRequested.exchange(true))
        return;
    connected = false;
    core.stop();
}

bool StompInterface::isConnected() const
{
    return connected.load();
}

void StompInterface::pub(const std::string &destination, const std::string &body)
{
    if (!connected)
        return;
    core.pub(destination, body);
}

void StompInterface::sub(const std::string &topic, std::function<void(const std::string &, const std::string &)> callback)
{
    {
        std::lock_guard<std::mutex> lock(subMutex);
        subscriptions.push_back({topic, callback});
    }
    core.sub(topic, "sub-" + std::to_string(subscriptions.size() - 1));
}

void StompInterface::onMessageHandler(const std::string &destination, const std::string &body)
{
    std::lock_guard<std::mutex> lock(subMutex);
    for (const auto &s : subscriptions)
    {
        if (s.topic == destination && s.callback)
        {
            s.callback(destination, body);
            break;
        }
    }
}