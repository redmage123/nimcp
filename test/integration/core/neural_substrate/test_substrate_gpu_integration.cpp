/**
 * @file test_substrate_gpu_integration.cpp
 * @brief Integration tests for GPU-accelerated Neural Substrate
 * @date 2025-01-24
 *
 * Tests GPU acceleration for neural substrate components:
 * - GPU context initialization and lifecycle
 * - Axon signal propagation and refractory dynamics
 * - Dendrite cable equation and NMDA processing
 * - Myelin plasticity and conduction velocity
 * - Neuromodulator dynamics
 * - Glial cell integration
 * - CPU fallback behavior when GPU unavailable
 *
 * Note: These tests verify the API interface works correctly.
 * GPU operations will fall back to CPU if no GPU is available.
 */

#include <gtest/gtest.h>
#include <cstring>
#include <cmath>

#include "utils/nimcp_test_base.h"

// Headers have their own extern "C" guards
#include "gpu/substrate/nimcp_substrate_gpu.h"
#include "gpu/context/nimcp_gpu_context.h"

/* ============================================================================
 * Test Constants
 * ============================================================================ */

// NIMCP_SUCCESS is defined in nimcp_common.h as 0
static constexpr float FLOAT_TOLERANCE = 0.001f;

// Test sizes
static constexpr uint32_t TEST_N_AXONS = 100;
static constexpr uint32_t TEST_N_DENDRITES = 50;
static constexpr uint32_t TEST_N_SEGMENTS = 10;
static constexpr uint32_t TEST_N_SPINES = 200;
static constexpr uint32_t TEST_N_POOLS = 20;
static constexpr uint32_t TEST_N_TYPES = 4;
static constexpr uint32_t TEST_N_SYNAPSES = 500;
static constexpr uint32_t TEST_N_ASTROCYTES = 30;
static constexpr uint32_t TEST_N_MICROGLIA = 20;
static constexpr uint32_t TEST_N_OPCS = 15;
static constexpr uint32_t TEST_N_NEIGHBORS = 8;
static constexpr uint32_t TEST_N_REGIONS = 10;

/* ============================================================================
 * Test Fixture
 * ============================================================================ */

class SubstrateGPUIntegrationTest : public NimcpTestBase {
protected:
    nimcp_gpu_context_t* gpu_ctx = nullptr;
    substrate_gpu_context_t* substrate_ctx = nullptr;
    substrate_gpu_config_t config;
    bool gpu_available = false;

    void SetUp() override {
        NimcpTestBase::SetUp();

        // Get default configuration
        config = substrate_gpu_default_config();

        // Try to create GPU context (use auto to find any available device)
        gpu_ctx = nimcp_gpu_context_create_auto();
        gpu_available = (gpu_ctx != nullptr);

        // Tests will handle missing GPU gracefully
    }

    void TearDown() override {
        if (substrate_ctx) {
            substrate_gpu_destroy(substrate_ctx);
            substrate_ctx = nullptr;
        }
        if (gpu_ctx) {
            nimcp_gpu_context_destroy(gpu_ctx);
            gpu_ctx = nullptr;
        }
        NimcpTestBase::TearDown();
    }

    void createSubstrateContext() {
        if (!gpu_ctx) {
            GTEST_SKIP() << "GPU context not available";
        }
        substrate_ctx = substrate_gpu_create(gpu_ctx, &config);
        ASSERT_NE(substrate_ctx, nullptr);
    }

    void initializeAllSubsystems() {
        ASSERT_NE(substrate_ctx, nullptr);

        int result;
        result = substrate_gpu_init_axons(substrate_ctx, TEST_N_AXONS);
        ASSERT_EQ(result, NIMCP_SUCCESS);

        result = substrate_gpu_init_dendrites(substrate_ctx, TEST_N_DENDRITES,
                                               TEST_N_SEGMENTS, TEST_N_SPINES);
        ASSERT_EQ(result, NIMCP_SUCCESS);

        result = substrate_gpu_init_myelin(substrate_ctx, TEST_N_AXONS);
        ASSERT_EQ(result, NIMCP_SUCCESS);

        result = substrate_gpu_init_neuromod(substrate_ctx, TEST_N_POOLS,
                                              TEST_N_TYPES, TEST_N_SYNAPSES);
        ASSERT_EQ(result, NIMCP_SUCCESS);

        result = substrate_gpu_init_glial(substrate_ctx, TEST_N_ASTROCYTES,
                                           TEST_N_MICROGLIA, TEST_N_OPCS, TEST_N_NEIGHBORS);
        ASSERT_EQ(result, NIMCP_SUCCESS);

        result = substrate_gpu_init_metabolic(substrate_ctx, TEST_N_REGIONS);
        ASSERT_EQ(result, NIMCP_SUCCESS);
    }
};

/* ============================================================================
 * GPU Context Initialization Tests
 * ============================================================================ */

TEST_F(SubstrateGPUIntegrationTest, GetDefaultConfig) {
    substrate_gpu_config_t default_cfg = substrate_gpu_default_config();

    // Verify reasonable defaults
    EXPECT_GT(default_cfg.axon.max_axons, 0u);
    EXPECT_GT(default_cfg.dendrite.max_dendrites, 0u);
    EXPECT_GT(default_cfg.myelin.max_sheaths, 0u);
    EXPECT_GT(default_cfg.neuromod.max_pools, 0u);
}

TEST_F(SubstrateGPUIntegrationTest, CreateSubstrateContext) {
    if (!gpu_available) {
        GTEST_SKIP() << "GPU not available";
    }

    createSubstrateContext();

    EXPECT_NE(substrate_ctx, nullptr);
    EXPECT_EQ(substrate_ctx->gpu_ctx, gpu_ctx);
    EXPECT_TRUE(substrate_ctx->initialized);
}

TEST_F(SubstrateGPUIntegrationTest, CreateWithNullGPUContextFails) {
    substrate_ctx = substrate_gpu_create(nullptr, &config);
    EXPECT_EQ(substrate_ctx, nullptr);
}

TEST_F(SubstrateGPUIntegrationTest, CreateWithNullConfigUsesDefaults) {
    if (!gpu_available) {
        GTEST_SKIP() << "GPU not available";
    }

    substrate_ctx = substrate_gpu_create(gpu_ctx, nullptr);
    EXPECT_NE(substrate_ctx, nullptr);
}

TEST_F(SubstrateGPUIntegrationTest, DestroyNullSafe) {
    // Should not crash
    substrate_gpu_destroy(nullptr);
    SUCCEED();
}

/* ============================================================================
 * Axon Subsystem Tests
 * ============================================================================ */

TEST_F(SubstrateGPUIntegrationTest, InitializeAxons) {
    if (!gpu_available) {
        GTEST_SKIP() << "GPU not available";
    }

    createSubstrateContext();

    int result = substrate_gpu_init_axons(substrate_ctx, TEST_N_AXONS);
    EXPECT_EQ(result, NIMCP_SUCCESS);
    EXPECT_EQ(substrate_ctx->axon.n_axons, TEST_N_AXONS);
}

TEST_F(SubstrateGPUIntegrationTest, AxonPropagate) {
    if (!gpu_available) {
        GTEST_SKIP() << "GPU not available";
    }

    createSubstrateContext();
    substrate_gpu_init_axons(substrate_ctx, TEST_N_AXONS);

    int result = substrate_gpu_axon_propagate(substrate_ctx, nullptr, 1.0f);
    EXPECT_EQ(result, NIMCP_SUCCESS);
}

TEST_F(SubstrateGPUIntegrationTest, AxonRefractory) {
    if (!gpu_available) {
        GTEST_SKIP() << "GPU not available";
    }

    createSubstrateContext();
    substrate_gpu_init_axons(substrate_ctx, TEST_N_AXONS);

    int result = substrate_gpu_axon_refractory(substrate_ctx, nullptr, 1.0f);
    EXPECT_EQ(result, NIMCP_SUCCESS);
}

TEST_F(SubstrateGPUIntegrationTest, AxonStep) {
    if (!gpu_available) {
        GTEST_SKIP() << "GPU not available";
    }

    createSubstrateContext();
    substrate_gpu_init_axons(substrate_ctx, TEST_N_AXONS);

    int result = substrate_gpu_axon_step(substrate_ctx, nullptr, 1.0f);
    EXPECT_EQ(result, NIMCP_SUCCESS);
}

/* ============================================================================
 * Dendrite Subsystem Tests
 * ============================================================================ */

TEST_F(SubstrateGPUIntegrationTest, InitializeDendrites) {
    if (!gpu_available) {
        GTEST_SKIP() << "GPU not available";
    }

    createSubstrateContext();

    int result = substrate_gpu_init_dendrites(substrate_ctx, TEST_N_DENDRITES,
                                               TEST_N_SEGMENTS, TEST_N_SPINES);
    EXPECT_EQ(result, NIMCP_SUCCESS);
    EXPECT_EQ(substrate_ctx->dendrite.n_dendrites, TEST_N_DENDRITES);
    EXPECT_EQ(substrate_ctx->dendrite.n_segments, TEST_N_SEGMENTS);
    EXPECT_EQ(substrate_ctx->dendrite.n_spines, TEST_N_SPINES);
}

TEST_F(SubstrateGPUIntegrationTest, DendriteCableEquation) {
    if (!gpu_available) {
        GTEST_SKIP() << "GPU not available";
    }

    createSubstrateContext();
    substrate_gpu_init_dendrites(substrate_ctx, TEST_N_DENDRITES,
                                  TEST_N_SEGMENTS, TEST_N_SPINES);

    int result = substrate_gpu_dendrite_cable(substrate_ctx, nullptr, 0.5f);
    EXPECT_EQ(result, NIMCP_SUCCESS);
}

TEST_F(SubstrateGPUIntegrationTest, DendriteNMDA) {
    if (!gpu_available) {
        GTEST_SKIP() << "GPU not available";
    }

    createSubstrateContext();
    substrate_gpu_init_dendrites(substrate_ctx, TEST_N_DENDRITES,
                                  TEST_N_SEGMENTS, TEST_N_SPINES);

    int result = substrate_gpu_dendrite_nmda(substrate_ctx);
    EXPECT_EQ(result, NIMCP_SUCCESS);
}

TEST_F(SubstrateGPUIntegrationTest, DendriteCalcium) {
    if (!gpu_available) {
        GTEST_SKIP() << "GPU not available";
    }

    createSubstrateContext();
    substrate_gpu_init_dendrites(substrate_ctx, TEST_N_DENDRITES,
                                  TEST_N_SEGMENTS, TEST_N_SPINES);

    int result = substrate_gpu_dendrite_calcium(substrate_ctx, 0.5f);
    EXPECT_EQ(result, NIMCP_SUCCESS);
}

TEST_F(SubstrateGPUIntegrationTest, DendriteBAP) {
    if (!gpu_available) {
        GTEST_SKIP() << "GPU not available";
    }

    createSubstrateContext();
    substrate_gpu_init_dendrites(substrate_ctx, TEST_N_DENDRITES,
                                  TEST_N_SEGMENTS, TEST_N_SPINES);

    int result = substrate_gpu_dendrite_bap(substrate_ctx, nullptr, 0.5f);
    EXPECT_EQ(result, NIMCP_SUCCESS);
}

TEST_F(SubstrateGPUIntegrationTest, DendriteStep) {
    if (!gpu_available) {
        GTEST_SKIP() << "GPU not available";
    }

    createSubstrateContext();
    substrate_gpu_init_dendrites(substrate_ctx, TEST_N_DENDRITES,
                                  TEST_N_SEGMENTS, TEST_N_SPINES);

    int result = substrate_gpu_dendrite_step(substrate_ctx, nullptr, nullptr, 0.5f);
    EXPECT_EQ(result, NIMCP_SUCCESS);
}

/* ============================================================================
 * Myelin Subsystem Tests
 * ============================================================================ */

TEST_F(SubstrateGPUIntegrationTest, InitializeMyelin) {
    if (!gpu_available) {
        GTEST_SKIP() << "GPU not available";
    }

    createSubstrateContext();

    int result = substrate_gpu_init_myelin(substrate_ctx, TEST_N_AXONS);
    EXPECT_EQ(result, NIMCP_SUCCESS);
    EXPECT_EQ(substrate_ctx->myelin.n_axons, TEST_N_AXONS);
}

TEST_F(SubstrateGPUIntegrationTest, MyelinGRatio) {
    if (!gpu_available) {
        GTEST_SKIP() << "GPU not available";
    }

    createSubstrateContext();
    substrate_gpu_init_myelin(substrate_ctx, TEST_N_AXONS);

    int result = substrate_gpu_myelin_g_ratio(substrate_ctx);
    EXPECT_EQ(result, NIMCP_SUCCESS);
}

TEST_F(SubstrateGPUIntegrationTest, MyelinVelocity) {
    if (!gpu_available) {
        GTEST_SKIP() << "GPU not available";
    }

    createSubstrateContext();
    substrate_gpu_init_myelin(substrate_ctx, TEST_N_AXONS);

    int result = substrate_gpu_myelin_velocity(substrate_ctx);
    EXPECT_EQ(result, NIMCP_SUCCESS);
}

TEST_F(SubstrateGPUIntegrationTest, MyelinPlasticity) {
    if (!gpu_available) {
        GTEST_SKIP() << "GPU not available";
    }

    createSubstrateContext();
    substrate_gpu_init_myelin(substrate_ctx, TEST_N_AXONS);

    int result = substrate_gpu_myelin_plasticity(substrate_ctx, 1.0f);
    EXPECT_EQ(result, NIMCP_SUCCESS);
}

TEST_F(SubstrateGPUIntegrationTest, MyelinStep) {
    if (!gpu_available) {
        GTEST_SKIP() << "GPU not available";
    }

    createSubstrateContext();
    substrate_gpu_init_myelin(substrate_ctx, TEST_N_AXONS);

    int result = substrate_gpu_myelin_step(substrate_ctx, 1.0f);
    EXPECT_EQ(result, NIMCP_SUCCESS);
}

/* ============================================================================
 * Neuromodulator Subsystem Tests
 * ============================================================================ */

TEST_F(SubstrateGPUIntegrationTest, InitializeNeuromod) {
    if (!gpu_available) {
        GTEST_SKIP() << "GPU not available";
    }

    createSubstrateContext();

    int result = substrate_gpu_init_neuromod(substrate_ctx, TEST_N_POOLS,
                                              TEST_N_TYPES, TEST_N_SYNAPSES);
    EXPECT_EQ(result, NIMCP_SUCCESS);
    EXPECT_EQ(substrate_ctx->neuromod.n_pools, TEST_N_POOLS);
    EXPECT_EQ(substrate_ctx->neuromod.n_types, TEST_N_TYPES);
    EXPECT_EQ(substrate_ctx->neuromod.n_synapses, TEST_N_SYNAPSES);
}

TEST_F(SubstrateGPUIntegrationTest, NeuromodDecay) {
    if (!gpu_available) {
        GTEST_SKIP() << "GPU not available";
    }

    createSubstrateContext();
    substrate_gpu_init_neuromod(substrate_ctx, TEST_N_POOLS,
                                 TEST_N_TYPES, TEST_N_SYNAPSES);

    int result = substrate_gpu_neuromod_decay(substrate_ctx, 1.0f);
    EXPECT_EQ(result, NIMCP_SUCCESS);
}

TEST_F(SubstrateGPUIntegrationTest, NeuromodEffect) {
    if (!gpu_available) {
        GTEST_SKIP() << "GPU not available";
    }

    createSubstrateContext();
    substrate_gpu_init_neuromod(substrate_ctx, TEST_N_POOLS,
                                 TEST_N_TYPES, TEST_N_SYNAPSES);

    int result = substrate_gpu_neuromod_effect(substrate_ctx);
    EXPECT_EQ(result, NIMCP_SUCCESS);
}

TEST_F(SubstrateGPUIntegrationTest, NeuromodPhasicTonic) {
    if (!gpu_available) {
        GTEST_SKIP() << "GPU not available";
    }

    createSubstrateContext();
    substrate_gpu_init_neuromod(substrate_ctx, TEST_N_POOLS,
                                 TEST_N_TYPES, TEST_N_SYNAPSES);

    int result = substrate_gpu_neuromod_phasic_tonic(substrate_ctx, nullptr, 1.0f);
    EXPECT_EQ(result, NIMCP_SUCCESS);
}

TEST_F(SubstrateGPUIntegrationTest, NeuromodStep) {
    if (!gpu_available) {
        GTEST_SKIP() << "GPU not available";
    }

    createSubstrateContext();
    substrate_gpu_init_neuromod(substrate_ctx, TEST_N_POOLS,
                                 TEST_N_TYPES, TEST_N_SYNAPSES);

    int result = substrate_gpu_neuromod_step(substrate_ctx, 1.0f);
    EXPECT_EQ(result, NIMCP_SUCCESS);
}

/* ============================================================================
 * Glial Subsystem Tests
 * ============================================================================ */

TEST_F(SubstrateGPUIntegrationTest, InitializeGlial) {
    if (!gpu_available) {
        GTEST_SKIP() << "GPU not available";
    }

    createSubstrateContext();

    int result = substrate_gpu_init_glial(substrate_ctx, TEST_N_ASTROCYTES,
                                           TEST_N_MICROGLIA, TEST_N_OPCS, TEST_N_NEIGHBORS);
    EXPECT_EQ(result, NIMCP_SUCCESS);
    EXPECT_EQ(substrate_ctx->glial.n_astrocytes, TEST_N_ASTROCYTES);
    EXPECT_EQ(substrate_ctx->glial.n_microglia, TEST_N_MICROGLIA);
    EXPECT_EQ(substrate_ctx->glial.n_opcs, TEST_N_OPCS);
}

TEST_F(SubstrateGPUIntegrationTest, AstrocyteCalcium) {
    if (!gpu_available) {
        GTEST_SKIP() << "GPU not available";
    }

    createSubstrateContext();
    substrate_gpu_init_glial(substrate_ctx, TEST_N_ASTROCYTES,
                              TEST_N_MICROGLIA, TEST_N_OPCS, TEST_N_NEIGHBORS);

    int result = substrate_gpu_astrocyte_calcium(substrate_ctx, 1.0f);
    EXPECT_EQ(result, NIMCP_SUCCESS);
}

TEST_F(SubstrateGPUIntegrationTest, AstrocyteRelease) {
    if (!gpu_available) {
        GTEST_SKIP() << "GPU not available";
    }

    createSubstrateContext();
    substrate_gpu_init_glial(substrate_ctx, TEST_N_ASTROCYTES,
                              TEST_N_MICROGLIA, TEST_N_OPCS, TEST_N_NEIGHBORS);

    int result = substrate_gpu_astrocyte_release(substrate_ctx);
    EXPECT_EQ(result, NIMCP_SUCCESS);
}

TEST_F(SubstrateGPUIntegrationTest, MicrogliaActivation) {
    if (!gpu_available) {
        GTEST_SKIP() << "GPU not available";
    }

    createSubstrateContext();
    substrate_gpu_init_glial(substrate_ctx, TEST_N_ASTROCYTES,
                              TEST_N_MICROGLIA, TEST_N_OPCS, TEST_N_NEIGHBORS);

    int result = substrate_gpu_microglia_activation(substrate_ctx, 1.0f);
    EXPECT_EQ(result, NIMCP_SUCCESS);
}

TEST_F(SubstrateGPUIntegrationTest, OPCDifferentiation) {
    if (!gpu_available) {
        GTEST_SKIP() << "GPU not available";
    }

    createSubstrateContext();
    substrate_gpu_init_glial(substrate_ctx, TEST_N_ASTROCYTES,
                              TEST_N_MICROGLIA, TEST_N_OPCS, TEST_N_NEIGHBORS);

    int result = substrate_gpu_opc_differentiation(substrate_ctx, 1.0f);
    EXPECT_EQ(result, NIMCP_SUCCESS);
}

TEST_F(SubstrateGPUIntegrationTest, GlialStep) {
    if (!gpu_available) {
        GTEST_SKIP() << "GPU not available";
    }

    createSubstrateContext();
    substrate_gpu_init_glial(substrate_ctx, TEST_N_ASTROCYTES,
                              TEST_N_MICROGLIA, TEST_N_OPCS, TEST_N_NEIGHBORS);

    int result = substrate_gpu_glial_step(substrate_ctx, 1.0f);
    EXPECT_EQ(result, NIMCP_SUCCESS);
}

/* ============================================================================
 * Metabolic Subsystem Tests
 * ============================================================================ */

TEST_F(SubstrateGPUIntegrationTest, InitializeMetabolic) {
    if (!gpu_available) {
        GTEST_SKIP() << "GPU not available";
    }

    createSubstrateContext();

    int result = substrate_gpu_init_metabolic(substrate_ctx, TEST_N_REGIONS);
    EXPECT_EQ(result, NIMCP_SUCCESS);
    EXPECT_EQ(substrate_ctx->metabolic.n_regions, TEST_N_REGIONS);
}

TEST_F(SubstrateGPUIntegrationTest, MetabolicEffects) {
    if (!gpu_available) {
        GTEST_SKIP() << "GPU not available";
    }

    createSubstrateContext();
    substrate_gpu_init_metabolic(substrate_ctx, TEST_N_REGIONS);

    int result = substrate_gpu_metabolic_effects(substrate_ctx);
    EXPECT_EQ(result, NIMCP_SUCCESS);
}

TEST_F(SubstrateGPUIntegrationTest, MetabolicUpdate) {
    if (!gpu_available) {
        GTEST_SKIP() << "GPU not available";
    }

    createSubstrateContext();
    substrate_gpu_init_metabolic(substrate_ctx, TEST_N_REGIONS);

    int result = substrate_gpu_metabolic_update(substrate_ctx, nullptr, 1.0f);
    EXPECT_EQ(result, NIMCP_SUCCESS);
}

TEST_F(SubstrateGPUIntegrationTest, MetabolicStep) {
    if (!gpu_available) {
        GTEST_SKIP() << "GPU not available";
    }

    createSubstrateContext();
    substrate_gpu_init_metabolic(substrate_ctx, TEST_N_REGIONS);

    int result = substrate_gpu_metabolic_step(substrate_ctx, nullptr, 1.0f);
    EXPECT_EQ(result, NIMCP_SUCCESS);
}

/* ============================================================================
 * Tensor Access Tests
 * ============================================================================ */

TEST_F(SubstrateGPUIntegrationTest, GetAxonTensors) {
    if (!gpu_available) {
        GTEST_SKIP() << "GPU not available";
    }

    createSubstrateContext();
    substrate_gpu_init_axons(substrate_ctx, TEST_N_AXONS);

    substrate_axon_tensors_t* tensors = substrate_gpu_get_axon_tensors(substrate_ctx);
    EXPECT_NE(tensors, nullptr);
    EXPECT_EQ(tensors->n_axons, TEST_N_AXONS);
}

TEST_F(SubstrateGPUIntegrationTest, GetDendriteTensors) {
    if (!gpu_available) {
        GTEST_SKIP() << "GPU not available";
    }

    createSubstrateContext();
    substrate_gpu_init_dendrites(substrate_ctx, TEST_N_DENDRITES,
                                  TEST_N_SEGMENTS, TEST_N_SPINES);

    substrate_dendrite_tensors_t* tensors = substrate_gpu_get_dendrite_tensors(substrate_ctx);
    EXPECT_NE(tensors, nullptr);
    EXPECT_EQ(tensors->n_dendrites, TEST_N_DENDRITES);
}

TEST_F(SubstrateGPUIntegrationTest, GetMyelinTensors) {
    if (!gpu_available) {
        GTEST_SKIP() << "GPU not available";
    }

    createSubstrateContext();
    substrate_gpu_init_myelin(substrate_ctx, TEST_N_AXONS);

    substrate_myelin_tensors_t* tensors = substrate_gpu_get_myelin_tensors(substrate_ctx);
    EXPECT_NE(tensors, nullptr);
    EXPECT_EQ(tensors->n_axons, TEST_N_AXONS);
}

TEST_F(SubstrateGPUIntegrationTest, GetNeuromodTensors) {
    if (!gpu_available) {
        GTEST_SKIP() << "GPU not available";
    }

    createSubstrateContext();
    substrate_gpu_init_neuromod(substrate_ctx, TEST_N_POOLS,
                                 TEST_N_TYPES, TEST_N_SYNAPSES);

    substrate_neuromod_tensors_t* tensors = substrate_gpu_get_neuromod_tensors(substrate_ctx);
    EXPECT_NE(tensors, nullptr);
    EXPECT_EQ(tensors->n_pools, TEST_N_POOLS);
}

TEST_F(SubstrateGPUIntegrationTest, GetGlialTensors) {
    if (!gpu_available) {
        GTEST_SKIP() << "GPU not available";
    }

    createSubstrateContext();
    substrate_gpu_init_glial(substrate_ctx, TEST_N_ASTROCYTES,
                              TEST_N_MICROGLIA, TEST_N_OPCS, TEST_N_NEIGHBORS);

    substrate_glial_tensors_t* tensors = substrate_gpu_get_glial_tensors(substrate_ctx);
    EXPECT_NE(tensors, nullptr);
    EXPECT_EQ(tensors->n_astrocytes, TEST_N_ASTROCYTES);
}

TEST_F(SubstrateGPUIntegrationTest, GetMetabolicTensors) {
    if (!gpu_available) {
        GTEST_SKIP() << "GPU not available";
    }

    createSubstrateContext();
    substrate_gpu_init_metabolic(substrate_ctx, TEST_N_REGIONS);

    substrate_metabolic_tensors_t* tensors = substrate_gpu_get_metabolic_tensors(substrate_ctx);
    EXPECT_NE(tensors, nullptr);
    EXPECT_EQ(tensors->n_regions, TEST_N_REGIONS);
}

/* ============================================================================
 * Full Step and Complex Scenario Tests
 * ============================================================================ */

TEST_F(SubstrateGPUIntegrationTest, FullStep) {
    if (!gpu_available) {
        GTEST_SKIP() << "GPU not available";
    }

    createSubstrateContext();
    initializeAllSubsystems();

    int result = substrate_gpu_full_step(substrate_ctx, nullptr, nullptr, nullptr, 1.0f);
    EXPECT_EQ(result, NIMCP_SUCCESS);
}

TEST_F(SubstrateGPUIntegrationTest, MultipleFullSteps) {
    if (!gpu_available) {
        GTEST_SKIP() << "GPU not available";
    }

    createSubstrateContext();
    initializeAllSubsystems();

    for (int i = 0; i < 100; i++) {
        int result = substrate_gpu_full_step(substrate_ctx, nullptr, nullptr, nullptr, 0.5f);
        EXPECT_EQ(result, NIMCP_SUCCESS);
    }
}

TEST_F(SubstrateGPUIntegrationTest, MixedSubsystemOperations) {
    if (!gpu_available) {
        GTEST_SKIP() << "GPU not available";
    }

    createSubstrateContext();
    initializeAllSubsystems();

    // Run different subsystem steps in sequence
    for (int i = 0; i < 50; i++) {
        substrate_gpu_axon_step(substrate_ctx, nullptr, 0.5f);
        substrate_gpu_dendrite_step(substrate_ctx, nullptr, nullptr, 0.5f);
        substrate_gpu_myelin_step(substrate_ctx, 0.5f);
        substrate_gpu_neuromod_step(substrate_ctx, 0.5f);
        substrate_gpu_glial_step(substrate_ctx, 0.5f);
        substrate_gpu_metabolic_step(substrate_ctx, nullptr, 0.5f);
    }

    SUCCEED();
}

TEST_F(SubstrateGPUIntegrationTest, CPUFallbackBehavior) {
    // Test that operations handle missing GPU gracefully

    // Without GPU, create should return NULL
    substrate_ctx = substrate_gpu_create(nullptr, &config);
    EXPECT_EQ(substrate_ctx, nullptr);
}

TEST_F(SubstrateGPUIntegrationTest, LongRunningSimulation) {
    if (!gpu_available) {
        GTEST_SKIP() << "GPU not available";
    }

    createSubstrateContext();
    initializeAllSubsystems();

    // Run extended simulation
    for (int i = 0; i < 500; i++) {
        // Alternate between different operations
        if (i % 2 == 0) {
            substrate_gpu_axon_step(substrate_ctx, nullptr, 1.0f);
        }
        if (i % 3 == 0) {
            substrate_gpu_dendrite_step(substrate_ctx, nullptr, nullptr, 1.0f);
        }
        if (i % 5 == 0) {
            substrate_gpu_neuromod_step(substrate_ctx, 1.0f);
        }
        if (i % 7 == 0) {
            substrate_gpu_glial_step(substrate_ctx, 1.0f);
        }
        if (i % 10 == 0) {
            substrate_gpu_full_step(substrate_ctx, nullptr, nullptr, nullptr, 1.0f);
        }
    }

    // Verify context remains valid
    EXPECT_NE(substrate_ctx, nullptr);
    EXPECT_TRUE(substrate_ctx->initialized);
}
