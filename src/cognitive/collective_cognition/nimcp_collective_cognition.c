/**
 * @file nimcp_collective_cognition.c
 * @brief Implementation of unified collective cognition system
 *
 * WHAT: Integrates hyperscanning, extended mind, collective phi, and shared intentionality
 * WHY: Enable distributed consciousness across multiple NIMCP brain instances
 * HOW: Unified coordination of all subsystems with bio-async messaging
 */

#include "cognitive/collective_cognition/nimcp_collective_cognition.h"
#include "cognitive/knowledge/nimcp_kg_reader.h"
#include "cognitive/kg/nimcp_wave10_affective_kg.h"  /* W10: collective phi events */
#include "cognitive/collective_cognition/nimcp_hyperscanning.h"
#include "cognitive/collective_cognition/nimcp_extended_mind.h"
#include "cognitive/collective_cognition/nimcp_collective_phi.h"
#include "cognitive/collective_cognition/nimcp_shared_intentionality.h"
#include "utils/memory/nimcp_memory.h"
#include "utils/time/nimcp_time.h"
#include "utils/exception/nimcp_exception_macros.h"
#include "utils/logging/nimcp_logging.h"
#include <string.h>
#include <stdio.h>
#include <math.h>
#include <stdatomic.h>
#include "utils/bridge/nimcp_bridge_boilerplate.h"
#include "mesh/nimcp_mesh_participant.h"
#include "mesh/nimcp_mesh_adapter.h"

BRIDGE_BOILERPLATE(collective_cognition, MESH_ADAPTER_CATEGORY_COGNITIVE)


/*=============================================================================
 * Internal Structures
 *===========================================================================*/

/**
 * @brief Registered brain instance
 */
typedef struct {
    uint32_t instance_id;
    brain_t* brain;                 /* Optional direct brain reference */
    bool active;

    /* Per-instance state */
    float local_phi;                /* Individual phi */
    float atp_level;                /* Metabolic state */
    float fatigue_level;
    uint64_t last_heartbeat_us;
    uint32_t missed_heartbeats;

    /* Sync state per band */
    float band_power[SYNC_BAND_COUNT];
    float band_phase[SYNC_BAND_COUNT];
} registered_instance_t;

/**
 * @brief Internal state for collective cognition system
 */
struct collective_cognition {
    /* Configuration */
    collective_cognition_config_t config;

    /* Registered instances */
    registered_instance_t instances[COLLECTIVE_MAX_INSTANCES];
    uint32_t instance_count;

    /* Subsystem handles (embedded for now, will be separate modules) */
    /* TODO: Replace with actual subsystem handles when implemented */
    void* hyperscanning;     /* hyperscanning_t* */
    void* extended_mind;     /* extended_mind_t* */
    void* phi_system;        /* collective_phi_system_t* */
    void* intentionality;    /* shared_intentionality_t* */

    /* Cached state */
    collective_cognition_state_t state;

    /* Bio-async integration */
    bio_router_t* bio_router;
    bool bio_async_connected;

    /* Statistics */
    collective_cognition_stats_t stats;

    /* Synchronization tracking */
    float pair_plv[COLLECTIVE_MAX_INSTANCES][COLLECTIVE_MAX_INSTANCES][SYNC_BAND_COUNT];

    /* Internal flags */
    bool initialized;
    uint64_t last_update_us;
};

/* ============================================================================
 * Phase 8: Full training implementation
 * ============================================================================ */
static _Atomic uint64_t g_collective_cognition_training_steps = 0;
static _Atomic double g_collective_cognition_training_total_error = 0.0;
static _Atomic double g_collective_cognition_training_best_error = 1e30;
static _Atomic bool g_collective_cognition_training_active = false;



// Forward declarations for static functions (SRP split)
static uint64_t get_timestamp_us(void);
static registered_instance_t* find_instance( collective_cognition_t* cc, uint32_t instance_id );
static registered_instance_t* find_free_slot(collective_cognition_t* cc);
static int find_instance_index( const collective_cognition_t* cc, uint32_t instance_id );
static float compute_plv( const registered_instance_t* a, const registered_instance_t* b, sync_band_t band );
static float compute_global_sync(collective_cognition_t* cc);
static void compute_collective_phi(collective_cognition_t* cc);
static collective_consciousness_level_t phi_to_level(float phi);
static void update_hyperscanning_state(collective_cognition_t* cc);
static void update_we_mode_state(collective_cognition_t* cc);
static void update_extended_mind_state(collective_cognition_t* cc);
static void update_aggregate_state(collective_cognition_t* cc);

//=============================================================================
// SRP Split: Function implementations organized by responsibility
//=============================================================================
#include "nimcp_collective_cognition_part_helpers.c"  // 11 functions: helpers
#include "nimcp_collective_cognition_part_lifecycle.c"  // 5 functions: lifecycle
#include "nimcp_collective_cognition_part_accessors.c"  // 20 functions: accessors
#include "nimcp_collective_cognition_part_core.c"  // 11 functions: core
#include "nimcp_collective_cognition_part_processing.c"  // 2 functions: processing
#include "nimcp_collective_cognition_part_io.c"  // 2 functions: io
