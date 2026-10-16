# ============================================================================
# Integration Tests: GPU Perception Recovery
# ============================================================================

if(NOT NIMCP_ENABLE_CUDA)
    message(STATUS "CUDA not enabled - skipping GPU perception integration tests")
    return()
endif()

set(TEST_NAME test_perception_gpu_integration)

add_executable(${TEST_NAME}
    test_perception_gpu_integration.cpp
)

target_include_directories(${TEST_NAME} PRIVATE
    ${PROJECT_SOURCE_DIR}/include
    ${PROJECT_SOURCE_DIR}/src
    ${GTEST_INCLUDE_DIRS}
)

target_link_libraries(${TEST_NAME}
    nimcp
    ${GTEST_LIBRARIES}
    ${GTEST_MAIN_LIBRARIES}
    pthread
)

if(NIMCP_ENABLE_CUDA)
    target_link_libraries(${TEST_NAME}
        ${CUDA_LIBRARIES}
        ${CUDA_CUBLAS_LIBRARIES}
        ${CUDA_CUFFT_LIBRARIES}
        ${CUDA_CURAND_LIBRARIES}
    )
    target_compile_definitions(${TEST_NAME} PRIVATE NIMCP_ENABLE_CUDA)
endif()

add_test(NAME ${TEST_NAME} COMMAND ${TEST_NAME})
set_tests_properties(${TEST_NAME} PROPERTIES
    LABELS "integration;gpu;perception;recovery"
    TIMEOUT 300
)
