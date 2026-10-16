# test/unit/gpu/misc/CMakeLists.txt
# Unit tests for miscellaneous GPU recovery integration

# DISABLED: Test has extern "C" linkage error - CUDA templates conflict with C linkage
# TODO: Fix extern "C" block in test_misc_gpu_recovery.cpp line 23
#
# # Test for misc GPU recovery integration
# add_executable(test_misc_gpu_recovery test_misc_gpu_recovery.cpp)
# target_link_libraries(test_misc_gpu_recovery PRIVATE
#     nimcp
#     GTest::gtest
#     GTest::gtest_main
# )
# target_include_directories(test_misc_gpu_recovery PRIVATE
#     ${PROJECT_SOURCE_DIR}/include
#     ${PROJECT_SOURCE_DIR}/src
# )
# add_test(NAME test_misc_gpu_recovery COMMAND test_misc_gpu_recovery)
# set_tests_properties(test_misc_gpu_recovery PROPERTIES
#     LABELS "unit;gpu;recovery"
#     TIMEOUT 60
# )
