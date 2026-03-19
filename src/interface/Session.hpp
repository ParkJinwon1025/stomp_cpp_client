#pragma once
#include "../core/StompCore.hpp"
#include "Subscriber.hpp"
#include <mutex>
#include <atomic>
#include <vector>
#include <functional>
#include <string>
#include <memory>
#include <iostream>

// 멀티스레드 환경에서 안전하게 콘솔 출력하기 위한 뮤텍스
// inline : 헤더 파일에서 전역 변수 선언 시 중복 정의 방지
//        : 모든 파일에서 하나의 객체를 공유하도록 허용
inline std::mutex coutMutex;
// 뮤텍스로 보호된 로그 출력 매크로
// lock_guard가 뮤텍스를 잠구고 코드 블록이 끝나면 자동으로 잠금 해제
#define LOG(msg)                                          \
    {                                                     \
        std::lock_guard<std::mutex> _log_lock(coutMutex); \
        std::cout << msg << std::endl;                    \
    }

// WebSocket + STOMP 연결을 관리하는 클래스
// Publisher, Reconnector, Subscriber가 이 Session을 통해 통신
class Session
{
public:
    Session();  // 생성자: 객체 초기화, 내부 멤버 초기화
    ~Session(); // 소멸자: 연결 종료, 자원 해제

    void Init(const std::string &url); // 서버 URL 설정
    void Connect();                    // WebSocket 연결 시작
    void Disconnect();                 // 연결 종료
    bool IsConnected() const;          // 현재 연결 상태 반환(const : Session 객체의 상태를 읽기만 하고 수정하지 않는 함수)

    void Publish(const std::string &destination, const std::string &body); // STOMP SEND 프레임 전송
    void Subscribe(const std::string &topic, Subscriber *subscriber);      // 토픽 구독 등록

private:
    StompCore core;   // WebSocket 실제 연결 담당 코어
    std::string url;  // 서버 URL
    std::string host; // 서버 호스트명

    // 단일 변수의 읽기/쓰기 보장
    std::atomic<bool> stopRequested{false}; // 종료 요청 플래그 (중복 종료 방지)
    std::atomic<bool> stompReady{false};    // STOMP CONNECTED 수신 후 true → 큐 처리 시작

    // asio : 이벤트 루프 객체
    asio::io_service *ioService{nullptr}; // ASIO 이벤트 루프 (post에 사용)

    // 등록된 구독 정보 (토픽 + 콜백 객체)
    struct Subscription
    {
        std::string topic;
        Subscriber *subscriber;
    };

    // vector : 동적 배열 클래스
    std::vector<Subscription> subscriptions; // 구독 목록
    std::mutex subMutex;                     // 구독 목록 접근 뮤텍스

    void Post(std::function<void()> task); // ASIO 스레드에 작업 위임 (순서 보장)
    void ReRegisterSubscriptions();        // 재연결 시 구독 목록 재등록

    // STOMP 프레임 빌더
    std::string BuildConnectFrame() const;
    std::string BuildSendFrame(const std::string &dest, const std::string &body) const;
    std::string BuildSubscribeFrame(const std::string &topic, const std::string &id) const;

    // 수신 처리
    void ParseMessage(const std::string &payload);                              // 수신 프레임 파싱
    void RouteMessage(const std::string &destination, const std::string &body); // 해당 토픽 Subscriber에 전달
};
