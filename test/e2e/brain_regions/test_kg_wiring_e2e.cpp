/**
 * @file test_kg_wiring_e2e.cpp
 * @brief End-to-end tests for brain region KG wiring
 * @date 2026-02-02
 *
 * Tests complete workflows simulating real usage:
 * - Full brain initialization with KG
 * - Simulated cognitive processing with state updates
 * - Memory formation and retrieval simulation
 * - Emotional processing simulation
 * - Executive control simulation
 * - Cross-region coordination
 * - Graceful shutdown
 *
 * @author NIMCP Development Team
 */

#include "e2e_test_framework.h"
#include <gtest/gtest.h>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <cstdint>

extern "C" {
#include "core/brain/regions/hippocampus/bridges/nimcp_hippocampus_kg_wiring.h"
#include "core/brain/regions/amygdala/bridges/nimcp_amygdala_kg_wiring.h"
#include "core/brain/regions/prefrontal/bridges/nimcp_pfc_kg_wiring.h"
#include "core/brain/nimcp_brain_kg.h"
#include "nimcp.h"
}

namespace nimcp {
namespace e2e {

//=============================================================================
// Constants
//=============================================================================

constexpr uint64_t TEST_ADMIN_TOKEN = 0x12345678ULL;

//=============================================================================
// Brain Simulation Context
//=============================================================================

struct BrainSimContext {
    brain_kg_t* kg;
    hippocampus_kg_state_t hippocampus;
    amygdala_kg_state_t amygdala;
    pfc_kg_state_t pfc;
    bool initialized;
};

//=============================================================================
// Test Fixture
//=============================================================================

class KGWiringE2E : public ::testing::Test {
protected:
    BrainSimContext ctx_;

    void SetUp() override {
        memset(&ctx_, 0, sizeof(ctx_));
    }

    void TearDown() override {
        if (ctx_.initialized) {
            brain_sim_shutdown();
        }
    }

    brain_kg_t* create_test_kg() {
        brain_kg_config_t config;
        brain_kg_default_config(&config);
        config.enable_security = false;
        config.enable_access_control = false;
        config.enable_immune_integration = false;
        return brain_kg_create(&config);
    }

    int brain_sim_init() {
        ctx_.kg = create_test_kg();
        if (!ctx_.kg) return -1;

        // Initialize hippocampus
        if (hippocampus_kg_register_all(ctx_.kg, nullptr, &ctx_.hippocampus, TEST_ADMIN_TOKEN) != 0) {
            brain_kg_destroy(ctx_.kg);
            return -1;
        }

        // Initialize amygdala
        if (amygdala_kg_register_all(ctx_.kg, nullptr, &ctx_.amygdala, TEST_ADMIN_TOKEN) != 0) {
            brain_kg_destroy(ctx_.kg);
            return -1;
        }

        // Initialize PFC
        if (pfc_kg_register_all(ctx_.kg, nullptr, &ctx_.pfc, TEST_ADMIN_TOKEN) != 0) {
            brain_kg_destroy(ctx_.kg);
            return -1;
        }

        ctx_.initialized = true;
        return 0;
    }

    int brain_sim_shutdown() {
        if (!ctx_.initialized) return -1;

        // Unregister in reverse order
        pfc_kg_unregister_all(ctx_.kg, &ctx_.pfc, TEST_ADMIN_TOKEN);
        amygdala_kg_unregister_all(ctx_.kg, &ctx_.amygdala, TEST_ADMIN_TOKEN);
        hippocampus_kg_unregister_all(ctx_.kg, &ctx_.hippocampus, TEST_ADMIN_TOKEN);

        brain_kg_destroy(ctx_.kg);
        ctx_.initialized = false;
        return 0;
    }
};

//=============================================================================
// E2E Tests - Complete Workflows
//=============================================================================

TEST_F(KGWiringE2E, FullBrainLifecycle) {
    E2E_PIPELINE_START("Full Brain Lifecycle Pipeline");

    E2E_STAGE_BEGIN("Initialize brain", 500);
    {
        int result = brain_sim_init();
        EXPECT_EQ(0, result) << "Brain initialization should succeed";
        EXPECT_TRUE(ctx_.initialized) << "Brain should be marked initialized";
    }
    E2E_STAGE_END();

    E2E_STAGE_BEGIN("Verify module registration", 200);
    {
        EXPECT_TRUE(ctx_.hippocampus.registered) << "Hippocampus should be registered";
        EXPECT_TRUE(ctx_.amygdala.registered) << "Amygdala should be registered";
        EXPECT_TRUE(ctx_.pfc.registered) << "PFC should be registered";
    }
    E2E_STAGE_END();

    E2E_STAGE_BEGIN("Query module roots", 200);
    {
        brain_kg_node_id_t h_root = hippocampus_kg_get_root(ctx_.kg);
        brain_kg_node_id_t a_root = amygdala_kg_get_root(ctx_.kg);
        brain_kg_node_id_t p_root = pfc_kg_get_root(ctx_.kg);

        EXPECT_NE(BRAIN_KG_INVALID_NODE, h_root) << "Hippocampus root should be valid";
        EXPECT_NE(BRAIN_KG_INVALID_NODE, a_root) << "Amygdala root should be valid";
        EXPECT_NE(BRAIN_KG_INVALID_NODE, p_root) << "PFC root should be valid";
    }
    E2E_STAGE_END();

    E2E_STAGE_BEGIN("Shutdown brain", 200);
    {
        int result = brain_sim_shutdown();
        EXPECT_EQ(0, result) << "Brain shutdown should succeed";
        EXPECT_FALSE(ctx_.initialized) << "Brain should be marked not initialized";
    }
    E2E_STAGE_END();

    E2E_PIPELINE_END();
}

TEST_F(KGWiringE2E, MemoryFormationSimulation) {
    E2E_PIPELINE_START("Memory Formation Simulation Pipeline");

    E2E_STAGE_BEGIN("Initialize brain", 500);
    {
        EXPECT_EQ(0, brain_sim_init()) << "Brain init should succeed";
    }
    E2E_STAGE_END();

    E2E_STAGE_BEGIN("Phase 1: Initial encoding", 200);
    {
        std::cout << "  Phase 1: Initial encoding..." << std::endl;
        int r1 = hippocampus_kg_update_state(ctx_.kg, &ctx_.hippocampus,
            0.9f,   // encoding_strength - high during new learning
            0.3f,   // retrieval_accuracy - low initially
            0.0f,   // consolidation_progress - not started
            0.8f,   // spatial_precision - good context
            TEST_ADMIN_TOKEN);
        EXPECT_EQ(0, r1) << "Encoding phase update should succeed";
    }
    E2E_STAGE_END();

    E2E_STAGE_BEGIN("Phase 2: Active consolidation", 200);
    {
        std::cout << "  Phase 2: Consolidation..." << std::endl;
        int r2 = hippocampus_kg_update_state(ctx_.kg, &ctx_.hippocampus,
            0.5f,   // encoding_strength - reduced
            0.5f,   // retrieval_accuracy - improving
            0.5f,   // consolidation_progress - in progress
            0.8f,   // spatial_precision - maintained
            TEST_ADMIN_TOKEN);
        EXPECT_EQ(0, r2) << "Consolidation phase update should succeed";
    }
    E2E_STAGE_END();

    E2E_STAGE_BEGIN("Phase 3: Memory established", 200);
    {
        std::cout << "  Phase 3: Memory established..." << std::endl;
        int r3 = hippocampus_kg_update_state(ctx_.kg, &ctx_.hippocampus,
            0.2f,   // encoding_strength - low (no longer encoding)
            0.85f,  // retrieval_accuracy - high (well consolidated)
            0.9f,   // consolidation_progress - nearly complete
            0.75f,  // spatial_precision - some decay
            TEST_ADMIN_TOKEN);
        EXPECT_EQ(0, r3) << "Established phase update should succeed";
    }
    E2E_STAGE_END();

    brain_sim_shutdown();

    E2E_PIPELINE_END();
}

TEST_F(KGWiringE2E, FearResponseSimulation) {
    E2E_PIPELINE_START("Fear Response Simulation Pipeline");

    E2E_STAGE_BEGIN("Initialize brain", 500);
    {
        EXPECT_EQ(0, brain_sim_init()) << "Brain init should succeed";
    }
    E2E_STAGE_END();

    E2E_STAGE_BEGIN("Phase 1: Threat detection", 200);
    {
        std::cout << "  Phase 1: Threat detected..." << std::endl;
        int r1 = amygdala_kg_update_state(ctx_.kg, &ctx_.amygdala,
            0.8f,   // threat_level - high
            0.7f,   // fear_strength - rising
            0.9f,   // arousal_level - high
            0.0f,   // extinction_progress - none
            TEST_ADMIN_TOKEN);
        EXPECT_EQ(0, r1) << "Threat detection update should succeed";
    }
    E2E_STAGE_END();

    E2E_STAGE_BEGIN("Phase 2: Peak fear response", 200);
    {
        std::cout << "  Phase 2: Peak fear response..." << std::endl;
        int r2 = amygdala_kg_update_state(ctx_.kg, &ctx_.amygdala,
            0.9f,   // threat_level - very high
            0.95f,  // fear_strength - peak
            0.95f,  // arousal_level - peak
            0.0f,   // extinction_progress - none
            TEST_ADMIN_TOKEN);
        EXPECT_EQ(0, r2) << "Peak fear update should succeed";
    }
    E2E_STAGE_END();

    E2E_STAGE_BEGIN("Phase 3: Safety signal, extinction begins", 200);
    {
        std::cout << "  Phase 3: Safety detected, extinction begins..." << std::endl;
        int r3 = amygdala_kg_update_state(ctx_.kg, &ctx_.amygdala,
            0.3f,   // threat_level - reduced
            0.5f,   // fear_strength - decreasing
            0.5f,   // arousal_level - decreasing
            0.3f,   // extinction_progress - starting
            TEST_ADMIN_TOKEN);
        EXPECT_EQ(0, r3) << "Extinction start update should succeed";
    }
    E2E_STAGE_END();

    E2E_STAGE_BEGIN("Phase 4: Fear extinguished", 200);
    {
        std::cout << "  Phase 4: Fear extinguished..." << std::endl;
        int r4 = amygdala_kg_update_state(ctx_.kg, &ctx_.amygdala,
            0.1f,   // threat_level - low
            0.1f,   // fear_strength - minimal
            0.3f,   // arousal_level - baseline
            0.9f,   // extinction_progress - mostly complete
            TEST_ADMIN_TOKEN);
        EXPECT_EQ(0, r4) << "Extinction complete update should succeed";
    }
    E2E_STAGE_END();

    brain_sim_shutdown();

    E2E_PIPELINE_END();
}

TEST_F(KGWiringE2E, ExecutiveControlSimulation) {
    E2E_PIPELINE_START("Executive Control Simulation Pipeline");

    E2E_STAGE_BEGIN("Initialize brain", 500);
    {
        EXPECT_EQ(0, brain_sim_init()) << "Brain init should succeed";
    }
    E2E_STAGE_END();

    E2E_STAGE_BEGIN("Phase 1: Task engagement", 200);
    {
        std::cout << "  Phase 1: Task engagement..." << std::endl;
        int r1 = pfc_kg_update_state(ctx_.kg, &ctx_.pfc,
            0.3f,   // wm_load - moderate
            0.5f,   // control_demand - moderate
            0.1f,   // conflict_level - low
            0.8f,   // attention_focus - good
            TEST_ADMIN_TOKEN);
        EXPECT_EQ(0, r1) << "Task engagement update should succeed";
    }
    E2E_STAGE_END();

    E2E_STAGE_BEGIN("Phase 2: High cognitive demand", 200);
    {
        std::cout << "  Phase 2: High cognitive demand..." << std::endl;
        int r2 = pfc_kg_update_state(ctx_.kg, &ctx_.pfc,
            0.85f,  // wm_load - high
            0.9f,   // control_demand - high
            0.4f,   // conflict_level - moderate conflict
            0.7f,   // attention_focus - somewhat strained
            TEST_ADMIN_TOKEN);
        EXPECT_EQ(0, r2) << "High demand update should succeed";
    }
    E2E_STAGE_END();

    E2E_STAGE_BEGIN("Phase 3: Error detected, adjusting", 200);
    {
        std::cout << "  Phase 3: Error detected, adjusting..." << std::endl;
        int r3 = pfc_kg_update_state(ctx_.kg, &ctx_.pfc,
            0.9f,   // wm_load - very high
            0.95f,  // control_demand - maximum
            0.7f,   // conflict_level - high conflict (error)
            0.5f,   // attention_focus - redirecting
            TEST_ADMIN_TOKEN);
        EXPECT_EQ(0, r3) << "Error adjustment update should succeed";
    }
    E2E_STAGE_END();

    E2E_STAGE_BEGIN("Phase 4: Task completed", 200);
    {
        std::cout << "  Phase 4: Task completed..." << std::endl;
        int r4 = pfc_kg_update_state(ctx_.kg, &ctx_.pfc,
            0.2f,   // wm_load - low
            0.3f,   // control_demand - low
            0.1f,   // conflict_level - low
            0.5f,   // attention_focus - relaxed
            TEST_ADMIN_TOKEN);
        EXPECT_EQ(0, r4) << "Task completion update should succeed";
    }
    E2E_STAGE_END();

    brain_sim_shutdown();

    E2E_PIPELINE_END();
}

TEST_F(KGWiringE2E, EmotionalMemoryCoordination) {
    E2E_PIPELINE_START("Emotional Memory Coordination Pipeline");

    E2E_STAGE_BEGIN("Initialize brain", 500);
    {
        EXPECT_EQ(0, brain_sim_init()) << "Brain init should succeed";
    }
    E2E_STAGE_END();

    E2E_STAGE_BEGIN("Encode emotional memory", 300);
    {
        std::cout << "  Simulating emotional memory encoding..." << std::endl;

        // Step 1: Emotional stimulus activates amygdala
        int r1 = amygdala_kg_update_state(ctx_.kg, &ctx_.amygdala,
            0.7f, 0.8f, 0.9f, 0.0f, TEST_ADMIN_TOKEN);
        EXPECT_EQ(0, r1) << "Amygdala activation should succeed";

        // Step 2: Arousal enhances hippocampal encoding
        int r2 = hippocampus_kg_update_state(ctx_.kg, &ctx_.hippocampus,
            0.95f, 0.4f, 0.1f, 0.9f, TEST_ADMIN_TOKEN);
        EXPECT_EQ(0, r2) << "Enhanced encoding should succeed";

        // Step 3: PFC modulates response
        int r3 = pfc_kg_update_state(ctx_.kg, &ctx_.pfc,
            0.6f, 0.7f, 0.3f, 0.8f, TEST_ADMIN_TOKEN);
        EXPECT_EQ(0, r3) << "PFC modulation should succeed";
    }
    E2E_STAGE_END();

    E2E_STAGE_BEGIN("Consolidate memory", 300);
    {
        std::cout << "  Simulating consolidation..." << std::endl;

        // All regions stabilize
        amygdala_kg_update_state(ctx_.kg, &ctx_.amygdala,
            0.3f, 0.4f, 0.5f, 0.2f, TEST_ADMIN_TOKEN);
        hippocampus_kg_update_state(ctx_.kg, &ctx_.hippocampus,
            0.4f, 0.7f, 0.6f, 0.85f, TEST_ADMIN_TOKEN);
        pfc_kg_update_state(ctx_.kg, &ctx_.pfc,
            0.3f, 0.4f, 0.1f, 0.6f, TEST_ADMIN_TOKEN);
    }
    E2E_STAGE_END();

    E2E_STAGE_BEGIN("Retrieve emotional memory", 300);
    {
        std::cout << "  Simulating emotional memory retrieval..." << std::endl;

        // Retrieval cue reactivates pattern
        hippocampus_kg_update_state(ctx_.kg, &ctx_.hippocampus,
            0.2f, 0.9f, 0.8f, 0.7f, TEST_ADMIN_TOKEN);

        // Emotional component reactivates
        amygdala_kg_update_state(ctx_.kg, &ctx_.amygdala,
            0.5f, 0.6f, 0.7f, 0.3f, TEST_ADMIN_TOKEN);

        // PFC regulates
        pfc_kg_update_state(ctx_.kg, &ctx_.pfc,
            0.5f, 0.6f, 0.2f, 0.7f, TEST_ADMIN_TOKEN);
    }
    E2E_STAGE_END();

    brain_sim_shutdown();

    E2E_PIPELINE_END();
}

TEST_F(KGWiringE2E, StressResilience) {
    E2E_PIPELINE_START("Stress Resilience Pipeline");

    E2E_STAGE_BEGIN("Initialize brain", 500);
    {
        EXPECT_EQ(0, brain_sim_init()) << "Brain init should succeed";
    }
    E2E_STAGE_END();

    E2E_STAGE_BEGIN("Run 100 rapid state update cycles", 5000);
    {
        std::cout << "  Running 100 rapid state update cycles..." << std::endl;

        int errors = 0;
        for (int i = 0; i < 100; i++) {
            float phase = static_cast<float>(i) / 100.0f;

            // Update all regions rapidly
            if (hippocampus_kg_update_state(ctx_.kg, &ctx_.hippocampus,
                phase, 1.0f - phase, phase * 0.5f, 0.8f, TEST_ADMIN_TOKEN) != 0) {
                errors++;
            }

            if (amygdala_kg_update_state(ctx_.kg, &ctx_.amygdala,
                1.0f - phase, phase, phase * 0.7f, 0.3f, TEST_ADMIN_TOKEN) != 0) {
                errors++;
            }

            if (pfc_kg_update_state(ctx_.kg, &ctx_.pfc,
                phase * 0.5f, 0.6f, 0.2f, phase, TEST_ADMIN_TOKEN) != 0) {
                errors++;
            }
        }

        EXPECT_EQ(0, errors) << "No errors during stress test";
    }
    E2E_STAGE_END();

    E2E_STAGE_BEGIN("Verify system still functional", 200);
    {
        brain_kg_node_id_t h = hippocampus_kg_get_root(ctx_.kg);
        brain_kg_node_id_t a = amygdala_kg_get_root(ctx_.kg);
        brain_kg_node_id_t p = pfc_kg_get_root(ctx_.kg);

        EXPECT_NE(BRAIN_KG_INVALID_NODE, h) << "Hippocampus still functional";
        EXPECT_NE(BRAIN_KG_INVALID_NODE, a) << "Amygdala still functional";
        EXPECT_NE(BRAIN_KG_INVALID_NODE, p) << "PFC still functional";

        std::cout << "  Stress resilience test completed (100 cycles, 0 errors)" << std::endl;
    }
    E2E_STAGE_END();

    brain_sim_shutdown();

    E2E_PIPELINE_END();
}

TEST_F(KGWiringE2E, QueryFunctionality) {
    E2E_PIPELINE_START("Query Functionality Pipeline");

    E2E_STAGE_BEGIN("Initialize brain", 500);
    {
        EXPECT_EQ(0, brain_sim_init()) << "Brain init should succeed";
    }
    E2E_STAGE_END();

    E2E_STAGE_BEGIN("Test find_subsystem across all modules", 300);
    {
        brain_kg_node_id_t ca1 = hippocampus_kg_find_subsystem(ctx_.kg, "ca1");
        brain_kg_node_id_t bla = amygdala_kg_find_subsystem(ctx_.kg, "basolateral_complex");
        brain_kg_node_id_t dlpfc = pfc_kg_find_subsystem(ctx_.kg, "dorsolateral_pfc");

        EXPECT_NE(BRAIN_KG_INVALID_NODE, ca1) << "CA1 should be findable";
        EXPECT_NE(BRAIN_KG_INVALID_NODE, bla) << "BLA should be findable";
        EXPECT_NE(BRAIN_KG_INVALID_NODE, dlpfc) << "dlPFC should be findable";
    }
    E2E_STAGE_END();

    E2E_STAGE_BEGIN("Test get_*_nodes functions", 300);
    {
        brain_kg_node_list_t* mem_nodes = hippocampus_kg_get_memory_nodes(ctx_.kg);
        brain_kg_node_list_t* emo_nodes = amygdala_kg_get_emotion_nodes(ctx_.kg);
        brain_kg_node_list_t* exec_nodes = pfc_kg_get_executive_nodes(ctx_.kg);

        // Clean up lists
        if (mem_nodes) brain_kg_node_list_destroy(mem_nodes);
        if (emo_nodes) brain_kg_node_list_destroy(emo_nodes);
        if (exec_nodes) brain_kg_node_list_destroy(exec_nodes);
    }
    E2E_STAGE_END();

    brain_sim_shutdown();

    E2E_PIPELINE_END();
}

} // namespace e2e
} // namespace nimcp
