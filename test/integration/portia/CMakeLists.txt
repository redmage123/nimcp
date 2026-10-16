#=============================================================================
# CMakeLists.txt - Portia Integration Tests
#=============================================================================
# @file CMakeLists.txt
# @brief Build configuration for Portia spider system integration tests
# @author NIMCP Development Team
# @date 2025-12-08
#=============================================================================

# Portia integration test sources
set(PORTIA_INTEGRATION_TESTS
    test_portia_tier_integration.cpp
    test_portia_power_tier_integration.cpp
    test_portia_planning_attention_integration.cpp
    test_portia_sensor_planning_integration.cpp
    test_portia_learning_classification_integration.cpp
    test_portia_accelerator_tier_integration.cpp
    test_portia_degradation_bio_async_integration.cpp
    test_portia_deception_sensor_integration.cpp
    test_portia_full_system_integration.cpp
    test_portia_swarm_bridge_integration.cpp
    test_portia_swarm_logic_integration.cpp
    test_portia_logic_bridge_integration.cpp
    test_perception_portia_swarm_integration.cpp
    test_cortical_portia_swarm_integration.cpp
)

# Create test executables
foreach(test_source ${PORTIA_INTEGRATION_TESTS})
    # Extract test name (remove .cpp extension)
    get_filename_component(test_name ${test_source} NAME_WE)

    # Create test executable
    add_executable(${test_name} ${test_source})

    # Link against required libraries
    target_link_libraries(${test_name}
        PRIVATE
            # GoogleTest
            GTest::gtest
            GTest::gtest_main
            GTest::gmock

            # Portia library
            nimcp_portia

            # Main nimcp library (contains all subsystems including async, platform, validation)
            nimcp

            # Threading
            Threads::Threads
    )

    # Include directories
    target_include_directories(${test_name}
        PRIVATE
            ${CMAKE_SOURCE_DIR}/include
            ${CMAKE_SOURCE_DIR}/include/portia
            ${CMAKE_SOURCE_DIR}/include/async
            ${CMAKE_SOURCE_DIR}/include/utils
            ${CMAKE_SOURCE_DIR}/include/utils/platform
            ${CMAKE_SOURCE_DIR}/include/utils/validation
            ${CMAKE_SOURCE_DIR}/include/swarm
            ${CMAKE_SOURCE_DIR}/include/middleware
            ${CMAKE_SOURCE_DIR}/include/middleware/training
    )

    # Compiler flags
    target_compile_features(${test_name} PRIVATE cxx_std_11)

    if(MSVC)
        target_compile_options(${test_name} PRIVATE /W4)
    else()
        target_compile_options(${test_name} PRIVATE -Wall -Wextra -Wpedantic)
    endif()

    # Add to CTest
    add_test(
        NAME ${test_name}
        COMMAND ${test_name}
        WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
    )

    # Test properties
    set_tests_properties(${test_name} PROPERTIES
        LABELS "integration;portia"
        TIMEOUT 60
    )

    # Install test executable (optional)
    install(TARGETS ${test_name}
        RUNTIME DESTINATION ${CMAKE_INSTALL_PREFIX}/test/integration/portia
    )
endforeach()

#=============================================================================
# Test Groups for Selective Execution
#=============================================================================

# Create test groups for easier execution
add_custom_target(test_portia_integration
    COMMAND ${CMAKE_CTEST_COMMAND} -L "portia" --output-on-failure
    DEPENDS ${PORTIA_INTEGRATION_TESTS}
    COMMENT "Running Portia integration tests"
)

add_custom_target(integration_portia_tier
    COMMAND ${CMAKE_CTEST_COMMAND} -R "portia_tier|portia_power_tier|portia_accelerator_tier" --output-on-failure
    COMMENT "Running Portia tier-related integration tests"
)

add_custom_target(integration_portia_sensors
    COMMAND ${CMAKE_CTEST_COMMAND} -R "portia_sensor|portia_deception" --output-on-failure
    COMMENT "Running Portia sensor integration tests"
)

add_custom_target(integration_portia_learning
    COMMAND ${CMAKE_CTEST_COMMAND} -R "portia_learning" --output-on-failure
    COMMENT "Running Portia learning integration tests"
)

add_custom_target(integration_portia_full
    COMMAND ${CMAKE_CTEST_COMMAND} -R "portia_full_system" --output-on-failure
    COMMENT "Running Portia full system integration test"
)

#=============================================================================
# Documentation
#=============================================================================

# Add test documentation comments
set_property(
    TEST test_portia_tier_integration
    PROPERTY DESCRIPTION
    "Tests platform tier detection and switching with real metrics"
)

set_property(
    TEST test_portia_power_tier_integration
    PROPERTY DESCRIPTION
    "Tests power profile changes and battery depletion effects"
)

set_property(
    TEST test_portia_planning_attention_integration
    PROPERTY DESCRIPTION
    "Tests planning and attention resource coordination"
)

set_property(
    TEST test_portia_sensor_planning_integration
    PROPERTY DESCRIPTION
    "Tests sensor fusion feeding planning system"
)

set_property(
    TEST test_portia_learning_classification_integration
    PROPERTY DESCRIPTION
    "Tests learned associations affecting classification"
)

set_property(
    TEST test_portia_accelerator_tier_integration
    PROPERTY DESCRIPTION
    "Tests GPU/accelerator availability affecting tier selection"
)

set_property(
    TEST test_portia_degradation_bio_async_integration
    PROPERTY DESCRIPTION
    "Tests degradation event broadcasting via bio-async"
)

set_property(
    TEST test_portia_deception_sensor_integration
    PROPERTY DESCRIPTION
    "Tests stealth and mimicry affecting sensor outputs"
)

set_property(
    TEST test_portia_full_system_integration
    PROPERTY DESCRIPTION
    "Tests complete Portia system with all subsystems"
)

set_property(
    TEST test_portia_swarm_bridge_integration
    PROPERTY DESCRIPTION
    "Tests Portia-Swarm bridge bidirectional integration and coordination"
)

#=============================================================================
# Portia-Swarm Bridge Test Group
#=============================================================================

add_custom_target(integration_portia_swarm
    COMMAND ${CMAKE_CTEST_COMMAND} -R "portia_swarm_bridge" --output-on-failure
    COMMENT "Running Portia-Swarm bridge integration tests"
)

#=============================================================================
# Platform-Specific Configuration
#=============================================================================

if(UNIX AND NOT APPLE)
    # Linux-specific settings
    foreach(test_source ${PORTIA_INTEGRATION_TESTS})
        get_filename_component(test_name ${test_source} NAME_WE)
        target_link_libraries(${test_name} PRIVATE rt pthread)
    endforeach()
elseif(APPLE)
    # macOS-specific settings
    foreach(test_source ${PORTIA_INTEGRATION_TESTS})
        get_filename_component(test_name ${test_source} NAME_WE)
        target_link_libraries(${test_name} PRIVATE pthread)
    endforeach()
endif()

#=============================================================================
# Code Coverage (if enabled)
#=============================================================================

if(ENABLE_COVERAGE)
    foreach(test_source ${PORTIA_INTEGRATION_TESTS})
        get_filename_component(test_name ${test_source} NAME_WE)
        target_compile_options(${test_name} PRIVATE --coverage)
        target_link_libraries(${test_name} PRIVATE gcov)
    endforeach()
endif()

#=============================================================================
# Sanitizers (if enabled)
#=============================================================================

if(ENABLE_ASAN)
    foreach(test_source ${PORTIA_INTEGRATION_TESTS})
        get_filename_component(test_name ${test_source} NAME_WE)
        target_compile_options(${test_name} PRIVATE -fsanitize=address -fno-omit-frame-pointer)
        target_link_options(${test_name} PRIVATE -fsanitize=address)
    endforeach()
endif()

if(ENABLE_UBSAN)
    foreach(test_source ${PORTIA_INTEGRATION_TESTS})
        get_filename_component(test_name ${test_source} NAME_WE)
        target_compile_options(${test_name} PRIVATE -fsanitize=undefined)
        target_link_options(${test_name} PRIVATE -fsanitize=undefined)
    endforeach()
endif()

#=============================================================================
# Test Summary
#=============================================================================

message(STATUS "Portia Integration Tests Configuration:")
message(STATUS "  Number of tests: ${CMAKE_CURRENT_LIST_FILE}")
message(STATUS "  Test directory: ${CMAKE_CURRENT_SOURCE_DIR}")
message(STATUS "  Build type: ${CMAKE_BUILD_TYPE}")

# Print test list
message(STATUS "Portia integration tests:")
foreach(test_source ${PORTIA_INTEGRATION_TESTS})
    get_filename_component(test_name ${test_source} NAME_WE)
    message(STATUS "  - ${test_name}")
endforeach()
