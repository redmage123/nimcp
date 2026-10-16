//=============================================================================
// test_layer_subtypes_regression.cpp — Regression for P4.1 layer-specific
// pyramidal subtypes (L23 / L4_STELLATE / L5_BETZ).
//=============================================================================
/**
 * @file test_layer_subtypes_regression.cpp
 * @brief Regression: switching a population's subclass back to PYRAMIDAL
 *        after first tagging it with a layer-specific subtype must restore
 *        network-default LIF parameters bit-for-bit.
 *
 * WHAT: Pins the round-trip property — tagging a pop as PYRAMIDAL_L23 (or
 *       L4_STELLATE / L5_BETZ) and then re-tagging it back to PYRAMIDAL
 *       must yield snn_pop_lif_params() exactly equal to the cfg defaults.
 *       Also pins that the round trip works through snn_network_set_pop_subclass()
 *       on a real population, not only via direct field manipulation.
 *
 * WHY:  When the P4.1 cases were added, the default arm of the switch was
 *       not modified. A future refactor that, say, caches "last applied
 *       deltas" on the population struct could leave stale tau_mem behind
 *       on a re-tag. This regression catches that — the only path back to
 *       network defaults is a clean fall-through to the default arm.
 *
 * HOW:  Two tests:
 *         1. Pure-function: stack-allocate a pop with subclass set to one of
 *            the new layer subtypes, observe the deltas, then flip subclass
 *            back to PYRAMIDAL and assert defaults are restored.
 *         2. Through the public API: build a tiny SNN, add a pop, tag it
 *            L5_BETZ via snn_network_set_pop_subclass(), then re-tag to
 *            PYRAMIDAL and confirm snn_pop_lif_params() returns network
 *            defaults.
 */

#include <gtest/gtest.h>
#include <cstring>
#include <cmath>

extern "C" {
#include "snn/nimcp_snn_types.h"
#include "snn/nimcp_snn_network.h"
#include "snn/nimcp_snn_config.h"
}

namespace {

constexpr float kTol = 1e-6f;

static snn_config_t make_default_config()
{
    snn_config_t cfg;
    std::memset(&cfg, 0, sizeof(cfg));
    EXPECT_EQ(snn_config_default(&cfg), 0);
    return cfg;
}

static void expect_equals_defaults(const snn_lif_params_t& p, const snn_config_t& cfg)
{
    EXPECT_NEAR(p.v_thresh, cfg.v_thresh, kTol);
    EXPECT_NEAR(p.v_reset,  cfg.v_reset,  kTol);
    EXPECT_NEAR(p.v_rest,   cfg.v_rest,   kTol);
    EXPECT_NEAR(p.tau_mem,  cfg.tau_mem,  kTol);
    EXPECT_NEAR(p.t_ref,    cfg.t_ref,    kTol);
}

}  // namespace

//=============================================================================
// L23 → PYRAMIDAL round trip restores defaults (pure-function path).
//=============================================================================
TEST(SnnLayerSubtypesRegression, RoundTripL23ToPyramidalRestoresDefaults)
{
    snn_config_t cfg = make_default_config();
    snn_population_t pop;
    std::memset(&pop, 0, sizeof(pop));

    /* First leg: tag L23 and observe the override is in effect. */
    pop.subclass = SNN_NSC_PYRAMIDAL_L23;
    snn_lif_params_t p_l23 = snn_pop_lif_params(&pop, &cfg);
    EXPECT_NEAR(p_l23.tau_mem, 18.0f, kTol)
        << "Sanity: L23 should have applied tau_mem=18 ms before flip-back";

    /* Second leg: flip back to PYRAMIDAL — must restore cfg defaults exactly. */
    pop.subclass = SNN_NSC_PYRAMIDAL;
    snn_lif_params_t p_back = snn_pop_lif_params(&pop, &cfg);
    expect_equals_defaults(p_back, cfg);
}

//=============================================================================
// L4_STELLATE → PYRAMIDAL round trip restores defaults.
//=============================================================================
TEST(SnnLayerSubtypesRegression, RoundTripL4StellateToPyramidalRestoresDefaults)
{
    snn_config_t cfg = make_default_config();
    snn_population_t pop;
    std::memset(&pop, 0, sizeof(pop));

    pop.subclass = SNN_NSC_PYRAMIDAL_L4_STELLATE;
    snn_lif_params_t p_st = snn_pop_lif_params(&pop, &cfg);
    EXPECT_NEAR(p_st.v_thresh, cfg.v_thresh + 2.0f, kTol);

    pop.subclass = SNN_NSC_PYRAMIDAL;
    snn_lif_params_t p_back = snn_pop_lif_params(&pop, &cfg);
    expect_equals_defaults(p_back, cfg);
}

//=============================================================================
// L5_BETZ → PYRAMIDAL round trip restores defaults.
//=============================================================================
TEST(SnnLayerSubtypesRegression, RoundTripL5BetzToPyramidalRestoresDefaults)
{
    snn_config_t cfg = make_default_config();
    snn_population_t pop;
    std::memset(&pop, 0, sizeof(pop));

    pop.subclass = SNN_NSC_PYRAMIDAL_L5_BETZ;
    snn_lif_params_t p_betz = snn_pop_lif_params(&pop, &cfg);
    EXPECT_NEAR(p_betz.v_thresh, cfg.v_thresh - 2.0f, kTol);
    EXPECT_NEAR(p_betz.tau_mem,  25.0f, kTol);

    pop.subclass = SNN_NSC_PYRAMIDAL;
    snn_lif_params_t p_back = snn_pop_lif_params(&pop, &cfg);
    expect_equals_defaults(p_back, cfg);
}

//=============================================================================
// Same round-trip property through the public snn_network_set_pop_subclass()
// API on a live SNN population. Catches any future refactor where the setter
// caches state on the pop that snn_pop_lif_params() then reads alongside the
// switch.
//=============================================================================
TEST(SnnLayerSubtypesRegression, RoundTripViaSetterRestoresDefaults)
{
    snn_config_t cfg;
    std::memset(&cfg, 0, sizeof(cfg));
    ASSERT_EQ(snn_config_default(&cfg), 0);
    cfg.n_inputs  = 1;
    cfg.n_outputs = 1;
    cfg.n_hidden  = 0;
    cfg.dt        = 1.0f;

    snn_network_t* net = snn_network_create(&cfg);
    ASSERT_NE(net, nullptr);

    int pop_id = snn_network_add_population_lightweight(
        net, /*n_neurons=*/8, NEURON_GENERIC_LIF, "round_trip_pop");
    ASSERT_GE(pop_id, 0);

    /* Tag L5_BETZ via the public setter. */
    ASSERT_EQ(snn_network_set_pop_subclass(net, (uint32_t)pop_id,
                                           SNN_NSC_PYRAMIDAL_L5_BETZ), 0);
    {
        snn_population_t* p = net->populations[pop_id];
        ASSERT_NE(p, nullptr);
        snn_lif_params_t lp = snn_pop_lif_params(p, &net->config);
        EXPECT_NEAR(lp.tau_mem, 25.0f, kTol);
        EXPECT_NEAR(lp.v_thresh, net->config.v_thresh - 2.0f, kTol);
    }

    /* Flip back to PYRAMIDAL via the same setter. */
    ASSERT_EQ(snn_network_set_pop_subclass(net, (uint32_t)pop_id,
                                           SNN_NSC_PYRAMIDAL), 0);
    {
        snn_population_t* p = net->populations[pop_id];
        snn_lif_params_t lp = snn_pop_lif_params(p, &net->config);
        expect_equals_defaults(lp, net->config);
    }

    snn_network_destroy(net);
}
