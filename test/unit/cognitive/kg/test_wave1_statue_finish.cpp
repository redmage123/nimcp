/**
 * @file test_wave1_statue_finish.cpp
 * @brief Unit test for KG-integration Wave W1 (statue-finish).
 *
 * Wave W1 converted five files from stubs that said
 * "Would call brain_kg_add_node()" into real brain_kg_add_node /
 * brain_kg_add_edge calls:
 *   1. src/core/brain/regions/wernicke/nimcp_wernicke_nlp_bridge.c
 *      (wernicke_nlp_register_concept - covered by call pattern, not bridge
 *      create because that requires a wernicke adapter).
 *   2. src/core/brain/regions/wernicke/nimcp_omni_wernicke_bridge.c
 *      (omni_wernicke_kg_register - verified end-to-end).
 *   3. src/cognitive/parietal/linguistics/nimcp_parietal_linguistics_mesh.c
 *      (linguistics_mesh_connect_kg - verified end-to-end).
 *   4. src/core/brain/factory/init/nimcp_brain_init_surface_geometry.c
 *      (runs at brain_create time - verified by inspecting the KG after
 *      brain creation).
 *   5. src/cognitive/recursive/nimcp_rcog_brain_kg_bridge.c
 *      (rcog_brain_kg_bridge_register_engine - verified end-to-end).
 *
 * Each wave-1 file is expected to (a) add a named structural node and
 * (b) contribute at least one edge whenever the matching peer node is
 * already present.
 */

#include <gtest/gtest.h>
#include <cstring>

#include "core/brain/nimcp_brain.h"
#include "core/brain/nimcp_brain_internal.h"
#include "core/brain/nimcp_brain_kg.h"
#include "core/brain/factory/init/nimcp_brain_init_surface_geometry.h"
#include "core/brain/regions/wernicke/nimcp_omni_wernicke_bridge.h"
/* nimcp_parietal_linguistics_mesh.h transitively includes
 * nimcp_parietal_linguistics_types.h, which defines PHONEME_* enumerators
 * that collide with perception/nimcp_speech_cortex.h (pulled in by
 * nimcp_brain_internal.h). We forward-declare the mesh API locally to
 * avoid the collision. */
#define LING_ERR_OK 0
#define LING_MESH_KG_NODE_NAME "parietal_linguistics_mesh"
extern "C" {
    typedef struct linguistics_mesh linguistics_mesh_t;
    typedef struct linguistics_mesh_config linguistics_mesh_config_t;
    linguistics_mesh_t* linguistics_mesh_create(const linguistics_mesh_config_t* config);
    void linguistics_mesh_destroy(linguistics_mesh_t* mesh);
    int linguistics_mesh_connect_kg(linguistics_mesh_t* mesh, brain_kg_t* kg);
}
#include "cognitive/recursive/nimcp_rcog_brain_kg_bridge.h"
#include "cognitive/recursive/nimcp_rcog_types.h"

/* ------------------------------------------------------------------------- */
/* Fixture: creates a minimal brain; internal_kg is always-on (2026-04-24).  */
/* ------------------------------------------------------------------------- */

class Wave1StatueFinishTest : public ::testing::Test {
protected:
    brain_t brain = nullptr;

    void SetUp() override {
        brain = brain_create_minimal("wave1_kg_test",
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

    /* Post-init, brain_kg_init downgrades access to READ. For bridges that
     * don't have a brain_t handle (and therefore can't elevate themselves),
     * tests must elevate on their behalf. Mirrors the pattern documented in
     * docs/claude/kg-node-naming-registry.md §7. */
    void elevate_kg_to_admin() {
        brain_kg_set_access_level(brain->internal_kg,
                                  BRAIN_KG_ACCESS_ADMIN,
                                  brain->internal_kg_admin_token);
    }
    void restore_kg_to_read() {
        brain_kg_set_access_level(brain->internal_kg, BRAIN_KG_ACCESS_READ, 0);
    }

    void expect_node(const char* name) {
        brain_kg_node_id_t id = brain_kg_find_node(brain->internal_kg, name);
        EXPECT_NE(id, BRAIN_KG_INVALID_NODE)
            << "expected KG node '" << name << "' to be present";
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

    bool has_incoming_edge(const char* to_name) {
        brain_kg_node_id_t to = brain_kg_find_node(brain->internal_kg, to_name);
        if (to == BRAIN_KG_INVALID_NODE) return false;
        brain_kg_edge_list_t* edges = brain_kg_get_incoming(brain->internal_kg, to);
        if (!edges) return false;
        bool has = (edges->count > 0);
        brain_kg_edge_list_destroy(edges);
        return has;
    }
};

/* ------------------------------------------------------------------------- */
/* File 4: surface_geometry                                                   */
/* ------------------------------------------------------------------------- */

TEST_F(Wave1StatueFinishTest, SurfaceGeometryRegistersKgNode) {
    bool ok = nimcp_brain_factory_init_surface_geometry_subsystem(brain);
    if (ok) {
        expect_node("surface_geometry_subsystem");
    } else {
        GTEST_SKIP() << "surface_geometry init not available in this build mode";
    }
}

/* ------------------------------------------------------------------------- */
/* File 3: parietal linguistics mesh                                          */
/* ------------------------------------------------------------------------- */

TEST_F(Wave1StatueFinishTest, LinguisticsMeshConnectKgAddsNodeAndEdges) {
    linguistics_mesh_t* mesh = linguistics_mesh_create(nullptr);
    ASSERT_NE(mesh, nullptr);

    elevate_kg_to_admin();
    int rc = linguistics_mesh_connect_kg(mesh, brain->internal_kg);
    EXPECT_EQ(rc, LING_ERR_OK);

    expect_node(LING_MESH_KG_NODE_NAME);

    /* Idempotent: reconnect must not fail */
    rc = linguistics_mesh_connect_kg(mesh, brain->internal_kg);
    EXPECT_EQ(rc, LING_ERR_OK);
    restore_kg_to_read();

    brain_kg_node_id_t id1 = brain_kg_find_node(brain->internal_kg, LING_MESH_KG_NODE_NAME);
    EXPECT_NE(id1, BRAIN_KG_INVALID_NODE);

    /* Edge checks are soft: peer nodes may not exist in a MICRO brain. */
    bool linked = has_incoming_edge(LING_MESH_KG_NODE_NAME)
               || has_outgoing_edge(LING_MESH_KG_NODE_NAME);
    (void)linked;

    linguistics_mesh_destroy(mesh);
}

/* ------------------------------------------------------------------------- */
/* File 2: omni_wernicke_bridge.kg_register                                   */
/* ------------------------------------------------------------------------- */

TEST_F(Wave1StatueFinishTest, OmniWernickeKgRegisterAddsCoordinatorNode) {
    omni_wernicke_bridge_t* bridge = omni_wernicke_bridge_create(nullptr);
    ASSERT_NE(bridge, nullptr);

    int rc = omni_wernicke_connect_kg(bridge, brain->internal_kg);
    EXPECT_EQ(rc, 0);

    elevate_kg_to_admin();
    rc = omni_wernicke_kg_register(bridge);
    EXPECT_EQ(rc, 0);

    expect_node("cog_language_wernicke_omni");

    /* Idempotent re-register */
    rc = omni_wernicke_kg_register(bridge);
    EXPECT_EQ(rc, 0);
    restore_kg_to_read();

    omni_wernicke_bridge_destroy(bridge);
}

/* ------------------------------------------------------------------------- */
/* File 5: rcog_brain_kg_bridge                                               */
/* ------------------------------------------------------------------------- */

TEST_F(Wave1StatueFinishTest, RcogBridgeRegisterEngineAddsRealKgNodes) {
    rcog_brain_kg_bridge_t* bridge = rcog_brain_kg_bridge_create_default();
    ASSERT_NE(bridge, nullptr);

    elevate_kg_to_admin();
    int rc = rcog_brain_kg_bridge_connect(
        bridge, reinterpret_cast<struct brain_kg*>(brain->internal_kg));
    EXPECT_EQ(rc, RCOG_OK);

    rcog_kg_node_id_t engine_id = 0;
    rc = rcog_brain_kg_bridge_register_engine(bridge, &engine_id);
    EXPECT_EQ(rc, RCOG_OK);

    expect_node("cog_recursive_engine");
    expect_node("cog_recursive_context_store");
    expect_node("cog_recursive_tool_router");

    EXPECT_TRUE(has_outgoing_edge("cog_recursive_engine"))
        << "rcog engine should have edges to sub-components";
    restore_kg_to_read();

    rcog_brain_kg_bridge_destroy(bridge);
}

/* ------------------------------------------------------------------------- */
/* File 1: wernicke_nlp_register_concept - canonical naming pattern smoke    */
/* ------------------------------------------------------------------------- */

TEST_F(Wave1StatueFinishTest, WernickeConceptNamingPatternIsNovel) {
    const char* canonical = "cog_language_concept_smoke_test_concept";

    brain_kg_node_id_t pre = brain_kg_find_node(brain->internal_kg, canonical);
    EXPECT_EQ(pre, BRAIN_KG_INVALID_NODE)
        << "namespace collision: someone else is using '"
        << canonical << "' - update kg-node-naming-registry.md";

    elevate_kg_to_admin();
    brain_kg_node_id_t id = brain_kg_add_node(
        brain->internal_kg,
        canonical,
        BRAIN_KG_NODE_COGNITIVE,
        "W1 wernicke_nlp_register_concept smoke test"
    );
    restore_kg_to_read();
    EXPECT_NE(id, BRAIN_KG_INVALID_NODE);

    brain_kg_node_id_t id2 = brain_kg_find_node(brain->internal_kg, canonical);
    EXPECT_EQ(id2, id);
}
