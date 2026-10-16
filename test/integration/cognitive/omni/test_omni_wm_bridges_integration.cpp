/**
 * @file test_omni_wm_bridges_integration.cpp
 * @brief Integration tests for all 11 World Model bridges working together
 * @version 1.0.0
 * @date 2026-01-24
 *
 * Tests integration of all World Model bridges:
 * - Cognitive Bridge (Executive, Attention, Working Memory, Salience, Meta-Learning)
 * - Memory Bridge (Hippocampus, Engram, Consolidation)
 * - ToM Bridge (Theory of Mind, Mirror Neurons)
 * - Plasticity Bridge (STDP, BCM, Eligibility, STP)
 * - KG Bridge (Knowledge Graph, Module Wiring)
 * - Logging Bridge (Audit Trail)
 * - Substrate Bridge (Metabolic Constraints)
 * - Thalamic Bridge (Attention Gating)
 * - Hypothalamus Bridge (Homeostatic Control)
 * - Parietal Bridge (Spatial Processing)
 * - Security/Immune Bridge (Threat Detection)
 */

#include <gtest/gtest.h>
#include <cstring>
#include <cmath>
#include <cstdlib>
#include <thread>
#include <chrono>
#include <atomic>
#include <vector>

/* World Model core */
#include "cognitive/omni/nimcp_omni_world_model.h"

/* All 11 WM bridges */
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

/* Bio-async for message routing */
#include "async/nimcp_bio_messages.h"

/* Memory utilities */
#include "utils/memory/nimcp_memory.h"

/* ============================================================================
 * Test Fixture for World Model Bridges Integration
 * ============================================================================ */

class WMBridgesIntegrationTest : public ::testing::Test {
protected:
    static constexpr uint32_t STATE_DIM = 64;
    static constexpr uint32_t ACTION_DIM = 16;
    static constexpr uint32_t OBS_DIM = 64;
    static constexpr float DT = 0.016f; /* 60 FPS timestep */

    /* World Model */
    omni_world_model_t* wm = nullptr;

    /* All bridges */
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

    void SetUp() override {
        /* Create world model with RSSM configuration */
        omni_wm_config_t wm_config;
        omni_wm_get_default_config(&wm_config);
        wm_config.state_dim = STATE_DIM;
        wm_config.action_dim = ACTION_DIM;
        wm_config.obs_dim = OBS_DIM;
        wm_config.rssm_h_dim = STATE_DIM;
        wm_config.rssm_z_dim = STATE_DIM / 2;
        wm_config.use_rssm = true;
        wm_config.enable_lateral = true;
        wm_config.enable_hierarchical = true;
        wm_config.enable_dreaming = true;

        wm = omni_wm_create(&wm_config);
    }

    void TearDown() override {
        /* Destroy bridges in reverse order of dependency */
        if (security_bridge) omni_wm_security_immune_bridge_destroy(security_bridge);
        if (parietal_bridge) omni_wm_parietal_bridge_destroy(parietal_bridge);
        if (hypothalamus_bridge) omni_wm_hypothalamus_bridge_destroy(hypothalamus_bridge);
        if (thalamic_bridge) omni_wm_thalamic_bridge_destroy(thalamic_bridge);
        if (substrate_bridge) omni_wm_substrate_bridge_destroy(substrate_bridge);
        if (logging_bridge) omni_wm_logging_bridge_destroy(logging_bridge);
        if (kg_bridge) omni_wm_kg_bridge_destroy(kg_bridge);
        if (plasticity_bridge) omni_wm_plasticity_bridge_destroy(plasticity_bridge);
        if (tom_bridge) omni_wm_tom_bridge_destroy(tom_bridge);
        if (memory_bridge) omni_wm_memory_bridge_destroy(memory_bridge);
        if (cognitive_bridge) omni_wm_cognitive_bridge_destroy(cognitive_bridge);
        if (wm) omni_wm_destroy(wm);
    }

    /* Helper to create all bridges with world model connected */
    void CreateAllBridges() {
        /* Cognitive Bridge */
        omni_wm_cognitive_bridge_config_t cog_config = omni_wm_cognitive_bridge_default_config();
        cognitive_bridge = omni_wm_cognitive_bridge_create(&cog_config);
        if (cognitive_bridge && wm) {
            omni_wm_cognitive_bridge_connect_world_model(cognitive_bridge, wm);
        }

        /* Memory Bridge */
        omni_wm_memory_bridge_config_t mem_config;
        omni_wm_memory_bridge_default_config(&mem_config);
        memory_bridge = omni_wm_memory_bridge_create(&mem_config);
        if (memory_bridge && wm) {
            omni_wm_memory_bridge_connect_world_model(memory_bridge, wm);
        }

        /* ToM Bridge */
        omni_wm_tom_bridge_config_t tom_config;
        omni_wm_tom_bridge_default_config(&tom_config);
        tom_bridge = omni_wm_tom_bridge_create(&tom_config);
        if (tom_bridge && wm) {
            omni_wm_tom_bridge_connect_world_model(tom_bridge, wm);
        }

        /* Plasticity Bridge */
        omni_wm_plasticity_bridge_config_t plast_config;
        omni_wm_plasticity_bridge_default_config(&plast_config);
        plasticity_bridge = omni_wm_plasticity_bridge_create(&plast_config);
        if (plasticity_bridge && wm) {
            omni_wm_plasticity_bridge_connect_world_model(plasticity_bridge, wm);
        }

        /* KG Bridge */
        omni_wm_kg_bridge_config_t kg_config;
        omni_wm_kg_bridge_default_config(&kg_config);
        kg_bridge = omni_wm_kg_bridge_create(&kg_config);
        if (kg_bridge && wm) {
            omni_wm_kg_bridge_connect_world_model(kg_bridge, wm);
        }

        /* Logging Bridge */
        omni_wm_logging_bridge_config_t log_config;
        omni_wm_logging_bridge_default_config(&log_config);
        logging_bridge = omni_wm_logging_bridge_create(&log_config);
        if (logging_bridge && wm) {
            omni_wm_logging_bridge_connect_world_model(logging_bridge, wm);
        }

        /* Substrate Bridge */
        omni_wm_substrate_bridge_config_t sub_config;
        omni_wm_substrate_bridge_default_config(&sub_config);
        substrate_bridge = omni_wm_substrate_bridge_create(&sub_config);
        if (substrate_bridge && wm) {
            omni_wm_substrate_bridge_connect_world_model(substrate_bridge, wm);
        }

        /* Thalamic Bridge */
        omni_wm_thalamic_bridge_config_t thal_config;
        omni_wm_thalamic_bridge_default_config(&thal_config);
        thalamic_bridge = omni_wm_thalamic_bridge_create(&thal_config);
        if (thalamic_bridge && wm) {
            omni_wm_thalamic_bridge_connect_world_model(thalamic_bridge, wm);
        }

        /* Hypothalamus Bridge */
        omni_wm_hypothalamus_bridge_config_t hypo_config;
        omni_wm_hypothalamus_bridge_default_config(&hypo_config);
        hypothalamus_bridge = omni_wm_hypothalamus_bridge_create(&hypo_config);
        if (hypothalamus_bridge && wm) {
            omni_wm_hypothalamus_bridge_connect_world_model(hypothalamus_bridge, wm);
        }

        /* Parietal Bridge */
        omni_wm_parietal_bridge_config_t par_config;
        omni_wm_parietal_bridge_default_config(&par_config);
        parietal_bridge = omni_wm_parietal_bridge_create(&par_config);
        if (parietal_bridge && wm) {
            omni_wm_parietal_bridge_connect_world_model(parietal_bridge, wm);
        }

        /* Security/Immune Bridge */
        omni_wm_security_immune_bridge_config_t sec_config;
        omni_wm_security_immune_bridge_default_config(&sec_config);
        security_bridge = omni_wm_security_immune_bridge_create(&sec_config);
        if (security_bridge && wm) {
            omni_wm_security_immune_bridge_connect_world_model(security_bridge, wm);
        }
    }

    /* Helper to generate random pattern */
    void generate_random_pattern(float* pattern, uint32_t dim) {
        for (uint32_t i = 0; i < dim; i++) {
            pattern[i] = ((float)rand() / RAND_MAX) * 2.0f - 1.0f;
        }
    }

    /* Count successfully created bridges */
    int CountCreatedBridges() {
        int count = 0;
        if (cognitive_bridge) count++;
        if (memory_bridge) count++;
        if (tom_bridge) count++;
        if (plasticity_bridge) count++;
        if (kg_bridge) count++;
        if (logging_bridge) count++;
        if (substrate_bridge) count++;
        if (thalamic_bridge) count++;
        if (hypothalamus_bridge) count++;
        if (parietal_bridge) count++;
        if (security_bridge) count++;
        return count;
    }
};

/* ============================================================================
 * World Model Creation Tests
 * ============================================================================ */

TEST_F(WMBridgesIntegrationTest, WorldModelCreation) {
    EXPECT_NE(wm, nullptr);
}

TEST_F(WMBridgesIntegrationTest, WorldModelDefaultConfig) {
    omni_wm_config_t config;
    nimcp_error_t ret = omni_wm_get_default_config(&config);
    EXPECT_EQ(ret, NIMCP_SUCCESS);
    EXPECT_GT(config.state_dim, 0u);
    EXPECT_GT(config.action_dim, 0u);
}

/* ============================================================================
 * Individual Bridge Creation Tests
 * ============================================================================ */

TEST_F(WMBridgesIntegrationTest, CognitiveBridgeCreation) {
    if (!wm) GTEST_SKIP();

    omni_wm_cognitive_bridge_config_t config = omni_wm_cognitive_bridge_default_config();
    cognitive_bridge = omni_wm_cognitive_bridge_create(&config);
    EXPECT_NE(cognitive_bridge, nullptr);

    if (cognitive_bridge) {
        nimcp_error_t ret = omni_wm_cognitive_bridge_connect_world_model(cognitive_bridge, wm);
        EXPECT_EQ(ret, NIMCP_SUCCESS);
        EXPECT_TRUE(omni_wm_cognitive_bridge_is_connected(cognitive_bridge));
    }
}

TEST_F(WMBridgesIntegrationTest, MemoryBridgeCreation) {
    if (!wm) GTEST_SKIP();

    omni_wm_memory_bridge_config_t config;
    omni_wm_memory_bridge_default_config(&config);
    memory_bridge = omni_wm_memory_bridge_create(&config);
    EXPECT_NE(memory_bridge, nullptr);

    if (memory_bridge) {
        nimcp_error_t ret = omni_wm_memory_bridge_connect_world_model(memory_bridge, wm);
        EXPECT_EQ(ret, NIMCP_SUCCESS);
        EXPECT_TRUE(omni_wm_memory_bridge_is_connected(memory_bridge));
    }
}

TEST_F(WMBridgesIntegrationTest, ToMBridgeCreation) {
    if (!wm) GTEST_SKIP();

    omni_wm_tom_bridge_config_t config;
    omni_wm_tom_bridge_default_config(&config);
    tom_bridge = omni_wm_tom_bridge_create(&config);
    EXPECT_NE(tom_bridge, nullptr);

    if (tom_bridge) {
        nimcp_error_t ret = omni_wm_tom_bridge_connect_world_model(tom_bridge, wm);
        EXPECT_EQ(ret, NIMCP_SUCCESS);
    }
}

TEST_F(WMBridgesIntegrationTest, PlasticityBridgeCreation) {
    if (!wm) GTEST_SKIP();

    omni_wm_plasticity_bridge_config_t config;
    omni_wm_plasticity_bridge_default_config(&config);
    plasticity_bridge = omni_wm_plasticity_bridge_create(&config);
    EXPECT_NE(plasticity_bridge, nullptr);

    if (plasticity_bridge) {
        nimcp_error_t ret = omni_wm_plasticity_bridge_connect_world_model(plasticity_bridge, wm);
        EXPECT_EQ(ret, NIMCP_SUCCESS);
        EXPECT_TRUE(omni_wm_plasticity_bridge_is_connected(plasticity_bridge));
    }
}

TEST_F(WMBridgesIntegrationTest, KGBridgeCreation) {
    if (!wm) GTEST_SKIP();

    omni_wm_kg_bridge_config_t config;
    omni_wm_kg_bridge_default_config(&config);
    kg_bridge = omni_wm_kg_bridge_create(&config);
    EXPECT_NE(kg_bridge, nullptr);

    if (kg_bridge) {
        nimcp_error_t ret = omni_wm_kg_bridge_connect_world_model(kg_bridge, wm);
        EXPECT_EQ(ret, NIMCP_SUCCESS);
        EXPECT_TRUE(omni_wm_kg_bridge_is_connected(kg_bridge));
    }
}

TEST_F(WMBridgesIntegrationTest, LoggingBridgeCreation) {
    if (!wm) GTEST_SKIP();

    omni_wm_logging_bridge_config_t config;
    omni_wm_logging_bridge_default_config(&config);
    logging_bridge = omni_wm_logging_bridge_create(&config);
    EXPECT_NE(logging_bridge, nullptr);

    if (logging_bridge) {
        nimcp_error_t ret = omni_wm_logging_bridge_connect_world_model(logging_bridge, wm);
        EXPECT_EQ(ret, NIMCP_SUCCESS);
        EXPECT_TRUE(omni_wm_logging_bridge_is_connected(logging_bridge));
    }
}

TEST_F(WMBridgesIntegrationTest, SubstrateBridgeCreation) {
    if (!wm) GTEST_SKIP();

    omni_wm_substrate_bridge_config_t config;
    omni_wm_substrate_bridge_default_config(&config);
    substrate_bridge = omni_wm_substrate_bridge_create(&config);
    EXPECT_NE(substrate_bridge, nullptr);

    if (substrate_bridge) {
        nimcp_error_t ret = omni_wm_substrate_bridge_connect_world_model(substrate_bridge, wm);
        EXPECT_EQ(ret, NIMCP_SUCCESS);
        EXPECT_TRUE(omni_wm_substrate_bridge_is_connected(substrate_bridge));
    }
}

TEST_F(WMBridgesIntegrationTest, ThalamicBridgeCreation) {
    if (!wm) GTEST_SKIP();

    omni_wm_thalamic_bridge_config_t config;
    omni_wm_thalamic_bridge_default_config(&config);
    thalamic_bridge = omni_wm_thalamic_bridge_create(&config);
    EXPECT_NE(thalamic_bridge, nullptr);

    if (thalamic_bridge) {
        nimcp_error_t ret = omni_wm_thalamic_bridge_connect_world_model(thalamic_bridge, wm);
        EXPECT_EQ(ret, NIMCP_SUCCESS);
        EXPECT_TRUE(omni_wm_thalamic_bridge_is_connected(thalamic_bridge));
    }
}

TEST_F(WMBridgesIntegrationTest, HypothalamusBridgeCreation) {
    if (!wm) GTEST_SKIP();

    omni_wm_hypothalamus_bridge_config_t config;
    omni_wm_hypothalamus_bridge_default_config(&config);
    hypothalamus_bridge = omni_wm_hypothalamus_bridge_create(&config);
    EXPECT_NE(hypothalamus_bridge, nullptr);

    if (hypothalamus_bridge) {
        nimcp_error_t ret = omni_wm_hypothalamus_bridge_connect_world_model(hypothalamus_bridge, wm);
        EXPECT_EQ(ret, NIMCP_SUCCESS);
        EXPECT_TRUE(omni_wm_hypothalamus_bridge_is_connected(hypothalamus_bridge));
    }
}

TEST_F(WMBridgesIntegrationTest, ParietalBridgeCreation) {
    if (!wm) GTEST_SKIP();

    omni_wm_parietal_bridge_config_t config;
    omni_wm_parietal_bridge_default_config(&config);
    parietal_bridge = omni_wm_parietal_bridge_create(&config);
    EXPECT_NE(parietal_bridge, nullptr);

    if (parietal_bridge) {
        nimcp_error_t ret = omni_wm_parietal_bridge_connect_world_model(parietal_bridge, wm);
        EXPECT_EQ(ret, NIMCP_SUCCESS);
        EXPECT_TRUE(omni_wm_parietal_bridge_is_connected(parietal_bridge));
    }
}

TEST_F(WMBridgesIntegrationTest, SecurityImmuneBridgeCreation) {
    if (!wm) GTEST_SKIP();

    omni_wm_security_immune_bridge_config_t config;
    omni_wm_security_immune_bridge_default_config(&config);
    security_bridge = omni_wm_security_immune_bridge_create(&config);
    EXPECT_NE(security_bridge, nullptr);

    if (security_bridge) {
        nimcp_error_t ret = omni_wm_security_immune_bridge_connect_world_model(security_bridge, wm);
        EXPECT_EQ(ret, NIMCP_SUCCESS);
        EXPECT_TRUE(omni_wm_security_immune_bridge_is_connected(security_bridge));
    }
}

/* ============================================================================
 * All Bridges Creation Test
 * ============================================================================ */

TEST_F(WMBridgesIntegrationTest, AllBridgesCreation) {
    if (!wm) GTEST_SKIP();

    CreateAllBridges();

    /* Should create all 11 bridges */
    int bridge_count = CountCreatedBridges();
    EXPECT_EQ(bridge_count, 11);
}

/* ============================================================================
 * Multi-Bridge Update Cycle Tests
 * ============================================================================ */

TEST_F(WMBridgesIntegrationTest, MultiBridgeUpdateCycle) {
    if (!wm) GTEST_SKIP();

    CreateAllBridges();

    /* Run update cycle on all bridges */
    if (cognitive_bridge) {
        nimcp_error_t ret = omni_wm_cognitive_bridge_update(cognitive_bridge, DT);
        EXPECT_EQ(ret, NIMCP_SUCCESS);
    }

    if (memory_bridge) {
        nimcp_error_t ret = omni_wm_memory_bridge_update(memory_bridge, DT);
        EXPECT_EQ(ret, NIMCP_SUCCESS);
    }

    if (tom_bridge) {
        nimcp_error_t ret = omni_wm_tom_bridge_update(tom_bridge, DT);
        EXPECT_EQ(ret, NIMCP_SUCCESS);
    }

    if (plasticity_bridge) {
        nimcp_error_t ret = omni_wm_plasticity_bridge_update(plasticity_bridge, DT);
        EXPECT_EQ(ret, NIMCP_SUCCESS);
    }

    if (kg_bridge) {
        nimcp_error_t ret = omni_wm_kg_bridge_update(kg_bridge, DT);
        EXPECT_EQ(ret, NIMCP_SUCCESS);
    }

    if (logging_bridge) {
        nimcp_error_t ret = omni_wm_logging_bridge_update(logging_bridge, DT);
        EXPECT_EQ(ret, NIMCP_SUCCESS);
    }

    if (substrate_bridge) {
        nimcp_error_t ret = omni_wm_substrate_bridge_update(substrate_bridge, DT);
        EXPECT_EQ(ret, NIMCP_SUCCESS);
    }

    if (thalamic_bridge) {
        nimcp_error_t ret = omni_wm_thalamic_bridge_update(thalamic_bridge, DT);
        EXPECT_EQ(ret, NIMCP_SUCCESS);
    }

    if (hypothalamus_bridge) {
        nimcp_error_t ret = omni_wm_hypothalamus_bridge_update(hypothalamus_bridge, DT);
        EXPECT_EQ(ret, NIMCP_SUCCESS);
    }

    if (parietal_bridge) {
        nimcp_error_t ret = omni_wm_parietal_bridge_update(parietal_bridge, DT);
        EXPECT_EQ(ret, NIMCP_SUCCESS);
    }

    if (security_bridge) {
        nimcp_error_t ret = omni_wm_security_immune_bridge_update(security_bridge, DT);
        EXPECT_EQ(ret, NIMCP_SUCCESS);
    }
}

TEST_F(WMBridgesIntegrationTest, MultipleUpdateCycles) {
    if (!wm) GTEST_SKIP();

    CreateAllBridges();

    const int NUM_CYCLES = 100;
    int successful_cycles = 0;

    for (int cycle = 0; cycle < NUM_CYCLES; cycle++) {
        bool cycle_success = true;

        /* Update all bridges in order */
        if (cognitive_bridge) {
            if (omni_wm_cognitive_bridge_update(cognitive_bridge, DT) != NIMCP_SUCCESS) {
                cycle_success = false;
            }
        }
        if (memory_bridge) {
            if (omni_wm_memory_bridge_update(memory_bridge, DT) != NIMCP_SUCCESS) {
                cycle_success = false;
            }
        }
        if (tom_bridge) {
            if (omni_wm_tom_bridge_update(tom_bridge, DT) != NIMCP_SUCCESS) {
                cycle_success = false;
            }
        }
        if (plasticity_bridge) {
            if (omni_wm_plasticity_bridge_update(plasticity_bridge, DT) != NIMCP_SUCCESS) {
                cycle_success = false;
            }
        }
        if (kg_bridge) {
            if (omni_wm_kg_bridge_update(kg_bridge, DT) != NIMCP_SUCCESS) {
                cycle_success = false;
            }
        }
        if (logging_bridge) {
            if (omni_wm_logging_bridge_update(logging_bridge, DT) != NIMCP_SUCCESS) {
                cycle_success = false;
            }
        }
        if (substrate_bridge) {
            if (omni_wm_substrate_bridge_update(substrate_bridge, DT) != NIMCP_SUCCESS) {
                cycle_success = false;
            }
        }
        if (thalamic_bridge) {
            if (omni_wm_thalamic_bridge_update(thalamic_bridge, DT) != NIMCP_SUCCESS) {
                cycle_success = false;
            }
        }
        if (hypothalamus_bridge) {
            if (omni_wm_hypothalamus_bridge_update(hypothalamus_bridge, DT) != NIMCP_SUCCESS) {
                cycle_success = false;
            }
        }
        if (parietal_bridge) {
            if (omni_wm_parietal_bridge_update(parietal_bridge, DT) != NIMCP_SUCCESS) {
                cycle_success = false;
            }
        }
        if (security_bridge) {
            if (omni_wm_security_immune_bridge_update(security_bridge, DT) != NIMCP_SUCCESS) {
                cycle_success = false;
            }
        }

        if (cycle_success) successful_cycles++;
    }

    EXPECT_EQ(successful_cycles, NUM_CYCLES);
}

/* ============================================================================
 * Cross-Bridge Effects Tests
 * ============================================================================ */

TEST_F(WMBridgesIntegrationTest, CognitiveBridgeEffectsQuery) {
    if (!wm) GTEST_SKIP();

    omni_wm_cognitive_bridge_config_t config = omni_wm_cognitive_bridge_default_config();
    cognitive_bridge = omni_wm_cognitive_bridge_create(&config);
    if (!cognitive_bridge) GTEST_SKIP();

    omni_wm_cognitive_bridge_connect_world_model(cognitive_bridge, wm);
    omni_wm_cognitive_bridge_update(cognitive_bridge, DT);

    /* Query effects */
    const omni_wm_to_cognitive_effects_t* wm_effects =
        omni_wm_cognitive_bridge_get_wm_effects(cognitive_bridge);
    const cognitive_to_omni_wm_effects_t* cog_effects =
        omni_wm_cognitive_bridge_get_cognitive_effects(cognitive_bridge);

    EXPECT_NE(wm_effects, nullptr);
    EXPECT_NE(cog_effects, nullptr);
}

TEST_F(WMBridgesIntegrationTest, MemoryBridgeEffectsQuery) {
    if (!wm) GTEST_SKIP();

    omni_wm_memory_bridge_config_t config;
    omni_wm_memory_bridge_default_config(&config);
    memory_bridge = omni_wm_memory_bridge_create(&config);
    if (!memory_bridge) GTEST_SKIP();

    omni_wm_memory_bridge_connect_world_model(memory_bridge, wm);
    omni_wm_memory_bridge_update(memory_bridge, DT);

    /* Query effects */
    const omni_wm_to_memory_effects_t* wm_effects =
        omni_wm_memory_bridge_get_wm_effects(memory_bridge);
    const memory_to_omni_wm_effects_t* mem_effects =
        omni_wm_memory_bridge_get_memory_effects(memory_bridge);

    EXPECT_NE(wm_effects, nullptr);
    EXPECT_NE(mem_effects, nullptr);
}

TEST_F(WMBridgesIntegrationTest, ToMBridgeEffectsQuery) {
    if (!wm) GTEST_SKIP();

    omni_wm_tom_bridge_config_t config;
    omni_wm_tom_bridge_default_config(&config);
    tom_bridge = omni_wm_tom_bridge_create(&config);
    if (!tom_bridge) GTEST_SKIP();

    omni_wm_tom_bridge_connect_world_model(tom_bridge, wm);
    omni_wm_tom_bridge_update(tom_bridge, DT);

    /* Query effects */
    const omni_wm_to_tom_effects_t* wm_effects =
        omni_wm_tom_bridge_get_wm_effects(tom_bridge);
    const tom_to_omni_wm_effects_t* tom_effects =
        omni_wm_tom_bridge_get_tom_effects(tom_bridge);

    EXPECT_NE(wm_effects, nullptr);
    EXPECT_NE(tom_effects, nullptr);
}

TEST_F(WMBridgesIntegrationTest, PlasticityBridgeEffectsQuery) {
    if (!wm) GTEST_SKIP();

    omni_wm_plasticity_bridge_config_t config;
    omni_wm_plasticity_bridge_default_config(&config);
    plasticity_bridge = omni_wm_plasticity_bridge_create(&config);
    if (!plasticity_bridge) GTEST_SKIP();

    omni_wm_plasticity_bridge_connect_world_model(plasticity_bridge, wm);
    omni_wm_plasticity_bridge_update(plasticity_bridge, DT);

    /* Query effects */
    const plasticity_to_omni_wm_effects_t* plast_effects =
        omni_wm_plasticity_bridge_get_plasticity_effects(plasticity_bridge);
    const omni_wm_to_plasticity_effects_t* wm_effects =
        omni_wm_plasticity_bridge_get_wm_effects(plasticity_bridge);

    EXPECT_NE(plast_effects, nullptr);
    EXPECT_NE(wm_effects, nullptr);
}

/* ============================================================================
 * Statistics Collection Tests
 * ============================================================================ */

TEST_F(WMBridgesIntegrationTest, CognitiveBridgeStats) {
    if (!wm) GTEST_SKIP();

    omni_wm_cognitive_bridge_config_t config = omni_wm_cognitive_bridge_default_config();
    cognitive_bridge = omni_wm_cognitive_bridge_create(&config);
    if (!cognitive_bridge) GTEST_SKIP();

    omni_wm_cognitive_bridge_connect_world_model(cognitive_bridge, wm);

    /* Run some updates */
    for (int i = 0; i < 10; i++) {
        omni_wm_cognitive_bridge_update(cognitive_bridge, DT);
    }

    /* Get stats */
    omni_wm_cognitive_bridge_stats_t stats;
    nimcp_error_t ret = omni_wm_cognitive_bridge_get_stats(cognitive_bridge, &stats);
    EXPECT_EQ(ret, NIMCP_SUCCESS);
    EXPECT_EQ(stats.total_updates, 10u);

    /* Reset stats */
    ret = omni_wm_cognitive_bridge_reset_stats(cognitive_bridge);
    EXPECT_EQ(ret, NIMCP_SUCCESS);

    ret = omni_wm_cognitive_bridge_get_stats(cognitive_bridge, &stats);
    EXPECT_EQ(ret, NIMCP_SUCCESS);
    EXPECT_EQ(stats.total_updates, 0u);
}

TEST_F(WMBridgesIntegrationTest, MemoryBridgeStats) {
    if (!wm) GTEST_SKIP();

    omni_wm_memory_bridge_config_t config;
    omni_wm_memory_bridge_default_config(&config);
    memory_bridge = omni_wm_memory_bridge_create(&config);
    if (!memory_bridge) GTEST_SKIP();

    omni_wm_memory_bridge_connect_world_model(memory_bridge, wm);

    for (int i = 0; i < 10; i++) {
        omni_wm_memory_bridge_update(memory_bridge, DT);
    }

    omni_wm_memory_bridge_stats_t stats;
    nimcp_error_t ret = omni_wm_memory_bridge_get_stats(memory_bridge, &stats);
    EXPECT_EQ(ret, NIMCP_SUCCESS);
    EXPECT_EQ(stats.total_updates, 10u);
}

TEST_F(WMBridgesIntegrationTest, ToMBridgeStats) {
    if (!wm) GTEST_SKIP();

    omni_wm_tom_bridge_config_t config;
    omni_wm_tom_bridge_default_config(&config);
    tom_bridge = omni_wm_tom_bridge_create(&config);
    if (!tom_bridge) GTEST_SKIP();

    omni_wm_tom_bridge_connect_world_model(tom_bridge, wm);

    for (int i = 0; i < 10; i++) {
        omni_wm_tom_bridge_update(tom_bridge, DT);
    }

    omni_wm_tom_bridge_stats_t stats;
    nimcp_error_t ret = omni_wm_tom_bridge_get_stats(tom_bridge, &stats);
    EXPECT_EQ(ret, NIMCP_SUCCESS);
    EXPECT_EQ(stats.total_updates, 10u);
}

TEST_F(WMBridgesIntegrationTest, PlasticityBridgeStats) {
    if (!wm) GTEST_SKIP();

    omni_wm_plasticity_bridge_config_t config;
    omni_wm_plasticity_bridge_default_config(&config);
    plasticity_bridge = omni_wm_plasticity_bridge_create(&config);
    if (!plasticity_bridge) GTEST_SKIP();

    omni_wm_plasticity_bridge_connect_world_model(plasticity_bridge, wm);

    for (int i = 0; i < 10; i++) {
        omni_wm_plasticity_bridge_update(plasticity_bridge, DT);
    }

    omni_wm_plasticity_bridge_stats_t stats;
    nimcp_error_t ret = omni_wm_plasticity_bridge_get_stats(plasticity_bridge, &stats);
    EXPECT_EQ(ret, NIMCP_SUCCESS);
    EXPECT_EQ(stats.total_updates, 10u);
}

/* ============================================================================
 * Bridge Reset Tests
 * ============================================================================ */

TEST_F(WMBridgesIntegrationTest, CognitiveBridgeReset) {
    if (!wm) GTEST_SKIP();

    omni_wm_cognitive_bridge_config_t config = omni_wm_cognitive_bridge_default_config();
    cognitive_bridge = omni_wm_cognitive_bridge_create(&config);
    if (!cognitive_bridge) GTEST_SKIP();

    omni_wm_cognitive_bridge_connect_world_model(cognitive_bridge, wm);

    /* Update to accumulate state */
    for (int i = 0; i < 10; i++) {
        omni_wm_cognitive_bridge_update(cognitive_bridge, DT);
    }

    /* Reset */
    nimcp_error_t ret = omni_wm_cognitive_bridge_reset(cognitive_bridge);
    EXPECT_EQ(ret, NIMCP_SUCCESS);

    /* Connection should remain */
    EXPECT_TRUE(omni_wm_cognitive_bridge_is_connected(cognitive_bridge));
}

TEST_F(WMBridgesIntegrationTest, MemoryBridgeReset) {
    if (!wm) GTEST_SKIP();

    omni_wm_memory_bridge_config_t config;
    omni_wm_memory_bridge_default_config(&config);
    memory_bridge = omni_wm_memory_bridge_create(&config);
    if (!memory_bridge) GTEST_SKIP();

    omni_wm_memory_bridge_connect_world_model(memory_bridge, wm);

    for (int i = 0; i < 10; i++) {
        omni_wm_memory_bridge_update(memory_bridge, DT);
    }

    nimcp_error_t ret = omni_wm_memory_bridge_reset(memory_bridge);
    EXPECT_EQ(ret, NIMCP_SUCCESS);
    EXPECT_TRUE(omni_wm_memory_bridge_is_connected(memory_bridge));
}

TEST_F(WMBridgesIntegrationTest, ToMBridgeReset) {
    if (!wm) GTEST_SKIP();

    omni_wm_tom_bridge_config_t config;
    omni_wm_tom_bridge_default_config(&config);
    tom_bridge = omni_wm_tom_bridge_create(&config);
    if (!tom_bridge) GTEST_SKIP();

    omni_wm_tom_bridge_connect_world_model(tom_bridge, wm);

    for (int i = 0; i < 10; i++) {
        omni_wm_tom_bridge_update(tom_bridge, DT);
    }

    nimcp_error_t ret = omni_wm_tom_bridge_reset(tom_bridge);
    EXPECT_EQ(ret, NIMCP_SUCCESS);
}

TEST_F(WMBridgesIntegrationTest, PlasticityBridgeReset) {
    if (!wm) GTEST_SKIP();

    omni_wm_plasticity_bridge_config_t config;
    omni_wm_plasticity_bridge_default_config(&config);
    plasticity_bridge = omni_wm_plasticity_bridge_create(&config);
    if (!plasticity_bridge) GTEST_SKIP();

    omni_wm_plasticity_bridge_connect_world_model(plasticity_bridge, wm);

    for (int i = 0; i < 10; i++) {
        omni_wm_plasticity_bridge_update(plasticity_bridge, DT);
    }

    nimcp_error_t ret = omni_wm_plasticity_bridge_reset(plasticity_bridge);
    EXPECT_EQ(ret, NIMCP_SUCCESS);
    EXPECT_TRUE(omni_wm_plasticity_bridge_is_connected(plasticity_bridge));
}

/* ============================================================================
 * Shared World Model Tests
 * ============================================================================ */

TEST_F(WMBridgesIntegrationTest, SharedWorldModelAcrossBridges) {
    if (!wm) GTEST_SKIP();

    /* Create multiple bridges sharing the same world model */
    omni_wm_cognitive_bridge_config_t cog_config = omni_wm_cognitive_bridge_default_config();
    cognitive_bridge = omni_wm_cognitive_bridge_create(&cog_config);

    omni_wm_memory_bridge_config_t mem_config;
    omni_wm_memory_bridge_default_config(&mem_config);
    memory_bridge = omni_wm_memory_bridge_create(&mem_config);

    omni_wm_plasticity_bridge_config_t plast_config;
    omni_wm_plasticity_bridge_default_config(&plast_config);
    plasticity_bridge = omni_wm_plasticity_bridge_create(&plast_config);

    /* Connect all to same WM */
    if (cognitive_bridge) {
        omni_wm_cognitive_bridge_connect_world_model(cognitive_bridge, wm);
        EXPECT_TRUE(omni_wm_cognitive_bridge_is_connected(cognitive_bridge));
    }

    if (memory_bridge) {
        omni_wm_memory_bridge_connect_world_model(memory_bridge, wm);
        EXPECT_TRUE(omni_wm_memory_bridge_is_connected(memory_bridge));
    }

    if (plasticity_bridge) {
        omni_wm_plasticity_bridge_connect_world_model(plasticity_bridge, wm);
        EXPECT_TRUE(omni_wm_plasticity_bridge_is_connected(plasticity_bridge));
    }

    /* Run updates on all bridges - should not interfere */
    for (int i = 0; i < 20; i++) {
        if (cognitive_bridge) {
            EXPECT_EQ(omni_wm_cognitive_bridge_update(cognitive_bridge, DT), NIMCP_SUCCESS);
        }
        if (memory_bridge) {
            EXPECT_EQ(omni_wm_memory_bridge_update(memory_bridge, DT), NIMCP_SUCCESS);
        }
        if (plasticity_bridge) {
            EXPECT_EQ(omni_wm_plasticity_bridge_update(plasticity_bridge, DT), NIMCP_SUCCESS);
        }
    }
}

/* ============================================================================
 * Resource Cleanup Tests
 * ============================================================================ */

TEST_F(WMBridgesIntegrationTest, NullSafeDestroy) {
    /* All destroy functions should handle NULL safely */
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

    /* No crash = success */
    SUCCEED();
}

TEST_F(WMBridgesIntegrationTest, CreateDestroyMultipleTimes) {
    if (!wm) GTEST_SKIP();

    /* Create and destroy bridges multiple times */
    for (int i = 0; i < 5; i++) {
        omni_wm_cognitive_bridge_config_t config = omni_wm_cognitive_bridge_default_config();
        omni_wm_cognitive_bridge_t* bridge = omni_wm_cognitive_bridge_create(&config);
        ASSERT_NE(bridge, nullptr);

        omni_wm_cognitive_bridge_connect_world_model(bridge, wm);
        omni_wm_cognitive_bridge_update(bridge, DT);
        omni_wm_cognitive_bridge_destroy(bridge);
    }

    SUCCEED();
}

/* ============================================================================
 * Stress Tests
 * ============================================================================ */

TEST_F(WMBridgesIntegrationTest, HighFrequencyUpdates) {
    if (!wm) GTEST_SKIP();

    CreateAllBridges();

    const int NUM_UPDATES = 1000;
    int successful_updates = 0;

    for (int i = 0; i < NUM_UPDATES; i++) {
        bool all_success = true;

        if (cognitive_bridge &&
            omni_wm_cognitive_bridge_update(cognitive_bridge, DT) != NIMCP_SUCCESS) {
            all_success = false;
        }
        if (memory_bridge &&
            omni_wm_memory_bridge_update(memory_bridge, DT) != NIMCP_SUCCESS) {
            all_success = false;
        }
        if (tom_bridge &&
            omni_wm_tom_bridge_update(tom_bridge, DT) != NIMCP_SUCCESS) {
            all_success = false;
        }
        if (plasticity_bridge &&
            omni_wm_plasticity_bridge_update(plasticity_bridge, DT) != NIMCP_SUCCESS) {
            all_success = false;
        }

        if (all_success) successful_updates++;
    }

    EXPECT_EQ(successful_updates, NUM_UPDATES);
}
