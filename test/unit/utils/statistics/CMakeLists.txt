# Statistics Module Unit Tests
# Comprehensive statistical and probability methods testing

# Helper macro to add statistics test
macro(add_statistics_test TEST_NAME)
    add_executable(unit_utils_statistics_${TEST_NAME}
        ${TEST_NAME}.cpp
    )
    target_link_libraries(unit_utils_statistics_${TEST_NAME}
        PRIVATE
            GTest::GTest
            GTest::Main
            nimcp
            m  # Math library
    )
    target_compile_features(unit_utils_statistics_${TEST_NAME} PRIVATE cxx_std_17)
    add_test(NAME unit_utils_statistics_${TEST_NAME}
             COMMAND unit_utils_statistics_${TEST_NAME})
endmacro()

# Add all statistics tests
add_statistics_test(test_statistics)
add_statistics_test(test_quantum_statistics)
add_statistics_test(test_statistics_exceptions)

# =============================================================================
# Statistics Enhancement Module Unit Tests (Phase STATS-ENH)
# =============================================================================

# Neural statistics tests (spike trains, population codes, tuning curves)
if(EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/test_neural_statistics.cpp)
    add_statistics_test(test_neural_statistics)
endif()

# Information theory tests (transfer entropy, mutual info, synergy)
if(EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/test_information_theory.cpp)
    add_statistics_test(test_information_theory)
endif()

# Multivariate analysis tests (PCA, ICA, factor analysis, CCA)
if(EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/test_multivariate.cpp)
    add_statistics_test(test_multivariate)
endif()

# Time series analysis tests (ARIMA, spectral, change-point)
if(EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/test_timeseries.cpp)
    add_statistics_test(test_timeseries)
endif()

# ML statistics tests (cross-validation, bootstrap, model selection)
if(EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/test_ml_statistics.cpp)
    add_statistics_test(test_ml_statistics)
endif()

# Streaming statistics tests (online mean/var, sketches, reservoir)
if(EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/test_streaming_statistics.cpp)
    add_statistics_test(test_streaming_statistics)
endif()

# Survival analysis tests (Kaplan-Meier, Cox regression, hazard)
if(EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/test_survival.cpp)
    add_statistics_test(test_survival)
endif()

# Advanced Bayesian tests (MCMC, variational inference, GP)
if(EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/test_bayesian_advanced.cpp)
    add_statistics_test(test_bayesian_advanced)
endif()

# Alloca-to-heap migration + bounds tests (C4/C5 bug fixes)
if(EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/test_alloca_bounds.cpp)
    add_statistics_test(test_alloca_bounds)
endif()
