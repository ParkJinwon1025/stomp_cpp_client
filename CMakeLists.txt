cmake_minimum_required(VERSION 3.15)
project(robot_stomp_client)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

if(MSVC)
    add_compile_options(/utf-8 /FS)
endif()

find_package(asio CONFIG REQUIRED)

include_directories(${CMAKE_SOURCE_DIR}/websocketpp)
include_directories(${CMAKE_SOURCE_DIR}/src/core)
include_directories(${CMAKE_SOURCE_DIR}/src/interface)

add_executable(robot_stomp_client
    src/main.cpp
    src/core/StompCore.cpp
    src/interface/Session.cpp
    src/interface/Publisher.cpp
    src/interface/Reconnector.cpp
)

target_link_libraries(robot_stomp_client
    PRIVATE
    asio::asio
)

if(WIN32)
    target_link_libraries(robot_stomp_client PRIVATE ws2_32)
endif()