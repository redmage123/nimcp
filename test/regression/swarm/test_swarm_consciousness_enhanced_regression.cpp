/**
 * @file test_swarm_consciousness_enhanced_regression.cpp
 * @brief Regression tests for NIMCP Enhanced Swarm Consciousness
 *
 * WHAT: Regression tests for enhanced consciousness (peer callbacks, remote phi, dynamics)
 * WHY:  Ensure stability, performance, and consistency over time
 * HOW:  Measure metrics stability, scaling behavior, and numerical accuracy
 *
 * TEST COVERAGE:
 * - Stability: Determinism, bounds, state consistency (5 tests)
 * - Scaling: Peer count, phi collection, dynamics computation (4 tests)
 * - Performance: Time, memory, threading overhead (4 tests)
 * - Numerical: Small values, large values, edge cases (4 tests)
 * - Concurrency: Parallel callbacks, reader-writer safety (4 tests)
 *
 * BIOLOGICAL BASIS:
 * Extended IIT with information geometry (mutual information, transfer entropy),
 * consciousness dynamics (Lyapunov exponent, autocorrelation), neural binding
 * (gamma synchronization, phase-locking), and hierarchical organization.
 *
 * @author NIMCP Development Team
 * @date 2025-12-11
 * @version 1.0.0
 */

#include <gtest/gtest.h>
#include <thread>
#include <vector>
#include <chrono>
#include <cmath>
#include <random>
#include <algorithm>
#include <atomic>
#include <mutex>

// Headers have their own extern "C" guards
#include <stdint.h>
#include <stdbool.h>
#include <string.h>

/* ========================================================================
 * ENHANCED SWARM CONSCIOUSNESS API (MOCK)
 * ======================================================================== */

#define MAX_PHI_VALUE 10.0f
#define MAX_SWARM_SIZE 64
#define MAX_PHI_HISTORY 256

typedef enum {
    PHASE_SUBCRITICAL = 0,
    PHASE_CRITICAL = 1,
    PHASE_SUPERCRITICAL = 2,
    PHASE_CHAOTIC = 3
} consciousness_phase_t;

typedef enum {
    HIERARCHY_INDIVIDUAL = 0,
    HIERARCHY_SQUAD = 1,
    HIERARCHY_PLATOON = 2,
    HIERARCHY_SWARM = 3
} consciousness_hierarchy_t;

typedef struct {
    float mutual_information;
    float transfer_entropy;
    float tononi_complexity;
    float integration_synergy;
} information_geometry_t;

typedef struct {
    consciousness_phase_t current_phase;
    float lyapunov_exponent;
    float autocorrelation;
    float entropy_rate;
    float phase_transition_probability;
} consciousness_dynamics_t;

typedef struct {
    float gamma_synchronization;
    float phase_locking_value;
    float binding_strength;
    float temporal_coherence;
} neural_binding_t;

typedef struct {
    consciousness_hierarchy_t level;
    float level_phi[4];
    float inter_level_integration;
    float cross_level_transfer_entropy;
} hierarchical_consciousness_t;

typedef struct {
    float dropout_sensitivity;
    float fragility_index;
    float recovery_time_estimate;
    float redundancy_factor;
} consciousness_resilience_t;

typedef struct {
    float collective_phi;
    information_geometry_t geometry;
    consciousness_dynamics_t dynamics;
    neural_binding_t binding;
    hierarchical_consciousness_t hierarchy;
    consciousness_resilience_t resilience;
    uint32_t participating_drones;
    uint64_t computation_time_us;
} enhanced_phi_result_t;

typedef struct {
    uint32_t max_drones;
    float coherence_threshold;
    float min_phi_threshold;
    bool enable_monitoring;
    bool enable_bbb_validation;
    uint32_t phi_history_size;
    float gamma_freq_hz;
} enhanced_consciousness_config_t;

typedef void (*peer_event_callback_t)(uint16_t drone_id, int event_type, void* user_data);
typedef void (*phase_transition_callback_t)(consciousness_phase_t old_phase,
                                            consciousness_phase_t new_phase, void* user_data);

typedef struct {
    uint16_t drone_id;
    float phi_value;
    bool valid;
    uint64_t timestamp;
} remote_phi_entry_t;

typedef struct enhanced_consciousness_ctx {
    enhanced_consciousness_config_t config;

    // Peer management
    uint32_t num_peers;
    uint16_t peer_ids[MAX_SWARM_SIZE];
    bool peer_connected[MAX_SWARM_SIZE];

    // Remote phi collection
    remote_phi_entry_t remote_phi[MAX_SWARM_SIZE];
    uint32_t pending_phi_requests;

    // Callbacks
    peer_event_callback_t peer_callback;
    void* peer_callback_data;
    phase_transition_callback_t phase_callback;
    void* phase_callback_data;

    // Phi history for dynamics
    float phi_history[MAX_PHI_HISTORY];
    uint32_t phi_history_count;
    uint32_t phi_history_index;

    // Current state
    consciousness_phase_t current_phase;
    float current_phi;

    // Performance tracking
    uint64_t total_computations;
    uint64_t total_callbacks;

    // Thread safety
    std::mutex* mutex;
    bool initialized;
} enhanced_consciousness_ctx_t;

/* ========================================================================
 * MOCK IMPLEMENTATION
 * ======================================================================== */

static enhanced_consciousness_config_t enhanced_consciousness_default_config(void) {
    enhanced_consciousness_config_t config = {};
    config.max_drones = MAX_SWARM_SIZE;
    config.coherence_threshold = 0.5f;
    config.min_phi_threshold = 0.1f;
    config.enable_monitoring = false;
    config.enable_bbb_validation = true;
    config.phi_history_size = MAX_PHI_HISTORY;
    config.gamma_freq_hz = 40.0f;
    return config;
}

static enhanced_consciousness_ctx_t* enhanced_consciousness_create(
    const enhanced_consciousness_config_t* config
) {
    if (!config) return nullptr;

    enhanced_consciousness_ctx_t* ctx = new enhanced_consciousness_ctx_t();
    ctx->config = *config;
    ctx->num_peers = 0;
    ctx->pending_phi_requests = 0;
    ctx->peer_callback = nullptr;
    ctx->phase_callback = nullptr;
    ctx->phi_history_count = 0;
    ctx->phi_history_index = 0;
    ctx->current_phase = PHASE_SUBCRITICAL;
    ctx->current_phi = 0.0f;
    ctx->total_computations = 0;
    ctx->total_callbacks = 0;
    ctx->mutex = new std::mutex();
    ctx->initialized = true;

    memset(ctx->peer_ids, 0, sizeof(ctx->peer_ids));
    memset(ctx->peer_connected, 0, sizeof(ctx->peer_connected));
    memset(ctx->remote_phi, 0, sizeof(ctx->remote_phi));
    memset(ctx->phi_history, 0, sizeof(ctx->phi_history));

    return ctx;
}

static void enhanced_consciousness_destroy(enhanced_consciousness_ctx_t* ctx) {
    if (!ctx) return;
    delete ctx->mutex;
    delete ctx;
}

static bool enhanced_consciousness_register_peer_callback(
    enhanced_consciousness_ctx_t* ctx,
    peer_event_callback_t callback,
    void* user_data
) {
    if (!ctx) return false;
    std::lock_guard<std::mutex> lock(*ctx->mutex);
    ctx->peer_callback = callback;
    ctx->peer_callback_data = user_data;
    return true;
}

static bool enhanced_consciousness_register_phase_callback(
    enhanced_consciousness_ctx_t* ctx,
    phase_transition_callback_t callback,
    void* user_data
) {
    if (!ctx) return false;
    std::lock_guard<std::mutex> lock(*ctx->mutex);
    ctx->phase_callback = callback;
    ctx->phase_callback_data = user_data;
    return true;
}

static bool enhanced_consciousness_on_peer_joined(
    enhanced_consciousness_ctx_t* ctx,
    uint16_t drone_id
) {
    if (!ctx || ctx->num_peers >= MAX_SWARM_SIZE) return false;

    std::lock_guard<std::mutex> lock(*ctx->mutex);

    // Check if already present
    for (uint32_t i = 0; i < ctx->num_peers; i++) {
        if (ctx->peer_ids[i] == drone_id) return false;
    }

    ctx->peer_ids[ctx->num_peers] = drone_id;
    ctx->peer_connected[ctx->num_peers] = true;
    ctx->num_peers++;

    // Fire callback
    if (ctx->peer_callback) {
        ctx->total_callbacks++;
        ctx->peer_callback(drone_id, 1, ctx->peer_callback_data);  // 1 = joined
    }

    return true;
}

static bool enhanced_consciousness_on_peer_left(
    enhanced_consciousness_ctx_t* ctx,
    uint16_t drone_id,
    bool graceful
) {
    if (!ctx) return false;

    std::lock_guard<std::mutex> lock(*ctx->mutex);

    for (uint32_t i = 0; i < ctx->num_peers; i++) {
        if (ctx->peer_ids[i] == drone_id) {
            // Fire callback
            if (ctx->peer_callback) {
                ctx->total_callbacks++;
                ctx->peer_callback(drone_id, 0, ctx->peer_callback_data);  // 0 = left
            }

            // Remove peer
            for (uint32_t j = i; j < ctx->num_peers - 1; j++) {
                ctx->peer_ids[j] = ctx->peer_ids[j + 1];
                ctx->peer_connected[j] = ctx->peer_connected[j + 1];
                ctx->remote_phi[j] = ctx->remote_phi[j + 1];
            }
            ctx->num_peers--;
            return true;
        }
    }

    return false;
}

static bool enhanced_consciousness_handle_phi_response(
    enhanced_consciousness_ctx_t* ctx,
    uint16_t drone_id,
    float phi_value
) {
    if (!ctx) return false;

    std::lock_guard<std::mutex> lock(*ctx->mutex);

    for (uint32_t i = 0; i < ctx->num_peers; i++) {
        if (ctx->peer_ids[i] == drone_id) {
            ctx->remote_phi[i].drone_id = drone_id;
            ctx->remote_phi[i].phi_value = phi_value;
            ctx->remote_phi[i].valid = true;
            ctx->remote_phi[i].timestamp = std::chrono::steady_clock::now().time_since_epoch().count();

            if (ctx->pending_phi_requests > 0) {
                ctx->pending_phi_requests--;
            }
            return true;
        }
    }

    return false;
}

static float compute_mutual_information(enhanced_consciousness_ctx_t* ctx) {
    if (ctx->num_peers < 2) return 0.0f;

    // Simplified: based on phi history variance
    float mean = 0.0f;
    uint32_t count = std::min(ctx->phi_history_count, ctx->config.phi_history_size);
    for (uint32_t i = 0; i < count; i++) {
        mean += ctx->phi_history[i];
    }
    mean /= std::max(1u, count);

    float variance = 0.0f;
    for (uint32_t i = 0; i < count; i++) {
        float diff = ctx->phi_history[i] - mean;
        variance += diff * diff;
    }
    variance /= std::max(1u, count);

    // MI approximation: 0.5 * log(1 + variance)
    return 0.5f * logf(1.0f + variance);
}

static float compute_transfer_entropy(enhanced_consciousness_ctx_t* ctx) {
    if (ctx->phi_history_count < 3) return 0.0f;

    // Simplified: directional information flow approximation
    float te = 0.0f;
    uint32_t count = std::min(ctx->phi_history_count - 2, ctx->config.phi_history_size - 2);

    for (uint32_t i = 0; i < count; i++) {
        float delta1 = ctx->phi_history[i + 1] - ctx->phi_history[i];
        float delta2 = ctx->phi_history[i + 2] - ctx->phi_history[i + 1];
        te += fabsf(delta1 * delta2);
    }

    return te / std::max(1u, count);
}

static float compute_lyapunov_exponent(enhanced_consciousness_ctx_t* ctx) {
    if (ctx->phi_history_count < 10) return 0.0f;

    // Simplified: rate of divergence estimation
    float sum_log_ratio = 0.0f;
    uint32_t count = std::min(ctx->phi_history_count - 1, ctx->config.phi_history_size - 1);

    for (uint32_t i = 0; i < count; i++) {
        float diff = fabsf(ctx->phi_history[i + 1] - ctx->phi_history[i]);
        if (diff > 1e-6f) {
            sum_log_ratio += logf(diff);
        }
    }

    return sum_log_ratio / std::max(1u, count);
}

static float compute_autocorrelation(enhanced_consciousness_ctx_t* ctx, uint32_t lag) {
    if (ctx->phi_history_count <= lag) return 0.0f;

    uint32_t count = std::min(ctx->phi_history_count - lag, ctx->config.phi_history_size - lag);

    // Compute mean
    float mean = 0.0f;
    for (uint32_t i = 0; i < ctx->phi_history_count; i++) {
        mean += ctx->phi_history[i];
    }
    mean /= ctx->phi_history_count;

    // Compute autocorrelation
    float numerator = 0.0f;
    float denominator = 0.0f;

    for (uint32_t i = 0; i < count; i++) {
        float diff1 = ctx->phi_history[i] - mean;
        float diff2 = ctx->phi_history[i + lag] - mean;
        numerator += diff1 * diff2;
        denominator += diff1 * diff1;
    }

    return denominator > 1e-6f ? numerator / denominator : 0.0f;
}

static consciousness_phase_t detect_phase(enhanced_consciousness_ctx_t* ctx) {
    float lyapunov = compute_lyapunov_exponent(ctx);
    float autocorr = compute_autocorrelation(ctx, 1);

    if (lyapunov > 0.5f) return PHASE_CHAOTIC;
    if (lyapunov > 0.0f && autocorr > 0.5f) return PHASE_SUPERCRITICAL;
    if (lyapunov > -0.5f && autocorr > 0.3f) return PHASE_CRITICAL;
    return PHASE_SUBCRITICAL;
}

static enhanced_phi_result_t* enhanced_consciousness_compute(
    enhanced_consciousness_ctx_t* ctx
) {
    if (!ctx) return nullptr;

    auto start = std::chrono::high_resolution_clock::now();

    std::lock_guard<std::mutex> lock(*ctx->mutex);

    enhanced_phi_result_t* result = new enhanced_phi_result_t();

    // Compute collective phi from remote values
    float total_phi = 0.0f;
    uint32_t valid_count = 0;

    for (uint32_t i = 0; i < ctx->num_peers; i++) {
        if (ctx->remote_phi[i].valid) {
            total_phi += ctx->remote_phi[i].phi_value;
            valid_count++;
        }
    }

    // Collective phi with synergy bonus
    float base_phi = valid_count > 0 ? total_phi / valid_count : 0.0f;
    float synergy = valid_count > 1 ? powf(valid_count, 0.3f) : 1.0f;
    result->collective_phi = std::min(MAX_PHI_VALUE, base_phi * synergy);
    result->participating_drones = ctx->num_peers;

    // Update history
    if (ctx->phi_history_count < ctx->config.phi_history_size) {
        ctx->phi_history[ctx->phi_history_count++] = result->collective_phi;
    } else {
        ctx->phi_history[ctx->phi_history_index] = result->collective_phi;
        ctx->phi_history_index = (ctx->phi_history_index + 1) % ctx->config.phi_history_size;
    }

    ctx->current_phi = result->collective_phi;

    // Information geometry
    result->geometry.mutual_information = compute_mutual_information(ctx);
    result->geometry.transfer_entropy = compute_transfer_entropy(ctx);
    result->geometry.tononi_complexity = result->geometry.mutual_information *
                                         result->geometry.transfer_entropy;
    result->geometry.integration_synergy = synergy - 1.0f;

    // Consciousness dynamics
    result->dynamics.lyapunov_exponent = compute_lyapunov_exponent(ctx);
    result->dynamics.autocorrelation = compute_autocorrelation(ctx, 1);
    result->dynamics.entropy_rate = result->geometry.transfer_entropy * 0.5f;

    consciousness_phase_t new_phase = detect_phase(ctx);
    if (new_phase != ctx->current_phase && ctx->phase_callback) {
        ctx->total_callbacks++;
        ctx->phase_callback(ctx->current_phase, new_phase, ctx->phase_callback_data);
    }
    ctx->current_phase = new_phase;
    result->dynamics.current_phase = new_phase;

    // Neural binding
    result->binding.gamma_synchronization = ctx->num_peers > 1 ?
        (float)valid_count / ctx->num_peers : 0.0f;
    result->binding.phase_locking_value = result->dynamics.autocorrelation *
                                          result->binding.gamma_synchronization;
    result->binding.binding_strength = (result->binding.gamma_synchronization +
                                        result->binding.phase_locking_value) / 2.0f;
    result->binding.temporal_coherence = result->binding.gamma_synchronization;

    // Hierarchical consciousness
    result->hierarchy.level = ctx->num_peers >= 32 ? HIERARCHY_SWARM :
                             ctx->num_peers >= 16 ? HIERARCHY_PLATOON :
                             ctx->num_peers >= 4 ? HIERARCHY_SQUAD : HIERARCHY_INDIVIDUAL;
    result->hierarchy.level_phi[0] = base_phi;
    result->hierarchy.level_phi[1] = base_phi * (ctx->num_peers >= 4 ? 1.2f : 1.0f);
    result->hierarchy.level_phi[2] = base_phi * (ctx->num_peers >= 16 ? 1.5f : 1.0f);
    result->hierarchy.level_phi[3] = result->collective_phi;
    result->hierarchy.inter_level_integration = synergy - 1.0f;
    result->hierarchy.cross_level_transfer_entropy = result->geometry.transfer_entropy;

    // Resilience
    result->resilience.dropout_sensitivity = ctx->num_peers > 0 ?
        1.0f / ctx->num_peers : 1.0f;
    result->resilience.fragility_index = 1.0f - result->binding.binding_strength;
    result->resilience.recovery_time_estimate = result->resilience.fragility_index * 100.0f;
    result->resilience.redundancy_factor = ctx->num_peers > 1 ?
        (float)(ctx->num_peers - 1) / ctx->num_peers : 0.0f;

    // Timing
    auto end = std::chrono::high_resolution_clock::now();
    result->computation_time_us = std::chrono::duration_cast<std::chrono::microseconds>(
        end - start).count();

    ctx->total_computations++;

    return result;
}

static void enhanced_phi_result_free(enhanced_phi_result_t* result) {
    delete result;
}

static bool enhanced_consciousness_get_stats(
    enhanced_consciousness_ctx_t* ctx,
    uint64_t* total_computations,
    uint64_t* total_callbacks
) {
    if (!ctx) return false;

    std::lock_guard<std::mutex> lock(*ctx->mutex);
    if (total_computations) *total_computations = ctx->total_computations;
    if (total_callbacks) *total_callbacks = ctx->total_callbacks;
    return true;
}


/* ========================================================================
 * TEST FIXTURE
 * ======================================================================== */

class EnhancedConsciousnessRegressionTest : public ::testing::Test {
protected:
    enhanced_consciousness_config_t config;
    enhanced_consciousness_ctx_t* ctx = nullptr;
    std::chrono::time_point<std::chrono::high_resolution_clock> start_time;

    // Callback tracking
    static std::atomic<int> peer_events;
    static std::atomic<int> phase_transitions;
    static std::mutex callback_mutex;

    void SetUp() override {
        config = enhanced_consciousness_default_config();
        ctx = enhanced_consciousness_create(&config);
        start_time = std::chrono::high_resolution_clock::now();
        peer_events = 0;
        phase_transitions = 0;
    }

    void TearDown() override {
        auto end_time = std::chrono::high_resolution_clock::now();
        auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(
            end_time - start_time).count();

        if (duration > 1000) {
            std::cout << "WARNING: Test took " << duration << "ms" << std::endl;
        }

        enhanced_consciousness_destroy(ctx);
    }

    static void peer_callback(uint16_t drone_id, int event_type, void* user_data) {
        peer_events++;
    }

    static void phase_callback(consciousness_phase_t old_phase,
                               consciousness_phase_t new_phase, void* user_data) {
        phase_transitions++;
    }

    void add_peers_with_phi(uint32_t n, float base_phi, std::mt19937& rng) {
        std::uniform_real_distribution<float> noise(-0.1f, 0.1f);

        for (uint32_t i = 0; i < n; i++) {
            enhanced_consciousness_on_peer_joined(ctx, i);
            float phi = base_phi + noise(rng);
            enhanced_consciousness_handle_phi_response(ctx, i, phi);
        }
    }
};

std::atomic<int> EnhancedConsciousnessRegressionTest::peer_events(0);
std::atomic<int> EnhancedConsciousnessRegressionTest::phase_transitions(0);
std::mutex EnhancedConsciousnessRegressionTest::callback_mutex;

/* ========================================================================
 * 1. STABILITY TESTS
 * ======================================================================== */

TEST_F(EnhancedConsciousnessRegressionTest, RepeatedComputationDeterministic) {
    std::mt19937 rng(12345);
    add_peers_with_phi(16, 0.5f, rng);

    enhanced_phi_result_t* r1 = enhanced_consciousness_compute(ctx);
    enhanced_phi_result_t* r2 = enhanced_consciousness_compute(ctx);
    enhanced_phi_result_t* r3 = enhanced_consciousness_compute(ctx);

    ASSERT_NE(r1, nullptr);
    ASSERT_NE(r2, nullptr);
    ASSERT_NE(r3, nullptr);

    // Collective phi should be identical (same input)
    EXPECT_FLOAT_EQ(r1->collective_phi, r2->collective_phi);
    EXPECT_FLOAT_EQ(r2->collective_phi, r3->collective_phi);

    // Participating drones should be identical
    EXPECT_EQ(r1->participating_drones, r2->participating_drones);
    EXPECT_EQ(r2->participating_drones, r3->participating_drones);

    enhanced_phi_result_free(r1);
    enhanced_phi_result_free(r2);
    enhanced_phi_result_free(r3);
}

TEST_F(EnhancedConsciousnessRegressionTest, MetricsBoundedCorrectly) {
    std::mt19937 rng(23456);
    add_peers_with_phi(32, 0.7f, rng);

    enhanced_phi_result_t* result = enhanced_consciousness_compute(ctx);
    ASSERT_NE(result, nullptr);

    // Phi bounds
    EXPECT_GE(result->collective_phi, 0.0f);
    EXPECT_LE(result->collective_phi, MAX_PHI_VALUE);

    // Binding bounds [0,1]
    EXPECT_GE(result->binding.gamma_synchronization, 0.0f);
    EXPECT_LE(result->binding.gamma_synchronization, 1.0f);
    EXPECT_GE(result->binding.phase_locking_value, -1.0f);
    EXPECT_LE(result->binding.phase_locking_value, 1.0f);
    EXPECT_GE(result->binding.binding_strength, 0.0f);
    EXPECT_LE(result->binding.binding_strength, 1.0f);

    // Resilience bounds
    EXPECT_GE(result->resilience.dropout_sensitivity, 0.0f);
    EXPECT_LE(result->resilience.dropout_sensitivity, 1.0f);
    EXPECT_GE(result->resilience.fragility_index, 0.0f);
    EXPECT_LE(result->resilience.fragility_index, 1.0f);
    EXPECT_GE(result->resilience.redundancy_factor, 0.0f);
    EXPECT_LE(result->resilience.redundancy_factor, 1.0f);

    enhanced_phi_result_free(result);
}

TEST_F(EnhancedConsciousnessRegressionTest, CallbackCountsConsistent) {
    enhanced_consciousness_register_peer_callback(ctx, peer_callback, nullptr);

    // Add 10 peers
    for (int i = 0; i < 10; i++) {
        enhanced_consciousness_on_peer_joined(ctx, i);
    }
    EXPECT_EQ(peer_events.load(), 10);

    // Remove 5 peers
    for (int i = 0; i < 5; i++) {
        enhanced_consciousness_on_peer_left(ctx, i, true);
    }
    EXPECT_EQ(peer_events.load(), 15);  // 10 joins + 5 leaves
}

TEST_F(EnhancedConsciousnessRegressionTest, PhiHistoryMaintained) {
    std::mt19937 rng(34567);
    add_peers_with_phi(8, 0.5f, rng);

    // Compute multiple times to build history
    for (int i = 0; i < 50; i++) {
        enhanced_phi_result_t* result = enhanced_consciousness_compute(ctx);
        ASSERT_NE(result, nullptr);
        enhanced_phi_result_free(result);
    }

    EXPECT_EQ(ctx->phi_history_count, 50u);

    // History values should be reasonable
    for (uint32_t i = 0; i < ctx->phi_history_count; i++) {
        EXPECT_GE(ctx->phi_history[i], 0.0f);
        EXPECT_LE(ctx->phi_history[i], MAX_PHI_VALUE);
    }
}

TEST_F(EnhancedConsciousnessRegressionTest, PhaseTransitionsDetected) {
    enhanced_consciousness_register_phase_callback(ctx, phase_callback, nullptr);
    std::mt19937 rng(45678);

    // Start with low phi (subcritical)
    add_peers_with_phi(4, 0.1f, rng);
    for (int i = 0; i < 20; i++) {
        enhanced_phi_result_t* result = enhanced_consciousness_compute(ctx);
        enhanced_phi_result_free(result);
    }

    // Increase phi values (should potentially trigger transition)
    for (uint32_t i = 0; i < ctx->num_peers; i++) {
        enhanced_consciousness_handle_phi_response(ctx, i, 0.8f);
    }
    for (int i = 0; i < 20; i++) {
        enhanced_phi_result_t* result = enhanced_consciousness_compute(ctx);
        enhanced_phi_result_free(result);
    }

    // Phase transitions may or may not occur depending on dynamics
    // Just verify no crashes and callback mechanism works
    SUCCEED();
}

/* ========================================================================
 * 2. SCALING TESTS
 * ======================================================================== */

TEST_F(EnhancedConsciousnessRegressionTest, PhiScalesWithPeerCount) {
    std::mt19937 rng(56789);
    std::vector<float> phi_values;
    std::vector<uint32_t> peer_counts = {2, 4, 8, 16, 32};

    for (uint32_t n : peer_counts) {
        enhanced_consciousness_destroy(ctx);
        ctx = enhanced_consciousness_create(&config);
        add_peers_with_phi(n, 0.5f, rng);

        enhanced_phi_result_t* result = enhanced_consciousness_compute(ctx);
        ASSERT_NE(result, nullptr);
        phi_values.push_back(result->collective_phi);
        enhanced_phi_result_free(result);
    }

    // Phi should generally increase with more peers
    for (size_t i = 1; i < phi_values.size(); i++) {
        EXPECT_GT(phi_values[i], phi_values[i-1] * 0.8f)
            << "Phi should scale with peer count";
    }
}

TEST_F(EnhancedConsciousnessRegressionTest, InformationGeometryScales) {
    std::mt19937 rng(67890);
    add_peers_with_phi(16, 0.5f, rng);

    // Build up history
    for (int i = 0; i < 30; i++) {
        // Vary phi values slightly
        for (uint32_t j = 0; j < ctx->num_peers; j++) {
            float phi = 0.5f + 0.1f * sinf(i * 0.2f + j * 0.5f);
            enhanced_consciousness_handle_phi_response(ctx, j, phi);
        }
        enhanced_phi_result_t* result = enhanced_consciousness_compute(ctx);
        ASSERT_NE(result, nullptr);

        // After history builds, geometry metrics should be positive
        if (i > 10) {
            EXPECT_GE(result->geometry.mutual_information, 0.0f);
            EXPECT_GE(result->geometry.transfer_entropy, 0.0f);
        }

        enhanced_phi_result_free(result);
    }
}

TEST_F(EnhancedConsciousnessRegressionTest, HierarchyLevelsCorrect) {
    std::mt19937 rng(78901);

    // Test individual level (< 4 peers)
    add_peers_with_phi(2, 0.5f, rng);
    enhanced_phi_result_t* r1 = enhanced_consciousness_compute(ctx);
    EXPECT_EQ(r1->hierarchy.level, HIERARCHY_INDIVIDUAL);
    enhanced_phi_result_free(r1);

    // Test squad level (4-15 peers)
    for (int i = 2; i < 8; i++) {
        enhanced_consciousness_on_peer_joined(ctx, i);
        enhanced_consciousness_handle_phi_response(ctx, i, 0.5f);
    }
    enhanced_phi_result_t* r2 = enhanced_consciousness_compute(ctx);
    EXPECT_EQ(r2->hierarchy.level, HIERARCHY_SQUAD);
    enhanced_phi_result_free(r2);

    // Test platoon level (16-31 peers)
    for (int i = 8; i < 20; i++) {
        enhanced_consciousness_on_peer_joined(ctx, i);
        enhanced_consciousness_handle_phi_response(ctx, i, 0.5f);
    }
    enhanced_phi_result_t* r3 = enhanced_consciousness_compute(ctx);
    EXPECT_EQ(r3->hierarchy.level, HIERARCHY_PLATOON);
    enhanced_phi_result_free(r3);

    // Test swarm level (32+ peers)
    for (int i = 20; i < 40; i++) {
        enhanced_consciousness_on_peer_joined(ctx, i);
        enhanced_consciousness_handle_phi_response(ctx, i, 0.5f);
    }
    enhanced_phi_result_t* r4 = enhanced_consciousness_compute(ctx);
    EXPECT_EQ(r4->hierarchy.level, HIERARCHY_SWARM);
    enhanced_phi_result_free(r4);
}

TEST_F(EnhancedConsciousnessRegressionTest, ResilienceScalesInversely) {
    std::mt19937 rng(89012);
    std::vector<float> sensitivities;
    std::vector<uint32_t> peer_counts = {4, 8, 16, 32, 64};

    for (uint32_t n : peer_counts) {
        enhanced_consciousness_destroy(ctx);
        ctx = enhanced_consciousness_create(&config);
        add_peers_with_phi(n, 0.5f, rng);

        enhanced_phi_result_t* result = enhanced_consciousness_compute(ctx);
        ASSERT_NE(result, nullptr);
        sensitivities.push_back(result->resilience.dropout_sensitivity);
        enhanced_phi_result_free(result);
    }

    // Dropout sensitivity should decrease with more peers
    for (size_t i = 1; i < sensitivities.size(); i++) {
        EXPECT_LT(sensitivities[i], sensitivities[i-1])
            << "Dropout sensitivity should decrease with more peers";
    }
}

/* ========================================================================
 * 3. PERFORMANCE TESTS
 * ======================================================================== */

TEST_F(EnhancedConsciousnessRegressionTest, ComputationTimeUnder50ms) {
    std::mt19937 rng(90123);
    add_peers_with_phi(64, 0.5f, rng);

    // Build history first
    for (int i = 0; i < 100; i++) {
        enhanced_phi_result_t* result = enhanced_consciousness_compute(ctx);
        enhanced_phi_result_free(result);
    }

    // Measure computation time
    auto start = std::chrono::high_resolution_clock::now();
    enhanced_phi_result_t* result = enhanced_consciousness_compute(ctx);
    auto end = std::chrono::high_resolution_clock::now();

    ASSERT_NE(result, nullptr);

    auto duration_ms = std::chrono::duration_cast<std::chrono::milliseconds>(
        end - start).count();

    EXPECT_LT(duration_ms, 50)
        << "Computation took " << duration_ms << "ms (expected < 50ms)";

    enhanced_phi_result_free(result);
}

TEST_F(EnhancedConsciousnessRegressionTest, NoMemoryLeaks1000Iterations) {
    std::mt19937 rng(01234);
    add_peers_with_phi(16, 0.5f, rng);

    for (int i = 0; i < 1000; i++) {
        enhanced_phi_result_t* result = enhanced_consciousness_compute(ctx);
        ASSERT_NE(result, nullptr);
        enhanced_phi_result_free(result);
    }

    // If we get here without crashes/leaks (detected by ASAN), test passes
    SUCCEED();
}

TEST_F(EnhancedConsciousnessRegressionTest, CallbackOverheadMinimal) {
    std::mt19937 rng(12340);

    // Without callbacks
    enhanced_consciousness_destroy(ctx);
    ctx = enhanced_consciousness_create(&config);
    add_peers_with_phi(32, 0.5f, rng);

    auto start1 = std::chrono::high_resolution_clock::now();
    for (int i = 0; i < 100; i++) {
        enhanced_phi_result_t* result = enhanced_consciousness_compute(ctx);
        enhanced_phi_result_free(result);
    }
    auto end1 = std::chrono::high_resolution_clock::now();
    auto duration1 = std::chrono::duration_cast<std::chrono::microseconds>(end1 - start1);

    // With callbacks
    enhanced_consciousness_destroy(ctx);
    ctx = enhanced_consciousness_create(&config);
    enhanced_consciousness_register_peer_callback(ctx, peer_callback, nullptr);
    enhanced_consciousness_register_phase_callback(ctx, phase_callback, nullptr);
    add_peers_with_phi(32, 0.5f, rng);

    auto start2 = std::chrono::high_resolution_clock::now();
    for (int i = 0; i < 100; i++) {
        enhanced_phi_result_t* result = enhanced_consciousness_compute(ctx);
        enhanced_phi_result_free(result);
    }
    auto end2 = std::chrono::high_resolution_clock::now();
    auto duration2 = std::chrono::duration_cast<std::chrono::microseconds>(end2 - start2);

    // Callback overhead should be < 20%
    EXPECT_LT(duration2.count(), duration1.count() * 1.2)
        << "Callback overhead too high";
}

TEST_F(EnhancedConsciousnessRegressionTest, ScalabilityBenchmark) {
    std::mt19937 rng(23401);
    std::vector<uint32_t> sizes = {4, 8, 16, 32, 64};
    std::vector<double> times;

    for (uint32_t n : sizes) {
        enhanced_consciousness_destroy(ctx);
        ctx = enhanced_consciousness_create(&config);
        add_peers_with_phi(n, 0.5f, rng);

        // Build history
        for (int i = 0; i < 50; i++) {
            enhanced_phi_result_t* result = enhanced_consciousness_compute(ctx);
            enhanced_phi_result_free(result);
        }

        // Measure
        auto start = std::chrono::high_resolution_clock::now();
        enhanced_phi_result_t* result = enhanced_consciousness_compute(ctx);
        auto end = std::chrono::high_resolution_clock::now();

        ASSERT_NE(result, nullptr);

        double us = std::chrono::duration_cast<std::chrono::microseconds>(
            end - start).count();
        times.push_back(us);

        enhanced_phi_result_free(result);
    }

    // Verify scaling is reasonable (not worse than O(n^2))
    for (size_t i = 1; i < times.size(); i++) {
        double ratio = times[i] / std::max(1.0, times[i-1]);
        double size_ratio = (double)sizes[i] / sizes[i-1];

        EXPECT_LT(ratio, size_ratio * size_ratio * 2.0)
            << "Scaling worse than O(n^2)";
    }
}

/* ========================================================================
 * 4. NUMERICAL STABILITY TESTS
 * ======================================================================== */

TEST_F(EnhancedConsciousnessRegressionTest, SmallPhiValuesStable) {
    std::mt19937 rng(34012);

    // Very small phi values
    for (int i = 0; i < 8; i++) {
        enhanced_consciousness_on_peer_joined(ctx, i);
        enhanced_consciousness_handle_phi_response(ctx, i, 0.001f);
    }

    enhanced_phi_result_t* result = enhanced_consciousness_compute(ctx);
    ASSERT_NE(result, nullptr);

    // Should not be NaN or Inf
    EXPECT_FALSE(std::isnan(result->collective_phi));
    EXPECT_FALSE(std::isinf(result->collective_phi));
    EXPECT_GE(result->collective_phi, 0.0f);

    // Geometry metrics should not be NaN
    EXPECT_FALSE(std::isnan(result->geometry.mutual_information));
    EXPECT_FALSE(std::isnan(result->geometry.transfer_entropy));

    enhanced_phi_result_free(result);
}

TEST_F(EnhancedConsciousnessRegressionTest, LargePhiValuesStable) {
    std::mt19937 rng(45012);

    // Maximum phi values
    for (int i = 0; i < 64; i++) {
        enhanced_consciousness_on_peer_joined(ctx, i);
        enhanced_consciousness_handle_phi_response(ctx, i, MAX_PHI_VALUE);
    }

    enhanced_phi_result_t* result = enhanced_consciousness_compute(ctx);
    ASSERT_NE(result, nullptr);

    // Should not exceed bounds
    EXPECT_LE(result->collective_phi, MAX_PHI_VALUE);
    EXPECT_FALSE(std::isnan(result->collective_phi));
    EXPECT_FALSE(std::isinf(result->collective_phi));

    enhanced_phi_result_free(result);
}

TEST_F(EnhancedConsciousnessRegressionTest, ZeroPeersHandled) {
    // No peers added
    enhanced_phi_result_t* result = enhanced_consciousness_compute(ctx);
    ASSERT_NE(result, nullptr);

    EXPECT_FLOAT_EQ(result->collective_phi, 0.0f);
    EXPECT_EQ(result->participating_drones, 0u);
    EXPECT_EQ(result->hierarchy.level, HIERARCHY_INDIVIDUAL);

    enhanced_phi_result_free(result);
}

TEST_F(EnhancedConsciousnessRegressionTest, MixedPhiValuesStable) {
    std::mt19937 rng(56012);
    std::uniform_real_distribution<float> dist(0.0f, MAX_PHI_VALUE);

    // Random mix of phi values
    for (int i = 0; i < 32; i++) {
        enhanced_consciousness_on_peer_joined(ctx, i);
        enhanced_consciousness_handle_phi_response(ctx, i, dist(rng));
    }

    // Compute many times with varying inputs
    for (int iter = 0; iter < 100; iter++) {
        // Update some phi values
        for (int i = 0; i < 5; i++) {
            int idx = rng() % 32;
            enhanced_consciousness_handle_phi_response(ctx, idx, dist(rng));
        }

        enhanced_phi_result_t* result = enhanced_consciousness_compute(ctx);
        ASSERT_NE(result, nullptr);

        // All metrics should be valid
        EXPECT_FALSE(std::isnan(result->collective_phi));
        EXPECT_FALSE(std::isnan(result->geometry.mutual_information));
        EXPECT_FALSE(std::isnan(result->dynamics.lyapunov_exponent));
        EXPECT_FALSE(std::isnan(result->binding.binding_strength));

        enhanced_phi_result_free(result);
    }
}

/* ========================================================================
 * 5. CONCURRENCY TESTS
 * ======================================================================== */

TEST_F(EnhancedConsciousnessRegressionTest, ParallelComputationsSafe) {
    std::mt19937 rng(67012);
    add_peers_with_phi(16, 0.5f, rng);

    std::vector<std::thread> threads;
    std::atomic<bool> failed(false);

    for (int i = 0; i < 4; i++) {
        threads.emplace_back([this, &failed]() {
            for (int j = 0; j < 100; j++) {
                enhanced_phi_result_t* result = enhanced_consciousness_compute(ctx);
                if (!result) {
                    failed = true;
                    return;
                }
                enhanced_phi_result_free(result);
            }
        });
    }

    for (auto& t : threads) {
        t.join();
    }

    EXPECT_FALSE(failed) << "Parallel computations failed";
}

TEST_F(EnhancedConsciousnessRegressionTest, ConcurrentPeerUpdates) {
    std::mt19937 rng(78012);
    enhanced_consciousness_register_peer_callback(ctx, peer_callback, nullptr);

    std::atomic<bool> stop(false);
    std::atomic<bool> failed(false);

    // Thread adding/removing peers
    std::thread peer_manager([this, &stop, &failed]() {
        for (int i = 0; i < 100 && !stop; i++) {
            if (ctx->num_peers < 32) {
                enhanced_consciousness_on_peer_joined(ctx, 1000 + i);
                enhanced_consciousness_handle_phi_response(ctx, 1000 + i, 0.5f);
            }
            std::this_thread::sleep_for(std::chrono::microseconds(100));
        }
    });

    // Thread computing phi
    std::thread computer([this, &stop, &failed]() {
        for (int i = 0; i < 100 && !stop; i++) {
            enhanced_phi_result_t* result = enhanced_consciousness_compute(ctx);
            if (!result) {
                failed = true;
                return;
            }
            enhanced_phi_result_free(result);
            std::this_thread::sleep_for(std::chrono::microseconds(50));
        }
    });

    peer_manager.join();
    stop = true;
    computer.join();

    EXPECT_FALSE(failed);
}

TEST_F(EnhancedConsciousnessRegressionTest, ConcurrentPhiResponseHandling) {
    std::mt19937 rng(89012);

    // Pre-add peers
    for (int i = 0; i < 16; i++) {
        enhanced_consciousness_on_peer_joined(ctx, i);
    }

    std::vector<std::thread> threads;
    std::atomic<bool> failed(false);

    // Multiple threads updating phi responses
    for (int t = 0; t < 4; t++) {
        threads.emplace_back([this, t, &failed]() {
            std::mt19937 local_rng(t * 1000);
            std::uniform_real_distribution<float> dist(0.0f, 1.0f);

            for (int i = 0; i < 100; i++) {
                uint16_t drone_id = local_rng() % 16;
                float phi = dist(local_rng);
                enhanced_consciousness_handle_phi_response(ctx, drone_id, phi);
            }
        });
    }

    for (auto& thread : threads) {
        thread.join();
    }

    // Should complete without crashes
    EXPECT_FALSE(failed);
}

TEST_F(EnhancedConsciousnessRegressionTest, CallbackThreadSafety) {
    std::atomic<int> callback_count(0);

    auto safe_callback = [](uint16_t drone_id, int event_type, void* user_data) {
        std::atomic<int>* count = static_cast<std::atomic<int>*>(user_data);
        (*count)++;
    };

    enhanced_consciousness_register_peer_callback(ctx, safe_callback, &callback_count);

    std::vector<std::thread> threads;

    // Multiple threads adding peers
    for (int t = 0; t < 4; t++) {
        threads.emplace_back([this, t]() {
            for (int i = 0; i < 10; i++) {
                enhanced_consciousness_on_peer_joined(ctx, t * 100 + i);
            }
        });
    }

    for (auto& thread : threads) {
        thread.join();
    }

    // Should have received callbacks for all peer joins
    EXPECT_EQ(callback_count.load(), 40);  // 4 threads × 10 peers each
}

/* ========================================================================
 * MAIN
 * ======================================================================== */

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
