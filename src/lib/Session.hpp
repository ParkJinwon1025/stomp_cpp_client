#pragma once

#include <nlohmann/json.hpp>
#include <string>
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
inline std::mutex coutMutex;
#define LOG(msg)                                          \
    {                                                     \
        std::lock_guard<std::mutex> _log_lock(coutMutex); \
        std::cout << msg << std::endl;                    \
    }

class Publisher;
class Subscriber;

class Session
{
public:
    Session(const std::string &url);
    ~Session();

    Session(const Session &) = delete;
    Session &operator=(const Session &) = delete;

    void Connect();
    void Disconnect();
    bool IsConnected() const;
    // void Publish(const std::string &name, Publisher *publisher);
    void Publish(const std::string &destination, const nlohmann::json &j);
    void Subscribe(const std::string &topic, Subscriber *subscriber);

    /////////////////////////////////////////////////////////////////////
    //  Send                                                           //
    /////////////////////////////////////////////////////////////////////
    // 1. 문자열로 Send
    // void Send(const std::string &destination, const std::string payload);

    // 라이브러리가 바뀌어도 Send 영향을 받지 않음.
    // 2. json 객체로 Send
    void Publish(const std::string &destination, const nlohmann::json &j);
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

    // Pimpl — websocketpp 등 구현 세부사항을 cpp로 숨김
    // url, host를 제외한 나머지 멤버는 Impl 안에
    struct Impl;
    std::unique_ptr<Impl> impl_;
};
