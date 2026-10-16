# Language Region Regression Tests
# Comprehensive regression tests for Broca's and Wernicke's brain regions
#
# Tests cover:
# - Broca's region (syntax, phonological, motor planning)
# - Wernicke's region (lexical access, semantic integration)
# - Language pathway (Broca-Wernicke integration via arcuate fasciculus)

# Find threading support
find_package(Threads REQUIRED)

#==============================================================================
# Broca Regression Tests
#==============================================================================

add_executable(regression_broca_comprehensive
    test_broca_regression.cpp
)

target_link_libraries(regression_broca_comprehensive
    PRIVATE
        nimcp
        GTest::GTest
        GTest::Main
        Threads::Threads
)

target_include_directories(regression_broca_comprehensive
    PRIVATE
        ${CMAKE_SOURCE_DIR}/include
        ${CMAKE_SOURCE_DIR}/src
        ${CMAKE_SOURCE_DIR}/test
)

add_test(
    NAME regression_broca_comprehensive
    COMMAND regression_broca_comprehensive
)

set_tests_properties(regression_broca_comprehensive
    PROPERTIES
        LABELS "regression;language;broca"
        TIMEOUT 180
)

#==============================================================================
# Wernicke Regression Tests
#==============================================================================

add_executable(regression_wernicke_comprehensive
    test_wernicke_regression.cpp
)

target_link_libraries(regression_wernicke_comprehensive
    PRIVATE
        nimcp
        GTest::GTest
        GTest::Main
        Threads::Threads
)

target_include_directories(regression_wernicke_comprehensive
    PRIVATE
        ${CMAKE_SOURCE_DIR}/include
        ${CMAKE_SOURCE_DIR}/src
        ${CMAKE_SOURCE_DIR}/test
)

add_test(
    NAME regression_wernicke_comprehensive
    COMMAND regression_wernicke_comprehensive
)

set_tests_properties(regression_wernicke_comprehensive
    PROPERTIES
        LABELS "regression;language;wernicke"
        TIMEOUT 180
)

#==============================================================================
# Language Pathway Regression Tests
#==============================================================================

add_executable(regression_language_pathway
    test_language_pathway_regression.cpp
)

target_link_libraries(regression_language_pathway
    PRIVATE
        nimcp
        GTest::GTest
        GTest::Main
        Threads::Threads
)

target_include_directories(regression_language_pathway
    PRIVATE
        ${CMAKE_SOURCE_DIR}/include
        ${CMAKE_SOURCE_DIR}/src
        ${CMAKE_SOURCE_DIR}/test
)

add_test(
    NAME regression_language_pathway
    COMMAND regression_language_pathway
)

set_tests_properties(regression_language_pathway
    PROPERTIES
        LABELS "regression;language;pathway"
        TIMEOUT 300
)

#==============================================================================
# Custom Targets for Running Language Tests
#==============================================================================

# Target to run all language regression tests
add_custom_target(run_language_regression
    COMMAND ${CMAKE_CTEST_COMMAND} -L language --output-on-failure
    DEPENDS
        regression_broca_comprehensive
        regression_wernicke_comprehensive
        regression_language_pathway
    COMMENT "Running all language regression tests"
)

# Target to run only Broca tests
add_custom_target(run_broca_regression
    COMMAND ${CMAKE_CTEST_COMMAND} -L broca --output-on-failure
    DEPENDS regression_broca_comprehensive
    COMMENT "Running Broca regression tests"
)

# Target to run only Wernicke tests
add_custom_target(run_wernicke_regression
    COMMAND ${CMAKE_CTEST_COMMAND} -L wernicke --output-on-failure
    DEPENDS regression_wernicke_comprehensive
    COMMENT "Running Wernicke regression tests"
)
