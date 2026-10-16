//=============================================================================
// nimcp_brain_init_fuzzy.c - Fuzzy Logic Subsystem Initialization
//=============================================================================
/**
 * @file nimcp_brain_init_fuzzy.c
 * @brief Fuzzy logic subsystem initialization with full brain integration
 *
 * BIOLOGICAL BASIS:
 * Neural systems encode information with graded firing rates, not binary
 * on/off. Fuzzy logic captures this biological reality by providing
 * continuous membership degrees for classification, decision-making,
 * and modulation throughout the brain.
 *
 * INTEGRATION POINTS:
 * - SNN: Fuzzy-to-spike population coding conversion
 * - STDP: Fuzzy temporal window for learning modulation
 * - Plasticity: Fuzzy rate scheduling from performance/stability
 * - LNN: State vector classification into fuzzy categories
 * - Training: Learning rate scheduling, convergence detection
 * - Immune: Inflammation modulates fuzzy precision
 * - Ethics: Fuzzy action scoring for graded safety assessment
 *
 * @author NIMCP Development Team
 * @date 2025-12-31
 */

#include "core/brain/nimcp_brain_internal.h"
#include "core/brain/factory/init/nimcp_brain_init_subsystems.h"
#include "utils/fuzzy/nimcp_fuzzy_bridge.h"

#include <stdio.h>
#include "utils/fault_tolerance/nimcp_health_agent_macros.h"
#include "utils/bridge/nimcp_bridge_boilerplate.h"
#include "mesh/nimcp_mesh_participant.h"
#include "mesh/nimcp_mesh_adapter.h"
#include "utils/exception/nimcp_exception_macros.h"

BRIDGE_BOILERPLATE_MESH_ONLY(brain_init_fuzzy, MESH_ADAPTER_CATEGORY_SYSTEM)


//=============================================================================
// Integration Helpers
//=============================================================================

static void connect_fuzzy_to_immune(fuzzy_bridge_t* fuzzy, brain_t brain) {
    if (!brain->immune_system) return;
    fuzzy_bridge_set_immune(fuzzy, brain->immune_system);
    fprintf(stderr, "[FUZZY] Connected to immune system\n");
}

static void connect_fuzzy_to_health(fuzzy_bridge_t* fuzzy, brain_t brain) {
    if (!brain->health_agent) return;
    fuzzy_bridge_set_health_agent(fuzzy, brain->health_agent);
    g_brain_init_fuzzy_health_agent = (nimcp_health_agent_t*)brain->health_agent;
    fprintf(stderr, "[FUZZY] Connected to health agent\n");
}

static void connect_fuzzy_to_kg(fuzzy_bridge_t* fuzzy, brain_t brain) {
    if (!brain->kg_reader) return;
    fuzzy_bridge_set_kg_wiring(fuzzy, (kg_wiring_t*)brain->kg_reader);
    fprintf(stderr, "[FUZZY] Connected to KG reader\n");
}

static void connect_fuzzy_to_ethics(fuzzy_bridge_t* fuzzy, brain_t brain) {
    if (!brain->ethics) return;
    fuzzy_bridge_set_ethics(fuzzy, brain->ethics);
    fprintf(stderr, "[FUZZY] Connected to ethics engine\n");
}

static void connect_fuzzy_to_snn(fuzzy_bridge_t* fuzzy, brain_t brain) {
    if (!brain->snn_network) return;
    fuzzy_bridge_set_snn(fuzzy, brain->snn_network);
    fprintf(stderr, "[FUZZY] Connected to SNN\n");
}

static void connect_fuzzy_to_training(fuzzy_bridge_t* fuzzy, brain_t brain) {
    if (!brain->training_ctx) return;
    fuzzy_bridge_set_training(fuzzy, brain->training_ctx);
    fprintf(stderr, "[FUZZY] Connected to training context\n");
}

//=============================================================================
// Main Init Function
//=============================================================================

bool nimcp_brain_factory_init_fuzzy_subsystem(brain_t brain) {
    if (!brain) {
        fprintf(stderr, "[FUZZY] ERROR: brain is NULL\n");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "nimcp_brain_factory_init_fuzzy_subsystem: brain is NULL");
        return false;
    }

    /* Check if fuzzy logic is enabled (default: true) */
    if (!brain->config.enable_fuzzy_logic) {
        brain->fuzzy_logic_enabled = false;
        brain->fuzzy_bridge = NULL;
        fprintf(stderr, "[FUZZY] Fuzzy logic disabled by config\n");
        return true; /* Not an error — just disabled */
    }

    brain_init_fuzzy_heartbeat("fuzzy_init", 0.0f);

    /* Create fuzzy bridge with default config */
    fuzzy_bridge_config_t config = fuzzy_bridge_default_config();
    fuzzy_bridge_t* fuzzy = fuzzy_bridge_create(&config);
    if (!fuzzy) {
        fprintf(stderr, "[FUZZY] ERROR: Failed to create fuzzy bridge\n");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "nimcp_brain_factory_init_fuzzy_subsystem: fuzzy is NULL");
        return false;
    }

    /* Store in brain struct */
    brain->fuzzy_bridge = fuzzy;
    brain->fuzzy_logic_enabled = true;

    /* Connect to available subsystems */
    connect_fuzzy_to_immune(fuzzy, brain);
    connect_fuzzy_to_health(fuzzy, brain);
    connect_fuzzy_to_kg(fuzzy, brain);
    connect_fuzzy_to_ethics(fuzzy, brain);
    connect_fuzzy_to_snn(fuzzy, brain);
    connect_fuzzy_to_training(fuzzy, brain);

    /* BBB integration */
    if (brain->bbb_enabled) {
        fuzzy_bridge_set_bbb(fuzzy, &brain->bbb_system);
        fprintf(stderr, "[FUZZY] Connected to BBB\n");
    }

    /* Security integration */
    if (brain->security_integration) {
        fuzzy_bridge_set_security(fuzzy, brain->security_integration);
        fprintf(stderr, "[FUZZY] Connected to security\n");
    }

    brain_init_fuzzy_heartbeat("fuzzy_init", 1.0f);
    fprintf(stderr, "[FUZZY] Fuzzy logic subsystem initialized successfully\n");
    return true;
}
