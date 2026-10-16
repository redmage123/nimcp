//=============================================================================
// nimcp_ethics.c - Ethics Engine Core Orchestration
//=============================================================================
// RESPONSIBILITY: Engine creation, destruction, and orchestration
//
// This is the main orchestration module that coordinates between:
// - nimcp_ethics_evaluation.c (Golden Rule, empathy)
// - nimcp_ethics_asimov.c (Asimov's Laws)
// - nimcp_ethics_policies.c (policy management)
// - nimcp_ethics_incidents.c (incident logging)
// - nimcp_ethics_learning.c (learning/adaptation)
//
// REFACTORED: Previously 3020 lines, now focused on orchestration only
//=============================================================================

#include "cognitive/ethics/nimcp_ethics.h"
#include "cognitive/ethics/nimcp_ethics_internal.h"
#include "cognitive/ethics/nimcp_ethics_snn_bridge.h"
#include "cognitive/ethics/nimcp_ethics_plasticity_bridge.h"
#include "nimcp.h"
#include "security/nimcp_security.h"
#include "security/nimcp_blood_brain_barrier.h"
#include "security/nimcp_w11_safety_kg_events.h"  /* W11: safety KG emission */

#include <math.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include "core/brain/nimcp_brain.h"
#include "utils/containers/nimcp_hash_table.h"
#include "utils/containers/nimcp_btree.h"
#include "utils/memory/nimcp_memory.h"
#include "utils/thread/nimcp_thread.h"
#include "utils/time/nimcp_time.h"
#include "utils/logging/nimcp_logging.h"

// BIO-ASYNC INTEGRATION
#include "async/nimcp_bio_async.h"
#include "async/nimcp_bio_messages.h"
#include "async/nimcp_bio_router.h"
#include "async/nimcp_wiring_helpers.h"
#include "utils/memory/nimcp_unified_memory.h"
#include "utils/error/nimcp_error_codes.h"
#include "cognitive/knowledge/nimcp_kg_reader.h"

#define LOG_MODULE "ethics"
#include "utils/bridge/nimcp_bridge_boilerplate.h"
#include "mesh/nimcp_mesh_participant.h"
#include "mesh/nimcp_mesh_adapter.h"

BRIDGE_BOILERPLATE(ethics, MESH_ADAPTER_CATEGORY_COGNITIVE)


// Phase 10.3: Emotional working memory integration
#include "cognitive/nimcp_working_memory.h"
#include "cognitive/nimcp_emotional_tagging.h"

// Phase 11: Symbolic logic integration
#include "cognitive/nimcp_symbolic_logic.h"
#include "utils/exception/nimcp_exception_macros.h"
#include "constants/nimcp_buffer_constants.h"

//=============================================================================
// Forward Declarations for BIO-ASYNC
//=============================================================================

static void bio_broadcast_ethics_response(ethics_engine_t engine,
                                          const ethics_evaluation_t* eval,
                                          uint32_t action_id);

static nimcp_error_t handle_ethics_request(
    const void* msg,
    size_t msg_size,
    nimcp_bio_promise_t response_promise,
    void* user_data);

/**
 * @brief Value type for policy hash table entries
 */
typedef struct {
    ethics_policy_t* policy;
} policy_value_t;

/**
 * @brief Inserts policy into hash table
 */


// Forward declarations for static functions (SRP split)
static int ethics_wiring_handler_callback( bio_module_context_t ctx, const bio_message_type_t* message_types, uint32_t message_count, void* user_data );
static void init_buffer_pool(feature_buffer_pool_t* pool);
static float* acquire_buffer(feature_buffer_pool_t* pool);
static void release_buffer(feature_buffer_pool_t* pool, float* buffer);
static hash_table_t* create_policy_hash_table(void);
static bool hash_table_insert_policy(hash_table_t* table, ethics_policy_t* policy);
static bool hash_table_remove_policy(hash_table_t* table, uint32_t policy_id);
static brain_t create_golden_rule_network(uint32_t feature_size);
static empathy_network_t create_empathy_network(void);
static bool allocate_policy_storage(ethics_engine_t engine);
static bool allocate_violation_storage(ethics_engine_t engine);
static void add_golden_rule_policy(ethics_engine_t engine);
static bool validate_evaluation_inputs(ethics_engine_t engine, const action_context_t* action, ethics_evaluation_t* result);
static float calculate_final_score(float golden_rule_score, float policy_score);
static bool expand_violations_array(ethics_engine_t engine);
static void record_violation(ethics_engine_t engine, const action_context_t* action, ethics_violation_type_t violation_type, float severity, float golden_rule_score);
static void generate_allowed_explanation(ethics_evaluation_t* result, float golden_rule_score, float threshold);
static void generate_blocked_explanation(ethics_evaluation_t* result, float golden_rule_score, float threshold, ethics_violation_type_t violation, float severity);
static void build_evaluation_result(ethics_engine_t engine, const action_context_t* action, float final_score, float golden_rule_score, ethics_violation_type_t worst_violation, float worst_severity, ethics_evaluation_t* result);
static void update_learning(ethics_engine_t engine, const action_context_t* action, const ethics_evaluation_t* result);

//=============================================================================
// SRP Split: Function implementations organized by responsibility
//=============================================================================
#include "nimcp_ethics_part_processing.c"  // 2 functions: processing
#include "nimcp_ethics_part_helpers.c"  // 22 functions: helpers
#include "nimcp_ethics_part_lifecycle.c"  // 2 functions: lifecycle
#include "nimcp_ethics_part_core.c"  // 10 functions: core
#include "nimcp_ethics_part_accessors.c"  // 15 functions: accessors
