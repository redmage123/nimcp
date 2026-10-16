# Swarm Sleep Bridge Unit Tests

add_executable(unit_swarm_sleep_bridges
    test_swarm_sleep_bridges.cpp
)
target_link_libraries(unit_swarm_sleep_bridges
    PRIVATE
        GTest::GTest
        GTest::Main
        nimcp
)
target_include_directories(unit_swarm_sleep_bridges
    PRIVATE
        ${CMAKE_SOURCE_DIR}/include
        ${CMAKE_SOURCE_DIR}/src
        ${CMAKE_SOURCE_DIR}/src/swarm/sleep
        ${CMAKE_SOURCE_DIR}/src/cognitive
        ${CMAKE_SOURCE_DIR}/src/utils/thread
)
add_test(NAME unit_swarm_sleep_bridges
         COMMAND unit_swarm_sleep_bridges)
set_tests_properties(unit_swarm_sleep_bridges PROPERTIES LABELS "unit")
