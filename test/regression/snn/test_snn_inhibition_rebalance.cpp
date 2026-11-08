/**
 * @file test_snn_inhibition_rebalance.cpp
 * @brief Regression tests for snn_network_rebalance_inhibition (2026-11-08)
 *
 * Bug: the PV/SOM interneurons were wired "intentionally low-gain" while the
 * mod-5 GABA recurrence carried E/I balance. The Dale fix removed that
 * recurrence and the interneurons were never retuned, so inhibition onto
 * pyramidal cells is ~1/8 of excitation and full-tier bursts run away.
 * The rebalance scales PV/SOM→pyramidal synapses up to a target I/E ratio,
 * never weakens inhibition (idempotent), and respects the metabolic cap.
 */

#include <gtest/gtest.h>
#include <cmath>
#include <cstdlib>
#include <cstring>
#include <vector>

extern "C" {
#include "snn/nimcp_snn_types.h"
#include "snn/nimcp_snn_network.h"
#include "snn/nimcp_snn_config.h"
#include "snn/nimcp_snn_synapse.h"
#include "snn/nimcp_snn_training.h"
}

namespace {

constexpr float kWExc = 0.10f;   /* pyramidal recurrent / input weight */
constexpr float kWPv  = 0.05f;   /* |PV→pyr| */
constexpr float kWSom = 0.03f;   /* |SOM→pyr| */

struct Net {
    snn_network_t* net = nullptr;
    uint32_t in = 0, pyr_a = 0, pyr_b = 0, pv = 0, som = 0;
};

/* Small hierarchical-style network: input → 2 pyramidal pops (recurrent),
 * pyr → PV/SOM, PV/SOM → pyr with GABA_A. Weights have zero spread. */
Net build_net(float w_pv = kWPv, float w_som = kWSom) {
    Net n;
    snn_config_t cfg;
    std::memset(&cfg, 0, sizeof(cfg));
    EXPECT_EQ(snn_config_default(&cfg), 0);
    cfg.n_inputs = 1;
    cfg.n_outputs = 1;
    cfg.n_hidden = 0;
    cfg.dt = 1.0f;
    n.net = snn_network_create(&cfg);
    EXPECT_NE(n.net, nullptr);
    if (!n.net) return n;

    int in  = snn_network_add_population_lightweight(n.net, 200, NEURON_GENERIC_LIF, "t_in");
    int pa  = snn_network_add_population_lightweight(n.net, 200, NEURON_GENERIC_LIF, "t_pyr_a");
    int pb  = snn_network_add_population_lightweight(n.net, 200, NEURON_GENERIC_LIF, "t_pyr_b");
    int pv  = snn_network_add_population_lightweight(n.net, 60,  NEURON_GENERIC_LIF, "t_PV");
    int som = snn_network_add_population_lightweight(n.net, 40,  NEURON_GENERIC_LIF, "t_SOM");
    EXPECT_GE(in, 0); EXPECT_GE(pa, 0); EXPECT_GE(pb, 0); EXPECT_GE(pv, 0); EXPECT_GE(som, 0);
    n.in = in; n.pyr_a = pa; n.pyr_b = pb; n.pv = pv; n.som = som;

    snn_network_set_pop_subclass(n.net, n.pv,  SNN_NSC_PV);
    snn_network_set_pop_subclass(n.net, n.som, SNN_NSC_SOM);

    for (uint32_t d : {n.pyr_a, n.pyr_b}) {
        snn_network_connect_populations(n.net, n.in, d, SNN_TOPO_RANDOM, 0.2f,
                                        SYNAPSE_AMPA, kWExc, 0.0f);
        snn_network_connect_populations(n.net, n.pyr_a == d ? n.pyr_b : n.pyr_a, d,
                                        SNN_TOPO_RANDOM, 0.2f, SYNAPSE_AMPA, kWExc, 0.0f);
        snn_network_connect_populations(n.net, n.pv, d, SNN_TOPO_RANDOM, 0.3f,
                                        SYNAPSE_GABA_A, -w_pv, 0.0f);
        snn_network_connect_populations(n.net, n.som, d, SNN_TOPO_RANDOM, 0.3f,
                                        SYNAPSE_GABA_A, -w_som, 0.0f);
    }
    for (uint32_t s : {n.pyr_a, n.pyr_b}) {
        snn_network_connect_populations(n.net, s, n.pv, SNN_TOPO_RANDOM, 0.1f,
                                        SYNAPSE_AMPA, kWExc, 0.0f);
        snn_network_connect_populations(n.net, s, n.som, SNN_TOPO_RANDOM, 0.1f,
                                        SYNAPSE_AMPA, kWExc, 0.0f);
    }
    snn_network_finalize_connections(n.net);
    return n;
}

snn_csr_storage_t* csr_of(const Net& n, uint32_t pop) {
    return n.net->populations[pop]->incoming_csr;
}

bool from_pvsom(const Net& n, const snn_csr_synapse_t& s) {
    return s.src_pop == n.pv || s.src_pop == n.som;
}

/* Pop-level I/E: Σ|negative w| / Σ positive w */
double pop_ratio(const Net& n, uint32_t pop) {
    snn_csr_storage_t* c = csr_of(n, pop);
    double e = 0.0, i = 0.0;
    for (uint32_t k = 0; k < c->n_synapses; k++) {
        float w = c->entries[k].weight;
        if (w > 0) e += w; else i += -w;
    }
    return e > 0 ? i / e : 0.0;
}

std::vector<float> weights_copy(const Net& n, uint32_t pop) {
    snn_csr_storage_t* c = csr_of(n, pop);
    std::vector<float> w(c->n_synapses);
    for (uint32_t k = 0; k < c->n_synapses; k++) w[k] = c->entries[k].weight;
    return w;
}

class InhRebalanceTest : public ::testing::Test {
protected:
    float saved_cap = 0.0f;
    void SetUp() override {
        saved_cap = snn_tune_get_metabolic_cap();
        unsetenv("NIMCP_SNN_INH_REBALANCE");
        unsetenv("NIMCP_SNN_INH_TARGET_RATIO");
        unsetenv("NIMCP_SNN_INH_MAX_SCALE");
    }
    void TearDown() override {
        snn_tune_set_metabolic_cap(saved_cap);
        unsetenv("NIMCP_SNN_INH_REBALANCE");
        unsetenv("NIMCP_SNN_INH_TARGET_RATIO");
        unsetenv("NIMCP_SNN_INH_MAX_SCALE");
    }
};

}  // namespace

/* 1. Bad arguments */
TEST_F(InhRebalanceTest, RejectsBadArgs) {
    snn_inh_rebalance_stats_t st;
    EXPECT_EQ(snn_network_rebalance_inhibition(nullptr, 0.5f, 8.0f, &st), SNN_ERROR_NULL_POINTER);
    Net n = build_net();
    ASSERT_NE(n.net, nullptr);
    EXPECT_EQ(snn_network_rebalance_inhibition(n.net, 0.0f, 8.0f, &st), SNN_ERROR_INVALID_CONFIG);
    EXPECT_EQ(snn_network_rebalance_inhibition(n.net, -1.0f, 8.0f, &st), SNN_ERROR_INVALID_CONFIG);
    EXPECT_EQ(snn_network_rebalance_inhibition(n.net, 0.5f, 0.5f, &st), SNN_ERROR_INVALID_CONFIG);
    EXPECT_EQ(snn_network_rebalance_inhibition(n.net, NAN, 8.0f, &st), SNN_ERROR_INVALID_CONFIG);
    EXPECT_EQ(snn_network_apply_inhibition_rebalance(nullptr), SNN_ERROR_NULL_POINTER);
    snn_network_destroy(n.net);
}

/* 2. Weak inhibition reaches the target ratio */
TEST_F(InhRebalanceTest, ReachesTargetRatio) {
    Net n = build_net();
    ASSERT_NE(n.net, nullptr);
    const double before = pop_ratio(n, n.pyr_a);
    ASSERT_LT(before, 0.25);

    snn_inh_rebalance_stats_t st;
    ASSERT_EQ(snn_network_rebalance_inhibition(n.net, 0.5f, 8.0f, &st), 0);
    EXPECT_EQ(st.pops_examined, 2u);
    EXPECT_EQ(st.pops_scaled, 2u);
    EXPECT_GT(st.synapses_scaled, 0u);
    EXPECT_EQ(st.neurons_capped, 0u);
    EXPECT_NEAR(pop_ratio(n, n.pyr_a), 0.5, 0.01);
    EXPECT_NEAR(pop_ratio(n, n.pyr_b), 0.5, 0.01);
    EXPECT_NEAR(st.mean_ratio_after, 0.5f, 0.01f);
    EXPECT_NEAR(st.mean_ratio_before, (float)before, 0.05f);
    snn_network_destroy(n.net);
}

/* 3. PV and SOM onto one pop share the multiplier (PV:SOM ratio kept) */
TEST_F(InhRebalanceTest, PreservesPvSomRatio) {
    Net n = build_net();
    ASSERT_NE(n.net, nullptr);
    ASSERT_EQ(snn_network_rebalance_inhibition(n.net, 0.5f, 8.0f, nullptr), 0);
    snn_csr_storage_t* c = csr_of(n, n.pyr_a);
    double pv_w = 0, som_w = 0;
    for (uint32_t k = 0; k < c->n_synapses; k++) {
        const auto& s = c->entries[k];
        if (s.src_pop == n.pv)  { if (pv_w == 0)  pv_w  = -s.weight; EXPECT_NEAR(-s.weight, pv_w, 1e-6); }
        if (s.src_pop == n.som) { if (som_w == 0) som_w = -s.weight; EXPECT_NEAR(-s.weight, som_w, 1e-6); }
    }
    ASSERT_GT(pv_w, kWPv);
    EXPECT_NEAR(pv_w / som_w, (double)kWPv / kWSom, 1e-4);
    snn_network_destroy(n.net);
}

/* 4. Excitatory synapses and synapses onto interneurons are untouched */
TEST_F(InhRebalanceTest, LeavesExcitatoryAndInterneuronInputsAlone) {
    Net n = build_net();
    ASSERT_NE(n.net, nullptr);
    const auto pv_in  = weights_copy(n, n.pv);
    const auto som_in = weights_copy(n, n.som);
    const auto pyr_before = weights_copy(n, n.pyr_a);
    ASSERT_EQ(snn_network_rebalance_inhibition(n.net, 0.5f, 8.0f, nullptr), 0);
    EXPECT_EQ(weights_copy(n, n.pv), pv_in);
    EXPECT_EQ(weights_copy(n, n.som), som_in);
    snn_csr_storage_t* c = csr_of(n, n.pyr_a);
    for (uint32_t k = 0; k < c->n_synapses; k++) {
        if (!from_pvsom(n, c->entries[k])) {
            EXPECT_FLOAT_EQ(c->entries[k].weight, pyr_before[k]);
        }
    }
    snn_network_destroy(n.net);
}

/* 5. Second application is a no-op */
TEST_F(InhRebalanceTest, IdempotentOnSecondCall) {
    Net n = build_net();
    ASSERT_NE(n.net, nullptr);
    ASSERT_EQ(snn_network_rebalance_inhibition(n.net, 0.5f, 8.0f, nullptr), 0);
    const auto once = weights_copy(n, n.pyr_a);
    snn_inh_rebalance_stats_t st;
    ASSERT_EQ(snn_network_rebalance_inhibition(n.net, 0.5f, 8.0f, &st), 0);
    EXPECT_EQ(st.pops_scaled, 0u);
    EXPECT_EQ(st.synapses_scaled, 0u);
    EXPECT_EQ(weights_copy(n, n.pyr_a), once);
    snn_network_destroy(n.net);
}

/* 6. Inhibition already above target is never weakened */
TEST_F(InhRebalanceTest, NeverWeakensStrongInhibition) {
    Net n = build_net(1.0f, 0.8f);   /* strong inhibition */
    ASSERT_NE(n.net, nullptr);
    ASSERT_GT(pop_ratio(n, n.pyr_a), 0.5);
    const auto before = weights_copy(n, n.pyr_a);
    snn_inh_rebalance_stats_t st;
    ASSERT_EQ(snn_network_rebalance_inhibition(n.net, 0.5f, 8.0f, &st), 0);
    EXPECT_EQ(st.pops_scaled, 0u);
    EXPECT_EQ(weights_copy(n, n.pyr_a), before);
    snn_network_destroy(n.net);
}

/* 7. Multiplier never exceeds max_scale */
TEST_F(InhRebalanceTest, ClampsToMaxScale) {
    Net n = build_net();
    ASSERT_NE(n.net, nullptr);
    snn_inh_rebalance_stats_t st;
    ASSERT_EQ(snn_network_rebalance_inhibition(n.net, 5.0f, 2.0f, &st), 0);
    EXPECT_FLOAT_EQ(st.max_scale, 2.0f);
    snn_csr_storage_t* c = csr_of(n, n.pyr_a);
    for (uint32_t k = 0; k < c->n_synapses; k++) {
        const auto& s = c->entries[k];
        if (s.src_pop == n.pv)  EXPECT_NEAR(-s.weight, 2.0f * kWPv, 1e-6);
        if (s.src_pop == n.som) EXPECT_NEAR(-s.weight, 2.0f * kWSom, 1e-6);
    }
    snn_network_destroy(n.net);
}

/* 8. Per-neuron Σ|w| stays within the metabolic cap */
TEST_F(InhRebalanceTest, RespectsMetabolicCap) {
    Net n = build_net();
    ASSERT_NE(n.net, nullptr);
    const float cap_factor = 0.12f;   /* just above the pre-rebalance mean |w| */
    snn_tune_set_metabolic_cap(cap_factor);
    ASSERT_FLOAT_EQ(snn_tune_get_metabolic_cap(), cap_factor);

    snn_inh_rebalance_stats_t st;
    ASSERT_EQ(snn_network_rebalance_inhibition(n.net, 2.0f, 8.0f, &st), 0);
    EXPECT_GT(st.neurons_capped, 0u);
    for (uint32_t pop : {n.pyr_a, n.pyr_b}) {
        snn_csr_storage_t* c = csr_of(n, pop);
        for (uint32_t r = 0; r < c->n_neurons; r++) {
            const uint32_t rs = c->row_ptr[r], re = c->row_ptr[r + 1];
            if (re == rs) continue;
            double sum_abs = 0.0;
            for (uint32_t k = rs; k < re; k++) sum_abs += std::fabs(c->entries[k].weight);
            EXPECT_LE(sum_abs, (double)cap_factor * (re - rs) + 1e-4)
                << "pop " << pop << " row " << r;
        }
    }
    snn_network_destroy(n.net);
}

/* 9. Flat weights[] mirror matches entries[] */
TEST_F(InhRebalanceTest, FlatWeightsMirrorEntries) {
    Net n = build_net();
    ASSERT_NE(n.net, nullptr);
    snn_csr_storage_t* c = csr_of(n, n.pyr_a);
    if (!c->weights) {
        GTEST_SKIP() << "flat weights[] not populated on this build (no GPU prep)";
    }
    ASSERT_EQ(snn_network_rebalance_inhibition(n.net, 0.5f, 8.0f, nullptr), 0);
    for (uint32_t k = 0; k < c->n_synapses; k++) {
        EXPECT_FLOAT_EQ(c->weights[k], c->entries[k].weight) << "synapse " << k;
    }
    snn_network_destroy(n.net);
}

/* 10. Env hook: disable switch and target override */
TEST_F(InhRebalanceTest, EnvHookDisableAndOverride) {
    Net n = build_net();
    ASSERT_NE(n.net, nullptr);
    const auto before = weights_copy(n, n.pyr_a);

    setenv("NIMCP_SNN_INH_REBALANCE", "0", 1);
    ASSERT_EQ(snn_network_apply_inhibition_rebalance(n.net), 0);
    EXPECT_EQ(weights_copy(n, n.pyr_a), before);

    unsetenv("NIMCP_SNN_INH_REBALANCE");
    setenv("NIMCP_SNN_INH_TARGET_RATIO", "0.3", 1);
    ASSERT_EQ(snn_network_apply_inhibition_rebalance(n.net), 0);
    EXPECT_NEAR(pop_ratio(n, n.pyr_a), 0.3, 0.01);
    snn_network_destroy(n.net);
}
