# Portia-Immune Bridge Unit Tests

# Portia Immune Bridges Unit Tests (Attention, Learning, Sensor Fusion)
add_executable(unit_portia_immune_bridges
    test_portia_immune_bridges.cpp
)
target_link_libraries(unit_portia_immune_bridges
    PRIVATE
        GTest::GTest
        GTest::Main
        nimcp
)
target_include_directories(unit_portia_immune_bridges
    PRIVATE
        ${CMAKE_SOURCE_DIR}/include
        ${CMAKE_SOURCE_DIR}/src
        ${CMAKE_SOURCE_DIR}/src/utils/memory
        ${CMAKE_SOURCE_DIR}/src/utils/logging
        ${CMAKE_SOURCE_DIR}/src/cognitive/immune
        ${CMAKE_SOURCE_DIR}/src/portia
)
add_test(NAME unit_portia_immune_bridges
         COMMAND unit_portia_immune_bridges)
set_tests_properties(unit_portia_immune_bridges PROPERTIES LABELS "unit")
