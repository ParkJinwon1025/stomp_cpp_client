#pragma once
#include <thread>
#include <atomic>
#include <mutex>
#include <deque>
#include <vector>
#include <chrono>
#include <string>
#include <functional>

class Session;

// 메시지 발신을 담당하는 클래스
// - 주기적 자동 전송 (AddPeriodic)
// - 즉시 1회 전송 (Enqueue)
// 하나의 워커 스레드에서 두 가지를 모두 처리
class Publisher
{
public:
    Publisher(Session &session);
    ~Publisher();

    // 주기적 전송 등록 (Start 전에 호출)
    // includeTimestamp=true 시 전송 시점 타임스탬프를 body에 자동 추가
    Publisher &AddPeriodic(const std::string &destination, const std::string &body, int intervalSec, bool includeTimestamp = false);
    // 람다로 body를 동적으로 생성하는 오버로드 (파싱/데이터 수집 로직 주입용)
    Publisher &AddPeriodic(const std::string &destination, std::function<std::string()> bodyFn, int intervalSec);

    void Start(); // 워커 스레드 시작
    void Stop();  // 워커 스레드 종료

    void Enqueue(const std::string &destination, const std::string &body); // 즉시 1회 전송 큐에 추가

private:
    Session &session;

    // 일반 메시지 큐 아이템
    struct Item
    {
        std::string destination;
        std::string body;
    };

    // 주기적 전송 아이템
    struct PeriodicItem
    {
        std::string destination;
        std::function<std::string()> bodyFn;            // 전송 시점에 body 생성 (timestamp 등 동적 값 지원)
        int intervalSec;                                // 전송 주기 (초)
        std::chrono::steady_clock::time_point lastSent; // 마지막 전송 시각
    };

    std::deque<Item> queue; // 발신 큐
    std::mutex queueMutex;  // 큐 접근 뮤텍스

    std::vector<PeriodicItem> periodics; // 주기적 전송 목록

    // 특정 조건이 될 때까지 스레드를 재워두고, 조건이 되면 깨움.
    std::condition_variable cv;       // 큐에 메시지 추가 시 워커 즉시 깨우기용
    std::atomic<bool> running{false}; // 워커 스레드 실행 플래그
    std::thread workerThread;         // 워커 스레드

    void WorkerLoop(); // 워커 스레드 본체: 주기 전송 + 큐 처리
};
