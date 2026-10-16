/**
 * @file nimcp_reasoning_events.h
 * @brief MODULE 6: Reasoning Event Publisher
 * @version 1.0.0
 * @date 2025-11-20
 *
 * SOLE RESPONSIBILITY: Publish all reasoning-related events to event bus
 *
 * WHAT: Centralized event publishing for all reasoning operations
 * WHY:  Decouple reasoning engine from event bus implementation
 * HOW:  One function per event type with typed parameters
 *
 * EVENT TYPES PUBLISHED:
 * - Logic gate evaluation (AND/OR/NOT)
 * - Inference lifecycle (started, complete, failed)
 * - Fact/rule management (added, removed)
 * - Unification (success/failure)
 * - Chaining steps (forward/backward)
 * - Proof results (found/failed)
 * - Contradiction detection
 * - Novel fact derivation
 */

#ifndef NIMCP_REASONING_EVENTS_H
#define NIMCP_REASONING_EVENTS_H

#include <stdint.h>
#include <stdbool.h>
#include "core/events/nimcp_event_bus.h"

#ifdef __cplusplus
extern "C" {
#endif

//=============================================================================
// Event Publishing Functions
//=============================================================================

/**
 * @brief Publish logic gate evaluation event
 *
 * @param bus Event bus
 * @param gate_id Gate identifier
 * @param gate_type Gate type (AND/OR/NOT)
 * @param result Evaluation result
 * @return true on success
 */
bool reasoning_events_publish_logic_gate_evaluated(
    event_bus_t bus,
    uint32_t gate_id,
    const char* gate_type,
    bool result
);

/**
 * @brief Publish inference started event
 *
 * @param bus Event bus
 * @param method Inference method (forward/backward/resolution)
 * @param goal Goal description
 * @return true on success
 */
bool reasoning_events_publish_inference_started(
    event_bus_t bus,
    const char* method,
    const char* goal
);

/**
 * @brief Publish inference complete event
 *
 * @param bus Event bus
 * @param method Inference method
 * @param success Whether inference succeeded
 * @param steps Number of steps taken
 * @return true on success
 */
bool reasoning_events_publish_inference_complete(
    event_bus_t bus,
    const char* method,
    bool success,
    uint32_t steps
);

/**
 * @brief Publish fact added event
 *
 * @param bus Event bus
 * @param fact Fact string
 * @param is_novel Whether fact is novel
 * @return true on success
 */
bool reasoning_events_publish_fact_added(
    event_bus_t bus,
    const char* fact,
    bool is_novel
);

/**
 * @brief Publish rule added event
 *
 * @param bus Event bus
 * @param rule Rule string
 * @return true on success
 */
bool reasoning_events_publish_rule_added(
    event_bus_t bus,
    const char* rule
);

/**
 * @brief Publish unification result event
 *
 * @param bus Event bus
 * @param term1 First term
 * @param term2 Second term
 * @param success Whether unification succeeded
 * @return true on success
 */
bool reasoning_events_publish_unification_result(
    event_bus_t bus,
    const char* term1,
    const char* term2,
    bool success
);

/**
 * @brief Publish forward chaining step event
 *
 * @param bus Event bus
 * @param rule_applied Rule that was applied
 * @param new_fact Newly derived fact
 * @return true on success
 */
bool reasoning_events_publish_forward_chain_step(
    event_bus_t bus,
    const char* rule_applied,
    const char* new_fact
);

/**
 * @brief Publish backward chaining step event
 *
 * @param bus Event bus
 * @param goal Current goal
 * @param subgoal Subgoal to prove
 * @return true on success
 */
bool reasoning_events_publish_backward_chain_step(
    event_bus_t bus,
    const char* goal,
    const char* subgoal
);

/**
 * @brief Publish proof result event
 *
 * @param bus Event bus
 * @param goal Goal that was proven/failed
 * @param success Whether proof succeeded
 * @param proof_steps Proof steps (NULL if failed)
 * @return true on success
 */
bool reasoning_events_publish_proof_result(
    event_bus_t bus,
    const char* goal,
    bool success,
    const char* proof_steps
);

/**
 * @brief Publish contradiction detected event
 *
 * @param bus Event bus
 * @param fact1 First contradicting fact
 * @param fact2 Second contradicting fact
 * @return true on success
 */
bool reasoning_events_publish_contradiction_detected(
    event_bus_t bus,
    const char* fact1,
    const char* fact2
);

/**
 * @brief Publish novel fact derived event
 *
 * @param bus Event bus
 * @param fact Novel fact
 * @param derivation How it was derived
 * @return true on success
 */
bool reasoning_events_publish_novel_fact_derived(
    event_bus_t bus,
    const char* fact,
    const char* derivation
);

//=============================================================================
// Event Data Structures
//=============================================================================

typedef struct {
    uint32_t gate_id;
    char gate_type[16];
    bool result;
} logic_gate_event_data_t;

typedef struct {
    char method[32];
    char goal[256];
    uint32_t steps;
    bool success;
} inference_event_data_t;

typedef struct {
    char fact[512];
    bool is_novel;
} fact_event_data_t;

typedef struct {
    char term1[256];
    char term2[256];
    bool success;
} unification_event_data_t;

typedef struct {
    char rule[512];
    char fact[512];
} chaining_event_data_t;

typedef struct {
    char goal[256];
    char proof_steps[1024];
    bool success;
} proof_event_data_t;

typedef struct {
    char fact1[512];
    char fact2[512];
} contradiction_event_data_t;

//=============================================================================
// Utility Functions
//=============================================================================

/**
 * @brief Create reasoning event with standard fields
 *
 * @param type Event type
 * @param priority Event priority
 * @return Initialized event
 */
brain_event_t reasoning_events_create(brain_event_type_t type, event_priority_t priority);

/**
 * @brief Validate event data size
 *
 * @param data_size Size of data to pack
 * @return true if data fits in event
 */
bool reasoning_events_validate_data_size(size_t data_size);

#ifdef __cplusplus
}
#endif

#endif // NIMCP_REASONING_EVENTS_H
