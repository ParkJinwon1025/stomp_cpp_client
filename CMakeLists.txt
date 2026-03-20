cmake_minimum_required(VERSION 3.15)
project(robot_stomp_client)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

if(MSVC)
    add_compile_options(/utf-8 /FS)
endif()

find_package(asio CONFIG REQUIRED)
find_package(nlohmann_json CONFIG REQUIRED)

set(OPENSSL_ROOT_DIR "C:/vcpkg/installed/x64-windows")
set(OPENSSL_INCLUDE_DIR "C:/vcpkg/installed/x64-windows/include")
set(OPENSSL_CRYPTO_LIBRARY "C:/vcpkg/installed/x64-windows/lib/libcrypto.lib")
set(OPENSSL_SSL_LIBRARY "C:/vcpkg/installed/x64-windows/lib/libssl.lib")
find_package(OpenSSL REQUIRED)

include_directories(${CMAKE_SOURCE_DIR}/websocketpp)
include_directories(${CMAKE_SOURCE_DIR}/websocketpp/config)
include_directories(${CMAKE_SOURCE_DIR}/src/core)
include_directories(${CMAKE_SOURCE_DIR}/src/Publisher2)

add_executable(robot_stomp_client
    src/main.cpp
    src/core/Session.cpp
    src/core/Subscriber.cpp
    src/core/Reconnector.cpp
    src/Publisher2/Publisher.cpp
)

target_link_libraries(robot_stomp_client
    PRIVATE
    asio::asio
    OpenSSL::SSL
    OpenSSL::Crypto
    nlohmann_json::nlohmann_json
)

if(WIN32)
    target_link_libraries(robot_stomp_client PRIVATE ws2_32)
endif()