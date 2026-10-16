# IO Unit Tests
# Test coverage for encryption, persistence, serialization

# ============================================================================
# Encryption Stub Tests (Bug H8)
# ============================================================================
add_executable(unit_io_test_encryption_stub
    test_encryption_stub.cpp
)
target_link_libraries(unit_io_test_encryption_stub
    PRIVATE
        nimcp
        GTest::gtest
        GTest::gtest_main
        Threads::Threads
        m
)
target_compile_features(unit_io_test_encryption_stub PRIVATE cxx_std_20)
gtest_discover_tests(unit_io_test_encryption_stub
    PROPERTIES LABELS "unit;io;encryption;stub"
    TIMEOUT 60
)

# ============================================================================
# Persistence Short-Read Tests (Bug H1)
# ============================================================================
add_executable(unit_io_test_persistence_short_read
    test_persistence_short_read.cpp
)
target_link_libraries(unit_io_test_persistence_short_read
    PRIVATE
        nimcp
        GTest::gtest
        GTest::gtest_main
        Threads::Threads
        m
)
target_compile_features(unit_io_test_persistence_short_read PRIVATE cxx_std_20)
gtest_discover_tests(unit_io_test_persistence_short_read
    PROPERTIES LABELS "unit;io;persistence;short_read"
    TIMEOUT 120
)
