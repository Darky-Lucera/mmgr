cmake_minimum_required(VERSION 3.10.0)
project(mmgr)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

add_library(mmgr
    src/mmgr.cpp
    src/mmgr.h
    src/nommgr.h
    src/nomalloc.h
    src/nonew.h
)

add_executable(test
    test/test.cpp
)
target_link_libraries(test mmgr)
target_include_directories(test PRIVATE src)

add_executable(testC
    test/testC.c
)
target_link_libraries(testC mmgr)
target_include_directories(testC PRIVATE src)

if(MSVC)
    add_compile_definitions(_CRT_SECURE_NO_WARNINGS)
    if(MSVC_VERSION GREATER_EQUAL 1900)
        # SDL checks 2015+
        add_compile_options(/sdl)
    endif()
else()
    add_compile_options(-Wall -Wextra)
endif()
