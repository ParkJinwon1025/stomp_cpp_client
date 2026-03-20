#include "Publisher.hpp"
#include "Session.hpp"
#include <nlohmann/json.hpp>
#include <string>
#include <iostream>

void Publisher::HandleStarted(Session &session)
{
    std::string message;

    // 무한 루프
    while (true)
    {
        // 텍스트 출력
        std::cout << "message (q to quit): " << std::flush;

        // 사용자가 엔터를 누를 때까지 키보드 입력을 기다려서 message 변수에 저장
        // 블로킹(입력이 올 때까지 여기서 멈춤)
        // cin : 키보드 입력 스트림(키보드에서 입력을 읽어오는 대상)
        // message : 읽은 내용을 저장할 변수
        std::getline(std::cin, message);

        // 입력이 q면 루프 탈출
        if (message == "q")
            break;

        session.Send("/app/ubisam", "{\"payload\":\"" + message + "\"}");
    }
}
