# Gate Ladder CMakeLists.txt
# KT-1: Ordered gates 0→9, weightless suite runs in <60s, no external artifacts
# Single gate_runner binary: all gate TUs register via create_gate_N() factories
# and gate_runner.cpp drives them (gates 0-8 have no main of their own).

add_executable(gate_runner
    gate_runner.cpp
    gate_0_cli_contract.cpp
    gate_1_unit_kernels.cpp
    gate_2_memory.cpp
    gate_3_equivalence.cpp
    gate_4_config_refusal.cpp
    gate_5_scale.cpp
    gate_6_ingestion.cpp
    gate_7_tiering.cpp
    gate_8_soundness.cpp
    gate_9_tiny_oracle.cpp
    # gate_10_cognitive_loop.cpp  # Disabled for now - needs API alignment
)
target_link_libraries(gate_runner PRIVATE YukiCore)
set_target_properties(gate_runner PROPERTIES RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/tests/gates)

# CTest registration (runs from the repo root — gates use data/ relative paths)
add_test(NAME gate_ladder COMMAND gate_runner
         WORKING_DIRECTORY ${CMAKE_SOURCE_DIR})

# Aggregate target: run all gates in order
add_custom_target(gate_ladder
    COMMAND gate_runner
    WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
    COMMENT "Running KT-1 Gate Ladder 0→9"
    VERBATIM
)

# Weightless suite (gates 0-4 only, no external artifacts, ~1 min)
add_custom_target(gate_weightless
    COMMAND gate_runner --weightless
    WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
    COMMENT "Running KT-1 Weightless Gate Suite"
    VERBATIM
)
