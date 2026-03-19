#include "Publisher.hpp"
#include "Session.hpp"

Publisher::Publisher(Session &session) : session(session) {}

Publisher::~Publisher() { stop(); }

void Publisher::start()
{
    running = true;
    workerThread = std::thread([this]() { workerLoop(); });
}

void Publisher::stop()
{
    running = false;
    cv.notify_all();
    if (workerThread.joinable())
        workerThread.join();
}

void Publisher::enqueue(const std::string &destination, const std::string &body)
{
    {
        std::lock_guard<std::mutex> lock(queueMutex);
        queue.push_back({destination, body});
        LOG("[PUBLISHER] push ->");
        printQueue();
    }
    cv.notify_one();
}

void Publisher::flush()
{
    LOG("[PUBLISHER] Flush -> waking worker");
    cv.notify_one();
}

void Publisher::printQueue()
{
    LOG("[QUEUE STATE] size: " << queue.size());
    int i = 0;
    for (const auto &item : queue)
    {
        LOG("  [" << i++ << "] " << item.destination << " | " << item.body);
    }
}

void Publisher::workerLoop()
{
    while (running)
    {
        std::unique_lock<std::mutex> lock(queueMutex);
        cv.wait(lock, [this]
                { return (!queue.empty() && session.isConnected()) || !running; });

        while (!queue.empty() && running && session.isConnected())
        {
            auto item = queue.front();
            queue.pop_front();
            LOG("[PUBLISHER] pop ->");
            printQueue();
            lock.unlock();

            LOG("[PUBLISHER] Sending -> " << item.destination);
            session.publish(item.destination, item.body);

            lock.lock();
        }
    }
}
