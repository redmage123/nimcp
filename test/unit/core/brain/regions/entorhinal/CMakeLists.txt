# Entorhinal Cortex Unit Tests
# Phase 5: Memory Systems (BR-5)
# Grid cells, path integration, spatial memory, hippocampal gateway

macro(add_entorhinal_test TEST_NAME)
    add_executable(unit_core_brain_regions_entorhinal_${TEST_NAME}
        ${TEST_NAME}.cpp
    )
    target_link_libraries(unit_core_brain_regions_entorhinal_${TEST_NAME}
        PRIVATE
            GTest::GTest
            GTest::Main
            nimcp
            m
            pthread
    )
    add_test(NAME unit_core_brain_regions_entorhinal_${TEST_NAME}
             COMMAND unit_core_brain_regions_entorhinal_${TEST_NAME})
endmacro()

add_entorhinal_test(test_entorhinal_core)
add_entorhinal_test(test_grid_cells)
add_entorhinal_test(test_border_cells)
add_entorhinal_test(test_hd_cells)
add_entorhinal_test(test_path_integration)
add_entorhinal_test(test_memory_gateway)
add_entorhinal_test(test_bridges)
add_entorhinal_test(test_hypothalamus_bridge)
add_entorhinal_test(test_omni_bridge)
add_entorhinal_test(test_brain_init_bridge)
