# Unit tests for parallel cognitive dispatch (actor pattern)

add_executable(unit_cognitive_dispatch
    test_cognitive_dispatch.cpp
)
target_link_libraries(unit_cognitive_dispatch
    PRIVATE nimcp GTest::gtest GTest::gtest_main Threads::Threads m
)
target_compile_features(unit_cognitive_dispatch PRIVATE cxx_std_17)
target_include_directories(unit_cognitive_dispatch
    PRIVATE
        ${CMAKE_SOURCE_DIR}/include
        ${CMAKE_SOURCE_DIR}/src
)
gtest_discover_tests(unit_cognitive_dispatch
    PROPERTIES LABELS "unit;core;cognitive_dispatch"
    TIMEOUT 60
)
