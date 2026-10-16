/**
 * @file nimcp_swarm_brain.c
 * @brief NIMCP Swarm Brain Coordinator Implementation
 *
 * Integrates all swarm components into a cohesive distributed cognitive system.
 *
 * @author NIMCP Development Team
 * @date 2025-12-08
 * @version 1.0.0
 */

#include "swarm/nimcp_swarm_brain.h"
#include "swarm/nimcp_swarm_signal.h"
#include "core/brain/nimcp_brain.h"
#include "utils/logging/nimcp_logging.h"
#include "utils/validation/nimcp_common.h"
#include "utils/time/nimcp_time.h"
#include "utils/memory/nimcp_memory.h"
#include "utils/platform/nimcp_platform_mutex.h"
#include "api/nimcp_api_exception.h"
#include "async/nimcp_bio_async.h"
#include "async/nimcp_bio_messages.h"
#include "cognitive/knowledge/nimcp_kg_reader.h"
#include "utils/exception/nimcp_exception_macros.h"

#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <math.h>
#include "utils/fault_tolerance/nimcp_health_agent_macros.h"
#include "constants/nimcp_buffer_constants.h"
#include "constants/nimcp_timing_constants.h"

NIMCP_DECLARE_HEALTH_AGENT_ATOMIC(swarm_brain)

// Type alias for consistency with other modules
typedef nimcp_platform_mutex_t nimcp_mutex_t;

//=============================================================================
// Module Constants
//=============================================================================

#define MODULE_NAME "swarm_brain"

// Timing constants
#define PEER_TIMEOUT_MS NIMCP_SHORT_TIMEOUT_MS           // Consider peer dead after 500ms
#define HEARTBEAT_JITTER_MS 10        // Random jitter for heartbeat
#define SYNC_JITTER_MS 5              // Random jitter for sync

// Vote constants
#define MAX_ACTIVE_VOTES 8            // Maximum concurrent votes
#define VOTE_QUORUM_RATIO 0.51f       // 51% quorum required

// Workspace constants
#define WORKSPACE_DECAY_RATE 0.95f    // Attention decay per second
#define WORKSPACE_MIN_ATTENTION 0.01f // Minimum attention threshold

//=============================================================================
// Internal Structures
//=============================================================================

/**
 * @brief Active vote tracking
 */
typedef struct {
    vote_proposal_t proposal;         // The proposal being voted on
    uint32_t votes_for;               // Count of approve votes
    uint32_t votes_against;           // Count of reject votes
    uint32_t votes_abstain;           // Count of abstain votes
    uint64_t expiry_ms;               // When vote expires
    bool active;                      // Vote is active
    bool completed;                   // Vote has completed
    vote_decision_t result;           // Final result
} active_vote_t;

/**
 * @brief Collective workspace implementation
 */
typedef struct {
    workspace_entry_t* entries;       // Workspace entries
    uint32_t size;                    // Number of entries
    uint64_t last_update_ms;          // Last update time
    float coherence;                  // Overall coherence
    nimcp_mutex_t* lock;              // Thread safety
} collective_workspace_t;

/**
 * @brief Emergence context
 */
typedef struct {
    swarm_emergence_tier_t current_tier;  // Current tier
    uint32_t tier_change_count;           // Number of tier changes
    uint64_t tier_enter_time_ms;          // Time entered current tier
    float coherence_history[10];          // Recent coherence values
    uint32_t coherence_index;             // Circular buffer index
} emergence_context_t;

/**
 * @brief Consensus context
 */
typedef struct {
    active_vote_t votes[MAX_ACTIVE_VOTES]; // Active votes
    uint32_t vote_count;                   // Number of active votes
    uint32_t total_votes_completed;        // Total completed
    nimcp_mutex_t* lock;                   // Thread safety
} consensus_context_t;

/**
 * @brief Swarm brain implementation
 */
struct nimcp_swarm_brain {
    // Configuration
    swarm_brain_config_t config;

    // Core components
    brain_t local_brain;                          // Local constrained brain
    nimcp_swarm_signal_adapter_t* signal_adapter; // Radio I/O
    collective_workspace_t* workspace;            // Shared workspace
    emergence_context_t* emergence;               // Emergence tracking
    consensus_context_t* consensus;               // Voting system

    // Peer tracking
    swarm_peer_info_t peers[SWARM_MAX_PEERS];    // Active peers
    uint32_t peer_count;                          // Number of active peers
    nimcp_mutex_t* peer_lock;                     // Peer list lock

    // State
    bool joined;                                  // Has joined swarm
    bool operational;                             // System is operational
    uint64_t creation_time_ms;                    // Creation timestamp
    uint64_t last_heartbeat_ms;                   // Last heartbeat sent
    uint64_t last_sync_ms;                        // Last neuromod sync

    // Statistics
    swarm_stats_t stats;                          // Runtime statistics
    nimcp_mutex_t* stats_lock;                    // Stats lock

    // Bio-async (optional)
    struct nimcp_bio_async_module_ctx* bio_ctx;   // Bio-async context
    bool bio_async_enabled;                       // Bio-async enabled

    // Thread safety
    nimcp_mutex_t* state_lock;                    // Overall state lock
};

//=============================================================================
// Forward Declarations
//=============================================================================

static bool process_incoming_messages(swarm_brain_t* swarm);
static bool send_heartbeat(swarm_brain_t* swarm);
static bool update_peers(swarm_brain_t* swarm);
static bool update_workspace(swarm_brain_t* swarm);
static bool process_votes(swarm_brain_t* swarm);
static bool update_emergence_tier(swarm_brain_t* swarm);
static void handle_message(swarm_brain_t* swarm, uint8_t* data, uint32_t len, uint32_t source_id);
static void handle_heartbeat(swarm_brain_t* swarm, uint8_t* data, uint32_t len, uint32_t source_id);
static void handle_perception(swarm_brain_t* swarm, uint8_t* data, uint32_t len, uint32_t source_id);
static void handle_threat(swarm_brain_t* swarm, uint8_t* data, uint32_t len, uint32_t source_id);
static void handle_vote_propose(swarm_brain_t* swarm, uint8_t* data, uint32_t len, uint32_t source_id);
static void handle_vote_cast(swarm_brain_t* swarm, uint8_t* data, uint32_t len, uint32_t source_id);
static void handle_neuromod_sync(swarm_brain_t* swarm, uint8_t* data, uint32_t len, uint32_t source_id);
static void handle_workspace_update(swarm_brain_t* swarm, uint8_t* data, uint32_t len, uint32_t source_id);
static void handle_goodbye(swarm_brain_t* swarm, uint8_t* data, uint32_t len, uint32_t source_id);

//=============================================================================
// Local Brain Instantiation Implementation (Features 1-4)
//=============================================================================

/**
 * @brief Local brain instance tracking
 */
typedef struct {
    brain_t brain;                    // Brain instance
    uint16_t agent_id;                // Agent ID
    swarm_local_brain_config_t config;// Brain configuration
    bool active;                      // Is active
    uint64_t creation_time_ms;        // Creation timestamp
    uint64_t last_sync_ms;            // Last sync time
} local_brain_instance_t;

#define MAX_LOCAL_BRAINS 64

/**
 * @brief Get or create local brain storage in swarm
 */


// Forward declarations for static functions (SRP split)
static uint64_t get_time_ms(void);
static uint32_t simple_rand(uint32_t max);
static swarm_peer_info_t* find_peer(swarm_brain_t* swarm, uint16_t drone_id);
static bool update_peer(swarm_brain_t* swarm, uint16_t drone_id);
static void remove_inactive_peers(swarm_brain_t* swarm);
static collective_workspace_t* create_workspace(uint32_t size);
static void destroy_workspace(collective_workspace_t* ws);
static void workspace_update_entry(collective_workspace_t* ws, uint32_t concept_id, float attention);
static void workspace_decay(collective_workspace_t* ws);
static float workspace_calculate_coherence(collective_workspace_t* ws);
static emergence_context_t* create_emergence_context(void);
static void destroy_emergence_context(emergence_context_t* ctx);
static void emergence_update_tier(emergence_context_t* ctx, uint32_t peer_count, float coherence);
static consensus_context_t* create_consensus_context(void);
static void destroy_consensus_context(consensus_context_t* ctx);
static bool consensus_start_vote(consensus_context_t* ctx, const vote_proposal_t* proposal);
static bool consensus_cast_vote(consensus_context_t* ctx, uint32_t proposal_id, vote_decision_t decision);
static void consensus_process_votes(consensus_context_t* ctx, uint32_t peer_count);
static bool broadcast_vote(swarm_brain_t* swarm, uint32_t proposal_id, vote_decision_t decision);
static local_brain_instance_t* get_local_brains(swarm_brain_t* swarm);
static local_brain_instance_t* find_local_brain(swarm_brain_t* swarm, uint16_t agent_id);

//=============================================================================
// SRP Split: Function implementations organized by responsibility
//=============================================================================
#include "nimcp_swarm_brain_part_helpers.c"  // 32 functions: helpers
#include "nimcp_swarm_brain_part_processing.c"  // 5 functions: processing
#include "nimcp_swarm_brain_part_accessors.c"  // 9 functions: accessors
#include "nimcp_swarm_brain_part_lifecycle.c"  // 5 functions: lifecycle
#include "nimcp_swarm_brain_part_core.c"  // 14 functions: core
