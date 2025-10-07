cmake_minimum_required(VERSION 3.21)
project(ystring)

add_executable(${PROJECT_NAME}
    ystring.h
    ystring.cpp
    test_string.cpp
)

add_executable("test-datetime"
    datetime.h
    datetime.c
    test_datetime.c
)