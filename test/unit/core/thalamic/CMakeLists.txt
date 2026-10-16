# NIMCP Shared Thalamic-Channel Unit Tests
# Tests for core/thalamic/nimcp_thalamic_channel (shared per-network adapter
# consumed by LNN/CNN/FNO/HNN; SNN retains its existing bridge for now).

find_package(GTest REQUIRED)

add_executable(unit_core_thalamic_test_thalamic_channel
    test_thalamic_channel.cpp
)

target_include_directories(unit_core_thalamic_test_thalamic_channel
    PRIVATE
        ${CMAKE_SOURCE_DIR}/include
        ${CMAKE_SOURCE_DIR}/src
        ${GTEST_INCLUDE_DIRS}
)

target_link_libraries(unit_core_thalamic_test_thalamic_channel
    PRIVATE
        nimcp
        GTest::GTest
        GTest::Main
        pthread
)

target_compile_features(unit_core_thalamic_test_thalamic_channel
    PRIVATE cxx_std_17)

add_test(NAME unit_core_thalamic_test_thalamic_channel
         COMMAND unit_core_thalamic_test_thalamic_channel --gtest_brief=1)

set_tests_properties(unit_core_thalamic_test_thalamic_channel
    PROPERTIES
        TIMEOUT 60
        LABELS "unit;core;thalamic"
)
