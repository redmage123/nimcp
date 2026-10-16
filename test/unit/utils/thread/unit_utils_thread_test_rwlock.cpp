//=============================================================================
// unit_utils_thread_test_rwlock.cpp - Read-Write Lock Unit Tests
//=============================================================================
/**
 * @file unit_utils_thread_test_rwlock.cpp
 * @brief TDD test suite for read-write lock implementation
 *
 * TEST PHILOSOPHY:
 * - Test-Driven Development (TDD): Tests guide implementation quality
 * - Guard clause verification: Test all error conditions and NULL safety
 * - Lifecycle testing: Init, destroy, and proper cleanup
 * - Concurrency testing: Multi-threaded scenarios with verification
 * - Edge case coverage: Reader/writer priority, starvation, deadlock
 *
 * COVERAGE:
 * 1. Initialization and destruction
 * 2. Multiple readers can hold lock simultaneously
 * 3. Writer blocks readers
 * 4. Reader blocks writers
 * 5. Writer-writer mutual exclusion
 * 6. NULL parameter handling
 * 7. Concurrent reader access
 * 8. Reader-writer fairness
 * 9. Stress testing
 *
 * Note: Trylock and timed lock tests are in unit_utils_thread_test_rwlock_extended.cpp
 *
 * @author NIMCP Development Team
 * @date 2025
 */

#include <gtest/gtest.h>
#include <thread>
#include <vector>
#include <atomic>
#include <chrono>

// Headers have their own extern "C" guards
#include "utils/thread/nimcp_thread.h"

//=============================================================================
// Test Fixtures and Helpers
//=============================================================================

/**
 * WHAT: Test fixture for rwlock tests
 * WHY: Set up/tear down resources for each test
 */
class RWLockTest : public ::testing::Test {
public:
    // Test parameters
    static const int THREAD_COUNT = 8;
    static const int ITERATIONS = 100;
    static const int READER_COUNT = 6;
    static const int WRITER_COUNT = 2;

protected:
    nimcp_rwlock_t lock;
    bool initialized;

    void SetUp() override {
        initialized = (nimcp_rwlock_init(&lock) == NIMCP_SUCCESS);
    }

    void TearDown() override {
        if (initialized) {
            nimcp_rwlock_destroy(&lock);
        }
    }
};

/**
 * WHAT: Helper to sleep for a short duration
 * WHY: Ensure threads have time to interact
 */
static void sleep_ms(int ms) {
    std::this_thread::sleep_for(std::chrono::milliseconds(ms));
}

//=============================================================================
// Lifecycle Tests
//=============================================================================

/**
 * TEST: RWLock initialization
 * WHY: Verify rwlock can be created
 */
TEST_F(RWLockTest, InitializationSuccess) {
    ASSERT_TRUE(initialized);
}

/**
 * TEST: RWLock init with NULL parameter
 * WHY: Verify error handling for invalid parameter
 */
TEST(RWLockErrorTest, InitNullParameter) {
    EXPECT_EQ(NIMCP_ERROR_INVALID_PARAM, nimcp_rwlock_init(NULL));
}

/**
 * TEST: RWLock destroy with NULL parameter
 * WHY: Verify error handling for invalid parameter
 */
TEST(RWLockErrorTest, DestroyNullParameter) {
    EXPECT_EQ(NIMCP_ERROR_INVALID_PARAM, nimcp_rwlock_destroy(NULL));
}

/**
 * TEST: RWLock destroy after init
 * WHY: Verify clean destruction
 */
TEST_F(RWLockTest, DestroySuccess) {
    ASSERT_TRUE(initialized);
    EXPECT_EQ(NIMCP_SUCCESS, nimcp_rwlock_destroy(&lock));
    initialized = false;  // Prevent double destroy in TearDown
}

//=============================================================================
// Basic Read Lock Tests
//=============================================================================

/**
 * TEST: Acquire read lock
 * WHY: Verify basic read lock acquisition
 */
TEST_F(RWLockTest, ReadLockBasic) {
    ASSERT_TRUE(initialized);

    EXPECT_EQ(NIMCP_SUCCESS, nimcp_rwlock_rdlock(&lock));
    EXPECT_EQ(NIMCP_SUCCESS, nimcp_rwlock_unlock(&lock));
}

/**
 * TEST: Read lock NULL parameter
 * WHY: Verify error handling for NULL
 */
TEST(RWLockErrorTest, ReadLockNullParameter) {
    EXPECT_EQ(NIMCP_ERROR_INVALID_PARAM, nimcp_rwlock_rdlock(NULL));
}

/**
 * TEST: Multiple read locks can be held simultaneously
 * WHY: Verify readers don't block each other
 */
TEST_F(RWLockTest, MultipleReadersSimultaneous) {
    ASSERT_TRUE(initialized);

    const int NUM_READERS = 4;
    std::atomic<int> readers_in_cs{0};
    std::atomic<int> max_concurrent_readers{0};
    std::atomic<bool> all_should_exit{false};

    std::vector<std::thread> readers;
    for (int i = 0; i < NUM_READERS; ++i) {
        readers.emplace_back([&]() {
            EXPECT_EQ(NIMCP_SUCCESS, nimcp_rwlock_rdlock(&lock));

            // Record that we're in critical section
            int current = ++readers_in_cs;
            if (current > max_concurrent_readers) {
                max_concurrent_readers = current;
            }

            // Hold lock for a bit to ensure overlap
            while (!all_should_exit) {
                sleep_ms(10);
            }

            readers_in_cs--;
            EXPECT_EQ(NIMCP_SUCCESS, nimcp_rwlock_unlock(&lock));
        });
    }

    // Wait for all readers to enter
    sleep_ms(200);

    // All readers should be in critical section simultaneously
    EXPECT_EQ(NUM_READERS, readers_in_cs.load());
    EXPECT_EQ(NUM_READERS, max_concurrent_readers.load());

    // Release readers
    all_should_exit = true;

    for (auto& t : readers) {
        t.join();
    }
}

/**
 * TEST: Same thread can acquire read lock multiple times
 * WHY: Verify recursive read locking (if supported)
 */
TEST_F(RWLockTest, RecursiveReadLock) {
    ASSERT_TRUE(initialized);

    // Acquire multiple read locks from same thread
    EXPECT_EQ(NIMCP_SUCCESS, nimcp_rwlock_rdlock(&lock));
    EXPECT_EQ(NIMCP_SUCCESS, nimcp_rwlock_rdlock(&lock));
    EXPECT_EQ(NIMCP_SUCCESS, nimcp_rwlock_rdlock(&lock));

    // Release all
    EXPECT_EQ(NIMCP_SUCCESS, nimcp_rwlock_unlock(&lock));
    EXPECT_EQ(NIMCP_SUCCESS, nimcp_rwlock_unlock(&lock));
    EXPECT_EQ(NIMCP_SUCCESS, nimcp_rwlock_unlock(&lock));
}

//=============================================================================
// Basic Write Lock Tests
//=============================================================================

/**
 * TEST: Acquire write lock
 * WHY: Verify basic write lock acquisition
 */
TEST_F(RWLockTest, WriteLockBasic) {
    ASSERT_TRUE(initialized);

    EXPECT_EQ(NIMCP_SUCCESS, nimcp_rwlock_wrlock(&lock));
    EXPECT_EQ(NIMCP_SUCCESS, nimcp_rwlock_unlock(&lock));
}

/**
 * TEST: Write lock NULL parameter
 * WHY: Verify error handling for NULL
 */
TEST(RWLockErrorTest, WriteLockNullParameter) {
    EXPECT_EQ(NIMCP_ERROR_INVALID_PARAM, nimcp_rwlock_wrlock(NULL));
}

/**
 * TEST: Unlock NULL parameter
 * WHY: Verify error handling for NULL
 */
TEST(RWLockErrorTest, UnlockNullParameter) {
    EXPECT_EQ(NIMCP_ERROR_INVALID_PARAM, nimcp_rwlock_unlock(NULL));
}

/**
 * TEST: Multiple lock/unlock cycles
 * WHY: Verify rwlock reusability
 */
TEST_F(RWLockTest, MultipleCycles) {
    ASSERT_TRUE(initialized);

    for (int i = 0; i < 100; ++i) {
        if (i % 2 == 0) {
            EXPECT_EQ(NIMCP_SUCCESS, nimcp_rwlock_rdlock(&lock));
        } else {
            EXPECT_EQ(NIMCP_SUCCESS, nimcp_rwlock_wrlock(&lock));
        }
        EXPECT_EQ(NIMCP_SUCCESS, nimcp_rwlock_unlock(&lock));
    }
}

//=============================================================================
// Writer-Writer Mutual Exclusion Tests
//=============================================================================

/**
 * TEST: Writers block each other
 * WHY: Verify write-write mutual exclusion
 */
TEST_F(RWLockTest, WriterBlocksWriter) {
    ASSERT_TRUE(initialized);

    std::atomic<int> writers_in_cs{0};
    std::atomic<int> max_concurrent_writers{0};

    std::vector<std::thread> writers;
    for (int i = 0; i < 4; ++i) {
        writers.emplace_back([&]() {
            for (int j = 0; j < 10; ++j) {
                EXPECT_EQ(NIMCP_SUCCESS, nimcp_rwlock_wrlock(&lock));

                int current = ++writers_in_cs;
                if (current > max_concurrent_writers) {
                    max_concurrent_writers = current;
                }

                sleep_ms(5);  // Hold lock briefly

                writers_in_cs--;
                EXPECT_EQ(NIMCP_SUCCESS, nimcp_rwlock_unlock(&lock));
            }
        });
    }

    for (auto& t : writers) {
        t.join();
    }

    // Only one writer should ever be in critical section
    EXPECT_EQ(1, max_concurrent_writers.load());
}

//=============================================================================
// Reader-Writer Interaction Tests
//=============================================================================

/**
 * TEST: Writer blocks readers
 * WHY: Verify exclusive access for writers
 */
TEST_F(RWLockTest, WriterBlocksReaders) {
    ASSERT_TRUE(initialized);

    std::atomic<bool> writer_holding{false};
    std::atomic<bool> reader_tried_during_write{false};
    std::atomic<bool> reader_entered_during_write{false};

    // Writer thread
    std::thread writer([&]() {
        EXPECT_EQ(NIMCP_SUCCESS, nimcp_rwlock_wrlock(&lock));
        writer_holding = true;

        // Hold for a bit
        sleep_ms(200);

        writer_holding = false;
        EXPECT_EQ(NIMCP_SUCCESS, nimcp_rwlock_unlock(&lock));
    });

    // Wait for writer to acquire lock
    while (!writer_holding) {
        sleep_ms(10);
    }

    // Reader thread tries to acquire while writer holds
    std::thread reader([&]() {
        reader_tried_during_write = true;

        // This should block until writer releases
        EXPECT_EQ(NIMCP_SUCCESS, nimcp_rwlock_rdlock(&lock));

        // Check if writer was still holding when we entered
        if (writer_holding) {
            reader_entered_during_write = true;
        }

        EXPECT_EQ(NIMCP_SUCCESS, nimcp_rwlock_unlock(&lock));
    });

    writer.join();
    reader.join();

    EXPECT_TRUE(reader_tried_during_write);
    EXPECT_FALSE(reader_entered_during_write);
}

/**
 * TEST: Readers block writer
 * WHY: Verify writer waits for readers to finish
 */
TEST_F(RWLockTest, ReadersBlockWriter) {
    ASSERT_TRUE(initialized);

    std::atomic<bool> reader_holding{false};
    std::atomic<bool> writer_entered_during_read{false};

    // Reader thread
    std::thread reader([&]() {
        EXPECT_EQ(NIMCP_SUCCESS, nimcp_rwlock_rdlock(&lock));
        reader_holding = true;

        sleep_ms(200);

        reader_holding = false;
        EXPECT_EQ(NIMCP_SUCCESS, nimcp_rwlock_unlock(&lock));
    });

    // Wait for reader to acquire
    while (!reader_holding) {
        sleep_ms(10);
    }

    // Writer tries to acquire
    std::thread writer([&]() {
        EXPECT_EQ(NIMCP_SUCCESS, nimcp_rwlock_wrlock(&lock));

        if (reader_holding) {
            writer_entered_during_read = true;
        }

        EXPECT_EQ(NIMCP_SUCCESS, nimcp_rwlock_unlock(&lock));
    });

    reader.join();
    writer.join();

    EXPECT_FALSE(writer_entered_during_read);
}

//=============================================================================
// Concurrent Access Pattern Tests
//=============================================================================

/**
 * TEST: Read-heavy workload
 * WHY: Verify performance with many readers, few writers
 */
TEST_F(RWLockTest, ReadHeavyWorkload) {
    ASSERT_TRUE(initialized);

    std::atomic<int> read_count{0};
    std::atomic<int> write_count{0};
    std::atomic<int> shared_data{0};
    std::atomic<bool> error_detected{false};

    std::vector<std::thread> threads;

    // Many readers
    for (int i = 0; i < READER_COUNT; ++i) {
        threads.emplace_back([&]() {
            for (int j = 0; j < ITERATIONS; ++j) {
                nimcp_rwlock_rdlock(&lock);

                // Read operation
                int val = shared_data.load();
                (void)val;  // Suppress unused warning
                read_count++;

                nimcp_rwlock_unlock(&lock);
            }
        });
    }

    // Few writers
    for (int i = 0; i < WRITER_COUNT; ++i) {
        threads.emplace_back([&]() {
            for (int j = 0; j < ITERATIONS / 10; ++j) {
                nimcp_rwlock_wrlock(&lock);

                // Write operation
                shared_data++;
                write_count++;

                nimcp_rwlock_unlock(&lock);
                sleep_ms(1);  // Slow down writers
            }
        });
    }

    for (auto& t : threads) {
        t.join();
    }

    EXPECT_FALSE(error_detected);
    EXPECT_EQ(READER_COUNT * ITERATIONS, read_count.load());
    EXPECT_EQ(WRITER_COUNT * (ITERATIONS / 10), write_count.load());
}

/**
 * TEST: Balanced read/write workload
 * WHY: Verify fairness with equal readers and writers
 */
TEST_F(RWLockTest, BalancedWorkload) {
    ASSERT_TRUE(initialized);

    const int NUM_EACH = 4;
    const int OPS_EACH = 50;

    std::atomic<int> shared_value{0};
    std::atomic<int> reader_ops{0};
    std::atomic<int> writer_ops{0};

    std::vector<std::thread> threads;

    // Readers
    for (int i = 0; i < NUM_EACH; ++i) {
        threads.emplace_back([&]() {
            for (int j = 0; j < OPS_EACH; ++j) {
                nimcp_rwlock_rdlock(&lock);
                int val = shared_value.load();
                (void)val;
                reader_ops++;
                nimcp_rwlock_unlock(&lock);
            }
        });
    }

    // Writers
    for (int i = 0; i < NUM_EACH; ++i) {
        threads.emplace_back([&]() {
            for (int j = 0; j < OPS_EACH; ++j) {
                nimcp_rwlock_wrlock(&lock);
                shared_value++;
                writer_ops++;
                nimcp_rwlock_unlock(&lock);
            }
        });
    }

    for (auto& t : threads) {
        t.join();
    }

    EXPECT_EQ(NUM_EACH * OPS_EACH, reader_ops.load());
    EXPECT_EQ(NUM_EACH * OPS_EACH, writer_ops.load());
    EXPECT_EQ(NUM_EACH * OPS_EACH, shared_value.load());
}

//=============================================================================
// Data Consistency Tests
//=============================================================================

/**
 * TEST: Readers see consistent data
 * WHY: Verify no torn reads during concurrent writes
 */
TEST_F(RWLockTest, ReadersSeeSonsistentData) {
    ASSERT_TRUE(initialized);

    struct SharedData {
        int field1;
        int field2;
        int field3;
    };

    SharedData data = {0, 0, 0};
    std::atomic<bool> inconsistency_detected{false};
    std::atomic<int> write_count{0};

    std::vector<std::thread> threads;

    // Readers check consistency
    for (int i = 0; i < 4; ++i) {
        threads.emplace_back([&]() {
            for (int j = 0; j < 1000; ++j) {
                nimcp_rwlock_rdlock(&lock);

                // All fields should be equal
                if (data.field1 != data.field2 || data.field2 != data.field3) {
                    inconsistency_detected = true;
                }

                nimcp_rwlock_unlock(&lock);
            }
        });
    }

    // Writers update all fields atomically
    for (int i = 0; i < 2; ++i) {
        threads.emplace_back([&]() {
            for (int j = 0; j < 100; ++j) {
                nimcp_rwlock_wrlock(&lock);

                // Update all fields to same value
                int new_val = ++write_count;
                data.field1 = new_val;
                data.field2 = new_val;
                data.field3 = new_val;

                nimcp_rwlock_unlock(&lock);
            }
        });
    }

    for (auto& t : threads) {
        t.join();
    }

    EXPECT_FALSE(inconsistency_detected);
}

//=============================================================================
// Stress Tests
//=============================================================================

/**
 * TEST: High contention stress test
 * WHY: Verify rwlock handles heavy concurrent load
 */
TEST_F(RWLockTest, HighContentionStress) {
    ASSERT_TRUE(initialized);

    const int STRESS_THREADS = 16;
    const int STRESS_OPS = 500;

    std::atomic<int> total_ops{0};
    std::atomic<int> errors{0};

    std::vector<std::thread> threads;
    for (int t = 0; t < STRESS_THREADS; ++t) {
        threads.emplace_back([&, t]() {
            for (int i = 0; i < STRESS_OPS; ++i) {
                nimcp_result_t result;

                if (t % 4 == 0) {
                    // 25% writers
                    result = nimcp_rwlock_wrlock(&lock);
                } else {
                    // 75% readers
                    result = nimcp_rwlock_rdlock(&lock);
                }

                if (result != NIMCP_SUCCESS) {
                    errors++;
                    continue;
                }

                total_ops++;

                result = nimcp_rwlock_unlock(&lock);
                if (result != NIMCP_SUCCESS) {
                    errors++;
                }
            }
        });
    }

    for (auto& t : threads) {
        t.join();
    }

    EXPECT_EQ(0, errors.load());
    EXPECT_EQ(STRESS_THREADS * STRESS_OPS, total_ops.load());
}

/**
 * TEST: Rapid lock/unlock cycles
 * WHY: Verify no leaks or corruption under rapid cycling
 */
TEST_F(RWLockTest, RapidCycles) {
    ASSERT_TRUE(initialized);

    for (int i = 0; i < 10000; ++i) {
        if (i % 3 == 0) {
            EXPECT_EQ(NIMCP_SUCCESS, nimcp_rwlock_wrlock(&lock));
        } else {
            EXPECT_EQ(NIMCP_SUCCESS, nimcp_rwlock_rdlock(&lock));
        }
        EXPECT_EQ(NIMCP_SUCCESS, nimcp_rwlock_unlock(&lock));
    }
}

/**
 * TEST: Many readers followed by writer
 * WHY: Verify writer eventually gets access
 */
TEST_F(RWLockTest, WriterStarvationPrevention) {
    ASSERT_TRUE(initialized);

    const int NUM_READERS = 8;
    std::atomic<bool> stop_readers{false};
    std::atomic<bool> writer_got_lock{false};
    std::atomic<int> active_readers{0};

    std::vector<std::thread> readers;
    for (int i = 0; i < NUM_READERS; ++i) {
        readers.emplace_back([&]() {
            while (!stop_readers) {
                nimcp_rwlock_rdlock(&lock);
                active_readers++;
                sleep_ms(1);
                active_readers--;
                nimcp_rwlock_unlock(&lock);
            }
        });
    }

    // Give readers time to start
    sleep_ms(50);

    // Writer should eventually get the lock
    std::thread writer([&]() {
        nimcp_rwlock_wrlock(&lock);
        writer_got_lock = true;
        nimcp_rwlock_unlock(&lock);
    });

    // Wait for writer (with timeout)
    auto start = std::chrono::steady_clock::now();
    while (!writer_got_lock) {
        auto now = std::chrono::steady_clock::now();
        auto elapsed = std::chrono::duration_cast<std::chrono::seconds>(now - start);
        if (elapsed.count() > 5) {
            break;  // Timeout
        }
        sleep_ms(10);
    }

    // Stop readers
    stop_readers = true;

    writer.join();
    for (auto& t : readers) {
        t.join();
    }

    EXPECT_TRUE(writer_got_lock);
}

//=============================================================================
// Main
//=============================================================================

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
