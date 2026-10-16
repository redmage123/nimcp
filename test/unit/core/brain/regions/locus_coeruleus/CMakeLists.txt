# ============================================================================
# Locus Coeruleus Unit Tests
# ============================================================================
# WHAT: Unit tests for Locus Coeruleus (LC) module
# WHY:  Verify NE release, novelty detection, arousal modulation, and adapter
# HOW:  GTest-based tests for all LC components

message(STATUS "Adding Locus Coeruleus unit tests...")

# LC Core test
add_executable(unit_lc_test_core
    test_lc_core.cpp
)

target_include_directories(unit_lc_test_core PRIVATE
    ${PROJECT_SOURCE_DIR}/include
    ${PROJECT_SOURCE_DIR}/src
)

target_link_libraries(unit_lc_test_core
    nimcp
    GTest::gtest
    GTest::gtest_main
)

add_test(NAME unit_lc_test_core
         COMMAND unit_lc_test_core)

# NE Release test
add_executable(unit_lc_test_ne_release
    test_ne_release.cpp
)

target_include_directories(unit_lc_test_ne_release PRIVATE
    ${PROJECT_SOURCE_DIR}/include
    ${PROJECT_SOURCE_DIR}/src
)

target_link_libraries(unit_lc_test_ne_release
    nimcp
    GTest::gtest
    GTest::gtest_main
)

add_test(NAME unit_lc_test_ne_release
         COMMAND unit_lc_test_ne_release)

# Novelty Detection test
add_executable(unit_lc_test_novelty_detection
    test_novelty_detection.cpp
)

target_include_directories(unit_lc_test_novelty_detection PRIVATE
    ${PROJECT_SOURCE_DIR}/include
    ${PROJECT_SOURCE_DIR}/src
)

target_link_libraries(unit_lc_test_novelty_detection
    nimcp
    GTest::gtest
    GTest::gtest_main
)

add_test(NAME unit_lc_test_novelty_detection
         COMMAND unit_lc_test_novelty_detection)

# Arousal Modulation test
add_executable(unit_lc_test_arousal_modulation
    test_arousal_modulation.cpp
)

target_include_directories(unit_lc_test_arousal_modulation PRIVATE
    ${PROJECT_SOURCE_DIR}/include
    ${PROJECT_SOURCE_DIR}/src
)

target_link_libraries(unit_lc_test_arousal_modulation
    nimcp
    GTest::gtest
    GTest::gtest_main
)

add_test(NAME unit_lc_test_arousal_modulation
         COMMAND unit_lc_test_arousal_modulation)

# LC Adapter test
add_executable(unit_lc_test_adapter
    test_lc_adapter.cpp
)

target_include_directories(unit_lc_test_adapter PRIVATE
    ${PROJECT_SOURCE_DIR}/include
    ${PROJECT_SOURCE_DIR}/src
)

target_link_libraries(unit_lc_test_adapter
    nimcp
    GTest::gtest
    GTest::gtest_main
)

add_test(NAME unit_lc_test_adapter
         COMMAND unit_lc_test_adapter)

# LC Math Fix test (approach_target delta bug)
add_executable(unit_lc_test_math
    test_locus_coeruleus_math.cpp
)

target_include_directories(unit_lc_test_math PRIVATE
    ${PROJECT_SOURCE_DIR}/include
    ${PROJECT_SOURCE_DIR}/src
)

target_link_libraries(unit_lc_test_math
    nimcp
    GTest::gtest
    GTest::gtest_main
)

add_test(NAME unit_lc_test_math
         COMMAND unit_lc_test_math)
