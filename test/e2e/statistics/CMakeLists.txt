#=============================================================================
# CMakeLists.txt - Statistics End-to-End Tests
#=============================================================================

# E2E test macro
macro(add_statistics_e2e_test TEST_NAME)
    add_executable(e2e_statistics_${TEST_NAME} ${TEST_NAME}.cpp)
    target_link_libraries(e2e_statistics_${TEST_NAME}
        PRIVATE
            GTest::GTest
            GTest::Main
            nimcp
            m
    )
    target_include_directories(e2e_statistics_${TEST_NAME}
        PRIVATE
            ${PROJECT_SOURCE_DIR}/include
    )
    target_compile_features(e2e_statistics_${TEST_NAME} PRIVATE cxx_std_17)
    add_test(
        NAME e2e_statistics_${TEST_NAME}
        COMMAND e2e_statistics_${TEST_NAME}
    )
endmacro()

# E2E tests
add_statistics_e2e_test(test_statistics_e2e)

# =============================================================================
# Statistics Enhancement Module E2E Tests (Phase STATS-ENH)
# =============================================================================

# Neural statistics E2E tests
if(EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/test_neural_statistics_e2e.cpp)
    add_statistics_e2e_test(test_neural_statistics_e2e)
endif()

# Information theory E2E tests
if(EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/test_information_theory_e2e.cpp)
    add_statistics_e2e_test(test_information_theory_e2e)
endif()

# Multivariate analysis E2E tests
if(EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/test_multivariate_e2e.cpp)
    add_statistics_e2e_test(test_multivariate_e2e)
endif()

# Time series analysis E2E tests
if(EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/test_timeseries_e2e.cpp)
    add_statistics_e2e_test(test_timeseries_e2e)
endif()

# ML statistics E2E tests
if(EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/test_ml_statistics_e2e.cpp)
    add_statistics_e2e_test(test_ml_statistics_e2e)
endif()

# Streaming statistics E2E tests
if(EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/test_streaming_statistics_e2e.cpp)
    add_statistics_e2e_test(test_streaming_statistics_e2e)
endif()

# Survival and Bayesian combined E2E tests
if(EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/test_survival_bayesian_e2e.cpp)
    add_statistics_e2e_test(test_survival_bayesian_e2e)
endif()

# Statistics GPU E2E tests
if(EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/test_statistics_gpu_e2e.cpp)
    add_statistics_e2e_test(test_statistics_gpu_e2e)
endif()
