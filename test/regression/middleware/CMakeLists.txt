# Middleware Regression Tests
add_subdirectory(buffering)
add_subdirectory(encoding)
add_subdirectory(events)
add_subdirectory(normalization)
add_subdirectory(patterns)
add_subdirectory(pipeline)
add_subdirectory(routing)
add_subdirectory(training)

# Signal Filter Regression Tests
add_executable(regression_middleware_test_signal_filter_regression
    test_signal_filter_regression.cpp
)
target_link_libraries(regression_middleware_test_signal_filter_regression
    PRIVATE
        GTest::GTest
        GTest::Main
        nimcp_middleware
        nimcp
    )
add_test(NAME regression_middleware_test_signal_filter_regression
         COMMAND regression_middleware_test_signal_filter_regression)
set_tests_properties(regression_middleware_test_signal_filter_regression PROPERTIES TIMEOUT 60)

# Phase 2 Regression Tests
# TODO: Fix outdated API usage in test_phase2_regression.cpp
# - Replace rate_coder_t with rate_coding_encoder_t
# - Replace spike_record_t with spike_train_t
# - Update all function signatures to match current API
# Temporarily disabled until Phase 3 implementation is complete
#add_executable(regression_middleware_middleware_test_phase2_regression
#    test_phase2_regression.cpp
#)
#target_link_libraries(regression_middleware_middleware_test_phase2_regression
#    PRIVATE
#        GTest::GTest
#        GTest::Main
#        nimcp_middleware
#        nimcp
#    )
#add_test(NAME regression_middleware_middleware_test_phase2_regression
#         COMMAND regression_middleware_middleware_test_phase2_regression)

# Brain Integration Regression Tests
add_executable(regression_middleware_test_brain_integration_regression
    test_brain_integration_regression.cpp
)
target_link_libraries(regression_middleware_test_brain_integration_regression
    PRIVATE
        GTest::GTest
        GTest::Main
        nimcp_middleware
        nimcp
    )
add_test(NAME regression_middleware_test_brain_integration_regression
         COMMAND regression_middleware_test_brain_integration_regression)

# Cognitive Adapters Regression Tests - DISABLED: Uses deprecated event subscription API
#add_executable(regression_middleware_test_cognitive_adapters_regression
#    test_cognitive_adapters_regression.cpp
#)
#target_link_libraries(regression_middleware_test_cognitive_adapters_regression
#    PRIVATE
#        GTest::GTest
#        GTest::Main
#        nimcp_middleware
#        nimcp
#    )
#add_test(NAME regression_middleware_test_cognitive_adapters_regression
#         COMMAND regression_middleware_test_cognitive_adapters_regression)

# Training Adapters Regression Tests - DISABLED: Uses deprecated event subscription API
#add_executable(regression_middleware_test_training_adapters_regression
#    test_training_adapters_regression.cpp
#)
#target_link_libraries(regression_middleware_test_training_adapters_regression
#    PRIVATE
#        GTest::GTest
#        GTest::Main
#        nimcp_middleware
#        nimcp
#    )
#add_test(NAME regression_middleware_test_training_adapters_regression
#         COMMAND regression_middleware_test_training_adapters_regression)

# Executive Regression Tests (Phase 1.5.2)
add_executable(regression_middleware_test_executive_regression
    test_executive_regression.cpp
)
target_link_libraries(regression_middleware_test_executive_regression
    PRIVATE
        GTest::GTest
        GTest::Main
        nimcp
        nimcp_middleware
        pthread
        m
)
add_test(NAME regression_middleware_test_executive_regression
         COMMAND regression_middleware_test_executive_regression)

# Positional encoding regression tests for sequence detection (DISABLED - API updates needed)
# add_executable(regression_middleware_sequence_pe_test
#     test_sequence_pe_regression.cpp
# )
# target_link_libraries(regression_middleware_sequence_pe_test
#     PRIVATE
#         nimcp
#         GTest::gtest
#         GTest::gtest_main
#         Threads::Threads
#         m
# )
# target_compile_features(regression_middleware_sequence_pe_test PRIVATE cxx_std_17)
# gtest_discover_tests(regression_middleware_sequence_pe_test
#     PROPERTIES
#         TIMEOUT 600
#         LABELS "regression;middleware;sequence;positional_encoding"
# )

# Phase 1.5.5 Middleware Controller Regression Tests
if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/integration")
    add_subdirectory(integration)
endif()
