cmake_minimum_required(VERSION 3.16)
project(mmgr VERSION 1.0.1 LANGUAGES C CXX)

# Options
#--------------------------------------
# A project that adds this one with add_subdirectory or FetchContent only builds the library, unless it asks for more.
# PROJECT_IS_TOP_LEVEL would do the same, but it needs CMake 3.21.
if (CMAKE_SOURCE_DIR STREQUAL CMAKE_CURRENT_SOURCE_DIR)
    set(MMGR_IS_TOP_LEVEL ON)
else()
    set(MMGR_IS_TOP_LEVEL OFF)
endif()

option(MMGR_BUILD_TESTS     "Build the test programs"                                       ${MMGR_IS_TOP_LEVEL})
option(MMGR_NO_DELETE_MACRO "Do not redefine delete, so that '= delete' works after mmgr.h" OFF)

# DJGPP headers expose key DOS APIs only in GNU (non-strict ANSI) mode.
if (DJGPP)
    set(MMGR_C_EXTENSIONS   ON)
    set(MMGR_CXX_EXTENSIONS ON)
else()
    set(MMGR_C_EXTENSIONS   OFF)
    set(MMGR_CXX_EXTENSIONS OFF)
endif()

# Standard, language extensions, warnings and MSVC definitions for the library and the tests, but not for the code that
# links mmgr.
function(mmgr_setup_target target)
    # With C++17, mmgr.cpp also replaces the sized delete (C++14) and the aligned new (C++17). The code itself only needs
    # C++11, and the code that links mmgr can use any standard.
    set_target_properties(${target}
    PROPERTIES
        C_STANDARD 11
        C_STANDARD_REQUIRED ON
        C_EXTENSIONS ${MMGR_C_EXTENSIONS}

        CXX_STANDARD 17
        CXX_STANDARD_REQUIRED ON
        CXX_EXTENSIONS ${MMGR_CXX_EXTENSIONS}
    )

    if (MSVC)
        target_compile_definitions(${target}
        PRIVATE
            _CRT_SECURE_NO_WARNINGS
        )

        target_compile_options(${target}
        PRIVATE
            /W4
            # Force Visual Studio to actualize __cplusplus version macro
            /Zc:__cplusplus
            /permissive-
            /utf-8
        )

        if (MSVC_VERSION GREATER_EQUAL 1900)
            target_compile_options(${target}
            PRIVATE
                /sdl
            )
        endif()
    else()
        target_compile_options(${target}
        PRIVATE
            -Wall -Wextra
        )
    endif()
endfunction()

# Library
#--------------------------------------
# Static, because there is no export list for a DLL, and on Windows a DLL would only replace operator new and delete
# for itself.
add_library(mmgr
STATIC
    src/mmgr.cpp
    src/mmgr.h
    src/nommgr.h
    src/nomalloc.h
    src/nonew.h
)

add_library(mmgr::mmgr ALIAS mmgr)

mmgr_setup_target(mmgr)

target_include_directories(mmgr
PUBLIC
    src
)

# libstdc++-6.dll keeps its own operator new and delete, which mmgr cannot replace, so the STL code inside it would
# allocate blocks that mmgr does not know. Linked statically, every new and delete of the program goes through mmgr.
if (MINGW)
    target_link_options(mmgr
    INTERFACE
        -static-libstdc++
    )
endif()

if (MMGR_NO_DELETE_MACRO)
    target_compile_definitions(mmgr
    PUBLIC
        MMGR_NO_DELETE_MACRO
    )
endif()

# The mutex of m_enableMultithread() needs the thread library on some systems. DJGPP has no threads, and with
# Emscripten every object of the program would have to be compiled for threads.
if (NOT DJGPP AND NOT EMSCRIPTEN)
    find_package(Threads REQUIRED)

    target_link_libraries(mmgr
    PUBLIC
        Threads::Threads
    )
endif()

# Tests
#--------------------------------------
# Each test checks its own results and returns the number of failed checks. The target names have a prefix, so that they
# do not collide with the targets of a parent project, and the program names fit the 8.3 limit of DOS.
if (MMGR_BUILD_TESTS)
    # include(CTest) writes DartConfiguration.tcl, which "ctest -T test" (used by IDEs such as VS Code) looks for. The
    # property keeps it from adding the CDash targets (Experimental, Nightly, Continuous) to the project.
    set_property(GLOBAL PROPERTY CTEST_TARGETS_ADDED 1)
    include(CTest)

    function(mmgr_add_test_program name source)
        set(target mmgr_${name})

        add_executable(${target}
            tests/${source}
            tests/check.h
        )

        mmgr_setup_target(${target})

        set_target_properties(${target}
        PROPERTIES
            OUTPUT_NAME ${name}
        )

        target_link_libraries(${target}
            mmgr
        )

        # Each test runs in its own folder, because mmgr writes memory.log and memleaks.log in the current folder.
        file(MAKE_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/tests/${name})
    endfunction()

    set(MMGR_TESTS
        leaks.cpp
        leaksc.c
        errors.cpp
        errorsc.c
        misuse.cpp
        sizeddel.cpp
        alignnew.cpp
        alignc.c
        stdalloc.cpp
        nodelete.cpp
        failure.c
        failnew.cpp
        logs.c
        options.c
        statics.cpp
    )

    if (NOT DJGPP AND NOT EMSCRIPTEN)
        list(APPEND MMGR_TESTS
            threads.cpp
        )
    endif()

    foreach (source ${MMGR_TESTS})
        get_filename_component(name ${source} NAME_WE)
        mmgr_add_test_program(${name} ${source})
        add_test(NAME mmgr_${name} COMMAND mmgr_${name} WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/tests/${name})
    endforeach()

    # exitthreads.cpp checks the leak report written at exit while other threads release blocks. The report only exists
    # once the program has ended, so a first run writes it and a second one reads it.
    if (NOT DJGPP AND NOT EMSCRIPTEN)
        mmgr_add_test_program(exitthreads exitthreads.cpp)

        foreach (step run check)
            add_test(NAME mmgr_exitthreads_${step} COMMAND mmgr_exitthreads ${step}
                     WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/tests/exitthreads)
        endforeach()

        set_tests_properties(mmgr_exitthreads_run
        PROPERTIES
            FIXTURES_SETUP exitthreads
        )

        set_tests_properties(mmgr_exitthreads_check
        PROPERTIES
            FIXTURES_REQUIRED exitthreads
        )
    endif()

    # stops.c checks that mmgr stops the program, so each run must fail. The asserts are empty in release builds, so
    # these runs only exist for debug builds.
    mmgr_add_test_program(stops stops.c)

    get_property(multiConfig GLOBAL PROPERTY GENERATOR_IS_MULTI_CONFIG)
    if (multiConfig OR NOT CMAKE_BUILD_TYPE MATCHES "^(Release|MinSizeRel|RelWithDebInfo)$")
        if (multiConfig)
            set(stopsConfigurations CONFIGURATIONS Debug)
        endif()

        foreach (scenario error allocation dealloc realloc)
            add_test(NAME mmgr_stops_${scenario} COMMAND mmgr_stops ${scenario} ${stopsConfigurations}
                     WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/tests/stops)

            set_tests_properties(mmgr_stops_${scenario}
            PROPERTIES
                WILL_FAIL TRUE
            )
        endforeach()
    endif()

    target_compile_definitions(mmgr_nodelete
    PRIVATE
        MMGR_NO_DELETE_MACRO
    )

    # This test catches std::bad_alloc, and Emscripten does not catch exceptions unless it is asked to
    if (EMSCRIPTEN)
        target_compile_options(mmgr_failnew
        PRIVATE
            -fexceptions
        )

        target_link_options(mmgr_failnew
        PRIVATE
            -fexceptions
        )
    endif()
endif()
