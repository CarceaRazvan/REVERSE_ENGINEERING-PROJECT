cmake_minimum_required(VERSION 3.13)
project(TestYara)

set(CMAKE_CXX_STANDARD 17)

add_executable(hello_world TestYara.cpp)
add_executable(suspicious_admin SuspiciousAdminTool.cpp)