cmake_minimum_required(VERSION 3.15)
project(SecureChatServer)

set(CMAKE_CXX_STANDARD 17)

find_package(Boost REQUIRED COMPONENTS system thread)
find_package(SQLite3 REQUIRED)
find_package(Threads REQUIRED)
find_package(OpenSSL REQUIRED)

include(FetchContent)
FetchContent_Declare(json URL https://github.com/nlohmann/json/releases/download/v3.11.2/json.tar.xz)
FetchContent_MakeAvailable(json)

include_directories(include ${SQLite3_INCLUDE_DIRS} ${OPENSSL_INCLUDE_DIR})

file(GLOB SOURCES "src/*.cpp")

add_executable(server main.cpp ${SOURCES})
target_link_libraries(server Boost::system Boost::thread ${SQLite3_LIBRARIES} Threads::Threads nlohmann_json::nlohmann_json OpenSSL::SSL OpenSSL::Crypto)