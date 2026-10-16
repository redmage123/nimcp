# ============================================================================
# Raphe Nuclei Unit Tests
# ============================================================================
# WHAT: Unit tests for Raphe Nuclei (Serotonin/Mood) module
# WHY:  Verify 5-HT release, mood regulation, impulse control, temporal discounting, adapter
# HOW:  GTest-based tests for all Raphe components

message(STATUS "Adding Raphe Nuclei unit tests...")

# Raphe Core test
add_executable(unit_raphe_test_core
    test_raphe_core.cpp
)

target_include_directories(unit_raphe_test_core PRIVATE
    ${PROJECT_SOURCE_DIR}/include
    ${PROJECT_SOURCE_DIR}/src
)

target_link_libraries(unit_raphe_test_core
    nimcp
    GTest::gtest
    GTest::gtest_main
)

add_test(NAME unit_raphe_test_core
         COMMAND unit_raphe_test_core)

# Serotonin Release test
add_executable(unit_raphe_test_serotonin
    test_serotonin_release.cpp
)

target_include_directories(unit_raphe_test_serotonin PRIVATE
    ${PROJECT_SOURCE_DIR}/include
    ${PROJECT_SOURCE_DIR}/src
)

target_link_libraries(unit_raphe_test_serotonin
    nimcp
    GTest::gtest
    GTest::gtest_main
)

add_test(NAME unit_raphe_test_serotonin
         COMMAND unit_raphe_test_serotonin)

# Mood Regulation test
add_executable(unit_raphe_test_mood
    test_mood_regulation.cpp
)

target_include_directories(unit_raphe_test_mood PRIVATE
    ${PROJECT_SOURCE_DIR}/include
    ${PROJECT_SOURCE_DIR}/src
)

target_link_libraries(unit_raphe_test_mood
    nimcp
    GTest::gtest
    GTest::gtest_main
)

add_test(NAME unit_raphe_test_mood
         COMMAND unit_raphe_test_mood)

# Impulse Control test
add_executable(unit_raphe_test_impulse
    test_impulse_control.cpp
)

target_include_directories(unit_raphe_test_impulse PRIVATE
    ${PROJECT_SOURCE_DIR}/include
    ${PROJECT_SOURCE_DIR}/src
)

target_link_libraries(unit_raphe_test_impulse
    nimcp
    GTest::gtest
    GTest::gtest_main
)

add_test(NAME unit_raphe_test_impulse
         COMMAND unit_raphe_test_impulse)

# Temporal Discounting test
add_executable(unit_raphe_test_temporal
    test_temporal_discounting.cpp
)

target_include_directories(unit_raphe_test_temporal PRIVATE
    ${PROJECT_SOURCE_DIR}/include
    ${PROJECT_SOURCE_DIR}/src
)

target_link_libraries(unit_raphe_test_temporal
    nimcp
    GTest::gtest
    GTest::gtest_main
)

add_test(NAME unit_raphe_test_temporal
         COMMAND unit_raphe_test_temporal)

# Raphe Adapter test (with training integration)
add_executable(unit_raphe_test_adapter
    test_raphe_adapter.cpp
)

target_include_directories(unit_raphe_test_adapter PRIVATE
    ${PROJECT_SOURCE_DIR}/include
    ${PROJECT_SOURCE_DIR}/src
)

target_link_libraries(unit_raphe_test_adapter
    nimcp
    GTest::gtest
    GTest::gtest_main
)

add_test(NAME unit_raphe_test_adapter
         COMMAND unit_raphe_test_adapter)
