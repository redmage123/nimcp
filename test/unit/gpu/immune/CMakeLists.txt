# GPU Immune System Unit Tests

# GPU Execution Immune Unit Tests
add_executable(unit_gpu_immune_execution
    test_gpu_execution_immune.cpp
)
target_link_libraries(unit_gpu_immune_execution
    PRIVATE
        GTest::GTest
        GTest::Main
        nimcp
)
target_include_directories(unit_gpu_immune_execution
    PRIVATE
        ${CMAKE_SOURCE_DIR}/include
        ${CMAKE_SOURCE_DIR}/src
)
add_test(NAME unit_gpu_immune_execution
         COMMAND unit_gpu_immune_execution)
set_tests_properties(unit_gpu_immune_execution PROPERTIES LABELS "unit")

# Multi-GPU Immune Unit Tests
add_executable(unit_gpu_immune_multigpu
    test_multigpu_immune.cpp
)
target_link_libraries(unit_gpu_immune_multigpu
    PRIVATE
        GTest::GTest
        GTest::Main
        nimcp
)
target_include_directories(unit_gpu_immune_multigpu
    PRIVATE
        ${CMAKE_SOURCE_DIR}/include
        ${CMAKE_SOURCE_DIR}/src
)
add_test(NAME unit_gpu_immune_multigpu
         COMMAND unit_gpu_immune_multigpu)
set_tests_properties(unit_gpu_immune_multigpu PROPERTIES LABELS "unit")
