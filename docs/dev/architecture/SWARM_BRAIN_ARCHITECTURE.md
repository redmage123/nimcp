# NIMCP Swarm Brain Architecture

## Executive Summary

This architecture enables a **swarm of constrained NIMCP brains** (drones, IoT devices) to communicate via speech cortex encoding over radio/signals, forming a **unified meta-brain** that exhibits emergent cognitive capabilities beyond individual units.

**Biological Analogy**: Like neurons in a brain, individual drone brains are limited alone, but when connected form a distributed consciousness with emergent intelligence.

---

## Architecture Overview

```
                           ┌──────────────────────────────────────────┐
                           │         SWARM META-BRAIN                 │
                           │  (Emergent when N > critical_mass)       │
                           │                                          │
                           │  ┌──────────────────────────────────┐   │
                           │  │   Collective Global Workspace     │   │
                           │  │   - Shared salience filtering     │   │
                           │  │   - Distributed working memory    │   │
                           │  │   - Consensus voting              │   │
                           │  └──────────────────────────────────┘   │
                           └──────────────────────────────────────────┘
                                              ▲
                                              │ Emergence
                                              │
    ┌──────────────────────────────────────────────────────────────────────────┐
    │                      SWARM COMMUNICATION LAYER                            │
    │   ┌─────────────┐    ┌─────────────┐    ┌─────────────┐                  │
    │   │ Phoneme→    │    │ Radio       │    │ →Phoneme    │                  │
    │   │ Signal      │───►│ Broadcast   │───►│ Decoder     │                  │
    │   │ Encoder     │    │ (RF/LoRa)   │    │             │                  │
    │   └─────────────┘    └─────────────┘    └─────────────┘                  │
    │                                                                           │
    │   Protocol: NIMCP Speech Cortex Phonemes (44 symbols)                     │
    │   Encoding: 6-bit phoneme + 8-bit payload + CRC                           │
    └──────────────────────────────────────────────────────────────────────────┘
                    ▲                    ▲                    ▲
                    │                    │                    │
         ┌──────────┴──────────┐ ┌──────┴──────┐ ┌──────────┴──────────┐
         │   DRONE BRAIN #1    │ │   DRONE #2   │ │   DRONE BRAIN #N    │
         │   (Constrained)     │ │   (Const.)   │ │   (Constrained)     │
         │                     │ │              │ │                     │
         │  ┌───────────────┐  │ │  ┌────────┐  │ │  ┌───────────────┐  │
         │  │ Neural Core   │  │ │  │ Neural │  │ │  │ Neural Core   │  │
         │  │ (200 neurons) │  │ │  │ Core   │  │ │  │ (200 neurons) │  │
         │  └───────────────┘  │ │  └────────┘  │ │  └───────────────┘  │
         │  ┌───────────────┐  │ │              │ │  ┌───────────────┐  │
         │  │ Visual Cortex │  │ │              │ │  │ Audio Cortex  │  │
         │  │ (130KB drone) │  │ │              │ │  │ (13KB drone)  │  │
         │  └───────────────┘  │ │              │ │  └───────────────┘  │
         │  ┌───────────────┐  │ │              │ │  ┌───────────────┐  │
         │  │ Speech Cortex │◄─┼─┼──────────────┼─┼─►│ Speech Cortex │  │
         │  │ (encode/recv) │  │ │              │ │  │ (encode/recv) │  │
         │  └───────────────┘  │ │              │ │  └───────────────┘  │
         └─────────────────────┘ └──────────────┘ └─────────────────────┘
```

---

## Architecture Decision Records (ADRs)

### ADR-007: Swarm Communication Protocol Using Speech Cortex Phonemes

**Status**: Proposed

**Context**: Drones need to communicate cognitive state and commands over limited-bandwidth radio links. Traditional protocols don't map to neural representations.

**Decision**: Use speech cortex phonemes (44 symbols) as the communication encoding, mapping neural states to "utterances" that other drones decode.

**Protocol Format**:
```c
typedef struct {
    uint8_t phoneme_sequence[8];  // Up to 8 phonemes per message
    uint8_t sequence_length;      // Actual phoneme count
    uint8_t message_type;         // See swarm_message_type_t
    uint16_t sender_id;           // Drone ID
    float payload[4];             // Type-specific data
    uint16_t crc16;               // Error detection
} swarm_phoneme_message_t;       // 24 bytes total
```

**Message Types**:
```c
typedef enum {
    SWARM_MSG_HEARTBEAT,        // "I am here" (phoneme: /hɛlo/)
    SWARM_MSG_THREAT_DETECTED,  // "Danger!" (phoneme: /deɪnʤər/)
    SWARM_MSG_TARGET_FOUND,     // "Found it" (phoneme: /faʊnd/)
    SWARM_MSG_REQUEST_BACKUP,   // "Help me" (phoneme: /hɛlp/)
    SWARM_MSG_FORMATION_CHANGE, // "Move to" (phoneme: /muːv/)
    SWARM_MSG_REWARD_SIGNAL,    // "Good!" (phoneme: /gʊd/)
    SWARM_MSG_NEUROMOD_SYNC,    // Chemical state sync
    SWARM_MSG_WORKSPACE_BROADCAST, // Global workspace item
    SWARM_MSG_VOTE_REQUEST,     // Consensus voting
    SWARM_MSG_VOTE_RESPONSE,    // Vote response
} swarm_message_type_t;
```

**Why Phonemes**:
1. Compact: 6 bits per phoneme (44 symbols)
2. Robust: Categorical perception tolerates noise
3. Neural: Maps directly to speech cortex representations
4. Learnable: Swarm can evolve new "words" via plasticity

**Consequences**:
- 24-byte messages fit in LoRa payload
- Speech cortex already handles encoding/decoding
- Human operators can "listen" to swarm chatter

**Integration Point**: `nimcp_speech_cortex.h`, new `nimcp_swarm_protocol.h`

---

### ADR-008: Collective Global Workspace

**Status**: Proposed

**Context**: Individual drone brains have local global workspaces. The swarm needs a meta-workspace for shared attention and decision-making.

**Decision**: Implement a distributed global workspace using existing bio-async infrastructure with CRDT-based conflict resolution.

**Architecture**:
```c
typedef struct {
    // Workspace item (broadcast to swarm)
    uint32_t item_id;           // Unique ID (drone_id << 16 | local_id)
    float salience;             // Priority [0,1]
    uint64_t vector_clock[8];   // Causality tracking (max 8 drones)
    workspace_item_type_t type; // Perception, goal, memory, etc.
    float content[16];          // Item content vector
} collective_workspace_item_t;

typedef struct collective_workspace {
    collective_workspace_item_t items[32]; // Top-32 salient items
    uint32_t item_count;
    uint32_t local_drone_id;
    uint32_t swarm_size;

    // Conflict resolution (CRDT: Last-Writer-Wins)
    uint64_t local_clock;

    // Emergence tracking
    float collective_coherence;  // How aligned is the swarm? [0,1]
    bool meta_cognition_active;  // Swarm > critical mass?
} collective_workspace_t;
```

**Broadcast Policy**:
- Items with salience > 0.7 broadcast to swarm
- Incoming items merged via salience comparison
- Top-32 items retained (Cowan's 4±1 × 8 drones)

**Consequences**:
- Swarm develops shared attention
- High-salience threats propagate instantly
- Emergent consensus on goals

**Integration Point**: `nimcp_global_workspace.h`, new `nimcp_collective_workspace.h`

---

### ADR-009: Emergent Meta-Cognition Trigger

**Status**: Proposed

**Context**: Individual constrained brains lack higher cognitive functions (reasoning, planning). When swarm size exceeds critical mass, emergent capabilities should activate.

**Decision**: Implement tiered cognitive emergence based on swarm connectivity.

**Emergence Tiers**:
```c
typedef enum {
    SWARM_TIER_INDIVIDUAL,    // N=1: Local reactive behavior only
    SWARM_TIER_PAIR,          // N=2-3: Cooperative sensing, simple consensus
    SWARM_TIER_SQUAD,         // N=4-7: Distributed working memory, formation
    SWARM_TIER_PLATOON,       // N=8-15: Meta-attention, collective planning
    SWARM_TIER_COMPANY,       // N=16-31: Emergent reasoning, theory of swarm
    SWARM_TIER_BATTALION      // N≥32: Full meta-cognition, swarm consciousness
} swarm_emergence_tier_t;
```

**Capability Unlocks**:
| Tier | Drones | Unlocked Capability | Biological Analogy |
|------|--------|--------------------|--------------------|
| INDIVIDUAL | 1 | Reactive behavior | Single neuron |
| PAIR | 2-3 | Stereo sensing, confirmation | Neural pair |
| SQUAD | 4-7 | Formation, shared memory | Mini-column |
| PLATOON | 8-15 | Collective attention, planning | Cortical column |
| COMPANY | 16-31 | Reasoning, prediction | Brain region |
| BATTALION | 32+ | Meta-cognition, self-model | Whole brain |

**Implementation**:
```c
swarm_emergence_tier_t swarm_get_emergence_tier(swarm_brain_t swarm) {
    uint32_t connected = swarm_get_connected_count(swarm);
    uint32_t healthy = swarm_get_healthy_count(swarm);
    float coherence = swarm_get_collective_coherence(swarm);

    // Require both quantity AND coherence for emergence
    if (healthy >= 32 && coherence > 0.7) return SWARM_TIER_BATTALION;
    if (healthy >= 16 && coherence > 0.6) return SWARM_TIER_COMPANY;
    if (healthy >= 8 && coherence > 0.5)  return SWARM_TIER_PLATOON;
    if (healthy >= 4 && coherence > 0.4)  return SWARM_TIER_SQUAD;
    if (healthy >= 2 && coherence > 0.3)  return SWARM_TIER_PAIR;
    return SWARM_TIER_INDIVIDUAL;
}
```

**Consequences**:
- Single drone: Reactive only (can't reason)
- 8+ coherent drones: Planning emerges
- 32+ coherent drones: Meta-cognition awakens
- Graceful degradation when drones lost

**Integration Point**: New `nimcp_swarm_emergence.h`

---

### ADR-010: Consensus Voting Mechanism

**Status**: Proposed

**Context**: Swarm needs to make collective decisions (target selection, formation changes) without central authority.

**Decision**: Implement Byzantine-fault-tolerant voting using speech cortex "vote" phonemes.

**Voting Protocol**:
```c
typedef struct {
    uint32_t proposal_id;       // Unique proposal ID
    uint32_t proposer_drone;    // Who proposed
    swarm_vote_topic_t topic;   // What we're voting on
    float proposal_value[4];    // Proposed action
    uint64_t deadline_ms;       // Vote deadline
} swarm_vote_proposal_t;

typedef struct {
    uint32_t proposal_id;
    uint32_t voter_drone;
    swarm_vote_choice_t choice; // AGREE, DISAGREE, ABSTAIN
    float confidence;           // Voter confidence [0,1]
} swarm_vote_response_t;

typedef enum {
    VOTE_TOPIC_TARGET_PRIORITY,    // Which target to pursue
    VOTE_TOPIC_FORMATION_CHANGE,   // Change formation
    VOTE_TOPIC_RETREAT,            // Retreat decision
    VOTE_TOPIC_RESOURCE_ALLOCATION,// Resource sharing
    VOTE_TOPIC_LEADER_ELECTION,    // Temporary leader
} swarm_vote_topic_t;
```

**Voting Rules**:
- Quorum: ≥50% of healthy drones must vote
- Threshold: ≥66% weighted agreement (by confidence)
- Timeout: 100ms default (adjustable)
- Fallback: Highest-salience proposer wins on tie

**Phoneme Encoding**:
```
VOTE_REQUEST:  /voʊt/ + /kwɛsʧən/ + [topic phoneme]
VOTE_AGREE:    /jɛs/ + [proposal_id phoneme]
VOTE_DISAGREE: /noʊ/ + [proposal_id phoneme]
VOTE_ABSTAIN:  /pæs/ + [proposal_id phoneme]
```

**Consequences**:
- Decentralized decision-making
- Tolerates 1/3 Byzantine (compromised) drones
- Fast consensus (<100ms typical)

**Integration Point**: New `nimcp_swarm_consensus.h`

---

### ADR-011: Radio/Signal Adapter Layer

**Status**: Proposed

**Context**: Speech cortex outputs phonemes, but drones communicate via radio (LoRa, WiFi, etc.). Need adapter layer.

**Decision**: Create signal adapter that converts phoneme sequences to/from radio-appropriate encoding.

**Adapter Interface**:
```c
typedef struct swarm_signal_adapter swarm_signal_adapter_t;

// Create adapter for specific radio type
swarm_signal_adapter_t* swarm_signal_adapter_create(
    swarm_radio_type_t radio_type,  // LORA, WIFI, BLUETOOTH, ULTRASONIC
    const swarm_signal_config_t* config
);

// Encode phoneme message to radio signal
bool swarm_signal_encode(
    swarm_signal_adapter_t* adapter,
    const swarm_phoneme_message_t* message,
    uint8_t* signal_buffer,
    uint32_t* signal_length
);

// Decode radio signal to phoneme message
bool swarm_signal_decode(
    swarm_signal_adapter_t* adapter,
    const uint8_t* signal_buffer,
    uint32_t signal_length,
    swarm_phoneme_message_t* message
);

// Radio types
typedef enum {
    SWARM_RADIO_LORA,      // Long range, low power, 256 byte packets
    SWARM_RADIO_WIFI,      // High bandwidth, limited range
    SWARM_RADIO_BLUETOOTH, // Short range, mesh capable
    SWARM_RADIO_ULTRASONIC,// Acoustic, underwater capable
    SWARM_RADIO_OPTICAL,   // Line-of-sight, secure
    SWARM_RADIO_CUSTOM     // User-defined transport
} swarm_radio_type_t;
```

**LoRa Encoding** (most constrained):
```
┌─────────────┬──────────────┬─────────────┬─────────────┬─────────┐
│ Preamble    │ Sync Word    │ Header      │ Payload     │ CRC     │
│ (8 symbols) │ (2 bytes)    │ (4 bytes)   │ (24 bytes)  │ (2 bytes)│
└─────────────┴──────────────┴─────────────┴─────────────┴─────────┘
                              │
                              ▼
                  ┌────────────────────────┐
                  │ swarm_phoneme_message_t │
                  │ (24 bytes exactly)      │
                  └────────────────────────┘
```

**Consequences**:
- Same phoneme protocol works across radio types
- Can switch radios without changing brain logic
- Supports hybrid networks (LoRa for range + WiFi for bandwidth)

**Integration Point**: New `nimcp_swarm_signal.h`

---

## Swarm Brain API

### Core API

```c
//=============================================================================
// nimcp_swarm_brain.h - Swarm Brain Coordinator
//=============================================================================

/**
 * @brief Create swarm brain coordinator for constrained drone
 *
 * @param config Swarm configuration
 * @param local_brain Local drone brain (constrained)
 * @param signal_adapter Radio signal adapter
 * @return Swarm brain coordinator or NULL on error
 */
swarm_brain_t swarm_brain_create(
    const swarm_brain_config_t* config,
    brain_t local_brain,
    swarm_signal_adapter_t* signal_adapter
);

/**
 * @brief Join existing swarm
 *
 * @param swarm Swarm brain coordinator
 * @param swarm_id Swarm identifier to join
 * @return true on success
 */
bool swarm_brain_join(swarm_brain_t swarm, const char* swarm_id);

/**
 * @brief Leave swarm gracefully
 *
 * @param swarm Swarm brain coordinator
 * @return true on success
 */
bool swarm_brain_leave(swarm_brain_t swarm);

/**
 * @brief Process swarm communication (call in main loop)
 *
 * Receives radio messages, updates collective workspace,
 * processes votes, broadcasts local state.
 *
 * @param swarm Swarm brain coordinator
 * @return Number of messages processed
 */
uint32_t swarm_brain_process(swarm_brain_t swarm);

/**
 * @brief Broadcast perception to swarm
 *
 * @param swarm Swarm brain coordinator
 * @param perception Perception data
 * @param salience Perception salience [0,1]
 * @return true on success
 */
bool swarm_brain_broadcast_perception(
    swarm_brain_t swarm,
    const float* perception,
    float salience
);

/**
 * @brief Propose action for consensus vote
 *
 * @param swarm Swarm brain coordinator
 * @param topic Vote topic
 * @param proposal Proposed action
 * @param callback Called when vote completes
 * @return Proposal ID or 0 on error
 */
uint32_t swarm_brain_propose_action(
    swarm_brain_t swarm,
    swarm_vote_topic_t topic,
    const float* proposal,
    swarm_vote_callback_t callback
);

/**
 * @brief Get current emergence tier
 *
 * @param swarm Swarm brain coordinator
 * @return Current emergence tier based on connectivity and coherence
 */
swarm_emergence_tier_t swarm_brain_get_emergence_tier(swarm_brain_t swarm);

/**
 * @brief Get collective workspace
 *
 * @param swarm Swarm brain coordinator
 * @return Pointer to collective workspace (read-only)
 */
const collective_workspace_t* swarm_brain_get_workspace(swarm_brain_t swarm);

/**
 * @brief Sync neuromodulators with swarm
 *
 * Broadcasts local neuromodulator state, receives swarm state,
 * computes weighted average based on emergence tier.
 *
 * @param swarm Swarm brain coordinator
 * @return true on success
 */
bool swarm_brain_sync_neuromodulators(swarm_brain_t swarm);
```

### Configuration

```c
typedef struct {
    // Identity
    uint16_t drone_id;           // Unique drone ID (1-65535)
    char swarm_name[32];         // Swarm identifier

    // Communication
    uint32_t heartbeat_ms;       // Heartbeat interval (default: 100ms)
    uint32_t sync_ms;            // Workspace sync interval (default: 50ms)
    uint32_t vote_timeout_ms;    // Vote timeout (default: 100ms)

    // Emergence
    float coherence_threshold;   // Min coherence for tier upgrade (default: 0.5)
    uint32_t critical_mass;      // Drones for full emergence (default: 8)

    // Workspace
    uint32_t workspace_size;     // Max workspace items (default: 32)
    float broadcast_threshold;   // Min salience to broadcast (default: 0.7)

    // Neuromodulator sync
    float neuromod_diffusion;    // Cross-swarm diffusion rate (default: 0.3)
    bool enable_reward_sharing;  // Share reward signals (default: true)
} swarm_brain_config_t;
```

---

## Integration with Existing NIMCP

### Existing Modules Used

| Module | Usage in Swarm Brain |
|--------|---------------------|
| `nimcp_speech_cortex.h` | Phoneme encoding/decoding |
| `nimcp_distributed_cognition.h` | Neuromodulator sync foundation |
| `nimcp_p2pnode.h` | Peer management, topology |
| `nimcp_replication.h` | CRDT conflict resolution |
| `nimcp_global_workspace.h` | Local workspace (extended to collective) |
| `nimcp_bio_async.h` | Event-driven message handling |

### New Modules Required

```
include/swarm/
├── nimcp_swarm_brain.h          # Core swarm coordinator
├── nimcp_swarm_protocol.h       # Phoneme-based protocol
├── nimcp_swarm_signal.h         # Radio signal adapter
├── nimcp_collective_workspace.h # Distributed workspace
├── nimcp_swarm_consensus.h      # Voting mechanism
└── nimcp_swarm_emergence.h      # Emergence tier logic

src/swarm/
├── nimcp_swarm_brain.c
├── nimcp_swarm_protocol.c
├── nimcp_swarm_signal.c
├── nimcp_collective_workspace.c
├── nimcp_swarm_consensus.c
└── nimcp_swarm_emergence.c
```

---

## Memory Budget (Per Drone)

| Component | Size | Notes |
|-----------|------|-------|
| Neural Core (200 neurons) | 8MB | Sparse synapse allocation |
| Visual Cortex (drone config) | 130KB | 160×120, 4 filters |
| Audio Cortex (drone config) | 13KB | 8kHz, 13 mel |
| Speech Cortex (minimal) | 50KB | Encode/decode only |
| Swarm Coordinator | 20KB | Protocol state |
| Collective Workspace | 4KB | 32 items × 128 bytes |
| Signal Adapter | 2KB | Radio buffers |
| **Total** | **~8.2MB** | Fits in 512MB drone |

---

## Latency Analysis

| Operation | Latency | Notes |
|-----------|---------|-------|
| Phoneme encode | 0.1ms | Speech cortex |
| Radio transmit | 5-50ms | Depends on radio type |
| Phoneme decode | 0.1ms | Speech cortex |
| Workspace merge | 0.5ms | CRDT merge |
| Consensus vote | 100ms | Timeout-bounded |
| Emergence check | 0.1ms | Simple threshold |
| **Total round-trip** | **~110ms** | Drone-to-drone |

---

## Example: Threat Response

```c
// Drone #3 detects threat via visual cortex
float threat_features[32];
visual_cortex_process(visual, camera_frame, threat_features);
float threat_salience = salience_evaluate(sal, threat_features);

if (threat_salience > 0.8) {
    // Broadcast to swarm using speech cortex phonemes
    // Encodes as /deɪnʤər/ (danger) + location payload
    swarm_brain_broadcast_perception(swarm, threat_features, threat_salience);

    // Propose retreat vote
    float retreat_direction[4] = {-1.0, 0.0, 0.0, 0.0}; // Retreat vector
    swarm_brain_propose_action(swarm, VOTE_TOPIC_RETREAT, retreat_direction, on_vote_complete);
}

// All drones receive broadcast, update collective workspace
// Vote propagates, consensus reached in ~100ms
// Swarm executes coordinated retreat
```

---

## Emergent Behaviors

When swarm reaches critical mass (8+ coherent drones):

1. **Collective Attention**: Swarm focuses on same high-salience item
2. **Distributed Memory**: Working memory spans multiple drones
3. **Emergent Planning**: Multi-step plans emerge from consensus
4. **Self-Model**: Swarm develops model of itself (where are we? how many?)
5. **Fault Tolerance**: Loss of individual drones doesn't crash swarm cognition

---

## Implementation Phases

### Phase 1: Foundation (Week 1-2)
- [ ] Implement `nimcp_swarm_protocol.h/c` (phoneme message format)
- [ ] Implement `nimcp_swarm_signal.h/c` (LoRa adapter)
- [ ] Unit tests for encoding/decoding

### Phase 2: Workspace (Week 3)
- [ ] Implement `nimcp_collective_workspace.h/c`
- [ ] CRDT merge logic
- [ ] Integration with local global workspace

### Phase 3: Emergence (Week 4)
- [ ] Implement `nimcp_swarm_emergence.h/c`
- [ ] Tier calculation and capability unlocks
- [ ] Integration tests with simulated swarm

### Phase 4: Consensus (Week 5)
- [ ] Implement `nimcp_swarm_consensus.h/c`
- [ ] Voting protocol and conflict resolution
- [ ] Byzantine fault tolerance testing

### Phase 5: Integration (Week 6)
- [ ] Implement `nimcp_swarm_brain.h/c` (coordinator)
- [ ] Integration with existing distributed cognition
- [ ] End-to-end swarm tests

### Phase 6: Validation (Week 7-8)
- [ ] Test on actual drone hardware
- [ ] Benchmark latency and bandwidth
- [ ] Validate emergent behaviors

---

## References

- Bonabeau, E. et al. (1999) "Swarm Intelligence: From Natural to Artificial Systems"
- Baars, B.J. (1988) "A Cognitive Theory of Consciousness" (Global Workspace)
- Castro, M. & Liskov, B. (1999) "Practical Byzantine Fault Tolerance"
- Shapiro, M. et al. (2011) "Conflict-free Replicated Data Types"
