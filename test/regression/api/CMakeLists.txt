# API Regression Tests
# Test API Backward Compatibility for Complex Number Opt-In

# Test Complex Opt-In Backward Compatibility - DISABLED: Uses deprecated pattern_library API
#add_executable(regression_api_test_complex_opt_in
#    test_complex_opt_in.cpp
#)
#target_link_libraries(regression_api_test_complex_opt_in
#    PRIVATE
#        GTest::GTest
#        GTest::Main
#        nimcp_middleware
#        nimcp
#    )
#add_test(NAME regression_api_test_complex_opt_in
#         COMMAND regression_api_test_complex_opt_in)

# Existing API tests
add_executable(regression_api_test_api_stability
    test_api_stability.cpp
)
target_link_libraries(regression_api_test_api_stability
    PRIVATE
        GTest::GTest
        GTest::Main
        nimcp
    )
add_test(NAME regression_api_test_api_stability
         COMMAND regression_api_test_api_stability)

add_executable(regression_api_test_api_memory_safety
    test_api_memory_safety.cpp
)
target_link_libraries(regression_api_test_api_memory_safety
    PRIVATE
        GTest::GTest
        GTest::Main
        nimcp
    )
add_test(NAME regression_api_test_api_memory_safety
         COMMAND regression_api_test_api_memory_safety)
set_tests_properties(regression_api_test_api_memory_safety PROPERTIES
    TIMEOUT 300 RESOURCE_LOCK "brain_heavy"
)

add_executable(regression_api_test_api_file_format
    test_api_file_format.cpp
)
target_link_libraries(regression_api_test_api_file_format
    PRIVATE
        GTest::GTest
        GTest::Main
        nimcp
    )
add_test(NAME regression_api_test_api_file_format
         COMMAND regression_api_test_api_file_format)
