cmake_minimum_required(VERSION 3.15) # CMake 최소 버전
project(stomp_cpp_client) # 프로젝트 이름

set(CMAKE_CXX_STANDARD 17) # C++17 사용
set(CMAKE_CXX_STANDARD_REQUIRED ON) # C++17 미지원 컴파일러면 에러

if(MSVC)
    add_compile_options(/utf-8 /FS)
endif()

find_package(asio CONFIG REQUIRED) # asio 없으면 빌드 중단
find_package(nlohmann_json CONFIG REQUIRED) # nlohmann 없으면 빌드 중단

set(VCPKG_ROOT "C:/vcpkg" CACHE PATH "Path to vcpkg installation") # VCPKG 설치 경로
set(OPENSSL_ROOT_DIR "${VCPKG_ROOT}/installed/x64-windows") # OpenSSL 찾을 경로 지정
find_package(OpenSSL REQUIRED)  # OpenSSL 없으면 빌드 중단

# 모든 example이 공통으로 컴파일할 cpp 파일 목록
set(LIB_SOURCES
    lib/Session.cpp
    lib/Reconnector.cpp
)

# 모든 example이 공통으로 사용할 헤더 검색 경로
set(COMMON_INCLUDES
    ${CMAKE_SOURCE_DIR}/websocketpp
    ${CMAKE_SOURCE_DIR}/websocketpp/config
    ${CMAKE_SOURCE_DIR}/lib
)

# 모든 example이 공통으로 링크할 라이브러리 목록
set(COMMON_LIBS
    asio::asio
    OpenSSL::SSL
    OpenSSL::Crypto
    nlohmann_json::nlohmann_json
)

# ex01, ex02, ex03, ex04 반복
foreach(EX ex01 ex02 ex03 ex04)
    set(EX_SOURCES     # 각 example의 소스 파일 목록 초기화
        examples/${EX}/main.cpp
        ${LIB_SOURCES}
    )
    # Publisher.cpp가 있으면 컴파일할 파일 목록에 추가
    if(EXISTS ${CMAKE_SOURCE_DIR}/examples/${EX}/Publisher.cpp)
        list(APPEND EX_SOURCES examples/${EX}/Publisher.cpp)
    endif()

    # Subscriber.cpp가 있으면 컴파일할 파일 목록에 추가
    if(EXISTS ${CMAKE_SOURCE_DIR}/examples/${EX}/Subscriber.cpp)
        list(APPEND EX_SOURCES examples/${EX}/Subscriber.cpp)
    endif()

    # 컴파일해서 .exe만들 파일들을 CMake에 등록
    add_executable(${EX} ${EX_SOURCES})
    target_include_directories(${EX} PRIVATE
        ${COMMON_INCLUDES}    # 공통 헤더 경로
        ${CMAKE_SOURCE_DIR}/examples/${EX}  # 각 ex 폴더 헤더 경로
    )
    
    # 공통 라이브러리 링크
    target_link_libraries(${EX} PRIVATE ${COMMON_LIBS})
    if(WIN32)
        target_link_libraries(${EX} PRIVATE ws2_32) # Windows 소켓 라이브러리
    endif()
endforeach()
