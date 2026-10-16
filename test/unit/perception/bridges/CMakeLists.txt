# Cochlea Bridge Unit Tests
# Test coverage for cochlea brain integration bridges

# Cochlea bridge tests
add_executable(unit_perception_bridges_test_cochlea_bridges
    test_cochlea_bridges.cpp
)
target_link_libraries(unit_perception_bridges_test_cochlea_bridges
    PRIVATE
        GTest::GTest
        GTest::Main
        nimcp
        m
        pthread
)
target_include_directories(unit_perception_bridges_test_cochlea_bridges
    PRIVATE
        ${CMAKE_SOURCE_DIR}/include
        ${CMAKE_SOURCE_DIR}/src
)
add_test(NAME unit_perception_bridges_test_cochlea_bridges
         COMMAND unit_perception_bridges_test_cochlea_bridges)
set_tests_properties(unit_perception_bridges_test_cochlea_bridges PROPERTIES
    LABELS "unit;perception;cochlea;bridges"
)
