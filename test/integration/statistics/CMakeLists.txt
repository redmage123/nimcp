#=============================================================================
# CMakeLists.txt - Statistics Integration Tests
#=============================================================================

# Integration test macro
macro(add_statistics_integration_test TEST_NAME)
    add_executable(integration_statistics_${TEST_NAME} ${TEST_NAME}.cpp)
    target_link_libraries(integration_statistics_${TEST_NAME}
        PRIVATE
            GTest::GTest
            GTest::Main
            nimcp
            m
    )
    target_include_directories(integration_statistics_${TEST_NAME}
        PRIVATE
            ${PROJECT_SOURCE_DIR}/include
    )
    target_compile_features(integration_statistics_${TEST_NAME} PRIVATE cxx_std_17)
    add_test(
        NAME integration_statistics_${TEST_NAME}
        COMMAND integration_statistics_${TEST_NAME}
    )
endmacro()

# Integration tests
add_statistics_integration_test(test_statistics_shannon_integration)

# =============================================================================
# Statistics Enhancement Module Integration Tests (Phase STATS-ENH)
# =============================================================================

# Neural statistics integration tests
if(EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/test_neural_statistics_integration.cpp)
    add_statistics_integration_test(test_neural_statistics_integration)
endif()

# Information theory integration tests
if(EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/test_information_theory_integration.cpp)
    add_statistics_integration_test(test_information_theory_integration)
endif()

# Multivariate analysis integration tests
if(EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/test_multivariate_integration.cpp)
    add_statistics_integration_test(test_multivariate_integration)
endif()

# Time series analysis integration tests
if(EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/test_timeseries_integration.cpp)
    add_statistics_integration_test(test_timeseries_integration)
endif()

# ML statistics integration tests
if(EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/test_ml_statistics_integration.cpp)
    add_statistics_integration_test(test_ml_statistics_integration)
endif()

# Streaming statistics integration tests
if(EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/test_streaming_statistics_integration.cpp)
    add_statistics_integration_test(test_streaming_statistics_integration)
endif()

# Survival and Bayesian combined integration tests
if(EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/test_survival_bayesian_integration.cpp)
    add_statistics_integration_test(test_survival_bayesian_integration)
endif()
