# Swarm Immune Bridge Unit Tests

add_executable(unit_swarm_immune_bridges
    test_swarm_immune_bridges.cpp
)
target_link_libraries(unit_swarm_immune_bridges
    PRIVATE
        GTest::GTest
        GTest::Main
        nimcp
)
target_include_directories(unit_swarm_immune_bridges
    PRIVATE
        ${CMAKE_SOURCE_DIR}/include
        ${CMAKE_SOURCE_DIR}/src
        ${CMAKE_SOURCE_DIR}/src/swarm/immune
        ${CMAKE_SOURCE_DIR}/src/cognitive/immune
        ${CMAKE_SOURCE_DIR}/src/utils/thread
)
add_test(NAME unit_swarm_immune_bridges
         COMMAND unit_swarm_immune_bridges)
set_tests_properties(unit_swarm_immune_bridges PROPERTIES LABELS "unit")
