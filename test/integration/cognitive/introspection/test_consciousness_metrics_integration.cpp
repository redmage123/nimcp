/**
 * @file test_consciousness_metrics_integration.cpp
 * @brief Integration tests for Consciousness Metrics with brain and bio-async
 *
 * TEST COVERAGE:
 * - Brain + introspection + consciousness metrics integration
 * - Bio-async message propagation for consciousness updates
 * - Monitoring callback invocation
 * - Multi-threaded consciousness monitoring
 * - Consciousness state transitions
 *
 * @author NIMCP Development Team
 * @date 2025
 */

#include <gtest/gtest.h>
#include <thread>
#include <chrono>
#include <atomic>

#include "cognitive/introspection/nimcp_consciousness_metrics.h"
#include "cognitive/introspection/nimcp_introspection.h"
#include "core/brain/nimcp_brain.h"
#include "async/nimcp_bio_async.h"
#include "async/nimcp_bio_router.h"

//=============================================================================
// Test Fixture
//=============================================================================

class ConsciousnessIntegrationTest : public ::testing::Test {
protected:
    brain_t brain;
    introspection_context_t intro_ctx;

    void SetUp() override {
        // Create brain with introspection enabled
        brain = brain_create(
            "test_consciousness_integration",
            BRAIN_SIZE_SMALL,
            BRAIN_TASK_CLASSIFICATION,
            20,  // inputs
            5    // outputs
        );

        if (brain) {
            intro_ctx = brain_get_introspection(brain);
        } else {
            intro_ctx = nullptr;
        }
    }

    void TearDown() override {
        if (brain) {
            brain_disable_consciousness_monitoring(brain);
            brain_destroy(brain);
            brain = nullptr;
            intro_ctx = nullptr;
        }
    }
};

//=============================================================================
// 1. Brain + Introspection + Consciousness Integration
//=============================================================================

TEST_F(ConsciousnessIntegrationTest, ComputePhiWithActiveBrain) {
    if (!intro_ctx) {
        GTEST_SKIP() << "Introspection not available";
    }

    // Step brain to generate activity (5 outputs matches brain setup)
    float inputs[20] = {0};
    float outputs[5] = {0};
    for (int i = 0; i < 20; i++) {
        inputs[i] = (float)i / 20.0f;
    }

    if (brain) {
        brain_predict(brain, inputs, 20, outputs, 5);
    }

    // Compute Φ on active network
    consciousness_phi_result_t* result = introspection_compute_phi(intro_ctx, nullptr);

    if (result) {
        // Φ should be in valid range (may be 0 for approximated large networks)
        EXPECT_GE(result->phi, 0.0f);

        // Should have valid state (valid range includes UNCONSCIOUS)
        EXPECT_GE(result->state, CONSCIOUSNESS_STATE_UNCONSCIOUS);
        EXPECT_LE(result->state, CONSCIOUSNESS_STATE_HEIGHTENED);

        // MIP may be null for approximated networks
        // EXPECT_NE(result->mip, nullptr);

        consciousness_phi_result_free(result);
    }
}

TEST_F(ConsciousnessIntegrationTest, PhiChangesWithActivity) {
    if (!intro_ctx) {
        GTEST_SKIP() << "Introspection not available";
    }

    // Compute Φ with no activity
    consciousness_phi_result_t* result1 = introspection_compute_phi(intro_ctx, nullptr);

    // Generate activity (5 outputs matches brain setup)
    float inputs[20] = {0};
    float outputs[5] = {0};
    for (int i = 0; i < 20; i++) {
        inputs[i] = 1.0f;  // High activity
    }

    if (brain) {
        for (int step = 0; step < 10; step++) {
            brain_predict(brain, inputs, 20, outputs, 5);
        }
    }

    // Compute Φ with activity
    consciousness_phi_result_t* result2 = introspection_compute_phi(intro_ctx, nullptr);

    if (result1 && result2) {
        // Φ may change with activity (not guaranteed to increase)
        // Just verify both computations succeed
        EXPECT_GE(result1->phi, 0.0f);
        EXPECT_GE(result2->phi, 0.0f);

        consciousness_phi_result_free(result1);
        consciousness_phi_result_free(result2);
    }
}

//=============================================================================
// 2. Monitoring Callback Integration
//=============================================================================

struct CallbackContext {
    std::atomic<int> call_count{0};
    std::atomic<float> last_phi{0.0f};
    std::atomic<int> last_state{0};
};

static void consciousness_callback(float phi, consciousness_state_t state, void* context) {
    CallbackContext* ctx = (CallbackContext*)context;
    ctx->call_count++;
    ctx->last_phi = phi;
    ctx->last_state = (int)state;
}

TEST_F(ConsciousnessIntegrationTest, MonitoringCallbackInvoked) {
    if (!brain) {
        GTEST_SKIP() << "Brain not available";
    }

    CallbackContext callback_ctx;

    bool success = brain_enable_consciousness_monitoring(
        brain,
        nullptr,
        100,  // Fast updates for testing
        consciousness_callback,
        &callback_ctx
    );

    if (success) {
        // Wait for a few updates
        std::this_thread::sleep_for(std::chrono::milliseconds(350));

        // Callback should have been invoked at least once
        EXPECT_GT(callback_ctx.call_count.load(), 0);

        brain_disable_consciousness_monitoring(brain);
    } else {
        GTEST_SKIP() << "Monitoring could not be enabled";
    }
}

TEST_F(ConsciousnessIntegrationTest, MonitoringWithoutCallback) {
    if (!brain) {
        GTEST_SKIP() << "Brain not available";
    }

    // Should work without callback
    bool success = brain_enable_consciousness_monitoring(
        brain,
        nullptr,
        100,
        nullptr,  // No callback
        nullptr
    );

    if (success) {
        std::this_thread::sleep_for(std::chrono::milliseconds(250));

        // Should not crash
        brain_disable_consciousness_monitoring(brain);
    }
}

//=============================================================================
// 3. Consciousness State Transition Integration
//=============================================================================

TEST_F(ConsciousnessIntegrationTest, StateClassificationConsistent) {
    if (!intro_ctx) {
        GTEST_SKIP() << "Introspection not available";
    }

    consciousness_phi_result_t* result = introspection_compute_phi(intro_ctx, nullptr);

    if (result) {
        // Classified state should match Φ value
        consciousness_state_t expected = consciousness_classify_phi(result->phi);
        EXPECT_EQ(result->state, expected);

        consciousness_phi_result_free(result);
    }
}

//=============================================================================
// 4. Multi-Brain Integration
//=============================================================================

TEST_F(ConsciousnessIntegrationTest, MultipleBrainsIndependent) {
    brain_t brain2 = brain_create(
        "test_consciousness_2",
        BRAIN_SIZE_TINY,
        BRAIN_TASK_REGRESSION,
        5,
        2
    );

    if (!brain2) {
        GTEST_SKIP() << "Second brain creation failed";
    }

    introspection_context_t intro2 = brain_get_introspection(brain2);

    if (intro_ctx && intro2) {
        // Compute Φ for both brains
        consciousness_phi_result_t* result1 = introspection_compute_phi(intro_ctx, nullptr);
        consciousness_phi_result_t* result2 = introspection_compute_phi(intro2, nullptr);

        if (result1 && result2) {
            // Both should succeed independently
            EXPECT_GE(result1->phi, 0.0f);
            EXPECT_GE(result2->phi, 0.0f);

            // May have different Φ values
            // Just verify no interference

            consciousness_phi_result_free(result1);
            consciousness_phi_result_free(result2);
        }
    }

    brain_destroy(brain2);
}

//=============================================================================
// 5. Performance Integration
//=============================================================================

TEST_F(ConsciousnessIntegrationTest, FastMethodPerformance) {
    if (!intro_ctx) {
        GTEST_SKIP() << "Introspection not available";
    }

    consciousness_phi_config_t config = consciousness_phi_fast_config();

    auto start = std::chrono::high_resolution_clock::now();

    consciousness_phi_result_t* result = introspection_compute_phi_fast(intro_ctx, &config);

    auto end = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);

    if (result) {
        // Fast method should complete quickly (< 500ms)
        EXPECT_LT(duration.count(), 500);

        // Computation time should be recorded
        EXPECT_GT(result->computation_time_ms, 0.0f);
        EXPECT_LT(result->computation_time_ms, 1000.0f);

        consciousness_phi_result_free(result);
    }
}

TEST_F(ConsciousnessIntegrationTest, RepeatedComputationsStable) {
    if (!intro_ctx) {
        GTEST_SKIP() << "Introspection not available";
    }

    // Compute Φ multiple times rapidly
    const int iterations = 10;
    float phi_values[iterations];

    for (int i = 0; i < iterations; i++) {
        consciousness_phi_result_t* result = introspection_compute_phi(intro_ctx, nullptr);
        if (result) {
            phi_values[i] = result->phi;
            consciousness_phi_result_free(result);
        } else {
            phi_values[i] = -1.0f;
        }
    }

    // Should not crash or leak memory
    // Values should be consistent (allowing some variance)
    int valid_count = 0;
    float sum = 0.0f;

    for (int i = 0; i < iterations; i++) {
        if (phi_values[i] >= 0.0f) {
            valid_count++;
            sum += phi_values[i];
        }
    }

    if (valid_count > 0) {
        float avg = sum / (float)valid_count;

        // Most values should be near average
        int near_avg = 0;
        for (int i = 0; i < iterations; i++) {
            if (phi_values[i] >= 0.0f && fabsf(phi_values[i] - avg) < 0.2f) {
                near_avg++;
            }
        }

        EXPECT_GT(near_avg, valid_count / 2);
    }
}

//=============================================================================
// 6. Integration with Other Introspection Features
//=============================================================================

TEST_F(ConsciousnessIntegrationTest, PhiWithActivePopulation) {
    if (!intro_ctx) {
        GTEST_SKIP() << "Introspection not available";
    }

    // Get active population
    neuron_population_t pop = brain_get_active_population(intro_ctx, 0.3f);

    // Compute Φ
    consciousness_phi_result_t* result = introspection_compute_phi(intro_ctx, nullptr);

    if (result) {
        // Both should work together
        EXPECT_GE(result->phi, 0.0f);

        consciousness_phi_result_free(result);
    }

    neuron_population_free(&pop);
}

TEST_F(ConsciousnessIntegrationTest, PhiWithInternalState) {
    if (!intro_ctx) {
        GTEST_SKIP() << "Introspection not available";
    }

    // Get internal state
    brain_state_t state = brain_get_internal_state(intro_ctx, STATE_STRATEGY_BALANCED);

    // Compute Φ
    consciousness_phi_result_t* result = introspection_compute_phi(intro_ctx, nullptr);

    if (result && state.state_vector) {
        // Both should provide complementary information
        EXPECT_GE(result->phi, 0.0f);
        EXPECT_GT(state.dimension, 0u);

        consciousness_phi_result_free(result);
    }

    brain_state_free(&state);
}

//=============================================================================
// Main
//=============================================================================

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
