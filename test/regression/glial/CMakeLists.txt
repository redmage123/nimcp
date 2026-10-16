# ============================================================================
# Glial Performance Regression Tests
# ============================================================================
# WHAT: Build configuration for glial module performance regression tests
# WHY:  Ensure glial operations meet timing requirements and scale correctly
# HOW:  Build test executables, register with CTest
#
# TESTS:
# 1. Microglia Pruning Performance (100 microglia x 100 synapses)
# 2. Oligodendrocyte Myelination Performance (50 oligo x 50 neurons)
# 3. Full Glial System Scalability (10 to 1000 cells)
# 4. Memory Efficiency (create/destroy 1000 cells)
# 5. Concurrent Access (multi-threaded access)
# ============================================================================

# Glial Performance Regression Test
add_executable(regression_glial_performance
    test_glial_performance_regression.cpp
)

target_link_libraries(regression_glial_performance
    PRIVATE
        GTest::GTest
        GTest::Main
        nimcp
        pthread
)

# Enable C++17 for structured bindings and modern features
target_compile_features(regression_glial_performance PRIVATE cxx_std_17)

# Include directories
target_include_directories(regression_glial_performance
    PRIVATE
        ${CMAKE_SOURCE_DIR}/include
        ${CMAKE_SOURCE_DIR}/src
)

# Register test with CTest
add_test(NAME regression_glial_performance
         COMMAND regression_glial_performance)

# Set test properties for timeout (performance tests may take longer)
set_tests_properties(regression_glial_performance
    PROPERTIES
        TIMEOUT 120
        LABELS "regression;glial;performance"
)

# ============================================================================
# Microglia Performance Regression Test
# ============================================================================
add_executable(regression_microglia_performance
    test_microglia_performance_regression.cpp
)

target_link_libraries(regression_microglia_performance
    PRIVATE
        GTest::GTest
        GTest::Main
        nimcp
        pthread
)

target_compile_features(regression_microglia_performance PRIVATE cxx_std_17)

target_include_directories(regression_microglia_performance
    PRIVATE
        ${CMAKE_SOURCE_DIR}/include
        ${CMAKE_SOURCE_DIR}/src
)

add_test(NAME regression_microglia_performance
         COMMAND regression_microglia_performance)

set_tests_properties(regression_microglia_performance
    PROPERTIES
        TIMEOUT 120
        LABELS "regression;glial;microglia;performance"
)

# ============================================================================
# Oligodendrocyte Performance Regression Test
# ============================================================================
add_executable(regression_oligodendrocyte_performance
    test_oligodendrocyte_performance_regression.cpp
)

target_link_libraries(regression_oligodendrocyte_performance
    PRIVATE
        GTest::GTest
        GTest::Main
        nimcp
        pthread
)

target_compile_features(regression_oligodendrocyte_performance PRIVATE cxx_std_17)

target_include_directories(regression_oligodendrocyte_performance
    PRIVATE
        ${CMAKE_SOURCE_DIR}/include
        ${CMAKE_SOURCE_DIR}/src
)

add_test(NAME regression_oligodendrocyte_performance
         COMMAND regression_oligodendrocyte_performance)

set_tests_properties(regression_oligodendrocyte_performance
    PROPERTIES
        TIMEOUT 180
        LABELS "regression;glial;oligodendrocyte;performance"
        RESOURCE_LOCK "brain_heavy"
)

# ============================================================================
# Myelin Sheath Performance Regression Test
# ============================================================================
add_executable(regression_myelin_performance
    test_myelin_performance_regression.cpp
)

target_link_libraries(regression_myelin_performance
    PRIVATE
        GTest::GTest
        GTest::Main
        nimcp
        pthread
)

target_compile_features(regression_myelin_performance PRIVATE cxx_std_17)

target_include_directories(regression_myelin_performance
    PRIVATE
        ${CMAKE_SOURCE_DIR}/include
        ${CMAKE_SOURCE_DIR}/src
)

add_test(NAME regression_myelin_performance
         COMMAND regression_myelin_performance)

set_tests_properties(regression_myelin_performance
    PROPERTIES
        TIMEOUT 180
        LABELS "regression;glial;myelin;performance"
)

# ============================================================================
# Myelin Math Biophysics Performance Regression Test
# ============================================================================
# WHAT: Performance benchmarks for all myelin math biophysics functions
# WHY:  Ensure mathematical calculations remain efficient at scale
# HOW:  Measure timing for G-ratio, cable theory, saltatory, metabolic, RNG
# ============================================================================
add_executable(regression_myelin_math_performance
    test_myelin_math_performance_regression.cpp
)

target_link_libraries(regression_myelin_math_performance
    PRIVATE
        GTest::GTest
        GTest::Main
        nimcp
        pthread
        m
)

target_compile_features(regression_myelin_math_performance PRIVATE cxx_std_17)

target_include_directories(regression_myelin_math_performance
    PRIVATE
        ${CMAKE_SOURCE_DIR}/include
        ${CMAKE_SOURCE_DIR}/src
)

add_test(NAME regression_myelin_math_performance
         COMMAND regression_myelin_math_performance)

set_tests_properties(regression_myelin_math_performance
    PROPERTIES
        TIMEOUT 300
        LABELS "regression;glial;myelin;biophysics;math;performance"
)
