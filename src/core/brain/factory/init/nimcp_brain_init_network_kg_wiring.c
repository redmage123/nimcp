//=============================================================================
// nimcp_brain_init_network_kg_wiring.c - Wave W5 Network-Level KG Wiring
//=============================================================================
/**
 * @file nimcp_brain_init_network_kg_wiring.c
 * @brief Register the 6 network-type root nodes into brain->internal_kg.
 *
 * Creates:
 *   - net_lnn       (Liquid Neural Network)
 *   - net_snn       (Spiking Neural Network)
 *   - net_cnn       (Convolutional Neural Network — per-cortex processors)
 *   - net_fno       (Fourier Neural Operator)
 *   - net_hnn       (Hamiltonian Neural Network, lives inside LNN namespace)
 *   - net_main_ann  (Main adaptive ANN — the decision network)
 *   - net_snn_population_0..2 (sample subsets — first 3 populations only,
 *                              not all of them; prevents per-neuron firehose)
 *
 * Cross-net integration edges: net_main_ann --integrates_with--> each other
 * network. LNN --contains--> HNN (HNN lives inside LNN namespace).
 *
 * Also registers each network's file-scope brain backpointer via
 * per-network public setters (net_*_kg_register_brain) so the
 * file-scope static emit functions can self-elevate inside hot paths.
 *
 * Wave 32 of parallel init. See kg-node-naming-registry.md §3 + §7.
 */

#include "core/brain/factory/init/nimcp_brain_init_network_kg_wiring.h"

#include <stdio.h>
#include <string.h>

#include "core/brain/nimcp_brain_internal.h"
#include "core/brain/nimcp_brain_kg.h"
#include "utils/logging/nimcp_logging.h"
#include "utils/exception/nimcp_exception_macros.h"
#include "utils/fault_tolerance/nimcp_health_agent_macros.h"
#include "utils/bridge/nimcp_bridge_boilerplate.h"
#include "mesh/nimcp_mesh_participant.h"
#include "mesh/nimcp_mesh_adapter.h"

BRIDGE_BOILERPLATE_MESH_ONLY(brain_init_network_kg_wiring, MESH_ADAPTER_CATEGORY_SYSTEM)

/* ---------------------------------------------------------------------------
 * Per-network brain-backpointer setters.
 * Each network .c owns a file-scope `static brain_t s_<net>_kg_brain` and
 * exposes one setter so the structural init can hand over the brain handle
 * once. The emit functions are file-scope static inside the network .c and
 * read the backpointer from the same TU. This keeps `brain_t` ownership
 * explicit without leaking the full `brain_struct` across every network
 * compilation unit.
 * ------------------------------------------------------------------------- */
extern void net_lnn_kg_register_brain(brain_t brain);
extern void net_snn_kg_register_brain(brain_t brain);
extern void net_cnn_kg_register_brain(brain_t brain);
extern void net_fno_kg_register_brain(brain_t brain);
extern void net_hnn_kg_register_brain(brain_t brain);
extern void net_main_ann_kg_register_brain(brain_t brain);

/* ---------------------------------------------------------------------------
 * Helpers
 * ------------------------------------------------------------------------- */

static int net_kg_is_usable(brain_t brain) {
    return (brain && brain->internal_kg_enabled && brain->internal_kg);
}

static brain_kg_node_id_t net_kg_ensure_node(brain_kg_t* kg, const char* name,
                                             brain_kg_node_type_t type,
                                             const char* desc) {
    brain_kg_node_id_t id = brain_kg_find_node(kg, name);
    if (id != BRAIN_KG_INVALID_NODE) return id;
    return brain_kg_add_node(kg, name, type, desc);
}

/* ---------------------------------------------------------------------------
 * Structural wiring
 * ------------------------------------------------------------------------- */

bool nimcp_brain_factory_init_network_kg_wiring_subsystem(brain_t brain) {
    if (!brain) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
            "nimcp_brain_factory_init_network_kg_wiring_subsystem: brain is NULL");
        return false;
    }

    /* Register brain backpointers unconditionally — emit fns will re-guard
     * internal_kg_enabled at use time, so this is safe even when KG is off. */
    net_lnn_kg_register_brain(brain);
    net_snn_kg_register_brain(brain);
    net_cnn_kg_register_brain(brain);
    net_fno_kg_register_brain(brain);
    net_hnn_kg_register_brain(brain);
    net_main_ann_kg_register_brain(brain);

    if (!net_kg_is_usable(brain)) {
        fprintf(stderr, "[W5_NET_KG] internal_kg disabled/missing - "
                "structural wiring skipped (backpointers registered)\n");
        return true;  /* null-tolerant */
    }

    fprintf(stderr, "[W5_NET_KG] Wiring W5 network structural roots...\n");

    brain_kg_t* kg = brain->internal_kg;
    brain_kg_set_access_level(kg, BRAIN_KG_ACCESS_ADMIN,
                              brain->internal_kg_admin_token);

    /* ---- 6 network root nodes ---- */
    struct {
        const char* name;
        brain_kg_node_type_t type;
        const char* desc;
    } roots[] = {
        { "net_lnn",      BRAIN_KG_NODE_CORE,
            "Liquid Neural Network - temporal/ODE layer, per-layer tau and recurrent W" },
        { "net_snn",      BRAIN_KG_NODE_CORE,
            "Spiking Neural Network - populations, STDP, Poisson inputs, R-STDP" },
        { "net_cnn",      BRAIN_KG_NODE_CORE,
            "Per-cortex CNN processors - visual / audio / speech / somatosensory" },
        { "net_fno",      BRAIN_KG_NODE_CORE,
            "Fourier Neural Operator - spectral convolution, mode truncation" },
        { "net_hnn",      BRAIN_KG_NODE_CORE,
            "Hamiltonian Neural Network - energy-conserving symplectic dynamics" },
        { "net_main_ann", BRAIN_KG_NODE_CORE,
            "Main adaptive ANN - core decision network (brain_decide output)" },
    };
    const size_t n_roots = sizeof(roots) / sizeof(roots[0]);

    brain_kg_node_id_t root_ids[sizeof(roots) / sizeof(roots[0])];
    for (size_t i = 0; i < n_roots; ++i) {
        root_ids[i] = net_kg_ensure_node(kg, roots[i].name, roots[i].type, roots[i].desc);
    }

    /* ---- Cross-network integration edges ---- */
    /* net_main_ann integrates_with all other networks. */
    brain_kg_node_id_t main_ann = brain_kg_find_node(kg, "net_main_ann");
    if (main_ann != BRAIN_KG_INVALID_NODE) {
        for (size_t i = 0; i < n_roots; ++i) {
            if (root_ids[i] == BRAIN_KG_INVALID_NODE) continue;
            if (strcmp(roots[i].name, "net_main_ann") == 0) continue;
            brain_kg_add_edge(kg, main_ann, root_ids[i],
                BRAIN_KG_EDGE_INTEGRATES_WITH,
                "fused/gated at decision time", 0.8f);
        }
    }

    /* LNN contains HNN (Hamiltonian lives inside lnn/nimcp_lnn_hamiltonian.c) */
    brain_kg_node_id_t lnn_id = brain_kg_find_node(kg, "net_lnn");
    brain_kg_node_id_t hnn_id = brain_kg_find_node(kg, "net_hnn");
    if (lnn_id != BRAIN_KG_INVALID_NODE && hnn_id != BRAIN_KG_INVALID_NODE) {
        brain_kg_add_edge(kg, lnn_id, hnn_id,
            BRAIN_KG_EDGE_CONNECTS_TO,
            "HNN = energy-conserving LNN variant", 1.0f);
    }

    /* SNN has FNO population variant (see src/snn/nimcp_snn_fno.c) */
    brain_kg_node_id_t snn_id = brain_kg_find_node(kg, "net_snn");
    brain_kg_node_id_t fno_id = brain_kg_find_node(kg, "net_fno");
    if (snn_id != BRAIN_KG_INVALID_NODE && fno_id != BRAIN_KG_INVALID_NODE) {
        brain_kg_add_edge(kg, snn_id, fno_id,
            BRAIN_KG_EDGE_INTEGRATES_WITH,
            "SNN population with FNO spectral encoding", 0.6f);
    }

    /* ---- Sample SNN population sub-nodes (first 3 only — not all) ---- */
    if (snn_id != BRAIN_KG_INVALID_NODE) {
        for (int i = 0; i < 3; ++i) {
            char name[64];
            char desc[128];
            snprintf(name, sizeof(name), "net_snn_population_%d", i);
            snprintf(desc, sizeof(desc),
                "SNN population %d - sampled aggregator (runtime rate tracked)", i);
            brain_kg_node_id_t pop = net_kg_ensure_node(
                kg, name, BRAIN_KG_NODE_CORE, desc);
            if (pop != BRAIN_KG_INVALID_NODE) {
                brain_kg_add_edge(kg, snn_id, pop,
                    BRAIN_KG_EDGE_CONNECTS_TO,
                    "contains SNN population", 1.0f);
            }
        }
    }

    brain_kg_set_access_level(kg, BRAIN_KG_ACCESS_READ, 0);

    fprintf(stderr, "[W5_NET_KG] W5 network structural wiring complete "
            "(6 roots + 3 sample SNN pops)\n");
    return true;
}
