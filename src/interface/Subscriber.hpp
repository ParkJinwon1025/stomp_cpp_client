#pragma once // 헤더 파일 중복 포함 방지
#include <string>
#include <functional>

class Session; // 순환 참조 방지를 위한 전방 선언

// 메시지 수신 시 실행할 콜백을 보관하는 클래스
class Subscriber
{
public:
    // 생성 시 콜백 함수를 받아 저장
    Subscriber(std::function<void(const std::string &, Session &)> cb) : callback(cb) {}

    // Session이 메시지 수신 시 호출 → 저장된 콜백 실행
    void Received(const std::string &body, Session &session) { callback(body, session); }

private:
    std::function<void(const std::string &, Session &)> callback; // 수신 시 실행할 콜백
};
