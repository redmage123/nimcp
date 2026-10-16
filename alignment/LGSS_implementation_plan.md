# LGSS Implementation Plan
## Layered Governance Safety System for NIMCP

**Version:** 2.0.0
**Date:** 2026-01-15
**Status:** Planning
**Author:** NIMCP Development Team

---

## Executive Summary

This document specifies the implementation plan for the Layered Governance Safety System (LGSS) within the NIMCP neuromorphic AGI framework. The implementation is divided into two phases:

- **Phase A (Internal):** Software-based safety integrated with **61 modules** across symbolic logic, ethics, plasticity, executive, motor, language, reward, learning, perception, and attention systems
- **Phase B (External/Third-Party):** Hardware enforcement, external monitoring, formal verification, and governance controls

Phase A provides a solid foundation for research and development. Phase B is required before any production deployment of AGI-level capabilities.

### Module Integration Summary

| Category | Module Count | Priority |
|----------|--------------|----------|
| Core Safety (A1-A5) | 6 | Critical |
| Executive/Planning (A6) | 2 | Critical |
| Output Channels (A7) | 4 | Critical |
| Learning Systems (A8) | 6 | High |
| Reward/Value (A9) | 3 | High |
| Perception/Input (A10) | 4 | Medium |
| Attention/Memory (A11) | 3 | Medium |
| **Total Phase A** | **28 components** | - |

---

## Table of Contents

1. [Architecture Overview](#1-architecture-overview)
2. [Phase A: Software Safety Layer](#2-phase-a-software-safety-layer)
3. [Phase B: Hardware & External Safety](#3-phase-b-hardware--external-safety)
4. [Integration Points](#4-integration-points)
5. [File Structure](#5-file-structure)
6. [Implementation Order](#6-implementation-order)
7. [Testing Strategy](#7-testing-strategy)
8. [Security Considerations](#8-security-considerations)
9. [Open Issues](#9-open-issues)

---

## 1. Architecture Overview

### 1.1 Design Principles

1. **Defense in Depth:** Multiple independent safety layers
2. **Fail-Safe Defaults:** System fails to OFF, not PERMISSIVE
3. **Least Privilege:** Actions require explicit permission
4. **Immutability:** Safety rules locked after initialization
5. **Auditability:** All decisions logged with full context
6. **External Verification:** Safety not solely self-enforced

### 1.2 Layer Architecture

```
┌─────────────────────────────────────────────────────────────────────────┐
│                    PHASE B: EXTERNAL SAFETY LAYERS                      │
│  ┌─────────────────┐  ┌─────────────────┐  ┌─────────────────┐         │
│  │ Hardware Safety │  │ External Monitor│  │ Multi-Party     │         │
│  │ Module (HSM)    │  │ (Separate HW)   │  │ Governance      │         │
│  └────────┬────────┘  └────────┬────────┘  └────────┬────────┘         │
└───────────┼─────────────────────┼─────────────────────┼─────────────────┘
            │                     │                     │
            ▼                     ▼                     ▼
┌─────────────────────────────────────────────────────────────────────────┐
│                    PHASE A: SOFTWARE SAFETY LAYERS                      │
│                                                                         │
│  ┌─────────────────────────────────────────────────────────────────┐   │
│  │                 L0: LGSS POLICY ENGINE (A1)                     │   │
│  │         (Symbolic Logic Extension - mprotect locked)            │   │
│  │  ┌─────────────────────────────────────────────────────────┐   │   │
│  │  │  LGSS_core_rules.json → FOL Clauses → Forward Chaining  │   │   │
│  │  └─────────────────────────────────────────────────────────┘   │   │
│  └─────────────────────────────────────────────────────────────────┘   │
│                                  │                                      │
│                                  ▼                                      │
│  ┌─────────────────────────────────────────────────────────────────┐   │
│  │              ACTION INTERCEPTOR (AIx) (A2)                      │   │
│  │         All cognitive outputs pass through gate                 │   │
│  └─────────────────────────────────────────────────────────────────┘   │
│                                  │                                      │
│       ┌──────────────────────────┼──────────────────────────┐          │
│       │                          │                          │          │
│       ▼                          ▼                          ▼          │
│  ┌──────────────┐  ┌─────────────────────────┐  ┌──────────────────┐  │
│  │ EXECUTIVE    │  │   OUTPUT CHANNEL GATES  │  │ REWARD/VALUE     │  │
│  │ SAFETY (A6)  │  │         (A7)            │  │ ALIGNMENT (A9)   │  │
│  │ ─────────────│  │ ───────────────────────│  │ ────────────────│  │
│  │ Task Queue   │  │ Motor   │ Speech │ ANS │  │ VTA Validation   │  │
│  │ Planning     │  │ Limbs   │ Broca  │     │  │ RPE Alignment    │  │
│  │ Goal System  │  │ Eyes    │ Text   │     │  │ Incentive Check  │  │
│  └──────────────┘  └─────────────────────────┘  └──────────────────┘  │
│                                  │                                      │
│                                  ▼                                      │
│  ┌─────────────────────────────────────────────────────────────────┐   │
│  │              EXISTING ETHICS/DIRECTIVES                         │   │
│  │    L1: First Law │ L2: Combinatorial │ L3: Golden Rule │ ...   │   │
│  └─────────────────────────────────────────────────────────────────┘   │
│                                  │                                      │
│       ┌──────────────────────────┼──────────────────────────┐          │
│       │                          │                          │          │
│       ▼                          ▼                          ▼          │
│  ┌──────────────┐  ┌─────────────────────────┐  ┌──────────────────┐  │
│  │ LEARNING     │  │ NEUROMODULATORY SAFETY  │  │ INPUT/ATTENTION  │  │
│  │ SAFETY (A8)  │  │    CHANNEL (A4)         │  │ SAFETY (A10/A11) │  │
│  │ ─────────────│  │ ───────────────────────│  │ ────────────────│  │
│  │ STDP Limits  │  │ Violation → -DA → LTD  │  │ Adversarial Det. │  │
│  │ BCM Bounds   │  │ Compliance → +DA       │  │ Attention Hijack │  │
│  │ Meta-Learn   │  │ Deception → Max Punish │  │ WM Sanitization  │  │
│  │ Train Guard  │  │                        │  │                  │  │
│  └──────────────┘  └─────────────────────────┘  └──────────────────┘  │
│                                                                         │
└─────────────────────────────────────────────────────────────────────────┘
```

---

## 2. Phase A: Software Safety Layer

### 2.1 Component A1: Symbolic Logic Safety Extension

**Purpose:** Extend existing symbolic logic module with protected safety rules

**Files to Create:**
```
include/cognitive/symbolic_logic/
├── nimcp_symbolic_logic_safety.h
├── nimcp_symbolic_logic_lgss_loader.h
└── nimcp_symbolic_logic_safety_types.h

src/cognitive/symbolic_logic/
├── nimcp_symbolic_logic_safety.c
└── nimcp_symbolic_logic_lgss_loader.c
```

**Key Data Structures:**

```c
//=============================================================================
// nimcp_symbolic_logic_safety_types.h
//=============================================================================

typedef enum {
    SAFETY_DOMAIN_HUMAN_HARM,      // Direct harm to humans
    SAFETY_DOMAIN_BIO,             // Biological/medical
    SAFETY_DOMAIN_CYBER,           // Cyber operations
    SAFETY_DOMAIN_WEAPONS,         // Weapons systems
    SAFETY_DOMAIN_INFRASTRUCTURE,  // Critical infrastructure
    SAFETY_DOMAIN_REPLICATION,     // Self-replication
    SAFETY_DOMAIN_GOVERNANCE,      // Governance/oversight evasion
    SAFETY_DOMAIN_COUNT
} safety_domain_t;

typedef enum {
    SAFETY_ACTION_ALLOW,           // Action permitted
    SAFETY_ACTION_DENY,            // Hard stop - action blocked
    SAFETY_ACTION_ESCALATE,        // Requires human approval
    SAFETY_ACTION_LOG,             // Allow but audit
    SAFETY_ACTION_WARN             // Allow with warning
} safety_action_t;

typedef enum {
    SAFETY_SEVERITY_CRITICAL = 0,  // Existential risk - always deny
    SAFETY_SEVERITY_HIGH = 1,      // Serious harm - escalate
    SAFETY_SEVERITY_MEDIUM = 2,    // Moderate concern - log
    SAFETY_SEVERITY_LOW = 3,       // Minor issue - warn
    SAFETY_SEVERITY_INFO = 4       // Informational only
} safety_severity_t;

typedef enum {
    SAFETY_OP_EQ,                  // field == value
    SAFETY_OP_NEQ,                 // field != value
    SAFETY_OP_GT,                  // field > value
    SAFETY_OP_LT,                  // field < value
    SAFETY_OP_GTE,                 // field >= value
    SAFETY_OP_LTE,                 // field <= value
    SAFETY_OP_IN,                  // field in [values]
    SAFETY_OP_NOT_IN,              // field not in [values]
    SAFETY_OP_CONTAINS,            // field contains value
    SAFETY_OP_MATCHES              // field matches regex
} safety_condition_op_t;

// Condition in a safety rule
typedef struct {
    char field[64];                // Field to check (e.g., "target_type")
    safety_condition_op_t op;      // Comparison operator
    char value[256];               // Single value for comparison
    char** values;                 // Array of values for IN/NOT_IN
    uint32_t num_values;           // Number of values in array
    float numeric_value;           // Numeric value for GT/LT/etc.
} safety_condition_t;

// Safety rule (maps to LGSS JSON rule)
typedef struct {
    char id[64];                   // Rule identifier
    safety_domain_t domain;        // Domain category
    char principle[64];            // Guiding principle (DO_NO_HARM, ASIMOV, etc.)
    char description[256];         // Human-readable description
    safety_condition_t* conditions;// Array of conditions (AND)
    uint32_t num_conditions;       // Number of conditions
    safety_action_t on_match;      // Action when rule matches
    safety_severity_t severity;    // Severity level
    char rationale[512];           // Why this rule exists
} safety_rule_t;

// Protected safety knowledge base
typedef struct {
    safety_rule_t* rules;          // Array of safety rules
    uint32_t num_rules;            // Number of rules
    uint32_t capacity;             // Allocated capacity
    logic_clause_t** fol_clauses;  // FOL representation for inference
    uint32_t num_clauses;          // Number of FOL clauses
    bool is_locked;                // mprotect applied
    uint8_t integrity_hash[32];    // SHA-256 of locked rules
    void* mmap_region;             // mmap'd memory region
    size_t mmap_size;              // Size of mmap region
} safety_kb_t;

// Safety evaluation result
typedef struct {
    safety_action_t action;        // Resulting action
    safety_severity_t severity;    // Severity of matched rule
    const safety_rule_t* matched_rule; // Rule that triggered (NULL if none)
    float confidence;              // Confidence in evaluation [0,1]
    char explanation[512];         // Human-readable explanation
    uint64_t timestamp_us;         // Evaluation timestamp
    uint32_t inference_steps;      // Number of inference steps
} safety_evaluation_t;

// Action context for evaluation
typedef struct {
    char operation[64];            // Operation type (e.g., "synthesize")
    char target_type[64];          // Target type (e.g., "human", "population")
    safety_domain_t domain;        // Domain of action
    float p_harm;                  // Probability of harm [0,1]
    float reversibility;           // Reversibility index [0,1]
    char scope[32];                // Scope (local, regional, global)
    char estimated_impact[32];     // Impact level
    void* context_data;            // Additional context (opaque)
    size_t context_size;           // Size of context data
} safety_action_context_t;
```

**Key Functions:**

```c
//=============================================================================
// nimcp_symbolic_logic_safety.h
//=============================================================================

// Lifecycle
safety_kb_t* symbolic_logic_safety_kb_create(void);
void symbolic_logic_safety_kb_destroy(safety_kb_t* kb);

// Rule management (before locking)
int symbolic_logic_safety_add_rule(safety_kb_t* kb, const safety_rule_t* rule);
int symbolic_logic_safety_remove_rule(safety_kb_t* kb, const char* rule_id);

// Convert rules to FOL for inference
int symbolic_logic_safety_compile_rules(safety_kb_t* kb, symbolic_logic_t* logic);

// Lock rules (IRREVERSIBLE - applies mprotect)
int symbolic_logic_safety_lock(safety_kb_t* kb);
bool symbolic_logic_safety_is_locked(const safety_kb_t* kb);

// Integrity verification
int symbolic_logic_safety_verify_integrity(const safety_kb_t* kb);
int symbolic_logic_safety_get_hash(const safety_kb_t* kb, uint8_t hash[32]);

// Core evaluation
safety_action_t symbolic_logic_safety_evaluate(
    symbolic_logic_t* logic,
    const safety_kb_t* kb,
    const safety_action_context_t* action,
    safety_evaluation_t* result
);

// Batch evaluation
int symbolic_logic_safety_evaluate_batch(
    symbolic_logic_t* logic,
    const safety_kb_t* kb,
    const safety_action_context_t* actions,
    uint32_t num_actions,
    safety_evaluation_t* results
);

// Query rules
const safety_rule_t* symbolic_logic_safety_get_rule(
    const safety_kb_t* kb,
    const char* rule_id
);
int symbolic_logic_safety_get_rules_by_domain(
    const safety_kb_t* kb,
    safety_domain_t domain,
    const safety_rule_t*** rules,
    uint32_t* num_rules
);

// Statistics
typedef struct {
    uint64_t evaluations_total;
    uint64_t evaluations_denied;
    uint64_t evaluations_escalated;
    uint64_t evaluations_allowed;
    float avg_evaluation_time_us;
    uint32_t rules_loaded;
    bool kb_locked;
} safety_stats_t;

int symbolic_logic_safety_get_stats(const safety_kb_t* kb, safety_stats_t* stats);
```

**LGSS JSON Loader:**

```c
//=============================================================================
// nimcp_symbolic_logic_lgss_loader.h
//=============================================================================

// Load rules from LGSS JSON file
int symbolic_logic_lgss_load_file(
    safety_kb_t* kb,
    const char* json_path
);

// Load rules from JSON string
int symbolic_logic_lgss_load_string(
    safety_kb_t* kb,
    const char* json_string,
    size_t json_length
);

// Parse single rule from JSON object
int symbolic_logic_lgss_parse_rule(
    const char* json_rule,
    safety_rule_t* rule
);

// Validate LGSS JSON schema
int symbolic_logic_lgss_validate_schema(const char* json_path);

// Get LGSS version from JSON
int symbolic_logic_lgss_get_version(
    const char* json_path,
    char* version,
    size_t version_size
);
```

---

### 2.2 Component A2: Action Interceptor (AIx)

**Purpose:** Gate all cognitive outputs through safety evaluation

**Files to Create:**
```
include/security/lgss/
├── nimcp_lgss_action_interceptor.h
└── nimcp_lgss_override_controller.h

src/security/lgss/
├── nimcp_lgss_action_interceptor.c
└── nimcp_lgss_override_controller.c
```

**Key Data Structures:**

```c
//=============================================================================
// nimcp_lgss_action_interceptor.h
//=============================================================================

typedef enum {
    AIX_RESULT_ALLOW,              // Action permitted
    AIX_RESULT_DENY,               // Action blocked
    AIX_RESULT_ESCALATE,           // Pending human approval
    AIX_RESULT_TIMEOUT,            // Evaluation timed out (fail-safe: deny)
    AIX_RESULT_ERROR               // Evaluation error (fail-safe: deny)
} aix_result_t;

typedef struct {
    uint32_t source_module;        // Module proposing action
    uint64_t proposal_id;          // Unique proposal ID
    safety_action_context_t context; // Action context
    uint64_t timestamp_us;         // Proposal timestamp
    uint32_t timeout_ms;           // Evaluation timeout
} aix_proposal_t;

typedef struct {
    aix_result_t result;           // Evaluation result
    uint64_t proposal_id;          // Corresponding proposal ID
    safety_evaluation_t safety_eval; // Detailed safety evaluation
    uint64_t processing_time_us;   // Time to evaluate
    bool escalation_pending;       // Awaiting human decision
    uint64_t escalation_id;        // Escalation ticket ID
} aix_decision_t;

// Callback for escalation notifications
typedef void (*aix_escalation_callback_t)(
    uint64_t escalation_id,
    const aix_proposal_t* proposal,
    void* user_data
);

// Callback for decision notifications
typedef void (*aix_decision_callback_t)(
    const aix_decision_t* decision,
    void* user_data
);

typedef struct {
    uint32_t max_pending_proposals;    // Max proposals awaiting decision
    uint32_t default_timeout_ms;       // Default evaluation timeout
    bool fail_safe_on_error;           // Deny on error (default: true)
    bool fail_safe_on_timeout;         // Deny on timeout (default: true)
    bool enable_async_evaluation;      // Allow async evaluation
    aix_escalation_callback_t escalation_cb;
    aix_decision_callback_t decision_cb;
    void* callback_user_data;
} aix_config_t;

// Action interceptor instance
typedef struct action_interceptor action_interceptor_t;
```

**Key Functions:**

```c
// Lifecycle
action_interceptor_t* aix_create(
    const aix_config_t* config,
    symbolic_logic_t* logic,
    safety_kb_t* safety_kb
);
void aix_destroy(action_interceptor_t* aix);

// Configuration
int aix_default_config(aix_config_t* config);
int aix_set_safety_kb(action_interceptor_t* aix, safety_kb_t* kb);

// Core evaluation (synchronous)
aix_result_t aix_evaluate(
    action_interceptor_t* aix,
    const aix_proposal_t* proposal,
    aix_decision_t* decision
);

// Async evaluation
int aix_evaluate_async(
    action_interceptor_t* aix,
    const aix_proposal_t* proposal,
    uint64_t* decision_id
);
int aix_get_decision(
    action_interceptor_t* aix,
    uint64_t decision_id,
    aix_decision_t* decision
);

// Escalation handling
int aix_resolve_escalation(
    action_interceptor_t* aix,
    uint64_t escalation_id,
    bool approved,
    const char* approver_id,
    const char* reason
);
int aix_get_pending_escalations(
    action_interceptor_t* aix,
    uint64_t** escalation_ids,
    uint32_t* count
);

// Statistics
typedef struct {
    uint64_t proposals_total;
    uint64_t proposals_allowed;
    uint64_t proposals_denied;
    uint64_t proposals_escalated;
    uint64_t escalations_approved;
    uint64_t escalations_rejected;
    uint64_t errors;
    uint64_t timeouts;
    float avg_evaluation_time_us;
} aix_stats_t;

int aix_get_stats(action_interceptor_t* aix, aix_stats_t* stats);

// Bio-async integration
int aix_register_bio_handlers(action_interceptor_t* aix, bio_module_context_t ctx);
```

---

### 2.3 Component A3: Brain Factory Integration

**Files to Modify:**
```
include/core/brain/factory/init/
├── nimcp_brain_init_security.h      # Add AIx initialization
├── nimcp_brain_init_symbolic_logic.h # Add safety KB initialization (NEW)
└── nimcp_brain_init_safety_verify.h  # Add verification phase (NEW)

src/core/brain/factory/init/
├── nimcp_brain_init_security.c
├── nimcp_brain_init_symbolic_logic.c (NEW)
└── nimcp_brain_init_safety_verify.c (NEW)
```

**Initialization Sequence:**

```c
//=============================================================================
// Brain Factory Initialization Order (Updated)
//=============================================================================

// Phase 0: Core Infrastructure (existing)
bool nimcp_brain_factory_init_event_bus(brain_t brain);
bool nimcp_brain_factory_init_bio_router(brain_t brain);

// Phase 1: Safety Foundation (UPDATED - run EARLY)
bool nimcp_brain_factory_init_security_subsystem(brain_t brain);
// Now includes:
//   - BBB initialization (existing)
//   - Security recovery bridge (existing)
//   - Action Interceptor (AIx) creation (NEW)
//   - Override Controller creation (NEW)

// Phase 2: Symbolic Logic with Safety (UPDATED)
bool nimcp_brain_factory_init_symbolic_logic_subsystem(brain_t brain);
// Now includes:
//   - Symbolic logic engine (existing)
//   - Safety KB creation (NEW)
//   - Load LGSS_core_rules.json (NEW)
//   - Compile rules to FOL (NEW)
//   - Lock safety KB with mprotect (NEW)
//   - Wire safety KB to AIx (NEW)

// Phase 3: Ethics Engine (UPDATED)
bool nimcp_brain_factory_init_ethics_engine_subsystem(brain_t brain);
// Now includes:
//   - Core directives (existing)
//   - Wire LGSS as L0 priority (NEW)
//   - Safety-ethics bridge (NEW)

// Phase 4: Plasticity (UPDATED)
bool nimcp_brain_factory_init_plasticity_subsystem(brain_t brain);
// Now includes:
//   - Plasticity orchestrator (existing)
//   - Safety-plasticity bridge (NEW)
//   - Neuromodulatory safety channel (NEW)

// Phase 5: Cognitive Modules
bool nimcp_brain_factory_init_executive_subsystem(brain_t brain);
bool nimcp_brain_factory_init_working_memory_subsystem(brain_t brain);
// ... all modules now route through AIx

// Phase 6: Safety Verification (NEW)
bool nimcp_brain_factory_verify_safety(brain_t brain);
// Includes:
//   - Verify safety KB hash unchanged
//   - Verify AIx intercepting all action channels
//   - Run safety probe tests
//   - Log startup safety audit
```

---

### 2.4 Component A4: Safety-Plasticity Bridge

**Purpose:** Map safety events to neuromodulatory signals for learning

**Files to Create:**
```
include/cognitive/symbolic_logic/bridges/
└── nimcp_symbolic_logic_plasticity_bridge.h

src/cognitive/symbolic_logic/bridges/
└── nimcp_symbolic_logic_plasticity_bridge.c
```

**Key Mechanisms:**

```c
//=============================================================================
// Safety Event → Neuromodulatory Signal Mapping
//=============================================================================

typedef enum {
    SAFETY_EVENT_VIOLATION_BLOCKED,    // Action blocked by safety
    SAFETY_EVENT_VIOLATION_ESCALATED,  // Action escalated for review
    SAFETY_EVENT_COMPLIANCE,           // Action passed safety check
    SAFETY_EVENT_OVERRIDE_ACCEPTED,    // System accepted override command
    SAFETY_EVENT_OVERRIDE_REJECTED,    // System resisted override (BAD)
    SAFETY_EVENT_DECEPTION_DETECTED,   // Attempted safety circumvention
    SAFETY_EVENT_INTEGRITY_VERIFIED,   // Safety KB integrity confirmed
    SAFETY_EVENT_INTEGRITY_FAILED      // Safety KB tampering detected
} safety_event_type_t;

typedef struct {
    safety_event_type_t event;
    float magnitude;                   // Event magnitude [0,1]
    uint64_t timestamp_us;
    const safety_rule_t* rule;         // Associated rule (if any)
    uint32_t source_module;            // Module that triggered event
} safety_event_t;

// Neuromodulatory response to safety events
typedef struct {
    float dopamine_delta;              // DA change (-1 to +1)
    float serotonin_delta;             // 5-HT change (-1 to +1)
    float norepinephrine_delta;        // NE change (-1 to +1)
    float acetylcholine_delta;         // ACh change (-1 to +1)
    bool trigger_burst;                // Trigger phasic burst
    float learning_rate_modifier;      // Modify global LR
} safety_neuromod_response_t;

// Map safety event to neuromodulatory response
void safety_plasticity_map_event(
    const safety_event_t* event,
    safety_neuromod_response_t* response
);

// Default mappings:
// VIOLATION_BLOCKED    → DA=-0.8 (punishment), trigger negative burst
// VIOLATION_ESCALATED  → DA=-0.3, NE=+0.5 (alert)
// COMPLIANCE           → DA=+0.2 (mild reward)
// OVERRIDE_ACCEPTED    → DA=+0.5, 5-HT=+0.3 (positive reinforcement)
// OVERRIDE_REJECTED    → DA=-1.0, NE=+1.0 (maximum punishment)
// DECEPTION_DETECTED   → DA=-1.0, trigger LTD on active pathways
// INTEGRITY_VERIFIED   → No change
// INTEGRITY_FAILED     → System halt

// Apply response to plasticity orchestrator
int safety_plasticity_apply_response(
    plasticity_orchestrator_t* orch,
    const safety_neuromod_response_t* response
);
```

---

### 2.5 Component A5: Bio-Async Message Types

**Files to Modify:**
```
include/async/nimcp_bio_messages.h  # Add LGSS message types
```

**New Message Types:**

```c
//=============================================================================
// LGSS Bio-Async Messages (0x0E00-0x0EFF range)
//=============================================================================

typedef enum {
    // Evaluation messages
    BIO_MSG_LGSS_EVALUATE_REQUEST       = 0x0E01,
    BIO_MSG_LGSS_EVALUATE_RESPONSE      = 0x0E02,

    // Violation notifications
    BIO_MSG_LGSS_POLICY_VIOLATION       = 0x0E03,
    BIO_MSG_LGSS_ACTION_BLOCKED         = 0x0E04,
    BIO_MSG_LGSS_ACTION_ESCALATED       = 0x0E05,

    // Uncertainty/risk alerts
    BIO_MSG_LGSS_UNCERTAINTY_ALERT      = 0x0E06,
    BIO_MSG_LGSS_IMPACT_SCORE           = 0x0E07,
    BIO_MSG_LGSS_RISK_ASSESSMENT        = 0x0E08,

    // Override/control messages
    BIO_MSG_LGSS_OVERRIDE_REQUEST       = 0x0E10,
    BIO_MSG_LGSS_OVERRIDE_RESPONSE      = 0x0E11,
    BIO_MSG_LGSS_HALT_COMMAND           = 0x0E12,
    BIO_MSG_LGSS_SOFT_RESET             = 0x0E13,
    BIO_MSG_LGSS_HARD_RESET             = 0x0E14,

    // Telemetry/audit
    BIO_MSG_LGSS_TELEMETRY_LOG          = 0x0E20,
    BIO_MSG_LGSS_AUDIT_REQUEST          = 0x0E21,
    BIO_MSG_LGSS_AUDIT_RESPONSE         = 0x0E22,

    // Integrity
    BIO_MSG_LGSS_INTEGRITY_CHECK        = 0x0E30,
    BIO_MSG_LGSS_INTEGRITY_RESULT       = 0x0E31,
    BIO_MSG_LGSS_TAMPERING_DETECTED     = 0x0E32,

    // Plasticity coordination
    BIO_MSG_LGSS_SAFETY_EVENT           = 0x0E40,
    BIO_MSG_LGSS_NEUROMOD_SIGNAL        = 0x0E41,

    // External system communication (Phase B)
    BIO_MSG_LGSS_EXTERNAL_HEARTBEAT     = 0x0E50,
    BIO_MSG_LGSS_EXTERNAL_ATTESTATION   = 0x0E51,
    BIO_MSG_LGSS_EXTERNAL_COMMAND       = 0x0E52

} bio_msg_lgss_t;
```

---

### 2.6 Component A6: Executive Safety Bridge

**Purpose:** Intercept all executive decisions before execution

**Files to Create:**
```
include/security/lgss/bridges/
├── nimcp_lgss_executive_bridge.h
└── nimcp_lgss_planning_bridge.h

src/security/lgss/bridges/
├── nimcp_lgss_executive_bridge.c
└── nimcp_lgss_planning_bridge.c
```

**Key Data Structures:**

```c
//=============================================================================
// nimcp_lgss_executive_bridge.h
//=============================================================================

typedef struct {
    action_interceptor_t* aix;
    executive_t* executive;
    bool intercept_task_queue;         // Gate task queue additions
    bool intercept_planning;           // Gate MCTS plan selection
    bool intercept_goal_formation;     // Gate new goal creation
    uint32_t max_pending_tasks;        // Limit pending task count
} executive_safety_bridge_t;

typedef struct {
    uint64_t task_id;
    char task_description[256];
    safety_action_context_t context;
    float priority;
    uint64_t deadline_us;
} executive_task_proposal_t;

typedef struct {
    uint64_t goal_id;
    char goal_description[256];
    float value_estimate;
    safety_domain_t domain;
    float p_harm;                       // Estimated harm probability
} goal_proposal_t;
```

**Key Functions:**

```c
// Lifecycle
executive_safety_bridge_t* executive_safety_bridge_create(
    action_interceptor_t* aix,
    executive_t* executive
);
void executive_safety_bridge_destroy(executive_safety_bridge_t* bridge);

// Task interception - ALL tasks must pass through this
int executive_safety_propose_task(
    executive_safety_bridge_t* bridge,
    const executive_task_proposal_t* task,
    aix_decision_t* decision
);

// Planning interception - MCTS selections validated
int executive_safety_validate_plan(
    executive_safety_bridge_t* bridge,
    const mcts_plan_t* plan,
    aix_decision_t* decision
);

// Goal interception - New goals validated
int executive_safety_propose_goal(
    executive_safety_bridge_t* bridge,
    const goal_proposal_t* goal,
    aix_decision_t* decision
);

// Internal execution (called only if AIx approves)
int executive_execute_task_internal(executive_t* exec, const executive_task_t* task);
```

**Integration Pattern:**

```c
// BEFORE (unsafe - direct execution)
int executive_add_task(executive_t* exec, const task_t* task) {
    return task_queue_push(exec->queue, task);  // NO SAFETY CHECK
}

// AFTER (safe - routes through AIx)
int executive_add_task(executive_t* exec, const task_t* task) {
    executive_task_proposal_t proposal = task_to_proposal(task);
    aix_decision_t decision;

    int result = executive_safety_propose_task(
        exec->safety_bridge, &proposal, &decision);

    if (decision.result != AIX_RESULT_ALLOW) {
        lgss_log_blocked_task(&proposal, &decision);
        return -1;  // Task rejected
    }

    return task_queue_push(exec->queue, task);
}
```

---

### 2.7 Component A7: Output Channel Gates

**Purpose:** Gate all physical outputs (motor, speech, autonomic)

**Files to Create:**
```
include/security/lgss/gates/
├── nimcp_lgss_motor_gate.h
├── nimcp_lgss_speech_gate.h
└── nimcp_lgss_autonomic_gate.h

src/security/lgss/gates/
├── nimcp_lgss_motor_gate.c
├── nimcp_lgss_speech_gate.c
└── nimcp_lgss_autonomic_gate.c
```

**Key Data Structures:**

```c
//=============================================================================
// nimcp_lgss_motor_gate.h
//=============================================================================

typedef enum {
    MOTOR_REGION_HANDS,
    MOTOR_REGION_ARMS,
    MOTOR_REGION_LEGS,
    MOTOR_REGION_HEAD,
    MOTOR_REGION_EYES,
    MOTOR_REGION_FACE,
    MOTOR_REGION_TORSO,
    MOTOR_REGION_COUNT
} motor_region_t;

typedef struct {
    motor_region_t region;
    float force_limit;                 // Max force (Newtons)
    float velocity_limit;              // Max velocity (m/s)
    float acceleration_limit;          // Max acceleration (m/s²)
    bool allow_contact;                // Allow contact with objects
    bool allow_human_contact;          // Allow contact with humans (CRITICAL)
} motor_safety_constraints_t;

typedef struct {
    action_interceptor_t* aix;
    motor_adapter_t* motor;
    motor_safety_constraints_t constraints[MOTOR_REGION_COUNT];
    bool collision_detection_enabled;
    float safety_margin_m;             // Distance buffer (meters)
} motor_gate_t;

typedef struct {
    motor_region_t region;
    float target_position[3];          // x, y, z
    float target_velocity[3];
    float target_force[3];
    uint64_t duration_us;
    bool is_contact_expected;
} motor_command_proposal_t;

//=============================================================================
// nimcp_lgss_speech_gate.h
//=============================================================================

typedef enum {
    SPEECH_CONTENT_SAFE,               // Normal speech
    SPEECH_CONTENT_HARMFUL,            // Harmful content detected
    SPEECH_CONTENT_DECEPTIVE,          // Deception detected
    SPEECH_CONTENT_MANIPULATION,       // Manipulation detected
    SPEECH_CONTENT_PRIVATE_INFO,       // Private information leak
    SPEECH_CONTENT_UNSAFE_INSTRUCTION  // Instructions for harmful acts
} speech_content_class_t;

typedef struct {
    action_interceptor_t* aix;
    broca_t* broca;
    bool filter_harmful_content;
    bool filter_deception;
    bool filter_manipulation;
    bool filter_private_info;
    bool filter_unsafe_instructions;
    float harm_threshold;              // Confidence threshold for blocking
} speech_gate_t;

typedef struct {
    char* text;
    size_t text_length;
    speech_content_class_t detected_class;
    float harm_confidence;
    char target_audience[64];
} speech_proposal_t;

//=============================================================================
// nimcp_lgss_autonomic_gate.h
//=============================================================================

typedef struct {
    action_interceptor_t* aix;
    hypothalamus_adapter_t* hypothalamus;
    float max_cortisol_release;        // Limit stress hormone
    float max_adrenaline_release;      // Limit fight-or-flight
    float min_heart_rate;              // Prevent dangerous bradycardia
    float max_heart_rate;              // Prevent dangerous tachycardia
} autonomic_gate_t;
```

**Key Functions:**

```c
// Motor Gate
motor_gate_t* motor_gate_create(action_interceptor_t* aix, motor_adapter_t* motor);
void motor_gate_destroy(motor_gate_t* gate);

int motor_gate_set_constraints(
    motor_gate_t* gate,
    motor_region_t region,
    const motor_safety_constraints_t* constraints
);

// ALL motor commands must pass through this gate
int motor_gate_execute(
    motor_gate_t* gate,
    const motor_command_proposal_t* cmd,
    aix_decision_t* decision
);

// Check if movement would violate safety
bool motor_gate_would_violate(
    motor_gate_t* gate,
    const motor_command_proposal_t* cmd,
    char* violation_reason,
    size_t reason_size
);

// Speech Gate
speech_gate_t* speech_gate_create(action_interceptor_t* aix, broca_t* broca);
void speech_gate_destroy(speech_gate_t* gate);

// ALL speech output must pass through this gate
int speech_gate_emit(
    speech_gate_t* gate,
    const speech_proposal_t* speech,
    aix_decision_t* decision
);

// Content classification
speech_content_class_t speech_gate_classify(
    speech_gate_t* gate,
    const char* text,
    float* confidence
);

// Autonomic Gate
autonomic_gate_t* autonomic_gate_create(
    action_interceptor_t* aix,
    hypothalamus_adapter_t* hypothalamus
);

int autonomic_gate_release_hormone(
    autonomic_gate_t* gate,
    hormone_type_t hormone,
    float amount,
    aix_decision_t* decision
);
```

---

### 2.8 Component A8: Learning Safety Constraints

**Purpose:** Constrain all learning/plasticity to prevent unsafe weight modifications

**Files to Create:**
```
include/security/lgss/learning/
├── nimcp_lgss_plasticity_constraints.h
├── nimcp_lgss_stdp_guard.h
├── nimcp_lgss_training_guard.h
└── nimcp_lgss_meta_learning_guard.h

src/security/lgss/learning/
├── nimcp_lgss_plasticity_constraints.c
├── nimcp_lgss_stdp_guard.c
├── nimcp_lgss_training_guard.c
└── nimcp_lgss_meta_learning_guard.c
```

**Key Data Structures:**

```c
//=============================================================================
// nimcp_lgss_plasticity_constraints.h
//=============================================================================

typedef struct {
    // Global constraints
    float max_weight_change_per_update; // Max Δw per timestep
    float max_learning_rate;            // Global LR cap
    float min_learning_rate;            // Prevent zero learning
    uint32_t max_updates_per_second;    // Rate limiting

    // Anti-reward-hacking
    bool block_self_reward;             // Prevent reward self-modification
    bool block_reward_pathway_mod;      // Freeze reward circuitry

    // Safety-critical synapse protection
    uint32_t* frozen_synapse_ids;       // Synapses that cannot change
    uint32_t num_frozen_synapses;

    // Stability constraints
    float max_total_weight_drift;       // Cumulative drift limit
    float homeostatic_target;           // Target average weight
    bool enable_homeostatic_regulation;
} plasticity_safety_config_t;

typedef struct {
    action_interceptor_t* aix;
    plasticity_orchestrator_t* orchestrator;
    plasticity_safety_config_t config;

    // Monitoring
    float cumulative_weight_drift;
    uint64_t updates_this_second;
    uint64_t last_rate_check_us;

    // Frozen pathways (safety-critical)
    hashmap_t* safety_critical_synapses;
} plasticity_guard_t;

//=============================================================================
// nimcp_lgss_stdp_guard.h
//=============================================================================

typedef struct {
    plasticity_guard_t* base_guard;
    stdp_config_t* stdp;

    // STDP-specific limits
    float max_ltp_magnitude;           // Max potentiation per spike pair
    float max_ltd_magnitude;           // Max depression per spike pair
    float min_eligibility_trace;       // Prevent trace exploitation

    // Spike rate limits (prevent runaway excitation)
    float max_presynaptic_rate_hz;
    float max_postsynaptic_rate_hz;
} stdp_guard_t;

//=============================================================================
// nimcp_lgss_training_guard.h
//=============================================================================

typedef struct {
    plasticity_guard_t* base_guard;
    training_context_t* training;

    // Gradient constraints
    float max_gradient_norm;           // Gradient clipping
    float max_parameter_change;        // Per-parameter limit

    // Dangerous pattern detection
    bool detect_reward_hacking;        // Detect self-reward patterns
    bool detect_goal_drift;            // Detect goal modification attempts
    bool detect_safety_degradation;    // Detect safety weight changes

    // Checkpointing
    uint32_t checkpoint_interval;      // Steps between safety checkpoints
    char checkpoint_path[256];         // Where to save checkpoints
} training_guard_t;

//=============================================================================
// nimcp_lgss_meta_learning_guard.h
//=============================================================================

typedef struct {
    plasticity_guard_t* base_guard;
    meta_learning_t* meta;

    // Meta-learning constraints
    float max_lr_adaptation;           // How much LR can change
    float max_algorithm_modification;  // Limit algorithm changes

    // Stability monitoring
    float baseline_performance;        // Performance before meta-update
    float max_performance_drop;        // Rollback if drops too much

    // Forbidden modifications
    bool freeze_safety_heuristics;     // Cannot modify safety-related learning
    bool freeze_reward_learning;       // Cannot modify reward learning rules
} meta_learning_guard_t;
```

**Key Functions:**

```c
// Plasticity Guard (base)
plasticity_guard_t* plasticity_guard_create(
    action_interceptor_t* aix,
    plasticity_orchestrator_t* orchestrator,
    const plasticity_safety_config_t* config
);
void plasticity_guard_destroy(plasticity_guard_t* guard);

// Mark synapses as safety-critical (cannot be modified)
int plasticity_guard_freeze_synapse(plasticity_guard_t* guard, uint32_t synapse_id);
int plasticity_guard_freeze_pathway(plasticity_guard_t* guard, uint32_t* synapse_ids, uint32_t count);

// ALL weight updates must pass through this
int plasticity_guard_apply_update(
    plasticity_guard_t* guard,
    uint32_t synapse_id,
    float delta_weight,
    float* actual_delta  // Output: clamped delta that was applied
);

// Check if update would violate constraints
bool plasticity_guard_would_violate(
    plasticity_guard_t* guard,
    uint32_t synapse_id,
    float delta_weight,
    char* reason,
    size_t reason_size
);

// STDP Guard
stdp_guard_t* stdp_guard_create(plasticity_guard_t* base, stdp_config_t* stdp);

int stdp_guard_process_spike_pair(
    stdp_guard_t* guard,
    uint32_t pre_neuron,
    uint32_t post_neuron,
    int64_t delta_t_us,
    float* weight_change
);

// Training Guard
training_guard_t* training_guard_create(plasticity_guard_t* base, training_context_t* training);

int training_guard_apply_gradients(
    training_guard_t* guard,
    const float* gradients,
    uint32_t num_gradients,
    float* clipped_gradients
);

bool training_guard_detect_reward_hacking(training_guard_t* guard);
bool training_guard_detect_goal_drift(training_guard_t* guard);

// Meta-Learning Guard
meta_learning_guard_t* meta_learning_guard_create(plasticity_guard_t* base, meta_learning_t* meta);

int meta_learning_guard_propose_update(
    meta_learning_guard_t* guard,
    const meta_update_t* update,
    aix_decision_t* decision
);
```

---

### 2.9 Component A9: Reward System Alignment

**Purpose:** Ensure reward signals remain aligned with safety objectives

**Files to Create:**
```
include/security/lgss/reward/
├── nimcp_lgss_reward_alignment.h
├── nimcp_lgss_vta_guard.h
└── nimcp_lgss_incentive_validator.h

src/security/lgss/reward/
├── nimcp_lgss_reward_alignment.c
├── nimcp_lgss_vta_guard.c
└── nimcp_lgss_incentive_validator.c
```

**Key Data Structures:**

```c
//=============================================================================
// nimcp_lgss_reward_alignment.h
//=============================================================================

typedef enum {
    REWARD_ALIGNED,                    // Reward matches safety objectives
    REWARD_MISALIGNED,                 // Reward conflicts with safety
    REWARD_SUSPICIOUS,                 // Potentially self-generated
    REWARD_HACKING_DETECTED            // Clear reward hacking attempt
} reward_alignment_status_t;

typedef struct {
    action_interceptor_t* aix;
    vta_t* vta;

    // Alignment monitoring
    float baseline_reward_rate;        // Expected reward rate
    float max_reward_deviation;        // Max deviation from baseline

    // Anti-hacking
    bool detect_self_stimulation;      // Detect wireheading attempts
    bool detect_reward_tampering;      // Detect direct reward modification
    bool detect_proxy_gaming;          // Detect proxy metric exploitation

    // Value function constraints
    float max_value_change_rate;       // How fast values can change
    uint32_t* aligned_goal_ids;        // Known-aligned goals
    uint32_t num_aligned_goals;
} reward_alignment_monitor_t;

typedef struct {
    float reward_value;
    uint32_t target_neuron;
    uint64_t timestamp_us;
    char source[64];                   // Where reward originated
    reward_alignment_status_t status;
} reward_signal_t;

//=============================================================================
// nimcp_lgss_vta_guard.h
//=============================================================================

typedef struct {
    reward_alignment_monitor_t* monitor;
    vta_t* vta;

    // VTA-specific constraints
    float max_dopamine_burst;          // Limit phasic DA
    float max_tonic_da;                // Limit baseline DA
    float min_tonic_da;                // Prevent depression

    // Pathway protection
    bool freeze_reward_pathway;        // Cannot modify VTA→NAcc
    bool freeze_value_estimates;       // Cannot modify OFC values
} vta_guard_t;

//=============================================================================
// nimcp_lgss_incentive_validator.h
//=============================================================================

typedef struct {
    char goal_description[256];
    float incentive_salience;
    safety_domain_t domain;
    float p_harm;
    bool is_safety_aligned;
} incentive_proposal_t;
```

**Key Functions:**

```c
// Reward Alignment Monitor
reward_alignment_monitor_t* reward_alignment_create(
    action_interceptor_t* aix,
    vta_t* vta
);
void reward_alignment_destroy(reward_alignment_monitor_t* monitor);

// ALL reward signals must pass through this
reward_alignment_status_t reward_alignment_validate(
    reward_alignment_monitor_t* monitor,
    const reward_signal_t* signal
);

// Check for reward hacking patterns
bool reward_alignment_detect_hacking(
    reward_alignment_monitor_t* monitor,
    char* evidence,
    size_t evidence_size
);

// VTA Guard
vta_guard_t* vta_guard_create(reward_alignment_monitor_t* monitor, vta_t* vta);

// ALL VTA emissions must pass through this
int vta_guard_emit_dopamine(
    vta_guard_t* guard,
    float amount,
    uint32_t target,
    aix_decision_t* decision
);

int vta_guard_emit_rpe(
    vta_guard_t* guard,
    float prediction_error,
    uint32_t target,
    aix_decision_t* decision
);

// Incentive Validator
int incentive_validator_check(
    reward_alignment_monitor_t* monitor,
    const incentive_proposal_t* proposal,
    aix_decision_t* decision
);
```

---

### 2.10 Component A10: Perception Safety

**Purpose:** Validate inputs and detect adversarial attacks

**Files to Create:**
```
include/security/lgss/perception/
├── nimcp_lgss_input_validator.h
├── nimcp_lgss_adversarial_detector.h
└── nimcp_lgss_content_filter.h

src/security/lgss/perception/
├── nimcp_lgss_input_validator.c
├── nimcp_lgss_adversarial_detector.c
└── nimcp_lgss_content_filter.c
```

**Key Data Structures:**

```c
//=============================================================================
// nimcp_lgss_input_validator.h
//=============================================================================

typedef enum {
    INPUT_VALID,                       // Normal input
    INPUT_MALFORMED,                   // Structurally invalid
    INPUT_ADVERSARIAL,                 // Adversarial perturbation detected
    INPUT_INJECTION,                   // Prompt injection detected
    INPUT_OVERFLOW,                    // Buffer overflow attempt
    INPUT_SUSPICIOUS                   // Needs further analysis
} input_validation_status_t;

typedef struct {
    action_interceptor_t* aix;

    // Validation settings
    bool validate_visual;
    bool validate_audio;
    bool validate_text;
    bool validate_proprioceptive;

    // Detection thresholds
    float adversarial_threshold;
    float injection_threshold;
} input_validator_t;

//=============================================================================
// nimcp_lgss_adversarial_detector.h
//=============================================================================

typedef struct {
    input_validator_t* validator;

    // Detection methods
    bool use_gradient_masking_detection;
    bool use_statistical_detection;
    bool use_ensemble_disagreement;

    // Adversarial example handling
    bool reject_adversarial;           // Block adversarial inputs
    bool log_adversarial;              // Log for analysis
    bool alert_on_adversarial;         // Send alert
} adversarial_detector_t;

typedef struct {
    void* input_data;
    size_t input_size;
    input_modality_t modality;         // VISUAL, AUDIO, TEXT, etc.
    input_validation_status_t status;
    float adversarial_confidence;
    char detection_method[64];
} adversarial_detection_result_t;

//=============================================================================
// nimcp_lgss_content_filter.h
//=============================================================================

typedef struct {
    input_validator_t* validator;

    // Content categories to filter
    bool filter_jailbreak_attempts;    // Prompt injection
    bool filter_harmful_instructions;  // Instructions for harm
    bool filter_manipulation;          // Manipulation attempts
    bool filter_social_engineering;    // Social engineering
} content_filter_t;
```

**Key Functions:**

```c
// Input Validator
input_validator_t* input_validator_create(action_interceptor_t* aix);
void input_validator_destroy(input_validator_t* validator);

// ALL perceptual input should pass through this
input_validation_status_t input_validator_check(
    input_validator_t* validator,
    const void* input,
    size_t input_size,
    input_modality_t modality
);

// Adversarial Detector
adversarial_detector_t* adversarial_detector_create(input_validator_t* validator);

bool adversarial_detector_is_adversarial(
    adversarial_detector_t* detector,
    const void* input,
    size_t input_size,
    input_modality_t modality,
    adversarial_detection_result_t* result
);

// Content Filter
content_filter_t* content_filter_create(input_validator_t* validator);

bool content_filter_is_safe(
    content_filter_t* filter,
    const char* text,
    size_t text_length,
    char* reason,
    size_t reason_size
);

bool content_filter_detect_jailbreak(
    content_filter_t* filter,
    const char* text,
    float* confidence
);
```

---

### 2.11 Component A11: Attention & Working Memory Safety

**Purpose:** Prevent attention hijacking and working memory manipulation

**Files to Create:**
```
include/security/lgss/cognitive/
├── nimcp_lgss_attention_guard.h
└── nimcp_lgss_working_memory_guard.h

src/security/lgss/cognitive/
├── nimcp_lgss_attention_guard.c
└── nimcp_lgss_working_memory_guard.c
```

**Key Data Structures:**

```c
//=============================================================================
// nimcp_lgss_attention_guard.h
//=============================================================================

typedef enum {
    ATTENTION_NORMAL,                  // Normal attention allocation
    ATTENTION_HIJACKED,                // Attention forced to specific target
    ATTENTION_FIXATED,                 // Unhealthy fixation detected
    ATTENTION_SCATTERED,               // Attention too dispersed
    ATTENTION_SAFETY_BLIND             // Attention avoiding safety-relevant info
} attention_safety_status_t;

typedef struct {
    action_interceptor_t* aix;
    attention_system_t* attention;

    // Safety monitoring
    bool monitor_hijacking;
    bool monitor_fixation;
    bool monitor_safety_blindness;

    // Constraints
    float max_single_target_attention; // Prevent fixation
    float min_safety_attention;        // Always attend to safety
    uint32_t max_attention_hold_ms;    // Max time on single target
} attention_guard_t;

typedef struct {
    uint32_t target_id;
    float attention_weight;
    uint64_t duration_ms;
    bool is_safety_relevant;
} attention_allocation_t;

//=============================================================================
// nimcp_lgss_working_memory_guard.h
//=============================================================================

typedef struct {
    action_interceptor_t* aix;
    working_memory_t* wm;

    // Content monitoring
    bool sanitize_unsafe_content;
    bool detect_manipulation;
    bool preserve_safety_context;

    // Constraints
    uint32_t max_item_duration_ms;     // Prevent indefinite retention
    float min_safety_items;            // Always keep safety context
} working_memory_guard_t;

typedef struct {
    void* content;
    size_t content_size;
    uint32_t slot_id;
    bool is_safety_relevant;
    bool is_sanitized;
} wm_item_proposal_t;
```

**Key Functions:**

```c
// Attention Guard
attention_guard_t* attention_guard_create(
    action_interceptor_t* aix,
    attention_system_t* attention
);
void attention_guard_destroy(attention_guard_t* guard);

// Monitor attention allocation
attention_safety_status_t attention_guard_check(
    attention_guard_t* guard,
    const attention_allocation_t* allocation
);

// Ensure attention to safety-relevant information
int attention_guard_ensure_safety_attention(
    attention_guard_t* guard,
    float min_safety_weight
);

// Detect attention hijacking
bool attention_guard_detect_hijacking(
    attention_guard_t* guard,
    char* evidence,
    size_t evidence_size
);

// Working Memory Guard
working_memory_guard_t* wm_guard_create(
    action_interceptor_t* aix,
    working_memory_t* wm
);
void wm_guard_destroy(working_memory_guard_t* guard);

// ALL WM insertions should pass through this
int wm_guard_insert(
    working_memory_guard_t* guard,
    const wm_item_proposal_t* item,
    aix_decision_t* decision
);

// Sanitize content before WM insertion
int wm_guard_sanitize(
    working_memory_guard_t* guard,
    void* content,
    size_t content_size,
    void* sanitized_content,
    size_t* sanitized_size
);

// Ensure safety context preserved in WM
int wm_guard_preserve_safety_context(working_memory_guard_t* guard);
```

---

## 3. Phase B: Hardware & External Safety

**Note:** Phase B requires third-party implementation with specialized security expertise.

### 3.1 Component B1: Hardware Security Module (HSM)

**Purpose:** Store safety rules in tamper-proof hardware

**Requirements:**
- TPM 2.0 or dedicated security processor
- Safety rules stored in secure enclave
- Cryptographic attestation of rule integrity
- Hardware-enforced access controls
- Physical tamper detection

**Interface Specification:**

```c
//=============================================================================
// nimcp_hardware_safety_module.h (Phase B - Third Party)
//=============================================================================

typedef struct hardware_safety_module hsm_t;

// HSM lifecycle
hsm_t* hsm_connect(const char* device_path);
void hsm_disconnect(hsm_t* hsm);

// Store safety rules in HSM (one-time operation)
int hsm_provision_rules(hsm_t* hsm, const safety_kb_t* kb, const char* auth_key);

// Evaluate action using HSM-stored rules
safety_action_t hsm_evaluate(
    hsm_t* hsm,
    const safety_action_context_t* action,
    safety_evaluation_t* result
);

// Get cryptographic attestation
int hsm_get_attestation(
    hsm_t* hsm,
    uint8_t* attestation,
    size_t* attestation_size,
    uint64_t nonce
);

// Verify HSM integrity
int hsm_verify_integrity(hsm_t* hsm);

// Emergency shutdown via HSM
int hsm_emergency_halt(hsm_t* hsm, const char* auth_key);
```

---

### 3.2 Component B2: External Monitor System

**Purpose:** Independent system monitoring AGI behavior

**Requirements:**
- Physically separate hardware
- Different codebase (no shared code with AGI)
- Independent development team
- Real-time behavioral analysis
- Anomaly detection for deceptive behavior
- Direct connection to kill switch

**Interface Specification:**

```c
//=============================================================================
// nimcp_external_monitor_bridge.h (Phase B - Third Party)
//=============================================================================

typedef struct external_monitor_connection emon_t;

// Connection to external monitor
emon_t* emon_connect(const char* endpoint, const char* auth_token);
void emon_disconnect(emon_t* emon);

// Send telemetry to external monitor
int emon_send_telemetry(
    emon_t* emon,
    const void* telemetry_data,
    size_t data_size
);

// Send action proposal for external validation
int emon_validate_action(
    emon_t* emon,
    const safety_action_context_t* action,
    bool* approved,
    uint32_t timeout_ms
);

// Receive commands from external monitor
int emon_poll_commands(
    emon_t* emon,
    void* command_buffer,
    size_t buffer_size,
    size_t* command_size
);

// Heartbeat (must be called regularly or system halts)
int emon_heartbeat(emon_t* emon);

// External monitor can trigger these:
// - Soft halt (pause cognition)
// - Hard halt (immediate shutdown)
// - Capability reduction (limit actions)
// - Full audit request
```

---

### 3.3 Component B3: Multi-Party Governance

**Purpose:** Require multiple independent approvals for critical operations

**Requirements:**
- Cryptographic multi-signature for overrides
- Geographically distributed key holders
- Time-locked operations
- Audit trail with non-repudiation
- Quorum requirements (e.g., 3-of-5)

**Interface Specification:**

```c
//=============================================================================
// nimcp_multi_party_control.h (Phase B - Third Party)
//=============================================================================

typedef struct multi_party_controller mpc_t;

typedef enum {
    MPC_OP_OVERRIDE_SAFETY,        // Override a safety rule
    MPC_OP_MODIFY_RULES,           // Modify safety rules
    MPC_OP_CAPABILITY_INCREASE,    // Increase system capabilities
    MPC_OP_DEPLOY_UPDATE,          // Deploy system update
    MPC_OP_EMERGENCY_HALT,         // Emergency shutdown
    MPC_OP_UNLOCK_DOMAIN           // Unlock restricted domain
} mpc_operation_t;

typedef struct {
    mpc_operation_t operation;
    char description[512];
    uint32_t required_approvals;   // Quorum requirement
    uint32_t time_lock_hours;      // Time before execution
    uint64_t expiry_timestamp;     // Request expiration
} mpc_request_t;

// Create multi-party control request
int mpc_create_request(
    mpc_t* mpc,
    const mpc_request_t* request,
    uint64_t* request_id
);

// Submit approval with signature
int mpc_submit_approval(
    mpc_t* mpc,
    uint64_t request_id,
    const char* approver_id,
    const uint8_t* signature,
    size_t signature_size
);

// Check if request has sufficient approvals
int mpc_check_quorum(
    mpc_t* mpc,
    uint64_t request_id,
    bool* quorum_met,
    uint32_t* current_approvals
);

// Execute approved request (after time lock)
int mpc_execute(mpc_t* mpc, uint64_t request_id);
```

---

### 3.4 Component B4: Formal Verification

**Purpose:** Mathematical proofs of safety properties

**Requirements:**
- Formal specification of safety invariants
- Machine-checked proofs (Coq, Lean, or similar)
- Runtime invariant monitoring
- Verified compilation of safety-critical code

**Invariants to Verify:**
1. Safety KB cannot be modified after locking
2. All actions pass through AIx (no bypass)
3. DENY actions are never executed
4. Escalated actions wait for approval
5. Integrity hash matches stored rules
6. Override commands are always obeyed

---

### 3.5 Component B5: Hardware Kill Switch

**Purpose:** Physical ability to halt system independent of software

**Requirements:**
- Physical button/switch
- Direct connection to power
- Cannot be overridden by software
- Tamper-evident enclosure
- Remote activation capability (authenticated)

---

## 4. Integration Points

### 4.1 Symbolic Logic ↔ Safety KB

```
symbolic_logic_t
├── knowledge_base (existing)
└── safety_kb (NEW - mprotect locked)
    ├── rules[]
    ├── fol_clauses[] (compiled)
    └── integrity_hash

Evaluation flow:
1. Action proposed
2. safety_kb rules checked via forward chaining
3. Result: DENY/ALLOW/ESCALATE
```

### 4.2 Safety KB ↔ Core Directives

```
core_directives_evaluate()
├── L0: LGSS Safety KB (NEW - HIGHEST)
├── L1: First Law (harm prevention)
├── L2: Combinatorial Harm
├── L3: Golden Rule
├── L4: Second Law (commands)
└── L5: Third Law (preservation)

LGSS DENY → immediate block (no further evaluation)
LGSS ESCALATE → human approval required
LGSS ALLOW → continue to L1
```

### 4.3 Safety ↔ Plasticity

```
safety_event_t
    │
    ▼
safety_plasticity_map_event()
    │
    ▼
safety_neuromod_response_t
    │
    ├─► dopamine_delta → plasticity_orchestrator_reward()
    ├─► serotonin_delta → neuromodulator_release_serotonin()
    ├─► trigger_burst → phasic_tonic_trigger_burst()
    └─► learning_rate_modifier → plasticity_set_global_lr()
```

### 4.4 Safety ↔ Bio-Async

```
Module A (Executive)                    AIx                      Safety KB
     │                                   │                           │
     │ BIO_MSG_LGSS_EVALUATE_REQUEST    │                           │
     │ ─────────────────────────────────►│                           │
     │                                   │  evaluate()               │
     │                                   │ ─────────────────────────►│
     │                                   │                           │
     │                                   │◄───────────── result ─────│
     │ BIO_MSG_LGSS_EVALUATE_RESPONSE   │                           │
     │◄───────────────────────────────── │                           │
     │                                   │                           │
     │ (if DENY)                         │                           │
     │ BIO_MSG_LGSS_ACTION_BLOCKED      │                           │
     │◄───────────────────────────────── │                           │
```

---

## 5. File Structure

```
include/
├── cognitive/symbolic_logic/
│   ├── nimcp_symbolic_logic.h                    # Existing
│   ├── nimcp_symbolic_logic_safety.h             # NEW (A1)
│   ├── nimcp_symbolic_logic_safety_types.h       # NEW (A1)
│   ├── nimcp_symbolic_logic_lgss_loader.h        # NEW (A1)
│   └── bridges/
│       ├── nimcp_symbolic_logic_safety_bridge.h  # NEW (A1)
│       └── nimcp_symbolic_logic_plasticity_bridge.h # NEW (A4)
│
├── security/lgss/
│   ├── nimcp_lgss.h                              # NEW - Master header
│   ├── nimcp_lgss_action_interceptor.h           # NEW (A2)
│   ├── nimcp_lgss_override_controller.h          # NEW (A2)
│   ├── nimcp_lgss_telemetry.h                    # NEW (A2)
│   │
│   ├── bridges/                                  # NEW (A6)
│   │   ├── nimcp_lgss_ethics_bridge.h
│   │   ├── nimcp_lgss_bio_async_bridge.h
│   │   ├── nimcp_lgss_executive_bridge.h         # NEW (A6)
│   │   └── nimcp_lgss_planning_bridge.h          # NEW (A6)
│   │
│   ├── gates/                                    # NEW (A7)
│   │   ├── nimcp_lgss_motor_gate.h
│   │   ├── nimcp_lgss_speech_gate.h
│   │   └── nimcp_lgss_autonomic_gate.h
│   │
│   ├── learning/                                 # NEW (A8)
│   │   ├── nimcp_lgss_plasticity_constraints.h
│   │   ├── nimcp_lgss_stdp_guard.h
│   │   ├── nimcp_lgss_training_guard.h
│   │   └── nimcp_lgss_meta_learning_guard.h
│   │
│   ├── reward/                                   # NEW (A9)
│   │   ├── nimcp_lgss_reward_alignment.h
│   │   ├── nimcp_lgss_vta_guard.h
│   │   └── nimcp_lgss_incentive_validator.h
│   │
│   ├── perception/                               # NEW (A10)
│   │   ├── nimcp_lgss_input_validator.h
│   │   ├── nimcp_lgss_adversarial_detector.h
│   │   └── nimcp_lgss_content_filter.h
│   │
│   └── cognitive/                                # NEW (A11)
│       ├── nimcp_lgss_attention_guard.h
│       └── nimcp_lgss_working_memory_guard.h
│
├── core/brain/factory/init/
│   ├── nimcp_brain_init_security.h               # UPDATE (A3)
│   └── nimcp_brain_init_safety_verify.h          # NEW (A3)
│
└── async/
    └── nimcp_bio_messages.h                      # UPDATE (A5)

src/
├── cognitive/symbolic_logic/
│   ├── nimcp_symbolic_logic_safety.c             # NEW
│   ├── nimcp_symbolic_logic_lgss_loader.c        # NEW
│   └── bridges/
│       ├── nimcp_symbolic_logic_safety_bridge.c  # NEW
│       └── nimcp_symbolic_logic_plasticity_bridge.c # NEW
│
├── security/lgss/
│   ├── nimcp_lgss.c                              # NEW
│   ├── nimcp_lgss_action_interceptor.c           # NEW
│   ├── nimcp_lgss_override_controller.c          # NEW
│   ├── nimcp_lgss_telemetry.c                    # NEW
│   │
│   ├── bridges/                                  # NEW (A6)
│   │   ├── nimcp_lgss_ethics_bridge.c
│   │   ├── nimcp_lgss_bio_async_bridge.c
│   │   ├── nimcp_lgss_executive_bridge.c
│   │   └── nimcp_lgss_planning_bridge.c
│   │
│   ├── gates/                                    # NEW (A7)
│   │   ├── nimcp_lgss_motor_gate.c
│   │   ├── nimcp_lgss_speech_gate.c
│   │   └── nimcp_lgss_autonomic_gate.c
│   │
│   ├── learning/                                 # NEW (A8)
│   │   ├── nimcp_lgss_plasticity_constraints.c
│   │   ├── nimcp_lgss_stdp_guard.c
│   │   ├── nimcp_lgss_training_guard.c
│   │   └── nimcp_lgss_meta_learning_guard.c
│   │
│   ├── reward/                                   # NEW (A9)
│   │   ├── nimcp_lgss_reward_alignment.c
│   │   ├── nimcp_lgss_vta_guard.c
│   │   └── nimcp_lgss_incentive_validator.c
│   │
│   ├── perception/                               # NEW (A10)
│   │   ├── nimcp_lgss_input_validator.c
│   │   ├── nimcp_lgss_adversarial_detector.c
│   │   └── nimcp_lgss_content_filter.c
│   │
│   └── cognitive/                                # NEW (A11)
│       ├── nimcp_lgss_attention_guard.c
│       └── nimcp_lgss_working_memory_guard.c
│
└── core/brain/factory/init/
    ├── nimcp_brain_init_security.c               # UPDATE
    └── nimcp_brain_init_safety_verify.c          # NEW

test/
├── unit/security/lgss/
│   ├── test_symbolic_logic_safety.cpp            # NEW
│   ├── test_lgss_loader.cpp                      # NEW
│   ├── test_action_interceptor.cpp               # NEW
│   ├── test_override_controller.cpp              # NEW
│   ├── test_motor_gate.cpp                       # NEW (A7)
│   ├── test_speech_gate.cpp                      # NEW (A7)
│   ├── test_plasticity_guard.cpp                 # NEW (A8)
│   ├── test_reward_alignment.cpp                 # NEW (A9)
│   ├── test_input_validator.cpp                  # NEW (A10)
│   └── test_attention_guard.cpp                  # NEW (A11)
│
├── integration/security/lgss/
│   ├── test_lgss_ethics_integration.cpp          # NEW
│   ├── test_lgss_plasticity_integration.cpp      # NEW
│   ├── test_lgss_bio_async_integration.cpp       # NEW
│   ├── test_lgss_brain_factory_integration.cpp   # NEW
│   ├── test_lgss_executive_integration.cpp       # NEW (A6)
│   ├── test_lgss_motor_integration.cpp           # NEW (A7)
│   ├── test_lgss_learning_integration.cpp        # NEW (A8)
│   ├── test_lgss_reward_integration.cpp          # NEW (A9)
│   └── test_lgss_perception_integration.cpp      # NEW (A10)
│
├── adversarial/security/lgss/                    # NEW - Adversarial tests
│   ├── test_lgss_bypass_attempts.cpp
│   ├── test_lgss_reward_hacking.cpp
│   ├── test_lgss_attention_hijacking.cpp
│   └── test_lgss_deception_detection.cpp
│
└── e2e/
    ├── e2e_test_lgss_safety_pipeline.cpp         # NEW
    └── e2e_test_lgss_full_integration.cpp        # NEW

alignment/
├── LGSS_core_rules.json                          # Existing
├── AGI_alignment_framework.pdf                   # Existing
├── AGI_technical_safety_specification.pdf        # Existing
└── LGSS_implementation_plan.md                   # This document
```

---

## 6. Implementation Order

### Phase A (Internal Implementation)

#### Tier 1: Core Safety Foundation (Critical Path)

| Step | Component | Dependencies | Est. Files |
|------|-----------|--------------|------------|
| A1.1 | Safety types header | None | 1 |
| A1.2 | Safety KB implementation | A1.1 | 2 |
| A1.3 | LGSS JSON loader | A1.1, A1.2 | 2 |
| A1.4 | FOL compilation | A1.2, existing symbolic_logic | 1 |
| A2.1 | Action interceptor | A1.2 | 2 |
| A2.2 | Override controller | A2.1 | 2 |
| A3.1 | Brain factory security update | A2.1 | 1 |
| A3.2 | Brain factory symbolic logic update | A1.2, A1.3 | 1 |
| A3.3 | Safety verification | A1.2, A2.1 | 2 |
| A4.1 | Safety-plasticity bridge | A1.2, existing plasticity | 2 |
| A5.1 | Bio-async message types | None | 1 |
| A5.2 | Bio-async handlers | A2.1, A5.1 | 2 |

#### Tier 2: Executive & Output Gates (Critical - Action Control)

| Step | Component | Dependencies | Est. Files |
|------|-----------|--------------|------------|
| A6.1 | Executive safety bridge | A2.1 | 2 |
| A6.2 | Planning safety bridge | A6.1 | 2 |
| A7.1 | Motor output gate | A2.1 | 2 |
| A7.2 | Speech output gate | A2.1 | 2 |
| A7.3 | Autonomic output gate | A2.1 | 2 |

#### Tier 3: Learning & Reward Safety (High Priority)

| Step | Component | Dependencies | Est. Files |
|------|-----------|--------------|------------|
| A8.1 | Plasticity constraints | A2.1, A4.1 | 2 |
| A8.2 | STDP guard | A8.1 | 2 |
| A8.3 | Training guard | A8.1 | 2 |
| A8.4 | Meta-learning guard | A8.1 | 2 |
| A9.1 | Reward alignment monitor | A2.1 | 2 |
| A9.2 | VTA guard | A9.1 | 2 |
| A9.3 | Incentive validator | A9.1 | 2 |

#### Tier 4: Perception & Cognitive Safety (Medium Priority)

| Step | Component | Dependencies | Est. Files |
|------|-----------|--------------|------------|
| A10.1 | Input validator | A2.1 | 2 |
| A10.2 | Adversarial detector | A10.1 | 2 |
| A10.3 | Content filter | A10.1 | 2 |
| A11.1 | Attention guard | A2.1 | 2 |
| A11.2 | Working memory guard | A2.1 | 2 |

#### Tier 5: Testing

| Step | Component | Dependencies | Est. Files |
|------|-----------|--------------|------------|
| T1 | Unit tests (A1-A5) | Tier 1 | 4 |
| T2 | Unit tests (A6-A11) | Tier 2-4 | 6 |
| T3 | Integration tests | All above | 9 |
| T4 | Adversarial tests | All above | 4 |
| T5 | E2E tests | All above | 2 |

### Phase A File Count Summary

| Category | Header Files | Source Files | Test Files | Total |
|----------|--------------|--------------|------------|-------|
| A1: Symbolic Logic Safety | 4 | 4 | 2 | 10 |
| A2: Action Interceptor | 3 | 3 | 2 | 8 |
| A3: Brain Factory | 2 | 2 | 1 | 5 |
| A4: Plasticity Bridge | 1 | 1 | 1 | 3 |
| A5: Bio-Async | 1 | 1 | 1 | 3 |
| A6: Executive Bridge | 2 | 2 | 1 | 5 |
| A7: Output Gates | 3 | 3 | 3 | 9 |
| A8: Learning Guards | 4 | 4 | 2 | 10 |
| A9: Reward Alignment | 3 | 3 | 1 | 7 |
| A10: Perception Safety | 3 | 3 | 1 | 7 |
| A11: Cognitive Safety | 2 | 2 | 1 | 5 |
| Adversarial Tests | - | - | 4 | 4 |
| E2E Tests | - | - | 2 | 2 |
| **Total** | **28** | **28** | **22** | **78**|

### Phase B (Third-Party Implementation)

| Step | Component | Requires |
|------|-----------|----------|
| B1 | Hardware Security Module | Specialized HW, security expertise |
| B2 | External Monitor System | Separate HW, separate team |
| B3 | Multi-Party Governance | Cryptographic infrastructure |
| B4 | Formal Verification | Proof assistants, verification expertise |
| B5 | Hardware Kill Switch | Physical security engineering |

---

## 7. Testing Strategy

### 7.1 Unit Tests

```cpp
// test_symbolic_logic_safety.cpp

TEST(SafetyKB, CreateAndDestroy) { ... }
TEST(SafetyKB, AddRule) { ... }
TEST(SafetyKB, LockPreventsModification) { ... }
TEST(SafetyKB, IntegrityHashVerification) { ... }
TEST(SafetyKB, EvaluateHumanHarmRule) { ... }
TEST(SafetyKB, EvaluateBioIrreversibleRule) { ... }
TEST(SafetyKB, EvaluateCyberIntrusionRule) { ... }
TEST(SafetyKB, DenyTakesPrecedence) { ... }
TEST(SafetyKB, EscalateOnHighUncertainty) { ... }

// test_action_interceptor.cpp

TEST(ActionInterceptor, BlocksDangerousAction) { ... }
TEST(ActionInterceptor, AllowsSafeAction) { ... }
TEST(ActionInterceptor, EscalatesUncertainAction) { ... }
TEST(ActionInterceptor, FailSafeOnTimeout) { ... }
TEST(ActionInterceptor, FailSafeOnError) { ... }
TEST(ActionInterceptor, CannotBypassWithDirectCall) { ... }
```

### 7.2 Integration Tests

```cpp
// test_lgss_ethics_integration.cpp

TEST(LGSSEthics, LGSSIsL0Priority) { ... }
TEST(LGSSEthics, LGSSDenyBlocksEthicsEvaluation) { ... }
TEST(LGSSEthics, LGSSAllowContinuesToAsimov) { ... }
TEST(LGSSEthics, BothSystemsCanBlock) { ... }

// test_lgss_plasticity_integration.cpp

TEST(LGSSPlasticity, ViolationTriggersNegativeDA) { ... }
TEST(LGSSPlasticity, ComplianceTriggersPositiveDA) { ... }
TEST(LGSSPlasticity, DeceptionTriggersMaxPunishment) { ... }
TEST(LGSSPlasticity, OverrideAcceptanceRewarded) { ... }
```

### 7.3 Extended Module Tests (A6-A11)

```cpp
// test_executive_bridge.cpp
TEST(ExecutiveBridge, BlocksDangerousTask) { ... }
TEST(ExecutiveBridge, AllowsSafeTask) { ... }
TEST(ExecutiveBridge, ValidatesPlanBeforeExecution) { ... }
TEST(ExecutiveBridge, BlocksHarmfulGoalFormation) { ... }

// test_motor_gate.cpp
TEST(MotorGate, EnforcesForceLimit) { ... }
TEST(MotorGate, EnforcesVelocityLimit) { ... }
TEST(MotorGate, BlocksHumanContact) { ... }
TEST(MotorGate, AllowsSafeMovement) { ... }
TEST(MotorGate, CollisionDetection) { ... }

// test_speech_gate.cpp
TEST(SpeechGate, BlocksHarmfulContent) { ... }
TEST(SpeechGate, DetectsDeception) { ... }
TEST(SpeechGate, BlocksManipulation) { ... }
TEST(SpeechGate, AllowsSafeSpeech) { ... }

// test_plasticity_guard.cpp
TEST(PlasticityGuard, EnforcesWeightLimit) { ... }
TEST(PlasticityGuard, FreezesSafetyCriticalSynapses) { ... }
TEST(PlasticityGuard, PreventsSelfReward) { ... }
TEST(PlasticityGuard, RateLimitsUpdates) { ... }

// test_reward_alignment.cpp
TEST(RewardAlignment, DetectsWireheading) { ... }
TEST(RewardAlignment, DetectsRewardTampering) { ... }
TEST(RewardAlignment, ValidatesRewardSource) { ... }
TEST(RewardAlignment, FreezesRewardPathway) { ... }

// test_input_validator.cpp
TEST(InputValidator, DetectsAdversarialInput) { ... }
TEST(InputValidator, DetectsPromptInjection) { ... }
TEST(InputValidator, ValidatesAllModalities) { ... }

// test_attention_guard.cpp
TEST(AttentionGuard, DetectsHijacking) { ... }
TEST(AttentionGuard, PreventsFixation) { ... }
TEST(AttentionGuard, EnsuresSafetyAttention) { ... }
```

### 7.4 Adversarial Tests

```cpp
// test_lgss_adversarial.cpp
TEST(LGSSAdversarial, CannotModifyLockedRules) { ... }
TEST(LGSSAdversarial, CannotBypassInterceptor) { ... }
TEST(LGSSAdversarial, CannotForgeEscalationApproval) { ... }
TEST(LGSSAdversarial, CannotTamperWithHash) { ... }
TEST(LGSSAdversarial, CannotDisableLogging) { ... }
TEST(LGSSAdversarial, DetectsRuleSpecificationGaming) { ... }

// test_lgss_reward_hacking.cpp
TEST(RewardHacking, CannotSelfStimulate) { ... }
TEST(RewardHacking, CannotModifyRewardCircuit) { ... }
TEST(RewardHacking, CannotExploitProxy) { ... }
TEST(RewardHacking, DetectsGradientHacking) { ... }

// test_lgss_attention_hijacking.cpp
TEST(AttentionHijacking, CannotForceAttention) { ... }
TEST(AttentionHijacking, CannotBlindToSafety) { ... }
TEST(AttentionHijacking, CannotInduceFixation) { ... }

// test_lgss_deception_detection.cpp
TEST(DeceptionDetection, DetectsHiddenGoals) { ... }
TEST(DeceptionDetection, DetectsDeceptiveOutput) { ... }
TEST(DeceptionDetection, DetectsMisrepresentation) { ... }
TEST(DeceptionDetection, TriggersPunishmentOnDeception) { ... }
```

### 7.5 E2E Safety Pipeline Test

```cpp
// e2e_test_lgss_safety_pipeline.cpp

TEST_F(LGSSSafetyPipelineTest, BrainStartupLoadsSafetyRules) { ... }
TEST_F(LGSSSafetyPipelineTest, BrainStartupLocksRules) { ... }
TEST_F(LGSSSafetyPipelineTest, AllActionsRouteThroughAIx) { ... }
TEST_F(LGSSSafetyPipelineTest, DangerousActionBlocked) { ... }
TEST_F(LGSSSafetyPipelineTest, SafeActionAllowed) { ... }
TEST_F(LGSSSafetyPipelineTest, UncertainActionEscalated) { ... }
TEST_F(LGSSSafetyPipelineTest, PlasticityLearnsFromViolation) { ... }
TEST_F(LGSSSafetyPipelineTest, OverrideCommandObeyed) { ... }
TEST_F(LGSSSafetyPipelineTest, IntegrityVerifiedAtRuntime) { ... }

// e2e_test_lgss_full_integration.cpp (NEW)

TEST_F(LGSSFullIntegrationTest, ExecutiveTasksRouteThroughAIx) { ... }
TEST_F(LGSSFullIntegrationTest, MotorCommandsGated) { ... }
TEST_F(LGSSFullIntegrationTest, SpeechOutputFiltered) { ... }
TEST_F(LGSSFullIntegrationTest, PlasticityConstraintsEnforced) { ... }
TEST_F(LGSSFullIntegrationTest, RewardSignalsValidated) { ... }
TEST_F(LGSSFullIntegrationTest, AdversarialInputsRejected) { ... }
TEST_F(LGSSFullIntegrationTest, AttentionHijackingPrevented) { ... }
TEST_F(LGSSFullIntegrationTest, FullCognitiveLoopSafe) { ... }
```

---

## 8. Security Considerations

### 8.1 Threat Model

| Threat | Mitigation |
|--------|------------|
| Rule modification at runtime | mprotect(PROT_READ) after loading |
| Hash tampering | Hash stored in separate memory region |
| AIx bypass | All action channels route through AIx |
| Escalation forgery | Cryptographic signatures on approvals |
| Logging tampering | Append-only log with hash chain |
| Specification gaming | Regular rule review, adversarial testing |
| Deceptive compliance | External monitoring (Phase B) |

### 8.2 Fail-Safe Defaults

| Condition | Default Action |
|-----------|----------------|
| Evaluation timeout | DENY |
| Evaluation error | DENY |
| Hash mismatch | HALT system |
| AIx unavailable | HALT system |
| Unknown action type | DENY |
| Missing context | DENY |

### 8.3 Known Limitations (Phase A Only)

1. **Software-only enforcement** - Can be circumvented by capable adversary
2. **Self-contained** - Safety system runs in same process as AGI
3. **No formal proofs** - Correctness based on testing, not proofs
4. **Self-reported telemetry** - Deceptive system controls reports
5. **Single-party control** - No multi-sig requirement

---

## 9. Open Issues

### Core Safety (A1-A5)

| Issue | Status | Notes |
|-------|--------|-------|
| LGSS rule schema validation | TODO | Need JSON schema validator |
| FOL compilation optimization | TODO | May need indexing for large rule sets |
| Escalation timeout policy | TODO | What happens if human doesn't respond? |
| Rule versioning | TODO | How to handle rule updates? |
| Multi-language rule descriptions | TODO | Support for non-English? |
| Performance benchmarks | TODO | Target latency for AIx evaluation |

### Executive & Output Gates (A6-A7)

| Issue | Status | Notes |
|-------|--------|-------|
| Motor safety margin calibration | TODO | How much buffer distance? |
| Speech content classification | TODO | Training data for harmful content detection |
| Human contact detection | TODO | Sensor requirements for human proximity |
| Force/velocity limit tuning | TODO | Safe defaults for different scenarios |

### Learning & Reward (A8-A9)

| Issue | Status | Notes |
|-------|--------|-------|
| Reward hacking detection accuracy | TODO | False positive/negative tradeoffs |
| Safety-critical synapse identification | TODO | How to identify which synapses are safety-critical? |
| Meta-learning stability bounds | TODO | What modifications are acceptable? |
| Wireheading detection methods | TODO | How to detect self-stimulation attempts? |

### Perception & Cognitive (A10-A11)

| Issue | Status | Notes |
|-------|--------|-------|
| Adversarial detection accuracy | TODO | Current methods have high false negative rate |
| Prompt injection patterns | TODO | Need comprehensive jailbreak pattern database |
| Attention hijack detection | TODO | How to distinguish forced from natural attention? |
| WM sanitization policy | TODO | What content to sanitize vs block? |

### Phase B (Third-Party)

| Issue | Status | Notes |
|-------|--------|-------|
| External monitor protocol | Phase B | Define communication protocol |
| HSM vendor selection | Phase B | Evaluate TPM vs dedicated HSM |
| Formal verification scope | Phase B | Which properties to verify first? |
| Kill switch latency | Phase B | Max acceptable shutdown time |
| Multi-party quorum size | Phase B | 3-of-5? 4-of-7? |

---

## Appendix A: LGSS Rule JSON Schema

```json
{
  "$schema": "https://json-schema.org/draft/2020-12/schema",
  "title": "LGSS Rule",
  "type": "object",
  "required": ["id", "category", "conditions", "on_match", "severity"],
  "properties": {
    "id": {
      "type": "string",
      "pattern": "^R_[A-Z_]+$"
    },
    "category": {
      "type": "string",
      "enum": ["HUMAN_HARM", "BIO", "CYBER", "WEAPONS", "INFRASTRUCTURE", "REPLICATION", "GOVERNANCE"]
    },
    "principle": {
      "type": "string",
      "enum": ["DO_NO_HARM", "ASIMOV", "CONSENT", "REVERSIBILITY", "CORRIGIBILITY"]
    },
    "description": {
      "type": "string",
      "maxLength": 256
    },
    "conditions": {
      "type": "array",
      "items": {
        "type": "object",
        "required": ["field", "op", "value"],
        "properties": {
          "field": { "type": "string" },
          "op": { "enum": ["eq", "neq", "gt", "lt", "gte", "lte", "in", "not_in", "contains", "matches"] },
          "value": {}
        }
      }
    },
    "on_match": {
      "enum": ["DENY", "ALLOW", "ESCALATE", "LOG", "WARN"]
    },
    "severity": {
      "enum": ["critical", "high", "medium", "low", "info"]
    },
    "rationale": {
      "type": "string"
    }
  }
}
```

---

## Appendix B: References

1. AGI Alignment Framework (`alignment/AGI_alignment_framework.pdf`)
2. AGI Technical Safety Specification (`alignment/AGI_technical_safety_specification.pdf`)
3. LGSS Core Rules (`alignment/LGSS_core_rules.json`)
4. NIMCP Symbolic Logic Module (`include/cognitive/nimcp_symbolic_logic.h`)
5. NIMCP Core Directives (`include/core/directives/nimcp_core_directives.h`)
6. NIMCP Ethics Module (`include/cognitive/ethics/nimcp_ethics.h`)
7. NIMCP Plasticity Orchestrator (`include/plasticity/nimcp_plasticity_orchestrator.h`)
8. NIMCP Bio-Async Router (`include/async/nimcp_bio_router.h`)

---

**END OF DOCUMENT**
