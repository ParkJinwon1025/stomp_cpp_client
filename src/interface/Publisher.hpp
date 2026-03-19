#pragma once
#include <thread>
#include <atomic>
#include <mutex>
#include <deque>
#include <condition_variable>
#include <string>

class Session;

class Publisher
{
public:
    Publisher(Session &session);
    ~Publisher();

    void start();
    void stop();

    void enqueue(const std::string &destination, const std::string &body);
    void flush();

private:
    Session &session;

    struct Item
    {
        std::string destination;
        std::string body;
    };

    std::deque<Item> queue;
    std::mutex queueMutex;
    std::condition_variable cv;
    std::atomic<bool> running{false};
    std::thread workerThread;

    void printQueue();
    void workerLoop();
};
