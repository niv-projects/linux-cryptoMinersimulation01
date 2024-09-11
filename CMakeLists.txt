cmake_minimum_required(VERSION 3.22)
project(miners)

set(CMAKE_CXX_STANDARD 17)
find_package(Threads REQUIRED)
find_package(ZLIB REQUIRED)

add_executable(miners.out main.cpp)
target_link_libraries(miners.out ZLIB::ZLIB Threads::Threads)
