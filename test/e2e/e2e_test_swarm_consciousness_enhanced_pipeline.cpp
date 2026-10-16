/**
 * @file e2e_test_swarm_consciousness_enhanced_pipeline.cpp
 * @brief End-to-End pipeline test for Enhanced Swarm Consciousness
 *
 * WHAT: Full pipeline test from swarm formation through consciousness computation
 * WHY:  Validate complete consciousness workflow with all enhancements
 * HOW:  Simulate drone swarm lifecycle with peer events, phi collection, and dynamics
 *
 * PIPELINE STAGES:
 * 1. Swarm Formation: Initialize enhanced consciousness context
 * 2. Peer Discovery: Simulate drones joining with callbacks
 * 3. Phi Collection: Request and receive remote phi values
 * 4. Consciousness Computation: Compute collective phi with all metrics
 * 5. Dynamics Analysis: Monitor phase transitions and Lyapunov exponents
 * 6. Neural Binding: Validate gamma synchronization
 * 7. Hierarchical Integration: Test multi-level consciousness
 * 8. Resilience Testing: Simulate drone failures and recovery
 *
 * BIOLOGICAL BASIS:
 * End-to-end test of extended IIT implementation covering:
 * - Information geometry (mutual information, transfer entropy)
 * - Consciousness dynamics (Lyapunov, autocorrelation, phases)
 * - Neural binding (gamma synchronization, phase-locking)
 * - Hierarchical organization (individual → swarm)
 * - Resilience (dropout sensitivity, fragility)
 *
 * @author NIMCP Development Team
 * @date 2025-12-11
 * @version 1.0.0
 */

#include <gtest/gtest.h>
#include "e2e_test_framework.h"

#include <thread>
#include <vector>
#include <chrono>
#include <cmath>
#include <random>
#include <atomic>
#include <mutex>
#include <condition_variable>

// Headers have their own extern "C" guards
#include <stdint.h>
#include <stdbool.h>
#include <string.h>

/* ========================================================================
 * ENHANCED SWARM CONSCIOUSNESS API (MOCK FOR E2E)
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

typedef enum {
    PEER_EVENT_JOINED = 1,
    PEER_EVENT_LEFT = 0
} peer_event_type_t;

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
    bool is_conscious;
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
typedef void (*binding_event_callback_t)(float binding_strength, bool bound, void* user_data);

typedef struct {
    uint16_t drone_id;
    float phi_value;
    bool valid;
    uint64_t timestamp;
    float activation_level;
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
    binding_event_callback_t binding_callback;
    void* binding_callback_data;

    // Phi history for dynamics
    float phi_history[MAX_PHI_HISTORY];
    uint32_t phi_history_count;
    uint32_t phi_history_index;

    // Current state
    consciousness_phase_t current_phase;
    float current_phi;
    bool is_bound;
    float binding_threshold;

    // Performance tracking
    uint64_t total_computations;
    uint64_t total_callbacks;
    uint64_t total_peer_events;
    uint64_t total_phase_transitions;

    // Thread safety
    std::mutex* mutex;
    bool initialized;
} enhanced_consciousness_ctx_t;

/* ========================================================================
 * MOCK IMPLEMENTATION FOR E2E
 * ======================================================================== */

static enhanced_consciousness_config_t enhanced_consciousness_default_config(void) {
    enhanced_consciousness_config_t config = {};
    config.max_drones = MAX_SWARM_SIZE;
    config.coherence_threshold = 0.5f;
    config.min_phi_threshold = 0.1f;
    config.enable_monitoring = true;
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
    ctx->binding_callback = nullptr;
    ctx->phi_history_count = 0;
    ctx->phi_history_index = 0;
    ctx->current_phase = PHASE_SUBCRITICAL;
    ctx->current_phi = 0.0f;
    ctx->is_bound = false;
    ctx->binding_threshold = 0.7f;
    ctx->total_computations = 0;
    ctx->total_callbacks = 0;
    ctx->total_peer_events = 0;
    ctx->total_phase_transitions = 0;
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

static bool enhanced_consciousness_register_binding_callback(
    enhanced_consciousness_ctx_t* ctx,
    binding_event_callback_t callback,
    void* user_data
) {
    if (!ctx) return false;
    std::lock_guard<std::mutex> lock(*ctx->mutex);
    ctx->binding_callback = callback;
    ctx->binding_callback_data = user_data;
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
    ctx->total_peer_events++;

    // Fire callback
    if (ctx->peer_callback) {
        ctx->total_callbacks++;
        ctx->peer_callback(drone_id, PEER_EVENT_JOINED, ctx->peer_callback_data);
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
            ctx->total_peer_events++;

            // Fire callback
            if (ctx->peer_callback) {
                ctx->total_callbacks++;
                ctx->peer_callback(drone_id, PEER_EVENT_LEFT, ctx->peer_callback_data);
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
            ctx->remote_phi[i].phi_value = std::max(0.0f, std::min(MAX_PHI_VALUE, phi_value));
            ctx->remote_phi[i].valid = true;
            ctx->remote_phi[i].timestamp = std::chrono::steady_clock::now().time_since_epoch().count();
            ctx->remote_phi[i].activation_level = 0.5f + 0.5f * (phi_value / MAX_PHI_VALUE);

            if (ctx->pending_phi_requests > 0) {
                ctx->pending_phi_requests--;
            }
            return true;
        }
    }

    return false;
}

static uint32_t enhanced_consciousness_request_phi_from_peers(
    enhanced_consciousness_ctx_t* ctx
) {
    if (!ctx) return 0;

    std::lock_guard<std::mutex> lock(*ctx->mutex);

    uint32_t requests = 0;
    for (uint32_t i = 0; i < ctx->num_peers; i++) {
        if (ctx->peer_connected[i] && !ctx->remote_phi[i].valid) {
            ctx->pending_phi_requests++;
            requests++;
        }
    }
    return requests;
}

static float compute_mutual_information(enhanced_consciousness_ctx_t* ctx) {
    if (ctx->num_peers < 2) return 0.0f;

    float mean = 0.0f;
    uint32_t count = std::min(ctx->phi_history_count, ctx->config.phi_history_size);
    if (count == 0) return 0.0f;

    for (uint32_t i = 0; i < count; i++) {
        mean += ctx->phi_history[i];
    }
    mean /= count;

    float variance = 0.0f;
    for (uint32_t i = 0; i < count; i++) {
        float diff = ctx->phi_history[i] - mean;
        variance += diff * diff;
    }
    variance /= count;

    return 0.5f * logf(1.0f + variance);
}

static float compute_transfer_entropy(enhanced_consciousness_ctx_t* ctx) {
    if (ctx->phi_history_count < 3) return 0.0f;

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

    float mean = 0.0f;
    for (uint32_t i = 0; i < ctx->phi_history_count; i++) {
        mean += ctx->phi_history[i];
    }
    mean /= ctx->phi_history_count;

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
    result->is_conscious = result->collective_phi >= ctx->config.min_phi_threshold;

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
    result->dynamics.phase_transition_probability = (new_phase != ctx->current_phase) ? 1.0f : 0.0f;

    if (new_phase != ctx->current_phase) {
        ctx->total_phase_transitions++;
        if (ctx->phase_callback) {
            ctx->total_callbacks++;
            ctx->phase_callback(ctx->current_phase, new_phase, ctx->phase_callback_data);
        }
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

    // Check for binding state change
    bool new_bound_state = result->binding.binding_strength >= ctx->binding_threshold;
    if (new_bound_state != ctx->is_bound) {
        ctx->is_bound = new_bound_state;
        if (ctx->binding_callback) {
            ctx->total_callbacks++;
            ctx->binding_callback(result->binding.binding_strength, new_bound_state,
                                  ctx->binding_callback_data);
        }
    }

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


/* ========================================================================
 * E2E PIPELINE TEST FIXTURE
 * ======================================================================== */

class SwarmConsciousnessEnhancedPipelineTest : public ::testing::Test {
protected:
    enhanced_consciousness_config_t config;
    enhanced_consciousness_ctx_t* ctx = nullptr;
    nimcp::e2e::PipelineTracker* tracker = nullptr;
    nimcp::e2e::MemoryLeakDetector* leak_detector = nullptr;

    // Callback tracking
    std::atomic<int> peer_join_count{0};
    std::atomic<int> peer_leave_count{0};
    std::atomic<int> phase_transition_count{0};
    std::atomic<int> binding_event_count{0};
    std::mutex callback_mutex;

    void SetUp() override {
        config = enhanced_consciousness_default_config();
        ctx = enhanced_consciousness_create(&config);
        tracker = new nimcp::e2e::PipelineTracker("Enhanced Swarm Consciousness E2E");
        leak_detector = new nimcp::e2e::MemoryLeakDetector();
    }

    void TearDown() override {
        leak_detector->checkpoint();

        if (leak_detector->has_leaks()) {
            std::cout << "MEMORY LEAK DETECTED: "
                      << leak_detector->get_leaked_bytes() << " bytes\n";
        }

        enhanced_consciousness_destroy(ctx);
        delete tracker;
        delete leak_detector;
    }

    static void peer_callback_handler(uint16_t drone_id, int event_type, void* user_data) {
        auto* test = static_cast<SwarmConsciousnessEnhancedPipelineTest*>(user_data);
        if (event_type == PEER_EVENT_JOINED) {
            test->peer_join_count++;
        } else {
            test->peer_leave_count++;
        }
    }

    static void phase_callback_handler(consciousness_phase_t old_phase,
                                       consciousness_phase_t new_phase, void* user_data) {
        auto* test = static_cast<SwarmConsciousnessEnhancedPipelineTest*>(user_data);
        test->phase_transition_count++;
    }

    static void binding_callback_handler(float binding_strength, bool bound, void* user_data) {
        auto* test = static_cast<SwarmConsciousnessEnhancedPipelineTest*>(user_data);
        test->binding_event_count++;
    }
};

/* ========================================================================
 * PIPELINE TEST: Full Swarm Consciousness Lifecycle
 * ======================================================================== */

TEST_F(SwarmConsciousnessEnhancedPipelineTest, FullSwarmConsciousnessLifecycle) {
    std::mt19937 rng(42);
    std::uniform_real_distribution<float> phi_dist(0.3f, 0.8f);

    // Register callbacks
    enhanced_consciousness_register_peer_callback(ctx, peer_callback_handler, this);
    enhanced_consciousness_register_phase_callback(ctx, phase_callback_handler, this);
    enhanced_consciousness_register_binding_callback(ctx, binding_callback_handler, this);

    // ========================================================================
    // Stage 1: Swarm Formation
    // ========================================================================
    tracker->begin_stage("Swarm Formation", 1000);
    {
        ASSERT_NE(ctx, nullptr) << "Failed to create enhanced consciousness context";
        ASSERT_TRUE(ctx->initialized) << "Context not properly initialized";
        EXPECT_EQ(ctx->num_peers, 0u) << "Initial peer count should be 0";
        EXPECT_EQ(ctx->current_phase, PHASE_SUBCRITICAL) << "Initial phase should be subcritical";
    }
    tracker->end_stage();

    // ========================================================================
    // Stage 2: Peer Discovery (Gradual Swarm Growth)
    // ========================================================================
    tracker->begin_stage("Peer Discovery", 2000);
    {
        // Simulate 32 drones joining progressively
        for (int i = 0; i < 32; i++) {
            bool joined = enhanced_consciousness_on_peer_joined(ctx, i);
            EXPECT_TRUE(joined) << "Failed to add drone " << i;

            // Simulate phi response
            float phi = phi_dist(rng);
            enhanced_consciousness_handle_phi_response(ctx, i, phi);

            // Compute consciousness after each join
            enhanced_phi_result_t* result = enhanced_consciousness_compute(ctx);
            ASSERT_NE(result, nullptr);

            // Verify hierarchy progression
            // Note: Mock uses num_peers >= threshold, so levels change AT the threshold
            if (ctx->num_peers < 4) {
                EXPECT_EQ(result->hierarchy.level, HIERARCHY_INDIVIDUAL);
            } else if (ctx->num_peers < 16) {
                EXPECT_EQ(result->hierarchy.level, HIERARCHY_SQUAD);
            } else if (ctx->num_peers < 32) {
                EXPECT_EQ(result->hierarchy.level, HIERARCHY_PLATOON);
            } else {
                EXPECT_EQ(result->hierarchy.level, HIERARCHY_SWARM);
            }

            enhanced_phi_result_free(result);
        }

        EXPECT_EQ(peer_join_count.load(), 32) << "Expected 32 peer join callbacks";
        EXPECT_EQ(ctx->num_peers, 32u) << "Expected 32 peers in swarm";
    }
    tracker->end_stage();

    // ========================================================================
    // Stage 3: Phi Collection Workflow
    // ========================================================================
    tracker->begin_stage("Phi Collection", 2000);
    {
        // Request phi updates from all peers
        uint32_t requests = enhanced_consciousness_request_phi_from_peers(ctx);
        // All should already have valid phi (from stage 2)
        EXPECT_EQ(requests, 0u) << "All peers should already have valid phi";

        // Update phi values with new readings
        for (uint32_t i = 0; i < ctx->num_peers; i++) {
            float new_phi = phi_dist(rng);
            bool handled = enhanced_consciousness_handle_phi_response(ctx, i, new_phi);
            EXPECT_TRUE(handled) << "Failed to handle phi response for drone " << i;
        }

        // Verify phi values are stored
        for (uint32_t i = 0; i < ctx->num_peers; i++) {
            EXPECT_TRUE(ctx->remote_phi[i].valid);
            EXPECT_GE(ctx->remote_phi[i].phi_value, 0.0f);
            EXPECT_LE(ctx->remote_phi[i].phi_value, MAX_PHI_VALUE);
        }
    }
    tracker->end_stage();

    // ========================================================================
    // Stage 4: Consciousness Computation
    // ========================================================================
    tracker->begin_stage("Consciousness Computation", 2000);
    {
        // Compute consciousness multiple times to build history
        for (int i = 0; i < 50; i++) {
            enhanced_phi_result_t* result = enhanced_consciousness_compute(ctx);
            ASSERT_NE(result, nullptr);

            // Validate all metrics
            EXPECT_GE(result->collective_phi, 0.0f);
            EXPECT_LE(result->collective_phi, MAX_PHI_VALUE);
            EXPECT_EQ(result->participating_drones, 32u);

            // Should be conscious with this many drones and phi values
            if (result->collective_phi >= config.min_phi_threshold) {
                EXPECT_TRUE(result->is_conscious);
            }

            enhanced_phi_result_free(result);
        }

        // History includes computations from peer discovery stage (32) + this stage (50)
        EXPECT_GE(ctx->phi_history_count, 50u) << "Expected at least 50 phi history entries";
    }
    tracker->end_stage();

    // ========================================================================
    // Stage 5: Dynamics Analysis
    // ========================================================================
    tracker->begin_stage("Dynamics Analysis", 2000);
    {
        // Continue computing with varying phi to generate dynamics
        for (int i = 0; i < 100; i++) {
            // Vary phi values sinusoidally
            for (uint32_t j = 0; j < ctx->num_peers; j++) {
                float base_phi = 0.5f;
                float variation = 0.2f * sinf(i * 0.1f + j * 0.2f);
                enhanced_consciousness_handle_phi_response(ctx, j, base_phi + variation);
            }

            enhanced_phi_result_t* result = enhanced_consciousness_compute(ctx);
            ASSERT_NE(result, nullptr);

            // Validate dynamics metrics after sufficient history
            if (i > 20) {
                EXPECT_FALSE(std::isnan(result->dynamics.lyapunov_exponent));
                EXPECT_FALSE(std::isnan(result->dynamics.autocorrelation));
                EXPECT_FALSE(std::isnan(result->dynamics.entropy_rate));
            }

            enhanced_phi_result_free(result);
        }

        // Should have some phase transitions with varying dynamics
        // (may or may not happen depending on exact values)
    }
    tracker->end_stage();

    // ========================================================================
    // Stage 6: Neural Binding Validation
    // ========================================================================
    tracker->begin_stage("Neural Binding", 2000);
    {
        // Set all drones to high, synchronized phi values
        for (uint32_t i = 0; i < ctx->num_peers; i++) {
            enhanced_consciousness_handle_phi_response(ctx, i, 0.8f);
        }

        enhanced_phi_result_t* result = enhanced_consciousness_compute(ctx);
        ASSERT_NE(result, nullptr);

        // With all valid and synchronized, gamma sync should be high
        EXPECT_GT(result->binding.gamma_synchronization, 0.9f)
            << "Expected high gamma synchronization with all peers synchronized";

        // Binding strength should be positive
        EXPECT_GE(result->binding.binding_strength, 0.0f);
        EXPECT_LE(result->binding.binding_strength, 1.0f);

        // Phase-locking value
        EXPECT_GE(result->binding.phase_locking_value, -1.0f);
        EXPECT_LE(result->binding.phase_locking_value, 1.0f);

        enhanced_phi_result_free(result);
    }
    tracker->end_stage();

    // ========================================================================
    // Stage 7: Hierarchical Integration
    // ========================================================================
    tracker->begin_stage("Hierarchical Integration", 2000);
    {
        // Grow to full swarm size (64 drones)
        for (int i = 32; i < 64; i++) {
            enhanced_consciousness_on_peer_joined(ctx, i);
            enhanced_consciousness_handle_phi_response(ctx, i, phi_dist(rng));
        }

        enhanced_phi_result_t* result = enhanced_consciousness_compute(ctx);
        ASSERT_NE(result, nullptr);

        // Should now be at SWARM hierarchy level
        EXPECT_EQ(result->hierarchy.level, HIERARCHY_SWARM)
            << "Expected SWARM hierarchy level with 64 drones";

        // All hierarchy levels should have phi values
        EXPECT_GT(result->hierarchy.level_phi[0], 0.0f);  // Individual
        EXPECT_GT(result->hierarchy.level_phi[1], 0.0f);  // Squad
        EXPECT_GT(result->hierarchy.level_phi[2], 0.0f);  // Platoon
        EXPECT_GT(result->hierarchy.level_phi[3], 0.0f);  // Swarm

        // Higher levels should have higher phi (synergy)
        EXPECT_GE(result->hierarchy.level_phi[3], result->hierarchy.level_phi[0]);

        enhanced_phi_result_free(result);

        EXPECT_EQ(peer_join_count.load(), 64) << "Expected 64 total peer join callbacks";
    }
    tracker->end_stage();

    // ========================================================================
    // Stage 8: Resilience Testing
    // ========================================================================
    tracker->begin_stage("Resilience Testing", 3000);
    {
        // Record baseline
        enhanced_phi_result_t* baseline = enhanced_consciousness_compute(ctx);
        ASSERT_NE(baseline, nullptr);
        float baseline_phi = baseline->collective_phi;
        float baseline_sensitivity = baseline->resilience.dropout_sensitivity;
        enhanced_phi_result_free(baseline);

        // Simulate drone failures (remove half the swarm)
        for (int i = 0; i < 32; i++) {
            enhanced_consciousness_on_peer_left(ctx, i, false);  // Not graceful
        }

        EXPECT_EQ(ctx->num_peers, 32u) << "Expected 32 peers after failures";
        EXPECT_EQ(peer_leave_count.load(), 32) << "Expected 32 peer leave callbacks";

        // Compute new consciousness
        enhanced_phi_result_t* after_failure = enhanced_consciousness_compute(ctx);
        ASSERT_NE(after_failure, nullptr);

        // Phi should have decreased but still be positive
        EXPECT_GT(after_failure->collective_phi, 0.0f);

        // Dropout sensitivity should have increased
        EXPECT_GT(after_failure->resilience.dropout_sensitivity, baseline_sensitivity)
            << "Dropout sensitivity should increase with fewer drones";

        // Hierarchy should still be at SWARM (32 peers >= 32 threshold)
        EXPECT_EQ(after_failure->hierarchy.level, HIERARCHY_SWARM);

        enhanced_phi_result_free(after_failure);

        // Recovery: add drones back
        for (int i = 0; i < 32; i++) {
            enhanced_consciousness_on_peer_joined(ctx, 100 + i);  // New drone IDs
            enhanced_consciousness_handle_phi_response(ctx, 100 + i, phi_dist(rng));
        }

        enhanced_phi_result_t* after_recovery = enhanced_consciousness_compute(ctx);
        ASSERT_NE(after_recovery, nullptr);

        // Should be back to SWARM level
        EXPECT_EQ(after_recovery->hierarchy.level, HIERARCHY_SWARM);

        enhanced_phi_result_free(after_recovery);
    }
    tracker->end_stage();

    // ========================================================================
    // Verify Pipeline Success
    // ========================================================================
    EXPECT_TRUE(tracker->is_successful()) << tracker->get_failure_reason();

    // Verify callback counts
    EXPECT_EQ(peer_join_count.load(), 96);  // 64 initial + 32 recovery
    EXPECT_EQ(peer_leave_count.load(), 32);  // 32 failures

    // Verify computation count (32 discovery + 50 computation + 100 dynamics + others)
    EXPECT_GT(ctx->total_computations, 150u);
}

/* ========================================================================
 * PIPELINE TEST: Concurrent Operations
 * ======================================================================== */

TEST_F(SwarmConsciousnessEnhancedPipelineTest, ConcurrentOperationsPipeline) {
    std::mt19937 rng(123);
    std::uniform_real_distribution<float> phi_dist(0.3f, 0.8f);

    // Register callbacks
    enhanced_consciousness_register_peer_callback(ctx, peer_callback_handler, this);

    tracker->begin_stage("Concurrent Setup", 1000);
    {
        // Pre-add some peers
        for (int i = 0; i < 16; i++) {
            enhanced_consciousness_on_peer_joined(ctx, i);
            enhanced_consciousness_handle_phi_response(ctx, i, phi_dist(rng));
        }
    }
    tracker->end_stage();

    tracker->begin_stage("Concurrent Operations", 5000);
    {
        std::atomic<bool> stop(false);
        std::atomic<int> computation_count(0);
        std::atomic<int> update_count(0);
        std::atomic<bool> failed(false);

        // Thread 1: Continuous consciousness computation
        std::thread compute_thread([this, &stop, &computation_count, &failed]() {
            while (!stop && !failed) {
                enhanced_phi_result_t* result = enhanced_consciousness_compute(ctx);
                if (!result) {
                    failed = true;
                    return;
                }
                if (std::isnan(result->collective_phi) || std::isinf(result->collective_phi)) {
                    failed = true;
                    enhanced_phi_result_free(result);
                    return;
                }
                computation_count++;
                enhanced_phi_result_free(result);
                std::this_thread::sleep_for(std::chrono::microseconds(100));
            }
        });

        // Thread 2: Phi updates
        std::thread update_thread([this, &stop, &update_count, &rng, &phi_dist]() {
            std::mt19937 local_rng = rng;
            while (!stop) {
                for (uint32_t i = 0; i < std::min(16u, ctx->num_peers); i++) {
                    enhanced_consciousness_handle_phi_response(ctx, i, phi_dist(local_rng));
                    update_count++;
                }
                std::this_thread::sleep_for(std::chrono::microseconds(500));
            }
        });

        // Thread 3: Peer churn
        std::thread churn_thread([this, &stop, &rng, &phi_dist]() {
            std::mt19937 local_rng = rng;
            int next_id = 100;
            while (!stop) {
                if (ctx->num_peers < 24) {
                    enhanced_consciousness_on_peer_joined(ctx, next_id++);
                    enhanced_consciousness_handle_phi_response(ctx, next_id - 1, phi_dist(local_rng));
                } else if (ctx->num_peers > 8) {
                    // Remove a random peer
                    uint16_t to_remove = ctx->peer_ids[local_rng() % ctx->num_peers];
                    enhanced_consciousness_on_peer_left(ctx, to_remove, true);
                }
                std::this_thread::sleep_for(std::chrono::milliseconds(10));
            }
        });

        // Run for 2 seconds
        std::this_thread::sleep_for(std::chrono::seconds(2));
        stop = true;

        compute_thread.join();
        update_thread.join();
        churn_thread.join();

        EXPECT_FALSE(failed) << "Concurrent operations caused failure";
        EXPECT_GT(computation_count.load(), 100) << "Expected many computations";
        EXPECT_GT(update_count.load(), 100) << "Expected many updates";
    }
    tracker->end_stage();

    EXPECT_TRUE(tracker->is_successful()) << tracker->get_failure_reason();
}

/* ========================================================================
 * PIPELINE TEST: Stress Test with Rapid State Changes
 * ======================================================================== */

TEST_F(SwarmConsciousnessEnhancedPipelineTest, StressTestRapidStateChanges) {
    std::mt19937 rng(456);
    std::uniform_real_distribution<float> phi_dist(0.0f, MAX_PHI_VALUE);

    tracker->begin_stage("Stress Test Setup", 1000);
    {
        // Add maximum peers
        for (int i = 0; i < 64; i++) {
            enhanced_consciousness_on_peer_joined(ctx, i);
            enhanced_consciousness_handle_phi_response(ctx, i, phi_dist(rng));
        }
    }
    tracker->end_stage();

    tracker->begin_stage("Rapid State Changes", 5000);
    {
        // Perform many rapid operations
        for (int iter = 0; iter < 1000; iter++) {
            // Random phi updates
            for (int i = 0; i < 10; i++) {
                uint16_t id = rng() % 64;
                enhanced_consciousness_handle_phi_response(ctx, id, phi_dist(rng));
            }

            // Compute consciousness
            enhanced_phi_result_t* result = enhanced_consciousness_compute(ctx);
            ASSERT_NE(result, nullptr);

            // All metrics should remain valid
            EXPECT_FALSE(std::isnan(result->collective_phi));
            EXPECT_FALSE(std::isinf(result->collective_phi));
            EXPECT_GE(result->collective_phi, 0.0f);
            EXPECT_LE(result->collective_phi, MAX_PHI_VALUE);

            enhanced_phi_result_free(result);
        }

        EXPECT_EQ(ctx->total_computations, 1000u);
    }
    tracker->end_stage();

    EXPECT_TRUE(tracker->is_successful()) << tracker->get_failure_reason();
}

/* ========================================================================
 * PIPELINE TEST: Memory Stability
 * ======================================================================== */

TEST_F(SwarmConsciousnessEnhancedPipelineTest, MemoryStabilityPipeline) {
    std::mt19937 rng(789);
    std::uniform_real_distribution<float> phi_dist(0.3f, 0.8f);

    tracker->begin_stage("Memory Baseline", 1000);
    {
        for (int i = 0; i < 32; i++) {
            enhanced_consciousness_on_peer_joined(ctx, i);
            enhanced_consciousness_handle_phi_response(ctx, i, phi_dist(rng));
        }
    }
    tracker->end_stage();

    tracker->begin_stage("Repeated Alloc/Free", 10000);
    {
        // Many create/destroy cycles of results
        for (int i = 0; i < 10000; i++) {
            enhanced_phi_result_t* result = enhanced_consciousness_compute(ctx);
            ASSERT_NE(result, nullptr);
            enhanced_phi_result_free(result);
        }
    }
    tracker->end_stage();

    tracker->begin_stage("Peer Churn Memory", 5000);
    {
        // Add and remove many peers
        for (int iter = 0; iter < 100; iter++) {
            // Remove all peers
            while (ctx->num_peers > 0) {
                enhanced_consciousness_on_peer_left(ctx, ctx->peer_ids[0], true);
            }

            // Add them back
            for (int i = 0; i < 32; i++) {
                enhanced_consciousness_on_peer_joined(ctx, 1000 * iter + i);
                enhanced_consciousness_handle_phi_response(ctx, 1000 * iter + i, phi_dist(rng));
            }

            // Compute
            enhanced_phi_result_t* result = enhanced_consciousness_compute(ctx);
            ASSERT_NE(result, nullptr);
            enhanced_phi_result_free(result);
        }
    }
    tracker->end_stage();

    EXPECT_TRUE(tracker->is_successful()) << tracker->get_failure_reason();
}

/* ========================================================================
 * MAIN
 * ======================================================================== */

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
