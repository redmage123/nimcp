# NIMCP Shared Substrate-Effects Unit Tests
# Tests for core/substrate/nimcp_substrate_effects (shared helper consumed by
# all per-network adapters: SNN, LNN, CNN, FNO, HNN).

find_package(GTest REQUIRED)

add_executable(unit_core_substrate_test_substrate_effects
    test_substrate_effects.cpp
)

target_include_directories(unit_core_substrate_test_substrate_effects
    PRIVATE
        ${CMAKE_SOURCE_DIR}/include
        ${CMAKE_SOURCE_DIR}/src
        ${GTEST_INCLUDE_DIRS}
)

target_link_libraries(unit_core_substrate_test_substrate_effects
    PRIVATE
        nimcp
        GTest::GTest
        GTest::Main
        pthread
)

target_compile_features(unit_core_substrate_test_substrate_effects
    PRIVATE cxx_std_17)

add_test(NAME unit_core_substrate_test_substrate_effects
         COMMAND unit_core_substrate_test_substrate_effects --gtest_brief=1)

set_tests_properties(unit_core_substrate_test_substrate_effects
    PROPERTIES
        TIMEOUT 60
        LABELS "unit;core;substrate"
)
