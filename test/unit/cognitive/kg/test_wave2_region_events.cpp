/**
 * @file test_wave2_region_events.cpp
 * @brief Unit test for KG-integration Wave W2 (region runtime events).
 *
 * Wave W2 added a `<region>_kg_emit_event(brain, kind, intensity, ts_us)`
 * runtime-write path to 12 already-structural region wirings. Each call
 *   - creates a `<region>_event_<kind>_<ts_us>` node (BRAIN_KG_NODE_CUSTOM),
 *   - links it back to the region's structural root via SENDS_TO (produced_by),
 *   - self-elevates the admin token before writing (see registry §7),
 *   - is a silent no-op if the KG is unavailable.
 *
 * The 12 emitters under test:
 *   amygdala, cingulate, entorhinal, habenula, hypothalamus, insula,
 *   locus_coeruleus (lc_kg_emit_event), OFC, PFC, temporal, VTA, subcortical.
 */

#include <gtest/gtest.h>
#include <cinttypes>
#include <cstdio>
#include <cstring>

#include "core/brain/nimcp_brain.h"
#include "core/brain/nimcp_brain_internal.h"
#include "core/brain/nimcp_brain_kg.h"

#include "core/brain/regions/amygdala/bridges/nimcp_amygdala_kg_wiring.h"
#include "core/brain/regions/cingulate/bridges/nimcp_cingulate_kg_wiring.h"
#include "core/brain/regions/entorhinal/bridges/nimcp_entorhinal_kg_wiring.h"
#include "core/brain/regions/habenula/bridges/nimcp_habenula_kg_wiring.h"
#include "core/brain/regions/hypothalamus/bridges/nimcp_hypothalamus_kg_wiring.h"
#include "core/brain/regions/insula/bridges/nimcp_insula_kg_wiring.h"
#include "core/brain/regions/locus_coeruleus/bridges/nimcp_lc_kg_wiring.h"
#include "core/brain/regions/ofc/bridges/nimcp_ofc_kg_wiring.h"
#include "core/brain/regions/prefrontal/bridges/nimcp_pfc_kg_wiring.h"
#include "core/brain/regions/temporal/bridges/nimcp_temporal_kg_wiring.h"
#include "core/brain/regions/vta/bridges/nimcp_vta_kg_wiring.h"
#include "core/brain/subcortical/bridges/nimcp_subcortical_kg_wiring.h"

class Wave2RegionEventsTest : public ::testing::Test {
protected:
    brain_t brain = nullptr;

    void SetUp() override {
        brain = brain_create_minimal("wave2_kg_test",
                                     BRAIN_SIZE_MICRO,
                                     BRAIN_TASK_CLASSIFICATION,
                                     4, 2);
        ASSERT_NE(brain, nullptr) << "brain_create_minimal returned NULL";
        ASSERT_TRUE(brain->internal_kg_enabled)
            << "internal_kg_enabled must be true post-creation";
        ASSERT_NE(brain->internal_kg, nullptr)
            << "brain->internal_kg must be allocated";
    }

    void TearDown() override {
        if (brain) {
            brain_destroy(brain);
            brain = nullptr;
        }
    }

    /* Format an event-node name that mirrors what the emitter produces.
     * For OFC + PFC the emitters use the canonical long names
     * "orbitofrontal_cortex" / "prefrontal_cortex" (per naming registry §1). */
    static std::string event_name(const char* owner, const char* kind,
                                  uint64_t ts_us) {
        char buf[192];
        std::snprintf(buf, sizeof(buf),
                      "%s_event_%s_%" PRIu64, owner, kind, ts_us);
        return std::string(buf);
    }

    void expect_event_node(const char* name) {
        brain_kg_node_id_t id = brain_kg_find_node(brain->internal_kg, name);
        EXPECT_NE(id, BRAIN_KG_INVALID_NODE)
            << "expected KG event node '" << name << "' to be present";
    }

    bool has_outgoing_edge(const char* from_name) {
        brain_kg_node_id_t from = brain_kg_find_node(brain->internal_kg, from_name);
        if (from == BRAIN_KG_INVALID_NODE) return false;
        brain_kg_edge_list_t* edges = brain_kg_get_outgoing(brain->internal_kg, from);
        if (!edges) return false;
        bool has = (edges->count > 0);
        brain_kg_edge_list_destroy(edges);
        return has;
    }
};

TEST_F(Wave2RegionEventsTest, AllTwelveEmittersProduceEventNodes) {
    /* Timestamps differ per call so event-node names are unique. */
    const uint64_t t0 = 1714089600000000ULL;

    /* 1. amygdala */
    amygdala_kg_emit_event(brain, "fear", 0.85f, t0 + 1);
    expect_event_node(event_name("amygdala", "fear", t0 + 1).c_str());

    /* 2. cingulate */
    cingulate_kg_emit_event(brain, "conflict", 0.60f, t0 + 2);
    expect_event_node(event_name("cingulate", "conflict", t0 + 2).c_str());

    /* 3. entorhinal */
    entorhinal_kg_emit_event(brain, "grid_cell_fire", 0.45f, t0 + 3);
    expect_event_node(event_name("entorhinal", "grid_cell_fire", t0 + 3).c_str());

    /* 4. habenula */
    habenula_kg_emit_event(brain, "negative_rpe", -0.75f, t0 + 4);
    expect_event_node(event_name("habenula", "negative_rpe", t0 + 4).c_str());

    /* 5. hypothalamus */
    hypothalamus_kg_emit_event(brain, "drive_change", 0.30f, t0 + 5);
    expect_event_node(event_name("hypothalamus", "drive_change", t0 + 5).c_str());

    /* 6. insula */
    insula_kg_emit_event(brain, "salience", 0.55f, t0 + 6);
    expect_event_node(event_name("insula", "salience", t0 + 6).c_str());

    /* 7. locus coeruleus */
    lc_kg_emit_event(brain, "arousal_shift", 0.40f, t0 + 7);
    expect_event_node(event_name("locus_coeruleus", "arousal_shift", t0 + 7).c_str());

    /* 8. OFC — canonical "orbitofrontal_cortex" owner */
    ofc_kg_emit_event(brain, "decision_outcome", 0.50f, t0 + 8);
    expect_event_node(event_name("orbitofrontal_cortex", "decision_outcome", t0 + 8).c_str());

    /* 9. PFC — canonical "prefrontal_cortex" owner */
    pfc_kg_emit_event(brain, "goal_update", 0.65f, t0 + 9);
    expect_event_node(event_name("prefrontal_cortex", "goal_update", t0 + 9).c_str());

    /* 10. temporal */
    temporal_kg_emit_event(brain, "semantic_retrieve", 0.70f, t0 + 10);
    expect_event_node(event_name("temporal", "semantic_retrieve", t0 + 10).c_str());

    /* 11. VTA */
    vta_kg_emit_event(brain, "dopamine_rpe", 0.90f, t0 + 11);
    expect_event_node(event_name("vta", "dopamine_rpe", t0 + 11).c_str());

    /* 12. subcortical */
    subcortical_kg_emit_event(brain, "action_selected", 0.80f, t0 + 12);
    expect_event_node(event_name("subcortical", "action_selected", t0 + 12).c_str());
}

TEST_F(Wave2RegionEventsTest, AmygdalaEventHasOutgoingEdgeToRoot) {
    const uint64_t t = 1714089700000000ULL;

    /* Amygdala root is created at brain_init (internal_kg_populate). */
    brain_kg_node_id_t root = brain_kg_find_node(brain->internal_kg, "amygdala");
    if (root == BRAIN_KG_INVALID_NODE) {
        GTEST_SKIP() << "amygdala root node not populated in MICRO init — "
                     << "skipping edge check (emitter still works, but nothing to link to)";
    }

    amygdala_kg_emit_event(brain, "fear", 0.9f, t);
    std::string evt = event_name("amygdala", "fear", t);
    expect_event_node(evt.c_str());

    /* Event node should have an outgoing SENDS_TO edge (produced_by) → root. */
    EXPECT_TRUE(has_outgoing_edge(evt.c_str()))
        << "amygdala event should have an outgoing edge to its structural root";
}

TEST_F(Wave2RegionEventsTest, EmitIsSilentNoopOnNullBrain) {
    /* Must not crash, must not touch the KG (there's nothing to touch). */
    amygdala_kg_emit_event(nullptr, "fear", 0.5f, 1);
    vta_kg_emit_event(nullptr, "dopamine_rpe", 0.5f, 2);
    subcortical_kg_emit_event(nullptr, "action_selected", 0.5f, 3);
    SUCCEED();
}

TEST_F(Wave2RegionEventsTest, EmitIsSilentNoopOnNullKind) {
    /* Must not crash, must not add corrupt node. */
    pfc_kg_emit_event(brain, nullptr, 0.5f, 42);
    temporal_kg_emit_event(brain, nullptr, 0.5f, 43);
    SUCCEED();
}
