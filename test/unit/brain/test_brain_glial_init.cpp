/**
 * @file test_brain_glial_init.cpp
 * @brief Unit tests for G1/G2 glial wiring — verify brain init creates and
 *        attaches astrocyte/oligodendrocyte/microglia networks.
 *
 * Covers:
 *   - enable_glial=true populates brain->{astrocyte,oligodendrocyte,microglia}_network
 *   - glial_integration picks up the three networks (not left NULL)
 *   - accessors return the expected pointers
 *   - nimcp_brain_attach_glial() is idempotent and safe on re-invocation
 *   - enable_glial=false leaves glial + networks at NULL
 *   - brain_destroy tears down cleanly with no double-free
 *
 * Companion: integration/glial/test_glial_hot_path.cpp (hot-path modulation)
 */

#include <gtest/gtest.h>
#include "core/brain/nimcp_brain.h"
#include "core/brain/nimcp_brain_internal.h"
#include "core/brain/factory/init/nimcp_brain_init_subsystems.h"
#include "glial/integration/nimcp_glial_integration.h"
#include "glial/astrocytes/nimcp_astrocytes.h"
#include "glial/oligodendrocytes/nimcp_oligodendrocytes.h"
#include "glial/microglia/nimcp_microglia.h"

// brain_create and brain_destroy are declared in nimcp_brain.h (included
// transitively). brain_size_t and brain_task_t enums are also there.

extern "C" {
    neural_network_t adaptive_network_get_base_network(adaptive_network_t);
    void* neural_network_get_glial_integration(neural_network_t);
}

static brain_t make_brain_with_glial(bool enable_glial, uint32_t inputs = 8,
                                     uint32_t outputs = 4) {
    brain_t brain = brain_create("glial_init_test", BRAIN_SIZE_TINY,
                                 BRAIN_TASK_CLASSIFICATION,
                                 inputs, outputs);
    if (!brain) return nullptr;
    brain->config.enable_glial = enable_glial;
    // Use tiny default counts so the test runs fast
    brain->config.num_astrocytes      = 16;
    brain->config.num_oligodendrocytes = 8;
    brain->config.num_microglia       = 8;
    // Re-run glial init after overriding config (brain_create may have
    // set enable_glial to the opposite from brain_create defaults).
    nimcp_brain_factory_init_glial_subsystem(brain);
    return brain;
}

class BrainGlialInitTest : public ::testing::Test {
protected:
    brain_t brain = nullptr;

    void TearDown() override {
        if (brain) {
            brain_destroy(brain);
            brain = nullptr;
        }
    }
};

TEST_F(BrainGlialInitTest, EnableGlialTrueCreatesAllThreeNetworks) {
    brain = make_brain_with_glial(true);
    ASSERT_NE(brain, nullptr);

    EXPECT_TRUE(nimcp_brain_glial_is_enabled(brain));
    EXPECT_NE(nimcp_brain_get_glial(brain), nullptr);
    EXPECT_NE(nimcp_brain_get_astrocyte_network(brain), nullptr);
    EXPECT_NE(nimcp_brain_get_oligodendrocyte_network(brain), nullptr);
    EXPECT_NE(nimcp_brain_get_microglia_network(brain), nullptr);
}

TEST_F(BrainGlialInitTest, IntegrationReceivesBorrowedPointers) {
    brain = make_brain_with_glial(true);
    ASSERT_NE(brain, nullptr);

    glial_integration_t* gi = (glial_integration_t*)nimcp_brain_get_glial(brain);
    ASSERT_NE(gi, nullptr);

    // The integration should have borrowed pointers to the three networks.
    EXPECT_EQ(gi->astrocyte_network,
              nimcp_brain_get_astrocyte_network(brain));
    EXPECT_EQ(gi->oligodendrocyte_network,
              nimcp_brain_get_oligodendrocyte_network(brain));
    EXPECT_EQ(gi->microglia_network,
              nimcp_brain_get_microglia_network(brain));
}

TEST_F(BrainGlialInitTest, ModulationEnabledByDefault) {
    brain = make_brain_with_glial(true);
    ASSERT_NE(brain, nullptr);

    glial_integration_t* gi = (glial_integration_t*)nimcp_brain_get_glial(brain);
    ASSERT_NE(gi, nullptr);

    // G1 init flips astrocyte_modulation + oligo_myelination on; leaves
    // microglia_pruning off until config explicitly opts in.
    EXPECT_TRUE(gi->enable_astrocyte_modulation);
    EXPECT_TRUE(gi->enable_oligodendrocyte_myelination);
    EXPECT_FALSE(gi->enable_microglia_pruning);
}

TEST_F(BrainGlialInitTest, MicrogliaPruningFlagHonored) {
    brain = brain_create("glial_prune_test", BRAIN_SIZE_TINY,
                         BRAIN_TASK_CLASSIFICATION, 8, 4);
    ASSERT_NE(brain, nullptr);
    brain->config.enable_glial = true;
    brain->config.enable_microglia_pruning = true;
    brain->config.num_astrocytes = 8;
    brain->config.num_oligodendrocytes = 4;
    brain->config.num_microglia = 4;
    nimcp_brain_factory_init_glial_subsystem(brain);

    glial_integration_t* gi = (glial_integration_t*)nimcp_brain_get_glial(brain);
    ASSERT_NE(gi, nullptr);
    // Attach_glial should flip the flag on per config.
    EXPECT_TRUE(gi->enable_microglia_pruning);
}

TEST_F(BrainGlialInitTest, EnableGlialFalseLeavesNetworksNull) {
    brain = make_brain_with_glial(false);
    ASSERT_NE(brain, nullptr);
    EXPECT_FALSE(nimcp_brain_glial_is_enabled(brain));
    // brain->glial may or may not exist from other init paths, but the
    // three networks must be NULL when enable_glial=false.
    EXPECT_EQ(nimcp_brain_get_astrocyte_network(brain), nullptr);
    EXPECT_EQ(nimcp_brain_get_oligodendrocyte_network(brain), nullptr);
    EXPECT_EQ(nimcp_brain_get_microglia_network(brain), nullptr);
}

TEST_F(BrainGlialInitTest, AttachGlialIsIdempotent) {
    brain = make_brain_with_glial(true);
    ASSERT_NE(brain, nullptr);

    void* gi_before = nimcp_brain_get_glial(brain);
    void* astro_before = nimcp_brain_get_astrocyte_network(brain);
    void* oligo_before = nimcp_brain_get_oligodendrocyte_network(brain);
    void* micro_before = nimcp_brain_get_microglia_network(brain);

    // Call attach multiple times; networks must be stable.
    nimcp_brain_attach_glial(brain);
    nimcp_brain_attach_glial(brain);
    nimcp_brain_attach_glial(brain);

    EXPECT_EQ(gi_before,    nimcp_brain_get_glial(brain));
    EXPECT_EQ(astro_before, nimcp_brain_get_astrocyte_network(brain));
    EXPECT_EQ(oligo_before, nimcp_brain_get_oligodendrocyte_network(brain));
    EXPECT_EQ(micro_before, nimcp_brain_get_microglia_network(brain));
}

TEST_F(BrainGlialInitTest, DestroyClearsAllFields) {
    brain = make_brain_with_glial(true);
    ASSERT_NE(brain, nullptr);
    ASSERT_NE(nimcp_brain_get_glial(brain), nullptr);

    nimcp_brain_factory_destroy_glial_subsystem(brain);

    EXPECT_EQ(nimcp_brain_get_glial(brain),                nullptr);
    EXPECT_EQ(nimcp_brain_get_astrocyte_network(brain),    nullptr);
    EXPECT_EQ(nimcp_brain_get_oligodendrocyte_network(brain), nullptr);
    EXPECT_EQ(nimcp_brain_get_microglia_network(brain),    nullptr);

    // Second call must be a no-op, not a double-free.
    nimcp_brain_factory_destroy_glial_subsystem(brain);
    nimcp_brain_factory_destroy_glial_subsystem(nullptr);  // NULL-tolerant
}

// C1 REGRESSION TEST: brain's glial pointer must be published to the base
// neural network. Without this, the hot-path G3/G4/G5 code in
// compute_input_for_neuron checks network->glial_integration and finds NULL,
// making the entire glial-wiring campaign a no-op. Guards against a repeat
// of the "statue" bug from the substrate/thalamic F6 campaign.
TEST_F(BrainGlialInitTest, NeuralNetworkReceivesGlialPointer) {
    brain = make_brain_with_glial(true);
    ASSERT_NE(brain, nullptr);

    neural_network_t base = adaptive_network_get_base_network(brain->network);
    ASSERT_NE(base, nullptr);

    void* gi_on_network = neural_network_get_glial_integration(base);
    EXPECT_NE(gi_on_network, nullptr);
    EXPECT_EQ(gi_on_network, nimcp_brain_get_glial(brain));
}

TEST_F(BrainGlialInitTest, NullBrainSafeForAllAccessors) {
    // Accessors must tolerate NULL.
    EXPECT_EQ(nimcp_brain_get_glial(nullptr),                nullptr);
    EXPECT_EQ(nimcp_brain_get_astrocyte_network(nullptr),    nullptr);
    EXPECT_EQ(nimcp_brain_get_oligodendrocyte_network(nullptr), nullptr);
    EXPECT_EQ(nimcp_brain_get_microglia_network(nullptr),    nullptr);
    EXPECT_FALSE(nimcp_brain_glial_is_enabled(nullptr));
    // Attach + destroy must not crash on NULL.
    nimcp_brain_attach_glial(nullptr);
    nimcp_brain_factory_destroy_glial_subsystem(nullptr);
}
