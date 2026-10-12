cmake_minimum_required(VERSION 3.20)
project(arc_tests)

add_executable(arc_test_harness arc_test_harness.cpp)

target_link_libraries(arc_test_harness PRIVATE YukiCore nlohmann_json::nlohmann_json)

add_test(NAME arc_test_harness COMMAND arc_test_harness)