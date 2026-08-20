#boilerplate to name project and define C++ version
cmake_minimum_required(VERSION 3.20)
project(matching-engine LANGUAGES CXX)

#set C++ standard to C++20
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

#download google test automatically on first build
include(FetchContent)
FetchContent_Declare(
    googletest
    GIT_REPOSITORY https://github.com/google/googletest.git
    GIT_TAG v1.15.2
)
FetchContent_MakeAvailable(googletest)

#define real program, compile into an executable named matching_engine
add_executable(matching_engine src/main.cpp)
target_include_directories(matching_engine PRIVATE include)

#separate test executable, compile into an executable named engine_tests
enable_testing()
add_executable(engine_tests ../tests/smoke_test.cpp)
target_include_directories(engine_tests PRIVATE include)
target_link_libraries(engine_tests PRIVATE gtest_main)

#lets ctest discover and run test cases within engine_tests individually
include(GoogleTest)
gtest_discover_tests(engine_tests)

