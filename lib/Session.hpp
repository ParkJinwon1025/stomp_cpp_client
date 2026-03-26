#pragma once

#include <nlohmann/json.hpp>
#include <string>
#include <map>
#include <functional>
#include <memory>
#include <mutex>
#include <iostream>

// 라이브러리 종류

// 1. BootStomp
// - 마지막 업데이트 : 4년전
// - WebSocket 미지원 / TCP 직접 연결만 지원

// 현실 => WebSocket 라이브러리 + STOMP 프레임 직접 구현
// C++ : 임베디드/로봇 분야에서는 STOMP 보다 다른 프로토콜을 많이 씀.

// 멀티스레드 환경에서 안전하게 콘솔 출력하기 위한 뮤텍스
// coutMutex : 뮤텍스를 잡아야만 std::cout로 출력 가능
// #define : 매크로
// cout : C++의 콘솔 출력 객체
inline std::mutex coutMutex;
#define LOG(msg)                                          \
    {                                                     \
        std::lock_guard<std::mutex> _log_lock(coutMutex); \
        std::cout << msg << std::endl;                    \
    }

#include "Publisher.hpp"
#include "Subscriber.hpp"

class Session
{
public:
    Session(const std::string &url);
    ~Session();

    // 복사 생성자 : 객체를 만들 때 호출
    Session(const Session &) = delete;

    // 복사 대입 연산자 = 이미 존재하는 객체에 덮어씌울 때 호출
    Session &operator=(const Session &) = delete;

    void Connect()
    {
        ConnectImpl([this]()
                    {
            for (auto &[topic, subscriber] : subscribers_)
                SubscribeImpl(topic, subscriber); });
    }
    void Disconnect();
    bool IsConnected() const;

    // Publisher 등록
    void Publish(const std::string &name, Publisher *publisher)
    {
        if (publishers_.count(name))
        {
            LOG("[SESSION] Publisher already exists: " << name);
            return;
        }
        publishers_[name] = publisher;
        LOG("[SESSION] Publisher started: " << name);
        publisher->HandleStarted(*this);
    }

    // 단일 JSON Publish
    // 전송 방식에 대한 단일 책임
    // 구조체는 사용자가 알아서 JSON으로 변환하고 보내도록 함.
    void Publish(const std::string &destination, const nlohmann::json &j)
    {
        PublishImpl(destination, j.dump());
    }

    // Subscriber 등록
    void Subscribe(const std::string &topic, Subscriber *subscriber)
    {
        subscribers_[topic] = subscriber; // Map 에 저장
        SubscribeImpl(topic, subscriber);
    }

    // 1. 입력하면 보내기
    // 2.timestamp 계쏙 보내기

    /////////////////////////////////////////////////////////////////////
    //  Send                                                           //
    /////////////////////////////////////////////////////////////////////
    // 1. 문자열로 Send
    // void Send(const std::string &destination, const std::string payload);

    // 라이브러리가 바뀌어도 Send 영향을 받지 않음.
    // 2. json 객체로 Send
    // {
    //     LOG("[SESSION] Send(json)   -> " << destination);
    //     Send(destination, j.dump());
    // }

    // 3. 구조체로 Send
    // template <typename T>
    // void Send(const std::string &destination, const T &data)
    // {
    //     nlohmann::json j = data;
    //     Send(destination, j);
    // }

private:
    std::string url;
    std::string host;

    std::map<std::string, Publisher *> publishers_;
    std::map<std::string, Subscriber *> subscribers_;

    // url, host를 제외한 나머지 멤버는 Impl 안에
    struct Impl; // Impl 구조체 선언

    // Impl 객체를 힙에 할당해서 포인터로 보유
    // unique_ptr : Session이 소멸될 때 자동으로 Impl도 같이 소멸
    // Pimpl — websocketpp 등 구현 세부사항을 cpp로 숨김
    std::unique_ptr<Impl> impl_;

    // 외부에서 직접 호출할 필요가 없기에 private
    void PublishImpl(const std::string &destination, const std::string &payload);
    void SubscribeImpl(const std::string &topic, Subscriber *subscriber);
    void ConnectImpl(std::function<void()> callback);
};
