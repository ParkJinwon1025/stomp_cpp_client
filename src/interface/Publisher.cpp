#include "Publisher.hpp"
#include "Session.hpp"
#include <nlohmann/json.hpp>
#include <string>
#include <iostream>

// 구조체 예시 - cpp 내부에서만 사용
struct ChatMessage
{
    std::string type;
    std::string value;
};
NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE(ChatMessage, type, value)

Publisher::Publisher(Session &session) : session(session) {}

void Publisher::run()
{
    std::string input;
    while (std::getline(std::cin, input))
    {
        if (input == "q")
            break;

        // 예시 1. 문자열로 보내기 → {"payload":"입력값"}
        // session.Send("/app/ubisam", input);

        // 예시 2. 구조체로 보내기 → {"type":"message","message":"입력값"}
        ChatMessage msg{"message", input};
        session.Send("/app/ubisam", msg);
    }
}
