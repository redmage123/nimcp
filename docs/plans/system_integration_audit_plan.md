# System Integration Audit Plan

**Version**: 1.0.0
**Date**: 2026-01-04
**Purpose**: Systematically verify all NIMCP modules are properly interconnected and functional using KG wiring infrastructure

---

## Overview

Use the existing `.aim/wiring/` KG infrastructure to systematically verify that all modules are properly interconnected and functional. This addresses the concern that individual modules may work in isolation but fail to integrate as a cohesive system.

---

## Existing Infrastructure

### KG Wiring Files
- `.aim/wiring/master.jsonl` - Core module connectivity
- `.aim/wiring/subsystems/*.jsonl` - Per-subsystem wiring (steering, immune, cognition, etc.)
- `.aim/wiring/platforms/*.jsonl` - Platform-specific adaptations (full, medium, constrained, minimal)
- `.aim/wiring/hardware/*.jsonl` - Hardware-specific configurations (cuda, rocm, loihi, etc.)
- `.aim/wiring/custom/*.jsonl` - User overrides

### Wiring Diagram System
- `include/async/nimcp_wiring_diagram.h` - Runtime module assembly API
- `src/async/nimcp_wiring_diagram.c` - Implementation
- Loads JSONL diagrams, merges based on profile, syncs to brain_kg, validates consistency

---

## Phase 1: KG Wiring Completeness Audit

**Goal**: Verify KG wiring diagrams are complete and consistent

| Step | Task | Method |
|------|------|--------|
| 1.1 | Inventory all wiring JSONL files | Parse `.aim/wiring/**/*.jsonl` |
| 1.2 | Extract all entities (modules) | Count unique modules per subsystem |
| 1.3 | Extract all relations (connections) | Build adjacency graph |
| 1.4 | Identify orphan entities | Modules with no inbound/outbound relations |
| 1.5 | Identify dangling references | Relations pointing to undefined entities |

**Output**: `audit/kg_completeness_report.md`

### Metrics to Collect
- Total entities per subsystem
- Total relations per subsystem
- Orphan entity count
- Dangling reference count
- Message type coverage

---

## Phase 2: Code-to-KG Alignment Audit

**Goal**: Verify code implements what KG declares

| Step | Task | Method |
|------|------|--------|
| 2.1 | Scan all `*_bridge.c` files | Extract registered message handlers |
| 2.2 | Scan all `bio_async` registrations | Find `nimcp_bio_register_handler()` calls |
| 2.3 | Compare KG `HANDLES_MESSAGE` relations | Match against actual registrations |
| 2.4 | Compare KG `SENDS_MESSAGE` relations | Verify `nimcp_bio_send()` calls exist |
| 2.5 | Flag mismatches | KG-declared but not implemented, or vice versa |

**Output**: `audit/code_kg_alignment_report.md`

### Mismatch Categories
- **KG-only**: Declared in wiring but not implemented in code
- **Code-only**: Implemented in code but not declared in wiring
- **Signature mismatch**: Handler exists but with wrong message type
- **Disabled**: Implemented but conditionally disabled

---

## Phase 3: Message Flow Tracing

**Goal**: Verify messages actually flow end-to-end

| Step | Task | Method |
|------|------|--------|
| 3.1 | Build message flow graph from KG | `A → BIO_MSG_X → B` chains |
| 3.2 | Identify critical paths | Input → Processing → Output pipelines |
| 3.3 | Create tracer tests | Inject test messages, verify delivery |
| 3.4 | Measure message latency | Time from send to handler invocation |
| 3.5 | Detect dead paths | Messages sent but never handled |

**Output**: `audit/message_flow_report.md`

### Critical Paths to Trace
1. **Reward Path**: Hypothalamus → SNc → Dopamine targets
2. **Threat Path**: Amygdala → Hypothalamus → Arousal → Attention
3. **Perception Path**: Sensory → Thalamus → Cortical regions
4. **Memory Path**: Experience → Hippocampus → Consolidation
5. **Ethics Path**: Action proposal → Ethics → Decision

---

## Phase 4: Module Initialization Audit

**Goal**: Verify all modules initialize and wire correctly at runtime

| Step | Task | Method |
|------|------|--------|
| 4.1 | Trace `nimcp_brain_create()` path | Follow all `init_*` functions |
| 4.2 | Verify `nimcp_wiring_load_all()` succeeds | Check diagram loading |
| 4.3 | Verify `nimcp_wiring_sync_to_kg()` succeeds | KG reflects wiring |
| 4.4 | Check module enable/disable by tier | Platform tier filtering works |
| 4.5 | Verify bio_async orchestrator sees modules | All expected modules registered |

**Output**: `audit/initialization_report.md`

### Initialization Checklist
- [ ] All JSONL files parse without error
- [ ] Entity count matches expected
- [ ] Relation count matches expected
- [ ] No duplicate entity names
- [ ] All message types are defined
- [ ] Tier filtering works correctly
- [ ] Hardware detection works correctly

---

## Phase 5: End-to-End Pipeline Tests

**Goal**: Verify real inputs flow through the entire system

| Test | Input | Expected Flow |
|------|-------|---------------|
| 5.1 | Simulated hunger signal | Hypothalamus → SNc → Dopamine → Executive |
| 5.2 | Simulated threat | Amygdala → Hypothalamus → Arousal → Attention |
| 5.3 | Simulated sensory input | Perception → Thalamus → Global Workspace |
| 5.4 | Simulated memory query | Hippocampus → Knowledge → Reasoning |
| 5.5 | Simulated ethical dilemma | Ethics → Executive → Global Workspace |

**Output**: `test/e2e/e2e_test_system_integration_pipeline.cpp`

### Test Requirements
- Each test injects a message at entry point
- Verify message reaches all expected handlers
- Verify correct transformations occur
- Verify timing constraints met
- Verify no dropped messages

---

## Implementation Architecture

```
┌─────────────────────────────────────────────────────────────────┐
│                    INTEGRATION AUDIT SYSTEM                      │
├─────────────────────────────────────────────────────────────────┤
│                                                                  │
│  ┌──────────────┐    ┌──────────────┐    ┌──────────────┐       │
│  │  KG Wiring   │───►│   Code       │───►│  Runtime     │       │
│  │  Scanner     │    │   Scanner    │    │  Tracer      │       │
│  │              │    │              │    │              │       │
│  │ .aim/wiring/ │    │ src/**/*.c   │    │ nimcp_brain  │       │
│  └──────────────┘    └──────────────┘    └──────────────┘       │
│         │                   │                   │                │
│         ▼                   ▼                   ▼                │
│  ┌─────────────────────────────────────────────────────────┐    │
│  │                  COMPARISON ENGINE                       │    │
│  │  - KG entities vs implemented modules                    │    │
│  │  - KG relations vs actual message flows                  │    │
│  │  - Expected handlers vs registered handlers              │    │
│  └─────────────────────────────────────────────────────────┘    │
│                            │                                     │
│                            ▼                                     │
│  ┌─────────────────────────────────────────────────────────┐    │
│  │                    AUDIT REPORTS                         │    │
│  │  - Gaps: What's missing?                                 │    │
│  │  - Orphans: What's unused?                               │    │
│  │  - Broken: What's not working?                           │    │
│  │  - Recommendations: What to fix?                         │    │
│  └─────────────────────────────────────────────────────────┘    │
│                                                                  │
└─────────────────────────────────────────────────────────────────┘
```

---

## Deliverables

| Deliverable | Description | Location |
|-------------|-------------|----------|
| Audit Tool | Scanner for KG and code analysis | `tools/integration_audit.py` |
| KG Report | Wiring completeness analysis | `audit/kg_completeness_report.md` |
| Alignment Report | Code-to-KG comparison | `audit/code_kg_alignment_report.md` |
| Flow Report | Message flow analysis | `audit/message_flow_report.md` |
| Init Report | Initialization verification | `audit/initialization_report.md` |
| E2E Tests | Pipeline integration tests | `test/e2e/e2e_test_system_integration_pipeline.cpp` |
| Fixes | Address identified gaps | Various source files |
| Updated KG | Sync wiring diagrams with reality | `.aim/wiring/*.jsonl` |

---

## Execution Order

1. **Phase 1**: KG Completeness (no code changes, read-only analysis)
2. **Phase 2**: Code-KG Alignment (identify gaps)
3. **Phase 3**: Message Flow (trace paths)
4. **Phase 4**: Initialization (runtime verification)
5. **Phase 5**: E2E Tests (functional validation)

---

## Success Criteria

- [ ] All KG entities have at least one relation
- [ ] All KG relations reference defined entities
- [ ] All `HANDLES_MESSAGE` relations have corresponding code
- [ ] All `SENDS_MESSAGE` relations have corresponding code
- [ ] All critical paths trace successfully end-to-end
- [ ] All modules initialize without error
- [ ] All E2E pipeline tests pass
- [ ] No orphan modules in code
- [ ] No dead-end message paths

---

## Risk Mitigation

| Risk | Mitigation |
|------|------------|
| Missing wiring definitions | Add to KG during audit |
| Stale KG entries | Remove or update during audit |
| Unimplemented handlers | Flag for implementation |
| Performance bottlenecks | Measure and optimize |
| Circular dependencies | Detect and refactor |

---

## Timeline Estimate

| Phase | Effort |
|-------|--------|
| Phase 1 | 1-2 hours (scripted scan) |
| Phase 2 | 2-4 hours (code analysis) |
| Phase 3 | 4-8 hours (graph building + tracing) |
| Phase 4 | 2-4 hours (runtime testing) |
| Phase 5 | 8-16 hours (E2E test development) |
| Fixes | Variable (depends on findings) |

**Total**: 17-34 hours + fix time

---

## Next Steps

1. Create `audit/` directory structure
2. Implement Phase 1 KG scanner
3. Generate initial completeness report
4. Review findings and prioritize gaps
5. Proceed with subsequent phases
