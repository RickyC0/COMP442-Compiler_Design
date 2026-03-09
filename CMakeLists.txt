cmake_minimum_required(VERSION 3.27)
project(CODE)

set(CMAKE_CXX_STANDARD 17)

include_directories(include)

add_executable(A1
        include/AST.h
        include/io.h
        include/ui.h
        include/token.h
        src/io.cpp
        src/ui.cpp
        src/driver.cpp
        src/AST.cpp
        src/token.cpp
        include/my_parser.h
        src/my_parser.cpp)
