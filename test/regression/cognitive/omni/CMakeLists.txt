# Omnidirectional Inference Regression Tests

if(GTest_FOUND)
    # Regression test executable for omnidirectional performance
    add_executable(test_omni_regression
        test_omni_regression.cpp
    )

    target_link_libraries(test_omni_regression
        nimcp
        GTest::gtest
        GTest::gtest_main
    )

    target_include_directories(test_omni_regression PRIVATE
        ${CMAKE_SOURCE_DIR}/include
        ${CMAKE_SOURCE_DIR}/src
    )

    # Register with CTest
    add_test(NAME OmniRegression
        COMMAND test_omni_regression
    )

    # Set test properties - longer timeout for benchmarks
    set_tests_properties(OmniRegression PROPERTIES
        TIMEOUT 300
        LABELS "regression;cognitive;omni;performance"
    )

    # DISABLED: Test needs API updates to match current WM security immune bridge API
    # TODO: Update omni_wm_security_immune_bridge_config_t and stats type usage
    #
    # # Regression test executable for World Model bridges
    # add_executable(test_omni_wm_bridges_regression
    #     test_omni_wm_bridges_regression.cpp
    # )
    #
    # target_link_libraries(test_omni_wm_bridges_regression
    #     nimcp
    #     GTest::gtest
    #     GTest::gtest_main
    # )
    #
    # target_include_directories(test_omni_wm_bridges_regression PRIVATE
    #     ${CMAKE_SOURCE_DIR}/include
    #     ${CMAKE_SOURCE_DIR}/src
    # )
    #
    # # Register with CTest
    # add_test(NAME OmniWMBridgesRegression
    #     COMMAND test_omni_wm_bridges_regression
    # )
    #
    # # Set test properties - longer timeout for comprehensive bridge tests
    # set_tests_properties(OmniWMBridgesRegression PROPERTIES
    #     TIMEOUT 600
    #     LABELS "regression;cognitive;omni;wm-bridges;performance"
    # )
endif()
