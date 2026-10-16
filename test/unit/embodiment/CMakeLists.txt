# ============================================================================
# Embodiment Module Unit Tests
# ============================================================================
# WHAT: Unit tests for Embodiment subsystems
# WHY:  Verify affordance processing, body ownership, embodied simulation,
#       and interoceptive prediction functionality
# HOW:  Test lifecycle, core functionality, and integration for all modules
# ============================================================================

# Embodiment Unit Tests
add_executable(unit_embodiment_test_embodiment
    test_embodiment.cpp
)
target_link_libraries(unit_embodiment_test_embodiment
    PRIVATE
        GTest::GTest
        GTest::Main
        nimcp
        pthread
)
target_include_directories(unit_embodiment_test_embodiment
    PRIVATE
        ${CMAKE_SOURCE_DIR}/include
        ${CMAKE_SOURCE_DIR}/include/embodiment
        ${CMAKE_SOURCE_DIR}/src
        ${CMAKE_SOURCE_DIR}/src/utils/memory
        ${CMAKE_SOURCE_DIR}/src/utils/logging
        ${CMAKE_SOURCE_DIR}/test
        ${CMAKE_SOURCE_DIR}/test/utils
)
target_compile_features(unit_embodiment_test_embodiment PRIVATE cxx_std_17)
add_test(NAME unit_embodiment_test_embodiment
         COMMAND unit_embodiment_test_embodiment)
set_tests_properties(unit_embodiment_test_embodiment PROPERTIES
    LABELS "unit;embodiment"
    TIMEOUT 120
)
