# =============================================================================
# Unit Tests for SNN Exception Handling
# =============================================================================
# WHAT: CMakeLists.txt for SNN exception handling unit tests
# WHY:  Test exception creation, dispatch, and handler chain for SNN modules
# HOW:  Build test executable and register with CTest
#
# TESTS INCLUDED:
# - Exception creation for all SNN module types
# - Error code to exception mapping
# - Handler chain dispatch
# - Recovery strategy determination
# - Thread safety
#
# @author NIMCP Development Team
# @date 2026-01-20
# =============================================================================

# SNN Exception Handling Unit Tests
add_executable(unit_snn_exception_handling
    test_snn_exception_handling.cpp
)

target_include_directories(unit_snn_exception_handling
    PRIVATE
        ${CMAKE_SOURCE_DIR}/include
)

target_link_libraries(unit_snn_exception_handling
    PRIVATE
        nimcp
        GTest::gtest
        GTest::gtest_main
        Threads::Threads
        m
)

target_compile_features(unit_snn_exception_handling PRIVATE cxx_std_17)

# Register with CTest using gtest_discover_tests for automatic test discovery
gtest_discover_tests(unit_snn_exception_handling
    PROPERTIES
        TIMEOUT 120
        LABELS "unit;snn;exception"
)

# Alternative manual test registration (if gtest_discover_tests has issues)
# add_test(
#     NAME unit_snn_exception_handling
#     COMMAND unit_snn_exception_handling --gtest_brief=1
# )
#
# set_tests_properties(unit_snn_exception_handling PROPERTIES
#     TIMEOUT 120
#     LABELS "unit;snn;exception"
# )

# =============================================================================
# SNN Bridge Exception Unit Tests
# =============================================================================
# WHAT: Test NIMCP_THROW_TO_IMMUNE exception handling for SNN bridge bio_async functions
# WHY:  Verify proper error reporting to immune system for NULL pointer conditions
# HOW:  Test disconnect_bio_async and is_bio_async_connected functions
#
# BRIDGES TESTED:
# - nimcp_snn_curiosity_bridge
# - nimcp_snn_empathetic_bridge
# - nimcp_snn_free_energy_bridge
# - nimcp_snn_grief_bridge
# - nimcp_snn_joy_bridge
# - nimcp_snn_love_bridge
# - nimcp_snn_medulla_bridge
# - nimcp_snn_reasoning_bridge
# =============================================================================

if(EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/test_snn_bridge_exception.cpp)
    add_executable(unit_snn_bridge_exception
        test_snn_bridge_exception.cpp
    )

    target_include_directories(unit_snn_bridge_exception
        PRIVATE
            ${CMAKE_SOURCE_DIR}/include
            ${CMAKE_SOURCE_DIR}/include/snn
            ${CMAKE_SOURCE_DIR}/test
    )

    target_link_libraries(unit_snn_bridge_exception
        PRIVATE
            nimcp_snn
            nimcp
            GTest::gtest
            GTest::gtest_main
            Threads::Threads
            m
    )

    target_compile_features(unit_snn_bridge_exception PRIVATE cxx_std_17)

    gtest_discover_tests(unit_snn_bridge_exception
        PROPERTIES
            TIMEOUT 120
            LABELS "unit;snn;bridges;exception"
    )

    message(STATUS "  Added SNN bridge exception unit test: test_snn_bridge_exception")
endif()
