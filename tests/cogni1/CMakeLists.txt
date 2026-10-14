cmake_minimum_required(VERSION 3.20)
project(cogni1_tests LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Include directories
include_directories(${CMAKE_SOURCE_DIR}/src)
include_directories(${CMAKE_SOURCE_DIR}/tests)
include_directories(${CMAKE_BINARY_DIR})  # For ConfigSchema_generated.h

# Gate 0 executable
add_executable(cogni1_gate0
    main_gate0.cpp
    GridworldSimulator.cpp
    BruteForceOracle.cpp
    BaselineRunner.cpp
    MetricsLogger.cpp
    Phase0_Gate0.cpp
)

target_link_libraries(cogni1_gate0
    PRIVATE
    nlohmann_json::nlohmann_json
    ${FLATBUFFERS_TARGET}
)

# Compiler options
target_compile_options(cogni1_gate0 PRIVATE
    -Wall -Wextra -Wpedantic
    -O2 -DNDEBUG
)

# Add test
add_test(NAME cogni1_gate0 COMMAND cogni1_gate0)

# Simple test executable
add_executable(cogni1_test_task
    test_task.cpp
    GridworldSimulator.cpp
)

target_link_libraries(cogni1_test_task
    PRIVATE
    nlohmann_json::nlohmann_json
    ${FLATBUFFERS_TARGET}
)

target_compile_options(cogni1_test_task PRIVATE
    -Wall -Wextra -Wpedantic
    -O2 -DNDEBUG
)

add_test(NAME cogni1_test_task COMMAND cogni1_test_task)

# Component test
add_executable(cogni1_test_components
    test_components.cpp
    GridworldSimulator.cpp
)

target_link_libraries(cogni1_test_components
    PRIVATE
    nlohmann_json::nlohmann_json
    ${FLATBUFFERS_TARGET}
)

target_compile_options(cogni1_test_components PRIVATE
    -Wall -Wextra -Wpedantic
    -O2 -DNDEBUG
)

add_test(NAME cogni1_test_components COMMAND cogni1_test_components)

# Detailed task test
add_executable(cogni1_test_task_detailed
    test_task_detailed.cpp
    GridworldSimulator.cpp
)

target_link_libraries(cogni1_test_task_detailed
    PRIVATE
    nlohmann_json::nlohmann_json
    ${FLATBUFFERS_TARGET}
)

target_compile_options(cogni1_test_task_detailed PRIVATE
    -Wall -Wextra -Wpedantic
    -O2 -DNDEBUG
)

add_test(NAME cogni1_test_task_detailed COMMAND cogni1_test_task_detailed)

# Isolation test
add_executable(cogni1_test_task_isolation
    test_task_isolation.cpp
    GridworldSimulator.cpp
)

target_link_libraries(cogni1_test_task_isolation
    PRIVATE
    nlohmann_json::nlohmann_json
    ${FLATBUFFERS_TARGET}
)

target_compile_options(cogni1_test_task_isolation PRIVATE
    -Wall -Wextra -Wpedantic
    -O2 -DNDEBUG
)

add_test(NAME cogni1_test_task_isolation COMMAND cogni1_test_task_isolation)

# WS only test
add_executable(cogni1_test_ws_only
    test_ws_only.cpp
    GridworldSimulator.cpp
)

target_link_libraries(cogni1_test_ws_only
    PRIVATE
    nlohmann_json::nlohmann_json
    ${FLATBUFFERS_TARGET}
)

target_compile_options(cogni1_test_ws_only PRIVATE
    -Wall -Wextra -Wpedantic
    -O2 -DNDEBUG
)

add_test(NAME cogni1_test_ws_only COMMAND cogni1_test_ws_only)

# WS member test
add_executable(cogni1_test_ws_member
    test_ws_member.cpp
    GridworldSimulator.cpp
)

target_link_libraries(cogni1_test_ws_member
    PRIVATE
    nlohmann_json::nlohmann_json
    ${FLATBUFFERS_TARGET}
)

target_compile_options(cogni1_test_ws_member PRIVATE
    -Wall -Wextra -Wpedantic
    -O2 -DNDEBUG
)

add_test(NAME cogni1_test_ws_member COMMAND cogni1_test_ws_member)

# TaskMinimal test
add_executable(cogni1_test_task_minimal
    test_task_minimal.cpp
    GridworldSimulator.cpp
)

target_link_libraries(cogni1_test_task_minimal
    PRIVATE
    nlohmann_json::nlohmann_json
    ${FLATBUFFERS_TARGET}
)

target_compile_options(cogni1_test_task_minimal PRIVATE
    -Wall -Wextra -Wpedantic
    -O2 -DNDEBUG
)

add_test(NAME cogni1_test_task_minimal COMMAND cogni1_test_task_minimal)

# Incremental test
add_executable(cogni1_test_task_incremental
    test_task_incremental.cpp
    GridworldSimulator.cpp
)

target_link_libraries(cogni1_test_task_incremental
    PRIVATE
    nlohmann_json::nlohmann_json
    ${FLATBUFFERS_TARGET}
)

target_compile_options(cogni1_test_task_incremental PRIVATE
    -Wall -Wextra -Wpedantic
    -O2 -DNDEBUG
)

add_test(NAME cogni1_test_task_incremental COMMAND cogni1_test_task_incremental)

# Incremental test 1
add_executable(cogni1_test_incremental1
    test_incremental1.cpp
    GridworldSimulator.cpp
)

target_link_libraries(cogni1_test_incremental1
    PRIVATE
    nlohmann_json::nlohmann_json
    ${FLATBUFFERS_TARGET}
)

target_compile_options(cogni1_test_incremental1 PRIVATE
    -Wall -Wextra -Wpedantic
    -O2 -DNDEBUG
)

add_test(NAME cogni1_test_incremental1 COMMAND cogni1_test_incremental1)

# Incremental test 2
add_executable(cogni1_test_incremental2
    test_incremental2.cpp
    GridworldSimulator.cpp
)

target_link_libraries(cogni1_test_incremental2
    PRIVATE
    nlohmann_json::nlohmann_json
    ${FLATBUFFERS_TARGET}
)

target_compile_options(cogni1_test_incremental2 PRIVATE
    -Wall -Wextra -Wpedantic
    -O2 -DNDEBUG
)

add_test(NAME cogni1_test_incremental2 COMMAND cogni1_test_incremental2)

# Incremental test 2 simple
add_executable(cogni1_test_incremental2simple
    test_incremental2simple.cpp
    GridworldSimulator.cpp
)

target_link_libraries(cogni1_test_incremental2simple
    PRIVATE
    nlohmann_json::nlohmann_json
    ${FLATBUFFERS_TARGET}
)

target_compile_options(cogni1_test_incremental2simple PRIVATE
    -Wall -Wextra -Wpedantic
    -O2 -DNDEBUG
)

add_test(NAME cogni1_test_incremental2simple COMMAND cogni1_test_incremental2simple)

# Vector only test
add_executable(cogni1_test_vector_only
    test_vector_only.cpp
    GridworldSimulator.cpp
)

target_link_libraries(cogni1_test_vector_only
    PRIVATE
    nlohmann_json::nlohmann_json
    ${FLATBUFFERS_TARGET}
)

target_compile_options(cogni1_test_vector_only PRIVATE
    -Wall -Wextra -Wpedantic
    -O2 -DNDEBUG
)

add_test(NAME cogni1_test_vector_only COMMAND cogni1_test_vector_only)

# Vector + string test
add_executable(cogni1_test_vector_string
    test_vector_string.cpp
    GridworldSimulator.cpp
)

target_link_libraries(cogni1_test_vector_string
    PRIVATE
    nlohmann_json::nlohmann_json
    ${FLATBUFFERS_TARGET}
)

target_compile_options(cogni1_test_vector_string PRIVATE
    -Wall -Wextra -Wpedantic
    -O2 -DNDEBUG
)

add_test(NAME cogni1_test_vector_string COMMAND cogni1_test_vector_string)

# COGNI-1 Integrated System Demo
add_executable(cogni1_demo
    main_cogni1.cpp
    GridworldSimulator.cpp
    TaskGenerator.cpp
    MetricsLogger.cpp
    Phase0_Gate0.cpp
    BruteForceOracle.cpp
    BaselineRunner.cpp
)

target_link_libraries(cogni1_demo
    PRIVATE
    YukiCore
    nlohmann_json::nlohmann_json
    ${FLATBUFFERS_TARGET}
)

target_compile_options(cogni1_demo PRIVATE
    -Wall -Wextra -Wpedantic
    -O2 -DNDEBUG
)

add_test(NAME cogni1_demo COMMAND cogni1_demo)