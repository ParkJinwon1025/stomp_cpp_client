#include "StompInterface.hpp"
#include <iostream>
#include <thread>
#include <atomic>
#include <chrono>

// 구독 등록
void doSubscribe(StompInterface &stomp)
{
    stomp.sub("/topic/robot", [&](const std::string &destination, const std::string &body)
              {
                  std::cout << "[received] destination: " << destination << std::endl;    // 수신 destination 출력
                  std::cout << "[received] body: " << body << std::endl;                  // 수신 body 출력
                  stomp.pub("/app/ubisam", "{\"type\":\"response\",\"message\":\"ok\"}"); // 응답 전송
              });
}

// 재연결 루프
void reconnectLoop(
    StompInterface &stomp,
    std::atomic<bool> &stopRequested, // 종료 신호 플래그
    int reconnectDelaySec = 3)        // 재연결 대기 시간
{
    while (!stopRequested) // 종료 신호 올 때까지 반복
    {
        stomp.start(); // 연결 시도

        // 연결될 때까지 최대 5초 대기
        int waited = 0;
        while (!stomp.isConnected() && !stopRequested && waited < 50)
        {
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
            waited++;
        }

        if (stomp.isConnected()) // 연결 성공 시
        {
            doSubscribe(stomp); // 구독 등록

            // 끊길 때까지 대기
            while (stomp.isConnected() && !stopRequested)
                std::this_thread::sleep_for(std::chrono::milliseconds(100));
        }

        if (stopRequested) // 종료 신호 시 즉시 종료
            return;

        // 재연결 카운트다운
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
    std::atomic<bool> &stopRequested) // 종료 신호 플래그
{
    std::string input;
    std::cout << "enter message (quit to exit)\n"
              << std::endl;

    while (true)
    {
        std::getline(std::cin, input);

        if (input.empty()) // 빈 입력 무시
            continue;

        if (input == "quit") // quit 입력 시 종료
        {
            stopRequested = true;
            stomp.stop();
            break;
        }

        if (!stomp.isConnected()) // 연결 안 됐으면 경고
        {
            std::cout << "[warning] not connected.\n";
            continue;
        }

        // { 로 시작하면 JSON 그대로, 아니면 JSON으로 감싸기
        std::string body;
        if (!input.empty() && input.front() == '{')
            body = input;
        else
            body = "{\"type\":\"status\",\"message\":\"" + input + "\"}";

        std::cout << "[send] destination: /app/ubisam" << std::endl;
        std::cout << "[send] body: " << body << std::endl;
        stomp.pub("/app/ubisam", body); // 전송
    }
}

int main()
{
    std::atomic<bool> stopRequested{false}; // 종료 신호 플래그 초기화

    StompInterface stomp(
        "ws://localhost:9030/stomp/websocket", // 서버 주소
        {[]()
         { std::cout << "[INFO] server connected.\n"; }, // 연결 시 출력
         []()
         { std::cout << "[INFO] server disconnected.\n"; }}); // 해제 시 출력

    // 재연결 루프를 별도 스레드에서 실행
    std::thread reconnectThread([&]()
                                { reconnectLoop(stomp, stopRequested); });

    // 메인 스레드에서 사용자 입력 처리
    inputLoop(stomp, stopRequested);

    // reconnectThread 종료될 때까지 대기 후 프로그램 종료
    if (reconnectThread.joinable())
        reconnectThread.join();

    return 0;
}