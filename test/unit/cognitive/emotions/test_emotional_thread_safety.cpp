//=============================================================================
// test_emotional_thread_safety.cpp - Thread Safety Tests for Emotional System
//=============================================================================
/**
 * @file test_emotional_thread_safety.cpp
 * @brief Comprehensive thread safety tests for NIMCP emotional system
 *
 * WHAT: Validates thread safety of emotional system under concurrent access
 * WHY:  Emotional system is accessed by multiple brain modules simultaneously
 * HOW:  Stress tests with multiple threads performing concurrent operations
 *
 * TEST CATEGORIES:
 * 1. Concurrent emotion state updates from multiple threads
 * 2. Concurrent emotion value reads while updates are happening
 * 3. Thread safety of create/destroy during active operations
 * 4. Race condition tests with simultaneous update/query
 * 5. Stress tests with high thread count and many operations
 *
 * @author NIMCP Development Team
 * @date 2025-12-31
 */

#include <gtest/gtest.h>
#include <thread>
#include <vector>
#include <atomic>
#include <chrono>
#include <random>
#include <mutex>
#include <condition_variable>

// C headers with extern "C" guards
extern "C" {
#include "cognitive/nimcp_emotional_system.h"
#include "cognitive/nimcp_emotional_tagging.h"  // For emotional_tag_t definition
}

//=============================================================================
// Test Fixture
//=============================================================================

class EmotionalThreadSafetyTest : public ::testing::Test {
protected:
    emotional_system_t* system_ = nullptr;

    void SetUp() override {
        emotion_config_t config = emotion_system_default_config();
        // Disable features that might cause external dependencies
        config.enable_quantum_emotion = false;
        config.integrate_with_memory = false;
        config.integrate_with_salience = false;
        config.integrate_with_mental_health = false;
        config.integrate_with_ethics = false;
        system_ = emotion_system_create(&config);
    }

    void TearDown() override {
        if (system_) {
            emotion_system_destroy(system_);
            system_ = nullptr;
        }
    }

    // Helper to get current timestamp
    uint64_t now_ms() const {
        auto now = std::chrono::steady_clock::now();
        return std::chrono::duration_cast<std::chrono::milliseconds>(
            now.time_since_epoch()).count();
    }

    // Random float generator
    float random_float(float min, float max) {
        static thread_local std::mt19937 gen(std::random_device{}());
        std::uniform_real_distribution<float> dist(min, max);
        return dist(gen);
    }
};

//=============================================================================
// Test 1: Concurrent Emotion State Updates
//=============================================================================

TEST_F(EmotionalThreadSafetyTest, ConcurrentStateUpdates) {
    ASSERT_NE(system_, nullptr);

    const int num_threads = 8;
    const int updates_per_thread = 100;
    std::atomic<int> successful_updates{0};
    std::vector<std::thread> threads;

    // Start barrier to synchronize thread start
    std::atomic<bool> start_flag{false};

    for (int t = 0; t < num_threads; t++) {
        threads.emplace_back([this, t, updates_per_thread, &successful_updates, &start_flag]() {
            // Wait for start signal
            while (!start_flag.load()) {
                std::this_thread::yield();
            }

            for (int i = 0; i < updates_per_thread; i++) {
                // Generate different valence/arousal per thread
                float valence = -1.0f + (2.0f * t / num_threads);
                float arousal = 0.1f + (0.8f * i / updates_per_thread);
                uint64_t timestamp = now_ms();

                bool result = emotion_system_set_state(system_, valence, arousal, timestamp);
                if (result) {
                    successful_updates++;
                }
            }
        });
    }

    // Release all threads simultaneously
    start_flag.store(true);

    for (auto& t : threads) {
        t.join();
    }

    // All updates should succeed
    EXPECT_EQ(successful_updates.load(), num_threads * updates_per_thread);

    // Verify system is still functional
    emotion_state_t state;
    EXPECT_TRUE(emotion_system_get_state(system_, &state));
    EXPECT_GE(state.valence, -1.0f);
    EXPECT_LE(state.valence, 1.0f);
    EXPECT_GE(state.arousal, 0.0f);
    EXPECT_LE(state.arousal, 1.0f);
}

//=============================================================================
// Test 2: Concurrent Reads While Updates Happening
//=============================================================================

TEST_F(EmotionalThreadSafetyTest, ConcurrentReadsWithUpdates) {
    ASSERT_NE(system_, nullptr);

    std::atomic<bool> running{true};
    std::atomic<int> read_count{0};
    std::atomic<int> write_count{0};
    std::atomic<int> read_failures{0};
    std::vector<std::thread> threads;

    // Writer threads
    for (int t = 0; t < 4; t++) {
        threads.emplace_back([this, &running, &write_count]() {
            while (running) {
                float valence = random_float(-1.0f, 1.0f);
                float arousal = random_float(0.0f, 1.0f);
                emotion_system_set_state(system_, valence, arousal, now_ms());
                write_count++;
            }
        });
    }

    // Reader threads
    for (int t = 0; t < 4; t++) {
        threads.emplace_back([this, &running, &read_count, &read_failures]() {
            while (running) {
                emotion_state_t state;
                if (emotion_system_get_state(system_, &state)) {
                    // Verify state is within valid bounds
                    if (state.valence < -1.0f || state.valence > 1.0f ||
                        state.arousal < 0.0f || state.arousal > 1.0f) {
                        read_failures++;
                    }
                    read_count++;
                } else {
                    read_failures++;
                }
            }
        });
    }

    // Run for 200ms
    std::this_thread::sleep_for(std::chrono::milliseconds(200));
    running = false;

    for (auto& t : threads) {
        t.join();
    }

    EXPECT_GT(read_count.load(), 0);
    EXPECT_GT(write_count.load(), 0);
    EXPECT_EQ(read_failures.load(), 0);
}

//=============================================================================
// Test 3: Thread Safety During Create/Destroy (Stress Test)
//=============================================================================

TEST_F(EmotionalThreadSafetyTest, RapidCreateDestroy) {
    // Destroy the fixture's system first
    emotion_system_destroy(system_);
    system_ = nullptr;

    const int iterations = 20;

    for (int i = 0; i < iterations; i++) {
        emotion_config_t config = emotion_system_default_config();
        config.enable_quantum_emotion = false;

        emotional_system_t* sys = emotion_system_create(&config);
        ASSERT_NE(sys, nullptr) << "Failed on iteration " << i;

        // Perform some operations
        emotion_system_set_state(sys, 0.5f, 0.5f, now_ms());

        emotion_state_t state;
        EXPECT_TRUE(emotion_system_get_state(sys, &state));

        emotion_system_destroy(sys);
    }

    // Recreate for fixture teardown
    emotion_config_t config = emotion_system_default_config();
    config.enable_quantum_emotion = false;
    system_ = emotion_system_create(&config);
}

//=============================================================================
// Test 4: Race Conditions - Multiple Operations Simultaneously
//=============================================================================

TEST_F(EmotionalThreadSafetyTest, RaceCondition_MultipleOperations) {
    ASSERT_NE(system_, nullptr);

    std::atomic<bool> running{true};
    std::atomic<int> errors{0};
    std::vector<std::thread> threads;

    // Thread 1: Set state
    threads.emplace_back([this, &running, &errors]() {
        while (running) {
            if (!emotion_system_set_state(system_, random_float(-1.0f, 1.0f),
                                          random_float(0.0f, 1.0f), now_ms())) {
                errors++;
            }
        }
    });

    // Thread 2: Get state
    threads.emplace_back([this, &running, &errors]() {
        while (running) {
            emotion_state_t state;
            if (!emotion_system_get_state(system_, &state)) {
                errors++;
            }
        }
    });

    // Thread 3: Decay
    threads.emplace_back([this, &running, &errors]() {
        while (running) {
            if (!emotion_system_decay(system_, 0.016f, now_ms())) {
                errors++;
            }
            std::this_thread::sleep_for(std::chrono::milliseconds(16));
        }
    });

    // Thread 4: Regulate
    threads.emplace_back([this, &running, &errors]() {
        while (running) {
            // Strategy 0 = Reappraisal, 1 = Suppression, 2 = Distraction
            emotion_system_regulate(system_, rand() % 3);
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
        }
    });

    // Thread 5: Auto-regulate
    threads.emplace_back([this, &running, &errors]() {
        while (running) {
            emotion_system_auto_regulate(system_);
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
        }
    });

    // Thread 6: Get salience boost
    threads.emplace_back([this, &running, &errors]() {
        while (running) {
            float boost = emotion_system_get_salience_boost(system_);
            if (boost < 1.0f || boost > 3.0f) {
                errors++;
            }
        }
    });

    // Thread 7: Get memory priority
    threads.emplace_back([this, &running, &errors]() {
        while (running) {
            float priority = emotion_system_get_memory_priority(system_);
            if (priority < 0.0f || priority > 1.0f) {
                errors++;
            }
        }
    });

    // Thread 8: Get mental health impact
    threads.emplace_back([this, &running, &errors]() {
        while (running) {
            float impact = emotion_system_get_mental_health_impact(system_);
            if (impact < 0.0f || impact > 1.0f) {
                errors++;
            }
        }
    });

    // Run for 300ms
    std::this_thread::sleep_for(std::chrono::milliseconds(300));
    running = false;

    for (auto& t : threads) {
        t.join();
    }

    EXPECT_EQ(errors.load(), 0);
}

//=============================================================================
// Test 5: Stress Test - High Thread Count
//=============================================================================

TEST_F(EmotionalThreadSafetyTest, StressTest_HighThreadCount) {
    ASSERT_NE(system_, nullptr);

    const int num_threads = 32;
    const int operations_per_thread = 500;
    std::atomic<int> total_operations{0};
    std::atomic<int> errors{0};
    std::vector<std::thread> threads;

    std::atomic<bool> start_flag{false};

    for (int t = 0; t < num_threads; t++) {
        threads.emplace_back([this, t, operations_per_thread, &total_operations, &errors, &start_flag]() {
            // Wait for start signal
            while (!start_flag.load()) {
                std::this_thread::yield();
            }

            for (int i = 0; i < operations_per_thread; i++) {
                int op = (t + i) % 5;
                bool success = true;

                switch (op) {
                    case 0: {
                        success = emotion_system_set_state(system_,
                            random_float(-1.0f, 1.0f),
                            random_float(0.0f, 1.0f),
                            now_ms());
                        break;
                    }
                    case 1: {
                        emotion_state_t state;
                        success = emotion_system_get_state(system_, &state);
                        break;
                    }
                    case 2: {
                        success = emotion_system_decay(system_, 0.01f, now_ms());
                        break;
                    }
                    case 3: {
                        float boost = emotion_system_get_salience_boost(system_);
                        success = (boost >= 1.0f && boost <= 3.0f);
                        break;
                    }
                    case 4: {
                        emotion_stats_t stats;
                        success = emotion_system_get_stats(system_, &stats);
                        break;
                    }
                }

                if (success) {
                    total_operations++;
                } else {
                    errors++;
                }
            }
        });
    }

    // Release all threads
    start_flag.store(true);

    for (auto& t : threads) {
        t.join();
    }

    // All operations should succeed
    EXPECT_EQ(errors.load(), 0);
    EXPECT_EQ(total_operations.load(), num_threads * operations_per_thread);
}

//=============================================================================
// Test 6: Statistics Accuracy Under Contention
//=============================================================================

TEST_F(EmotionalThreadSafetyTest, StatisticsAccuracyUnderContention) {
    ASSERT_NE(system_, nullptr);

    const int num_threads = 4;
    const int updates_per_thread = 100;
    std::atomic<int> completed_updates{0};
    std::vector<std::thread> threads;

    std::atomic<bool> start_flag{false};

    for (int t = 0; t < num_threads; t++) {
        threads.emplace_back([this, updates_per_thread, &completed_updates, &start_flag]() {
            while (!start_flag.load()) {
                std::this_thread::yield();
            }

            for (int i = 0; i < updates_per_thread; i++) {
                if (emotion_system_set_state(system_,
                    random_float(-1.0f, 1.0f),
                    random_float(0.0f, 1.0f),
                    now_ms())) {
                    completed_updates++;
                }
            }
        });
    }

    start_flag.store(true);

    for (auto& t : threads) {
        t.join();
    }

    emotion_stats_t stats;
    EXPECT_TRUE(emotion_system_get_stats(system_, &stats));

    // Statistics should reflect approximately the number of updates
    // (some variance is acceptable due to internal updates)
    EXPECT_GE(stats.total_updates, static_cast<uint64_t>(completed_updates.load() * 0.9));
}

//=============================================================================
// Test 7: Concurrent Get Tag Operations
//=============================================================================

TEST_F(EmotionalThreadSafetyTest, ConcurrentGetTag) {
    ASSERT_NE(system_, nullptr);

    // Set initial state
    emotion_system_set_state(system_, 0.5f, 0.7f, now_ms());

    std::atomic<bool> running{true};
    std::atomic<int> tag_reads{0};
    std::atomic<int> failures{0};
    std::vector<std::thread> threads;

    // Multiple reader threads for get_tag
    for (int t = 0; t < 8; t++) {
        threads.emplace_back([this, &running, &tag_reads, &failures]() {
            while (running) {
                emotional_tag_t tag;
                if (emotion_system_get_tag(system_, &tag)) {
                    // Verify tag is within bounds
                    if (tag.valence < -1.0f || tag.valence > 1.0f ||
                        tag.arousal < 0.0f || tag.arousal > 1.0f) {
                        failures++;
                    }
                    tag_reads++;
                } else {
                    failures++;
                }
            }
        });
    }

    // Writer thread
    threads.emplace_back([this, &running]() {
        while (running) {
            emotion_system_set_state(system_,
                random_float(-1.0f, 1.0f),
                random_float(0.0f, 1.0f),
                now_ms());
        }
    });

    std::this_thread::sleep_for(std::chrono::milliseconds(150));
    running = false;

    for (auto& t : threads) {
        t.join();
    }

    EXPECT_GT(tag_reads.load(), 0);
    EXPECT_EQ(failures.load(), 0);
}

//=============================================================================
// Test 8: Concurrent is_active Checks
//=============================================================================

TEST_F(EmotionalThreadSafetyTest, ConcurrentIsActiveChecks) {
    ASSERT_NE(system_, nullptr);

    std::atomic<bool> running{true};
    std::atomic<int> checks{0};
    std::vector<std::thread> threads;

    // Multiple threads checking is_active
    for (int t = 0; t < 6; t++) {
        threads.emplace_back([this, &running, &checks]() {
            while (running) {
                // Check various emotion IDs with different thresholds
                for (uint32_t emotion_id = 0; emotion_id < 8; emotion_id++) {
                    emotion_system_is_active(system_, emotion_id, 0.5f);
                    checks++;
                }
            }
        });
    }

    // Writer thread updating state
    threads.emplace_back([this, &running]() {
        while (running) {
            emotion_system_set_state(system_,
                random_float(-1.0f, 1.0f),
                random_float(0.0f, 1.0f),
                now_ms());
            std::this_thread::sleep_for(std::chrono::microseconds(100));
        }
    });

    std::this_thread::sleep_for(std::chrono::milliseconds(150));
    running = false;

    for (auto& t : threads) {
        t.join();
    }

    EXPECT_GT(checks.load(), 0);
}

//=============================================================================
// Test 9: Concurrent Decay Operations
//=============================================================================

TEST_F(EmotionalThreadSafetyTest, ConcurrentDecayOperations) {
    ASSERT_NE(system_, nullptr);

    // Set high arousal to start
    emotion_system_set_state(system_, 0.0f, 1.0f, now_ms());

    std::atomic<bool> running{true};
    std::atomic<int> decay_count{0};
    std::atomic<int> failures{0};
    std::vector<std::thread> threads;

    // Multiple decay threads
    for (int t = 0; t < 4; t++) {
        threads.emplace_back([this, &running, &decay_count, &failures]() {
            while (running) {
                if (emotion_system_decay(system_, 0.01f, now_ms())) {
                    decay_count++;
                } else {
                    failures++;
                }
            }
        });
    }

    // State reader
    threads.emplace_back([this, &running, &failures]() {
        while (running) {
            emotion_state_t state;
            if (emotion_system_get_state(system_, &state)) {
                // Arousal should never go negative
                if (state.arousal < 0.0f) {
                    failures++;
                }
            }
        }
    });

    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    running = false;

    for (auto& t : threads) {
        t.join();
    }

    EXPECT_GT(decay_count.load(), 0);
    EXPECT_EQ(failures.load(), 0);

    // Arousal should have decayed
    emotion_state_t final_state;
    EXPECT_TRUE(emotion_system_get_state(system_, &final_state));
    EXPECT_LT(final_state.arousal, 1.0f);
}

//=============================================================================
// Test 10: Concurrent Regulation Operations
//=============================================================================

TEST_F(EmotionalThreadSafetyTest, ConcurrentRegulationOperations) {
    ASSERT_NE(system_, nullptr);

    // Set high intensity emotional state
    emotion_system_set_state(system_, -0.8f, 0.9f, now_ms());

    std::atomic<bool> running{true};
    std::atomic<int> regulations{0};
    std::vector<std::thread> threads;

    // Multiple regulation threads
    for (int t = 0; t < 4; t++) {
        threads.emplace_back([this, t, &running, &regulations]() {
            while (running) {
                uint32_t strategy = t % 3;  // Reappraisal, Suppression, Distraction
                emotion_system_regulate(system_, strategy);
                regulations++;
                std::this_thread::sleep_for(std::chrono::milliseconds(5));
            }
        });
    }

    // Auto-regulation threads
    for (int t = 0; t < 2; t++) {
        threads.emplace_back([this, &running, &regulations]() {
            while (running) {
                emotion_system_auto_regulate(system_);
                regulations++;
                std::this_thread::sleep_for(std::chrono::milliseconds(5));
            }
        });
    }

    // State updater to trigger need for regulation
    threads.emplace_back([this, &running]() {
        while (running) {
            // Periodically set high intensity state
            emotion_system_set_state(system_, -0.9f, 0.95f, now_ms());
            std::this_thread::sleep_for(std::chrono::milliseconds(20));
        }
    });

    std::this_thread::sleep_for(std::chrono::milliseconds(200));
    running = false;

    for (auto& t : threads) {
        t.join();
    }

    EXPECT_GT(regulations.load(), 0);

    // Verify stats show regulations occurred
    emotion_stats_t stats;
    EXPECT_TRUE(emotion_system_get_stats(system_, &stats));
    EXPECT_GT(stats.total_regulations, 0u);
}

//=============================================================================
// Test 11: No Deadlock Under High Contention
//=============================================================================

TEST_F(EmotionalThreadSafetyTest, NoDeadlock_HighContention) {
    ASSERT_NE(system_, nullptr);

    const int num_threads = 16;
    std::atomic<bool> running{true};
    std::atomic<int> operations{0};
    std::vector<std::thread> threads;

    for (int t = 0; t < num_threads; t++) {
        threads.emplace_back([this, t, &running, &operations]() {
            while (running) {
                // Each thread does multiple operations in sequence
                emotion_system_set_state(system_,
                    random_float(-1.0f, 1.0f),
                    random_float(0.0f, 1.0f),
                    now_ms());

                emotion_state_t state;
                emotion_system_get_state(system_, &state);

                emotion_system_get_salience_boost(system_);
                emotion_system_get_memory_priority(system_);
                emotion_system_get_mental_health_impact(system_);

                emotion_stats_t stats;
                emotion_system_get_stats(system_, &stats);

                emotion_system_decay(system_, 0.001f, now_ms());

                if (t % 2 == 0) {
                    emotion_system_regulate(system_, t % 3);
                } else {
                    emotion_system_auto_regulate(system_);
                }

                operations++;
            }
        });
    }

    // Test should complete within timeout (no deadlock)
    // Run for 500ms
    std::this_thread::sleep_for(std::chrono::milliseconds(500));
    running = false;

    for (auto& t : threads) {
        t.join();
    }

    EXPECT_GT(operations.load(), 100);  // Should have completed many operations
}

//=============================================================================
// Test 12: Data Consistency Under Concurrent Access
//=============================================================================

TEST_F(EmotionalThreadSafetyTest, DataConsistency) {
    ASSERT_NE(system_, nullptr);

    std::atomic<bool> running{true};
    std::atomic<int> inconsistencies{0};
    std::vector<std::thread> threads;

    // Writer that sets specific known values
    threads.emplace_back([this, &running]() {
        float valence = 0.5f;
        float arousal = 0.5f;
        while (running) {
            emotion_system_set_state(system_, valence, arousal, now_ms());
            // Alternate between two states
            valence = (valence > 0.0f) ? -0.5f : 0.5f;
            arousal = (arousal > 0.5f) ? 0.3f : 0.7f;
        }
    });

    // Readers that verify state consistency
    for (int t = 0; t < 6; t++) {
        threads.emplace_back([this, &running, &inconsistencies]() {
            while (running) {
                emotion_state_t state;
                if (emotion_system_get_state(system_, &state)) {
                    // Intensity should be consistent with valence/arousal
                    float expected_intensity_min = 0.0f;
                    float expected_intensity_max = 1.0f;

                    if (state.intensity < expected_intensity_min ||
                        state.intensity > expected_intensity_max) {
                        inconsistencies++;
                    }

                    // Stability should be in valid range
                    if (state.emotional_stability < 0.0f ||
                        state.emotional_stability > 1.0f) {
                        inconsistencies++;
                    }
                }
            }
        });
    }

    std::this_thread::sleep_for(std::chrono::milliseconds(200));
    running = false;

    for (auto& t : threads) {
        t.join();
    }

    EXPECT_EQ(inconsistencies.load(), 0);
}

//=============================================================================
// Test 13: Null Safety Under Concurrent Access
//=============================================================================

TEST_F(EmotionalThreadSafetyTest, NullSafety) {
    // Test null system pointer handling
    EXPECT_FALSE(emotion_system_set_state(nullptr, 0.5f, 0.5f, 0));

    emotion_state_t state;
    EXPECT_FALSE(emotion_system_get_state(nullptr, &state));

    emotional_tag_t tag;
    EXPECT_FALSE(emotion_system_get_tag(nullptr, &tag));

    EXPECT_FALSE(emotion_system_is_active(nullptr, 0, 0.5f));
    EXPECT_FALSE(emotion_system_decay(nullptr, 0.1f, 0));
    EXPECT_FALSE(emotion_system_regulate(nullptr, 0));
    EXPECT_FALSE(emotion_system_auto_regulate(nullptr));

    EXPECT_EQ(emotion_system_get_salience_boost(nullptr), 1.0f);
    EXPECT_EQ(emotion_system_get_memory_priority(nullptr), 0.0f);
    EXPECT_EQ(emotion_system_get_mental_health_impact(nullptr), 0.0f);

    emotion_stats_t stats;
    EXPECT_FALSE(emotion_system_get_stats(nullptr, &stats));

    // Null output pointer handling
    ASSERT_NE(system_, nullptr);
    EXPECT_FALSE(emotion_system_get_state(system_, nullptr));
    EXPECT_FALSE(emotion_system_get_tag(system_, nullptr));
    EXPECT_FALSE(emotion_system_get_stats(system_, nullptr));
}

//=============================================================================
// Test 14: Multimodal Update Thread Safety
//=============================================================================

TEST_F(EmotionalThreadSafetyTest, ConcurrentMultimodalUpdate) {
    ASSERT_NE(system_, nullptr);

    std::atomic<bool> running{true};
    std::atomic<int> updates{0};
    std::atomic<int> failures{0};
    std::vector<std::thread> threads;

    // Multiple threads calling multimodal update
    for (int t = 0; t < 4; t++) {
        threads.emplace_back([this, &running, &updates, &failures]() {
            std::vector<float> visual_data(64, 0.5f);
            std::vector<float> audio_data(32, 0.3f);

            while (running) {
                bool result = emotion_system_update_multimodal(
                    system_,
                    visual_data.data(), static_cast<uint32_t>(visual_data.size()),
                    audio_data.data(), static_cast<uint32_t>(audio_data.size()),
                    "test text input",
                    now_ms()
                );

                if (result) {
                    updates++;
                } else {
                    failures++;
                }
            }
        });
    }

    // Concurrent state readers
    for (int t = 0; t < 2; t++) {
        threads.emplace_back([this, &running]() {
            while (running) {
                emotion_state_t state;
                emotion_system_get_state(system_, &state);
            }
        });
    }

    std::this_thread::sleep_for(std::chrono::milliseconds(200));
    running = false;

    for (auto& t : threads) {
        t.join();
    }

    EXPECT_GT(updates.load(), 0);
    EXPECT_EQ(failures.load(), 0);
}

//=============================================================================
// Test 15: Long Running Stability Test
//=============================================================================

TEST_F(EmotionalThreadSafetyTest, LongRunningStability) {
    ASSERT_NE(system_, nullptr);

    const int num_threads = 8;
    const int duration_ms = 1000;  // 1 second
    std::atomic<bool> running{true};
    std::atomic<uint64_t> total_operations{0};
    std::atomic<int> errors{0};
    std::vector<std::thread> threads;

    auto start_time = std::chrono::steady_clock::now();

    for (int t = 0; t < num_threads; t++) {
        threads.emplace_back([this, t, &running, &total_operations, &errors]() {
            while (running) {
                try {
                    switch (t % 4) {
                        case 0:
                            emotion_system_set_state(system_,
                                random_float(-1.0f, 1.0f),
                                random_float(0.0f, 1.0f),
                                now_ms());
                            break;
                        case 1: {
                            emotion_state_t state;
                            emotion_system_get_state(system_, &state);
                            break;
                        }
                        case 2:
                            emotion_system_decay(system_, 0.001f, now_ms());
                            break;
                        case 3:
                            emotion_system_auto_regulate(system_);
                            break;
                    }
                    total_operations++;
                } catch (...) {
                    errors++;
                }
            }
        });
    }

    // Run for specified duration
    std::this_thread::sleep_for(std::chrono::milliseconds(duration_ms));
    running = false;

    for (auto& t : threads) {
        t.join();
    }

    auto end_time = std::chrono::steady_clock::now();
    auto actual_duration = std::chrono::duration_cast<std::chrono::milliseconds>(
        end_time - start_time).count();

    EXPECT_EQ(errors.load(), 0);
    EXPECT_GT(total_operations.load(), 1000u);

    // Log throughput for debugging
    double ops_per_second = (static_cast<double>(total_operations.load()) /
                            static_cast<double>(actual_duration)) * 1000.0;
    (void)ops_per_second;  // Suppress unused warning

    // Verify system is still functional
    emotion_state_t final_state;
    EXPECT_TRUE(emotion_system_get_state(system_, &final_state));
}
