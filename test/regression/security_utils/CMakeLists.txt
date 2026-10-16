# ============================================================================
# Security & Utils Walkthrough P1/P2/P3 Regression Tests
# ============================================================================
# WHAT: Regression tests for P1-P3 walkthrough fixes in Security & Utils modules
# WHY:  Prevent reintroduction of overflow, false positive, error code,
#        volatile, and thread-safety issues
# HOW:  GTest with targeted verification of each fix
# ============================================================================

add_executable(regression_security_utils_walkthrough_fixes
    test_security_utils_walkthrough_fixes.cpp
)
target_link_libraries(regression_security_utils_walkthrough_fixes
    PRIVATE
        GTest::GTest
        GTest::Main
        nimcp
        pthread
        m
)
target_include_directories(regression_security_utils_walkthrough_fixes
    PRIVATE
        ${CMAKE_SOURCE_DIR}/include
        ${CMAKE_SOURCE_DIR}/include/security
        ${CMAKE_SOURCE_DIR}/include/security/lgss/cognitive
        ${CMAKE_SOURCE_DIR}/include/utils
        ${CMAKE_SOURCE_DIR}/include/utils/thread
        ${CMAKE_SOURCE_DIR}/include/utils/containers
        ${CMAKE_SOURCE_DIR}/include/utils/tensor
        ${CMAKE_SOURCE_DIR}/include/utils/memory
        ${CMAKE_SOURCE_DIR}/include/utils/exception
        ${CMAKE_SOURCE_DIR}/include/utils/error
        ${CMAKE_SOURCE_DIR}/src
        ${CMAKE_SOURCE_DIR}/test
        ${CMAKE_SOURCE_DIR}/test/utils
)
target_compile_features(regression_security_utils_walkthrough_fixes PRIVATE cxx_std_17)
add_test(NAME regression_security_utils_walkthrough_fixes
         COMMAND regression_security_utils_walkthrough_fixes)
set_tests_properties(regression_security_utils_walkthrough_fixes PROPERTIES
    LABELS "regression;security;utils;p1;p2;p3;walkthrough"
    TIMEOUT 120
)
