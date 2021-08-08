cmake_minimum_required(VERSION 3.9)
project(unittests CXX)
add_subdirectory(/usr/src/googletest gtest)

add_executable(tests
    array-test.cpp
    atomic-test.cpp
    list-test.cpp
    locked-test.cpp
    refcounted-test.cpp
    vector-test.cpp
)
target_compile_features(tests PRIVATE cxx_std_20)
target_link_libraries(tests gtest gtest_main)
