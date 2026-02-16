cmake_minimum_required(VERSION 3.27)
project(CODE)

set(CMAKE_CXX_STANDARD 17)

include_directories(include)

add_executable(A1
        include/io.h
        include/token.h
        src/io.cpp
        src/driver.cpp
        src/token.cpp
        include/my_parser.h
        src/my_parser.cpp)
