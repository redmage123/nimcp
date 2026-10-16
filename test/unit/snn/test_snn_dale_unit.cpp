// =============================================================================
// test_snn_dale_unit.cpp
// =============================================================================
// WHAT: Unit tests for snn_network_validate_dale() — the runtime Dale's
//       principle validator added in Wave C task #260.
// WHY:  Dale's principle says one neuron releases ONE classical
//       neurotransmitter at all of its synapses. The validator walks the
//       per-receptor table (synapse_type_per_src) and flags any source
//       pop that emits BOTH excitatory (AMPA/NMDA) and inhibitory
//       (GABA_A/GABA_B) synapses.
// HOW:  GoogleTest. Builds tiny synthetic networks via
//       snn_network_add_population_lightweight and connects them with
//       chosen receptor types, then calls the validator and inspects the
//       returned violation count + err_buf.
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

snn_network_t* make_empty_net() {
    snn_config_t cfg;
    snn_config_default(&cfg);
    cfg.n_inputs = 1;
    cfg.n_outputs = 1;
    cfg.n_hidden = 0;
    cfg.dt = 1.0f;
    return snn_network_create(&cfg);
}

// Small helper: make a connection and check its return.
int connect_dense(snn_network_t* net, int src, int dst,
                  synapse_type_t st, float w) {
    return snn_network_connect_populations(
        net, (uint32_t)src, (uint32_t)dst,
        SNN_TOPO_FULL, 1.0f, st, w, 0.0f);
}

}  // namespace

// -----------------------------------------------------------------------------
// 1. NULL / empty network: validator returns 0.
// -----------------------------------------------------------------------------
TEST(SnnDaleUnit, NullAndEmptyAreClean) {
    char err[256] = "preexisting";
    EXPECT_EQ(snn_network_validate_dale(nullptr, err, sizeof(err)), 0);
    EXPECT_STREQ(err, "");  // err_buf cleared even on NULL net

    snn_network_t* net = make_empty_net();
    ASSERT_NE(net, nullptr);
    EXPECT_EQ(snn_network_validate_dale(net, err, sizeof(err)), 0);
    EXPECT_STREQ(err, "");
    snn_network_destroy(net);
}

// -----------------------------------------------------------------------------
// 2. Pure excitatory network: validator stays silent.
// -----------------------------------------------------------------------------
TEST(SnnDaleUnit, CleanExcitatoryNetwork) {
    snn_network_t* net = make_empty_net();
    ASSERT_NE(net, nullptr);

    int a = snn_network_add_population_lightweight(net, 4, NEURON_GENERIC_LIF, "A");
    int b = snn_network_add_population_lightweight(net, 4, NEURON_GENERIC_LIF, "B");
    int c = snn_network_add_population_lightweight(net, 4, NEURON_GENERIC_LIF, "C");
    ASSERT_GE(a, 0); ASSERT_GE(b, 0); ASSERT_GE(c, 0);

    // A → B AMPA, A → C NMDA (still excitatory class), B → C AMPA
    EXPECT_GT(connect_dense(net, a, b, SYNAPSE_AMPA, 0.1f), 0);
    EXPECT_GT(connect_dense(net, a, c, SYNAPSE_NMDA, 0.1f), 0);
    EXPECT_GT(connect_dense(net, b, c, SYNAPSE_AMPA, 0.1f), 0);

    char err[256];
    EXPECT_EQ(snn_network_validate_dale(net, err, sizeof(err)), 0);
    EXPECT_STREQ(err, "");

    snn_network_destroy(net);
}

// -----------------------------------------------------------------------------
// 3. Pure inhibitory source: validator stays silent.
// -----------------------------------------------------------------------------
TEST(SnnDaleUnit, CleanInhibitoryNetwork) {
    snn_network_t* net = make_empty_net();
    ASSERT_NE(net, nullptr);

    int pv = snn_network_add_population_lightweight(net, 4, NEURON_GENERIC_LIF, "PV");
    int p1 = snn_network_add_population_lightweight(net, 4, NEURON_GENERIC_LIF, "pyr1");
    int p2 = snn_network_add_population_lightweight(net, 4, NEURON_GENERIC_LIF, "pyr2");
    ASSERT_GE(pv, 0); ASSERT_GE(p1, 0); ASSERT_GE(p2, 0);

    // PV → pyr1 GABA_A, PV → pyr2 GABA_B (both inhibitory class)
    EXPECT_GT(connect_dense(net, pv, p1, SYNAPSE_GABA_A, -0.1f), 0);
    EXPECT_GT(connect_dense(net, pv, p2, SYNAPSE_GABA_B, -0.1f), 0);

    char err[256];
    EXPECT_EQ(snn_network_validate_dale(net, err, sizeof(err)), 0);
    EXPECT_STREQ(err, "");

    snn_network_destroy(net);
}

// -----------------------------------------------------------------------------
// 4. Single-pop network with no outgoing synapses: validator stays silent.
// -----------------------------------------------------------------------------
TEST(SnnDaleUnit, SinglePopNoOutgoingIsClean) {
    snn_network_t* net = make_empty_net();
    ASSERT_NE(net, nullptr);

    int p = snn_network_add_population_lightweight(net, 4, NEURON_GENERIC_LIF, "lonely");
    ASSERT_GE(p, 0);

    char err[256];
    EXPECT_EQ(snn_network_validate_dale(net, err, sizeof(err)), 0);
    EXPECT_STREQ(err, "");

    snn_network_destroy(net);
}

// -----------------------------------------------------------------------------
// 5. Mixed source: pop A emits AMPA AND GABA_A. Validator MUST flag.
// -----------------------------------------------------------------------------
TEST(SnnDaleUnit, MixedSourceIsViolation) {
    snn_network_t* net = make_empty_net();
    ASSERT_NE(net, nullptr);

    int a  = snn_network_add_population_lightweight(net, 4, NEURON_GENERIC_LIF, "A_chimera");
    int d1 = snn_network_add_population_lightweight(net, 4, NEURON_GENERIC_LIF, "dst_exc");
    int d2 = snn_network_add_population_lightweight(net, 4, NEURON_GENERIC_LIF, "dst_inh");
    ASSERT_GE(a, 0); ASSERT_GE(d1, 0); ASSERT_GE(d2, 0);

    EXPECT_GT(connect_dense(net, a, d1, SYNAPSE_AMPA,    0.1f), 0);
    EXPECT_GT(connect_dense(net, a, d2, SYNAPSE_GABA_A, -0.1f), 0);

    char err[256];
    int v = snn_network_validate_dale(net, err, sizeof(err));
    EXPECT_GT(v, 0);
    // The error message must mention the source pop name.
    EXPECT_NE(std::string(err).find("A_chimera"), std::string::npos)
        << "err='" << err << "'";

    snn_network_destroy(net);
}
