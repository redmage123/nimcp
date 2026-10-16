# Medulla Oblongata Unit Tests
# Tests for arousal state, protective cutoff, circadian rhythm, brainstem coupling, and medulla orchestrator

# Find Google Test
find_package(GTest REQUIRED)

# Common includes
set(MEDULLA_TEST_INCLUDES
    ${CMAKE_SOURCE_DIR}/include
    ${CMAKE_SOURCE_DIR}/test
    ${GTEST_INCLUDE_DIRS}
)

# Common libraries
set(MEDULLA_TEST_LIBS
    nimcp
    GTest::GTest
    GTest::Main
    pthread
)

# Arousal State Tests
add_executable(unit_core_medulla_arousal_state
    test_arousal_state.cpp
)
target_include_directories(unit_core_medulla_arousal_state PRIVATE ${MEDULLA_TEST_INCLUDES})
target_link_libraries(unit_core_medulla_arousal_state ${MEDULLA_TEST_LIBS})
add_test(NAME unit_core_medulla_arousal_state COMMAND unit_core_medulla_arousal_state --gtest_brief=1)

# Protective Cutoff Tests
add_executable(unit_core_medulla_protective_cutoff
    test_protective_cutoff.cpp
)
target_include_directories(unit_core_medulla_protective_cutoff PRIVATE ${MEDULLA_TEST_INCLUDES})
target_link_libraries(unit_core_medulla_protective_cutoff ${MEDULLA_TEST_LIBS})
add_test(NAME unit_core_medulla_protective_cutoff COMMAND unit_core_medulla_protective_cutoff --gtest_brief=1)

# Circadian Rhythm Tests
add_executable(unit_core_medulla_circadian
    test_circadian.cpp
)
target_include_directories(unit_core_medulla_circadian PRIVATE ${MEDULLA_TEST_INCLUDES})
target_link_libraries(unit_core_medulla_circadian ${MEDULLA_TEST_LIBS})
add_test(NAME unit_core_medulla_circadian COMMAND unit_core_medulla_circadian --gtest_brief=1)

# Brainstem Coupling Tests
add_executable(unit_core_medulla_brainstem_coupling
    test_brainstem_coupling.cpp
)
target_include_directories(unit_core_medulla_brainstem_coupling PRIVATE ${MEDULLA_TEST_INCLUDES})
target_link_libraries(unit_core_medulla_brainstem_coupling ${MEDULLA_TEST_LIBS})
add_test(NAME unit_core_medulla_brainstem_coupling COMMAND unit_core_medulla_brainstem_coupling --gtest_brief=1)

# Medulla Orchestrator Tests
add_executable(unit_core_medulla_orchestrator
    test_medulla.cpp
)
target_include_directories(unit_core_medulla_orchestrator PRIVATE ${MEDULLA_TEST_INCLUDES})
target_link_libraries(unit_core_medulla_orchestrator ${MEDULLA_TEST_LIBS})
add_test(NAME unit_core_medulla_orchestrator COMMAND unit_core_medulla_orchestrator --gtest_brief=1)

# Brain-Medulla Integration Tests
add_executable(unit_core_medulla_brain_integration
    test_brain_medulla_integration.cpp
)
target_include_directories(unit_core_medulla_brain_integration PRIVATE ${MEDULLA_TEST_INCLUDES})
target_link_libraries(unit_core_medulla_brain_integration ${MEDULLA_TEST_LIBS})
add_test(NAME unit_core_medulla_brain_integration COMMAND unit_core_medulla_brain_integration --gtest_brief=1)

# Medulla-Immune Bridge Tests
add_executable(unit_core_medulla_immune_bridge
    test_medulla_immune_bridge.cpp
)
target_include_directories(unit_core_medulla_immune_bridge PRIVATE ${MEDULLA_TEST_INCLUDES})
target_link_libraries(unit_core_medulla_immune_bridge ${MEDULLA_TEST_LIBS})
add_test(NAME unit_core_medulla_immune_bridge COMMAND unit_core_medulla_immune_bridge --gtest_brief=1)

# Medulla-Cerebellum Bridge Tests
add_executable(unit_core_medulla_cerebellum_bridge
    test_medulla_cerebellum_bridge.cpp
)
target_include_directories(unit_core_medulla_cerebellum_bridge PRIVATE ${MEDULLA_TEST_INCLUDES})
target_link_libraries(unit_core_medulla_cerebellum_bridge ${MEDULLA_TEST_LIBS})
add_test(NAME unit_core_medulla_cerebellum_bridge COMMAND unit_core_medulla_cerebellum_bridge --gtest_brief=1)

# Medulla Exception Handling Tests
add_executable(unit_core_medulla_exception_handling
    test_medulla_exception_handling.cpp
)
target_include_directories(unit_core_medulla_exception_handling PRIVATE ${MEDULLA_TEST_INCLUDES})
target_link_libraries(unit_core_medulla_exception_handling ${MEDULLA_TEST_LIBS})
add_test(NAME unit_core_medulla_exception_handling COMMAND unit_core_medulla_exception_handling --gtest_brief=1)

message(STATUS "Medulla unit tests configured")
