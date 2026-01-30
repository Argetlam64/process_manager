cmake_minimum_required(VERSION 3.30)
project(process_manager)

set(CMAKE_CXX_STANDARD 26)
find_package(Curses REQUIRED)
include_directories(${CURSES_INCLUDE_DIR})

add_executable(process_manager main.cpp)
target_link_libraries(process_manager ${CURSES_LIBRARIES})