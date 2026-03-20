#include "Publisher.hpp"
#include "Session.hpp"
#include <nlohmann/json.hpp>
#include <string>
#include <iostream>

void Publisher::HandleStarted(Session &session)
{
    std::string message;

    while (true)
    {
        std::cout << "message (q to quit): " << std::flush;
        std::getline(std::cin, message);
        if (message == "q")
            break;

        session.Send("/app/ubisam", message);
    }
}
