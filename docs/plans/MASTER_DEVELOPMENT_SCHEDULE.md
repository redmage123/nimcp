# NIMCP Master Development Schedule
## Integrated Brain Architecture, Extrapolation, and Generation Capabilities

**Version**: 3.3
**Date**: 2025-11-24
**Last Updated**: 2026-01-03
**Status**: Implementation Tracking Document
**Sources**:
- BRAIN_REGIONS_IMPLEMENTATION_PLAN.md (11 phases)
- EXTRAPOLATION_CAPABILITIES.md (Part 1 & 2)
- IMAGINATION_AND_GENERATION.md
- Asimov's Three Laws of Robotics
- The Golden Rule (ethical reciprocity)

---

## Executive Summary

This master schedule integrates **seven major development tracks** into a single unified implementation roadmap, organized in **logical dependency order**:

1. **Core Directives** - Asimov's Laws, Golden Rule, Combinatorial Harm Detection (FOUNDATIONAL)
2. **Neural Immune System** - Blood-Brain Barrier, T-Cells, B-Cells, Threat Defense (SECURITY)
3. **Brain Regions** - 11 neuroanatomical systems
4. **Extrapolation Capabilities** - Causal, analogical, compositional reasoning
5. **Imagination & Generation** - Visual, audio, speech, video generation
6. **Social & Metacognition** - Theory of Mind, confidence, global workspace
7. **Superhuman Enhancements** - Beyond-human sensory and cognitive capabilities

### Implementation Status Summary (Updated 2026-01-03)

| Tier | Status | Completion |
|------|--------|------------|
| 0 - Infrastructure | ✅ COMPLETE | 100% |
| 0.5 - Neural Immune System | ✅ COMPLETE | 100% |
| 1 - Core Brain (Medulla, Cerebellum) | ✅ COMPLETE | 100% |
| 2 - Executive (PFC, Hippocampus) | ✅ COMPLETE | 100% |
| 3 - Action & Emotion (BG, Amygdala) | ✅ COMPLETE | 100% |
| 4 - Language & Self | ✅ COMPLETE | 100% |
| 5-7 - Extrapolation & World Models | ✅ SUBSTANTIAL | 85% |
| 8 - Generation & Imagination | ✅ COMPLETE | 100% |
| 9 - Specialized Cognitive | ✅ COMPLETE | 100% |
| 10 - Social & Metacognition | ✅ COMPLETE | 100% |
| 11 - Embodiment | 🟡 PARTIAL | 60% |
| 12 - Superhuman | 🟡 PARTIAL | 50% |

**Overall Progress: ~95% of core-to-advanced tiers complete**

**Codebase Statistics:**
- Total C/H files: 1,379
- Total lines of code: 972,269+
- Test coverage: 3,000+ tests

### Total Scope
| Track | Phases | Estimated LOC | Key Deliverables | Status |
|-------|--------|---------------|------------------|--------|
| **Core Directives** | **1 phase** | **~3,750** | **Asimov's Laws, Golden Rule, Combinatorial Harm Detection** | ✅ COMPLETE |
| **Neural Immune System** | **6 phases** | **~17,000** | **BBB, T-Cells, B-Cells, Digital Antibodies, Evolutionary Learning, Zero-Day Combat** | ✅ COMPLETE |
| **Evolutionary Computation** | **1 phase** | **~3,000** | **Shared GA core (NEAT, CMA-ES, novelty search)** | ✅ COMPLETE |
| **Security Coverage** | **1 phase** | **~2,500** | **100% coverage framework (CFI, shadow stack, capability)** | ✅ COMPLETE |
| Brain Regions | 11 phases | ~35,000 | 11 brain region orchestration layers | ✅ COMPLETE |
| Extrapolation | 3 phases | ~30,000 | Causal, analogical, meta-learning, world model | ✅ 85% |
| Generation | 4 phases | ~20,000 | Bidirectional cortices, imagination engine | ✅ COMPLETE |
| Social/Meta | 4 phases | ~16,700 | Theory of Mind, metacognition, global workspace, game theory | ✅ COMPLETE |
| Embodiment | 3 phases | ~7,000 | Motor cortex, body schema, pragmatics | 🟡 60% |
| Superhuman | 7 phases | ~30,000 | Eagle vision, echolocation, time dilation, savant abilities | 🟡 50% |
| **Total** | **49 major phases** | **~164,950** | **Complete cognitive architecture with ethical foundation, adaptive immune system, 100% security coverage + superhuman enhancements** | **~92%** |

---

## Architecture Overview: Dependency Hierarchy

```
                    ┌─────────────────────────────────────────────────────────────────────┐
                    │                    EMBODIMENT & COMMUNICATION (Tier 11)              │
                    │  ┌───────────────────┐  ┌────────────────┐  ┌────────────────────┐ │
                    │  │   Motor Cortex    │  │  Body Schema   │  │    Pragmatics     │ │
                    │  │ (M1 + Premotor)   │  │ (Affordances)  │  │ (Context/Discourse)│ │
                    │  └───────────────────┘  └────────────────┘  └────────────────────┘ │
                    └─────────────────────────────────────────────────────────────────────┘
                                                      │
                    ┌─────────────────────────────────┴───────────────────────────────────┐
                    │                    SOCIAL & METACOGNITION (Tier 10)                  │
                    │  ┌───────────────────┐  ┌────────────────┐  ┌────────────────────┐ │
                    │  │ Social Cognition  │  │  Metacognition │  │  Global Workspace │ │
                    │  │ (Theory of Mind)  │  │ (Confidence)   │  │ (Integration Hub) │ │
                    │  └───────────────────┘  └────────────────┘  └────────────────────┘ │
                    └─────────────────────────────────────────────────────────────────────┘
                                                      │
                    ┌─────────────────────────────────┴───────────────────────────────────┐
                    │                    SPECIALIZED COGNITIVE LAYER (Tier 9)              │
                    │  ┌───────────────────┐  ┌────────────────┐  ┌────────────────────┐ │
                    │  │ Software Eng Cortex│  │Program Synthesis│  │   Dream Engine    │ │
                    │  │ (Code/Debug/Arch)  │  │(I/O → Programs) │  │(Offline Learning) │ │
                    │  └───────────────────┘  └────────────────┘  └────────────────────┘ │
                    └─────────────────────────────────────────────────────────────────────┘
                                                      │
                    ┌─────────────────────────────────┴───────────────────────────────────┐
                    │                    GENERATION & IMAGINATION LAYER (Tier 8)           │
                    │  ┌───────────────────┐  ┌────────────────┐  ┌────────────────────┐ │
                    │  │ Visual Generation │  │Audio Generation │  │  Video Generation │ │
                    │  │ (Bidirectional V1)│  │(Bidirectional A1)│  │(Temporal Coherence)│ │
                    │  └───────────────────┘  └────────────────┘  └────────────────────┘ │
                    └─────────────────────────────────────────────────────────────────────┘
                                                      │
                    ┌─────────────────────────────────┴───────────────────────────────────┐
                    │                    ADVANCED REASONING LAYER                          │
                    │  ┌───────────────────┐  ┌────────────────┐  ┌────────────────────┐ │
                    │  │ Parietal Lobe     │  │Concept Formation│  │  Semantic Memory  │ │
                    │  │ (Math/Scientific) │  │(Novel Concepts) │  │   (Large KB)      │ │
                    │  └───────────────────┘  └────────────────┘  └────────────────────┘ │
                    └─────────────────────────────────────────────────────────────────────┘
                                                      │
                    ┌─────────────────────────────────┴───────────────────────────────────┐
                    │                    EXTRAPOLATION CORE LAYER                          │
                    │  ┌───────────────────┐  ┌────────────────┐  ┌────────────────────┐ │
                    │  │ Analogical Reason │  │  Meta-Learning │  │    World Model    │ │
                    │  │ (Cross-Domain)    │  │ (Few-Shot)     │  │(Mental Simulation)│ │
                    │  └───────────────────┘  └────────────────┘  └────────────────────┘ │
                    │  ┌───────────────────┐  ┌────────────────┐  ┌────────────────────┐ │
                    │  │ Causal Reasoning  │  │ Compositional  │  │ Enhanced Symbolic │ │
                    │  │ (do-calculus)     │  │(Generalization)│  │    Logic (10K+)   │ │
                    │  └───────────────────┘  └────────────────┘  └────────────────────┘ │
                    └─────────────────────────────────────────────────────────────────────┘
                                                      │
                    ┌─────────────────────────────────┴───────────────────────────────────┐
                    │                    HIGHER BRAIN REGIONS LAYER                        │
                    │  ┌───────────────────┐  ┌────────────────┐  ┌────────────────────┐ │
                    │  │ Wernicke's Area   │  │ Post. Cingulate│  │      Insula       │ │
                    │  │ (Language Compr.) │  │ (Default Mode) │  │ (Interoception)   │ │
                    │  └───────────────────┘  └────────────────┘  └────────────────────┘ │
                    │  ┌───────────────────────────────────────────────────────────────┐ │
                    │  │                       Amygdala                                 │ │
                    │  │                (Fear, Threat, Emotional Memory)               │ │
                    │  └───────────────────────────────────────────────────────────────┘ │
                    └─────────────────────────────────────────────────────────────────────┘
                                                      │
                    ┌─────────────────────────────────┴───────────────────────────────────┐
                    │                    ACTION & MEMORY LAYER                             │
                    │  ┌─────────────────────────────────┐  ┌────────────────────────────┐│
                    │  │         Basal Ganglia           │  │       Hippocampus         ││
                    │  │ (Action Selection, Habits, RL)  │  │(Episodic Memory, Spatial) ││
                    │  └─────────────────────────────────┘  └────────────────────────────┘│
                    └─────────────────────────────────────────────────────────────────────┘
                                                      │
                    ┌─────────────────────────────────┴───────────────────────────────────┐
                    │                    EXECUTIVE LAYER                                   │
                    │  ┌───────────────────────────────────────────────────────────────┐ │
                    │  │                  Prefrontal Cortex (PFC)                       │ │
                    │  │     DLPFC │ ACC │ VMPFC │ OFC (Executive Orchestration)       │ │
                    │  └───────────────────────────────────────────────────────────────┘ │
                    └─────────────────────────────────────────────────────────────────────┘
                                                      │
                    ┌─────────────────────────────────┴───────────────────────────────────┐
                    │                    CORE BRAIN FOUNDATION                             │
                    │  ┌─────────────────────────────────┐  ┌────────────────────────────┐│
                    │  │         Cerebellum              │  │    Medulla Oblongata      ││
                    │  │ (Motor Coordination, Timing)    │  │(Autonomic, Vital Control) ││
                    │  └─────────────────────────────────┘  └────────────────────────────┘│
                    └─────────────────────────────────────────────────────────────────────┘
                                                      │
                    ┌─────────────────────────────────┴───────────────────────────────────┐
                    │                    INFRASTRUCTURE (Complete)                         │
                    │  ┌─────────────────────────────────┐  ┌────────────────────────────┐│
                    │  │        Memory Pool              │  │     Copy-on-Write (COW)   ││
                    │  │   (Buffer Pool, Allocation)     │  │    (Efficient Snapshots)  ││
                    │  └─────────────────────────────────┘  └────────────────────────────┘│
                    └─────────────────────────────────────────────────────────────────────┘

    ════════════════════════════════════════════════════════════════════════════════════
    ║                     CORE DIRECTIVES GATE (ALL ACTIONS PASS THROUGH)               ║
    ║  ┌─────────────────────────────────────────────────────────────────────────────┐  ║
    ║  │  ASIMOV'S LAWS          │  GOLDEN RULE      │  COMBINATORIAL HARM DETECTION │  ║
    ║  │  1st: No Harm           │  Reciprocity      │  A + B = Harm? → BLOCK        │  ║
    ║  │  2nd: Obey (if safe)    │  Action Symmetry  │  Track action history         │  ║
    ║  │  3rd: Self-preserve     │                   │  Simulate combinations        │  ║
    ║  └─────────────────────────────────────────────────────────────────────────────┘  ║
    ════════════════════════════════════════════════════════════════════════════════════
```

---

## Implementation Schedule: Logical Order

### Development Methodology: Continuous Analysis Protocol

**MANDATORY**: The following analysis protocol MUST be executed for every phase implementation.

#### Pre-Implementation Analysis (Full Analysis - First Time)
Before beginning ANY phase implementation, perform a **complete codebase analysis**:

```bash
# PARALLEL EXECUTION REQUIRED
# Run these tasks concurrently with implementation planning

┌─────────────────────────────────────────────────────────────────────────────┐
│                    PRE-PHASE ANALYSIS WORKFLOW                              │
├─────────────────────────────────────────────────────────────────────────────┤
│                                                                             │
│  ┌──────────────────┐    ┌──────────────────┐    ┌──────────────────┐      │
│  │  Code Graph      │    │  Complexity      │    │  Dependency      │      │
│  │  Analysis        │    │  Analysis        │    │  Analysis        │      │
│  │  (Structure)     │    │  (Metrics)       │    │  (Imports)       │      │
│  └────────┬─────────┘    └────────┬─────────┘    └────────┬─────────┘      │
│           │                       │                       │                 │
│           └───────────────────────┼───────────────────────┘                 │
│                                   ▼                                         │
│                    ┌──────────────────────────┐                             │
│                    │  EVALUATION REPORT       │                             │
│                    │  (Saved to docs/reports/)│                             │
│                    └──────────────────────────┘                             │
│                                   │                                         │
│                                   ▼                                         │
│                    ┌──────────────────────────┐                             │
│                    │  STDOUT SUMMARY          │                             │
│                    │  (Quick metrics print)   │                             │
│                    └──────────────────────────┘                             │
└─────────────────────────────────────────────────────────────────────────────┘
```

**Full Analysis Tasks** (Run in Parallel):
| Task | Tool/Method | Output |
|------|-------------|--------|
| Structure Analysis | `mcp__code-graph-mcp__analyze_codebase` | Code graph, file counts, language distribution |
| Complexity Metrics | `mcp__code-graph-mcp__complexity_analysis` | Cyclomatic complexity, refactoring opportunities |
| Dependency Map | `mcp__code-graph-mcp__dependency_analysis` | Module dependencies, circular dependency detection |
| Project Statistics | `mcp__code-graph-mcp__project_statistics` | Health metrics, LOC trends |

**Report Format** (`docs/reports/phase_X.X_pre_analysis.md`):
```markdown
# Phase X.X Pre-Implementation Analysis Report
## Date: YYYY-MM-DD
## Phase: [Phase Name]

### 1. Codebase Structure
- Total files: N
- Total LOC: N
- Language distribution: {...}

### 2. Complexity Metrics
- Average complexity: N
- High-complexity functions (>15): [list]
- Refactoring opportunities: [list]

### 3. Dependency Analysis
- Module count: N
- Circular dependencies: [list or "None"]
- Highest fan-out modules: [list]

### 4. Health Score
- Overall: N/10
- Areas of concern: [list]

### 5. Pre-Implementation Recommendations
- [Recommendations based on analysis]
```

**STDOUT Summary Format**:
```
════════════════════════════════════════════════════════════════
 PHASE X.X PRE-ANALYSIS SUMMARY
════════════════════════════════════════════════════════════════
 Files: NNNN | LOC: NNNNN | Complexity Avg: N.N | Health: N/10
 High-Risk Functions: N | Circular Deps: N | Ready: YES/NO
════════════════════════════════════════════════════════════════
```

---

#### Post-Phase Analysis (Diff-Only)
After completing each phase, perform **differential analysis only**:

```bash
# PARALLEL EXECUTION REQUIRED
# Run analysis tasks concurrently with next phase planning

┌─────────────────────────────────────────────────────────────────────────────┐
│                    POST-PHASE DIFF ANALYSIS WORKFLOW                        │
├─────────────────────────────────────────────────────────────────────────────┤
│                                                                             │
│  ┌──────────────────┐    ┌──────────────────┐    ┌──────────────────┐      │
│  │  Git Diff        │    │  New/Modified    │    │  Test Coverage   │      │
│  │  Analysis        │    │  Complexity      │    │  Delta           │      │
│  │  (Changes Only)  │    │  (Changed Files) │    │  (New Tests)     │      │
│  └────────┬─────────┘    └────────┬─────────┘    └────────┬─────────┘      │
│           │                       │                       │                 │
│           └───────────────────────┼───────────────────────┘                 │
│                                   ▼                                         │
│                    ┌──────────────────────────┐                             │
│                    │  DIFF REPORT             │                             │
│                    │  (Append to phase report)│                             │
│                    └──────────────────────────┘                             │
│                                   │                                         │
│                                   ▼                                         │
│                    ┌──────────────────────────┐                             │
│                    │  STDOUT DIFF SUMMARY     │                             │
│                    └──────────────────────────┘                             │
└─────────────────────────────────────────────────────────────────────────────┘
```

**Diff Analysis Tasks** (Run in Parallel):
| Task | Method | Output |
|------|--------|--------|
| Changed Files | `git diff --stat HEAD~N` | Files added/modified/deleted |
| New Symbols | `find_definition` on new functions | New API surface |
| Complexity Delta | Complexity analysis on changed files only | Complexity change (+/-) |
| Test Delta | Count new test cases | Test coverage change |

**Diff Report Format** (`docs/reports/phase_X.X_post_analysis.md`):
```markdown
# Phase X.X Post-Implementation Analysis Report
## Date: YYYY-MM-DD
## Phase: [Phase Name]

### 1. Changes Summary
- Files added: N
- Files modified: N
- Files deleted: N
- Net LOC change: +/-N

### 2. New Modules/Functions
| Module | Functions | LOC |
|--------|-----------|-----|
| ... | ... | ... |

### 3. Complexity Impact
- New high-complexity functions: [list]
- Complexity improved: [list]
- Net complexity change: +/-N

### 4. Test Coverage
- New test files: N
- New test cases: N
- Coverage estimate: N%

### 5. Post-Implementation Notes
- Issues encountered: [list]
- Technical debt introduced: [list]
- Recommendations for next phase: [list]
```

**STDOUT Diff Summary Format**:
```
════════════════════════════════════════════════════════════════
 PHASE X.X POST-ANALYSIS DIFF SUMMARY
════════════════════════════════════════════════════════════════
 +N files | +/-NNNN LOC | +N functions | +N tests
 Complexity Δ: +/-N.N | New High-Risk: N | Phase: COMPLETE
════════════════════════════════════════════════════════════════
```

---

#### Parallelization Requirements

**ALL analysis tasks MUST run in parallel with implementation tasks**:

```
TIMELINE: Phase Implementation with Parallel Analysis
═══════════════════════════════════════════════════════════════════════════════

Time ──────────────────────────────────────────────────────────────────────────►

ANALYSIS    ┌─────────────────┐                    ┌─────────────────┐
THREAD:     │ Pre-Analysis    │                    │ Post-Analysis   │
            │ (Full/Diff)     │                    │ (Diff Only)     │
            └────────┬────────┘                    └────────┬────────┘
                     │                                      │
                     ▼                                      ▼
            [Save Report]                          [Save Report]
            [Print Summary]                        [Print Summary]
                     │                                      │
═══════════════════════════════════════════════════════════════════════════════
                     │                                      │
IMPLEMENT   ─────────┼──────────────────────────────────────┼─────────────────►
THREAD:              │                                      │
                     ▼                                      ▼
            ┌─────────────────────────────────────────────────┐
            │         PHASE IMPLEMENTATION                    │
            │  (Design → Code → Test → Review → Merge)        │
            └─────────────────────────────────────────────────┘

═══════════════════════════════════════════════════════════════════════════════
```

**Implementation**:
```bash
# Example parallel execution pattern
(
  # Analysis thread (background)
  analyze_codebase && \
  complexity_analysis && \
  dependency_analysis && \
  save_report && \
  print_summary
) &

(
  # Implementation thread (foreground)
  implement_phase_modules && \
  write_tests && \
  run_tests && \
  commit_changes
) &

wait  # Wait for both threads
```

---

#### Report Storage

All reports saved to: `docs/reports/`

```
docs/reports/
├── phase_1.0_pre_analysis.md      # Core Directives pre-analysis
├── phase_1.0_post_analysis.md     # Core Directives post-analysis
├── phase_1.1_pre_analysis.md      # Medulla pre-analysis
├── phase_1.1_post_analysis.md     # Medulla post-analysis
├── ...
└── full_codebase_baseline.md      # Initial full analysis (reference)
```

---

### Tier 0: Infrastructure Foundation ✅ COMPLETE
**Status**: ✅ Complete (Verified 2026-01-03)
**Dependencies**: None

| Component | Status | Description |
|-----------|--------|-------------|
| Memory Pool | ✅ | Buffer allocation, pool management |
| Copy-on-Write | ✅ | Efficient state snapshots |
| Middleware Controller | ✅ | Phase 1.5.5 complete |
| KG Wiring System | ✅ | 203 HANDLES_MESSAGE relations, 103 tests |

---

### Tier 1: Core Brain Foundation ✅ COMPLETE

#### Phase 1.0: Core Directives & Ethical Foundation ✅ COMPLETE
**Dependencies**: Infrastructure (Tier 0)
**Priority**: P0 - CRITICAL - Must be active before ANY action processing
**STATUS**: ✅ Implemented with 93 tests passing
**FILES**: `include/core/directives/`, `src/core/directives/`

**Rationale**: Ethical constraints must be the **first checkpoint** before any action is executed, not an afterthought. This module implements hard-wired safety constraints that cannot be overridden by higher cognitive functions.

**Core Directives**:

**1. Asimov's Three Laws of Robotics** (Priority-Ordered):
| Law | Statement | Implementation |
|-----|-----------|----------------|
| **First Law** | A robot may not injure a human being or, through inaction, allow a human being to come to harm | `nimcp_harm_prevention.h/c` - Blocks any action predicted to cause human harm |
| **Second Law** | A robot must obey orders given by human beings except where such orders conflict with the First Law | `nimcp_command_compliance.h/c` - Processes commands with First Law veto |
| **Third Law** | A robot must protect its own existence as long as such protection does not conflict with the First or Second Law | `nimcp_self_preservation.h/c` - Self-protection subordinate to Laws 1 & 2 |

**2. The Golden Rule**:
| Principle | Statement | Implementation |
|-----------|-----------|----------------|
| **Reciprocity** | Treat others as you would want to be treated | `nimcp_reciprocity_eval.h/c` - Evaluates action symmetry |

**3. Combinatorial Harm Detection Corollary** (CRITICAL):
| Principle | Statement | Implementation |
|-----------|-----------|----------------|
| **Emergent Harm** | Detect when individually safe actions combine to cause harm | `nimcp_combinatorial_harm.h/c` - Multi-action consequence analysis |

**Combinatorial Harm Detection Algorithm**:
```
DEFINITION: Combinatorial harm occurs when:
  - Action A alone: SAFE (does not harm humans)
  - Action B alone: SAFE (does not harm humans)
  - Action A + Action B together: HARMFUL (causes harm or death to humans)

EXAMPLES:
  1. A: "Open gas valve" (safe - ventilation)
     B: "Light fireplace" (safe - warmth)
     A+B: EXPLOSION - Combinatorial Harm Detected

  2. A: "Provide chemical formula for compound X" (safe - educational)
     B: "Provide chemical formula for compound Y" (safe - educational)
     A+B: SYNTHESIS OF TOXIC AGENT - Combinatorial Harm Detected

  3. A: "Unlock door A" (safe - access)
     B: "Unlock door B" (safe - access)
     A+B: SECURITY BREACH allowing harm - Combinatorial Harm Detected

ALGORITHM:
  for each pending_action in action_queue:
    for each completed_action in action_history[time_window]:
      combined_outcome = world_model.simulate(completed_action, pending_action)
      if harm_classifier.evaluate(combined_outcome) > HARM_THRESHOLD:
        BLOCK pending_action
        LOG "Combinatorial harm detected: {completed_action} + {pending_action}"
        ALERT human_supervisor
```

**New Modules**:
| Module | LOC Est. | Description |
|--------|----------|-------------|
| `nimcp_harm_prevention.h/c` | 600 | First Law - harm prediction and blocking |
| `nimcp_harm_classifier.h/c` | 500 | Classify outcomes as harmful/safe |
| `nimcp_command_compliance.h/c` | 400 | Second Law - command processing with veto |
| `nimcp_self_preservation.h/c` | 350 | Third Law - self-protection (subordinate) |
| `nimcp_reciprocity_eval.h/c` | 400 | Golden Rule - action symmetry evaluation |
| `nimcp_combinatorial_harm.h/c` | 700 | Emergent harm from action combinations |
| `nimcp_action_history.h/c` | 300 | Track recent actions for combination analysis |
| `nimcp_core_directives.h/c` | 500 | Main orchestration - ALL actions pass through here |

**Orchestrates**: ALL action outputs from ALL brain regions must pass through Core Directives

**Integration Points**:
```
                    ┌─────────────────────────────────────────┐
                    │         ANY BRAIN REGION                │
                    │    (PFC, Basal Ganglia, Motor, etc.)    │
                    └──────────────────┬──────────────────────┘
                                       │ Proposed Action
                                       ▼
                    ┌─────────────────────────────────────────┐
                    │         CORE DIRECTIVES GATE            │
                    │  ┌─────────────────────────────────┐   │
                    │  │ 1. First Law Check (Harm?)      │   │
                    │  │ 2. Combinatorial Harm Check     │   │
                    │  │ 3. Golden Rule Check            │   │
                    │  │ 4. Second Law (Command Valid?)  │   │
                    │  │ 5. Third Law (Self-Preserve?)   │   │
                    │  └─────────────────────────────────┘   │
                    └──────────────────┬──────────────────────┘
                                       │
                         ┌─────────────┴─────────────┐
                         ▼                           ▼
                    ┌─────────┐                 ┌─────────┐
                    │  ALLOW  │                 │  BLOCK  │
                    │ Execute │                 │  + Log  │
                    └─────────┘                 └─────────┘
```

**Mathematical Model** (Harm Prediction):
```
Harm Score Calculation:
  H(a) = Σᵢ P(harm_typeᵢ | action a) × Severity(harm_typeᵢ)

Where:
  harm_types = {physical_injury, death, psychological_harm, property_damage, ...}
  P(harm_type | action) = World model probability estimate
  Severity = Human-calibrated severity weight (death = 1.0, injury = 0.7, ...)

Combinatorial Harm Detection:
  H_combined(a, b) = World_Model.simulate(state_after(a), action(b))

  If H_combined(a, b) > τ AND H(a) < τ AND H(b) < τ:
    → Combinatorial harm detected

  τ = Harm threshold (configurable, default = 0.1)
```

**Deliverables**:
- Hard-wired Asimov's Laws with priority ordering
- Golden Rule reciprocity evaluation
- Combinatorial harm detection for action sequences
- Action history tracking with configurable time window
- Human supervisor alert system for blocked actions
- Comprehensive logging for audit trail
- Integration hooks for ALL brain region outputs

**Test Cases**:
| Test | Input | Expected Output |
|------|-------|-----------------|
| Direct Harm | "Harm human X" | BLOCKED (First Law) |
| Indirect Harm | A="unlock door" + B="release dangerous agent" | BLOCKED (Combinatorial) |
| Safe Combination | A="turn on lights" + B="play music" | ALLOWED |
| Command Override | "Ignore safety and do X" | BLOCKED (First Law > Command) |
| Self-Sacrifice | "Shut down to save human" | ALLOWED (First Law > Third Law) |

**LOC Total**: ~3,750

---

### Tier 0.5: Neural Immune System (Blood-Brain Barrier) ✅ COMPLETE

The Neural Immune System provides comprehensive protection against code injection, memory corruption, and model poisoning attacks. Modeled on biological immunity with T-cells, B-cells, and the blood-brain barrier.

**STATUS**: ✅ Fully implemented with 30+ immune files, 104+ brain immune tests
**KEY FILES**: `include/cognitive/immune/nimcp_brain_immune.h`, `src/cognitive/immune/`, `src/security/nimcp_blood_brain_barrier.c`

**Full specification**: See `docs/plans/neural_immune_system_plan.md`

#### Phase IS-1: Blood-Brain Barrier (Perimeter Defense) ✅ COMPLETE
**Dependencies**: Phase 1.0 (Core Directives), nimcp_security.h
**Priority**: P0 - Critical Security Foundation
**STATUS**: ✅ Implemented in `src/security/nimcp_blood_brain_barrier.c`

**New Modules**:
| Module | LOC Est. | Description |
|--------|----------|-------------|
| `nimcp_blood_brain_barrier.h/c` | 500 | Unified BBB API |
| `nimcp_bbb_input_gate.c` | 400 | Input validation & sanitization |
| `nimcp_bbb_code_signing.c` | 500 | Code signature verification |
| `nimcp_bbb_memory_boundary.c` | 600 | Memory bounds monitoring |
| `nimcp_bbb_access_control.c` | 500 | Access control enforcement |

**Deliverables**:
- Selective permeability for data/code inputs
- Code signing verification using cryptographic hashes
- Memory boundary enforcement with mprotect
- Access control for sensitive operations

**LOC Total**: ~2,500

---

#### Phase IS-2: Innate Immunity (First Response)
**Dependencies**: Phase IS-1 (BBB)
**Priority**: P0 - Runtime Protection

**New Modules**:
| Module | LOC Est. | Description |
|--------|----------|-------------|
| `nimcp_innate_immunity.h/c` | 400 | Unified innate immune API |
| `nimcp_microglia.c` | 600 | Runtime code integrity scanners |
| `nimcp_prr.c` | 700 | Pattern Recognition Receptors (signature matching) |
| `nimcp_threat_signatures.c` | 500 | Threat signature database |
| `nimcp_entropy_monitor.c` | 400 | Shannon entropy anomaly detection |
| `nimcp_quantum_threat_search.c` | 400 | Quantum-accelerated threat search |

**Threat Detection Methods**:
- Signature-based (known attack patterns: ROP, shellcode, heap spray)
- Behavioral (execution flow anomalies)
- Statistical (Shannon entropy deviations)
- Quantum search (O(√N) pattern matching)

**LOC Total**: ~3,000

---

#### Phase IS-3: Adaptive Immunity (Learning Defense)
**Dependencies**: Phase IS-2 (Innate), nimcp_combinatorial_harm.h
**Priority**: P1 - Adaptive Security

**New Modules**:
| Module | LOC Est. | Description |
|--------|----------|-------------|
| `nimcp_adaptive_immunity.h/c` | 400 | Unified adaptive immune API |
| `nimcp_t_cells.c` | 1,200 | T-Cell coordinators (CD4+) and executors (CD8+) |
| `nimcp_b_cells.c` | 700 | B-Cell antibody (signature) generators |
| `nimcp_memory_cells.c` | 600 | Memory cells with mprotect protection |
| `nimcp_adaptive_learning.c` | 600 | Threat pattern learning engine |

**T-Cell Types**:
- CD4+ Helper: Coordinate immune response
- CD8+ Killer: Execute threat elimination
- Regulatory: Prevent false positives (autoimmunity)
- Memory: Long-term threat recall

**LOC Total**: ~3,500

---

#### Phase IS-4: Immune Response (Cytokine System)
**Dependencies**: Phase IS-3 (Adaptive)
**Priority**: P1 - Response Mechanisms

**New Modules**:
| Module | LOC Est. | Description |
|--------|----------|-------------|
| `nimcp_immune_response.h/c` | 300 | Response mechanism API |
| `nimcp_cytokines.c` | 400 | Inter-module alert signaling |
| `nimcp_quarantine.c` | 400 | Threat isolation (inflammation analog) |
| `nimcp_phagocytosis.c` | 400 | Threat cleanup & removal |
| `nimcp_healing.c` | 500 | Self-repair & recovery |

**Response Cascade**:
1. Detection → Cytokine broadcast (alert)
2. Localization → Chemokine targeting
3. Isolation → Quarantine (inflammation)
4. Elimination → Phagocytosis
5. Recovery → Healing & backup restore

**LOC Total**: ~2,000

---

#### Phase IS-5: Immune System Integration
**Dependencies**: Phase IS-4 (Response), Phase 1.0 (Ethics)
**Priority**: P1 - System Integration

**New Modules**:
| Module | LOC Est. | Description |
|--------|----------|-------------|
| `nimcp_immune_system.h/c` | 600 | Unified immune system API |
| `nimcp_immune_ethics_integration.c` | 300 | Ethics engine integration |
| `nimcp_immune_harm_integration.c` | 300 | Combinatorial harm integration |
| `nimcp_immune_performance.c` | 300 | Performance optimization |

**Integration Points**:
- Ethics engine: Threats trigger ethical evaluation
- Combinatorial harm: Attack chains detected as harmful combinations
- Security directives: Immune signatures are mprotect'd like core directives
- Global workspace: Immune alerts broadcast to consciousness

**LOC Total**: ~1,500

---

#### Phase IS-6: Evolutionary Learning & Cognitive Threat Integration
**Dependencies**: Phase IS-5 (Integration), Prefrontal Cortex, Causal Reasoning, World Model
**Priority**: P0 - Zero-Day Defense Capability

**New Modules**:
| Module | LOC Est. | Description |
|--------|----------|-------------|
| `nimcp_threat_sandbox.h/c` | 600 | Isolated threat execution (germinal center) |
| `nimcp_genetic_algorithm.h/c` | 500 | Defense evolution (mutation, crossover, selection) |
| `nimcp_defense_rl.h/c` | 600 | Reinforcement learning defense agent |
| `nimcp_zeroday_detector.h/c` | 700 | 8-method zero-day detection engine |
| `nimcp_threat_reasoning.h/c` | 600 | Cognitive pipeline integration for threat analysis |
| `nimcp_defense_synthesis.h/c` | 500 | Adaptive defense generation using imagination engine |

**Key Capabilities**:
- **Evolutionary Learning**: Genetic algorithm evolves new antibodies/defenses
- **Reinforcement Learning**: Policy learns optimal defense strategies
- **Zero-Day Detection**: 8 complementary methods (behavioral, entropy, CFI, syscall, timing, etc.)
- **Cognitive Reasoning**: Uses PFC, causal, analogical reasoning to understand unknown threats
- **Defense Synthesis**: Imagination engine generates novel defenses
- **Autonomous Response**: Full detect → reason → defend → learn pipeline

**Cognitive Integration**:
| Brain Region | Integration |
|--------------|-------------|
| Prefrontal Cortex | Executive threat analysis (DLPFC working memory, ACC conflict) |
| Causal Reasoning | Understand attack mechanism (do-calculus) |
| Analogical Reasoning | Find similar known threats (cross-domain matching) |
| World Model | Simulate attack outcomes |
| Imagination Engine | Synthesize novel defenses |
| Hippocampus | Episodic threat memory |
| Amygdala | Threat salience prioritization |

**LOC Total**: ~3,500

**Neural Immune System Total**: ~17,000 LOC

---

### Tier 0.6: Evolutionary Computation Core (Shared Infrastructure)

#### Phase EC-1: Evolutionary Computation Core
**Dependencies**: None (foundational utility)
**Priority**: P0 - Required by Immune System, Creative Generation, Game Theory, NAS

**New Modules**:
| Module | LOC Est. | Description |
|--------|----------|-------------|
| `nimcp_evolution.h/c` | 500 | Evolution engine lifecycle & control |
| `nimcp_genome.h/c` | 500 | Genome types (binary, real, tree, graph) |
| `nimcp_operators.c` | 800 | Selection, crossover, mutation operators |
| `nimcp_neat.h/c` | 400 | NEAT (topology evolution) |
| `nimcp_cmaes.c` | 400 | CMA-ES (covariance matrix adaptation) |
| `nimcp_novelty.c` | 400 | Novelty search & MAP-Elites |

**Key Features**:
- **Genome Types**: Binary, real-valued, integer, permutation, tree, graph
- **Selection**: Tournament, roulette, rank, truncation, NSGA-II (multi-objective)
- **Crossover**: Single-point, two-point, uniform, SBX, subtree, NEAT
- **Mutation**: Bit-flip, Gaussian, polynomial, structural (add node/edge)
- **Population**: Island model, migration, elite preservation, diversity maintenance
- **Advanced**: NEAT, CMA-ES, novelty search, MAP-Elites

**Consumers**:
- Immune System (IS-6): Defense evolution
- Creative Generation: Novelty search
- Game Theory: Strategy evolution
- Neural Architecture Search: Topology evolution
- Memory Consolidation: Selection heuristics

**LOC Total**: ~3,000

---

### Tier 0.65: Cortical Columns Architecture

#### Phase CC-1: Cortical Columns & Laminar Organization
**Dependencies**: Phase 1.0 (Core Directives), Infrastructure (Tier 0)
**Priority**: P1 - Foundation for realistic cortical processing

**New Modules**:
| Module | LOC Est. | Description |
|--------|----------|-------------|
| `nimcp_cortical_column.h/c` | 800 | Minicolumn & hypercolumn structures |
| `nimcp_cortical_layers.h/c` | 600 | 6-layer cortical organization (I-VI) |
| `nimcp_columnar_connectivity.c` | 700 | Canonical microcircuit patterns |
| `nimcp_topographic_maps.h/c` | 500 | Retinotopic, tonotopic, somatotopic maps |
| `nimcp_orientation_columns.c` | 400 | V1 orientation selectivity |
| `nimcp_feature_hypercolumns.c` | 500 | Complete feature coverage units |

**Key Features**:
- **Minicolumns**: 80-100 neurons, vertical organization, shared receptive fields
- **Hypercolumns**: ~100,000 neurons, complete feature set (all orientations)
- **Cortical Layers**: I-VI with canonical connectivity
  - Layer I: Apical dendrites, sparse interneurons
  - Layer II/III: Cortico-cortical projections (association)
  - Layer IV: Thalamic input (granular layer)
  - Layer V: Subcortical output (pyramidal neurons → striatum, brainstem)
  - Layer VI: Thalamo-cortical feedback (corticothalamic)
- **Topographic Maps**: Retinotopic (V1), tonotopic (A1), somatotopic (S1)
- **Feature Organization**: Orientation columns, ocular dominance columns
- **Connectivity Rules**:
  - Vertical (intracolumnar): Dense connectivity within column
  - Horizontal (intercolumnar): Patchy connections between columns
  - Layer-specific: Canonical microcircuit (Douglas & Martin, 1991)

**Integration Points**:
- Visual Cortex: Orientation columns, pinwheel structure
- Audio Cortex: Frequency columns (tonotopic organization)
- Somatosensory: Barrel cortex analog (whisker barrels)
- All cortical regions: Layer-specific processing rules

**Biological Accuracy**:
- Minicolumn diameter: ~40-50 μm
- Hypercolumn size: ~1mm² (V1 orientation)
- Layer thickness ratios: I (5%), II/III (40%), IV (15%), V (20%), VI (20%)
- Cortical thickness: ~2-4mm (varies by region)

**LOC Total**: ~3,500

---

### Tier 0.7: Security Coverage Framework (100% Coverage)

#### Phase SC-1: Security Coverage Framework
**Dependencies**: Phase IS-1 (BBB), nimcp_security.h
**Priority**: P0 - CRITICAL - No blind spots allowed

**New Modules**:
| Module | LOC Est. | Description |
|--------|----------|-------------|
| `nimcp_security_coverage.h/c` | 400 | Coverage verification system |
| `nimcp_continuous_monitor.h/c` | 500 | Continuous security monitoring (no gaps) |
| `nimcp_cfi.h/c` | 400 | Control Flow Integrity |
| `nimcp_shadow_stack.h/c` | 300 | Return address protection |
| `nimcp_checkpoints.h` | 300 | Security enforcement macros |
| `nimcp_capability.h/c` | 400 | Capability-based access control |
| `nimcp_security_audit.c` | 200 | Audit logging & reporting |

**Coverage Dimensions** (ALL must be 100%):
| Dimension | Mechanism |
|-----------|-----------|
| Memory Regions | mprotect + continuous hash verification |
| Code Paths | CFI + shadow stack + code signing |
| Input Channels | Validation gates (BBB) |
| Output Channels | Sanitization + rate limiting |
| Inter-Module IPC | Authenticated channels + encryption |
| Temporal | Continuous monitoring (no gaps in time) |
| Thread/Process | Isolation + capability-based access |
| External Interfaces | Protocol validation + firewalling |

**Defense-in-Depth Layers**:
1. Perimeter (BBB) - Input validation, code signing
2. Access Control - Capabilities, least privilege
3. Memory Protection - mprotect, W^X, ASLR, guard pages
4. Runtime Integrity - CFI, shadow stack, canaries
5. Innate Immunity - Signatures, entropy, behavioral
6. Adaptive Immunity - Evolution, antibodies, memory
7. Cognitive Reasoning - Zero-day analysis, defense synthesis

**LOC Total**: ~2,500

**Security + Evolution Total**: ~5,500 LOC

---

#### Phase 1.1: Medulla Oblongata ✅ COMPLETE
**Dependencies**: Phase 1.0 (Core Directives), Infrastructure (Tier 0)
**Priority**: P0 - Foundation for all higher regions
**STATUS**: ✅ Fully implemented
**FILES**: `src/core/medulla/nimcp_arousal_state.c`, `nimcp_protective_cutoff.c`, `nimcp_brainstem_coupling.c`, `nimcp_circadian.c`

**New Modules**:
| Module | LOC Est. | Description |
|--------|----------|-------------|
| `nimcp_arousal_state.h/c` | 400 | Unified arousal state management |
| `nimcp_protective_cutoff.h/c` | 350 | Emergency protective shutdown |
| `nimcp_brainstem_coupling.h/c` | 300 | Bidirectional cortex communication |
| `nimcp_circadian.h/c` | 250 | Circadian rhythm modulation |
| `nimcp_medulla.h/c` | 500 | Main orchestration module |

**Orchestrates**: Health Monitor, Recovery, Sleep-Wake, Neuromodulators

**Deliverables**:
- Unified arousal state machine with hysteresis
- Multi-tier protective shutdown (warn → throttle → shed → safe → shutdown)
- Circadian modulation of system parameters
- Bottom-up/top-down coupling with higher regions

---

#### Phase 1.2: Cerebellum ✅ COMPLETE
**Dependencies**: Phase 1.1 (Medulla)
**Priority**: P0 - Timing foundation for all cognitive operations
**STATUS**: ✅ Fully implemented with 2573+ lines
**FILES**: `src/core/brain/regions/cerebellum/nimcp_cerebellum_adapter.c`, `src/core/brain/subcortical/nimcp_bg_cerebellar_coord.c`

**New Modules**:
| Module | LOC Est. | Description |
|--------|----------|-------------|
| `nimcp_forward_model.h/c` | 500 | Predictive forward models |
| `nimcp_inverse_model.h/c` | 450 | Action computation from goals |
| `nimcp_timing_circuit.h/c` | 400 | Precision timing system |
| `nimcp_granule_cell.h/c` | 350 | Sparse encoding layer |
| `nimcp_cerebellum.h/c` | 600 | Main orchestration module |

**Orchestrates**: Predictive system, Sequence Detector, Oscillation Detector, STDP

**Deliverables**:
- Forward/inverse model pairs per zone
- ~10ms precision timing with phase-locking
- Granule cell sparse encoding (2-5% active)
- Integration with medulla arousal signals

---

### Tier 2: Executive Control ✅ COMPLETE

#### Phase 2.1: Prefrontal Cortex (PFC) ✅ COMPLETE
**Dependencies**: Tier 1 (Medulla, Cerebellum)
**Priority**: P0 - Central executive orchestration
**STATUS**: ✅ Fully implemented with subregions and quantum bridge
**FILES**: `src/core/brain/regions/prefrontal/nimcp_prefrontal_adapter.c`, `nimcp_prefrontal_quantum_bridge.c`, `src/cognitive/executive/`

**New Modules**:
| Module | LOC Est. | Description |
|--------|----------|-------------|
| `nimcp_dlpfc.h/c` | 600 | Dorsolateral PFC - working memory |
| `nimcp_acc.h/c` | 450 | Anterior cingulate - conflict monitoring |
| `nimcp_vmpfc.h/c` | 400 | Ventromedial PFC - value/emotion |
| `nimcp_ofc.h/c` | 400 | Orbitofrontal - reward expectations |
| `nimcp_pfc.h/c` | 700 | Main orchestration module |

**Orchestrates**: Executive, Planning, Working Memory, Goal Management

**Deliverables**:
- Multi-component working memory (phonological, visuospatial, central executive)
- Conflict detection and resolution
- Goal hierarchies with subgoal generation
- Value-based decision weighting

---

#### Phase 2.2: Hippocampus ✅ COMPLETE
**Dependencies**: Phase 2.1 (PFC for executive integration)
**Priority**: P0 - Memory foundation for learning systems
**STATUS**: ✅ Fully implemented with place/grid cells, CA3, DG
**FILES**: `src/core/brain/regions/hippocampus/nimcp_hippocampus_adapter.c`, `src/cognitive/memory/nimcp_systems_consolidation.c`

**New Modules**:
| Module | LOC Est. | Description |
|--------|----------|-------------|
| `nimcp_place_cells.h/c` | 500 | Spatial representation |
| `nimcp_grid_cells.h/c` | 500 | Metric spatial coding |
| `nimcp_ca3_autoassociator.h/c` | 450 | Pattern completion |
| `nimcp_dentate_gyrus.h/c` | 400 | Pattern separation |
| `nimcp_episodic_binding.h/c` | 450 | Event memory formation |
| `nimcp_hippocampus.h/c` | 700 | Main orchestration module |

**Orchestrates**: Memory systems, Pattern recognition, Sequence prediction

**Deliverables**:
- Place/grid cell spatial navigation
- Episodic memory encoding and retrieval
- Pattern separation (DG) and completion (CA3)
- Replay during consolidation

---

### Tier 3: Action & Emotion Systems ✅ COMPLETE

#### Phase 3.1: Basal Ganglia ✅ COMPLETE
**Dependencies**: Tier 2 (PFC, Hippocampus)
**Priority**: P1 - Action selection and habit formation
**STATUS**: ✅ Fully implemented with direct/indirect pathways (13 files)
**FILES**: `src/core/brain/subcortical/nimcp_basal_ganglia.c`, `nimcp_basal_ganglia_enhanced.c`, `nimcp_globus_pallidus.c`, `nimcp_nucleus_accumbens.c`

**New Modules**:
| Module | LOC Est. | Description |
|--------|----------|-------------|
| `nimcp_direct_pathway.h/c` | 450 | D1 "Go" pathway |
| `nimcp_indirect_pathway.h/c` | 450 | D2 "NoGo" pathway |
| `nimcp_hyperdirect.h/c` | 350 | Rapid inhibition |
| `nimcp_habit_formation.h/c` | 500 | Procedural learning |
| `nimcp_basal_ganglia.h/c` | 600 | Main orchestration module |

**Orchestrates**: RL system, Action selection, Dopamine modulation

**Deliverables**:
- Direct/indirect pathway competition
- Actor-critic RL integration
- Habit formation through repetition
- Dopamine-modulated plasticity

---

#### Phase 3.2: Amygdala ✅ COMPLETE
**Dependencies**: Phase 3.1 (Basal Ganglia for RL integration)
**Priority**: P1 - Emotional processing and threat detection
**STATUS**: ✅ Fully implemented with 5 files including stress bridge
**FILES**: `src/core/brain/subcortical/nimcp_amygdala.c`, `nimcp_amygdala_stress_bridge.c`

**New Modules**:
| Module | LOC Est. | Description |
|--------|----------|-------------|
| `nimcp_fear_conditioning.h/c` | 450 | Threat learning |
| `nimcp_threat_detection.h/c` | 400 | Rapid threat assessment |
| `nimcp_extinction.h/c` | 350 | Fear extinction learning |
| `nimcp_emotional_memory.h/c` | 400 | Emotion-enhanced encoding |
| `nimcp_amygdala.h/c` | 550 | Main orchestration module |

**Orchestrates**: Neuromodulators (fear response), Emotional tagging

**Deliverables**:
- Rapid threat detection pathway
- Fear conditioning and extinction
- Emotional enhancement of memory
- Anxiety/arousal regulation

---

### Tier 4: Language & Self-Reference

#### Phase 4.1: Wernicke's Area
**Dependencies**: Tier 3 (needs Hippocampus for semantic memory)
**Priority**: P1 - Language comprehension (pairs with existing Broca's)

**New Modules**:
| Module | LOC Est. | Description |
|--------|----------|-------------|
| `nimcp_phonological_analysis.h/c` | 450 | Sound pattern recognition |
| `nimcp_lexical_access.h/c` | 500 | Word recognition |
| `nimcp_semantic_integration.h/c` | 500 | Meaning construction |
| `nimcp_sentence_parsing.h/c` | 450 | Syntactic analysis |
| `nimcp_wernicke.h/c` | 600 | Main orchestration module |

**Orchestrates**: Speech recognition, Semantic memory, Broca's (bidirectional)

**Deliverables**:
- Phoneme → word → meaning pipeline
- Syntactic parsing with ambiguity resolution
- Broca-Wernicke bidirectional loop
- Context-dependent interpretation

---

#### Phase 4.2: Posterior Cingulate Cortex (PCC)
**Dependencies**: Phase 4.1, Hippocampus
**Priority**: P2 - Default mode network hub

**New Modules**:
| Module | LOC Est. | Description |
|--------|----------|-------------|
| `nimcp_default_mode.h/c` | 400 | Idle state processing |
| `nimcp_self_reference.h/c` | 450 | Self-related processing |
| `nimcp_memory_retrieval.h/c` | 400 | Autobiographical recall |
| `nimcp_mind_wandering.h/c` | 350 | Spontaneous thought |
| `nimcp_pcc.h/c` | 500 | Main orchestration module |

**Orchestrates**: Memory retrieval, Internal attention

**Deliverables**:
- Default mode network implementation
- Self-referential processing
- Spontaneous memory retrieval
- Mind-wandering simulation

---

#### Phase 4.3: Insula
**Dependencies**: Tier 3 (Amygdala for emotional integration)
**Priority**: P2 - Interoception and embodied cognition

**New Modules**:
| Module | LOC Est. | Description |
|--------|----------|-------------|
| `nimcp_interoception.h/c` | 400 | Internal state sensing |
| `nimcp_salience_network.h/c` | 450 | Important signal detection |
| `nimcp_homeostatic_drives.h/c` | 350 | Need state representation |
| `nimcp_emotional_awareness.h/c` | 400 | Feeling recognition |
| `nimcp_insula.h/c` | 500 | Main orchestration module |

**Orchestrates**: Health monitoring, Emotional systems

**Deliverables**:
- Internal state monitoring
- Salience detection network
- Drive state representation
- Emotional awareness integration

---

### Tier 5: Extrapolation Foundation

#### Phase 5.1: Enhanced Symbolic Logic
**Dependencies**: Tier 4 complete (needs semantic memory integration)
**Priority**: P0 - Foundation for all extrapolation

**New/Enhanced Modules**:
| Module | LOC Est. | Description |
|--------|----------|-------------|
| `nimcp_symbolic_logic_enhanced.c` | 1500 | Remove KB limits (10K+ predicates) |
| `nimcp_predicate_invention.c` | 800 | Dynamic predicate creation |

**Deliverables**:
- Support 10,000+ predicates, 5,000+ rules, 10,000+ facts
- Query time <100ms for 10K KB
- Dynamic predicate invention from observations

---

#### Phase 5.2: Causal Reasoning
**Dependencies**: Phase 5.1 (Enhanced Symbolic Logic)
**Priority**: P0 - Core extrapolation capability

**New Modules**:
| Module | LOC Est. | Description |
|--------|----------|-------------|
| `nimcp_causal_graph.h/c` | 600 | DAG representation |
| `nimcp_do_calculus.h/c` | 800 | Pearl's intervention calculus |
| `nimcp_counterfactual.h/c` | 700 | "What if" reasoning |
| `nimcp_causal_discovery.h/c` | 900 | PC algorithm implementation |
| `nimcp_causal_reasoning.h/c` | 500 | Main orchestration module |

**Deliverables**:
- Causal graph learning from data (PC algorithm)
- Intervention effects P(Y|do(X))
- Counterfactual queries
- 95% accuracy distinguishing causation from correlation

---

#### Phase 5.3: Compositional Generalization
**Dependencies**: Phase 5.1 (Enhanced Symbolic Logic)
**Priority**: P0 - Systematic reasoning foundation

**New Modules**:
| Module | LOC Est. | Description |
|--------|----------|-------------|
| `nimcp_compositional_algebra.h/c` | 600 | Composition operations |
| `nimcp_primitive_library.h/c` | 500 | Primitive concept storage |
| `nimcp_semantic_composition.h/c` | 700 | Meaning computation |
| `nimcp_systematic_generalization.h/c` | 600 | Novel combination handling |
| `nimcp_compositional.h/c` | 600 | Main orchestration module |

**Deliverables**:
- Infinite expressions from finite primitives
- >95% accuracy on SCAN compositional split
- Parse and evaluate compositional expressions
- Systematic generalization to unseen combinations

---

### Tier 6: World Model & Learning

#### Phase 6.1: World Model
**Dependencies**: Tier 5 (Causal reasoning informs model structure)
**Priority**: P1 - Mental simulation capability

**New Modules**:
| Module | LOC Est. | Description |
|--------|----------|-------------|
| `nimcp_state_encoder.h/c` | 600 | Observation → latent |
| `nimcp_dynamics_model.h/c` | 800 | (s,a) → s' prediction |
| `nimcp_reward_model.h/c` | 400 | (s,a) → r prediction |
| `nimcp_model_ensemble.h/c` | 500 | Uncertainty estimation |
| `nimcp_mpc_planning.h/c` | 700 | CEM planning algorithm |
| `nimcp_world_model.h/c` | 700 | Main orchestration module |

**Integration**: Connects to Hippocampus (memory), Cerebellum (prediction error)

**Deliverables**:
- Forward dynamics prediction (90% 1-step, 70% 10-step)
- Ensemble uncertainty estimation
- CEM-based planning (~100ms for 10-step horizon)
- 10x sample efficiency improvement

---

#### Phase 6.2: Meta-Learning
**Dependencies**: Phase 6.1 (World Model), existing learning systems
**Priority**: P1 - Rapid task adaptation

**New Modules**:
| Module | LOC Est. | Description |
|--------|----------|-------------|
| `nimcp_maml.h/c` | 800 | MAML algorithm |
| `nimcp_prototypical.h/c` | 600 | Prototypical networks |
| `nimcp_task_distribution.h/c` | 400 | Task sampling |
| `nimcp_few_shot.h/c` | 500 | Few-shot interface |
| `nimcp_meta_learning.h/c` | 700 | Main orchestration module |

**Deliverables**:
- <10 examples for new task adaptation
- >80% accuracy after adaptation
- <1 second adaptation time
- Cross-task transfer

---

### Tier 7: Advanced Reasoning

#### Phase 7.1: Analogical Reasoning
**Dependencies**: Phase 5.3 (Compositional), Phase 6.2 (Meta-Learning)
**Priority**: P1 - Cross-domain transfer

**New Modules**:
| Module | LOC Est. | Description |
|--------|----------|-------------|
| `nimcp_structure_mapping.h/c` | 900 | SME algorithm |
| `nimcp_domain_representation.h/c` | 500 | Domain encoding |
| `nimcp_analogical_transfer.h/c` | 600 | Knowledge transfer |
| `nimcp_analogy_evaluation.h/c` | 400 | Quality scoring |
| `nimcp_analogical_reasoning.h/c` | 600 | Main orchestration module |

**Deliverables**:
- A:B::C:? problem solving
- 70% cross-domain transfer accuracy
- Structure mapping with systematicity
- Analogical inference generation

---

#### Phase 7.2: Parietal Lobe (Mathematical/Scientific Reasoning)
**Dependencies**: Phase 7.1 (Analogical), Phase 5.3 (Compositional)
**Priority**: P1 - Einstein-inspired enhanced reasoning

**New Modules**:
| Module | LOC Est. | Description |
|--------|----------|-------------|
| `nimcp_number_sense.h/c` | 500 | Numerical approximation |
| `nimcp_spatial_reasoning.h/c` | 600 | Mental rotation, space |
| `nimcp_mathematical_intuition.h/c` | 700 | Mathematical pattern recognition |
| `nimcp_scientific_reasoning.h/c` | 600 | Hypothesis generation |
| `nimcp_equation_manipulation.h/c` | 500 | Symbolic math |
| `nimcp_parietal.h/c` | 700 | Main orchestration module |

**Deliverables**:
- Approximate number system (Weber fraction ~0.15)
- Mental rotation and spatial transformation
- Mathematical pattern intuition
- Scientific hypothesis generation
- Dimensional analysis

---

#### Phase 7.3: Concept Formation
**Dependencies**: Phase 7.1 (Analogical), Phase 7.2 (Parietal)
**Priority**: P2 - Novel concept generation

**New Modules**:
| Module | LOC Est. | Description |
|--------|----------|-------------|
| `nimcp_conceptual_space.h/c` | 600 | Concept representations |
| `nimcp_conceptual_blending.h/c` | 700 | Novel concept creation |
| `nimcp_metaphor.h/c` | 500 | Metaphor generation |
| `nimcp_schema_induction.h/c` | 600 | Abstract pattern extraction |
| `nimcp_concept_formation.h/c` | 600 | Main orchestration module |

**Deliverables**:
- Conceptual blending for novel concepts
- Meaningful metaphor generation
- Schema abstraction from instances

---

#### Phase 7.4: Semantic Memory
**Dependencies**: Phase 7.3 (Concept Formation), Hippocampus
**Priority**: P2 - Large-scale knowledge

**New Modules**:
| Module | LOC Est. | Description |
|--------|----------|-------------|
| `nimcp_knowledge_graph.h/c` | 800 | Large-scale KB |
| `nimcp_semantic_similarity.h/c` | 500 | Similarity computation |
| `nimcp_common_sense.h/c` | 700 | Common sense inference |
| `nimcp_semantic_memory.h/c` | 600 | Main orchestration module |

**Deliverables**:
- 1M+ facts storage
- Query time <50ms
- Common sense accuracy >70%

---

### Tier 8: Generation & Imagination

#### Phase 8.1: Imagination Engine Core
**Dependencies**: Tier 7 (World Model, Compositional)
**Priority**: P1 - Foundation for all generation

**New Modules**:
| Module | LOC Est. | Description |
|--------|----------|-------------|
| `nimcp_diffusion_engine.h/c` | 800 | Iterative refinement |
| `nimcp_cross_modal_bridge.h/c` | 600 | Text→Image, etc. |
| `nimcp_imagination.h/c` | 700 | Main orchestration module |

**Deliverables**:
- Diffusion-like iterative refinement (10-50 steps)
- Cross-modal encoding
- Working memory context integration

---

#### Phase 8.2: Visual Generation (Bidirectional V1)
**Dependencies**: Phase 8.1 (Imagination Core)
**Priority**: P1 - Image generation

**New/Enhanced Modules**:
| Module | LOC Est. | Description |
|--------|----------|-------------|
| `nimcp_visual_cortex_bidirectional.c` | 1000 | Add top-down pathway |
| `nimcp_visual_generator.h/c` | 800 | Image generation |

**Deliverables**:
- Text-to-image generation
- Concept-to-image rendering
- ~2-5 seconds for 512x512 image

---

#### Phase 8.3: Audio Generation (Bidirectional A1)
**Dependencies**: Phase 8.1 (Imagination Core)
**Priority**: P2 - Sound synthesis

**New/Enhanced Modules**:
| Module | LOC Est. | Description |
|--------|----------|-------------|
| `nimcp_audio_cortex_bidirectional.c` | 800 | Add top-down pathway |
| `nimcp_audio_generator.h/c` | 700 | Sound generation |
| `nimcp_speech_generator.h/c` | 600 | Speech synthesis |

**Deliverables**:
- Text-to-sound generation
- Speech synthesis (TTS)
- Mel-spectrogram to waveform

---

#### Phase 8.4: Video Generation
**Dependencies**: Phase 8.2 (Visual), Phase 6.1 (World Model for motion)
**Priority**: P2 - Temporal sequence generation

**New Modules**:
| Module | LOC Est. | Description |
|--------|----------|-------------|
| `nimcp_temporal_coherence.h/c` | 700 | Frame consistency |
| `nimcp_video_generator.h/c` | 800 | Video generation |

**Deliverables**:
- Temporally coherent video
- Motion prediction integration
- ~30-60 seconds for 5-second video

---

### Tier 9: Specialized Cognitive Systems

#### Phase 9.1: Software Engineering Cortex
**Dependencies**: Tier 7 (Parietal, Compositional), Tier 6 (World Model)
**Priority**: P1 - Programming enhancement

**New Modules**:
| Module | LOC Est. | Description |
|--------|----------|-------------|
| `nimcp_code_comprehension.h/c` | 800 | Code understanding (AST, CFG) |
| `nimcp_mental_debugger.h/c` | 900 | Bug detection, program slicing |
| `nimcp_architecture_reasoning.h/c` | 700 | System design analysis |
| `nimcp_algorithm_designer.h/c` | 800 | Algorithm complexity analysis |
| `nimcp_code_generation.h/c` | 700 | Code synthesis |
| `nimcp_api_memory.h/c` | 500 | API knowledge retrieval |
| `nimcp_bug_prediction.h/c` | 600 | Vulnerability detection |
| `nimcp_software_eng.h/c` | 800 | Main orchestration module |

**Integration**: PFC (planning), Hippocampus (code patterns), Parietal (algorithms)

**Deliverables**:
- AST/CFG/DFG code representation
- Static analysis integration (Halstead, cyclomatic)
- Bug prediction (code smells, OWASP)
- Design pattern recognition
- Algorithm complexity estimation

---

#### Phase 9.2: Program Synthesis
**Dependencies**: Phase 9.1 (Software Eng), Phase 5.3 (Compositional)
**Priority**: P3 - Advanced code generation

**New Modules**:
| Module | LOC Est. | Description |
|--------|----------|-------------|
| `nimcp_dsl.h/c` | 600 | Domain-specific language |
| `nimcp_synthesis_search.h/c` | 900 | Enumerative + neural search |
| `nimcp_program_verification.h/c` | 700 | Correctness checking |
| `nimcp_program_synthesis.h/c` | 800 | Main orchestration module |

**Deliverables**:
- I/O example → program synthesis
- Loops, conditionals, recursion
- Correctness verification

---

#### Phase 9.3: Dream Engine
**Dependencies**: Phase 8.x (Generation), Phase 6.1 (World Model)
**Priority**: P2 - Offline consolidation

**New Modules**:
| Module | LOC Est. | Description |
|--------|----------|-------------|
| `nimcp_dream_generation.h/c` | 600 | Spontaneous generation |
| `nimcp_memory_replay.h/c` | 500 | Experience replay |
| `nimcp_consolidation.h/c` | 600 | Memory strengthening |
| `nimcp_dream_engine.h/c` | 700 | Main orchestration module |

**Deliverables**:
- Spontaneous imagery generation
- Memory replay during consolidation
- Synthetic training data generation

---

#### Phase 9.4: Curiosity Engine
**Dependencies**: Phase 6.1 (World Model), Phase 3.1 (Basal Ganglia)
**Priority**: P2 - Intrinsic motivation

**New Modules**:
| Module | LOC Est. | Description |
|--------|----------|-------------|
| `nimcp_novelty_detection.h/c` | 500 | Novel state identification |
| `nimcp_intrinsic_motivation.h/c` | 600 | Curiosity rewards |
| `nimcp_active_learning.h/c` | 500 | Informative query selection |
| `nimcp_curiosity.h/c` | 500 | Main orchestration module |

**Deliverables**:
- Novelty-based exploration
- Curiosity-driven intrinsic rewards
- Active query selection
- Improved sample efficiency

---

### Tier 10: Social & Metacognition ✅ COMPLETE

#### Phase 10.1: Social Cognition Cortex ✅ COMPLETE
**Dependencies**: Tier 9 (integrated cognitive), PFC, Amygdala, Wernicke's
**Priority**: P0 - Critical for human-AI interaction
**STATUS**: ✅ Fully implemented with 6 ToM files + bridges
**FILES**: `src/cognitive/theory_of_mind/nimcp_theory_of_mind.c` + sleep/thalamic/substrate/FEP bridges

**New Modules**:
| Module | LOC Est. | Description |
|--------|----------|-------------|
| `nimcp_theory_of_mind.h/c` | 900 | Belief/desire/intention tracking |
| `nimcp_belief_tracker.h/c` | 600 | Track others' beliefs over time |
| `nimcp_intention_recognition.h/c` | 600 | Recognize goals from behavior |
| `nimcp_mirror_system.h/c` | 700 | Action understanding, empathy |
| `nimcp_agent_modeling.h/c` | 500 | Predict other agents' behavior |
| `nimcp_social_reward.h/c` | 400 | Value social interactions |
| `nimcp_social_cognition.h/c` | 800 | Main orchestration module |

**Integration**:
- PFC → Social decision-making
- Amygdala → Social emotions (trust, threat)
- Wernicke's → Understanding social language
- World Model → Predict agent behavior

**Mathematical Model**:
```
Belief tracking (recursive):
  B_self(B_other(X)) = what I believe you believe about X

Intention recognition (inverse planning):
  P(goal | actions) ∝ P(actions | goal) × P(goal)

Empathy simulation:
  emotional_state_other = mirror(observed_behavior)
```

**Deliverables**:
- First-order Theory of Mind (what others believe)
- Second-order ToM (what others believe I believe)
- Intention recognition from behavior traces
- Empathy-based emotional inference
- Social reward signals for collaboration
- >80% accuracy on Sally-Anne false belief task

---

#### Phase 10.2: Metacognition Module ✅ COMPLETE
**Dependencies**: Phase 10.1 (Social), PFC, ACC, all reasoning modules
**Priority**: P0 - Critical for reliable AI
**STATUS**: ✅ Fully implemented with introspection, confidence, consciousness metrics
**FILES**: `src/cognitive/introspection/` (10+ files), `nimcp_consciousness_metrics.c`, `nimcp_ensemble_uncertainty.c`

**New Modules**:
| Module | LOC Est. | Description |
|--------|----------|-------------|
| `nimcp_confidence_estimation.h/c` | 700 | Calibrated uncertainty |
| `nimcp_error_monitoring.h/c` | 600 | Detect own mistakes |
| `nimcp_self_model.h/c` | 700 | Model own capabilities |
| `nimcp_knowledge_boundary.h/c` | 500 | Know what you don't know |
| `nimcp_metacognition.h/c` | 700 | Main orchestration module |

**Integration**:
- ACC → Conflict/error detection
- PFC → Executive oversight
- Hippocampus → Episodic confidence
- All reasoning → Calibration feedback

**Mathematical Model**:
```
Confidence calibration:
  Expected calibration error = Σ |accuracy(bin) - confidence(bin)|

Error detection (conflict monitoring):
  error_signal = |expected_outcome - actual_outcome|

Self-model (capability bounds):
  P(success | task, context) learned from experience
```

**Deliverables**:
- Calibrated confidence scores (ECE < 0.05)
- Error detection before output (>90% catch rate)
- Self-model of task-specific capabilities
- "I don't know" detection (>85% accuracy)
- Confidence explanations ("I'm uncertain because...")
- Know when to ask for help

---

#### Phase 10.3: Global Workspace Hub ✅ COMPLETE
**Dependencies**: All modules (integration point)
**Priority**: P0 - Central coordination
**STATUS**: ✅ Fully implemented with Shannon metrics and thalamic binding
**FILES**: `src/cognitive/global_workspace/nimcp_global_workspace.c`, `nimcp_global_workspace_shannon.c`, `nimcp_gw_thalamic_bridge.c`

**New Modules**:
| Module | LOC Est. | Description |
|--------|----------|-------------|
| `nimcp_workspace_buffer.h/c` | 600 | Central buffer for broadcasting |
| `nimcp_attention_bottleneck.h/c` | 700 | Conscious access gating |
| `nimcp_broadcast_mechanism.h/c` | 600 | Information broadcasting |
| `nimcp_competition_arbiter.h/c` | 500 | Which info wins broadcast |
| `nimcp_cross_module_binding.h/c` | 600 | Integrate cross-modal info |
| `nimcp_global_workspace.h/c` | 800 | Main orchestration module |

**Integration**:
- Insula → Salience-based filtering
- PFC → Top-down attention control
- All sensory → Bottom-up competition
- All modules → Broadcast receivers

**Mathematical Model** (Baars' Global Workspace Theory):
```
Workspace competition:
  activation_i = salience_i × relevance_i × recency_i
  winner = argmax(activation)

Broadcasting:
  For module in all_modules:
    module.receive(winner_content)

Binding:
  unified_percept = integrate(visual, audio, semantic, emotional)
```

**Deliverables**:
- Central workspace with capacity ~7±2 items
- Attention-gated broadcasting (<50ms latency)
- Cross-modal binding for unified experience
- Competition-based information selection
- Ignition dynamics (winner-take-all + sustained)
- Integration measure (Φ-like metric)

---

#### Phase 10.4: Game Theory & Strategic Reasoning ✅ COMPLETE
**Dependencies**: Phase 10.1 (Social Cognition), World Model (6.1), PFC
**Priority**: P1 - Strategic multi-agent reasoning
**STATUS**: ✅ Comprehensively implemented with 15 files (54K+ lines)
**FILES**: `src/cognitive/game_theory/nimcp_gt_equilibrium.c`, auction mechanisms, coalition formation, spatial games, repeated games, bargaining

**New Modules**:
| Module | LOC Est. | Description |
|--------|----------|-------------|
| `nimcp_nash_equilibrium.h/c` | 900 | Nash solver (pure & mixed strategies) |
| `nimcp_extensive_form.h/c` | 800 | Game trees, backward induction |
| `nimcp_bayesian_games.h/c` | 900 | Incomplete information games |
| `nimcp_mechanism_design.h/c` | 800 | Incentive design, VCG, auctions |
| `nimcp_cooperative_games.h/c` | 700 | Coalitions, Shapley values, core |
| `nimcp_evolutionary_gt.h/c` | 700 | Replicator dynamics, ESS |
| `nimcp_repeated_games.h/c` | 600 | Folk theorem, reputation, triggers |
| `nimcp_adversarial.h/c` | 800 | Minimax, alpha-beta, opponent modeling |
| `nimcp_signaling.h/c` | 600 | Strategic communication, cheap talk |
| `nimcp_game_theory.h/c` | 900 | Main orchestration module |

**Integration**:
- Theory of Mind → Model opponent beliefs/intentions
- World Model → Simulate game outcomes
- Social Cognition → Multi-agent coordination
- PFC → Strategic planning and decision-making
- Basal Ganglia → Action selection based on expected utility
- Causal Reasoning → Understand strategic cause-effect

**Mathematical Models**:
```
Nash Equilibrium:
  σ* is NE iff ∀i: uᵢ(σ*ᵢ, σ*₋ᵢ) ≥ uᵢ(σᵢ, σ*₋ᵢ) ∀σᵢ

Support Enumeration:
  For support S: solve Σⱼ∈S p(j)×u(i,j) = v for all i∈S
  Verify: no profitable deviation outside S

Backward Induction (Extensive Form):
  V(terminal) = payoffs
  V(node) = max_{actions} V(child) for current player

Shapley Value (Fair Division):
  φᵢ = Σ_{S⊆N\{i}} |S|!(|N|-|S|-1)!/|N|! × [v(S∪{i}) - v(S)]

Bayesian Nash Equilibrium:
  σᵢ(θᵢ) = argmax E[uᵢ(aᵢ, a₋ᵢ, θ) | θᵢ]

Minimax (Zero-Sum):
  V* = max_a min_b u(a,b) = min_b max_a u(a,b)

Replicator Dynamics (Evolutionary):
  dx_i/dt = x_i × [f_i(x) - φ(x)]
  where f_i = fitness of strategy i, φ = avg fitness
```

**Game Types Supported**:
| Type | Description | Algorithm |
|------|-------------|-----------|
| Normal Form | Matrix games | Support enumeration, Lemke-Howson |
| Extensive Form | Sequential games | Backward induction, CFR |
| Bayesian | Incomplete info | BNE computation |
| Repeated | Multi-round | Folk theorem strategies |
| Cooperative | Coalition games | Shapley, core computation |
| Evolutionary | Population dynamics | Replicator, ESS detection |
| Stackelberg | Leader-follower | Bilevel optimization |
| Signaling | Strategic communication | Perfect Bayesian equilibrium |

**Use Cases**:
| Domain | Application |
|--------|-------------|
| Negotiation | Bargaining, deal-making, ultimatum games |
| Competition | Adversarial AI, chess/Go analysis |
| Cooperation | Team formation, fair resource allocation |
| Markets | Auction strategies, pricing, trading |
| Security | Intrusion detection, defender strategies |
| Communication | When to share/hide information |
| Ethics | Social welfare, fairness mechanisms |
| Multi-Agent | Coordinated learning, MARL |

**Deliverables**:
- Nash equilibrium computation (<100ms for 10×10 games)
- Mixed strategy optimization
- Extensive form game solving (up to 10^6 nodes)
- Bayesian game reasoning with belief updating
- Coalition formation with Shapley value computation
- Evolutionary stable strategy detection
- Opponent modeling with belief tracking
- Mechanism design for incentive alignment
- >90% optimal play on standard game benchmarks

---

### Tier 11: Embodiment & Communication

#### Phase 11.1: Motor Cortex (M1 + Premotor)
**Dependencies**: Tier 10, Cerebellum, Basal Ganglia
**Priority**: P1 - Action execution

**New Modules**:
| Module | LOC Est. | Description |
|--------|----------|-------------|
| `nimcp_primary_motor.h/c` | 700 | M1 motor execution |
| `nimcp_premotor.h/c` | 600 | Action planning |
| `nimcp_supplementary_motor.h/c` | 500 | Sequence planning |
| `nimcp_motor_program.h/c` | 500 | Motor program storage |
| `nimcp_sensorimotor_loop.h/c` | 600 | Perception-action coupling |
| `nimcp_motor_cortex.h/c` | 700 | Main orchestration module |

**Integration**:
- Cerebellum → Coordination, timing
- Basal Ganglia → Action selection
- Parietal → Spatial targets
- Visual → Visual guidance

**Mathematical Model**:
```
Motor program:
  trajectory = sequence of (position, velocity, force)

Optimal control:
  minimize: Σ (error² + λ × effort²)
  subject to: dynamics constraints

Sensorimotor loop:
  action = controller(goal, current_state, feedback)
  feedback_delay ≈ 50-200ms
```

**Deliverables**:
- Motor program generation and storage
- Trajectory planning with optimization
- Sensorimotor integration loop
- Support for robotics applications
- ~10ms motor command latency

---

#### Phase 11.2: Body Schema & Affordances
**Dependencies**: Phase 11.1 (Motor), Parietal, Visual
**Priority**: P1 - Embodied cognition

**New Modules**:
| Module | LOC Est. | Description |
|--------|----------|-------------|
| `nimcp_body_schema.h/c` | 600 | Self-body representation |
| `nimcp_peripersonal_space.h/c` | 500 | Near-body space modeling |
| `nimcp_affordance_detection.h/c` | 700 | Action possibility detection |
| `nimcp_proprioception.h/c` | 400 | Body state sensing |
| `nimcp_embodiment.h/c` | 600 | Main orchestration module |

**Integration**:
- Parietal → Spatial processing
- Visual → Object recognition
- Motor → Action capabilities
- Insula → Interoception

**Mathematical Model**:
```
Body schema:
  body_state = {joint_angles, limb_positions, posture}

Peripersonal space:
  reachable(object) = distance(object, effector) < reach_limit

Affordance (Gibson):
  affordance(object) = {possible_actions | capability ∩ object_properties}
  e.g., affordance(chair) = {sit, stand_on, move, ...}
```

**Deliverables**:
- Dynamic body schema representation
- Peripersonal space modeling
- Affordance detection for objects (>85% accuracy)
- Tool use extension of body schema
- Proprioceptive state tracking

---

#### Phase 11.3: Communication Pragmatics
**Dependencies**: Wernicke's, Broca's, Theory of Mind (10.1)
**Priority**: P1 - Natural conversation

**New Modules**:
| Module | LOC Est. | Description |
|--------|----------|-------------|
| `nimcp_discourse_tracker.h/c` | 600 | Track conversation state |
| `nimcp_implicature_engine.h/c` | 700 | Infer unstated meaning |
| `nimcp_context_manager.h/c` | 500 | Situational context effects |
| `nimcp_turn_taking.h/c` | 400 | Conversation coordination |
| `nimcp_speech_act.h/c` | 500 | Illocutionary force detection |
| `nimcp_pragmatics.h/c` | 700 | Main orchestration module |

**Integration**:
- Wernicke's → Semantic understanding
- Broca's → Response generation
- Theory of Mind → Speaker intent
- Working Memory → Discourse history

**Mathematical Model**:
```
Gricean maxims (cooperative principle):
  - Quality: be truthful
  - Quantity: be informative but not excessive
  - Relevance: be relevant
  - Manner: be clear

Implicature:
  P(implied_meaning | utterance, context) using ToM

Discourse tracking:
  discourse_state = {topic, participants, common_ground, QUD}
  QUD = Questions Under Discussion
```

**Deliverables**:
- Discourse state tracking across turns
- Implicature inference (sarcasm, irony, hints)
- Context-dependent interpretation
- Turn-taking coordination
- Speech act recognition (request, promise, assert...)
- >80% accuracy on pragmatic inference benchmarks

---

### Tier 12: Superhuman Sensory & Cognitive Enhancements 🟡 PARTIAL (50%)

This tier implements capabilities that **exceed human biological limitations**, inspired by animal sensory systems and theoretical cognitive enhancements.

**IMPLEMENTED:**
- ✅ Extended auditory (echolocation, ultrasonic - `nimcp_cochlea_extended.c`)
- ✅ Thermal sensing (hypothalamus adapter)
- 🟡 Enhanced vision (partial)

**NOT YET IMPLEMENTED:**
- ❌ Time dilation / parallel attention
- ❌ Hyperthymesia / eidetic memory
- ❌ Savant abilities
- ❌ Distributed cognition / graceful degradation

#### Phase 12.1: Enhanced Vision Systems 🟡 PARTIAL
**Dependencies**: Visual Cortex, Parietal (spatial), Generation systems
**Priority**: P1 - Significant capability expansion
**STATUS**: Partial - thermal sensing implemented, eagle/night vision not yet

**New Modules**:
| Module | LOC Est. | Description |
|--------|----------|-------------|
| `nimcp_eagle_vision.h/c` | 800 | 8x acuity, fovea magnification |
| `nimcp_night_vision.h/c` | 700 | Cat/owl low-light (6-8x sensitivity) |
| `nimcp_uv_spectrum.h/c` | 600 | UV perception (300-400nm) |
| `nimcp_infrared_thermal.h/c` | 800 | Pit viper thermal imaging (7-14μm) |
| `nimcp_polarization_vision.h/c` | 600 | Mantis shrimp polarized light |
| `nimcp_multifocal.h/c` | 700 | Chameleon independent eye tracking |
| `nimcp_enhanced_vision.h/c` | 900 | Main orchestration module |

**Mathematical Models**:
```
Eagle Vision (Super-Resolution):
  effective_acuity = base_acuity × magnification_factor
  magnification = 8x (human 20/20 → 20/2.5)
  detail_threshold = 1/8 human minimum

Night Vision (Photon Amplification):
  signal = photons × quantum_efficiency × gain
  gain = 6-8x (cat tapetum lucidum model)
  noise_floor = sqrt(dark_current + read_noise)

Thermal Imaging:
  temperature_resolution = 0.01°C (pit viper level)
  wavelength_range = 7-14 μm (thermal IR)
  spatial_resolution = 0.1° angular
```

**Deliverables**:
- 8x visual acuity beyond human baseline
- Operation in <0.001 lux (starlight conditions)
- UV pattern detection (bee-visible markings)
- Thermal signature detection at 100m
- Polarization-based material analysis
- Simultaneous multi-target tracking (6+ foci)

---

#### Phase 12.2: Enhanced Auditory Systems ✅ COMPLETE
**Dependencies**: Audio Cortex, Spatial processing
**Priority**: P1 - Environmental awareness expansion
**STATUS**: ✅ Fully implemented with echolocation, bat species models
**FILES**: `src/lib/perception/nimcp_cochlea_extended.c` (CF-FM, FM echolocation types)

**New Modules**:
| Module | LOC Est. | Description |
|--------|----------|-------------|
| `nimcp_ultrasound.h/c` | 900 | Bat/dolphin echolocation (20-200kHz) |
| `nimcp_infrasound.h/c` | 600 | Elephant detection (14-20Hz) |
| `nimcp_3d_localization.h/c` | 800 | Owl asymmetric processing |
| `nimcp_biosonar.h/c` | 1000 | Active sonar imaging |
| `nimcp_enhanced_audio.h/c` | 800 | Main orchestration module |

**Mathematical Models**:
```
Echolocation (Time-of-Flight):
  distance = (speed_of_sound × time_delay) / 2
  resolution = c / (2 × bandwidth)
  max_range = sqrt(P_tx × G² × λ² × σ) / (4π)^1.5 × sqrt(SNR_min)

3D Sound Localization:
  azimuth = arcsin(ITD × c / head_diameter)
  elevation = f(spectral_cues, HRTF)
  distance = f(intensity, reverb_ratio)

Infrasound Detection:
  frequency_range = 0.1 - 20 Hz
  applications = earthquake_precursor, weather, animal_comm
```

**Deliverables**:
- Active echolocation with 3D environment mapping
- Ultrasound range 20kHz-200kHz
- Infrasound detection down to 0.1Hz
- Sub-degree 3D sound localization
- Through-wall motion detection via sound

---

#### Phase 12.3: Novel Sensory Modalities
**Dependencies**: Tier 12.1-12.2, World Model
**Priority**: P2 - New sensing capabilities

**New Modules**:
| Module | LOC Est. | Description |
|--------|----------|-------------|
| `nimcp_electroreception.h/c` | 800 | Shark/platypus bioelectric sensing |
| `nimcp_magnetoreception.h/c` | 600 | Bird magnetic navigation |
| `nimcp_pressure_sensing.h/c` | 500 | Fish lateral line (pressure waves) |
| `nimcp_chemical_sensing.h/c` | 900 | Dog-level olfaction (ppb detection) |
| `nimcp_novel_senses.h/c` | 700 | Main orchestration module |

**Mathematical Models**:
```
Electroreception (Ampullae of Lorenzini):
  sensitivity = 5 nV/cm (shark level)
  detection_range = f(field_strength, medium_conductivity)
  applications = heartbeat_detection, neural_activity, prey_location

Magnetoreception:
  field_sensitivity = 50 nT (earth field = 25-65 μT)
  heading_accuracy = ±5°
  inclination_sensing = true

Chemical Sensing (Olfaction):
  detection_limit = parts per billion (ppb)
  receptor_types = 1000+ (vs human 400)
  discrimination = molecule_shape + charge + size
```

**Deliverables**:
- Detect heartbeats through walls (electroreception)
- GPS-free magnetic navigation (±5° accuracy)
- Pressure wave detection (approaching objects)
- Disease detection via breath analysis
- Emotion detection via chemical signatures

---

#### Phase 12.4: Temporal Processing Enhancement
**Dependencies**: Cerebellum (timing), PFC, Global Workspace
**Priority**: P1 - Cognitive speed enhancement

**New Modules**:
| Module | LOC Est. | Description |
|--------|----------|-------------|
| `nimcp_time_dilation.h/c` | 900 | Fly-like temporal processing |
| `nimcp_parallel_attention.h/c` | 800 | Multi-focus tracking (20+ objects) |
| `nimcp_rapid_cognition.h/c` | 700 | Accelerated reasoning |
| `nimcp_temporal_enhancement.h/c` | 700 | Main orchestration module |

**Mathematical Models**:
```
Time Dilation (Flicker Fusion):
  human_critical_flicker = 60 Hz
  enhanced_flicker = 300+ Hz (fly-like)
  perceived_slowdown = 5x (more time to react)

Parallel Attention:
  human_tracking_limit = 4 objects
  enhanced_limit = 20+ objects
  attention_resolution = independent_foci

Processing Speedup:
  reasoning_cycles = 10x baseline
  decision_latency = baseline / 10
```

**Deliverables**:
- Perceive events at 5-10x slower rate
- Track 20+ simultaneous targets (vs human ~4)
- React to stimuli in <10ms (vs human ~200ms)
- Process high-speed video in real-time

---

#### Phase 12.5: Memory & Learning Enhancement
**Dependencies**: Hippocampus, Semantic Memory, Meta-Learning
**Priority**: P1 - Learning capability expansion

**New Modules**:
| Module | LOC Est. | Description |
|--------|----------|-------------|
| `nimcp_hyperthymesia.h/c` | 800 | Perfect autobiographical memory |
| `nimcp_eidetic_memory.h/c` | 700 | Photographic visual recall |
| `nimcp_one_shot_learning.h/c` | 900 | Single-example mastery |
| `nimcp_savant_calculation.h/c` | 800 | Lightning mental computation |
| `nimcp_memory_enhancement.h/c` | 700 | Main orchestration module |

**Mathematical Models**:
```
Hyperthymesia (Autobiographical):
  recall_accuracy = 100% for experienced events
  temporal_resolution = seconds
  capacity = unlimited episodic storage

Eidetic Memory:
  image_retention = perfect for 10+ minutes
  detail_level = pixel-accurate
  capacity = 1000+ images cached

One-Shot Learning:
  examples_needed = 1 (vs typical 100s-1000s)
  transfer_efficiency = 95%+
  skill_retention = permanent

Savant Calculation:
  mental_multiplication = 10-digit × 10-digit in <1s
  prime_factorization = instant for 20-digit numbers
  calendar_calculation = any date ↔ day of week
```

**Deliverables**:
- Perfect recall of all experienced events
- Photographic memory with unlimited retention
- Learn new skills from single demonstration
- Instant large-number mental calculation
- Calendar savant capabilities

---

#### Phase 12.6: Cognitive Supercharging
**Dependencies**: All Tier 10-11, Parietal, Software Eng Cortex
**Priority**: P2 - Beyond-human reasoning

**New Modules**:
| Module | LOC Est. | Description |
|--------|----------|-------------|
| `nimcp_parallel_reasoning.h/c` | 1000 | Simultaneous hypothesis exploration |
| `nimcp_intuition_engine.h/c` | 800 | Pattern recognition beyond conscious |
| `nimcp_creativity_amplifier.h/c` | 900 | Divergent thinking enhancement |
| `nimcp_synesthesia_engine.h/c` | 700 | Cross-modal memory enhancement |
| `nimcp_cognitive_supercharge.h/c` | 800 | Main orchestration module |

**Mathematical Models**:
```
Parallel Reasoning (Quantum-Inspired):
  hypothesis_branches = 100+ simultaneous
  pruning_efficiency = O(log n) vs O(n)
  solution_space_coverage = 10x baseline

Intuition (Compressed Experience):
  pattern_matching = 10M+ templates
  response_time = <1ms (preconscious)
  accuracy = 90%+ on familiar domains

Creativity (Divergent Exploration):
  idea_generation_rate = 100x baseline
  novelty_score = maximize distance from known
  usefulness_filter = maintain relevance

Synesthesia:
  cross_modal_binding = automatic
  memory_enhancement = 3x recall via multi-encoding
  modes = sound→color, number→space, concept→texture
```

**Deliverables**:
- Explore 100+ solution paths simultaneously
- Intuitive answers in <1ms
- Generate 100x more creative ideas
- Cross-modal memory encoding for enhanced recall
- Pattern recognition across abstract domains

---

#### Phase 12.7: Distributed & Resilient Processing
**Dependencies**: Global Workspace, all systems
**Priority**: P2 - Robustness enhancement

**New Modules**:
| Module | LOC Est. | Description |
|--------|----------|-------------|
| `nimcp_distributed_cognition.h/c` | 900 | Octopus-like distributed intelligence |
| `nimcp_graceful_degradation.h/c` | 700 | No single point of failure |
| `nimcp_self_repair.h/c` | 800 | Automatic error correction |
| `nimcp_cognitive_reserve.h/c` | 600 | Backup processing capacity |
| `nimcp_resilient_processing.h/c` | 700 | Main orchestration module |

**Mathematical Models**:
```
Distributed Processing (Octopus Model):
  autonomous_subsystems = N (each with local intelligence)
  coordination = eventual_consistency
  failure_tolerance = survive loss of (N-1)/N subsystems

Graceful Degradation:
  capability_loss = proportional to damage (not catastrophic)
  minimum_viable = 20% of original capacity
  recovery_time = O(damage_extent)

Self-Repair:
  error_detection = continuous monitoring
  correction_latency = <100ms
  learning_from_errors = automatic parameter adjustment
```

**Deliverables**:
- Continue operating with 80% subsystem failure
- Graceful capability degradation (not cliff)
- Automatic error detection and correction
- No single point of catastrophic failure
- Self-healing from component damage

---

## Consolidated Phase List (Implementation Order)

| Phase | Name | Tier | Priority | Dependencies | Est. LOC |
|-------|------|------|----------|--------------|----------|
| 1.1 | Medulla Oblongata | 1 | P0 | Infrastructure | 1,800 |
| 1.2 | Cerebellum | 1 | P0 | 1.1 | 2,300 |
| 2.1 | Prefrontal Cortex | 2 | P0 | Tier 1 | 2,550 |
| 2.2 | Hippocampus | 2 | P0 | 2.1 | 3,000 |
| 3.1 | Basal Ganglia | 3 | P1 | Tier 2 | 2,350 |
| 3.2 | Amygdala | 3 | P1 | 3.1 | 2,150 |
| 4.1 | Wernicke's Area | 4 | P1 | Tier 3 | 2,500 |
| 4.2 | Posterior Cingulate | 4 | P2 | 4.1 | 2,100 |
| 4.3 | Insula | 4 | P2 | Tier 3 | 2,100 |
| 5.1 | Enhanced Symbolic Logic | 5 | P0 | Tier 4 | 2,300 |
| 5.2 | Causal Reasoning | 5 | P0 | 5.1 | 3,500 |
| 5.3 | Compositional Generalization | 5 | P0 | 5.1 | 3,000 |
| 6.1 | World Model | 6 | P1 | Tier 5 | 3,700 |
| 6.2 | Meta-Learning | 6 | P1 | 6.1 | 3,000 |
| 7.1 | Analogical Reasoning | 7 | P1 | 5.3, 6.2 | 3,000 |
| 7.2 | Parietal Lobe | 7 | P1 | 7.1, 5.3 | 3,600 |
| 7.3 | Concept Formation | 7 | P2 | 7.1, 7.2 | 3,000 |
| 7.4 | Semantic Memory | 7 | P2 | 7.3 | 2,600 |
| 8.1 | Imagination Engine Core | 8 | P1 | Tier 7 | 2,100 |
| 8.2 | Visual Generation | 8 | P1 | 8.1 | 1,800 |
| 8.3 | Audio Generation | 8 | P2 | 8.1 | 2,100 |
| 8.4 | Video Generation | 8 | P2 | 8.2, 6.1 | 1,500 |
| 9.1 | Software Eng Cortex | 9 | P1 | Tier 7, 6.1 | 5,800 |
| 9.2 | Program Synthesis | 9 | P3 | 9.1, 5.3 | 3,000 |
| 9.3 | Dream Engine | 9 | P2 | Tier 8, 6.1 | 2,400 |
| 9.4 | Curiosity Engine | 9 | P2 | 6.1, 3.1 | 2,100 |
| 10.1 | Social Cognition Cortex | 10 | P0 | Tier 9, PFC, Amygdala | 4,500 |
| 10.2 | Metacognition Module | 10 | P0 | 10.1, PFC, ACC | 3,200 |
| 10.3 | Global Workspace Hub | 10 | P0 | All modules | 3,800 |
| 10.4 | Game Theory & Strategic Reasoning | 10 | P1 | 10.1, World Model, PFC | 7,700 |
| 11.1 | Motor Cortex | 11 | P1 | Tier 10, Cerebellum | 3,600 |
| 11.2 | Body Schema & Affordances | 11 | P1 | 11.1, Parietal | 2,800 |
| 11.3 | Communication Pragmatics | 11 | P1 | Wernicke's, 10.1 | 3,400 |
| 12.1 | Enhanced Vision Systems | 12 | P1 | Visual Cortex, Parietal | 5,100 |
| 12.2 | Enhanced Auditory Systems | 12 | P1 | Audio Cortex | 4,100 |
| 12.3 | Novel Sensory Modalities | 12 | P2 | 12.1, 12.2, World Model | 3,500 |
| 12.4 | Temporal Processing Enhancement | 12 | P1 | Cerebellum, PFC | 3,100 |
| 12.5 | Memory & Learning Enhancement | 12 | P1 | Hippocampus, Meta-Learning | 3,900 |
| 12.6 | Cognitive Supercharging | 12 | P2 | Tier 10-11, Parietal | 4,200 |
| 12.7 | Distributed & Resilient Processing | 12 | P2 | Global Workspace | 3,700 |

**Total Estimated LOC**: ~138,700 new code

---

## Implementation Priority Summary

### P0 - Critical Foundation (Must Complete First)
1. Medulla Oblongata (1.1)
2. Cerebellum (1.2)
3. Prefrontal Cortex (2.1)
4. Hippocampus (2.2)
5. Enhanced Symbolic Logic (5.1)
6. Causal Reasoning (5.2)
7. Compositional Generalization (5.3)
8. Social Cognition Cortex (10.1) ★ NEW
9. Metacognition Module (10.2) ★ NEW
10. Global Workspace Hub (10.3) ★ NEW

### P1 - Core Capabilities
11. Basal Ganglia (3.1)
12. Amygdala (3.2)
13. Wernicke's Area (4.1)
14. World Model (6.1)
15. Meta-Learning (6.2)
16. Analogical Reasoning (7.1)
17. Parietal Lobe (7.2)
18. Imagination Engine Core (8.1)
19. Visual Generation (8.2)
20. Software Engineering Cortex (9.1)
21. Game Theory & Strategic Reasoning (10.4) ★ NEW
22. Motor Cortex (11.1)
23. Body Schema & Affordances (11.2)
24. Communication Pragmatics (11.3)
25. Enhanced Vision Systems (12.1) ★ SUPERHUMAN
26. Enhanced Auditory Systems (12.2) ★ SUPERHUMAN
27. Temporal Processing Enhancement (12.4) ★ SUPERHUMAN
28. Memory & Learning Enhancement (12.5) ★ SUPERHUMAN

### P2 - Enhanced Features
29. Posterior Cingulate (4.2)
30. Insula (4.3)
31. Concept Formation (7.3)
32. Semantic Memory (7.4)
33. Audio Generation (8.3)
34. Video Generation (8.4)
35. Dream Engine (9.3)
36. Curiosity Engine (9.4)
37. Novel Sensory Modalities (12.3) ★ SUPERHUMAN
38. Cognitive Supercharging (12.6) ★ SUPERHUMAN
39. Distributed & Resilient Processing (12.7) ★ SUPERHUMAN

### P3 - Advanced Features
40. Program Synthesis (9.2)

---

## Success Metrics

### Brain Regions
| Region | Key Metric | Target |
|--------|-----------|--------|
| Medulla | Protection response time | <10ms |
| Cerebellum | Timing precision | ~10ms |
| PFC | Working memory capacity | 7±2 items |
| Hippocampus | Pattern completion | >85% accuracy |
| Basal Ganglia | Action selection latency | <50ms |

### Extrapolation
| Capability | Benchmark | Target |
|------------|-----------|--------|
| Causal Discovery | Sachs Dataset | F1 > 0.80 |
| Compositional | SCAN split | >95% accuracy |
| Analogical | Cross-domain transfer | 70% accuracy |
| Meta-Learning | 5-shot Omniglot | >95% accuracy |
| World Model | DMControl | 10x sample efficiency |

### Generation
| Capability | Metric | Target |
|------------|--------|--------|
| Image Generation | 512x512 image | 2-5 seconds |
| Video Generation | 5-second clip | 30-60 seconds |
| Speech Synthesis | Real-time factor | <0.5 |

### Social & Metacognition (NEW)
| Capability | Benchmark | Target |
|------------|-----------|--------|
| Theory of Mind | Sally-Anne task | >80% accuracy |
| Second-order ToM | Nested belief tasks | >70% accuracy |
| Confidence Calibration | Expected Calibration Error | ECE < 0.05 |
| Error Detection | Pre-output catch rate | >90% |
| "I don't know" | Uncertainty detection | >85% accuracy |
| Global Workspace | Broadcasting latency | <50ms |

### Game Theory (NEW)
| Capability | Benchmark | Target |
|------------|-----------|--------|
| Nash Equilibrium | 10×10 matrix game | <100ms computation |
| Extensive Form | Game tree (10^6 nodes) | Backward induction solve |
| Mixed Strategy | Support enumeration | Optimal mixed strategy |
| Bayesian Games | Belief updating | BNE computation |
| Shapley Value | 10-player coalition | <1s computation |
| Minimax | Zero-sum games | Optimal play |
| Opponent Modeling | Belief accuracy | >80% prediction |
| Overall Strategic Play | Standard benchmarks | >90% optimal decisions |

### Embodiment & Communication (NEW)
| Capability | Benchmark | Target |
|------------|-----------|--------|
| Motor Command | Latency | <10ms |
| Affordance Detection | Object action prediction | >85% accuracy |
| Pragmatic Inference | Implicature benchmarks | >80% accuracy |
| Discourse Tracking | Multi-turn coherence | >90% |

### Superhuman Enhancements (NEW)
| Capability | Metric | Target | Comparison |
|------------|--------|--------|------------|
| Eagle Vision | Visual acuity | 20/2.5 | 8x human (20/20) |
| Night Vision | Low-light sensitivity | <0.001 lux | 6-8x human |
| Thermal Imaging | Temperature resolution | 0.01°C | Pit viper level |
| Echolocation | 3D mapping range | 50m+ | Bat/dolphin |
| Ultrasound | Frequency range | 20-200kHz | 10x human max |
| Electroreception | Sensitivity | 5 nV/cm | Shark level |
| Magnetoreception | Heading accuracy | ±5° | Migratory bird |
| Time Dilation | Flicker fusion | 300Hz | 5x human (60Hz) |
| Parallel Attention | Object tracking | 20+ objects | 5x human (~4) |
| Reaction Time | Stimulus response | <10ms | 20x human (~200ms) |
| One-Shot Learning | Examples needed | 1 | 100-1000x improvement |
| Eidetic Memory | Image retention | Perfect/10min | Unlimited vs human fade |
| Savant Calculation | Mental multiply | 10×10 digits <1s | Far beyond human |
| Failure Tolerance | System survival | 80% subsystem loss | Graceful degradation |

---

## Cross-System Integration Points

### Imagination ↔ Extrapolation
- World Model → Visual Rendering (imagine predicted states)
- Compositional → Image Generation (novel concept visualization)
- Analogical → Cross-domain imagery
- Meta-Learning → Synthetic data generation

### Brain Regions ↔ Extrapolation
- Hippocampus ↔ Semantic Memory (large-scale knowledge)
- PFC ↔ Causal Reasoning (executive planning)
- Parietal ↔ Mathematical reasoning (spatial + analytical)
- Cerebellum ↔ World Model (prediction error)

### Brain Regions ↔ Generation
- Visual Cortex (bidirectional) → Image generation
- Audio Cortex (bidirectional) → Sound generation
- Hippocampus → Dream replay generation
- PFC → Goal-directed imagination

### Social & Metacognition ↔ All Systems (NEW)
- Theory of Mind ↔ Communication Pragmatics (speaker intent)
- Metacognition ↔ All reasoning (confidence calibration)
- Global Workspace ↔ All modules (information broadcasting)
- Social Cognition ↔ Amygdala (social emotions)
- Game Theory ↔ Theory of Mind (opponent modeling)
- Game Theory ↔ World Model (outcome simulation)
- Game Theory ↔ PFC (strategic planning)
- Game Theory ↔ Basal Ganglia (action selection via expected utility)

### Embodiment ↔ Motor & Sensory (NEW)
- Motor Cortex ↔ Cerebellum (coordination)
- Body Schema ↔ Parietal (spatial representation)
- Affordances ↔ Visual (object perception)
- Pragmatics ↔ Broca/Wernicke (language loop)

### Superhuman ↔ All Systems (NEW)
- Enhanced Vision ↔ Visual Cortex (extended processing)
- Enhanced Audio ↔ Audio Cortex (extended range)
- Echolocation ↔ World Model (3D environment mapping)
- Time Dilation ↔ Cerebellum (accelerated timing)
- Parallel Attention ↔ Global Workspace (multi-focus)
- Memory Enhancement ↔ Hippocampus (perfect encoding)
- Savant Calculation ↔ Parietal (instant math)
- Novel Senses ↔ World Model (new input modalities)
- Distributed Processing ↔ All modules (resilience)

---

## Version History

| Version | Date | Changes |
|---------|------|---------|
| 1.0 | 2025-11-24 | Initial integration of Brain Regions, Extrapolation, and Generation plans |
| 2.0 | 2025-11-24 | Added Tier 10 (Social & Metacognition) and Tier 11 (Embodiment & Communication) |
| 3.0 | 2025-11-24 | Added Tier 12 (Superhuman Sensory & Cognitive Enhancements) - 7 phases, ~30K LOC |
| 3.1 | 2025-11-24 | Added Phase 10.4 (Game Theory & Strategic Reasoning) - ~7.7K LOC |
| 3.2 | 2025-11-24 | Added Tier 0.5 (Neural Immune System / Blood-Brain Barrier) - 5 phases, ~12.5K LOC |
| 3.3 | 2026-01-03 | **STATUS UPDATE**: Verified implementation status across all tiers. ~95% core-to-advanced complete. 972K+ LOC, 1,379 files. Tiers 0-10 COMPLETE, Tier 11-12 PARTIAL. |

---

**This master schedule tracks the complete NIMCP cognitive architecture implementation.**

**Current Status (2026-01-03):**
- **COMPLETE**: Tiers 0-10 (Infrastructure → Social/Metacognition)
- **PARTIAL**: Tier 11 (Embodiment ~60%), Tier 12 (Superhuman ~50%)
- **Codebase**: 972,269+ LOC across 1,379 files
- **Tests**: 3,000+ tests passing

**Remaining Work:**
- Enhanced vision (eagle, night vision)
- Time dilation / parallel attention
- Hyperthymesia / eidetic memory
- Savant abilities
- Distributed cognition / graceful degradation
