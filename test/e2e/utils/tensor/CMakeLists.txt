# ============================================================================
# NIMCP Tensor End-to-End Tests
# ============================================================================
# WHAT: E2E tests for complete tensor einsum + autodiff pipelines
# WHY:  Validate full training-like workflows: forward einsum, loss, backward,
#       gradient verification, and parameter updates
# HOW:  GTest-based tests with full nimcp library linkage
# ============================================================================

message(STATUS "  Adding Tensor E2E tests...")

if(EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/test_tensor_e2e.cpp)
    add_executable(e2e_utils_tensor_test_tensor_e2e
        ${CMAKE_CURRENT_SOURCE_DIR}/test_tensor_e2e.cpp
    )
    target_include_directories(e2e_utils_tensor_test_tensor_e2e PRIVATE
        ${CMAKE_SOURCE_DIR}/include
    )
    target_link_libraries(e2e_utils_tensor_test_tensor_e2e PRIVATE
        GTest::GTest
        GTest::Main
        nimcp
        pthread
        m
    )
    target_compile_features(e2e_utils_tensor_test_tensor_e2e PRIVATE cxx_std_17)
    target_compile_definitions(e2e_utils_tensor_test_tensor_e2e PRIVATE
        NIMCP_TESTING
        NIMCP_E2E_TESTING
    )
    add_test(NAME e2e_utils_tensor_test_tensor_e2e
             COMMAND e2e_utils_tensor_test_tensor_e2e)
    set_tests_properties(e2e_utils_tensor_test_tensor_e2e PROPERTIES
        LABELS "e2e;utils;tensor;einsum;autodiff"
        TIMEOUT 300
    )
    message(STATUS "    Added: e2e_utils_tensor_test_tensor_e2e")
endif()
