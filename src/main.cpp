#include "StompInterface.hpp"
#include <iostream>
#include <thread>
#include <atomic>
#include <chrono>

// 재연결 루트
void reconnectLoop(
    StompInterface &stomp,

    // 종료 신호 플래그
    std::atomic<bool> &stopRequested,

    // 재연결 대기 시간
    int reconnectDelaySec = 3)
{
    auto doSubscribe = [&]()
    {
        stomp.sub("/topic/robot", [&](const std::string &destination, const std::string &body)
                  {
            std::cout << "[received] destination: " << destination << std::endl; // 수신 Destination 출력
            std::cout << "[received] body: " << body << std::endl;  // 수신 Body 출력
            stomp.pub("/app/ubisam", "{\"type\":\"response\",\"message\":\"ok\"}"); }); // 응답 전송
    };

    while (!stopRequested) // 종료 신호 올 때까지 반복
    {
        // 연결 시도
        stomp.start();

        // 연결될 때까지 최대 5초 대기
        int waited = 0;
        while (!stomp.isConnected() && !stopRequested && waited < 50)
        {
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
            waited++;
        }

        // 연결 성공
        if (stomp.isConnected())
        {
            doSubscribe(); // 구독 등록

            // 끊길 때까지 대기
            while (stomp.isConnected() && !stopRequested)
                std::this_thread::sleep_for(std::chrono::milliseconds(100));
        }

        // 종료 신호 시 즉시 종료
        if (stopRequested)
            return;

        // 카운트다운
        for (int i = reconnectDelaySec; i > 0; i--)
        {
            if (stopRequested)
                return;
            std::cout << "[reconnecting...] " << i << "s" << std::endl;
            std::this_thread::sleep_for(std::chrono::seconds(1));
        }
    }
}

// 사용자 입력 처리
void inputLoop(
    StompInterface &stomp,

    // 종료 신호 플래그
    std::atomic<bool> &stopRequested)
{
    std::string input;
    std::cout << "enter message (quit to exit)\n"
              << std::endl;

    while (true)
    {
        std::getline(std::cin, input);

        // 빈 입력은 무시
        if (input.empty())
            continue;

        // quit 입력 시 탈출
        if (input == "quit")
        {
            stopRequested = true;
            stomp.stop();
            break;
        }

        if (!stomp.isConnected())
        {
            std::cout << "[warning] not connected.\n";
            continue;
        }

        // 입력 파싱
        std::string body;
        if (!input.empty() && input.front() == '{')
            body = input;
        else
            body = "{\"type\":\"status\",\"message\":\"" + input + "\"}";

        std::cout << "[send] destination: /app/ubisam" << std::endl;
        std::cout << "[send] body: " << body << std::endl;
        stomp.pub("/app/ubisam", body);
    }
}

int main()
{
    // 종료 신호 플래그 초기화
    std::atomic<bool> stopRequested{false};

    StompInterface stomp(
        "ws://localhost:9030/stomp/websocket", // 서버 주소
        {[]()
         { std::cout << "[INFO] server connected.\n"; }, // 연결 시 출력
         []()
         { std::cout << "[INFO] server disconnected.\n"; }}); // 해제 시 출력

    // 재연결 루프를 별도 쓰레드에서 실행
    std::thread reconnectThread([&]()
                                { reconnectLoop(stomp, stopRequested); });

    // 메인 쓰레드에서 사용자 입력 처리
    inputLoop(stomp, stopRequested);

    if (reconnectThread.joinable())
        reconnectThread.join();

    return 0;
}