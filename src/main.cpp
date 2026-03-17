#include "StompInterface.hpp"
#include <iostream>
#include <thread>
#include <chrono>

// 초기화 및 연결
void InitAndConnect(StompInterface &stomp, const std::string &url)
{
    stomp.init(
        {[]()
         { std::cout << "[INFO] server connected.\n"; },
         []()
         { std::cout << "[INFO] server disconnected.\n"; }});

    std::cout << "[TEST] Connection_Start()" << std::endl;
    stomp.Connection_Start(url);

    // 연결될 때까지 최대 5초 대기
    int waited = 0;
    while (!stomp.Connection_IsAlive() && waited < 50)
    {
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
        waited++;
    }

    std::cout << "[TEST] Connection_IsAlive(): "
              << (stomp.Connection_IsAlive() ? "true" : "false") << std::endl;
}

// 구독 및 발행 테스트
void RunPubSubTest(StompInterface &stomp)
{
    std::cout << "[TEST] Message_Subscribe()" << std::endl;
    stomp.Message_Subscribe("/topic/robot", [&](const std::string &destination, const std::string &body)
                            {
        std::cout << "[received] destination: " << destination << std::endl;
        std::cout << "[received] body: " << body << std::endl;

        std::cout << "[TEST] Message_Publish() in callback" << std::endl;
        stomp.Message_Publish("/app/ubisam", "{\"type\":\"response\",\"message\":\"ok\"}"); });

    std::cout << "[TEST] Message_Publish() 직접 호출" << std::endl;
    stomp.Message_Publish("/app/ubisam", "{\"type\":\"status\",\"message\":\"hello\"}");

    // 메시지 수신 대기
    std::this_thread::sleep_for(std::chrono::seconds(3));
}

// 종료
void Shutdown(StompInterface &stomp)
{
    std::cout << "[TEST] Connection_End()" << std::endl;
    stomp.Connection_End();

    std::cout << "[TEST] Connection_IsAlive() after end: "
              << (stomp.Connection_IsAlive() ? "true" : "false") << std::endl;
}

int main()
{
    const std::string url = "ws://localhost:9030/stomp/websocket";

    // & : 참조
    // auto : 컴파일러가 타입을 자동으로 추론
    // const : 값을 변경할 수 없도록 고정하는 키워드
    auto &stomp = StompInterface::getInstance();

    InitAndConnect(stomp, url);

    if (stomp.Connection_IsAlive())
        RunPubSubTest(stomp);

    Shutdown(stomp);

    return 0;
}
