# Swarm Intelligence Integration with Executive Function and Salience Detection

## Overview

This document describes the complete integration of Swarm Intelligence with Executive Function and Salience Detection modules in NIMCP.

## Architecture

```
┌─────────────────────────────────────────────────────────────────┐
│                     SWARM-COGNITIVE INTEGRATION                  │
├─────────────────────────────────────────────────────────────────┤
│                                                                   │
│  ┌──────────────────┐         ┌──────────────────┐             │
│  │   Salience       │         │   Executive      │             │
│  │   Detection      │         │   Function       │             │
│  └────────┬─────────┘         └────────┬─────────┘             │
│           │                             │                        │
│           │ BIO_MSG_SWARM_SIGNAL_UPDATE │                        │
│           │◄────────────────────────────┤                        │
│           │                             │                        │
│           │ BIO_MSG_SWARM_SALIENCE_     │                        │
│           │      AGGREGATE              │                        │
│           ├─────────────────────────────►                        │
│           │                             │                        │
│           │                             │ BIO_MSG_SWARM_         │
│           │                             │ CONSENSUS_REQUEST      │
│           │                             ├───────────────┐        │
│           │                             │               │        │
│           │                             │               ▼        │
│           │                             │      ┌─────────────┐  │
│           │                             │      │   Swarm     │  │
│           │                             │      │  Consensus  │  │
│           │                             │      └─────────────┘  │
│           │                             │               │        │
│           │                             │ BIO_MSG_SWARM_         │
│           │                             │ CONSENSUS_REACHED      │
│           │                             │◄──────────────┘        │
│           │                             │                        │
│           │                             │ BIO_MSG_EXECUTIVE_     │
│           │                             │ DECISION_BROADCAST     │
│           │                             ├───────────────►        │
│           │                             │                        │
└───────────┴─────────────────────────────┴────────────────────────┘
```

## Implementation Summary

### 1. Message Types (bio_messages.h) ✓ COMPLETE

Added 5 new message types to enable swarm-cognitive communication:

- `BIO_MSG_SWARM_CONSENSUS_REACHED` - Consensus result notification
- `BIO_MSG_SWARM_CONSENSUS_REQUEST` - Request for swarm vote
- `BIO_MSG_SWARM_SIGNAL_UPDATE` - Swarm signal state update
- `BIO_MSG_SWARM_SALIENCE_AGGREGATE` - Aggregated salience from swarm
- `BIO_MSG_EXECUTIVE_DECISION_BROADCAST` - Executive decision to swarm

### 2. Executive Structure Extension ✓ COMPLETE

Added to `struct executive_controller`:

```c
// Swarm coordination (Phase 14.x)
void* swarm;                            // Swarm consensus context (opaque)
bool swarm_coordination_enabled;        // Enable swarm coordination
float swarm_consensus_threshold;        // Threshold for consensus [0,1]
uint32_t pending_consensus_proposals;   // Count of proposals awaiting consensus
nimcp_mutex_t swarm_lock;               // Protect swarm state
```

### 3. Salience Structure Extension (PENDING)

To be added to `struct salience_evaluator_struct`:

```c
// Swarm aggregation (Phase 14.x)
void* swarm;                            // Swarm signal context (opaque)
bool swarm_aggregation_enabled;         // Enable swarm aggregation
float swarm_salience_weight;            // Weight for swarm input [0,1]
uint32_t swarm_agent_count;             // Number of swarm agents
float swarm_avg_salience;               // Running average from swarm
nimcp_mutex_t swarm_lock;               // Protect swarm state
```

### 4. Executive Bio-Async Handlers (TO IMPLEMENT)

#### handle_swarm_consensus_reached()

```c
/**
 * @brief Handle swarm consensus reached notification
 *
 * WHAT: Process consensus result and update executive decision
 * WHY:  Executive needs to know when swarm validates its decisions
 * HOW:  Match proposal_id to decision, update confidence
 */
static nimcp_error_t handle_swarm_consensus_reached(
    const void* msg,
    size_t msg_size,
    nimcp_bio_promise_t response_promise,
    void* user_data)
{
    const bio_msg_swarm_consensus_reached_t* consensus = msg;
    executive_controller_t* exec = user_data;

    // Guard: Validate inputs
    if (!consensus || !exec) {
        return NIMCP_ERROR_NULL_ARG;
    }

    nimcp_mutex_lock(&exec->swarm_lock);

    // WHAT: Find matching decision
    // WHY:  Need to update the decision with consensus result
    // HOW:  Search for decision_id in consensus message

    if (consensus->passed) {
        LOG_INFO("Swarm consensus PASSED: decision=%u, agreement=%.2f",
                 consensus->decision_id, consensus->weighted_agreement);

        // Update decision confidence with swarm consensus
        // If consensus threshold met, proceed with action
        if (consensus->weighted_agreement >= exec->swarm_consensus_threshold) {
            // Trigger action execution
        }
    } else {
        LOG_WARN("Swarm consensus FAILED: decision=%u, agreement=%.2f",
                 consensus->decision_id, consensus->weighted_agreement);

        // Reconsider decision or escalate
    }

    exec->pending_consensus_proposals--;

    nimcp_mutex_unlock(&exec->swarm_lock);

    return NIMCP_SUCCESS;
}
```

### 5. Executive Swarm Coordination Functions (TO IMPLEMENT)

#### executive_request_swarm_consensus()

```c
/**
 * @brief Request swarm consensus on executive decision
 *
 * WHAT: Submit decision to swarm for distributed validation
 * WHY:  Enable collective intelligence in decision-making
 * HOW:  Broadcast consensus request via bio-async
 *
 * COMPLEXITY: O(1)
 * THREAD-SAFE: Yes (mutex-protected)
 *
 * @param exec Executive controller
 * @param task_id Task requiring consensus
 * @param topic Decision topic
 * @param urgency Decision urgency [0,1]
 * @return NIMCP_SUCCESS or error code
 */
nimcp_error_t executive_request_swarm_consensus(
    executive_controller_t* exec,
    uint32_t task_id,
    swarm_vote_topic_t topic,
    float urgency)
{
    // Guard: Validate inputs
    if (!exec) {
        set_error("NULL executive controller");
        return NIMCP_ERROR_NULL_ARG;
    }

    if (!exec->swarm_coordination_enabled) {
        set_error("Swarm coordination not enabled");
        return NIMCP_ERROR_INVALID_STATE;
    }

    nimcp_mutex_lock(&exec->swarm_lock);

    // WHAT: Create consensus request message
    // WHY:  Need to communicate decision to swarm
    // HOW:  Populate bio-async message and broadcast

    bio_msg_swarm_consensus_request_t request = {0};
    bio_msg_init_header(&request.header, BIO_MSG_SWARM_CONSENSUS_REQUEST,
                        bio_module_context_get_id(exec->bio_ctx),
                        BIO_MODULE_ALL, sizeof(request));
    request.header.flags |= BIO_MSG_FLAG_BROADCAST;
    request.header.channel = BIO_CHANNEL_SEROTONIN;  // Deliberative

    request.decision_id = exec->next_task_id;
    request.task_id = task_id;
    request.topic = topic;
    request.urgency = urgency;
    request.quorum_required = 0;  // All agents
    request.threshold = exec->swarm_consensus_threshold;
    request.deadline_ms = nimcp_time_monotonic_ms() + 5000;  // 5 second deadline

    // Broadcast request
    bio_router_broadcast(exec->bio_ctx, &request, sizeof(request));

    exec->pending_consensus_proposals++;

    nimcp_mutex_unlock(&exec->swarm_lock);

    LOG_INFO("Requested swarm consensus: decision=%u, topic=%d, urgency=%.2f",
             request.decision_id, topic, urgency);

    return NIMCP_SUCCESS;
}
```

#### executive_broadcast_decision()

```c
/**
 * @brief Broadcast executive decision to swarm
 *
 * WHAT: Inform swarm of executive decision for coordination
 * WHY:  Enable swarm to act on executive commands
 * HOW:  Broadcast decision via bio-async
 *
 * @param exec Executive controller
 * @param task_id Task that was decided
 * @param selected_option Option selected
 * @param confidence Decision confidence
 * @return NIMCP_SUCCESS or error code
 */
nimcp_error_t executive_broadcast_decision(
    executive_controller_t* exec,
    uint32_t task_id,
    uint32_t selected_option,
    float confidence)
{
    // Guard: Validate inputs
    if (!exec) {
        return NIMCP_ERROR_NULL_ARG;
    }

    if (!exec->swarm_coordination_enabled || !exec->bio_async_enabled) {
        return NIMCP_ERROR_INVALID_STATE;
    }

    // WHAT: Create decision broadcast message
    // WHY:  Swarm needs to know executive decisions
    // HOW:  Populate and broadcast bio-async message

    bio_msg_executive_decision_broadcast_t broadcast = {0};
    bio_msg_init_header(&broadcast.header, BIO_MSG_EXECUTIVE_DECISION_BROADCAST,
                        bio_module_context_get_id(exec->bio_ctx),
                        BIO_MODULE_ALL, sizeof(broadcast));
    broadcast.header.flags |= BIO_MSG_FLAG_BROADCAST;
    broadcast.header.channel = BIO_CHANNEL_DOPAMINE;  // Action signal

    broadcast.decision_id = exec->next_task_id;
    broadcast.task_id = task_id;
    broadcast.selected_option = selected_option;
    broadcast.decision_confidence = confidence;
    broadcast.requires_coordination = true;
    broadcast.priority = 0.8f;

    bio_router_broadcast(exec->bio_ctx, &broadcast, sizeof(broadcast));

    LOG_INFO("Broadcast decision: task=%u, option=%u, confidence=%.2f",
             task_id, selected_option, confidence);

    return NIMCP_SUCCESS;
}
```

### 6. Salience Bio-Async Handlers (TO IMPLEMENT)

#### handle_swarm_signal_update()

```c
/**
 * @brief Handle swarm signal update
 *
 * WHAT: Process swarm signal for salience aggregation
 * WHY:  Collective signals inform individual salience
 * HOW:  Update running average of swarm salience
 */
static nimcp_error_t handle_swarm_signal_update(
    const void* msg,
    size_t msg_size,
    nimcp_bio_promise_t response_promise,
    void* user_data)
{
    const bio_msg_swarm_signal_update_t* signal = msg;
    salience_evaluator_t eval = user_data;

    // Guard: Validate inputs
    if (!signal || !eval) {
        return NIMCP_ERROR_NULL_ARG;
    }

    nimcp_mutex_lock(&eval->swarm_lock);

    // WHAT: Integrate swarm signal into salience evaluation
    // WHY:  Swarm consensus affects attention allocation
    // HOW:  Update running average with decay

    float alpha = 0.2f;  // Learning rate
    eval->swarm_avg_salience = alpha * signal->collective_intensity +
                               (1.0f - alpha) * eval->swarm_avg_salience;

    nimcp_mutex_unlock(&eval->swarm_lock);

    LOG_DEBUG("Swarm signal update: agent=%u, intensity=%.2f, avg=%.2f",
              signal->agent_id, signal->collective_intensity, eval->swarm_avg_salience);

    return NIMCP_SUCCESS;
}
```

### 7. Salience Swarm Aggregation Functions (TO IMPLEMENT)

#### salience_broadcast_to_swarm()

```c
/**
 * @brief Broadcast salient event to swarm
 *
 * WHAT: Notify swarm of high-salience stimulus
 * WHY:  Enable swarm coordination on important events
 * HOW:  Broadcast via bio-async if salience exceeds threshold
 *
 * @param evaluator Salience evaluator
 * @param salience Computed salience
 * @param stimulus_id Stimulus identifier
 * @return NIMCP_SUCCESS or error code
 */
nimcp_error_t salience_broadcast_to_swarm(
    salience_evaluator_t evaluator,
    const brain_salience_t* salience,
    uint32_t stimulus_id)
{
    // Guard: Validate inputs
    if (!evaluator || !salience) {
        return NIMCP_ERROR_NULL_ARG;
    }

    if (!evaluator->swarm_aggregation_enabled) {
        return NIMCP_ERROR_INVALID_STATE;
    }

    // WHAT: Only broadcast high-salience events
    // WHY:  Avoid flooding swarm with low-importance signals
    // HOW:  Check against threshold

    if (salience->salience < evaluator->config.high_salience_threshold) {
        return NIMCP_SUCCESS;  // Below threshold, don't broadcast
    }

    // WHAT: Create swarm salience aggregate message
    // WHY:  Share salience assessment with swarm
    // HOW:  Populate bio-async message

    bio_msg_swarm_salience_aggregate_t aggregate = {0};
    bio_msg_init_header(&aggregate.header, BIO_MSG_SWARM_SALIENCE_AGGREGATE,
                        bio_module_context_get_id(evaluator->bio_ctx),
                        BIO_MODULE_ALL, sizeof(aggregate));
    aggregate.header.flags |= BIO_MSG_FLAG_BROADCAST;
    aggregate.header.channel = BIO_CHANNEL_NOREPINEPHRINE;  // Alerting

    aggregate.stimulus_id = stimulus_id;
    aggregate.agent_count = 1;  // This agent
    aggregate.avg_salience = salience->salience;
    aggregate.avg_novelty = salience->novelty;
    aggregate.avg_surprise = salience->surprise;
    aggregate.avg_urgency = salience->urgency;
    aggregate.consensus_confidence = salience->confidence;
    aggregate.variance = 0.0f;  // Single agent
    aggregate.high_agreement = true;

    bio_router_broadcast(evaluator->bio_ctx, &aggregate, sizeof(aggregate));

    LOG_INFO("Broadcast salience to swarm: stimulus=%u, salience=%.2f",
             stimulus_id, salience->salience);

    return NIMCP_SUCCESS;
}
```

### 8. Initialization Changes

#### executive_create_custom()

Add after bio-async registration:

```c
// Initialize swarm coordination
exec->swarm = NULL;
exec->swarm_coordination_enabled = false;
exec->swarm_consensus_threshold = SWARM_DEFAULT_THRESHOLD;  // 0.666
exec->pending_consensus_proposals = 0;
nimcp_mutex_init(&exec->swarm_lock, NULL);

// Register swarm message handlers if bio-async enabled
if (exec->bio_async_enabled && exec->bio_ctx) {
    bio_router_register_handler(exec->bio_ctx, BIO_MSG_SWARM_CONSENSUS_REACHED,
                                 handle_swarm_consensus_reached);
    LOG_INFO("Registered swarm consensus handler");
}
```

#### salience_evaluator_create()

Add after bio-async registration:

```c
// Initialize swarm aggregation
eval->swarm = NULL;
eval->swarm_aggregation_enabled = false;
eval->swarm_salience_weight = 0.3f;  // 30% weight to swarm input
eval->swarm_agent_count = 0;
eval->swarm_avg_salience = 0.0f;
nimcp_mutex_init(&eval->swarm_lock, NULL);

// Register swarm message handlers if bio-async enabled
if (eval->bio_async_enabled && eval->bio_ctx) {
    bio_router_register_handler(eval->bio_ctx, BIO_MSG_SWARM_SIGNAL_UPDATE,
                                 handle_swarm_signal_update);
    LOG_INFO("Registered swarm signal handler");
}
```

### 9. Cleanup Changes

#### executive_destroy()

Add before freeing controller:

```c
// Cleanup swarm coordination
if (exec->swarm_coordination_enabled && exec->swarm) {
    nimcp_mutex_destroy(&exec->swarm_lock);
}
```

#### salience_evaluator_destroy()

Add before freeing evaluator:

```c
// Cleanup swarm aggregation
if (eval->swarm_aggregation_enabled && eval->swarm) {
    nimcp_mutex_destroy(&eval->swarm_lock);
}
```

### 10. Security Integration (BBB)

Register swarm interactions with Blood-Brain Barrier:

```c
// In executive_create_custom() or salience_evaluator_create()
if (bbb_system_is_initialized()) {
    // Register swarm message types as trusted
    bbb_register_trusted_message_type(BIO_MSG_SWARM_CONSENSUS_REACHED);
    bbb_register_trusted_message_type(BIO_MSG_SWARM_CONSENSUS_REQUEST);
    bbb_register_trusted_message_type(BIO_MSG_SWARM_SIGNAL_UPDATE);
    bbb_register_trusted_message_type(BIO_MSG_SWARM_SALIENCE_AGGREGATE);
    bbb_register_trusted_message_type(BIO_MSG_EXECUTIVE_DECISION_BROADCAST);

    LOG_INFO("Registered swarm message types with BBB");
}
```

### 11. Integration Flow

#### Distributed Decision Flow

1. **Salience Detection**: Salient event detected
2. **Broadcast to Swarm**: `salience_broadcast_to_swarm()`
3. **Swarm Aggregation**: Multiple agents evaluate salience
4. **Executive Receives**: Aggregated salience informs priority
5. **Decision Making**: Executive makes decision on high-priority task
6. **Request Consensus**: `executive_request_swarm_consensus()`
7. **Swarm Voting**: Agents vote via consensus system
8. **Consensus Reached**: `handle_swarm_consensus_reached()`
9. **Execute Action**: If consensus passed, execute
10. **Broadcast Result**: `executive_broadcast_decision()`

## Testing Requirements

### Unit Tests

#### test_executive_swarm_integration.c

```c
void test_executive_swarm_consensus_request(void)
void test_executive_swarm_consensus_handler(void)
void test_executive_swarm_broadcast(void)
void test_executive_swarm_initialization(void)
void test_executive_swarm_cleanup(void)
```

#### test_salience_swarm_integration.c

```c
void test_salience_swarm_signal_handler(void)
void test_salience_swarm_broadcast(void)
void test_salience_swarm_aggregation(void)
void test_salience_swarm_initialization(void)
void test_salience_swarm_cleanup(void)
```

### Integration Tests

#### test_distributed_decision_flow.cpp

```cpp
// Test full flow:
// 1. Create swarm with 5 agents
// 2. Each agent has executive + salience
// 3. Trigger salient event on agent 1
// 4. Verify broadcast to swarm
// 5. Verify executive requests consensus
// 6. Verify swarm votes
// 7. Verify consensus reached
// 8. Verify decision broadcast
// 9. Verify coordination
```

### Regression Tests

#### test_multi_agent_scenarios.cpp

```cpp
// Scenario 1: Unanimous consensus
// Scenario 2: Split decision (fail consensus)
// Scenario 3: High salience propagation
// Scenario 4: Conflicting salience assessments
// Scenario 5: Timeout handling
// Scenario 6: Byzantine fault tolerance
```

## Performance Considerations

- Swarm coordination adds ~5-10ms latency to decisions
- Bio-async messaging overhead: ~0.1ms per message
- Consensus voting: O(N) where N = number of agents
- Salience aggregation: O(N) for swarm signals
- Memory overhead: ~200 bytes per executive/salience for swarm state

## Configuration

### Executive Swarm Configuration

```c
typedef struct {
    bool enable_swarm_coordination;     // Enable feature
    float consensus_threshold;          // Voting threshold [0,1]
    uint32_t max_pending_proposals;     // Limit concurrent votes
    uint32_t consensus_timeout_ms;      // Voting deadline
} executive_swarm_config_t;
```

### Salience Swarm Configuration

```c
typedef struct {
    bool enable_swarm_aggregation;      // Enable feature
    float swarm_weight;                 // Weight for swarm input [0,1]
    float broadcast_threshold;          // Min salience to broadcast
    uint32_t max_swarm_agents;          // Expected swarm size
} salience_swarm_config_t;
```

## Logging

All swarm interactions use module-specific logging:

- Executive: `LOG_MODULE = "cognitive.executive"`
- Salience: `LOG_MODULE = "salience"`

Key events logged:
- Consensus requests
- Consensus results
- Decision broadcasts
- Salience broadcasts
- Signal updates
- Errors and timeouts

## Error Handling

All functions return `nimcp_error_t`:

- `NIMCP_SUCCESS` - Operation succeeded
- `NIMCP_ERROR_NULL_ARG` - NULL pointer argument
- `NIMCP_ERROR_INVALID_STATE` - Feature not enabled
- `NIMCP_ERROR_TIMEOUT` - Consensus timeout
- `NIMCP_ERROR_INSUFFICIENT_VOTES` - Quorum not met

## Future Enhancements

1. **Adaptive Consensus Thresholds**: Adjust based on situation urgency
2. **Priority-Based Consensus**: Weight votes by agent competence
3. **Hierarchical Swarms**: Multi-level consensus
4. **Partial Consensus**: Act on partial agreement with lower confidence
5. **Learning from Disagreement**: Update models when consensus fails
6. **Cross-Swarm Consensus**: Coordinate between multiple swarms

## References

- NIMCP Bio-Async API: `include/async/nimcp_bio_async.h`
- Swarm Consensus: `include/swarm/nimcp_swarm_consensus.h`
- Executive Functions: `include/cognitive/nimcp_executive.h`
- Salience Detection: `include/cognitive/salience/nimcp_salience.h`
- Blood-Brain Barrier: `include/security/nimcp_blood_brain_barrier.h`
