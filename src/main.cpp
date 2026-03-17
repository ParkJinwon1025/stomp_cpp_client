#include "StompInterface.hpp"
#include <iostream>
#include <thread>
#include <atomic>
#include <chrono>

int main()
{
    std::atomic<bool> stopRequested{false}; // 종료 신호 플래그 초기화

    const std::string url = "ws://localhost:9030/stomp/websocket"; // 서버 주소

    // StompInterface 생성
    StompInterface stomp(
        {[]()
         { std::cout << "[INFO] server connected.\n"; }, // 연결 시 출력
         []()
         { std::cout << "[INFO] server disconnected.\n"; }}); // 해제 시 출력

    // 1. start() 테스트
    std::cout << "[TEST] start()" << std::endl;
    stomp.start(url);

    // 연결될 때까지 최대 5초 대기
    int waited = 0;
    while (!stomp.isConnected() && waited < 50)
    {
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
        waited++;
    }

    // 2. isConnected() 테스트
    std::cout << "[TEST] isConnected(): " << (stomp.isConnected() ? "true" : "false") << std::endl;

    if (stomp.isConnected())
    {
        // 3. sub() 테스트
        std::cout << "[TEST] sub()" << std::endl;
        stomp.sub("/topic/robot", [&](const std::string &destination, const std::string &body)
                  {
            std::cout << "[received] destination: " << destination << std::endl;
            std::cout << "[received] body: " << body << std::endl;

            // 4. pub() 테스트
            std::cout << "[TEST] pub()" << std::endl;
            stomp.pub("/app/ubisam", "{\"type\":\"response\",\"message\":\"ok\"}"); });

        // pub() 직접 테스트
        std::cout << "[TEST] pub() 직접 호출" << std::endl;
        stomp.pub("/app/ubisam", "{\"type\":\"status\",\"message\":\"hello\"}");

        // 3초 대기 (메시지 수신 확인)
        std::this_thread::sleep_for(std::chrono::seconds(3));
    }

    // 5. stop() 테스트
    std::cout << "[TEST] stop()" << std::endl;
    stomp.stop();

    std::cout << "[TEST] isConnected() after stop: " << (stomp.isConnected() ? "true" : "false") << std::endl;

    return 0;
}
