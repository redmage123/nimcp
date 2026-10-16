# Biology Layer E2E Tests CMakeLists.txt

# Neurovascular pipeline E2E test
add_executable(e2e_test_neurovascular_pipeline
    e2e_test_neurovascular_pipeline.cpp
)

target_include_directories(e2e_test_neurovascular_pipeline PRIVATE
    ${PROJECT_SOURCE_DIR}/include
    ${PROJECT_SOURCE_DIR}/src
)

target_link_libraries(e2e_test_neurovascular_pipeline
    nimcp
    GTest::gtest
    GTest::gtest_main
)

gtest_discover_tests(e2e_test_neurovascular_pipeline
    PROPERTIES LABELS "e2e;biology;neurovascular"
)
