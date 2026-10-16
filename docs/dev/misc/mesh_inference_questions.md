# NIMCP Mesh Network Inference Questions

This document contains a series of inference questions designed to exercise and validate
all parts of the NIMCP Mesh Network architecture. Each question includes a complete
code walkthrough to verify logic correctness and identify any coverage gaps.

**Purpose**: Validate mesh network logic through systematic code tracing
**Last Updated**: 2025-01-31
**Version**: 1.0.0

---

## Table of Contents

1. [Simple Questions (Single Component)](#simple-questions)
2. [Moderate Questions (Multi-Component)](#moderate-questions)
3. [Complex Questions (Cross-Channel)](#complex-questions)
4. [Advanced Questions (Failure Scenarios)](#advanced-questions)
5. [Coverage Analysis Summary](#coverage-analysis-summary)
6. [Identified Logic Issues](#identified-logic-issues)

---

## Simple Questions

### Q1: Simple Fact Recall - "What is 2 + 2?"

**Components Exercised**: Participant, Channel, Transaction (single channel)

#### Code Walkthrough

**Step 1: Query arrives at participant**
```
File: src/mesh/nimcp_mesh_participant.c:283-286
mesh_participant_register() creates participant with ID encoding:
  - Channel encoded in bits 56-63
  - Type encoded in bits 48-55
  - Module ID in bits 0-47
```

**Step 2: Transaction creation**
```
File: src/mesh/nimcp_mesh_transaction.c:158-192
mesh_tx_manager_create_tx():
  - Allocates transaction with unique ID
  - Sets type to MESH_TX_BELIEF_UPDATE
  - Encodes proposer_id, source_channel, target_channel (same)
  - Sets deadline_ns = now + timeout_ms * 1000000

ISSUE IDENTIFIED: Line 181 - deadline calculation may overflow for large timeout values
  uint64_t deadline_ns = now_ns + (uint64_t)(config->default_timeout_ms * 1000000.0f);
  RECOMMENDATION: Use uint64_t multiplication to avoid float precision loss
```

**Step 3: Propose transaction**
```
File: src/mesh/nimcp_mesh_transaction.c:217-269
mesh_tx_propose():
  - Validates manager and transaction (magic check)
  - Checks state == MESH_TX_STATE_INIT
  - Transitions to MESH_TX_STATE_PROPOSED
  - Adds to pending transactions queue
  - Invokes on_propose callback if set

COVERAGE CHECK: ✓ State validation correct
COVERAGE CHECK: ✓ Thread-safe with mutex lock/unlock
```

**Step 4: Gossip within channel**
```
File: src/mesh/nimcp_mesh_channel.c:557-627
mesh_channel_gossip_round():
  - For each belief in beliefs array:
    - Propagate to random subset (gossip_fanout)
    - Decay certainty: belief.certainty *= (1.0f - 0.1f * delta_factor)
    - Count if certainty > 0.5 for consensus check
  - Compute consensus if count >= channel->consensus_threshold

ISSUE IDENTIFIED: Line 594 - Consensus threshold hardcoded at creation
  Not dynamically adjusted based on active participants
  Could lead to unreachable consensus with participant churn
```

**Result**: For simple fact recall, transaction completes within single channel
with minimal component interaction.

---

### Q2: Basic State Query - "What color is the sky?"

**Components Exercised**: Participant, Channel, World State (CRDT)

#### Code Walkthrough

**Step 1: World state lookup**
```
File: src/mesh/nimcp_mesh_channel.c:359-388
mesh_channel_get_from_world_state():
  - Validates channel handle (magic check)
  - Acquires mutex lock
  - Calls collective_workspace_get() on world_state
  - Returns value or NULL if not found

COVERAGE CHECK: ✓ Proper null handling
COVERAGE CHECK: ✓ Thread-safe access
```

**Step 2: If not in local world state, gossip query**
```
File: src/mesh/nimcp_mesh_channel.c:531-555
mesh_channel_query_state():
  - Creates belief with query content
  - Introduces belief via mesh_channel_introduce_belief()
  - Waits for gossip convergence

ISSUE IDENTIFIED: No timeout on query waiting
  If gossip never converges, query hangs indefinitely
  RECOMMENDATION: Add timeout parameter to mesh_channel_query_state()
```

---

### Q3: Simple Motor Command - "Raise right hand"

**Components Exercised**: Endorsement, Motor Policy, Single Channel

#### Code Walkthrough

**Step 1: Create motor command transaction**
```
File: src/mesh/nimcp_mesh_transaction.c:158-192
mesh_tx_manager_create_tx():
  - Type: MESH_TX_STATE_CHANGE
  - Payload: motor command data
```

**Step 2: Get endorsement policy**
```
File: src/mesh/nimcp_mesh_endorsement.c:388-405
mesh_endorsement_get_policy_for_tx():
  - Iterates through registered policies
  - Returns first matching by tx_type

  Motor policy (line 426-434):
    policy_name = "motor_command"
    expression = "motor_cortex AND cerebellum"
    timeout_ms = 20.0f
```

**Step 3: Collect endorsements**
```
File: src/mesh/nimcp_mesh_endorsement.c:476-522
mesh_endorsement_start_collection():
  - Creates collection entry
  - Sets deadline based on policy timeout
  - Initializes endorsement set

File: src/mesh/nimcp_mesh_endorsement.c:524-565
mesh_endorsement_add():
  - Adds endorsement to collection
  - Checks for duplicates
  - Evaluates policy satisfaction after each add

ISSUE IDENTIFIED: Line 555-562 - Policy satisfaction evaluation
  mesh_endorsement_evaluate_policy() is called after EVERY endorsement add
  For policies with many endorsers, this is O(n*m) where n=endorsements, m=policy endorsers
  RECOMMENDATION: Track required count incrementally instead of re-evaluating
```

**Step 4: Evaluate motor policy**
```
File: src/mesh/nimcp_mesh_endorsement.c:665-708
mesh_endorsement_evaluate_policy():
  - Check for vetoes first (amygdala can veto per plan)
  - Iterate through policy endorsers
  - Count required vs approved
  - Return true if approved_count >= required_count

COVERAGE GAP: Veto check (line 683) only checks ENDORSER_ROLE_VETO
  The plan specifies "NOT amygdala_veto" but current implementation
  doesn't support negative conditions in expressions
  Expression parsing is TODO (line 223-225)
```

---

## Moderate Questions

### Q4: Multi-Modal Sensory Fusion - "What am I looking at?"

**Components Exercised**: Multiple Channels (sensory), Cross-Channel, Endorsement

#### Code Walkthrough

**Step 1: Sensory channels generate beliefs**

Each sensory channel (visual, auditory, somatosensory) operates independently:
```
File: src/mesh/nimcp_mesh_channel.c:478-528
mesh_channel_introduce_belief():
  - Creates belief_t with content, certainty
  - Adds to channel's beliefs array
  - Triggers gossip propagation

Visual Channel: certainty = 0.9 (high confidence in visual data)
Auditory Channel: certainty = 0.7 (ambient noise detected)
Somatosensory: certainty = 0.8 (tactile feedback)
```

**Step 2: Sensory fusion endorsement policy**
```
File: src/mesh/nimcp_mesh_endorsement.c:407-423
Built-in policy not created for sensory_fusion
(Only cognitive, motor, cross_hemisphere, emergency, gpu created)

COVERAGE GAP: Missing MESH_POLICY_SENSORY_FUSION
  Plan specifies: "ANY 2 OF (visual_cortex, auditory_cortex, somatosensory)"
  This policy is not implemented in mesh_endorsement_create_builtin_policies()
  RECOMMENDATION: Add sensory fusion policy with ANY N OF M logic
```

**Step 3: Cross-channel aggregation**
```
File: src/mesh/nimcp_mesh_cross_channel.c:256-323
mesh_cross_router_submit():
  - Validates source and target channels differ
  - Checks MSP authorization
  - Routes to target channel's pending queue

For sensory fusion, need to aggregate from 3 channels to cognitive:
  Visual -> Cognitive (cross-channel)
  Auditory -> Cognitive (cross-channel)
  Somatosensory -> Cognitive (cross-channel)

ISSUE IDENTIFIED: No batch cross-channel support
  Each sensory channel submits separately
  No mechanism to wait for all N inputs before processing
  RECOMMENDATION: Add mesh_cross_router_submit_batch() with aggregation callback
```

**Step 4: Free energy computation for fusion**
```
File: src/mesh/nimcp_mesh_channel.c:647-675
compute_free_energy():
  - Simplified FEP: F ≈ 1 - coherence
  - coherence = sum(certainty) / belief_count

  For 3 sensory inputs:
    coherence = (0.9 + 0.7 + 0.8) / 3 = 0.8
    F = 1 - 0.8 = 0.2 (low free energy = good)
```

---

### Q5: Memory Consolidation - "Remember this phone number: 555-1234"

**Components Exercised**: Hippocampus endorsement, MSP credential check, World State update

#### Code Walkthrough

**Step 1: Create memory store transaction**
```
File: src/mesh/nimcp_mesh_transaction.c:158-192
  Type: MESH_TX_STATE_CHANGE (world state modification)
  Payload: {key: "phone_number", value: "555-1234", salience: 0.6}
```

**Step 2: MSP validation**
```
File: src/mesh/nimcp_mesh_msp.c:452-534
mesh_msp_validate_transaction():
  - Authenticate proposer (line 467-477)
  - Check quarantine/revocation status (line 479-488)
  - Validate channel membership (line 490-505)
  - Check MSP capabilities (line 507-522)

COVERAGE CHECK: ✓ Full validation chain implemented
```

**Step 3: Memory store endorsement (missing)**
```
COVERAGE GAP: No MESH_POLICY_MEMORY_STORE policy
  Plan specifies: "hippocampus AND (PFC OR emotional_salience > 0.7)"

  The expression parser (nimcp_mesh_endorsement.c:217-226) is stub:
    mesh_endorsement_policy_parse() returns NIMCP_SUCCESS but doesn't parse

  No mechanism to evaluate:
    - OR conditions
    - Threshold comparisons (emotional_salience > 0.7)

  RECOMMENDATION: Implement expression parser with AST evaluation
```

**Step 4: World state update**
```
File: src/mesh/nimcp_mesh_channel.c:336-358
mesh_channel_put_to_world_state():
  - Validates channel
  - Calls collective_workspace_put() - CRDT merge
  - CRDT handles concurrent writes via LWW (Last Writer Wins)

COVERAGE CHECK: ✓ CRDT integration correct
```

---

### Q6: Coordinator Pool Election - "Leader coordinator fails"

**Components Exercised**: Coordinator Pool, BFT Election, Failover

#### Code Walkthrough

**Step 1: Detect failure**
```
File: src/mesh/nimcp_mesh_coordinator_pool.c:1022-1068
mesh_coordinator_pool_update():
  - Updates all coordinators
  - Checks leader health (line 1038-1045)
  - If health < 0.3f, triggers election

File: src/mesh/nimcp_mesh_coordinator.c:509-532
mesh_coordinator_report_failure():
  - Increments consecutive_failures
  - Updates health: health = 1 - (failures / MAX_FAILURES)
  - If health == 0, marks state as COORD_STATE_FAILED
```

**Step 2: Handle failure and migrate participants**
```
File: src/mesh/nimcp_mesh_coordinator_pool.c:928-974
mesh_coordinator_pool_handle_failure():
  - Marks coordinator as COORD_STATE_FAILED
  - Retrieves assigned participants (max 64)
  - For each participant:
    - Unassign from failed coordinator
    - Find least loaded worker
    - Reassign to new coordinator
  - If was leader, trigger election

ISSUE IDENTIFIED: Line 949-951 - Only 64 participants migrated
  mesh_participant_id_t participants[64];
  If coordinator had > 64 participants, some are lost!
  RECOMMENDATION: Loop until all participants migrated or use dynamic array
```

**Step 3: BFT leader election**
```
File: src/mesh/nimcp_mesh_coordinator_pool.c:618-729
mesh_coordinator_pool_elect_leader():
  - Increment term
  - Each active coordinator votes:
    - If health > 0.5, vote for self
    - Otherwise, vote for healthiest coordinator
  - Check for winner: votes >= quorum
    - quorum = (active_count * 0.67) + 1

  Line 206: MESH_VOTE_QUORUM constant used
  get_election_winner() at line 202-214:
    quorum = (size_t)((float)active * MESH_VOTE_QUORUM) + 1

ISSUE IDENTIFIED: Line 206 quorum calculation
  For 3 coordinators: quorum = (3 * 0.67) + 1 = 3.01 -> 3
  This means ALL 3 must agree, not 2/3 majority
  With any disagreement, no winner is selected

  BFT requires: N >= 3f + 1 for f faults
  For 3 nodes, f = 0 (can't tolerate any faults!)

  RECOMMENDATION: Ensure minimum 4 coordinators for fault tolerance
  Or adjust quorum: quorum = (active / 2) + 1 for majority
```

**Step 4: Promote new leader**
```
File: src/mesh/nimcp_mesh_coordinator_pool.c:679-720
On election winner:
  - Demote old leader to COORD_ROLE_WORKER
  - Set new leader role to COORD_ROLE_LEADER
  - Update pool's leader_index and leader_id
  - Record election result
  - Invoke election callback if registered

COVERAGE CHECK: ✓ Leader transition logic correct
```

---

## Complex Questions

### Q7: Cross-Hemisphere Decision - "Should I accept this job offer?"

**Components Exercised**: Both Hemispheres, Cross-Channel, System Coordinator, FEP Arbitration

#### Code Walkthrough

**Step 1: Left hemisphere analysis (analytical)**
```
Channel: LEFT_HEMISPHERE
Processing: salary, benefits, commute time, career growth
Creates transaction with analytical assessment
```

**Step 2: Right hemisphere analysis (holistic)**
```
Channel: RIGHT_HEMISPHERE
Processing: gut feeling, work-life balance, creative opportunities
Creates transaction with holistic assessment
```

**Step 3: Cross-hemisphere endorsement**
```
File: src/mesh/nimcp_mesh_endorsement.c:437-445
Policy: MESH_POLICY_CROSS_HEMISPHERE
  expression = "left_leader AND right_leader"
  timeout_ms = 75.0f

Both hemisphere leaders must endorse the decision.
```

**Step 4: Cross-channel submission**
```
File: src/mesh/nimcp_mesh_cross_channel.c:256-323
mesh_cross_router_submit():
  - Line 280-290: Check not cross-channel if same channel
  - Line 293-303: MSP validation for cross-channel

For cross-hemisphere:
  left_tx.source_channel = LEFT_HEMISPHERE
  left_tx.target_channel = SYSTEM (for arbitration)

  right_tx.source_channel = RIGHT_HEMISPHERE
  right_tx.target_channel = SYSTEM (for arbitration)
```

**Step 5: System coordinator arbitration**
```
File: src/mesh/nimcp_mesh_cross_channel.c:382-453
mesh_system_coord_arbitrate():
  - Compute free energy for both transactions
  - Winner = lower free energy (more coherent)
  - Commit winner through ordering service

File: src/mesh/nimcp_mesh_cross_channel.c:456-498
mesh_system_coord_compute_free_energy():
  F = prediction_error + complexity_penalty

  prediction_error = fabs(outcome - prediction) based on tx payload
  complexity_penalty = 0.1 * endorsement_count

ISSUE IDENTIFIED: Line 478-485 - Simplified FE calculation
  Uses arbitrary payload interpretation:
    outcome = *(float*)tx->payload
    prediction = *(float*)(tx->payload + sizeof(float))

  This assumes specific payload structure not enforced
  RECOMMENDATION: Define mesh_tx_fep_payload_t struct with explicit fields
```

**Step 6: Commit winner**
```
File: src/mesh/nimcp_mesh_cross_channel.c:430-435
  mesh_ordering_submit(router->ordering, winner);

Winner transaction goes through ordering service for sequencing.
Loser's channel receives notification (not implemented).

COVERAGE GAP: No notification mechanism for losing transaction
  mesh_system_coord_arbitrate() commits winner but doesn't notify loser
  RECOMMENDATION: Add callback or event for arbitration results
```

---

### Q8: Emergency Override - "Sudden danger detected!"

**Components Exercised**: Amygdala fast-path, Emergency Policy, MSP Bypass

#### Code Walkthrough

**Step 1: Emergency transaction creation**
```
File: src/mesh/nimcp_mesh_transaction.c:158-192
  Type: MESH_TX_EMERGENCY_OVERRIDE
  Sets high priority flag
```

**Step 2: Emergency endorsement policy**
```
File: src/mesh/nimcp_mesh_endorsement.c:448-456
Policy: MESH_POLICY_EMERGENCY_OVERRIDE
  expression = "amygdala"
  timeout_ms = 10.0f (very fast)

File: src/mesh/nimcp_mesh_endorsement.c:677-679
Emergency policy with 0 endorsers = auto-satisfied:
  if (policy->endorser_count == 0) {
    return true;  // Emergency override bypasses normal endorsement
  }

ISSUE IDENTIFIED: Emergency policy has 0 endorsers!
  mesh_endorsement_policy_create() at line 448 creates policy but
  never calls mesh_endorsement_policy_add_endorser() for amygdala

  Result: Emergency transactions ALWAYS satisfied immediately
  This bypasses any safety checks!

  RECOMMENDATION: Add amygdala as required endorser for emergency policy
```

**Step 3: Fast-path processing**
```
File: src/mesh/nimcp_mesh_ordering.c:458-497
mesh_ordering_submit():
  Regular transactions go to pending queue

  No priority handling for emergency transactions!

COVERAGE GAP: No priority queue for emergencies
  All transactions processed in FIFO order
  Emergency doesn't jump the queue

  RECOMMENDATION: Add priority field to transaction
  Check priority in mesh_ordering_create_batch() to process emergencies first
```

---

### Q9: GPU Batch Processing - "Process 1000 visual frames"

**Components Exercised**: GPU Channel, Batch Collection, Recovery Integration

#### Code Walkthrough

**Step 1: GPU batch transaction**
```
File: src/mesh/nimcp_mesh_endorsement.c:459-467
Policy: MESH_POLICY_GPU_BATCH
  expression = "gpu_coordinator"
  timeout_ms = 25.0f
```

**Step 2: Batch collection**

Looking at the mesh implementation, there's no explicit GPU channel code in the
files I've read. The plan mentions `src/mesh/nimcp_mesh_gpu.c` but checking...

```
COVERAGE GAP: GPU channel implementation missing
  Plan Phase 7 specified nimcp_mesh_gpu.c and nimcp_mesh_gpu.h
  These files appear to not exist or weren't included in implementation

  The gpu_channel_t structure from plan is not implemented:
    - gpu_coordinators pool
    - multi-GPU support
    - batch processing
    - recovery context integration

  CRITICAL: Phase 7 of plan was not implemented
  RECOMMENDATION: Implement GPU channel module per plan specification
```

**Step 3: Recovery integration (not implemented)**
```
COVERAGE GAP: GPU recovery integration missing
  Plan specified:
    NIMCP_GPU_TRY_WITH_RECOVERY macro integration
    4-tier recovery (retry, device reset, multi-GPU, CPU fallback)

  Without GPU channel, recovery integration not possible
```

---

### Q10: Full Transaction Flow - "Learn that Paris is in France"

**Components Exercised**: Complete EOV Flow, All Components

#### Code Walkthrough

**Phase 1: EXECUTE (Propose)**
```
File: src/mesh/nimcp_mesh_transaction.c:217-269
mesh_tx_propose():
  1. Validate tx state == MESH_TX_STATE_INIT
  2. Set state = MESH_TX_STATE_PROPOSED
  3. Set proposed_at timestamp
  4. Add to pending queue
  5. Invoke on_propose callback

State: INIT -> PROPOSED
```

**Phase 2: ENDORSE**
```
File: src/mesh/nimcp_mesh_endorsement.c:476-565
1. mesh_endorsement_start_collection() - Initialize collection
2. Request endorsements from policy endorsers
3. mesh_endorsement_add() - Collect responses
4. mesh_endorsement_evaluate_policy() - Check satisfaction

File: src/mesh/nimcp_mesh_transaction.c:288-349
mesh_tx_add_endorsement():
  1. Validate tx in PROPOSED or ENDORSED state
  2. Check endorsement count < capacity
  3. Verify not duplicate endorser
  4. Add endorsement to tx
  5. Check if policy satisfied
  6. If satisfied, transition to ENDORSED

State: PROPOSED -> ENDORSED
```

**Phase 3: ORDER**
```
File: src/mesh/nimcp_mesh_ordering.c:458-497
mesh_ordering_submit():
  1. Validate ordering service
  2. Lock queue mutex
  3. Check queue not full
  4. Add transaction to pending queue
  5. Increment stats

File: src/mesh/nimcp_mesh_ordering.c:500-561
mesh_ordering_create_batch():
  1. Lock mutex
  2. Move transactions from pending to current batch
  3. Up to batch_size transactions
  4. Clear pending queue

File: src/mesh/nimcp_mesh_ordering.c:564-620
mesh_ordering_sequence_batch():
  1. Verify is leader (Raft)
  2. For each transaction in batch:
     - Assign sequence number (atomic increment)
     - Set sequenced_at timestamp
     - Transition to ORDERED state
  3. Replicate to followers (TODO)

State: ENDORSED -> ORDERED
```

**Phase 4: VALIDATE**
```
File: src/mesh/nimcp_mesh_transaction.c:362-429
mesh_tx_validate():
  1. Verify state == ORDERED
  2. Validate all endorsements present
  3. Verify signatures (TODO - always returns true)
  4. Check no conflicts with existing state
  5. Transition to VALIDATED

ISSUE IDENTIFIED: Line 403 - Signature verification is stub
  mesh_endorsement_verify_signature() always returns true (line 820-826)
  No actual cryptographic verification

  RECOMMENDATION: Implement signature verification using nimcp_crypto

State: ORDERED -> VALIDATED
```

**Phase 5: COMMIT**
```
File: src/mesh/nimcp_mesh_transaction.c:432-494
mesh_tx_commit():
  1. Verify state == VALIDATED
  2. Apply to world state (CRDT merge)
  3. Set committed_at timestamp
  4. Transition to COMMITTED
  5. Invoke on_commit callback
  6. Remove from pending

State: VALIDATED -> COMMITTED
```

**Complete State Machine:**
```
INIT -> PROPOSED -> ENDORSED -> ORDERED -> VALIDATED -> COMMITTED
                                                      \
                                                       -> ABORTED (on failure)
```

---

## Advanced Questions

### Q11: Byzantine Fault - "Coordinator sends conflicting messages"

**Components Exercised**: BFT Consensus, Vote Validation, Fault Detection

#### Code Walkthrough

**Scenario**: Coordinator C1 sends vote for A to C2, vote for B to C3

**Step 1: Vote processing**
```
File: src/mesh/nimcp_mesh_coordinator_pool.c:731-766
mesh_coordinator_pool_process_vote():
  - Adds vote to tally
  - Checks for winner
  - No validation that voter hasn't voted twice!

ISSUE IDENTIFIED: No duplicate vote detection
  A Byzantine coordinator can vote multiple times for different candidates
  add_vote() at line 186-197 doesn't check if voter already voted

  RECOMMENDATION: Track who voted (not just vote counts)
  Add: voted_participants hashset to verify single vote per coordinator
```

**Step 2: BFT validity check**
```
File: src/mesh/nimcp_mesh_coordinator_pool.c:1208-1219
mesh_coordinator_pool_is_bft_valid():
  - Calculates tolerated faults: (N-1)/3
  - Returns true if failed <= tolerated

  For N=4: tolerated = 1 fault
  For N=7: tolerated = 2 faults

This only checks count, not Byzantine behavior detection.

COVERAGE GAP: No Byzantine behavior detection
  BFT requires:
    - Detecting conflicting messages
    - Evidence collection for accountability
    - View change protocol for stuck states

  RECOMMENDATION: Implement message signing and conflict detection
```

---

### Q12: Network Partition - "Half the coordinators become unreachable"

**Components Exercised**: Quorum, Election Timeout, Partition Healing

#### Code Walkthrough

**Scenario**: 4 coordinators, 2 become unreachable

**Step 1: Heartbeat timeout detection**
```
File: src/mesh/nimcp_mesh_coordinator.c:605-611
mesh_coordinator_heartbeat_timed_out():
  timeout_ns = election_timeout_ms * 1000000
  return (now - last_heartbeat_ns) > timeout_ns

Each coordinator individually detects missing heartbeats.
```

**Step 2: Election attempt**
```
File: src/mesh/nimcp_mesh_coordinator_pool.c:618-729
mesh_coordinator_pool_elect_leader():
  - Only active coordinators participate
  - get_active_count() returns 2 (the reachable ones)
  - quorum = (2 * 0.67) + 1 = 2.34 -> 2

  If both reachable coordinators vote for same candidate: election succeeds
  If they vote differently: no winner (split vote)
```

**Step 3: Partition impact**
```
With 4 coordinators split 2-2:
  - Each partition might elect its own leader
  - Results in split-brain scenario!

ISSUE IDENTIFIED: No partition detection
  Each partition thinks it's the valid cluster
  Could have 2 leaders processing transactions

  RECOMMENDATION:
    - Require majority of ORIGINAL cluster size, not active
    - Track cluster membership explicitly
    - Add partition detection via epoch/view numbers
```

**Step 4: Partition healing**
```
File: src/mesh/nimcp_mesh_coordinator_pool.c:1022-1068
mesh_coordinator_pool_update():
  When partitioned nodes reconnect:
  - They receive heartbeats with higher term
  - mesh_coordinator_receive_heartbeat() updates term

  But no reconciliation of divergent state!

COVERAGE GAP: No partition recovery protocol
  After split-brain, world states may have diverged
  CRDT provides eventual consistency but doesn't detect conflicts

  RECOMMENDATION: Implement epoch-based state reconciliation
```

---

### Q13: Cascading Failure - "Multiple components fail simultaneously"

**Components Exercised**: Immune System, MSP Quarantine, Recovery

#### Code Walkthrough

**Scenario**: Malformed transaction causes 3 participants to crash

**Step 1: First failure**
```
File: src/mesh/nimcp_mesh_msp.c:635-686
mesh_msp_on_immune_event():
  - IMMUNE_EVENT_THREAT_DETECTED -> quarantine
  - IMMUNE_EVENT_CHRONIC_FAILURE -> revoke
  - IMMUNE_EVENT_RECOVERY -> restore

On first failure:
  - immune_present_antigen() called
  - MSP receives callback
  - Quarantines failing participant
```

**Step 2: Cascade detection**
```
COVERAGE GAP: No cascade detection
  Each failure processed independently
  No pattern recognition for "3 failures in 100ms from same channel"

  MSP tracks quarantined set but doesn't analyze patterns:
    nimcp_hashset_t* quarantined;  // Just a set, no time tracking

  RECOMMENDATION: Add failure rate tracking per channel/coordinator
  Trigger circuit breaker if rate exceeds threshold
```

**Step 3: Recovery coordination**
```
File: src/mesh/nimcp_mesh_msp.c:679-684
On IMMUNE_EVENT_RECOVERY:
  - Remove from quarantined set
  - Log restoration

  No coordination with:
  - Coordinator pool (participant might need reassignment)
  - Channel (might need re-gossip of missed beliefs)
  - Transaction manager (might have pending transactions)

COVERAGE GAP: Incomplete recovery coordination
  RECOMMENDATION: Add recovery callbacks to all affected components
```

---

### Q14: Ordering Service Raft Failover - "Leader orderer crashes during batch"

**Components Exercised**: Raft Consensus, Log Replication, Leader Election

#### Code Walkthrough

**Step 1: Leader crash during batch**
```
File: src/mesh/nimcp_mesh_ordering.c:564-620
mesh_ordering_sequence_batch():
  Leader is sequencing transactions when crash occurs
  Some transactions have sequence numbers, others don't
```

**Step 2: Follower detection**
```
File: src/mesh/nimcp_mesh_ordering.c:766-819
mesh_ordering_tick():
  - Checks election timeout
  - If leader heartbeat timed out, starts election

ISSUE IDENTIFIED: Line 789-800 - Election start
  mesh_ordering_start_election():
    - Transitions to CANDIDATE
    - Votes for self
    - Requests votes from followers

  But partially sequenced batch is lost!
  Transactions in leader's memory not replicated yet.
```

**Step 3: Log replication status**
```
File: src/mesh/nimcp_mesh_ordering.c:623-689
mesh_ordering_replicate_batch():
  - TODO comment at line 650: "Send batch to followers"
  - Actual replication not implemented!

CRITICAL COVERAGE GAP: Raft log replication not implemented
  mesh_ordering_replicate_batch() only updates local state
  No actual communication with followers

  Without replication:
    - Followers have no transaction data
    - New leader has empty log
    - ALL in-flight transactions lost on leader failure

  RECOMMENDATION: Implement actual RPC/message passing for replication
```

---

### Q15: Resource Exhaustion - "Transaction queue fills up"

**Components Exercised**: Backpressure, Queue Management, Error Handling

#### Code Walkthrough

**Step 1: Queue capacity check**
```
File: src/mesh/nimcp_mesh_ordering.c:458-497
mesh_ordering_submit():
  Line 478-482:
    if (ordering->pending_count >= MESH_ORDERING_MAX_PENDING) {
        nimcp_mutex_unlock(ordering->mutex);
        return NIMCP_ERROR_CAPACITY_EXCEEDED;
    }

  Returns error when queue full.
```

**Step 2: Caller handling**
```
File: src/mesh/nimcp_mesh_transaction.c - No backpressure handling
  mesh_tx_propose() doesn't check ordering queue before proposing

COVERAGE GAP: No backpressure propagation
  When ordering queue is full:
    - Transactions still get proposed
    - Endorsements still collected
    - Only fails at submit time
    - Wasted endorsement work

  RECOMMENDATION: Add queue depth check in mesh_tx_propose()
  Return NIMCP_ERROR_BACKPRESSURE if ordering queue > 80% full
```

**Step 3: Queue statistics**
```
File: src/mesh/nimcp_mesh_ordering.c:948-983
mesh_ordering_get_stats():
  Provides:
    - pending_count
    - transactions_ordered
    - batches_created
    - avg_batch_size

  No queue_utilization_percent for monitoring!

  RECOMMENDATION: Add utilization metric for alerting
```

---

## Coverage Analysis Summary

### Components Fully Covered
- [x] Transaction lifecycle (INIT -> COMMITTED)
- [x] Participant registration
- [x] Basic channel operations
- [x] World state CRDT operations
- [x] Coordinator pool lifecycle
- [x] Basic endorsement collection

### Components Partially Covered
- [~] Leader election (no Byzantine detection)
- [~] Endorsement policies (expression parsing incomplete)
- [~] MSP validation (signature verification stub)
- [~] Coordinator failover (participant limit issue)
- [~] Cross-channel routing (no batch support)

### Components Missing or Incomplete
- [ ] GPU channel (Phase 7 not implemented)
- [ ] Raft log replication
- [ ] Expression parser for policies
- [ ] Priority queue for emergencies
- [ ] Partition recovery protocol
- [ ] Cascade failure detection
- [ ] Backpressure propagation

---

## Identified Logic Issues

### Critical Issues

| ID | Location | Description | Recommendation |
|----|----------|-------------|----------------|
| C1 | coordinator_pool.c:949 | Only 64 participants migrated on failure | Use dynamic array or loop |
| C2 | ordering.c:650 | Raft replication not implemented | Implement RPC for log sync |
| C3 | endorsement.c:448 | Emergency policy has 0 endorsers | Add amygdala endorser |
| C4 | coordinator_pool.c:206 | Quorum requires ALL (not 2/3) | Fix quorum calculation |

### Moderate Issues

| ID | Location | Description | Recommendation |
|----|----------|-------------|----------------|
| M1 | transaction.c:181 | Float overflow in deadline calc | Use uint64 multiplication |
| M2 | channel.c:594 | Static consensus threshold | Dynamic based on active count |
| M3 | endorsement.c:555 | O(n*m) policy evaluation | Incremental counting |
| M4 | cross_channel.c:478 | Unstructured FE payload | Define explicit struct |
| M5 | transaction.c:403 | Signature verification stub | Implement crypto verification |

### Minor Issues

| ID | Location | Description | Recommendation |
|----|----------|-------------|----------------|
| L1 | channel.c:531 | No query timeout | Add timeout parameter |
| L2 | cross_channel.c:430 | No loser notification | Add callback mechanism |
| L3 | ordering.c:948 | No utilization metric | Add queue percentage |

---

## Test Cases to Add

Based on this analysis, the following test cases should be added:

### Unit Tests
1. `test_coordinator_pool_migrate_many_participants` - >64 participants
2. `test_endorsement_emergency_with_endorser` - Verify amygdala required
3. `test_ordering_quorum_calculation` - Verify 2/3 not 3/3

### Integration Tests
4. `test_partition_split_brain_prevention` - No dual leaders
5. `test_cascade_failure_circuit_breaker` - Rate limiting
6. `test_backpressure_propagation` - Queue depth check

### Regression Tests
7. `test_deadline_overflow` - Large timeout values
8. `test_signature_verification` - When implemented
9. `test_raft_replication` - When implemented

---

## Document Revision History

| Version | Date | Changes |
|---------|------|---------|
| 1.0.0 | 2025-01-31 | Initial document with 15 questions and walkthroughs |
