cmake_minimum_required(VERSION 3.10)
project(test_sdk)

set(CMAKE_CXX_STANDARD 17)

find_package(cvedix REQUIRED)

add_executable(test_sdk main.cpp)
target_link_libraries(test_sdk PRIVATE cvedix::cvedix_instance_sdk)
