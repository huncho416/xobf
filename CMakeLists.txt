cmake_minimum_required(VERSION 3.16)
project(xobf LANGUAGES CXX)

add_library(xobf INTERFACE)
target_include_directories(xobf INTERFACE
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>)
target_compile_features(xobf INTERFACE cxx_std_17)

if(NOT DEFINED XOBF_LEVEL)
    set(XOBF_LEVEL 2)
endif()
target_compile_definitions(xobf INTERFACE XOBF_LEVEL=${XOBF_LEVEL})

option(XOBF_BUILD_TESTS "Build xobf tests and demo" ON)

if(XOBF_BUILD_TESTS)
    enable_testing()

    add_executable(xobf_tests tests/tests.cpp)
    target_link_libraries(xobf_tests PRIVATE xobf)
    add_test(NAME xobf_tests COMMAND xobf_tests)

    add_executable(xobf_demo examples/demo.cpp)
    target_link_libraries(xobf_demo PRIVATE xobf)

    add_test(NAME xobf_leak_check
        COMMAND ${CMAKE_COMMAND} -E env bash
        ${CMAKE_CURRENT_SOURCE_DIR}/tests/leak_check.sh $<TARGET_FILE:xobf_demo>)
endif()
