cmake_minimum_required(VERSION 3.13)
project(TestYara)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_FLAGS_DEBUG "/Od /Oy-")

#add_executable(hello_world TestYara.cpp)
#add_executable(suspicious_admin SuspiciousAdminTool.cpp)
#add_executable(encoded_string Encoded_String.cpp)
add_executable(example_program ExampleProgram.cpp)