# ============================================================================
# NIMCP Axon Unit Tests
# ============================================================================

# Unit test for axon module
add_executable(unit_core_axon_test_axon
    test_axon.cpp
    ${CMAKE_SOURCE_DIR}/src/core/axon/nimcp_axon.c
)

target_link_libraries(unit_core_axon_test_axon
    PRIVATE
        GTest::GTest
        GTest::Main
        nimcp
        pthread
)

target_compile_features(unit_core_axon_test_axon PRIVATE cxx_std_17)

target_include_directories(unit_core_axon_test_axon
    PRIVATE
        ${CMAKE_SOURCE_DIR}/include
        ${CMAKE_SOURCE_DIR}/src
)

add_test(NAME unit_core_axon_test_axon
         COMMAND unit_core_axon_test_axon)

set_tests_properties(unit_core_axon_test_axon
    PROPERTIES
        TIMEOUT 60
        LABELS "unit;core;axon"
)

# Enhanced unit tests
add_executable(unit_core_axon_test_axon_enhanced
    test_axon_enhanced.cpp
    ${CMAKE_SOURCE_DIR}/src/core/axon/nimcp_axon.c
)

target_link_libraries(unit_core_axon_test_axon_enhanced
    PRIVATE
        GTest::GTest
        GTest::Main
        nimcp
        pthread
)

target_compile_features(unit_core_axon_test_axon_enhanced PRIVATE cxx_std_17)

target_include_directories(unit_core_axon_test_axon_enhanced
    PRIVATE
        ${CMAKE_SOURCE_DIR}/include
        ${CMAKE_SOURCE_DIR}/src
)

add_test(NAME unit_core_axon_test_axon_enhanced
         COMMAND unit_core_axon_test_axon_enhanced)

set_tests_properties(unit_core_axon_test_axon_enhanced
    PROPERTIES
        TIMEOUT 120
        LABELS "unit;core;axon"
)
