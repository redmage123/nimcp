# ============================================================================
# LNN Unit Tests CMakeLists.txt
# ============================================================================
# WHAT: Build configuration for LNN unit tests
# WHY:  Verify correctness of liquid neural network components
# HOW:  Build GoogleTest executables for each LNN module
# ============================================================================

# Guard: Check GTest availability
if(NOT GTest_FOUND)
    message(STATUS "GTest not found - skipping LNN unit tests")
    return()
endif()

# Common link libraries for all LNN tests
set(LNN_TEST_LIBS
    nimcp_lnn
    nimcp
    GTest::gtest
    GTest::gtest_main
    Threads::Threads
    m
)

# Common include directories
set(LNN_TEST_INCLUDES
    ${CMAKE_SOURCE_DIR}/include
    ${CMAKE_SOURCE_DIR}/include/lnn
)

# ============================================================================
# Test 1: LNN Configuration
# ============================================================================
if(EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/test_lnn_config.cpp)
    add_executable(unit_lnn_test_lnn_config
        test_lnn_config.cpp
    )

    target_link_libraries(unit_lnn_test_lnn_config
        PRIVATE ${LNN_TEST_LIBS}
    )

    target_include_directories(unit_lnn_test_lnn_config
        PRIVATE ${LNN_TEST_INCLUDES}
    )

    target_compile_features(unit_lnn_test_lnn_config PRIVATE cxx_std_17)

    gtest_discover_tests(unit_lnn_test_lnn_config
        WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
        PROPERTIES
            LABELS "unit;lnn;config"
            TIMEOUT 60
    )

    message(STATUS "  Added LNN unit test: test_lnn_config")
endif()

# ============================================================================
# Test 2: LNN Neuron
# ============================================================================
if(EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/test_lnn_neuron.cpp)
    add_executable(unit_lnn_test_lnn_neuron
        test_lnn_neuron.cpp
    )

    target_link_libraries(unit_lnn_test_lnn_neuron
        PRIVATE ${LNN_TEST_LIBS}
    )

    target_include_directories(unit_lnn_test_lnn_neuron
        PRIVATE ${LNN_TEST_INCLUDES}
    )

    target_compile_features(unit_lnn_test_lnn_neuron PRIVATE cxx_std_17)

    gtest_discover_tests(unit_lnn_test_lnn_neuron
        WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
        PROPERTIES
            LABELS "unit;lnn;neuron"
            TIMEOUT 60
    )

    message(STATUS "  Added LNN unit test: test_lnn_neuron")
endif()

# ============================================================================
# Test 3: LNN Layer
# ============================================================================
if(EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/test_lnn_layer.cpp)
    add_executable(unit_lnn_test_lnn_layer
        test_lnn_layer.cpp
    )

    target_link_libraries(unit_lnn_test_lnn_layer
        PRIVATE ${LNN_TEST_LIBS}
    )

    target_include_directories(unit_lnn_test_lnn_layer
        PRIVATE ${LNN_TEST_INCLUDES}
    )

    target_compile_features(unit_lnn_test_lnn_layer PRIVATE cxx_std_17)

    gtest_discover_tests(unit_lnn_test_lnn_layer
        WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
        PROPERTIES
            LABELS "unit;lnn;layer"
            TIMEOUT 120
    )

    message(STATUS "  Added LNN unit test: test_lnn_layer")
endif()

# ============================================================================
# Test 4: LNN Sleep Bridge
# ============================================================================
if(EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/test_lnn_sleep_bridge.cpp)
    add_executable(unit_lnn_test_lnn_sleep_bridge
        test_lnn_sleep_bridge.cpp
    )

    target_link_libraries(unit_lnn_test_lnn_sleep_bridge
        PRIVATE ${LNN_TEST_LIBS}
    )

    target_include_directories(unit_lnn_test_lnn_sleep_bridge
        PRIVATE ${LNN_TEST_INCLUDES}
    )

    target_compile_features(unit_lnn_test_lnn_sleep_bridge PRIVATE cxx_std_17)

    gtest_discover_tests(unit_lnn_test_lnn_sleep_bridge
        WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
        PROPERTIES
            LABELS "unit;lnn;sleep_bridge"
            TIMEOUT 60
    )

    message(STATUS "  Added LNN unit test: test_lnn_sleep_bridge")
endif()

# ============================================================================
# Test 5: LNN Substrate Adapter (Phase 2 biological-substrate wiring)
# ============================================================================
if(EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/test_lnn_substrate_adapter.cpp)
    add_executable(unit_lnn_test_lnn_substrate_adapter
        test_lnn_substrate_adapter.cpp
    )

    target_link_libraries(unit_lnn_test_lnn_substrate_adapter
        PRIVATE ${LNN_TEST_LIBS}
    )

    target_include_directories(unit_lnn_test_lnn_substrate_adapter
        PRIVATE ${LNN_TEST_INCLUDES}
    )

    target_compile_features(unit_lnn_test_lnn_substrate_adapter PRIVATE cxx_std_17)

    gtest_discover_tests(unit_lnn_test_lnn_substrate_adapter
        WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
        PROPERTIES
            LABELS "unit;lnn;substrate"
            TIMEOUT 120
    )

    message(STATUS "  Added LNN unit test: test_lnn_substrate_adapter")
endif()

# ============================================================================
# Test 6: LNN Thalamic Adapter (Phase 2 — attention-gated input modulation)
# ============================================================================
if(EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/test_lnn_thalamic_adapter.cpp)
    add_executable(unit_lnn_test_lnn_thalamic_adapter
        test_lnn_thalamic_adapter.cpp
    )

    target_link_libraries(unit_lnn_test_lnn_thalamic_adapter
        PRIVATE ${LNN_TEST_LIBS}
    )

    target_include_directories(unit_lnn_test_lnn_thalamic_adapter
        PRIVATE ${LNN_TEST_INCLUDES}
    )

    target_compile_features(unit_lnn_test_lnn_thalamic_adapter PRIVATE cxx_std_17)

    gtest_discover_tests(unit_lnn_test_lnn_thalamic_adapter
        WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
        PROPERTIES
            LABELS "unit;lnn;thalamic;adapter"
            TIMEOUT 120
    )

    message(STATUS "  Added LNN unit test: test_lnn_thalamic_adapter")
endif()

# ============================================================================
# Test 7: HNN Rayleigh Dissipation (biologically-motivated, substrate-coupled)
# ============================================================================
if(EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/test_hnn_dissipation.cpp)
    add_executable(unit_lnn_test_hnn_dissipation
        test_hnn_dissipation.cpp
    )

    target_link_libraries(unit_lnn_test_hnn_dissipation
        PRIVATE ${LNN_TEST_LIBS}
    )

    target_include_directories(unit_lnn_test_hnn_dissipation
        PRIVATE ${LNN_TEST_INCLUDES}
    )

    target_compile_features(unit_lnn_test_hnn_dissipation PRIVATE cxx_std_17)

    gtest_discover_tests(unit_lnn_test_hnn_dissipation
        WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
        PROPERTIES
            LABELS "unit;lnn;hnn;dissipation"
            TIMEOUT 60
    )

    message(STATUS "  Added LNN unit test: test_hnn_dissipation")
endif()

# ============================================================================
# Future tests (uncomment as source files are added):
# ============================================================================
# - test_lnn_network.cpp
# - test_lnn_ode.cpp
# - test_lnn_wiring.cpp
# - test_lnn_gradient.cpp
# - test_lnn_training.cpp
# - test_lnn_bio_async.cpp
# - test_lnn_immune.cpp
# - test_lnn_parallel.cpp

# Exception handling tests
if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/exception/CMakeLists.txt")
    add_subdirectory(exception)
endif()
