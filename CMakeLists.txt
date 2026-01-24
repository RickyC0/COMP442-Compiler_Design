cmake_minimum_required(VERSION 3.27)
project(A1)

set(CMAKE_CXX_STANDARD 14)

include_directories(include)

add_executable(A1
        include/io.h
        include/token.h
        src/io.cpp
        src/lexdriver.cpp
        src/token.cpp)
