/**
 * @file test_thermodynamics_tls_fix.cpp
 * @brief Tests for thermodynamics TLS-to-shared-state migration (Bug H7)
 *
 * WHAT: Verify that thermodynamic state is visible across threads
 * WHY:  Previously used __thread storage, making state invisible between threads
 * HOW:  Initialize on one thread, read from another thread
 */

#include <gtest/gtest.h>
#include <thread>
#include <atomic>

// Headers have their own extern "C" guards
#include "physics/thermodynamics/nimcp_thermodynamics.h"

class ThermodynamicsTLSFixTest : public ::testing::Test {
protected:
    nimcp_thermodynamic_state_t state;
    bool initialized = false;

    void SetUp() override {
        memset(&state, 0, sizeof(state));
    }

    void TearDown() override {
        if (initialized) {
            nimcp_thermo_destroy(&state);
            initialized = false;
        }
    }
};

/**
 * WHAT: Test basic init and query on same thread (sanity check)
 * WHY:  Ensure the TLS removal didn't break single-thread usage
 */
TEST_F(ThermodynamicsTLSFixTest, SingleThreadInitAndQuery) {
    nimcp_thermo_config_t config = nimcp_thermo_default_config();
    nimcp_error_t err = nimcp_thermo_init(&state, &config);
    ASSERT_EQ(err, NIMCP_SUCCESS);
    initialized = true;

    EXPECT_TRUE(state.initialized);
    EXPECT_DOUBLE_EQ(state.atp_available, config.atp_pool_size);

    double ratio = nimcp_thermo_get_atp_ratio(&state);
    EXPECT_NEAR(ratio, 1.0, 0.01);
}

/**
 * WHAT: Test that state initialized on thread A is queryable from thread B
 * WHY:  This was the core bug — __thread made cross-thread access return NULL
 */
TEST_F(ThermodynamicsTLSFixTest, CrossThreadStateVisibility) {
    nimcp_thermo_config_t config = nimcp_thermo_default_config();
    nimcp_error_t err = nimcp_thermo_init(&state, &config);
    ASSERT_EQ(err, NIMCP_SUCCESS);
    initialized = true;

    /* Update state on main thread — use tiny power to avoid exhausting ATP pool.
     * Default pool is 1e-12 moles; power_consumed=1e-20 W * 0.001s = 1e-23 J,
     * which consumes negligible ATP. */
    err = nimcp_thermo_update(&state, 0.001, 1.0e-20, 1);
    ASSERT_EQ(err, NIMCP_SUCCESS);

    /* Read state from a different thread */
    std::atomic<double> cross_thread_ratio{-1.0};
    std::atomic<bool> cross_thread_critical{false};

    std::thread reader([&]() {
        double ratio = nimcp_thermo_get_atp_ratio(&state);
        cross_thread_ratio.store(ratio);

        bool critical = nimcp_thermo_is_atp_critical(&state, 0.001);
        cross_thread_critical.store(critical);
    });
    reader.join();

    /* If TLS was still in use, ratio would be 0.0 (NULL internal state) */
    double ratio = cross_thread_ratio.load();
    EXPECT_GT(ratio, 0.0) << "Cross-thread ratio should be positive (not 0 from NULL internal state)";
    EXPECT_LE(ratio, 1.0);

    /* ATP should NOT be critical at 0.1% threshold with tiny power draw */
    EXPECT_FALSE(cross_thread_critical.load());
}

/**
 * WHAT: Test concurrent updates from multiple threads
 * WHY:  After removing TLS, mutex must protect shared state
 */
TEST_F(ThermodynamicsTLSFixTest, ConcurrentUpdates) {
    nimcp_thermo_config_t config = nimcp_thermo_default_config();
    nimcp_error_t err = nimcp_thermo_init(&state, &config);
    ASSERT_EQ(err, NIMCP_SUCCESS);
    initialized = true;

    const int NUM_ITERATIONS = 100;
    std::atomic<int> success_count{0};

    auto updater = [&]() {
        for (int i = 0; i < NUM_ITERATIONS; i++) {
            nimcp_error_t result = nimcp_thermo_update(&state, 0.001, 0.01, 10);
            if (result == NIMCP_SUCCESS) {
                success_count.fetch_add(1);
            }
        }
    };

    std::thread t1(updater);
    std::thread t2(updater);
    t1.join();
    t2.join();

    EXPECT_EQ(success_count.load(), NUM_ITERATIONS * 2);
    EXPECT_GT(state.total_energy_consumed, 0.0);
}

/**
 * WHAT: Test efficiency query from different thread than init
 * WHY:  Efficiency uses internal state — must be visible cross-thread
 */
TEST_F(ThermodynamicsTLSFixTest, CrossThreadEfficiencyQuery) {
    nimcp_thermo_config_t config = nimcp_thermo_default_config();
    nimcp_error_t err = nimcp_thermo_init(&state, &config);
    ASSERT_EQ(err, NIMCP_SUCCESS);
    initialized = true;

    /* Do some work to generate efficiency data */
    nimcp_thermo_update(&state, 0.001, 10.0, 1000);

    std::atomic<bool> query_succeeded{false};
    std::thread querier([&]() {
        double comp_eff = 0, thermo_eff = 0, landauer_eff = 0;
        nimcp_error_t result = nimcp_thermo_get_efficiency(
            &state, &comp_eff, &thermo_eff, &landauer_eff);
        if (result == NIMCP_SUCCESS) {
            query_succeeded.store(true);
        }
    });
    querier.join();

    EXPECT_TRUE(query_succeeded.load())
        << "Efficiency query from different thread should succeed";
}
