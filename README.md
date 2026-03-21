# stomp_cpp_client

C++ STOMP over WebSocket 클라이언트 라이브러리

## 요구사항

- Visual Studio 2019 이상 (C++ 개발 워크로드 포함)
- CMake 3.15 이상
- vcpkg
- websocketpp (프로젝트 내 포함)

## 환경 설정 (처음 설치하는 경우)

### 1. Visual Studio 설치

[Visual Studio 다운로드](https://visualstudio.microsoft.com/ko/)에서 설치 시 **C++를 사용한 데스크톱 개발** 워크로드를 선택하세요.

### 2. CMake 설치

[CMake 다운로드](https://cmake.org/download/)에서 Windows 설치파일을 받아서 설치하세요.

> 설치 시 **"Add CMake to the system PATH"** 옵션을 선택하세요.

### 3. vcpkg 설치

```bash
git clone https://github.com/microsoft/vcpkg.git C:/vcpkg
cd C:/vcpkg
bootstrap-vcpkg.bat
```

### 4. 패키지 설치

```bash
C:/vcpkg/vcpkg.exe install asio:x64-windows
C:/vcpkg/vcpkg.exe install nlohmann-json:x64-windows
C:/vcpkg/vcpkg.exe install openssl:x64-windows
```

## 빌드

```bash
git clone <repo-url>
cd stomp_cpp_client
mkdir build && cd build
cmake .. -DCMAKE_TOOLCHAIN_FILE=<vcpkg경로>/scripts/buildsystems/vcpkg.cmake -DVCPKG_ROOT=<vcpkg경로>
cmake --build .
```

> **예시** (vcpkg가 `C:/vcpkg`에 설치된 경우 — 기본값이라 생략 가능):
> ```bash
> cmake .. -DCMAKE_TOOLCHAIN_FILE=C:/vcpkg/scripts/buildsystems/vcpkg.cmake
> ```

> **참고**: 빌드 후 `'pwsh.exe'은(는) 내부 또는 외부 명령...` 경고가 뜰 수 있어요. PowerShell 7이 없을 때 나오는 경고로 빌드/실행에는 영향 없어요. PowerShell 터미널에서 실행하거나 [PowerShell 7](https://aka.ms/pscore6)을 설치하면 사라져요.

## 실행

```bash
# Windows
Debug\robot_stomp_client.exe
```

## Publisher 선택

`CMakeLists.txt`에서 사용할 Publisher 폴더를 지정:

```cmake
include_directories(${CMAKE_SOURCE_DIR}/src/Publisher3)  # 또는 Publisher2, Publisher4

add_executable(robot_stomp_client
    ...
    src/Publisher3/Publisher.cpp  # 사용할 Publisher로 교체
)
```

| Publisher | 방식 |
|---|---|
| Publisher1 | 키보드 입력 → 문자열 직접 Send |
| Publisher2 | 1초마다 timestamp → 문자열 Send |
| Publisher3 | 1초마다 → nlohmann::json 객체 → Send |
| Publisher4 | 1초마다 → struct → Send |

## 서버 URL 변경

`src/main.cpp`에서 수정:

```cpp
Session session("ws://localhost:9030/stomp/websocket");
```
