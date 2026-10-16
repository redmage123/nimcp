# Unit tests for plasticity-immune integrations

# Neuromodulator-immune test
add_executable(unit_plasticity_neuromodulator_immune
    test_neuromodulator_immune.cpp
)

target_link_libraries(unit_plasticity_neuromodulator_immune
    nimcp
    gtest
    gtest_main
    pthread
)

target_include_directories(unit_plasticity_neuromodulator_immune PRIVATE
    ${CMAKE_SOURCE_DIR}/include
)

add_test(
    NAME unit_plasticity_neuromodulator_immune
    COMMAND unit_plasticity_neuromodulator_immune --gtest_brief=1
)

set_tests_properties(unit_plasticity_neuromodulator_immune PROPERTIES
    TIMEOUT 60
    LABELS "unit;plasticity;immune;neuromodulator"
)

# Homeostatic-immune test
add_executable(unit_plasticity_homeostatic_immune
    test_homeostatic_immune_integration.cpp
)

target_link_libraries(unit_plasticity_homeostatic_immune
    nimcp
    gtest
    gtest_main
    pthread
)

target_include_directories(unit_plasticity_homeostatic_immune PRIVATE
    ${CMAKE_SOURCE_DIR}/include
)

add_test(
    NAME unit_plasticity_homeostatic_immune
    COMMAND unit_plasticity_homeostatic_immune --gtest_brief=1
)

set_tests_properties(unit_plasticity_homeostatic_immune PROPERTIES
    TIMEOUT 60
    LABELS "unit;plasticity;immune;homeostatic"
)

# Eligibility-immune test
add_executable(unit_plasticity_eligibility_immune
    test_eligibility_immune_integration.cpp
)

target_link_libraries(unit_plasticity_eligibility_immune
    nimcp
    gtest
    gtest_main
    pthread
)

target_include_directories(unit_plasticity_eligibility_immune PRIVATE
    ${CMAKE_SOURCE_DIR}/include
)

add_test(
    NAME unit_plasticity_eligibility_immune
    COMMAND unit_plasticity_eligibility_immune --gtest_brief=1
)

set_tests_properties(unit_plasticity_eligibility_immune PROPERTIES
    TIMEOUT 60
    LABELS "unit;plasticity;immune;eligibility"
)

# Dendritic-immune test
add_executable(unit_plasticity_dendritic_immune
    test_dendritic_immune_integration.cpp
)

target_link_libraries(unit_plasticity_dendritic_immune
    nimcp
    gtest
    gtest_main
    pthread
)

target_include_directories(unit_plasticity_dendritic_immune PRIVATE
    ${CMAKE_SOURCE_DIR}/include
)

add_test(
    NAME unit_plasticity_dendritic_immune
    COMMAND unit_plasticity_dendritic_immune --gtest_brief=1
)

set_tests_properties(unit_plasticity_dendritic_immune PROPERTIES
    TIMEOUT 60
    LABELS "unit;plasticity;immune;dendritic"
)
