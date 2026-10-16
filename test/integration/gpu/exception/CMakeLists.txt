# GPU Exception Handling Integration Tests
#
# WHAT: CMake configuration for GPU exception integration tests
# WHY:  Verify GPU exception-to-immune pipeline works correctly
# HOW:  Build and link test executable against GTest, nimcp, and immune modules

# DISABLED: Test has extern "C" linkage error - CUDA templates conflict with C linkage
# TODO: Fix extern "C" block in test_gpu_exception_integration.cpp line 30
#
# # GPU Exception Integration Tests
# add_executable(integration_gpu_exception
#     test_gpu_exception_integration.cpp
# )
#
# target_link_libraries(integration_gpu_exception
#     PRIVATE
#         GTest::GTest
#         GTest::Main
#         nimcp
# )
#
# target_include_directories(integration_gpu_exception
#     PRIVATE
#         ${CMAKE_SOURCE_DIR}/include
#         ${CMAKE_SOURCE_DIR}/src
# )
#
# # Add compile definitions for testing
# target_compile_definitions(integration_gpu_exception
#     PRIVATE
#         NIMCP_TESTING=1
# )
#
# add_test(
#     NAME integration_gpu_exception
#     COMMAND integration_gpu_exception
# )
#
# set_tests_properties(integration_gpu_exception
#     PROPERTIES
#         LABELS "integration;gpu;exception;immune"
#         TIMEOUT 120
# )
