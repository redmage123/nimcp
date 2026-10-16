# ============================================================================
# Unit Tests: GPU Perception Recovery
# ============================================================================

if(NOT NIMCP_ENABLE_CUDA)
    message(STATUS "CUDA not enabled - skipping GPU perception unit tests")
    return()
endif()

set(TEST_NAME test_perception_gpu_recovery)

add_executable(${TEST_NAME}
    test_perception_gpu_recovery.cpp
)

target_include_directories(${TEST_NAME} PRIVATE
    ${CMAKE_SOURCE_DIR}/include
    ${CMAKE_SOURCE_DIR}/src
    ${CUDA_INCLUDE_DIRS}
)

target_link_libraries(${TEST_NAME}
    PRIVATE
        nimcp
        GTest::gtest
        GTest::gtest_main
        ${CUDA_LIBRARIES}
        m
        pthread
)

add_test(NAME ${TEST_NAME} COMMAND ${TEST_NAME})
set_tests_properties(${TEST_NAME} PROPERTIES
    LABELS "unit;gpu;perception;recovery"
    TIMEOUT 120
)
