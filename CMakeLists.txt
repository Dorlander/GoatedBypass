cmake_minimum_required(VERSION 3.20)
project(GoatedBypass CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Dependencies via vcpkg
find_package(Boost REQUIRED COMPONENTS system)
find_package(OpenSSL REQUIRED)
find_package(ZLIB REQUIRED)
find_package(nlohmann_json CONFIG REQUIRED)

set(SOURCES
    GoatedBypass/main.cpp
    GoatedBypass/ConfigProxy.cpp
    GoatedBypass/HttpHelpers.cpp
    GoatedBypass/JsonPatcher.cpp
    GoatedBypass/Logger.cpp
    GoatedBypass/TokenCapture.cpp
    GoatedBypass/RiotUtils.cpp
)

add_executable(GoatedBypass ${SOURCES})

target_compile_definitions(GoatedBypass PRIVATE
    _WIN32_WINNT=0x0601
    WIN32_LEAN_AND_MEAN
    NOMINMAX
    UNICODE
    _UNICODE
)

target_link_libraries(GoatedBypass PRIVATE
    Boost::system
    OpenSSL::SSL
    OpenSSL::Crypto
    nlohmann_json::nlohmann_json
    ZLIB::ZLIB
    Crypt32
    ws2_32
    winhttp
)