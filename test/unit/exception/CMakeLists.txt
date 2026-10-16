#=============================================================================
# CMakeLists.txt - Exception Handling Macro Integration Unit Tests
#=============================================================================
# WHAT: Build tests for exception handling macro integration
# WHY:  Verify NIMCP_CHECK_THROW, security module exceptions, and swarm exceptions
# HOW:  Google Test framework with fixture setup/teardown
#=============================================================================

find_package(GTest QUIET)

if(NOT GTest_FOUND AND NOT TARGET GTest::gtest)
    message(STATUS "GTest not found - skipping exception macro integration tests")
    return()
endif()

#=============================================================================
# Test 1: Exception Macro Behavior Tests
#=============================================================================
# WHAT: Tests for NIMCP_CHECK_THROW macro behavior
# WHY:  Verify correct error code returns and exception notification
#=============================================================================

add_executable(unit_exception_test_macro_behavior
    test_exception_macro_behavior.cpp
)

target_include_directories(unit_exception_test_macro_behavior PRIVATE
    ${CMAKE_SOURCE_DIR}/include
)

target_link_libraries(unit_exception_test_macro_behavior
    nimcp
    GTest::gtest
    GTest::gtest_main
    pthread
    m
)

add_test(
    NAME unit_exception_test_macro_behavior
    COMMAND unit_exception_test_macro_behavior
)

set_tests_properties(unit_exception_test_macro_behavior
    PROPERTIES
    LABELS "unit;exception;macros"
    TIMEOUT 120
)

#=============================================================================
# Test 2: Security Module Exception Macros Tests
#=============================================================================
# WHAT: Tests for security facade and related module exception handling
# WHY:  Verify security exception flow through facades
#=============================================================================

add_executable(unit_exception_test_security_macros
    test_exception_security_macros.cpp
)

target_include_directories(unit_exception_test_security_macros PRIVATE
    ${CMAKE_SOURCE_DIR}/include
)

target_link_libraries(unit_exception_test_security_macros
    nimcp
    GTest::gtest
    GTest::gtest_main
    pthread
    m
)

add_test(
    NAME unit_exception_test_security_macros
    COMMAND unit_exception_test_security_macros
)

set_tests_properties(unit_exception_test_security_macros
    PROPERTIES
    LABELS "unit;exception;security"
    TIMEOUT 120
)

#=============================================================================
# Test 3: Swarm Module Exception Macros Tests
#=============================================================================
# WHAT: Tests for swarm module exception handling
# WHY:  Verify emotional contagion, swarm brain, and proprioception exceptions
#=============================================================================

add_executable(unit_exception_test_swarm_macros
    test_exception_swarm_macros.cpp
)

target_include_directories(unit_exception_test_swarm_macros PRIVATE
    ${CMAKE_SOURCE_DIR}/include
)

target_link_libraries(unit_exception_test_swarm_macros
    nimcp
    GTest::gtest
    GTest::gtest_main
    pthread
    m
)

add_test(
    NAME unit_exception_test_swarm_macros
    COMMAND unit_exception_test_swarm_macros
)

set_tests_properties(unit_exception_test_swarm_macros
    PROPERTIES
    LABELS "unit;exception;swarm"
    TIMEOUT 120
)

#=============================================================================
# Test 4: Security and LGSS Module Exception Macros Tests
#=============================================================================
# WHAT: Tests for LGSS guards and security module exception handling
# WHY:  Verify exception flow through LGSS STDP guards, training guards,
#       autonomic gates, perception input bridges, and key derivation
#=============================================================================

add_executable(unit_exception_test_security_lgss_macros
    test_exception_security_lgss_macros.cpp
)

target_include_directories(unit_exception_test_security_lgss_macros PRIVATE
    ${CMAKE_SOURCE_DIR}/include
)

target_link_libraries(unit_exception_test_security_lgss_macros
    nimcp
    GTest::gtest
    GTest::gtest_main
    pthread
    m
)

add_test(
    NAME unit_exception_test_security_lgss_macros
    COMMAND unit_exception_test_security_lgss_macros
)

set_tests_properties(unit_exception_test_security_lgss_macros
    PROPERTIES
    LABELS "unit;exception;lgss;security"
    TIMEOUT 120
)

#=============================================================================
# Test 5: Swarm Memory Module Exception Macros Tests
#=============================================================================
# WHAT: Tests for swarm memory and cognitive memory exception handling
# WHY:  Verify swarm memory, emotional contagion, energy gossip,
#       temporal replay, and Hopfield memory exception flows
#=============================================================================

add_executable(unit_exception_test_swarm_memory_macros
    test_exception_swarm_memory_macros.cpp
)

target_include_directories(unit_exception_test_swarm_memory_macros PRIVATE
    ${CMAKE_SOURCE_DIR}/include
)

target_link_libraries(unit_exception_test_swarm_memory_macros
    nimcp
    GTest::gtest
    GTest::gtest_main
    pthread
    m
)

add_test(
    NAME unit_exception_test_swarm_memory_macros
    COMMAND unit_exception_test_swarm_memory_macros
)

set_tests_properties(unit_exception_test_swarm_memory_macros
    PROPERTIES
    LABELS "unit;exception;swarm;memory;hopfield;replay"
    TIMEOUT 120
)

#=============================================================================
# Test 6: Comprehensive Exception Macro Tests
#=============================================================================
# WHAT: Comprehensive tests for all exception handling macros
# WHY:  Full coverage of NIMCP_CHECK_THROW, NIMCP_TRY_CATCH, typed exceptions
#=============================================================================

add_executable(unit_exception_test_macro_comprehensive
    test_exception_macro_comprehensive.cpp
)

target_include_directories(unit_exception_test_macro_comprehensive PRIVATE
    ${CMAKE_SOURCE_DIR}/include
)

target_link_libraries(unit_exception_test_macro_comprehensive
    nimcp
    GTest::gtest
    GTest::gtest_main
    pthread
    m
)

target_compile_features(unit_exception_test_macro_comprehensive PRIVATE cxx_std_17)

add_test(
    NAME unit_exception_test_macro_comprehensive
    COMMAND unit_exception_test_macro_comprehensive
)

set_tests_properties(unit_exception_test_macro_comprehensive
    PROPERTIES
    LABELS "unit;exception;macros;comprehensive"
    TIMEOUT 180
)
