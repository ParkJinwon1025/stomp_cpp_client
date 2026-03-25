cmake_minimum_required(VERSION 3.15)
project(robot_stomp_client)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

if(MSVC)
    add_compile_options(/utf-8 /FS)
endif()

find_package(asio CONFIG REQUIRED)
find_package(nlohmann_json CONFIG REQUIRED)

set(VCPKG_ROOT "C:/vcpkg" CACHE PATH "Path to vcpkg installation")
set(OPENSSL_ROOT_DIR "${VCPKG_ROOT}/installed/x64-windows")
find_package(OpenSSL REQUIRED)

set(LIB_SOURCES
    lib/Session.cpp
    lib/Reconnector.cpp
)

set(COMMON_INCLUDES
    ${CMAKE_SOURCE_DIR}/websocketpp
    ${CMAKE_SOURCE_DIR}/websocketpp/config
    ${CMAKE_SOURCE_DIR}/lib
)

set(COMMON_LIBS
    asio::asio
    OpenSSL::SSL
    OpenSSL::Crypto
    nlohmann_json::nlohmann_json
)

foreach(EX ex01 ex02 ex03 ex04)
    set(EX_SOURCES
        examples/${EX}/main.cpp
        examples/${EX}/Publisher.cpp
        ${LIB_SOURCES}
    )
    if(EXISTS ${CMAKE_SOURCE_DIR}/examples/${EX}/Subscriber.cpp)
        list(APPEND EX_SOURCES examples/${EX}/Subscriber.cpp)
    endif()
    add_executable(${EX} ${EX_SOURCES})
    target_include_directories(${EX} PRIVATE
        ${COMMON_INCLUDES}
        ${CMAKE_SOURCE_DIR}/examples/${EX}
    )
    target_link_libraries(${EX} PRIVATE ${COMMON_LIBS})
    if(WIN32)
        target_link_libraries(${EX} PRIVATE ws2_32)
    endif()
endforeach()
