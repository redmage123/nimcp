// =============================================================================
// test_snn_dale_regression.cpp
// =============================================================================
// WHAT: Regression test for snn_network_validate_dale() — guarantees the
//       validator always catches a deliberately-introduced Dale violation
//       (and stays silent on a clean control network with the same shape).
// WHY:  If a future refactor accidentally lets a chimeric src pop slip
//       past the validator (e.g. by skipping the SYNAPSE_GENERIC sentinel
//       check or by overwriting the per-src table out of order), the
//       daemon would deposit the same spike into both g_ampa and
//       g_gaba_a — biophysically impossible. This test is the canary.
// HOW:  GoogleTest. Two cases:
//       1. Pop A → dst1 AMPA + Pop A → dst2 GABA_A → validator must flag.
//       2. Same topology but the second connection re-typed AMPA →
//          validator must stay silent.
// =============================================================================

#include <gtest/gtest.h>
#include <cstring>
#include <string>

extern "C" {
#include "snn/nimcp_snn_network.h"
#include "snn/nimcp_snn_types.h"
#include "snn/nimcp_snn_config.h"
#include "core/synapse_types/nimcp_synapse_types.h"
}

namespace {

snn_network_t* make_three_pop_net() {
    snn_config_t cfg;
    snn_config_default(&cfg);
    cfg.n_inputs = 1;
    cfg.n_outputs = 1;
    cfg.n_hidden = 0;
    cfg.dt = 1.0f;
    return snn_network_create(&cfg);
}

}  // namespace

// -----------------------------------------------------------------------------
// 1. Deliberate Dale violation: validator MUST flag.
// -----------------------------------------------------------------------------
TEST(SnnDaleRegression, MixedSourceIsCaught) {
    snn_network_t* net = make_three_pop_net();
    ASSERT_NE(net, nullptr);

    int a    = snn_network_add_population_lightweight(net, 4, NEURON_GENERIC_LIF, "violator_A");
    int dst1 = snn_network_add_population_lightweight(net, 4, NEURON_GENERIC_LIF, "dst1");
    int dst2 = snn_network_add_population_lightweight(net, 4, NEURON_GENERIC_LIF, "dst2");
    ASSERT_GE(a, 0);
    ASSERT_GE(dst1, 0);
    ASSERT_GE(dst2, 0);

    EXPECT_GT(snn_network_connect_populations(net, (uint32_t)a, (uint32_t)dst1,
                                              SNN_TOPO_FULL, 1.0f,
                                              SYNAPSE_AMPA, 0.1f, 0.0f), 0);
    EXPECT_GT(snn_network_connect_populations(net, (uint32_t)a, (uint32_t)dst2,
                                              SNN_TOPO_FULL, 1.0f,
                                              SYNAPSE_GABA_A, -0.1f, 0.0f), 0);

    char err[256];
    int v = snn_network_validate_dale(net, err, sizeof(err));
    EXPECT_GT(v, 0) << "Dale validator failed to catch deliberate violation";
    EXPECT_NE(std::string(err).find("violator_A"), std::string::npos)
        << "err message must name the violating pop, got '" << err << "'";

    snn_network_destroy(net);
}

// -----------------------------------------------------------------------------
// 2. Clean control: same topology but uniformly excitatory. No violation.
// -----------------------------------------------------------------------------
TEST(SnnDaleRegression, MatchedCleanControlPasses) {
    snn_network_t* net = make_three_pop_net();
    ASSERT_NE(net, nullptr);

    int a    = snn_network_add_population_lightweight(net, 4, NEURON_GENERIC_LIF, "clean_A");
    int dst1 = snn_network_add_population_lightweight(net, 4, NEURON_GENERIC_LIF, "dst1");
    int dst2 = snn_network_add_population_lightweight(net, 4, NEURON_GENERIC_LIF, "dst2");
    ASSERT_GE(a, 0);
    ASSERT_GE(dst1, 0);
    ASSERT_GE(dst2, 0);

    EXPECT_GT(snn_network_connect_populations(net, (uint32_t)a, (uint32_t)dst1,
                                              SNN_TOPO_FULL, 1.0f,
                                              SYNAPSE_AMPA, 0.1f, 0.0f), 0);
    EXPECT_GT(snn_network_connect_populations(net, (uint32_t)a, (uint32_t)dst2,
                                              SNN_TOPO_FULL, 1.0f,
                                              SYNAPSE_NMDA, 0.1f, 0.0f), 0);

    char err[256] = "preexisting";
    int v = snn_network_validate_dale(net, err, sizeof(err));
    EXPECT_EQ(v, 0);
    EXPECT_STREQ(err, "");

    snn_network_destroy(net);
}
