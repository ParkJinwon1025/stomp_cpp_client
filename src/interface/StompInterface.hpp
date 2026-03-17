#pragma once
#include "StompCore.hpp"
#include <mutex>
#include <atomic>
#include <vector>

// 실제 동작부 + 호출부  코어부를 가져다 쓸 때 종속성이 x => 유연성 증가를 위함임.
// 헝가리안 표기법

// pub/ sub 큐 추가
// pub : 의도하지 않은 스케줄링 타임에 처리될 수 있음. 큐 필요 / 즉시 발행 큐잉 보완
// sub : 스케줄러가 로직의 상태를 보고 받을 수 있을 때 던짐.  큐 필요

// 이넡페이스 자체를 싱글톤
class StompInterface
{
public:
    struct Handlers
    {
        std::function<void()> onConnect;
        std::function<void()> onDisconnect;
    };

    StompInterface(Handlers handlers = {}); // url 제거
    ~StompInterface();

    StompInterface(const StompInterface &) = delete;
    StompInterface &operator=(const StompInterface &) = delete;

    // Core에 시작 명령
    void start(const std::string &url);

    // Core에 종료 명령
    void stop();

    // Core에 연결 상태 확인 위임
    bool isConnected() const;

    // 연결 확인 후 Core에 전송 명령
    void pub(const std::string &destination, const std::string &body);

    // 구독 목록 저장 후 Core에 구독 명령
    void sub(const std::string &topic, std::function<void(const std::string &, const std::string &)> callback = nullptr);

private:
    Handlers handlers;
    StompCore core;

    std::atomic<bool> stopRequested{false};

    struct Subscription
    {
        std::string topic;
        std::function<void(const std::string &, const std::string &)> callback;
    };
    std::vector<Subscription> subscriptions;
    std::mutex subMutex;

    // 수신 메시지를 구독 목록에서 찾아 콜백 실행
    void onMessageHandler(const std::string &destination, const std::string &body);
};