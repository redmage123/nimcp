# VCS Integration Unit Tests

# VCS Integration Unit Tests
add_executable(unit_utils_vcs_integration
    test_vcs_integration.cpp
)
target_link_libraries(unit_utils_vcs_integration
    PRIVATE
        GTest::GTest
        GTest::Main
        nimcp
)
target_include_directories(unit_utils_vcs_integration
    PRIVATE
        ${CMAKE_SOURCE_DIR}/include
        ${CMAKE_SOURCE_DIR}/src/utils/memory
        ${CMAKE_SOURCE_DIR}/src/utils/logging
)
add_test(NAME unit_utils_vcs_integration
         COMMAND unit_utils_vcs_integration)
set_tests_properties(unit_utils_vcs_integration PROPERTIES LABELS "unit;vcs;self-repair")

# VCS Bio-Async Unit Tests
add_executable(unit_utils_vcs_bio_async
    test_vcs_bio_async.cpp
)
target_link_libraries(unit_utils_vcs_bio_async
    PRIVATE
        GTest::GTest
        GTest::Main
        nimcp
        Threads::Threads
)
target_include_directories(unit_utils_vcs_bio_async
    PRIVATE
        ${CMAKE_SOURCE_DIR}/include
)
add_test(NAME unit_utils_vcs_bio_async
         COMMAND unit_utils_vcs_bio_async)
set_tests_properties(unit_utils_vcs_bio_async PROPERTIES
    LABELS "unit;vcs;bio-async"
    TIMEOUT 60
)
