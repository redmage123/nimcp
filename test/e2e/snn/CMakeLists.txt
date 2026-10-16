# ============================================================================
# SNN Bridge E2E Tests CMakeLists.txt
# ============================================================================
# WHAT: Build configuration for SNN bridge end-to-end tests
# WHY:  Test full bridge lifecycle and multi-bridge workflows
# HOW:  Build GoogleTest executable linking all required modules
# ============================================================================

# Guard: Check GTest availability
if(NOT GTest_FOUND)
    message(STATUS "GTest not found - skipping SNN bridge E2E tests")
    return()
endif()

# Common link libraries.
# zlib is required because nimcp_snn calls gzseek/gzread/gzwrite (snn network
# save/load via gzipped .snn sidecar). Without explicit -lz the shared-lib
# linker flag-mismatch rejects the DSO.
find_package(ZLIB QUIET)

set(SNN_E2E_LIBS
    nimcp_snn
    nimcp
    nimcp_middleware
    GTest::gtest
    GTest::gtest_main
    Threads::Threads
    m
)
if(ZLIB_FOUND)
    list(APPEND SNN_E2E_LIBS ZLIB::ZLIB)
else()
    list(APPEND SNN_E2E_LIBS z)
endif()

# Common include directories
set(SNN_E2E_INCLUDES
    ${CMAKE_SOURCE_DIR}/include
    ${CMAKE_SOURCE_DIR}/include/snn
    ${CMAKE_SOURCE_DIR}/test
)

# ============================================================================
# SNN Bridge Exception E2E Test
# ============================================================================
if(EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/test_snn_bridge_exception_e2e.cpp)
    add_executable(e2e_snn_bridge_exception
        test_snn_bridge_exception_e2e.cpp
    )

    target_link_libraries(e2e_snn_bridge_exception
        PRIVATE ${SNN_E2E_LIBS}
    )

    target_include_directories(e2e_snn_bridge_exception
        PRIVATE ${SNN_E2E_INCLUDES}
    )

    target_compile_features(e2e_snn_bridge_exception PRIVATE cxx_std_17)

    target_compile_definitions(e2e_snn_bridge_exception PRIVATE
        NIMCP_TESTING
        NIMCP_E2E_TESTING
    )

    gtest_discover_tests(e2e_snn_bridge_exception
        WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
        PROPERTIES
            LABELS "e2e;snn;bridges;exception"
            TIMEOUT 300
    )

    message(STATUS "  Added SNN bridge exception E2E test: test_snn_bridge_exception_e2e")
endif()

# ============================================================================
# SNN Biophysical Training E2E Test
# ============================================================================
# WHAT: Verifies SNN biophysical stability mechanisms at scale.
# WHY:  The five default-enabled features (AHP, Na/K pump, basket cells,
#       E/I-balanced noise, anti-reward) collectively keep the hierarchical
#       SNN in a HEALTHY firing-rate band under sustained input load.
#       Disabling them causes collapse or saturation — the ablation arm
#       proves the features are doing real work.
# HOW:  Builds a down-scaled (~48K-neuron) hierarchical topology mirroring
#       the 1.8M production SNN and drives it for 500 steps with random
#       inputs, classifying populations by firing_rate_ema.
if(EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/test_snn_biophysical_training_e2e.cpp)
    add_executable(e2e_snn_test_snn_biophysical_training_e2e
        test_snn_biophysical_training_e2e.cpp
    )

    target_link_libraries(e2e_snn_test_snn_biophysical_training_e2e
        PRIVATE ${SNN_E2E_LIBS}
    )

    target_include_directories(e2e_snn_test_snn_biophysical_training_e2e
        PRIVATE ${SNN_E2E_INCLUDES}
    )

    target_compile_features(e2e_snn_test_snn_biophysical_training_e2e PRIVATE cxx_std_17)

    target_compile_definitions(e2e_snn_test_snn_biophysical_training_e2e PRIVATE
        NIMCP_TESTING
        NIMCP_E2E_TESTING
    )

    gtest_discover_tests(e2e_snn_test_snn_biophysical_training_e2e
        WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
        PROPERTIES
            LABELS "e2e;snn;biophysical"
            TIMEOUT 600
    )

    message(STATUS "  Added SNN biophysical training E2E test: test_snn_biophysical_training_e2e")
endif()
