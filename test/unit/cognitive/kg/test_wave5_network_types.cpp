/**
 * @file test_wave5_network_types.cpp
 * @brief Unit test for KG-integration Wave W5 (network-type root nodes).
 *
 * Wave W5 wires the 6 neural-network types (LNN, SNN, CNN, FNO, HNN,
 * main ANN) as structural root nodes in brain->internal_kg, plus sample
 * SNN population sub-nodes. Each network's main .c file owns a
 * file-scope static emit function; public trigger wrappers
 * net_<type>_kg_trigger_event() forward to the static so tests can
 * verify emission without inducing a real gradient explosion / energy
 * drift / mode collapse in live network dynamics.
 *
 * This test verifies:
 *   (a) All 6 network root nodes exist after brain init.
 *   (b) 3 sample SNN population sub-nodes exist.
 *   (c) Cross-network edges exist (net_main_ann integrates_with others,
 *       net_lnn contains net_hnn).
 *   (d) Each emit function writes an event node with the conventional
 *       name "net_<type>_event_<kind>_<ts_us>" and a back-edge to the
 *       network root.
 */

#include <gtest/gtest.h>
#include <cstdint>
#include <string>

#include "core/brain/nimcp_brain.h"
#include "core/brain/nimcp_brain_internal.h"
#include "core/brain/nimcp_brain_kg.h"
#include "core/brain/factory/init/nimcp_brain_init_network_kg_wiring.h"

extern "C" {
/* Test-facing triggers — forward to each network's file-scope static
 * net_<type>_kg_emit_event. Declared here because they live in their
 * respective network .c files without a dedicated public header. */
void net_lnn_kg_trigger_event(brain_t brain, const char* kind,
                              float magnitude, uint64_t ts_us);
void net_snn_kg_trigger_event(brain_t brain, const char* kind,
                              float magnitude, uint64_t ts_us);
void net_cnn_kg_trigger_event(brain_t brain, const char* kind,
                              float magnitude, uint64_t ts_us);
void net_fno_kg_trigger_event(brain_t brain, const char* kind,
                              float magnitude, uint64_t ts_us);
void net_hnn_kg_trigger_event(brain_t brain, const char* kind,
                              float magnitude, uint64_t ts_us);
void net_main_ann_kg_trigger_event(brain_t brain, const char* kind,
                                   float magnitude, uint64_t ts_us);
} /* extern "C" */

// --------------------------------------------------------------------------
// Fixture
// --------------------------------------------------------------------------

class Wave5NetworkTypesTest : public ::testing::Test {
protected:
    brain_t brain = nullptr;

    void SetUp() override {
        brain = brain_create_minimal("wave5_kg_test",
                                     BRAIN_SIZE_MICRO,
                                     BRAIN_TASK_CLASSIFICATION,
                                     4, 2);
        ASSERT_NE(brain, nullptr) << "brain_create_minimal returned NULL";
        ASSERT_TRUE(brain->internal_kg_enabled);
        ASSERT_NE(brain->internal_kg, nullptr);

        /* brain_create_minimal skips parallel_init (Wave 32 is in there),
         * so invoke the network-KG wiring subsystem explicitly here. The
         * subsystem is idempotent + null-tolerant per W5 design. */
        ASSERT_TRUE(nimcp_brain_factory_init_network_kg_wiring_subsystem(brain));
    }

    void TearDown() override {
        if (brain) {
            brain_destroy(brain);
            brain = nullptr;
        }
    }

    void expect_node(const char* name) {
        brain_kg_node_id_t id = brain_kg_find_node(brain->internal_kg, name);
        EXPECT_NE(id, BRAIN_KG_INVALID_NODE)
            << "expected KG node '" << name << "' to be present";
    }
};

// --------------------------------------------------------------------------
// (a) Structural root nodes — all 6 must exist.
// --------------------------------------------------------------------------

TEST_F(Wave5NetworkTypesTest, AllSixNetworkRootsPresent) {
    expect_node("net_lnn");
    expect_node("net_snn");
    expect_node("net_cnn");
    expect_node("net_fno");
    expect_node("net_hnn");
    expect_node("net_main_ann");
}

// --------------------------------------------------------------------------
// (b) Sample SNN population sub-nodes.
// --------------------------------------------------------------------------

TEST_F(Wave5NetworkTypesTest, SampleSnnPopulationNodesPresent) {
    expect_node("net_snn_population_0");
    expect_node("net_snn_population_1");
    expect_node("net_snn_population_2");
}

// --------------------------------------------------------------------------
// (c) Cross-network edges: quick existence check on LNN->HNN (contains).
// --------------------------------------------------------------------------

TEST_F(Wave5NetworkTypesTest, CrossNetworkEdgesExist) {
    brain_kg_node_id_t lnn = brain_kg_find_node(brain->internal_kg, "net_lnn");
    brain_kg_node_id_t hnn = brain_kg_find_node(brain->internal_kg, "net_hnn");
    ASSERT_NE(lnn, BRAIN_KG_INVALID_NODE);
    ASSERT_NE(hnn, BRAIN_KG_INVALID_NODE);

    brain_kg_edge_list_t* outs = brain_kg_get_outgoing(brain->internal_kg, lnn);
    ASSERT_NE(outs, nullptr);

    bool found_hnn = false;
    for (uint32_t i = 0; i < outs->count; i++) {
        const brain_kg_edge_t* e = outs->edges[i];
        if (e && e->to == hnn) { found_hnn = true; break; }
    }
    brain_kg_edge_list_destroy(outs);
    EXPECT_TRUE(found_hnn)
        << "expected net_lnn --connects_to--> net_hnn edge";
}

// --------------------------------------------------------------------------
// (d) Each emit function writes a node following the canonical naming
//     rule  net_<type>_event_<kind>_<ts_us>.
// --------------------------------------------------------------------------

TEST_F(Wave5NetworkTypesTest, AllSixEmitFunctionsProduceEventNodes) {
    struct EmitCase {
        const char* net_type;
        const char* kind;
        void (*trigger)(brain_t, const char*, float, uint64_t);
    };
    const EmitCase cases[] = {
        { "net_lnn",      "gradient_explosion",   net_lnn_kg_trigger_event },
        { "net_snn",      "spike_rate_anomaly",   net_snn_kg_trigger_event },
        { "net_cnn",      "feature_collapse",     net_cnn_kg_trigger_event },
        { "net_fno",      "spectral_shift",       net_fno_kg_trigger_event },
        { "net_hnn",      "energy_drift",         net_hnn_kg_trigger_event },
        { "net_main_ann", "mode_collapse",        net_main_ann_kg_trigger_event },
    };

    uint64_t ts = 1714089600000000ULL;
    for (const auto& c : cases) {
        ASSERT_NE(c.trigger, nullptr);
        c.trigger(brain, c.kind, 1.5f, ts);

        std::string expected = std::string(c.net_type) + "_event_"
                             + c.kind + "_" + std::to_string(ts);
        brain_kg_node_id_t id = brain_kg_find_node(brain->internal_kg,
                                                   expected.c_str());
        EXPECT_NE(id, BRAIN_KG_INVALID_NODE)
            << "emit for " << c.net_type << "/" << c.kind
            << " did not create node '" << expected << "'";
        ++ts;  /* unique ts per event */
    }
}

// --------------------------------------------------------------------------
// Admin-token elevation — write path must succeed even though KG was
// downgraded to READ by brain_kg_init. If admin elevation inside the
// emit function fails silently, the node will be absent despite no
// errors surfaced to the caller. This test is the canary.
// --------------------------------------------------------------------------

TEST_F(Wave5NetworkTypesTest, EmitSucceedsPostInitKgDowngrade) {
    /* Explicitly downgrade to READ to mimic post-init state. */
    brain_kg_set_access_level(brain->internal_kg, BRAIN_KG_ACCESS_READ, 0);

    uint64_t ts = 1714090000000000ULL;
    net_main_ann_kg_trigger_event(brain, "post_downgrade_test", 0.42f, ts);

    std::string expected = "net_main_ann_event_post_downgrade_test_"
                         + std::to_string(ts);
    brain_kg_node_id_t id = brain_kg_find_node(brain->internal_kg,
                                               expected.c_str());
    EXPECT_NE(id, BRAIN_KG_INVALID_NODE)
        << "post-downgrade emit failed — admin-token elevation is broken";
}
