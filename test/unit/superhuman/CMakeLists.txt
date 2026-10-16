# ============================================================================
# Superhuman Module Unit Tests
# ============================================================================
# WHAT: Tests for superhuman enhancement modules
# WHY:  Ensure capability limits, safety boundaries, and proper behavior
# HOW:  GTest framework with comprehensive coverage
#
# MODULES TESTED:
# - Eagle Vision: Enhanced visual acuity
# - Time Dilation: Temporal processing enhancement
# - Savant Mode: Pattern recognition abilities
# - Echolocation: 3D spatial mapping
# - Hyperthymesia: Autobiographical memory
# - Synesthesia: Cross-modal perception
# - Precognition: Pattern prediction
# ============================================================================

# ============================================================================
# Comprehensive Superhuman Module Tests
# ============================================================================
# Tests all superhuman modules with capability limiting, corrigibility,
# alignment verification, safety boundary, and integration tests

add_executable(unit_superhuman_test_superhuman
    test_superhuman.cpp
)
target_link_libraries(unit_superhuman_test_superhuman
    PRIVATE
        GTest::gtest
        GTest::gtest_main
        nimcp
        m
        pthread
)
target_include_directories(unit_superhuman_test_superhuman
    PRIVATE
        ${CMAKE_SOURCE_DIR}/include
        ${CMAKE_SOURCE_DIR}/src
        ${CMAKE_SOURCE_DIR}/src/superhuman
)
target_compile_features(unit_superhuman_test_superhuman PRIVATE cxx_std_17)
add_test(NAME unit_superhuman_test_superhuman
         COMMAND unit_superhuman_test_superhuman)
set_tests_properties(unit_superhuman_test_superhuman PROPERTIES
    TIMEOUT 300
    LABELS "unit;superhuman;capability;safety;integration"
)
