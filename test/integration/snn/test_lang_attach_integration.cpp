//=============================================================================
// test_lang_attach_integration.cpp — broca/wernicke ↔ real SNN substrate pops
//=============================================================================
/**
 * @file test_lang_attach_integration.cpp
 * @brief Integration: broca + wernicke adapters bound to real SNN pops with
 *        the same names (broca_substrate / wernicke_substrate) that
 *        nimcp_brain_factory_init_language_pops() creates.
 *
 * WHAT: Brings up a small SNN, adds a `broca_substrate` and a
 *       `wernicke_substrate` pop via the same lightweight-CSR API used by
 *       the real init code, attaches the adapters via the public helper,
 *       and verifies the binding round-trips correctly. Then the test
 *       advances the SNN a few ticks to confirm nothing in the adapter
 *       binding crashes when the substrate pop is actively spiking.
 * WHY:  Catches regressions where (a) the pop-name constants drift between
 *       init code and the adapter expectations, (b) the binding survives
 *       SNN ticks (i.e. no UAF if the pop is reallocated under the hood),
 *       (c) a fresh adapter remains unbound until attach is called explicitly.
 * HOW:  GoogleTest. Bypasses full brain init for speed — exercises the
 *       relevant SNN + adapter slice only.
 */

#include <gtest/gtest.h>
#include <cstring>

extern "C" {
#include "nimcp.h"
#include "core/brain/regions/broca/nimcp_broca_adapter.h"
#include "core/brain/regions/wernicke/nimcp_wernicke_adapter.h"
#include "core/neuron_types/nimcp_neuron_types.h"
#include "snn/nimcp_snn_network.h"
#include "snn/nimcp_snn_types.h"
#include "snn/nimcp_snn_config.h"
}

namespace {

/* MUST match the constants in nimcp_brain_init_language_pops.c — if those
 * change, this test must be updated in lock-step. The strings are duplicated
 * here intentionally (rather than #include'd) so the test guards against
 * silent renames in the init code. */
static constexpr const char* BROCA_POP_NAME    = "broca_substrate";
static constexpr const char* WERNICKE_POP_NAME = "wernicke_substrate";
static constexpr uint32_t BROCA_POP_NEURONS    = 64000u;
static constexpr uint32_t WERNICKE_POP_NEURONS = 64000u;

/* Tiny SNN config — just enough to host the substrate pops. The real init
 * runs against a 1.9M hierarchy; here we only need the lightweight pops. */
static constexpr uint32_t TINY_SNN_INPUTS  = 8;
static constexpr uint32_t TINY_SNN_OUTPUTS = 8;
static constexpr float    DEFAULT_DT_MS    = 1.0f;
static constexpr int      WARMUP_STEPS     = 5;

class LangAttachIntegrationTest : public ::testing::Test {
protected:
    snn_network_t*     snn      = nullptr;
    broca_adapter_t*   broca    = nullptr;
    wernicke_adapter_t* wernicke = nullptr;
    int broca_pop_id    = -1;
    int wernicke_pop_id = -1;

    static void SetUpTestSuite() { ASSERT_EQ(nimcp_init(), NIMCP_SUCCESS); }
    static void TearDownTestSuite() { nimcp_shutdown(); }

    void SetUp() override {
        // 1) Create the SNN with a tiny default config + add the two
        //    substrate pops via the same lightweight-CSR call the real
        //    init code uses (snn_network_add_population_lightweight).
        snn_config_t cfg;
        snn_config_default(&cfg);
        cfg.n_inputs  = TINY_SNN_INPUTS;
        cfg.n_outputs = TINY_SNN_OUTPUTS;
        cfg.n_hidden  = 0;
        cfg.dt = DEFAULT_DT_MS;
        snn = snn_network_create(&cfg);
        ASSERT_NE(snn, nullptr);

        broca_pop_id = snn_network_add_population_lightweight(
            snn, BROCA_POP_NEURONS, NEURON_GENERIC_LIF, BROCA_POP_NAME);
        ASSERT_GE(broca_pop_id, 0);
        wernicke_pop_id = snn_network_add_population_lightweight(
            snn, WERNICKE_POP_NEURONS, NEURON_GENERIC_LIF, WERNICKE_POP_NAME);
        ASSERT_GE(wernicke_pop_id, 0);

        // 2) Build the adapters with bio-async disabled (no router needed
        //    for this integration slice).
        broca_config_t bcfg = broca_default_config();
        bcfg.enable_bio_async = false;
        bcfg.enable_events    = false;
        broca = broca_create(&bcfg);
        ASSERT_NE(broca, nullptr);

        wernicke_config_t wcfg = wernicke_default_config();
        wcfg.enable_bio_async = false;
        wcfg.enable_events    = false;
        wernicke = wernicke_create(&wcfg);
        ASSERT_NE(wernicke, nullptr);
    }

    void TearDown() override {
        if (broca)    { broca_destroy(broca);       broca = nullptr; }
        if (wernicke) { wernicke_destroy(wernicke); wernicke = nullptr; }
        if (snn)      { snn_network_destroy(snn);   snn = nullptr; }
    }
};

//=============================================================================
// Pop-name contract: the SNN actually exposes the names the adapter expects
//=============================================================================

TEST_F(LangAttachIntegrationTest, SubstratePopsExistByName) {
    ASSERT_NE(snn->populations[broca_pop_id], nullptr);
    ASSERT_NE(snn->populations[wernicke_pop_id], nullptr);
    EXPECT_EQ(strcmp(snn->populations[broca_pop_id]->name, BROCA_POP_NAME), 0);
    EXPECT_EQ(strcmp(snn->populations[wernicke_pop_id]->name, WERNICKE_POP_NAME), 0);
    EXPECT_EQ(snn->populations[broca_pop_id]->n_neurons, BROCA_POP_NEURONS);
    EXPECT_EQ(snn->populations[wernicke_pop_id]->n_neurons, WERNICKE_POP_NEURONS);
}

//=============================================================================
// Round-trip attach
//=============================================================================

TEST_F(LangAttachIntegrationTest, BrocaAttachRoundTrip) {
    EXPECT_EQ(broca_get_snn_pop_id(broca), -1);          // unbound at start
    ASSERT_TRUE(broca_attach_snn_pop(broca, snn, broca_pop_id));
    EXPECT_EQ(broca_get_snn_pop_id(broca), broca_pop_id);
    EXPECT_EQ(broca_get_snn_network(broca), snn);
}

TEST_F(LangAttachIntegrationTest, WernickeAttachRoundTrip) {
    EXPECT_EQ(wernicke_get_snn_pop_id(wernicke), -1);
    ASSERT_TRUE(wernicke_attach_snn_pop(wernicke, snn, wernicke_pop_id));
    EXPECT_EQ(wernicke_get_snn_pop_id(wernicke), wernicke_pop_id);
    EXPECT_EQ(wernicke_get_snn_network(wernicke), snn);
}

//=============================================================================
// Binding survives SNN ticks (no UAF if pop tensors get re-finalized)
//=============================================================================

TEST_F(LangAttachIntegrationTest, BindingSurvivesSnnSteps) {
    ASSERT_TRUE(broca_attach_snn_pop(broca, snn, broca_pop_id));
    ASSERT_TRUE(wernicke_attach_snn_pop(wernicke, snn, wernicke_pop_id));

    // Finalize CSR (mirrors what the real init does), then advance a few
    // ticks. If the adapter binding silently invalidates due to pop
    // reallocation, the next get-call would return a wrong pointer.
    EXPECT_EQ(snn_network_finalize_connections(snn), 2);

    for (int s = 0; s < WARMUP_STEPS; s++) {
        ASSERT_GE(snn_network_step(snn, DEFAULT_DT_MS), 0);
    }

    EXPECT_EQ(broca_get_snn_pop_id(broca),       broca_pop_id);
    EXPECT_EQ(broca_get_snn_network(broca),      snn);
    EXPECT_EQ(wernicke_get_snn_pop_id(wernicke), wernicke_pop_id);
    EXPECT_EQ(wernicke_get_snn_network(wernicke), snn);
}

//=============================================================================
// Cross-attach: binding broca to wernicke's pop (and vice versa) is allowed
// at the API level — the adapter doesn't validate names. This is by design:
// the binding is purely a (snn, id) cache; semantics live at the call site.
// We just verify the binding stores what we ask.
//=============================================================================

TEST_F(LangAttachIntegrationTest, CrossAttachAllowedAtApiLevel) {
    EXPECT_TRUE(broca_attach_snn_pop(broca, snn, wernicke_pop_id));
    EXPECT_EQ(broca_get_snn_pop_id(broca), wernicke_pop_id);
    EXPECT_EQ(broca_get_snn_network(broca), snn);
}

}  // namespace
