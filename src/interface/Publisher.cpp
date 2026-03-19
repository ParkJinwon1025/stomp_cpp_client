#include "Publisher.hpp"
#include "Session.hpp"
#include <chrono>

Publisher::Publisher(Session &session) : session(session) {}

Publisher::~Publisher() { Stop(); } // 소멸 시 자동 종료

Publisher &Publisher::AddPeriodic(const std::string &destination, const std::string &body, int intervalSec, bool includeTimestamp)
{
    std::function<std::string()> bodyFn;

    if (includeTimestamp)
    {
        // 전송 시점마다 현재 타임스탬프를 body 마지막 } 앞에 삽입
        bodyFn = [body]()
        {
            auto ts = std::chrono::duration_cast<std::chrono::milliseconds>(
                          std::chrono::system_clock::now().time_since_epoch())
                          .count();
            std::string b = body;
            auto pos = b.rfind('}');
            if (pos != std::string::npos)
                b.insert(pos, ",\"timestamp\":" + std::to_string(ts));
            return b;
        };
    }
    else
    {
        // 고정 body는 그대로 반환
        bodyFn = [body]()
        { return body; };
    }

    // 주기 전송 목록에 추가, lastSent를 now로 초기화해서 등록 즉시 intervalSec 후 첫 전송
    periodics.push_back({destination, bodyFn, intervalSec, std::chrono::steady_clock::now()});
    return *this; // 체이닝 지원
}

Publisher &Publisher::AddPeriodic(const std::string &destination, std::function<std::string()> bodyFn, int intervalSec)
{
    // 람다를 그대로 저장 (파싱/데이터 수집 로직을 외부에서 주입)
    periodics.push_back({destination, bodyFn, intervalSec, std::chrono::steady_clock::now()});
    return *this;
}

void Publisher::Start()
{
    running = true;
    workerThread = std::thread([this]()
                               { WorkerLoop(); });
}

void Publisher::Stop()
{
    running = false;
    cv.notify_one(); // 대기 중인 워커 즉시 깨워서 종료
    if (workerThread.joinable())
        workerThread.join(); // 워커 스레드 종료 대기
}

void Publisher::Enqueue(const std::string &destination, const std::string &body)
{
    {
        std::lock_guard<std::mutex> lock(queueMutex);
        queue.push_back({destination, body}); // 큐에 추가

        // 큐 상태 출력
        LOG("[QUEUE] push | size: " << queue.size());
        int i = 0;
        for (const auto &item : queue)
            LOG("  [" << i++ << "] " << item.destination << " | " << item.body);
    }
    cv.notify_one(); // 워커 스레드 즉시 깨움
}

void Publisher::WorkerLoop()
{
    bool wasConnected = false;
    bool everConnected = false;
    while (running)
    {
        bool isConnected = session.IsConnected();

        // 재연결 감지 시 서버 세션 안정화 대기 후 큐 상태 출력
        if (isConnected && !wasConnected)
        {
            if (everConnected) // 첫 연결은 제외, 재연결 시에만
            {
                std::this_thread::sleep_for(std::chrono::milliseconds(500)); // 서버 STOMP 세션 준비 대기
                std::lock_guard<std::mutex> lock(queueMutex);
                LOG("[QUEUE] reconnected | pending: " << queue.size());
                for (size_t i = 0; i < queue.size(); i++)
                    LOG("  [" << i << "] " << queue[i].destination << " | " << queue[i].body);
            }
            everConnected = true;
        }
        wasConnected = isConnected;

        if (isConnected)
        {
            auto now = std::chrono::steady_clock::now();

            // 주기적 전송: 각 항목의 경과 시간 체크 후 전송
            for (auto &p : periodics)
            {
                if (now - p.lastSent >= std::chrono::seconds(p.intervalSec))
                {
                    std::string body = p.bodyFn();
                    LOG("[PERIODIC] -> " << p.destination << " | " << body);
                    session.Publish(p.destination, body);
                    p.lastSent = now;
                }
            }

            // 큐 처리: 1개씩 전송, 전송 간 10ms 간격으로 순서 보장
            {
                Item item;
                bool hasItem = false;
                {
                    std::lock_guard<std::mutex> lock(queueMutex);
                    if (!queue.empty())
                    {
                        item = queue.front();
                        queue.pop_front();
                        hasItem = true;
                        LOG("[QUEUE] pop -> " << item.body << " | remaining: " << queue.size());
                    }
                }
                if (hasItem)
                {
                    session.Publish(item.destination, item.body);
                    std::this_thread::sleep_for(std::chrono::milliseconds(10)); // 전송 간격
                }
            }
        }

        {
            std::unique_lock<std::mutex> lock(queueMutex);
            if (queue.empty())
                cv.wait_for(lock, std::chrono::milliseconds(100)); // 큐 비었을 때만 대기
        }
    }
}
