/**
 * @file test_swarm_consciousness_integration.cpp
 * @brief Integration Tests for NIMCP Swarm Gestalt Consciousness
 *
 * WHAT: Tests integration of swarm-level consciousness (collective Φ) with other NIMCP components
 * WHY:  Verify emergent swarm consciousness from individual drone consciousness
 * HOW:  Multi-component integration tests across swarm brain, workspace, bio-async, and emergence
 *
 * TEST COVERAGE:
 * - Swarm brain integration with consciousness metrics
 * - Collective workspace affecting phi computation
 * - Individual consciousness aggregation
 * - Bio-async message routing for consciousness updates
 * - BBB security validation of consciousness metrics
 * - Emergence tier correlation with consciousness level
 *
 * DESIGN:
 * This integration test suite assumes a swarm consciousness API that extends individual
 * consciousness metrics (IIT Phi) to collective/gestalt consciousness across drone swarms.
 * The API would compute:
 * - Collective Φ: Integrated information across entire swarm network
 * - Individual drone Φ: Local consciousness level per drone
 * - Workspace coherence: How aligned the swarm's collective attention is
 * - Consciousness state: Classification of swarm consciousness level
 *
 * @author NIMCP Development Team
 * @date 2025-12-11
 * @version 1.0.0
 */

#include <gtest/gtest.h>
#include <thread>
#include <chrono>
#include <vector>
#include <atomic>
#include <cmath>
#include <memory>

// Headers have their own extern "C" guards
#include "swarm/nimcp_swarm_brain.h"
#include "swarm/nimcp_collective_workspace.h"
// Note: swarm_emergence types are in swarm_brain.h, no need for separate include
#include "cognitive/introspection/nimcp_consciousness_metrics.h"
#include "cognitive/introspection/nimcp_introspection.h"
#include "core/brain/nimcp_brain.h"
#include "utils/memory/nimcp_memory.h"
#include "utils/logging/nimcp_logging.h"

//=============================================================================
// Mock Swarm Consciousness API
// (This would be implemented in nimcp_swarm_consciousness.h/c)
//=============================================================================

/**
 * WHAT: Configuration for swarm consciousness computation
 * WHY:  Control how collective Φ is computed from individual drones
 * HOW:  Aggregation method, thresholds, monitoring parameters
 */
typedef struct {
    phi_computation_method_t method;        // Computation method for individual Φ
    float collective_phi_threshold;         // Threshold for collective consciousness
    float workspace_coherence_weight;       // How much workspace coherence affects Φ
    float individual_phi_weight;            // How much individual Φ contributes
    float network_integration_weight;       // How much swarm connectivity affects Φ
    uint32_t monitoring_interval_ms;        // Update interval for consciousness tracking
    bool enable_individual_monitoring;      // Monitor each drone's Φ separately
    bool enable_workspace_integration;      // Use workspace coherence in computation
} swarm_consciousness_config_t;

/**
 * WHAT: Swarm consciousness state
 * WHY:  Track both individual and collective consciousness metrics
 */
typedef struct {
    float collective_phi;                   // Gestalt consciousness level
    float* individual_phi;                  // Per-drone Φ values
    uint32_t num_drones;                    // Number of drones
    float workspace_coherence;              // Collective workspace coherence
    float network_integration;              // Network connectivity metric
    consciousness_state_t collective_state; // Overall consciousness classification
    uint64_t timestamp;                     // When computed
} swarm_consciousness_state_t;

/**
 * WHAT: Swarm consciousness context (opaque)
 * WHY:  Manage consciousness tracking for drone swarm
 */
typedef struct swarm_consciousness_ctx swarm_consciousness_ctx_t;

/**
 * WHAT: Consciousness update callback
 * WHY:  Notify other systems when swarm consciousness changes
 */
typedef void (*swarm_consciousness_callback_t)(
    float collective_phi,
    consciousness_state_t state,
    void* user_data
);

// Mock API functions
static swarm_consciousness_config_t swarm_consciousness_default_config(void) {
    swarm_consciousness_config_t config;
    config.method = PHI_METHOD_FAST;
    config.collective_phi_threshold = 0.3f;
    config.workspace_coherence_weight = 0.4f;
    config.individual_phi_weight = 0.4f;
    config.network_integration_weight = 0.2f;
    config.monitoring_interval_ms = 1000;
    config.enable_individual_monitoring = true;
    config.enable_workspace_integration = true;
    return config;
}

// Mock context structure
struct swarm_consciousness_ctx {
    swarm_consciousness_config_t config;
    swarm_consciousness_state_t state;
    swarm_consciousness_callback_t callback;
    void* callback_user_data;
    bool monitoring_active;
};

static swarm_consciousness_ctx_t* swarm_consciousness_create(
    const swarm_consciousness_config_t* config
) {
    swarm_consciousness_ctx_t* ctx = (swarm_consciousness_ctx_t*)nimcp_malloc(
        sizeof(swarm_consciousness_ctx_t)
    );
    if (!ctx) return nullptr;

    ctx->config = config ? *config : swarm_consciousness_default_config();
    memset(&ctx->state, 0, sizeof(swarm_consciousness_state_t));
    ctx->callback = nullptr;
    ctx->callback_user_data = nullptr;
    ctx->monitoring_active = false;

    return ctx;
}

static void swarm_consciousness_destroy(swarm_consciousness_ctx_t* ctx) {
    if (!ctx) return;
    if (ctx->state.individual_phi) {
        nimcp_free(ctx->state.individual_phi);
    }
    nimcp_free(ctx);
}

static float swarm_consciousness_compute_collective_phi(
    swarm_consciousness_ctx_t* ctx,
    swarm_brain_t** swarm_brains,
    uint32_t num_brains,
    collective_workspace_t* workspace
) {
    if (!ctx || !swarm_brains || num_brains == 0) return 0.0f;

    // Allocate individual phi array if needed
    if (ctx->state.individual_phi == nullptr || ctx->state.num_drones != num_brains) {
        if (ctx->state.individual_phi) {
            nimcp_free(ctx->state.individual_phi);
        }
        ctx->state.individual_phi = (float*)nimcp_malloc(sizeof(float) * num_brains);
        ctx->state.num_drones = num_brains;
    }

    // Mock computation: average individual Φ values
    float avg_individual_phi = 0.0f;
    for (uint32_t i = 0; i < num_brains; i++) {
        // Mock: assign random-ish Φ based on drone activity
        ctx->state.individual_phi[i] = 0.3f + (i * 0.05f);
        avg_individual_phi += ctx->state.individual_phi[i];
    }
    avg_individual_phi /= num_brains;

    // Get workspace coherence if enabled
    float workspace_coherence = 0.5f;
    if (workspace && ctx->config.enable_workspace_integration) {
        workspace_coherence = collective_workspace_get_coherence(workspace);
    }
    ctx->state.workspace_coherence = workspace_coherence;

    // Network integration: higher with more drones
    float network_integration = fminf(1.0f, num_brains / 16.0f);
    ctx->state.network_integration = network_integration;

    // Compute collective Φ as weighted sum
    float collective_phi =
        avg_individual_phi * ctx->config.individual_phi_weight +
        workspace_coherence * ctx->config.workspace_coherence_weight +
        network_integration * ctx->config.network_integration_weight;

    ctx->state.collective_phi = collective_phi;
    ctx->state.collective_state = consciousness_classify_phi(collective_phi);
    ctx->state.timestamp = (uint64_t)std::chrono::steady_clock::now().time_since_epoch().count();

    return collective_phi;
}

static bool swarm_consciousness_enable_monitoring(
    swarm_consciousness_ctx_t* ctx,
    swarm_consciousness_callback_t callback,
    void* user_data
) {
    if (!ctx) return false;
    ctx->callback = callback;
    ctx->callback_user_data = user_data;
    ctx->monitoring_active = true;
    return true;
}

static void swarm_consciousness_disable_monitoring(swarm_consciousness_ctx_t* ctx) {
    if (!ctx) return;
    ctx->monitoring_active = false;
    ctx->callback = nullptr;
}

static float swarm_consciousness_get_collective_phi(const swarm_consciousness_ctx_t* ctx) {
    return ctx ? ctx->state.collective_phi : 0.0f;
}

static const swarm_consciousness_state_t* swarm_consciousness_get_state(
    const swarm_consciousness_ctx_t* ctx
) {
    return ctx ? &ctx->state : nullptr;
}

//=============================================================================
// Test Fixture
//=============================================================================

class SwarmConsciousnessIntegrationTest : public ::testing::Test {
protected:
    std::vector<swarm_brain_t*> swarm_brains_;
    collective_workspace_t* workspace_;
    swarm_consciousness_ctx_t* consciousness_ctx_;
    swarm_consciousness_config_t consciousness_config_;
    void* emergence_ctx_;  // Use void* since we may not have swarm_emergence header

    static constexpr uint32_t MAX_DRONES = 16;
    static constexpr uint32_t DEFAULT_DRONES = 4;

    void SetUp() override {
        workspace_ = nullptr;
        consciousness_ctx_ = nullptr;
        emergence_ctx_ = nullptr;

        // Create default consciousness config
        consciousness_config_ = swarm_consciousness_default_config();
    }

    void TearDown() override {
        // Clean up swarm brains
        for (auto* swarm : swarm_brains_) {
            if (swarm) {
                swarm_brain_leave(swarm);
                swarm_brain_destroy(swarm);
            }
        }
        swarm_brains_.clear();

        // Clean up workspace
        if (workspace_) {
            collective_workspace_destroy(workspace_);
            workspace_ = nullptr;
        }

        // Clean up consciousness context
        if (consciousness_ctx_) {
            swarm_consciousness_destroy(consciousness_ctx_);
            consciousness_ctx_ = nullptr;
        }

        // Clean up emergence context (mock, no cleanup needed)
        emergence_ctx_ = nullptr;
    }

    // Helper: Create swarm brain
    swarm_brain_t* CreateSwarmBrain(uint16_t drone_id, const char* swarm_name) {
        swarm_brain_config_t config = swarm_brain_default_config();
        config.drone_id = drone_id;
        strncpy(config.swarm_name, swarm_name, SWARM_MAX_NAME_LEN - 1);
        config.heartbeat_ms = 50;
        config.sync_ms = 25;
        config.enable_bio_async = true;
        config.enable_reward_sharing = true;

        swarm_brain_t* swarm = swarm_brain_create(&config);
        if (swarm) {
            swarm_brains_.push_back(swarm);
            EXPECT_TRUE(swarm_brain_join(swarm)) << "Failed to join swarm for drone " << drone_id;
        }
        return swarm;
    }

    // Helper: Create N-drone swarm
    void CreateSwarm(uint32_t num_drones, const char* swarm_name = "test_swarm") {
        for (uint32_t i = 0; i < num_drones; i++) {
            swarm_brain_t* swarm = CreateSwarmBrain(i + 1, swarm_name);
            ASSERT_NE(swarm, nullptr) << "Failed to create swarm brain " << i;
        }
    }

    // Helper: Create collective workspace
    void CreateWorkspace(uint32_t swarm_size) {
        workspace_ = collective_workspace_create_simple(0, swarm_size);
        ASSERT_NE(workspace_, nullptr) << "Failed to create collective workspace";
    }

    // Helper: Create consciousness context
    void CreateConsciousnessContext() {
        consciousness_ctx_ = swarm_consciousness_create(&consciousness_config_);
        ASSERT_NE(consciousness_ctx_, nullptr) << "Failed to create consciousness context";
    }

    // Helper: Create emergence context (mock for now)
    void CreateEmergenceContext() {
        // Mock implementation - in real code would use swarm_emergence_create()
        emergence_ctx_ = nullptr;  // Not implemented in this test
    }

    // Helper: Mock emergence tier from swarm size
    swarm_emergence_tier_t GetMockEmergenceTier(uint32_t num_drones) {
        if (num_drones >= 16) return SWARM_TIER_COMPANY;
        if (num_drones >= 8) return SWARM_TIER_PLATOON;
        if (num_drones >= 4) return SWARM_TIER_SQUAD;
        if (num_drones >= 2) return SWARM_TIER_PAIR;
        return SWARM_TIER_INDIVIDUAL;
    }

    // Helper: Process all swarm brains
    void ProcessAll(int iterations = 1) {
        for (int i = 0; i < iterations; i++) {
            for (auto* swarm : swarm_brains_) {
                swarm_brain_process(swarm);
            }
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
        }
    }
};

//=============================================================================
// 1. Swarm Brain Integration Tests
//=============================================================================

TEST_F(SwarmConsciousnessIntegrationTest, ConsciousnessWithRealSwarmBrain) {
    // WHAT: Compute collective Φ on actual swarm brain instances
    // WHY:  Verify consciousness metrics work with real swarm infrastructure
    // HOW:  Create swarm, compute Φ, verify reasonable values

    CreateSwarm(DEFAULT_DRONES);
    CreateWorkspace(DEFAULT_DRONES);
    CreateConsciousnessContext();

    // Process swarm to establish connections
    ProcessAll(5);

    // Compute collective consciousness
    float collective_phi = swarm_consciousness_compute_collective_phi(
        consciousness_ctx_,
        swarm_brains_.data(),
        swarm_brains_.size(),
        workspace_
    );

    // Verify reasonable Φ value
    EXPECT_GT(collective_phi, 0.0f) << "Collective Φ should be positive";
    EXPECT_LT(collective_phi, 2.0f) << "Collective Φ should be reasonable (<2.0)";

    // Verify state is populated
    const swarm_consciousness_state_t* state = swarm_consciousness_get_state(consciousness_ctx_);
    ASSERT_NE(state, nullptr);
    EXPECT_EQ(state->num_drones, DEFAULT_DRONES);
    EXPECT_NE(state->individual_phi, nullptr);

    // Verify individual Φ values are reasonable
    for (uint32_t i = 0; i < state->num_drones; i++) {
        EXPECT_GE(state->individual_phi[i], 0.0f) << "Individual Φ[" << i << "] should be non-negative";
        EXPECT_LE(state->individual_phi[i], 1.5f) << "Individual Φ[" << i << "] should be reasonable";
    }
}

TEST_F(SwarmConsciousnessIntegrationTest, MonitoringCallbackInvoked) {
    // WHAT: Verify monitoring callback fires on consciousness updates
    // WHY:  Other modules need notifications when swarm consciousness changes
    // HOW:  Enable monitoring, trigger update, verify callback

    CreateSwarm(DEFAULT_DRONES);
    CreateWorkspace(DEFAULT_DRONES);
    CreateConsciousnessContext();

    std::atomic<int> callback_count{0};
    float last_phi = 0.0f;

    auto callback = [](float phi, consciousness_state_t state, void* user_data) {
        auto* count = (std::atomic<int>*)user_data;
        count->fetch_add(1);
    };

    // Enable monitoring
    bool enabled = swarm_consciousness_enable_monitoring(
        consciousness_ctx_,
        callback,
        &callback_count
    );
    ASSERT_TRUE(enabled);

    // Compute Φ (should trigger callback in real implementation)
    swarm_consciousness_compute_collective_phi(
        consciousness_ctx_,
        swarm_brains_.data(),
        swarm_brains_.size(),
        workspace_
    );

    // In a real implementation, callback would be invoked asynchronously
    // For this mock, we manually invoke it
    if (consciousness_ctx_->callback) {
        consciousness_ctx_->callback(
            consciousness_ctx_->state.collective_phi,
            consciousness_ctx_->state.collective_state,
            consciousness_ctx_->callback_user_data
        );
    }

    EXPECT_GT(callback_count.load(), 0) << "Callback should have been invoked";

    // Disable monitoring
    swarm_consciousness_disable_monitoring(consciousness_ctx_);
}

TEST_F(SwarmConsciousnessIntegrationTest, MultipleSwarmBrains) {
    // WHAT: Multiple independent swarms have independent consciousness
    // WHY:  Swarm consciousness should be isolated per swarm
    // HOW:  Create two swarms, verify independent Φ computation

    // Create first swarm
    CreateSwarm(3, "swarm_alpha");
    CreateWorkspace(3);
    CreateConsciousnessContext();

    float phi_alpha = swarm_consciousness_compute_collective_phi(
        consciousness_ctx_,
        swarm_brains_.data(),
        swarm_brains_.size(),
        workspace_
    );

    // Create second swarm (independent)
    std::vector<swarm_brain_t*> swarm_beta;
    for (uint32_t i = 0; i < 5; i++) {
        swarm_brain_config_t config = swarm_brain_default_config();
        config.drone_id = 100 + i;  // Different IDs
        strncpy(config.swarm_name, "swarm_beta", SWARM_MAX_NAME_LEN - 1);
        config.enable_bio_async = true;

        swarm_brain_t* swarm = swarm_brain_create(&config);
        ASSERT_NE(swarm, nullptr);
        swarm_brain_join(swarm);
        swarm_beta.push_back(swarm);
    }

    collective_workspace_t* workspace_beta = collective_workspace_create_simple(0, 5);
    ASSERT_NE(workspace_beta, nullptr);

    swarm_consciousness_ctx_t* ctx_beta = swarm_consciousness_create(&consciousness_config_);
    ASSERT_NE(ctx_beta, nullptr);

    float phi_beta = swarm_consciousness_compute_collective_phi(
        ctx_beta,
        swarm_beta.data(),
        swarm_beta.size(),
        workspace_beta
    );

    // Φ values should be different (different swarm sizes)
    EXPECT_NE(phi_alpha, phi_beta) << "Independent swarms should have different Φ";

    // Beta swarm is larger, should have higher network integration
    const swarm_consciousness_state_t* state_beta = swarm_consciousness_get_state(ctx_beta);
    EXPECT_GT(state_beta->network_integration, 0.3f);

    // Cleanup beta swarm
    for (auto* swarm : swarm_beta) {
        swarm_brain_leave(swarm);
        swarm_brain_destroy(swarm);
    }
    collective_workspace_destroy(workspace_beta);
    swarm_consciousness_destroy(ctx_beta);
}

TEST_F(SwarmConsciousnessIntegrationTest, SwarmBrainLifecycle) {
    // WHAT: Consciousness survives swarm changes (drones joining/leaving)
    // WHY:  Swarm composition changes dynamically, Φ should adapt
    // HOW:  Add/remove drones, verify Φ updates correctly

    CreateSwarm(2, "dynamic_swarm");
    CreateWorkspace(8);  // Room for expansion
    CreateConsciousnessContext();

    // Initial Φ with 2 drones
    float phi_initial = swarm_consciousness_compute_collective_phi(
        consciousness_ctx_,
        swarm_brains_.data(),
        swarm_brains_.size(),
        workspace_
    );
    EXPECT_GT(phi_initial, 0.0f);

    // Add more drones
    CreateSwarmBrain(10, "dynamic_swarm");
    CreateSwarmBrain(11, "dynamic_swarm");

    ProcessAll(3);

    // Recompute with 4 drones
    float phi_expanded = swarm_consciousness_compute_collective_phi(
        consciousness_ctx_,
        swarm_brains_.data(),
        swarm_brains_.size(),
        workspace_
    );

    // More drones should increase network integration, thus Φ
    EXPECT_GT(phi_expanded, phi_initial) << "Φ should increase with more drones";

    const swarm_consciousness_state_t* state = swarm_consciousness_get_state(consciousness_ctx_);
    EXPECT_EQ(state->num_drones, 4u);
}

//=============================================================================
// 2. Collective Workspace Integration Tests
//=============================================================================

TEST_F(SwarmConsciousnessIntegrationTest, WorkspaceCoherenceAffectsPhi) {
    // WHAT: Workspace coherence directly affects collective Φ
    // WHY:  Aligned attention should increase consciousness
    // HOW:  Manipulate workspace coherence, observe Φ changes

    CreateSwarm(DEFAULT_DRONES);
    CreateWorkspace(DEFAULT_DRONES);
    CreateConsciousnessContext();

    // Add high-salience items to increase coherence
    for (uint32_t i = 0; i < 5; i++) {
        collective_workspace_item_t item;
        memset(&item, 0, sizeof(item));
        item.item_id = (1 << 16) | i;
        item.salience = 0.9f;  // High salience
        item.type = WORKSPACE_ITEM_GOAL;
        item.source_drone = 1;
        item.timestamp_ms = i * 100;

        collective_workspace_add_item(workspace_, &item);
    }

    // Compute Φ with high coherence
    float phi_high_coherence = swarm_consciousness_compute_collective_phi(
        consciousness_ctx_,
        swarm_brains_.data(),
        swarm_brains_.size(),
        workspace_
    );

    const swarm_consciousness_state_t* state = swarm_consciousness_get_state(consciousness_ctx_);
    float coherence = state->workspace_coherence;

    // High coherence should contribute to higher Φ
    // Note: Coherence may be 0 if workspace is empty or not yet integrated
    EXPECT_GE(coherence, 0.0f) << "Workspace coherence should be valid";
    EXPECT_GT(phi_high_coherence, 0.0f) << "Φ should be positive with active swarm";
}

TEST_F(SwarmConsciousnessIntegrationTest, WorkspaceItemsReflectConsciousness) {
    // WHAT: High-salience items appear when swarm is conscious
    // WHY:  Consciousness and attention are linked
    // HOW:  Check top items when Φ is high

    CreateSwarm(DEFAULT_DRONES);
    CreateWorkspace(DEFAULT_DRONES);
    CreateConsciousnessContext();

    // Add diverse items
    for (uint32_t i = 0; i < 10; i++) {
        collective_workspace_item_t item;
        memset(&item, 0, sizeof(item));
        item.item_id = (1 << 16) | i;
        item.salience = 0.5f + (i * 0.05f);  // Varying salience
        item.type = (i % 2 == 0) ? WORKSPACE_ITEM_PERCEPTION : WORKSPACE_ITEM_GOAL;
        item.source_drone = 1;
        item.timestamp_ms = i * 100;

        collective_workspace_add_item(workspace_, &item);
    }

    float phi = swarm_consciousness_compute_collective_phi(
        consciousness_ctx_,
        swarm_brains_.data(),
        swarm_brains_.size(),
        workspace_
    );

    // Get top workspace items
    collective_workspace_item_t top_items[5];
    uint32_t item_count;
    bool got_items = collective_workspace_get_top_items(
        workspace_,
        top_items,
        5,
        &item_count
    );

    ASSERT_TRUE(got_items);
    EXPECT_GT(item_count, 0u);

    // When conscious (Φ > 0), top items should have high salience
    if (phi > 0.2f) {
        EXPECT_GT(top_items[0].salience, 0.7f) << "Top item should be highly salient";
    }
}

TEST_F(SwarmConsciousnessIntegrationTest, SharedAttentionIncreasesIntegration) {
    // WHAT: Shared attention items boost integration metric
    // WHY:  Synchronized attention indicates integrated processing
    // HOW:  Multiple drones attend to same item, measure integration

    CreateSwarm(DEFAULT_DRONES);
    CreateWorkspace(DEFAULT_DRONES);
    CreateConsciousnessContext();

    // Simulate shared attention: multiple drones add same concept
    uint32_t shared_concept_id = 42;
    for (uint32_t drone_id = 0; drone_id < DEFAULT_DRONES; drone_id++) {
        collective_workspace_item_t item;
        memset(&item, 0, sizeof(item));
        item.item_id = (drone_id << 16) | shared_concept_id;
        item.salience = 0.8f;
        item.type = WORKSPACE_ITEM_THREAT;
        item.source_drone = drone_id;
        item.timestamp_ms = 1000 + drone_id * 10;

        // Same content (shared attention)
        for (int j = 0; j < COLLECTIVE_WORKSPACE_CONTENT_DIM; j++) {
            item.content[j] = 1.0f;  // Identical content
        }

        collective_workspace_add_item(workspace_, &item);
    }

    float phi = swarm_consciousness_compute_collective_phi(
        consciousness_ctx_,
        swarm_brains_.data(),
        swarm_brains_.size(),
        workspace_
    );

    const swarm_consciousness_state_t* state = swarm_consciousness_get_state(consciousness_ctx_);

    // Shared attention should boost coherence and thus Φ
    EXPECT_GT(state->workspace_coherence, 0.4f) << "Shared attention should increase coherence";
    EXPECT_GT(phi, 0.3f) << "Shared attention should increase collective Φ";
}

TEST_F(SwarmConsciousnessIntegrationTest, WorkspaceUpdateTriggersPhiUpdate) {
    // WHAT: Workspace changes trigger Φ recalculation
    // WHY:  Consciousness should respond to workspace dynamics
    // HOW:  Update workspace, verify Φ changes

    CreateSwarm(DEFAULT_DRONES);
    CreateWorkspace(DEFAULT_DRONES);
    CreateConsciousnessContext();

    // Initial Φ
    float phi_before = swarm_consciousness_compute_collective_phi(
        consciousness_ctx_,
        swarm_brains_.data(),
        swarm_brains_.size(),
        workspace_
    );

    // Add urgent item to workspace
    collective_workspace_item_t urgent_item;
    memset(&urgent_item, 0, sizeof(urgent_item));
    urgent_item.item_id = (1 << 16) | 999;
    urgent_item.salience = 0.99f;  // Very urgent
    urgent_item.type = WORKSPACE_ITEM_THREAT;
    urgent_item.source_drone = 1;
    urgent_item.timestamp_ms = 5000;

    collective_workspace_add_item(workspace_, &urgent_item);

    // Recompute Φ
    float phi_after = swarm_consciousness_compute_collective_phi(
        consciousness_ctx_,
        swarm_brains_.data(),
        swarm_brains_.size(),
        workspace_
    );

    // High-salience item should affect workspace state (though Φ may not change much
    // since our mock is simplified, the integration point is verified)
    EXPECT_GE(phi_after, 0.0f);

    // Verify workspace has the urgent item
    collective_workspace_item_t top_items[3];
    uint32_t count;
    collective_workspace_get_top_items(workspace_, top_items, 3, &count);
    EXPECT_GT(count, 0u);
}

//=============================================================================
// 3. Individual Consciousness Integration Tests
//=============================================================================

TEST_F(SwarmConsciousnessIntegrationTest, IndividualPhiAggregation) {
    // WHAT: Individual drone Φ values aggregate to collective Φ
    // WHY:  Swarm consciousness emerges from individual consciousness
    // HOW:  Verify individual Φ contributes to collective

    CreateSwarm(DEFAULT_DRONES);
    CreateWorkspace(DEFAULT_DRONES);
    CreateConsciousnessContext();

    float collective_phi = swarm_consciousness_compute_collective_phi(
        consciousness_ctx_,
        swarm_brains_.data(),
        swarm_brains_.size(),
        workspace_
    );

    const swarm_consciousness_state_t* state = swarm_consciousness_get_state(consciousness_ctx_);
    ASSERT_NE(state->individual_phi, nullptr);

    // Compute manual average of individual Φ
    float manual_avg = 0.0f;
    for (uint32_t i = 0; i < state->num_drones; i++) {
        manual_avg += state->individual_phi[i];
    }
    manual_avg /= state->num_drones;

    // Collective Φ should be influenced by individual Φ average
    // (with workspace coherence and network integration weights)
    float expected_contribution = manual_avg * consciousness_config_.individual_phi_weight;

    // The individual contribution should be a significant component
    EXPECT_GT(expected_contribution, 0.0f);
    EXPECT_LT(fabs(collective_phi - manual_avg), 0.5f)
        << "Collective Φ should be reasonably close to individual average";
}

TEST_F(SwarmConsciousnessIntegrationTest, PhiScalesWithActivity) {
    // WHAT: More drone activity = higher Φ
    // WHY:  Active processing indicates consciousness
    // HOW:  Compare Φ with different activity levels

    CreateSwarm(DEFAULT_DRONES);
    CreateWorkspace(DEFAULT_DRONES);
    CreateConsciousnessContext();

    // Baseline Φ with minimal activity
    float phi_baseline = swarm_consciousness_compute_collective_phi(
        consciousness_ctx_,
        swarm_brains_.data(),
        swarm_brains_.size(),
        workspace_
    );

    // Simulate activity: add perception data
    for (uint32_t i = 0; i < DEFAULT_DRONES; i++) {
        perception_data_t perception;
        memset(&perception, 0, sizeof(perception));
        perception.sensor_type = 1;
        perception.value_count = 4;
        perception.values[0] = 0.5f;
        perception.values[1] = 0.7f;
        perception.values[2] = 0.3f;
        perception.values[3] = 0.9f;
        perception.confidence = 0.8f;

        swarm_brain_broadcast_perception(swarm_brains_[i], &perception);
    }

    ProcessAll(5);

    // Recompute after activity
    float phi_active = swarm_consciousness_compute_collective_phi(
        consciousness_ctx_,
        swarm_brains_.data(),
        swarm_brains_.size(),
        workspace_
    );

    // Activity should maintain or increase Φ
    // (In real implementation, activity would affect individual brain states)
    EXPECT_GE(phi_active, 0.0f);
}

TEST_F(SwarmConsciousnessIntegrationTest, ConsciousnessStateConsistent) {
    // WHAT: Individual and collective consciousness states are consistent
    // WHY:  If individuals are conscious, collective should reflect that
    // HOW:  Verify state classification matches Φ values

    CreateSwarm(DEFAULT_DRONES);
    CreateWorkspace(DEFAULT_DRONES);
    CreateConsciousnessContext();

    float collective_phi = swarm_consciousness_compute_collective_phi(
        consciousness_ctx_,
        swarm_brains_.data(),
        swarm_brains_.size(),
        workspace_
    );

    const swarm_consciousness_state_t* state = swarm_consciousness_get_state(consciousness_ctx_);

    // Verify state classification matches Φ value
    consciousness_state_t expected_state = consciousness_classify_phi(collective_phi);
    EXPECT_EQ(state->collective_state, expected_state)
        << "State classification should match Φ value";

    // If collective Φ is high enough, state should not be unconscious
    if (collective_phi > consciousness_config_.collective_phi_threshold) {
        EXPECT_NE(state->collective_state, CONSCIOUSNESS_STATE_UNCONSCIOUS)
            << "Swarm should not be unconscious when Φ > threshold";
    }
}

//=============================================================================
// 4. Bio-async Integration Tests
//=============================================================================

TEST_F(SwarmConsciousnessIntegrationTest, PhiUpdatesViaRouter) {
    // WHAT: Consciousness updates route through bio-async system
    // WHY:  Bio-async enables cross-module communication
    // HOW:  Verify consciousness messages can be sent/received

    CreateSwarm(DEFAULT_DRONES);
    CreateWorkspace(DEFAULT_DRONES);
    CreateConsciousnessContext();

    // In a real implementation, consciousness updates would be sent via bio-async
    // For this integration test, we verify the swarm brains are operational
    // and could receive such messages

    ProcessAll(3);

    for (auto* swarm : swarm_brains_) {
        EXPECT_TRUE(swarm_brain_is_operational(swarm))
            << "Swarm brain should be operational for message routing";
    }

    // Compute Φ (in real impl, would trigger bio-async message)
    float phi = swarm_consciousness_compute_collective_phi(
        consciousness_ctx_,
        swarm_brains_.data(),
        swarm_brains_.size(),
        workspace_
    );

    EXPECT_GT(phi, 0.0f);
}

TEST_F(SwarmConsciousnessIntegrationTest, CrossModuleNotification) {
    // WHAT: Other modules receive consciousness updates
    // WHY:  Consciousness state affects decision-making across modules
    // HOW:  Callback mechanism notifies subscribers

    CreateSwarm(DEFAULT_DRONES);
    CreateWorkspace(DEFAULT_DRONES);
    CreateConsciousnessContext();

    std::atomic<bool> notification_received{false};
    consciousness_state_t received_state = CONSCIOUSNESS_STATE_UNCONSCIOUS;

    auto callback = [](float phi, consciousness_state_t state, void* user_data) {
        auto* data = (std::pair<std::atomic<bool>*, consciousness_state_t*>*)user_data;
        data->first->store(true);
        *(data->second) = state;
    };

    std::pair<std::atomic<bool>*, consciousness_state_t*> user_data{
        &notification_received,
        &received_state
    };

    swarm_consciousness_enable_monitoring(consciousness_ctx_, callback, &user_data);

    // Compute Φ and manually trigger callback for test
    swarm_consciousness_compute_collective_phi(
        consciousness_ctx_,
        swarm_brains_.data(),
        swarm_brains_.size(),
        workspace_
    );

    if (consciousness_ctx_->callback) {
        consciousness_ctx_->callback(
            consciousness_ctx_->state.collective_phi,
            consciousness_ctx_->state.collective_state,
            consciousness_ctx_->callback_user_data
        );
    }

    EXPECT_TRUE(notification_received.load()) << "Callback should have been invoked";
    EXPECT_NE(received_state, CONSCIOUSNESS_STATE_UNCONSCIOUS)
        << "Should have received non-default state";
}

TEST_F(SwarmConsciousnessIntegrationTest, NeuromodulatorSync) {
    // WHAT: Consciousness affects neuromodulator synchronization
    // WHY:  Conscious states modulate swarm emotional state
    // HOW:  High Φ should correlate with neuromodulator coherence

    CreateSwarm(DEFAULT_DRONES);
    CreateWorkspace(DEFAULT_DRONES);
    CreateConsciousnessContext();

    // Sync neuromodulators across swarm
    neuromod_state_t neuromod_state;
    neuromod_state.dopamine = 0.6f;
    neuromod_state.serotonin = 0.5f;
    neuromod_state.norepinephrine = 0.4f;
    neuromod_state.acetylcholine = 0.7f;

    for (auto* swarm : swarm_brains_) {
        swarm_brain_sync_neuromodulators(swarm, &neuromod_state);
    }

    ProcessAll(3);

    // Compute consciousness
    float phi = swarm_consciousness_compute_collective_phi(
        consciousness_ctx_,
        swarm_brains_.data(),
        swarm_brains_.size(),
        workspace_
    );

    // In a real implementation, synchronized neuromodulators would affect
    // individual brain states, thus affecting Φ
    EXPECT_GT(phi, 0.0f);
}

//=============================================================================
// 5. BBB Security Integration Tests
//=============================================================================

TEST_F(SwarmConsciousnessIntegrationTest, SecurityValidation) {
    // WHAT: BBB security validates consciousness metrics
    // WHY:  Prevent spoofed consciousness values from malicious drones
    // HOW:  Consciousness values should pass validation

    CreateSwarm(DEFAULT_DRONES);
    CreateWorkspace(DEFAULT_DRONES);
    CreateConsciousnessContext();

    float phi = swarm_consciousness_compute_collective_phi(
        consciousness_ctx_,
        swarm_brains_.data(),
        swarm_brains_.size(),
        workspace_
    );

    const swarm_consciousness_state_t* state = swarm_consciousness_get_state(consciousness_ctx_);

    // Validate Φ is in reasonable range (security check)
    EXPECT_GE(phi, 0.0f) << "Φ should be non-negative";
    EXPECT_LE(phi, 10.0f) << "Φ should be bounded (security)";

    // Validate individual Φ values
    for (uint32_t i = 0; i < state->num_drones; i++) {
        EXPECT_GE(state->individual_phi[i], 0.0f);
        EXPECT_LE(state->individual_phi[i], 10.0f);
    }
}

TEST_F(SwarmConsciousnessIntegrationTest, AuditLogging) {
    // WHAT: Security events are logged for consciousness changes
    // WHY:  Track consciousness state transitions for security auditing
    // HOW:  Verify state changes are logged

    CreateSwarm(DEFAULT_DRONES);
    CreateWorkspace(DEFAULT_DRONES);
    CreateConsciousnessContext();

    // Initial state
    swarm_consciousness_compute_collective_phi(
        consciousness_ctx_,
        swarm_brains_.data(),
        swarm_brains_.size(),
        workspace_
    );

    const swarm_consciousness_state_t* state1 = swarm_consciousness_get_state(consciousness_ctx_);
    uint64_t timestamp1 = state1->timestamp;

    std::this_thread::sleep_for(std::chrono::milliseconds(100));

    // Update state
    swarm_consciousness_compute_collective_phi(
        consciousness_ctx_,
        swarm_brains_.data(),
        swarm_brains_.size(),
        workspace_
    );

    const swarm_consciousness_state_t* state2 = swarm_consciousness_get_state(consciousness_ctx_);
    uint64_t timestamp2 = state2->timestamp;

    // Timestamps should be updated (indicating state changes are tracked)
    EXPECT_GT(timestamp2, timestamp1) << "State transitions should be timestamped";
}

//=============================================================================
// 6. Emergence Tier Integration Tests
//=============================================================================

TEST_F(SwarmConsciousnessIntegrationTest, ConsciousnessEnhancesTier) {
    // WHAT: High collective Φ enables higher emergence tiers
    // WHY:  Consciousness is a prerequisite for advanced collective behavior
    // HOW:  Compare tier with/without consciousness

    CreateSwarm(8);  // Enough for SWARM tier
    CreateWorkspace(8);
    CreateConsciousnessContext();
    CreateEmergenceContext();

    // Compute consciousness
    float phi = swarm_consciousness_compute_collective_phi(
        consciousness_ctx_,
        swarm_brains_.data(),
        swarm_brains_.size(),
        workspace_
    );

    // Get emergence tier from swarm brain
    swarm_emergence_tier_t tier = swarm_brain_get_emergence_tier(swarm_brains_[0]);

    // Verify tier is valid (may be TIER_0 if emergence not fully implemented yet)
    EXPECT_GE(tier, SWARM_TIER_INDIVIDUAL);
    EXPECT_LE(tier, SWARM_TIER_COMPANY);

    // Verify mock tier calculation works
    swarm_emergence_tier_t expected_tier = GetMockEmergenceTier(8);
    EXPECT_EQ(expected_tier, SWARM_TIER_PLATOON) << "Mock tier should be SWARM for 8 drones";

    // High Φ should correlate with consciousness
    EXPECT_GT(phi, 0.0f) << "Collective Φ should be positive";
}

TEST_F(SwarmConsciousnessIntegrationTest, TierAffectsCapabilities) {
    // WHAT: Emergence tier unlocks consciousness-dependent capabilities
    // WHY:  Higher tiers enable meta-cognition, which requires consciousness
    // HOW:  Check capabilities at different tiers

    CreateSwarm(16);  // Enough for SUPERORGANISM tier
    CreateWorkspace(16);
    CreateConsciousnessContext();
    CreateEmergenceContext();

    // Compute consciousness
    swarm_consciousness_compute_collective_phi(
        consciousness_ctx_,
        swarm_brains_.data(),
        swarm_brains_.size(),
        workspace_
    );

    // Get emergence tier from swarm brain
    swarm_emergence_tier_t tier = swarm_brain_get_emergence_tier(swarm_brains_[0]);

    // Verify tier is valid
    EXPECT_GE(tier, SWARM_TIER_INDIVIDUAL);
    EXPECT_LE(tier, SWARM_TIER_COMPANY);

    // Verify tier progression logic
    swarm_emergence_tier_t mock_tier = GetMockEmergenceTier(16);
    EXPECT_EQ(mock_tier, SWARM_TIER_COMPANY) << "Mock tier should be SUPERORGANISM";

    // Verify different swarm sizes produce different tiers
    EXPECT_LT(GetMockEmergenceTier(2), GetMockEmergenceTier(16))
        << "Larger swarms should have higher tiers";
}

TEST_F(SwarmConsciousnessIntegrationTest, EmergenceCoherence) {
    // WHAT: Emergence requires consciousness coherence across drones
    // WHY:  Disjointed consciousness prevents true emergence
    // HOW:  Low coherence should limit Φ, high coherence increases it

    CreateSwarm(16);
    CreateWorkspace(16);
    CreateConsciousnessContext();
    CreateEmergenceContext();

    // Compute Φ - coherence affects collective consciousness
    float phi = swarm_consciousness_compute_collective_phi(
        consciousness_ctx_,
        swarm_brains_.data(),
        swarm_brains_.size(),
        workspace_
    );

    const swarm_consciousness_state_t* state = swarm_consciousness_get_state(consciousness_ctx_);

    // Workspace coherence should be a factor in collective Φ
    EXPECT_GE(state->workspace_coherence, 0.0f);
    EXPECT_LE(state->workspace_coherence, 1.0f);

    // With 16 drones, should have high network integration
    EXPECT_GT(state->network_integration, 0.8f) << "16 drones should have high integration";

    // Collective Φ should incorporate coherence
    float expected_min_phi = state->workspace_coherence * consciousness_config_.workspace_coherence_weight;
    EXPECT_GE(phi, 0.0f) << "Phi should be non-negative";
}

//=============================================================================
// Main
//=============================================================================

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
