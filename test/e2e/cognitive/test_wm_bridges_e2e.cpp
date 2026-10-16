/**
 * @file test_wm_bridges_e2e.cpp
 * @brief End-to-end tests for World Model Bridge Integration
 * @version 1.0.0
 * @date 2026-01-24
 *
 * Comprehensive E2E tests for all 11 World Model bridges. Tests complete
 * cognitive workflows including:
 *
 * - Multi-bridge initialization and coordination
 * - Full cognitive loop with all bridges active
 * - Bridge update coordination
 * - Statistics collection across bridges
 * - Reset and recovery scenarios
 *
 * BRIDGES TESTED:
 * 1. Security-Immune Bridge (0x0E63) - Code/Brain immune integration
 * 2. Logging Bridge (0x0E64) - Centralized logging and diagnostics
 * 3. Cognitive Bridge (0x0E65) - Executive, Attention, Working Memory
 * 4. Parietal Bridge (0x0E66) - Spatial reasoning, numerosity
 * 5. Hypothalamus Bridge (0x0E67) - Homeostatic regulation
 * 6. Thalamic Bridge (0x0E68) - Attention gating, sensory relay
 * 7. Substrate Bridge (0x0E69) - Metabolic constraints
 * 8. Memory Bridge (0x0E6A) - Hippocampus, Engram, Consolidation
 * 9. KG Wiring Bridge (0x0E6B) - Knowledge graph integration
 * 10. Theory of Mind Bridge (0x0E6C) - Social cognition
 * 11. Plasticity Bridge (0x0E6D) - STDP, BCM, eligibility traces
 */

#include <gtest/gtest.h>
#include <cstring>
#include <cmath>
#include <vector>
#include <random>
#include <chrono>

/* World Model */
#include "cognitive/omni/nimcp_omni_world_model.h"

/* All WM Bridges */
#include "cognitive/omni/bridges/nimcp_omni_wm_cognitive_bridge.h"
#include "cognitive/omni/bridges/nimcp_omni_wm_memory_bridge.h"
#include "cognitive/omni/bridges/nimcp_omni_wm_tom_bridge.h"
#include "cognitive/omni/bridges/nimcp_omni_wm_plasticity_bridge.h"
#include "cognitive/omni/bridges/nimcp_omni_wm_kg_bridge.h"
#include "cognitive/omni/bridges/nimcp_omni_wm_logging_bridge.h"
#include "cognitive/omni/bridges/nimcp_omni_wm_substrate_bridge.h"
#include "cognitive/omni/bridges/nimcp_omni_wm_thalamic_bridge.h"
#include "cognitive/omni/bridges/nimcp_omni_wm_hypothalamus_bridge.h"
#include "cognitive/omni/bridges/nimcp_omni_wm_parietal_bridge.h"
#include "cognitive/omni/bridges/nimcp_omni_wm_security_immune_bridge.h"

/* Memory utilities */
#include "utils/memory/nimcp_memory.h"

/* ============================================================================
 * Test Configuration Constants
 * ============================================================================ */

namespace {
    /* World model dimensions */
    constexpr uint32_t STATE_DIM = 64;
    constexpr uint32_t ACTION_DIM = 8;
    constexpr uint32_t OBS_DIM = 64;
    constexpr uint32_t LATENT_DIM = 32;

    /* Simulation parameters */
    constexpr float DT = 0.016f;  /* 60 FPS timestep */
    constexpr uint32_t SIMULATION_STEPS = 100;
    constexpr uint32_t WARMUP_STEPS = 10;
}

/* ============================================================================
 * E2E Test Fixture
 * ============================================================================ */

class WMBridgesE2ETest : public ::testing::Test {
protected:
    /* World Model */
    omni_world_model_t* wm = nullptr;

    /* All 11 bridges */
    omni_wm_cognitive_bridge_t* cognitive_bridge = nullptr;
    omni_wm_memory_bridge_t* memory_bridge = nullptr;
    omni_wm_tom_bridge_t* tom_bridge = nullptr;
    omni_wm_plasticity_bridge_t* plasticity_bridge = nullptr;
    omni_wm_kg_bridge_t* kg_bridge = nullptr;
    omni_wm_logging_bridge_t* logging_bridge = nullptr;
    omni_wm_substrate_bridge_t* substrate_bridge = nullptr;
    omni_wm_thalamic_bridge_t* thalamic_bridge = nullptr;
    omni_wm_hypothalamus_bridge_t* hypothalamus_bridge = nullptr;
    omni_wm_parietal_bridge_t* parietal_bridge = nullptr;
    omni_wm_security_immune_bridge_t* security_bridge = nullptr;

    /* Random number generation */
    std::mt19937 rng{42};
    std::uniform_real_distribution<float> uniform_dist{-1.0f, 1.0f};

    void SetUp() override {
        /* Create world model with RSSM */
        omni_wm_config_t wm_config;
        omni_wm_get_default_config(&wm_config);
        wm_config.state_dim = STATE_DIM;
        wm_config.action_dim = ACTION_DIM;
        wm_config.obs_dim = OBS_DIM;
        wm_config.latent_dim = LATENT_DIM;
        wm_config.rssm_h_dim = STATE_DIM / 2;
        wm_config.rssm_z_dim = LATENT_DIM / 2;
        wm_config.replay_buffer_size = 500;
        wm_config.batch_size = 8;
        wm_config.enable_dreaming = true;
        wm_config.use_rssm = true;
        wm_config.use_symlog_rewards = true;

        wm = omni_wm_create(&wm_config);
        ASSERT_NE(wm, nullptr) << "Failed to create world model";
    }

    void TearDown() override {
        /* Destroy bridges in reverse order */
        DestroyAllBridges();

        /* Destroy world model */
        if (wm) {
            omni_wm_destroy(wm);
            wm = nullptr;
        }
    }

    /* Helper: Create all bridges */
    void CreateAllBridges() {
        /* 1. Logging bridge (first - other bridges may use it) */
        omni_wm_logging_bridge_config_t log_config;
        ASSERT_EQ(omni_wm_logging_bridge_default_config(&log_config), NIMCP_SUCCESS);
        log_config.enable_bio_async = true;
        logging_bridge = omni_wm_logging_bridge_create(&log_config);
        ASSERT_NE(logging_bridge, nullptr);
        ASSERT_EQ(omni_wm_logging_bridge_connect_world_model(logging_bridge, wm),
                  NIMCP_SUCCESS);

        /* 2. Security-Immune bridge */
        omni_wm_security_immune_config_t sec_config;
        ASSERT_EQ(omni_wm_security_immune_bridge_default_config(&sec_config), NIMCP_SUCCESS);
        sec_config.enable_bio_async = true;
        security_bridge = omni_wm_security_immune_bridge_create(&sec_config);
        ASSERT_NE(security_bridge, nullptr);
        ASSERT_EQ(omni_wm_security_immune_bridge_connect_world_model(security_bridge, wm),
                  NIMCP_SUCCESS);

        /* 3. Cognitive bridge - returns config directly */
        omni_wm_cognitive_bridge_config_t cog_config = omni_wm_cognitive_bridge_default_config();
        cog_config.enable_bio_async = true;
        cog_config.enable_goal_conditioning = true;
        cog_config.enable_attention_modulation = true;
        cognitive_bridge = omni_wm_cognitive_bridge_create(&cog_config);
        ASSERT_NE(cognitive_bridge, nullptr);
        ASSERT_EQ(omni_wm_cognitive_bridge_connect_world_model(cognitive_bridge, wm),
                  NIMCP_SUCCESS);

        /* 4. Parietal bridge */
        omni_wm_parietal_bridge_config_t par_config;
        ASSERT_EQ(omni_wm_parietal_bridge_default_config(&par_config), NIMCP_SUCCESS);
        par_config.enable_bio_async = true;
        parietal_bridge = omni_wm_parietal_bridge_create(&par_config);
        ASSERT_NE(parietal_bridge, nullptr);
        ASSERT_EQ(omni_wm_parietal_bridge_connect_world_model(parietal_bridge, wm),
                  NIMCP_SUCCESS);

        /* 5. Hypothalamus bridge */
        omni_wm_hypothalamus_bridge_config_t hypo_config;
        ASSERT_EQ(omni_wm_hypothalamus_bridge_default_config(&hypo_config), NIMCP_SUCCESS);
        hypo_config.enable_bio_async = true;
        hypothalamus_bridge = omni_wm_hypothalamus_bridge_create(&hypo_config);
        ASSERT_NE(hypothalamus_bridge, nullptr);
        ASSERT_EQ(omni_wm_hypothalamus_bridge_connect_world_model(hypothalamus_bridge, wm),
                  NIMCP_SUCCESS);

        /* 6. Thalamic bridge */
        omni_wm_thalamic_bridge_config_t thal_config;
        ASSERT_EQ(omni_wm_thalamic_bridge_default_config(&thal_config), NIMCP_SUCCESS);
        thal_config.enable_bio_async = true;
        thalamic_bridge = omni_wm_thalamic_bridge_create(&thal_config);
        ASSERT_NE(thalamic_bridge, nullptr);
        ASSERT_EQ(omni_wm_thalamic_bridge_connect_world_model(thalamic_bridge, wm),
                  NIMCP_SUCCESS);

        /* 7. Substrate bridge */
        omni_wm_substrate_bridge_config_t sub_config;
        ASSERT_EQ(omni_wm_substrate_bridge_default_config(&sub_config), NIMCP_SUCCESS);
        sub_config.enable_bio_async = true;
        substrate_bridge = omni_wm_substrate_bridge_create(&sub_config);
        ASSERT_NE(substrate_bridge, nullptr);
        ASSERT_EQ(omni_wm_substrate_bridge_connect_world_model(substrate_bridge, wm),
                  NIMCP_SUCCESS);

        /* 8. Memory bridge */
        omni_wm_memory_bridge_config_t mem_config;
        ASSERT_EQ(omni_wm_memory_bridge_default_config(&mem_config), NIMCP_SUCCESS);
        mem_config.enable_bio_async = true;
        memory_bridge = omni_wm_memory_bridge_create(&mem_config);
        ASSERT_NE(memory_bridge, nullptr);
        ASSERT_EQ(omni_wm_memory_bridge_connect_world_model(memory_bridge, wm),
                  NIMCP_SUCCESS);

        /* 9. KG Wiring bridge */
        omni_wm_kg_bridge_config_t kg_config;
        ASSERT_EQ(omni_wm_kg_bridge_default_config(&kg_config), NIMCP_SUCCESS);
        kg_config.enable_bio_async = true;
        kg_bridge = omni_wm_kg_bridge_create(&kg_config);
        ASSERT_NE(kg_bridge, nullptr);
        ASSERT_EQ(omni_wm_kg_bridge_connect_world_model(kg_bridge, wm),
                  NIMCP_SUCCESS);

        /* 10. Theory of Mind bridge */
        omni_wm_tom_bridge_config_t tom_config;
        ASSERT_EQ(omni_wm_tom_bridge_default_config(&tom_config), NIMCP_SUCCESS);
        tom_config.enable_bio_async = true;
        tom_config.max_tracked_agents = 5;
        tom_bridge = omni_wm_tom_bridge_create(&tom_config);
        ASSERT_NE(tom_bridge, nullptr);
        ASSERT_EQ(omni_wm_tom_bridge_connect_world_model(tom_bridge, wm),
                  NIMCP_SUCCESS);

        /* 11. Plasticity bridge */
        omni_wm_plasticity_bridge_config_t plas_config;
        ASSERT_EQ(omni_wm_plasticity_bridge_default_config(&plas_config), NIMCP_SUCCESS);
        plas_config.enable_bio_async = true;
        plasticity_bridge = omni_wm_plasticity_bridge_create(&plas_config);
        ASSERT_NE(plasticity_bridge, nullptr);
        ASSERT_EQ(omni_wm_plasticity_bridge_connect_world_model(plasticity_bridge, wm),
                  NIMCP_SUCCESS);
    }

    /* Helper: Destroy all bridges */
    void DestroyAllBridges() {
        /* Destroy in reverse order of creation */
        if (plasticity_bridge) {
            omni_wm_plasticity_bridge_destroy(plasticity_bridge);
            plasticity_bridge = nullptr;
        }
        if (tom_bridge) {
            omni_wm_tom_bridge_destroy(tom_bridge);
            tom_bridge = nullptr;
        }
        if (kg_bridge) {
            omni_wm_kg_bridge_destroy(kg_bridge);
            kg_bridge = nullptr;
        }
        if (memory_bridge) {
            omni_wm_memory_bridge_destroy(memory_bridge);
            memory_bridge = nullptr;
        }
        if (substrate_bridge) {
            omni_wm_substrate_bridge_destroy(substrate_bridge);
            substrate_bridge = nullptr;
        }
        if (thalamic_bridge) {
            omni_wm_thalamic_bridge_destroy(thalamic_bridge);
            thalamic_bridge = nullptr;
        }
        if (hypothalamus_bridge) {
            omni_wm_hypothalamus_bridge_destroy(hypothalamus_bridge);
            hypothalamus_bridge = nullptr;
        }
        if (parietal_bridge) {
            omni_wm_parietal_bridge_destroy(parietal_bridge);
            parietal_bridge = nullptr;
        }
        if (cognitive_bridge) {
            omni_wm_cognitive_bridge_destroy(cognitive_bridge);
            cognitive_bridge = nullptr;
        }
        if (security_bridge) {
            omni_wm_security_immune_bridge_destroy(security_bridge);
            security_bridge = nullptr;
        }
        if (logging_bridge) {
            omni_wm_logging_bridge_destroy(logging_bridge);
            logging_bridge = nullptr;
        }
    }

    /* Helper: Run a single update cycle on all bridges */
    void RunAllBridgesUpdate(float dt) {
        ASSERT_EQ(omni_wm_logging_bridge_update(logging_bridge, dt), NIMCP_SUCCESS);
        ASSERT_EQ(omni_wm_security_immune_bridge_update(security_bridge, dt), NIMCP_SUCCESS);
        ASSERT_EQ(omni_wm_cognitive_bridge_update(cognitive_bridge, dt), NIMCP_SUCCESS);
        ASSERT_EQ(omni_wm_parietal_bridge_update(parietal_bridge, dt), NIMCP_SUCCESS);
        ASSERT_EQ(omni_wm_hypothalamus_bridge_update(hypothalamus_bridge, dt), NIMCP_SUCCESS);
        ASSERT_EQ(omni_wm_thalamic_bridge_update(thalamic_bridge, dt), NIMCP_SUCCESS);
        ASSERT_EQ(omni_wm_substrate_bridge_update(substrate_bridge, dt), NIMCP_SUCCESS);
        ASSERT_EQ(omni_wm_memory_bridge_update(memory_bridge, dt), NIMCP_SUCCESS);
        ASSERT_EQ(omni_wm_kg_bridge_update(kg_bridge, dt), NIMCP_SUCCESS);
        ASSERT_EQ(omni_wm_tom_bridge_update(tom_bridge, dt), NIMCP_SUCCESS);
        ASSERT_EQ(omni_wm_plasticity_bridge_update(plasticity_bridge, dt), NIMCP_SUCCESS);
    }
};

/* ============================================================================
 * E2E Test: Full System Initialization
 * ============================================================================ */

TEST_F(WMBridgesE2ETest, FullSystemInitialization) {
    /* Create all 11 bridges */
    CreateAllBridges();

    /* Verify all bridges are connected and valid */
    EXPECT_NE(logging_bridge, nullptr);
    EXPECT_NE(security_bridge, nullptr);
    EXPECT_NE(cognitive_bridge, nullptr);
    EXPECT_NE(parietal_bridge, nullptr);
    EXPECT_NE(hypothalamus_bridge, nullptr);
    EXPECT_NE(thalamic_bridge, nullptr);
    EXPECT_NE(substrate_bridge, nullptr);
    EXPECT_NE(memory_bridge, nullptr);
    EXPECT_NE(kg_bridge, nullptr);
    EXPECT_NE(tom_bridge, nullptr);
    EXPECT_NE(plasticity_bridge, nullptr);

    /* Verify bio-async module IDs are correct */
    EXPECT_EQ(logging_bridge->base.module_id, BIO_MODULE_WM_LOGGING_BRIDGE);
    EXPECT_EQ(security_bridge->base.module_id, BIO_MODULE_WM_SECURITY_IMMUNE_BRIDGE);
    EXPECT_EQ(cognitive_bridge->base.module_id, BIO_MODULE_WM_COGNITIVE_BRIDGE);
    EXPECT_EQ(parietal_bridge->base.module_id, BIO_MODULE_WM_PARIETAL_BRIDGE);
    EXPECT_EQ(hypothalamus_bridge->base.module_id, BIO_MODULE_WM_HYPOTHALAMUS_BRIDGE);
    EXPECT_EQ(thalamic_bridge->base.module_id, BIO_MODULE_WM_THALAMIC_BRIDGE);
    EXPECT_EQ(substrate_bridge->base.module_id, BIO_MODULE_WM_SUBSTRATE_BRIDGE);
    EXPECT_EQ(memory_bridge->base.module_id, BIO_MODULE_WM_MEMORY_BRIDGE);
    EXPECT_EQ(kg_bridge->base.module_id, BIO_MODULE_WM_KG_BRIDGE);
    EXPECT_EQ(tom_bridge->base.module_id, BIO_MODULE_WM_TOM_BRIDGE);
    EXPECT_EQ(plasticity_bridge->base.module_id, BIO_MODULE_WM_PLASTICITY_BRIDGE);

    /* Verify all bridges share the same world model */
    EXPECT_EQ(logging_bridge->world_model, wm);
    EXPECT_EQ(security_bridge->world_model, wm);
    EXPECT_EQ(cognitive_bridge->world_model, wm);
    EXPECT_EQ(memory_bridge->world_model, wm);
    EXPECT_EQ(tom_bridge->world_model, wm);
    EXPECT_EQ(plasticity_bridge->world_model, wm);
    EXPECT_EQ(parietal_bridge->world_model, wm);
    EXPECT_EQ(hypothalamus_bridge->world_model, wm);
    EXPECT_EQ(thalamic_bridge->world_model, wm);
    EXPECT_EQ(substrate_bridge->world_model, wm);
    EXPECT_EQ(kg_bridge->world_model, wm);
}

/* ============================================================================
 * E2E Test: Multi-Bridge Update Cycle
 * ============================================================================ */

TEST_F(WMBridgesE2ETest, MultiBridgeUpdateCycle) {
    CreateAllBridges();

    /* Run multiple update cycles with all bridges */
    for (uint32_t step = 0; step < SIMULATION_STEPS; step++) {
        RunAllBridgesUpdate(DT);
    }

    /* Verify all bridges accumulated statistics */
    omni_wm_cognitive_bridge_stats_t cog_stats;
    EXPECT_EQ(omni_wm_cognitive_bridge_get_stats(cognitive_bridge, &cog_stats), NIMCP_SUCCESS);
    EXPECT_EQ(cog_stats.total_updates, SIMULATION_STEPS);

    omni_wm_memory_bridge_stats_t mem_stats;
    EXPECT_EQ(omni_wm_memory_bridge_get_stats(memory_bridge, &mem_stats), NIMCP_SUCCESS);
    EXPECT_EQ(mem_stats.total_updates, SIMULATION_STEPS);

    omni_wm_tom_bridge_stats_t tom_stats;
    EXPECT_EQ(omni_wm_tom_bridge_get_stats(tom_bridge, &tom_stats), NIMCP_SUCCESS);
    EXPECT_EQ(tom_stats.total_updates, SIMULATION_STEPS);

    omni_wm_plasticity_bridge_stats_t plas_stats;
    EXPECT_EQ(omni_wm_plasticity_bridge_get_stats(plasticity_bridge, &plas_stats), NIMCP_SUCCESS);
    EXPECT_EQ(plas_stats.total_updates, SIMULATION_STEPS);

    omni_wm_kg_bridge_stats_t kg_stats;
    EXPECT_EQ(omni_wm_kg_bridge_get_stats(kg_bridge, &kg_stats), NIMCP_SUCCESS);
    EXPECT_EQ(kg_stats.total_updates, SIMULATION_STEPS);

    omni_wm_logging_bridge_stats_t log_stats;
    EXPECT_EQ(omni_wm_logging_bridge_get_stats(logging_bridge, &log_stats), NIMCP_SUCCESS);
    EXPECT_EQ(log_stats.total_updates, SIMULATION_STEPS);

    omni_wm_substrate_bridge_stats_t sub_stats;
    EXPECT_EQ(omni_wm_substrate_bridge_get_stats(substrate_bridge, &sub_stats), NIMCP_SUCCESS);
    EXPECT_EQ(sub_stats.total_updates, SIMULATION_STEPS);

    omni_wm_thalamic_bridge_stats_t thal_stats;
    EXPECT_EQ(omni_wm_thalamic_bridge_get_stats(thalamic_bridge, &thal_stats), NIMCP_SUCCESS);
    EXPECT_EQ(thal_stats.total_updates, SIMULATION_STEPS);

    omni_wm_hypothalamus_bridge_stats_t hypo_stats;
    EXPECT_EQ(omni_wm_hypothalamus_bridge_get_stats(hypothalamus_bridge, &hypo_stats), NIMCP_SUCCESS);
    EXPECT_EQ(hypo_stats.total_updates, SIMULATION_STEPS);

    omni_wm_parietal_bridge_stats_t par_stats;
    EXPECT_EQ(omni_wm_parietal_bridge_get_stats(parietal_bridge, &par_stats), NIMCP_SUCCESS);
    EXPECT_EQ(par_stats.total_updates, SIMULATION_STEPS);

    omni_wm_security_immune_stats_t sec_stats;
    EXPECT_EQ(omni_wm_security_immune_bridge_get_stats(security_bridge, &sec_stats), NIMCP_SUCCESS);
    EXPECT_EQ(sec_stats.total_updates, SIMULATION_STEPS);
}

/* ============================================================================
 * E2E Test: Stress Test - High Frequency Updates
 * ============================================================================ */

TEST_F(WMBridgesE2ETest, StressTestHighFrequencyUpdates) {
    CreateAllBridges();

    /* Run 1000 rapid update cycles */
    const uint32_t STRESS_CYCLES = 1000;
    const float FAST_DT = 0.001f;  /* 1ms timestep */

    auto start = std::chrono::high_resolution_clock::now();

    for (uint32_t i = 0; i < STRESS_CYCLES; i++) {
        RunAllBridgesUpdate(FAST_DT);
    }

    auto end = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);

    /* Verify no crashes and reasonable performance */
    /* 1000 cycles for 11 bridges should complete in reasonable time */
    EXPECT_LT(duration.count(), 30000);  /* Less than 30 seconds */

    /* Verify all bridges still functional */
    omni_wm_cognitive_bridge_stats_t cog_stats;
    EXPECT_EQ(omni_wm_cognitive_bridge_get_stats(cognitive_bridge, &cog_stats), NIMCP_SUCCESS);
    EXPECT_EQ(cog_stats.total_updates, STRESS_CYCLES);

    /* Verify no memory corruption by checking bridge validity */
    EXPECT_NE(cognitive_bridge->world_model, nullptr);
    EXPECT_NE(memory_bridge->world_model, nullptr);
    EXPECT_NE(tom_bridge->world_model, nullptr);
}

/* ============================================================================
 * E2E Test: Reset and Recovery
 * ============================================================================ */

TEST_F(WMBridgesE2ETest, ResetAndRecovery) {
    CreateAllBridges();

    /* Phase 1: Run some updates */
    for (uint32_t i = 0; i < SIMULATION_STEPS / 2; i++) {
        RunAllBridgesUpdate(DT);
    }

    /* Phase 2: Reset all bridges */
    EXPECT_EQ(omni_wm_cognitive_bridge_reset(cognitive_bridge), NIMCP_SUCCESS);
    EXPECT_EQ(omni_wm_memory_bridge_reset(memory_bridge), NIMCP_SUCCESS);
    EXPECT_EQ(omni_wm_tom_bridge_reset(tom_bridge), NIMCP_SUCCESS);
    EXPECT_EQ(omni_wm_plasticity_bridge_reset(plasticity_bridge), NIMCP_SUCCESS);
    EXPECT_EQ(omni_wm_kg_bridge_reset(kg_bridge), NIMCP_SUCCESS);
    EXPECT_EQ(omni_wm_logging_bridge_reset(logging_bridge), NIMCP_SUCCESS);
    EXPECT_EQ(omni_wm_substrate_bridge_reset(substrate_bridge), NIMCP_SUCCESS);
    EXPECT_EQ(omni_wm_thalamic_bridge_reset(thalamic_bridge), NIMCP_SUCCESS);
    EXPECT_EQ(omni_wm_hypothalamus_bridge_reset(hypothalamus_bridge), NIMCP_SUCCESS);
    EXPECT_EQ(omni_wm_parietal_bridge_reset(parietal_bridge), NIMCP_SUCCESS);
    EXPECT_EQ(omni_wm_security_immune_bridge_reset(security_bridge), NIMCP_SUCCESS);

    /* Phase 3: Verify stats are reset */
    omni_wm_cognitive_bridge_stats_t cog_stats;
    EXPECT_EQ(omni_wm_cognitive_bridge_get_stats(cognitive_bridge, &cog_stats), NIMCP_SUCCESS);
    EXPECT_EQ(cog_stats.total_updates, 0u);

    omni_wm_memory_bridge_stats_t mem_stats;
    EXPECT_EQ(omni_wm_memory_bridge_get_stats(memory_bridge, &mem_stats), NIMCP_SUCCESS);
    EXPECT_EQ(mem_stats.replay_sequences_received, 0u);

    /* Phase 4: Verify bridges still work after reset */
    for (uint32_t i = 0; i < WARMUP_STEPS; i++) {
        RunAllBridgesUpdate(DT);
    }

    EXPECT_EQ(omni_wm_cognitive_bridge_get_stats(cognitive_bridge, &cog_stats), NIMCP_SUCCESS);
    EXPECT_EQ(cog_stats.total_updates, WARMUP_STEPS);

    /* Phase 5: World model connection preserved */
    EXPECT_EQ(cognitive_bridge->world_model, wm);
    EXPECT_EQ(memory_bridge->world_model, wm);
}

/* ============================================================================
 * E2E Test: Module ID Uniqueness
 * ============================================================================ */

TEST_F(WMBridgesE2ETest, ModuleIDUniqueness) {
    CreateAllBridges();

    /* Collect all module IDs */
    std::vector<uint32_t> module_ids = {
        logging_bridge->base.module_id,
        security_bridge->base.module_id,
        cognitive_bridge->base.module_id,
        parietal_bridge->base.module_id,
        hypothalamus_bridge->base.module_id,
        thalamic_bridge->base.module_id,
        substrate_bridge->base.module_id,
        memory_bridge->base.module_id,
        kg_bridge->base.module_id,
        tom_bridge->base.module_id,
        plasticity_bridge->base.module_id
    };

    /* Verify all IDs are unique */
    std::sort(module_ids.begin(), module_ids.end());
    auto last = std::unique(module_ids.begin(), module_ids.end());
    EXPECT_EQ(last, module_ids.end()) << "Duplicate module IDs found!";

    /* Verify all IDs are in the expected WM bridge range (0x0E63 - 0x0E6D) */
    for (uint32_t id : module_ids) {
        EXPECT_GE(id, 0x0E63u);
        EXPECT_LE(id, 0x0E6Du);
    }
}

/* ============================================================================
 * E2E Test: Create/Destroy Cycles
 * ============================================================================ */

TEST_F(WMBridgesE2ETest, CreateDestroyCycles) {
    const uint32_t NUM_CYCLES = 10;

    for (uint32_t cycle = 0; cycle < NUM_CYCLES; cycle++) {
        /* Create all bridges */
        CreateAllBridges();

        /* Run a few updates */
        for (uint32_t i = 0; i < 5; i++) {
            RunAllBridgesUpdate(DT);
        }

        /* Destroy all bridges */
        DestroyAllBridges();

        /* Verify all bridges are null */
        EXPECT_EQ(logging_bridge, nullptr);
        EXPECT_EQ(security_bridge, nullptr);
        EXPECT_EQ(cognitive_bridge, nullptr);
        EXPECT_EQ(parietal_bridge, nullptr);
        EXPECT_EQ(hypothalamus_bridge, nullptr);
        EXPECT_EQ(thalamic_bridge, nullptr);
        EXPECT_EQ(substrate_bridge, nullptr);
        EXPECT_EQ(memory_bridge, nullptr);
        EXPECT_EQ(kg_bridge, nullptr);
        EXPECT_EQ(tom_bridge, nullptr);
        EXPECT_EQ(plasticity_bridge, nullptr);
    }
}

/* ============================================================================
 * E2E Test: Selective Bridge Creation
 * ============================================================================ */

TEST_F(WMBridgesE2ETest, SelectiveBridgeCreation) {
    /* Create only a subset of bridges (Cognitive + Memory + Plasticity) */

    /* Cognitive bridge - returns config directly */
    omni_wm_cognitive_bridge_config_t cog_config = omni_wm_cognitive_bridge_default_config();
    cognitive_bridge = omni_wm_cognitive_bridge_create(&cog_config);
    ASSERT_NE(cognitive_bridge, nullptr);
    EXPECT_EQ(omni_wm_cognitive_bridge_connect_world_model(cognitive_bridge, wm), NIMCP_SUCCESS);

    /* Memory bridge */
    omni_wm_memory_bridge_config_t mem_config;
    EXPECT_EQ(omni_wm_memory_bridge_default_config(&mem_config), NIMCP_SUCCESS);
    memory_bridge = omni_wm_memory_bridge_create(&mem_config);
    ASSERT_NE(memory_bridge, nullptr);
    EXPECT_EQ(omni_wm_memory_bridge_connect_world_model(memory_bridge, wm), NIMCP_SUCCESS);

    /* Plasticity bridge */
    omni_wm_plasticity_bridge_config_t plas_config;
    EXPECT_EQ(omni_wm_plasticity_bridge_default_config(&plas_config), NIMCP_SUCCESS);
    plasticity_bridge = omni_wm_plasticity_bridge_create(&plas_config);
    ASSERT_NE(plasticity_bridge, nullptr);
    EXPECT_EQ(omni_wm_plasticity_bridge_connect_world_model(plasticity_bridge, wm), NIMCP_SUCCESS);

    /* Run updates on just these bridges */
    for (uint32_t i = 0; i < WARMUP_STEPS; i++) {
        EXPECT_EQ(omni_wm_cognitive_bridge_update(cognitive_bridge, DT), NIMCP_SUCCESS);
        EXPECT_EQ(omni_wm_memory_bridge_update(memory_bridge, DT), NIMCP_SUCCESS);
        EXPECT_EQ(omni_wm_plasticity_bridge_update(plasticity_bridge, DT), NIMCP_SUCCESS);
    }

    /* Verify stats */
    omni_wm_cognitive_bridge_stats_t cog_stats;
    EXPECT_EQ(omni_wm_cognitive_bridge_get_stats(cognitive_bridge, &cog_stats), NIMCP_SUCCESS);
    EXPECT_EQ(cog_stats.total_updates, WARMUP_STEPS);
}

/* ============================================================================
 * E2E Test: Null Safety
 * ============================================================================ */

TEST_F(WMBridgesE2ETest, NullSafety) {
    /* Destroy null bridges should not crash */
    omni_wm_cognitive_bridge_destroy(nullptr);
    omni_wm_memory_bridge_destroy(nullptr);
    omni_wm_tom_bridge_destroy(nullptr);
    omni_wm_plasticity_bridge_destroy(nullptr);
    omni_wm_kg_bridge_destroy(nullptr);
    omni_wm_logging_bridge_destroy(nullptr);
    omni_wm_substrate_bridge_destroy(nullptr);
    omni_wm_thalamic_bridge_destroy(nullptr);
    omni_wm_hypothalamus_bridge_destroy(nullptr);
    omni_wm_parietal_bridge_destroy(nullptr);
    omni_wm_security_immune_bridge_destroy(nullptr);

    /* Reset null bridges should return error */
    EXPECT_NE(omni_wm_cognitive_bridge_reset(nullptr), NIMCP_SUCCESS);
    EXPECT_NE(omni_wm_memory_bridge_reset(nullptr), NIMCP_SUCCESS);
    EXPECT_NE(omni_wm_tom_bridge_reset(nullptr), NIMCP_SUCCESS);
    EXPECT_NE(omni_wm_plasticity_bridge_reset(nullptr), NIMCP_SUCCESS);
    EXPECT_NE(omni_wm_kg_bridge_reset(nullptr), NIMCP_SUCCESS);
    EXPECT_NE(omni_wm_logging_bridge_reset(nullptr), NIMCP_SUCCESS);
    EXPECT_NE(omni_wm_substrate_bridge_reset(nullptr), NIMCP_SUCCESS);
    EXPECT_NE(omni_wm_thalamic_bridge_reset(nullptr), NIMCP_SUCCESS);
    EXPECT_NE(omni_wm_hypothalamus_bridge_reset(nullptr), NIMCP_SUCCESS);
    EXPECT_NE(omni_wm_parietal_bridge_reset(nullptr), NIMCP_SUCCESS);
    EXPECT_NE(omni_wm_security_immune_bridge_reset(nullptr), NIMCP_SUCCESS);

    /* Update null bridges should return error */
    EXPECT_NE(omni_wm_cognitive_bridge_update(nullptr, DT), NIMCP_SUCCESS);
    EXPECT_NE(omni_wm_memory_bridge_update(nullptr, DT), NIMCP_SUCCESS);
    EXPECT_NE(omni_wm_tom_bridge_update(nullptr, DT), NIMCP_SUCCESS);
    EXPECT_NE(omni_wm_plasticity_bridge_update(nullptr, DT), NIMCP_SUCCESS);
    EXPECT_NE(omni_wm_kg_bridge_update(nullptr, DT), NIMCP_SUCCESS);
    EXPECT_NE(omni_wm_logging_bridge_update(nullptr, DT), NIMCP_SUCCESS);
    EXPECT_NE(omni_wm_substrate_bridge_update(nullptr, DT), NIMCP_SUCCESS);
    EXPECT_NE(omni_wm_thalamic_bridge_update(nullptr, DT), NIMCP_SUCCESS);
    EXPECT_NE(omni_wm_hypothalamus_bridge_update(nullptr, DT), NIMCP_SUCCESS);
    EXPECT_NE(omni_wm_parietal_bridge_update(nullptr, DT), NIMCP_SUCCESS);
    EXPECT_NE(omni_wm_security_immune_bridge_update(nullptr, DT), NIMCP_SUCCESS);
}

/* ============================================================================
 * Entry Point
 * ============================================================================ */

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
