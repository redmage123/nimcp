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
 *
 * 2026-11-09: (a) snn_network_save does not persist pop subclass, conduction
 * delay, heterogeneity or gap coupling, so every resume turned PV/SOM into
 * plain pyramidal cells and the rebalance found nothing to scale;
 * snn_hierarchical_restore_pop_properties re-applies them on load.
 * (b) L2 rows already sat at the metabolic cap, so inhibition could not
 * grow; rows over the cap are now multiplicatively renormalized (floor
 * min_exc_scale) so the new I/E ratio survives.
 */

#include <gtest/gtest.h>
#include <cmath>
#include <cstdlib>
#include <cstring>
#include <string>
#include <unistd.h>
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
        unsetenv("NIMCP_SNN_INH_MIN_EXC_SCALE");
        unsetenv("NIMCP_SNN_RESTORE_POP_PROPS");
    }
    void TearDown() override {
        snn_tune_set_metabolic_cap(saved_cap);
        unsetenv("NIMCP_SNN_INH_REBALANCE");
        unsetenv("NIMCP_SNN_INH_TARGET_RATIO");
        unsetenv("NIMCP_SNN_INH_MAX_SCALE");
        unsetenv("NIMCP_SNN_INH_MIN_EXC_SCALE");
        unsetenv("NIMCP_SNN_RESTORE_POP_PROPS");
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

/* ===================== Cap renormalization (2026-11-09) ===================== */

namespace {

double row_sum_abs(snn_csr_storage_t* c, uint32_t r) {
    double s = 0.0;
    for (uint32_t k = c->row_ptr[r]; k < c->row_ptr[r + 1]; k++) s += std::fabs(c->entries[k].weight);
    return s;
}

/* Cap factor just above the pre-rebalance mean |w| (≈0.084) but below what
 * the target ratio needs, so most rows must be renormalized. */
constexpr float kTightCap = 0.09f;

}  // namespace

/* 11. Rows over the cap keep the target ratio; excitation gives up the room */
TEST_F(InhRebalanceTest, RenormalizesRowsAtCapKeepingRatio) {
    Net n = build_net();
    ASSERT_NE(n.net, nullptr);
    snn_tune_set_metabolic_cap(kTightCap);
    snn_inh_rebalance_stats_t st;
    ASSERT_EQ(snn_network_rebalance_inhibition(n.net, 0.5f, 8.0f, &st), 0);
    EXPECT_GT(st.neurons_renormalized, 0u);
    EXPECT_LT(st.min_exc_scale, 1.0f);
    EXPECT_GE(st.min_exc_scale, SNN_INH_REBALANCE_MIN_EXC_SCALE);
    EXPECT_NEAR(pop_ratio(n, n.pyr_a), 0.5, 0.01);
    EXPECT_NEAR(pop_ratio(n, n.pyr_b), 0.5, 0.01);
    for (uint32_t pop : {n.pyr_a, n.pyr_b}) {
        snn_csr_storage_t* c = csr_of(n, pop);
        for (uint32_t r = 0; r < c->n_neurons; r++) {
            const uint32_t fan = c->row_ptr[r + 1] - c->row_ptr[r];
            if (fan == 0) continue;
            EXPECT_LE(row_sum_abs(c, r), (double)kTightCap * fan + 1e-4) << "row " << r;
        }
        bool shrank = false;
        for (uint32_t k = 0; k < c->n_synapses; k++) {
            const float w = c->entries[k].weight;
            if (w > 0.0f) {
                EXPECT_LE(w, kWExc + 1e-6f);
                EXPECT_GE(w, kWExc * SNN_INH_REBALANCE_MIN_EXC_SCALE - 1e-6f);
                if (w < kWExc - 1e-6f) shrank = true;
            }
        }
        EXPECT_TRUE(shrank) << "pop " << pop;
    }
    snn_network_destroy(n.net);
}

/* 12. Renormalization is uniform within a row (excitation shrinks by one factor) */
TEST_F(InhRebalanceTest, RenormalizationUniformWithinRow) {
    Net n = build_net();
    ASSERT_NE(n.net, nullptr);
    snn_tune_set_metabolic_cap(kTightCap);
    ASSERT_EQ(snn_network_rebalance_inhibition(n.net, 0.5f, 8.0f, nullptr), 0);
    snn_csr_storage_t* c = csr_of(n, n.pyr_a);
    for (uint32_t r = 0; r < c->n_neurons; r++) {
        float first = 0.0f;
        for (uint32_t k = c->row_ptr[r]; k < c->row_ptr[r + 1]; k++) {
            const float w = c->entries[k].weight;
            if (w <= 0.0f) continue;
            if (first == 0.0f) first = w;
            EXPECT_NEAR(w, first, 1e-6f) << "row " << r;
        }
    }
    snn_network_destroy(n.net);
}

/* 13. min_exc_scale floor: excitation never drops below it; inhibition is held instead */
TEST_F(InhRebalanceTest, MinExcScaleFloorHoldsExcitation) {
    Net n = build_net();
    ASSERT_NE(n.net, nullptr);
    snn_tune_set_metabolic_cap(kTightCap);
    const double before = pop_ratio(n, n.pyr_a);
    snn_inh_rebalance_stats_t st;
    ASSERT_EQ(snn_network_rebalance_inhibition_ex(n.net, 2.0f, 8.0f, 0.9f, &st), 0);
    EXPECT_GT(st.neurons_capped, 0u);
    EXPECT_GE(st.min_exc_scale, 0.9f - 1e-5f);
    const double after = pop_ratio(n, n.pyr_a);
    EXPECT_GT(after, before);
    EXPECT_LT(after, 2.0);
    snn_csr_storage_t* c = csr_of(n, n.pyr_a);
    for (uint32_t k = 0; k < c->n_synapses; k++) {
        const float w = c->entries[k].weight;
        if (w > 0.0f) EXPECT_GE(w, 0.9f * kWExc - 1e-6f);
    }
    for (uint32_t r = 0; r < c->n_neurons; r++) {
        const uint32_t fan = c->row_ptr[r + 1] - c->row_ptr[r];
        if (fan == 0) continue;
        EXPECT_LE(row_sum_abs(c, r), (double)kTightCap * fan + 1e-4) << "row " << r;
    }
    snn_network_destroy(n.net);
}

/* 14. min_exc_scale = 1 (and the env override) keeps excitation untouched */
TEST_F(InhRebalanceTest, MinExcScaleOneKeepsExcitation) {
    for (int via_env = 0; via_env < 2; via_env++) {
        Net n = build_net();
        ASSERT_NE(n.net, nullptr);
        snn_tune_set_metabolic_cap(kTightCap);
        const auto before = weights_copy(n, n.pyr_a);
        if (via_env) {
            setenv("NIMCP_SNN_INH_MIN_EXC_SCALE", "1", 1);
            ASSERT_EQ(snn_network_apply_inhibition_rebalance(n.net), 0);
            unsetenv("NIMCP_SNN_INH_MIN_EXC_SCALE");
        } else {
            snn_inh_rebalance_stats_t st;
            ASSERT_EQ(snn_network_rebalance_inhibition_ex(n.net, 0.5f, 8.0f, 1.0f, &st), 0);
            EXPECT_EQ(st.neurons_renormalized, 0u);
            EXPECT_FLOAT_EQ(st.min_exc_scale, 1.0f);
        }
        snn_csr_storage_t* c = csr_of(n, n.pyr_a);
        for (uint32_t k = 0; k < c->n_synapses; k++) {
            if (before[k] > 0.0f) EXPECT_FLOAT_EQ(c->entries[k].weight, before[k]) << "env=" << via_env;
            else EXPECT_LE(c->entries[k].weight, before[k] + 1e-7f);   /* never weakened */
        }
        snn_network_destroy(n.net);
    }
}

/* 15. _ex rejects a bad floor */
TEST_F(InhRebalanceTest, ExRejectsBadMinExcScale) {
    Net n = build_net();
    ASSERT_NE(n.net, nullptr);
    EXPECT_EQ(snn_network_rebalance_inhibition_ex(n.net, 0.5f, 8.0f, 0.0f, nullptr), SNN_ERROR_INVALID_CONFIG);
    EXPECT_EQ(snn_network_rebalance_inhibition_ex(n.net, 0.5f, 8.0f, 1.5f, nullptr), SNN_ERROR_INVALID_CONFIG);
    EXPECT_EQ(snn_network_rebalance_inhibition_ex(n.net, 0.5f, 8.0f, NAN, nullptr), SNN_ERROR_INVALID_CONFIG);
    EXPECT_EQ(snn_network_rebalance_inhibition_ex(nullptr, 0.5f, 8.0f, 0.5f, nullptr), SNN_ERROR_NULL_POINTER);
    snn_network_destroy(n.net);
}

/* ================ Pop property restore on load (2026-11-09) ================ */

namespace {

struct NamedNet {
    snn_network_t* net = nullptr;
    int in = -1, l4 = -1, l2 = -1, pv = -1, som = -1, vip = -1, trn = -1;
    int other_a = -1, other_b = -1, other_c = -1;
};

/* Builder-style names, everything at load defaults (pyramidal, delay 0,
 * σ 0, gap 0). L4_integr_0 and L2_pattern_1 get excitation + weak PV/SOM. */
NamedNet build_named_net() {
    NamedNet n;
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
    auto add = [&](uint32_t sz, const char* name) {
        int id = snn_network_add_population_lightweight(n.net, sz, NEURON_GENERIC_LIF, name);
        EXPECT_GE(id, 0) << name;
        return id;
    };
    n.in      = add(200, "input_0");
    n.l4      = add(200, "L4_integr_0");
    n.l2      = add(200, "L2_pattern_1");
    n.pv      = add(60,  "L4_integr_PV");
    n.som     = add(40,  "L4_integr_SOM");
    n.vip     = add(20,  "L4_integr_VIP");
    n.trn     = add(40,  "thalamus_reticular");
    n.other_a = add(40,  "wernicke_substrate");
    n.other_b = add(40,  "custom_7");
    n.other_c = add(40,  "L4_integr_x");
    for (int d : {n.l4, n.l2}) {
        snn_network_connect_populations(n.net, n.in, d, SNN_TOPO_RANDOM, 0.2f,
                                        SYNAPSE_AMPA, kWExc, 0.0f);
        snn_network_connect_populations(n.net, n.pv, d, SNN_TOPO_RANDOM, 0.3f,
                                        SYNAPSE_GABA_A, -kWPv, 0.0f);
        snn_network_connect_populations(n.net, n.som, d, SNN_TOPO_RANDOM, 0.3f,
                                        SYNAPSE_GABA_A, -kWSom, 0.0f);
    }
    snn_network_connect_populations(n.net, n.l4, n.pv, SNN_TOPO_RANDOM, 0.1f,
                                    SYNAPSE_AMPA, kWExc, 0.0f);
    snn_network_connect_populations(n.net, n.l4, n.som, SNN_TOPO_RANDOM, 0.1f,
                                    SYNAPSE_AMPA, kWExc, 0.0f);
    snn_network_finalize_connections(n.net);
    return n;
}

snn_population_t* pop_named(snn_network_t* net, const char* name) {
    for (uint32_t p = 0; p < net->n_populations; p++) {
        snn_population_t* pop = net->populations[p];
        if (pop && std::strcmp(pop->name, name) == 0) return pop;
    }
    return nullptr;
}

double ratio_of(snn_population_t* pop) {
    snn_csr_storage_t* c = pop->incoming_csr;
    double e = 0.0, i = 0.0;
    for (uint32_t k = 0; k < c->n_synapses; k++) {
        const float w = c->entries[k].weight;
        if (w > 0) e += w; else i += -w;
    }
    return e > 0 ? i / e : 0.0;
}

std::string tmp_snn_path(const char* tag) {
    return std::string("/tmp/nimcp_restore_test_") + tag + "_" +
           std::to_string((long)getpid()) + ".snn";
}

}  // namespace

/* R1. NULL network */
TEST_F(InhRebalanceTest, RestoreRejectsNull) {
    EXPECT_EQ(snn_hierarchical_restore_pop_properties(nullptr), -1);
}

/* R2. Builder names get the builder's subclass, delay, σ and gap coupling */
TEST_F(InhRebalanceTest, RestoreAppliesBuilderProperties) {
    NamedNet n = build_named_net();
    ASSERT_NE(n.net, nullptr);
    snn_population_t** P = n.net->populations;
    ASSERT_EQ(P[n.pv]->subclass, SNN_NSC_PYRAMIDAL);   /* load-default state */

    EXPECT_EQ(snn_hierarchical_restore_pop_properties(n.net), 7);  /* 3 pyr + 3 inh + TRN */

    EXPECT_EQ(P[n.in]->subclass, SNN_NSC_PYRAMIDAL);
    EXPECT_EQ(P[n.l4]->subclass, SNN_NSC_PYRAMIDAL_L4_STELLATE);
    EXPECT_EQ(P[n.l2]->subclass, SNN_NSC_PYRAMIDAL_L23);
    EXPECT_EQ(P[n.pv]->subclass, SNN_NSC_PV);
    EXPECT_EQ(P[n.som]->subclass, SNN_NSC_SOM);
    EXPECT_EQ(P[n.vip]->subclass, SNN_NSC_VIP);
    EXPECT_EQ(P[n.trn]->subclass, SNN_NSC_TRN);

    for (int p : {n.in, n.l4, n.l2}) {
        EXPECT_EQ(P[p]->conduction_delay_steps, 1u) << P[p]->name;
        EXPECT_FLOAT_EQ(P[p]->heterogeneity_sigma, 0.10f) << P[p]->name;
    }
    EXPECT_EQ(P[n.pv]->conduction_delay_steps, 0u);
    EXPECT_FLOAT_EQ(P[n.pv]->gap_coupling, 0.05f);
    EXPECT_EQ(P[n.som]->conduction_delay_steps, 1u);
    EXPECT_EQ(P[n.vip]->conduction_delay_steps, 1u);
    EXPECT_FLOAT_EQ(P[n.som]->gap_coupling, 0.0f);
    snn_network_destroy(n.net);
}

/* R3. Pops whose names are not builder names are untouched */
TEST_F(InhRebalanceTest, RestoreLeavesOtherNamesAlone) {
    NamedNet n = build_named_net();
    ASSERT_NE(n.net, nullptr);
    snn_population_t** P = n.net->populations;
    for (int p : {n.other_a, n.other_b, n.other_c}) {
        ASSERT_EQ(snn_network_set_pop_subclass(n.net, p, SNN_NSC_VIP), 0);
        ASSERT_EQ(snn_network_set_pop_conduction_delay(n.net, p, 2), 0);
    }
    ASSERT_GE(snn_hierarchical_restore_pop_properties(n.net), 0);
    for (int p : {n.other_a, n.other_b, n.other_c}) {
        EXPECT_EQ(P[p]->subclass, SNN_NSC_VIP) << P[p]->name;
        EXPECT_EQ(P[p]->conduction_delay_steps, 2u) << P[p]->name;
        EXPECT_FLOAT_EQ(P[p]->heterogeneity_sigma, 0.0f) << P[p]->name;
        EXPECT_FLOAT_EQ(P[p]->gap_coupling, 0.0f) << P[p]->name;
    }
    snn_network_destroy(n.net);
}

/* R4. Second call changes nothing */
TEST_F(InhRebalanceTest, RestoreIsIdempotent) {
    NamedNet n = build_named_net();
    ASSERT_NE(n.net, nullptr);
    const int first = snn_hierarchical_restore_pop_properties(n.net);
    std::vector<int> sub;
    std::vector<unsigned> del;
    std::vector<float> sig, gap;
    for (uint32_t p = 0; p < n.net->n_populations; p++) {
        snn_population_t* pop = n.net->populations[p];
        sub.push_back((int)pop->subclass); del.push_back(pop->conduction_delay_steps);
        sig.push_back(pop->heterogeneity_sigma); gap.push_back(pop->gap_coupling);
    }
    EXPECT_EQ(snn_hierarchical_restore_pop_properties(n.net), first);
    for (uint32_t p = 0; p < n.net->n_populations; p++) {
        snn_population_t* pop = n.net->populations[p];
        EXPECT_EQ((int)pop->subclass, sub[p]);
        EXPECT_EQ((unsigned)pop->conduction_delay_steps, del[p]);
        EXPECT_FLOAT_EQ(pop->heterogeneity_sigma, sig[p]);
        EXPECT_FLOAT_EQ(pop->gap_coupling, gap[p]);
    }
    snn_network_destroy(n.net);
}

/* R5. Save → load: properties come back and the rebalance runs (the live bug) */
TEST_F(InhRebalanceTest, SaveLoadRestoresAndRebalances) {
    NamedNet n = build_named_net();
    ASSERT_NE(n.net, nullptr);
    ASSERT_EQ(snn_hierarchical_restore_pop_properties(n.net), 7);
    const double before = ratio_of(n.net->populations[n.l4]);
    ASSERT_LT(before, SNN_INH_REBALANCE_TARGET_RATIO - 0.1);   /* load must raise it */
    const std::string path = tmp_snn_path("rt");
    ASSERT_EQ(snn_network_save(n.net, path.c_str()), 0);
    snn_network_destroy(n.net);

    snn_network_t* ld = snn_network_load(path.c_str());
    std::remove(path.c_str());
    ASSERT_NE(ld, nullptr);
    snn_population_t* pv  = pop_named(ld, "L4_integr_PV");
    snn_population_t* som = pop_named(ld, "L4_integr_SOM");
    snn_population_t* l4  = pop_named(ld, "L4_integr_0");
    snn_population_t* l2  = pop_named(ld, "L2_pattern_1");
    snn_population_t* trn = pop_named(ld, "thalamus_reticular");
    ASSERT_TRUE(pv && som && l4 && l2 && trn);
    EXPECT_EQ(pv->subclass, SNN_NSC_PV);
    EXPECT_EQ(som->subclass, SNN_NSC_SOM);
    EXPECT_EQ(trn->subclass, SNN_NSC_TRN);
    EXPECT_EQ(l4->subclass, SNN_NSC_PYRAMIDAL_L4_STELLATE);
    EXPECT_FLOAT_EQ(pv->gap_coupling, 0.05f);
    EXPECT_EQ(l4->conduction_delay_steps, 1u);
    EXPECT_FLOAT_EQ(l4->heterogeneity_sigma, 0.10f);
    EXPECT_NEAR(ratio_of(l4), SNN_INH_REBALANCE_TARGET_RATIO, 0.01);
    EXPECT_NEAR(ratio_of(l2), SNN_INH_REBALANCE_TARGET_RATIO, 0.01);
    snn_network_destroy(ld);
}

/* R6. A balanced network survives a round trip without being scaled again;
 *     with the restore switched off PV/SOM come back pyramidal (old bug) */
TEST_F(InhRebalanceTest, SaveLoadNoDoubleScaleAndKillSwitch) {
    NamedNet n = build_named_net();
    ASSERT_NE(n.net, nullptr);
    ASSERT_EQ(snn_hierarchical_restore_pop_properties(n.net), 7);
    ASSERT_EQ(snn_network_rebalance_inhibition(n.net, SNN_INH_REBALANCE_TARGET_RATIO,
                                               SNN_INH_REBALANCE_MAX_SCALE, nullptr), 0);
    const double balanced = ratio_of(n.net->populations[n.l4]);
    ASSERT_NEAR(balanced, SNN_INH_REBALANCE_TARGET_RATIO, 0.01);
    const std::string path = tmp_snn_path("bal");
    ASSERT_EQ(snn_network_save(n.net, path.c_str()), 0);
    snn_network_destroy(n.net);

    snn_network_t* ld = snn_network_load(path.c_str());
    ASSERT_NE(ld, nullptr);
    EXPECT_NEAR(ratio_of(pop_named(ld, "L4_integr_0")), balanced, 1e-4);
    snn_network_destroy(ld);

    setenv("NIMCP_SNN_RESTORE_POP_PROPS", "0", 1);
    ld = snn_network_load(path.c_str());
    unsetenv("NIMCP_SNN_RESTORE_POP_PROPS");
    std::remove(path.c_str());
    ASSERT_NE(ld, nullptr);
    EXPECT_EQ(pop_named(ld, "L4_integr_PV")->subclass, SNN_NSC_PYRAMIDAL);
    EXPECT_EQ(pop_named(ld, "L4_integr_PV")->conduction_delay_steps, 0u);
    snn_network_destroy(ld);
}
