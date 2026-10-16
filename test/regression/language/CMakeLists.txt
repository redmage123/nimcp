# ============================================================================
# Grounded Language Regression Tests
# ============================================================================
add_executable(regression_grounded_language
    test_grounded_language_regression.cpp
)
target_link_libraries(regression_grounded_language
    PRIVATE nimcp GTest::gtest GTest::gtest_main Threads::Threads m
)
target_include_directories(regression_grounded_language
    PRIVATE
        ${PROJECT_SOURCE_DIR}/include
        ${PROJECT_SOURCE_DIR}/src
)
target_compile_features(regression_grounded_language PRIVATE cxx_std_17)
gtest_discover_tests(regression_grounded_language
    WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
    PROPERTIES
        LABELS "regression;language;grounded"
        TIMEOUT 120
)

# ============================================================================
# Collapse-Fix Regression Guards
# ============================================================================
add_executable(regression_grounded_language_collapse_fix
    test_grounded_language_collapse_fix_regression.cpp
)
target_link_libraries(regression_grounded_language_collapse_fix
    PRIVATE nimcp GTest::gtest GTest::gtest_main Threads::Threads m
)
target_include_directories(regression_grounded_language_collapse_fix
    PRIVATE
        ${PROJECT_SOURCE_DIR}/include
        ${PROJECT_SOURCE_DIR}/src
)
target_compile_features(regression_grounded_language_collapse_fix PRIVATE cxx_std_17)
gtest_discover_tests(regression_grounded_language_collapse_fix
    WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
    PROPERTIES
        LABELS "regression;language;grounded;collapse"
        TIMEOUT 60
)

# ============================================================================
# Language Cortex Training Regression (multilingual: FR/DE/IT/ES/ZH)
# ============================================================================
add_executable(regression_language_cortex_training
    test_language_cortex_training_regression.cpp
)
target_link_libraries(regression_language_cortex_training
    PRIVATE nimcp GTest::gtest GTest::gtest_main Threads::Threads m
)
target_include_directories(regression_language_cortex_training
    PRIVATE
        ${PROJECT_SOURCE_DIR}/include
        ${PROJECT_SOURCE_DIR}/src
)
target_compile_features(regression_language_cortex_training PRIVATE cxx_std_17)
gtest_discover_tests(regression_language_cortex_training
    WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
    PROPERTIES
        LABELS "regression;language;cortex;training;multilingual"
        TIMEOUT 180
)
