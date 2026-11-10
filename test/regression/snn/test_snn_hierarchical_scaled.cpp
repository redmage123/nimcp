/**
 * @file test_snn_hierarchical_scaled.cpp
 * @brief Regression tests for target-scaled hierarchical SNN builds (2026-11-10)
 *
 * Bug: hierarchical_build ignored target_total_neurons and always built the
 * 1.8M-neuron production table, so brain.enable_multi_network() on a small
 * test brain needed >45 GB (scripts/overfit_probe.py was SIGKILLed on a
 * 62 GB host). A target below the table total now scales every table-sized
 * population, raises connectivity of shrunk sources to keep per-neuron
 * fan-in, keeps production weights, and never touches the cached sidecar.
 */

#include <gtest/gtest.h>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <sys/stat.h>
#include <unistd.h>

extern "C" {
#include "snn/nimcp_snn_types.h"
#include "snn/nimcp_snn_network.h"
#include "snn/nimcp_snn_synapse.h"
}

namespace {

constexpr uint32_t kIn = 1024;
constexpr uint32_t kOut = 4096;
constexpr uint32_t kTarget = 50000;

/* Production per-population sizes and connectivities used below
 * (TIER_DEFS / INH_* in src/snn/nimcp_snn_hierarchical.c). */
constexpr uint32_t kInputPopOrig = 20000;   /* tier 0 "input" */
constexpr float    kInputFfConn = 0.005f;   /* tier 0 → tier 1 */
constexpr uint32_t kL2PopOrig = 30000;      /* tier 2 "L2_pattern" */
constexpr uint32_t kL2PvOrig = 3600;        /* 0.12 × 30000 */
constexpr float    kInhToPyrConn = 0.005f;
constexpr uint32_t kTrnOrig = 10000;

snn_population_t* pop_named(snn_network_t* net, const char* name, uint32_t* idx = nullptr) {
    for (uint32_t p = 0; p < net->n_populations; p++) {
        snn_population_t* pop = net->populations[p];
        if (pop && std::strcmp(pop->name, name) == 0) {
            if (idx) *idx = p;
            return pop;
        }
    }
    return nullptr;
}

/* "<tier>_<digits>" for the 8 tier names */
bool is_tier_pyr(const char* name) {
    static const char* kTiers[] = {"input", "L1_feature", "L2_pattern", "L3_concept",
                                   "L4_integr", "L5_exec", "L6_project", "output"};
    for (const char* t : kTiers) {
        size_t n = std::strlen(t);
        if (std::strncmp(name, t, n) == 0 && name[n] == '_' && name[n + 1] != '\0') {
            const char* c = name + n + 1;
            while (*c >= '0' && *c <= '9') c++;
            if (*c == '\0') return true;
        }
    }
    return false;
}

/* Mean incoming synapses per destination neuron from src_pop, and their mean weight */
void fanin_from(snn_population_t* dst, uint32_t src_pop, double* per_neuron, double* mean_w) {
    snn_csr_storage_t* c = dst->incoming_csr;
    ASSERT_NE(c, nullptr);
    uint64_t n = 0;
    double w = 0.0;
    for (uint32_t k = 0; k < c->n_synapses; k++) {
        if (c->entries[k].src_pop == src_pop) {
            n++;
            w += c->entries[k].weight;
        }
    }
    *per_neuron = (double)n / (double)dst->n_neurons;
    *mean_w = n ? w / (double)n : 0.0;
}

class ScaledHierTest : public ::testing::Test {
protected:
    static snn_network_t* net;   /* shared kTarget build (tests 1-3) */
    static void SetUpTestSuite() {
        net = snn_create_hierarchical_network(kIn, kOut, kTarget);
    }
    static void TearDownTestSuite() {
        if (net) snn_network_destroy(net);
        net = nullptr;
    }
};
snn_network_t* ScaledHierTest::net = nullptr;

}  // namespace

/* 1. A small target builds a small, complete, correctly tagged hierarchy */
TEST_F(ScaledHierTest, SmallTargetBuildsSmallCompleteHierarchy) {
    ASSERT_NE(net, nullptr);
    EXPECT_TRUE(snn_hierarchical_has_tiers(net));
    uint64_t tier_neurons = 0;
    uint32_t tier_pops = 0, pv = 0, som = 0, vip = 0, trn = 0;
    for (uint32_t p = 0; p < net->n_populations; p++) {
        snn_population_t* pop = net->populations[p];
        if (!pop) continue;
        if (is_tier_pyr(pop->name)) { tier_pops++; tier_neurons += pop->n_neurons; }
        if (pop->subclass == SNN_NSC_PV)  pv++;
        if (pop->subclass == SNN_NSC_SOM) som++;
        if (pop->subclass == SNN_NSC_VIP) vip++;
        if (pop->subclass == SNN_NSC_TRN) trn++;
    }
    EXPECT_EQ(tier_pops, 46u);
    EXPECT_NEAR((double)tier_neurons, (double)kTarget, 0.10 * kTarget);
    EXPECT_EQ(pv, 5u);
    EXPECT_EQ(som, 5u);
    EXPECT_EQ(vip, 5u);
    EXPECT_EQ(trn, 1u);
    snn_population_t* l2 = pop_named(net, "L2_pattern_0");
    ASSERT_NE(l2, nullptr);
    EXPECT_EQ(l2->n_neurons, snn_hierarchical_scaled_pop_size(kL2PopOrig, kTarget));
    EXPECT_LT(l2->n_neurons, kL2PopOrig);
    snn_population_t* t = pop_named(net, "thalamus_reticular");
    ASSERT_NE(t, nullptr);
    EXPECT_EQ(t->n_neurons, snn_hierarchical_scaled_pop_size(kTrnOrig, kTarget));
}

/* 2. Per-neuron fan-in stays at the production value n_orig · conn */
TEST_F(ScaledHierTest, PreservesProductionFanIn) {
    ASSERT_NE(net, nullptr);
    uint32_t src = 0, pv_idx = 0;
    ASSERT_NE(pop_named(net, "input_0", &src), nullptr);
    snn_population_t* l1 = pop_named(net, "L1_feature_0");
    ASSERT_NE(l1, nullptr);
    double fan = 0.0, w = 0.0;
    fanin_from(l1, src, &fan, &w);
    const double expect_ff = (double)kInputPopOrig * kInputFfConn;   /* 100 */
    EXPECT_NEAR(fan, expect_ff, 0.15 * expect_ff) << "input_0 → L1_feature_0";

    ASSERT_NE(pop_named(net, "L2_pattern_PV", &pv_idx), nullptr);
    snn_population_t* l2 = pop_named(net, "L2_pattern_0");
    ASSERT_NE(l2, nullptr);
    fanin_from(l2, pv_idx, &fan, &w);
    const double expect_pv = (double)kL2PvOrig * kInhToPyrConn;     /* 18 */
    EXPECT_NEAR(fan, expect_pv, 0.15 * expect_pv) << "L2_pattern_PV → L2_pattern_0";
    EXPECT_LT(w, 0.0);
}

/* 3. Synaptic weights do not depend on the scale (production formula uses
 *    n_orig · conn, which scaling preserves). input→L1 is used because L1
 *    has no PV/SOM, so the load-time inhibition rebalance never touches it. */
TEST_F(ScaledHierTest, WeightsIndependentOfScale) {
    ASSERT_NE(net, nullptr);
    snn_network_t* other = snn_create_hierarchical_network(kIn, kOut, 30000);
    ASSERT_NE(other, nullptr);
    double w_a = 0.0, w_b = 0.0, fan = 0.0;
    uint32_t src_a = 0, src_b = 0;
    ASSERT_NE(pop_named(net, "input_0", &src_a), nullptr);
    ASSERT_NE(pop_named(other, "input_0", &src_b), nullptr);
    fanin_from(pop_named(net, "L1_feature_0"), src_a, &fan, &w_a);
    fanin_from(pop_named(other, "L1_feature_0"), src_b, &fan, &w_b);
    ASSERT_GT(w_a, 0.0);
    EXPECT_NEAR(w_b, w_a, 0.05 * w_a);
    EXPECT_NE(pop_named(other, "L1_feature_0")->n_neurons,
              pop_named(net, "L1_feature_0")->n_neurons);   /* really a different scale */
    snn_network_destroy(other);
}

/* 4. No scaling at or above the table total (production), checked on the
 *    size helper the builder uses — a full build needs >45 GB. */
TEST(ScaledHierSizes, ProductionTargetsAreUnscaled) {
    const uint32_t table = snn_hierarchical_table_total();
    EXPECT_EQ(table, 1800000u);
    for (uint32_t target : {0u, table, table + 1u, 5000000u}) {
        EXPECT_EQ(snn_hierarchical_scaled_pop_size(kL2PopOrig, target), kL2PopOrig) << target;
        EXPECT_EQ(snn_hierarchical_scaled_pop_size(kTrnOrig, target), kTrnOrig) << target;
    }
    EXPECT_EQ(snn_hierarchical_scaled_pop_size(kL2PopOrig, table / 2), kL2PopOrig / 2);
    EXPECT_EQ(snn_hierarchical_scaled_pop_size(100, 1000), 32u);   /* floor */
}

/* 5. A scaled build neither loads nor overwrites the cached sidecar */
TEST(ScaledHierCache, ScaledBuildIgnoresAndPreservesSidecar) {
    char tmpl[] = "/tmp/nimcp_hier_scaled_XXXXXX";
    char* dir = mkdtemp(tmpl);
    ASSERT_NE(dir, nullptr);
    char cwd[4096];
    ASSERT_NE(getcwd(cwd, sizeof(cwd)), nullptr);
    ASSERT_EQ(chdir(dir), 0);
    ASSERT_EQ(mkdir("checkpoints", 0755), 0);
    ASSERT_EQ(mkdir("checkpoints/athena", 0755), 0);
    const char* side = "checkpoints/athena/athena_immersive.bin.snn";

    /* Cache a valid tiered network with matching dims at a different scale */
    snn_network_t* cached = snn_create_hierarchical_network(kIn, kOut, 30000);
    ASSERT_NE(cached, nullptr);
    struct stat st0;
    EXPECT_NE(stat(side, &st0), 0) << "a scaled build must not write the sidecar";
    const uint32_t cached_l2 = pop_named(cached, "L2_pattern_0")->n_neurons;
    ASSERT_EQ(snn_network_save(cached, side), 0);
    snn_network_destroy(cached);
    ASSERT_EQ(stat(side, &st0), 0);

    snn_network_t* built = snn_create_hierarchical_network(kIn, kOut, kTarget);
    ASSERT_NE(built, nullptr);
    const uint32_t built_l2 = pop_named(built, "L2_pattern_0")->n_neurons;
    EXPECT_EQ(built_l2, snn_hierarchical_scaled_pop_size(kL2PopOrig, kTarget));
    EXPECT_NE(built_l2, cached_l2) << "scaled build loaded the cached sidecar";
    snn_network_destroy(built);

    struct stat st1;
    ASSERT_EQ(stat(side, &st1), 0);
    EXPECT_EQ(st1.st_size, st0.st_size);
    EXPECT_EQ(st1.st_mtime, st0.st_mtime);

    std::remove(side);
    rmdir("checkpoints/athena");
    rmdir("checkpoints");
    ASSERT_EQ(chdir(cwd), 0);
    rmdir(dir);
}

/* 6. A tiny target still builds, with every table population at the floor or above */
TEST(ScaledHierFloor, TinyTargetRespectsFloor) {
    snn_network_t* net = snn_create_hierarchical_network(kIn, kOut, 5000);
    ASSERT_NE(net, nullptr);
    EXPECT_TRUE(snn_hierarchical_has_tiers(net));
    for (uint32_t p = 0; p < net->n_populations; p++) {
        snn_population_t* pop = net->populations[p];
        if (!pop) continue;
        if (is_tier_pyr(pop->name) || pop->subclass == SNN_NSC_PV ||
            pop->subclass == SNN_NSC_SOM || pop->subclass == SNN_NSC_VIP ||
            pop->subclass == SNN_NSC_TRN) {
            EXPECT_GE(pop->n_neurons, 32u) << pop->name;
        }
    }
    snn_network_destroy(net);
}
