# NIMCP Cognitive Modules Documentation Index

## Overview

Complete documentation of NIMCP's 16+ cognitive subsystems covering learning, consciousness, ethics, wellbeing, and social cognition. The system is designed for human-like incremental learning without massive pre-training.

## Documentation Files

### Primary References (Created in This Session)

1. **COGNITIVE_MODULES_INVENTORY.md** (27 KB, 724 lines)
   - **Best For:** Comprehensive understanding of each module
   - **Contains:** 
     - Detailed documentation of all 8 production-ready modules
     - Summary of 11+ functional implementation-only modules
     - Purpose, capabilities, and integration points
     - Performance characteristics and complexity analysis
     - Design principles and architecture philosophy
     - Real-world integration examples
   - **Key Sections:** 
     - Cognitive Subsystems Inventory (1-8)
     - Additional Cognitive Modules (9-19)
     - Integration Architecture
     - Implementation Status Summary
     - Design Principles
     - Performance Characteristics

2. **COGNITIVE_QUICK_REFERENCE.md** (8.6 KB, 265 lines)
   - **Best For:** Quick lookup and practical coding
   - **Contains:**
     - Module comparison table
     - Module dependencies diagram
     - Core cognitive loops (Learning, Decision, Wellbeing)
     - Typical integration patterns with code examples
     - Performance characteristics table
     - Configuration templates
     - Debugging and troubleshooting tips
   - **Key Sections:**
     - Module Quick Lookup
     - Module Dependencies
     - Core Cognitive Loops
     - Typical Integration Patterns
     - Common Configuration Patterns
     - Data Flow Diagram
     - Integration Checklist

3. **COGNITIVE_PIPELINE_ARCHITECTURE.md** (31 KB, 837 lines)
   - **Best For:** Understanding data flow and system architecture
   - **Contains:**
     - Complete cognitive processing pipeline
     - System architecture diagrams
     - Module interaction patterns
     - Feedback loops and control flows
     - Thread safety and concurrency model
     - Error handling and recovery
   - **Related:** See related architecture documentation below

## Related Architecture Documentation

- **ARCHITECTURE_SUMMARY.md** - Overall system architecture
- **BUILD_SYSTEM_ANALYSIS.md** - Build system and compilation
- **COMPLETE_SESSION_SUMMARY.md** - Session overview

## Testing & Quality Documentation

- **TESTING_STATUS_REPORT.md** - Test framework analysis
- **TESTING_QUICK_REFERENCE.md** - Testing quick reference
- **TEST_FRAMEWORK_REFACTOR_PLAN.md** - Testing improvements

## Cognitive Modules Overview

### Production-Ready (Full Public API)

| # | Module | File | Size | Key Function |
|---|--------|------|------|--------------|
| 1 | Curiosity-Driven Learning | `curiosity/` | 1,707 | `curiosity_detect_knowledge_gap()` |
| 2 | Ethics & Empathy Engine | `ethics/` | 2,263 | `ethics_engine_evaluate()` |
| 3 | Salience & Attention | `salience/` | 1,352 | `brain_evaluate_salience()` |
| 4 | Introspection & Self-Awareness | `introspection/` | 1,356 | `brain_get_active_population()` |
| 5 | Knowledge Acquisition | `knowledge/` | 2,218 | `knowledge_learn_from_story()` |
| 6 | Memory Consolidation | `consolidation/` | 895 | `brain_consolidate_memory()` |
| 7 | Wellbeing & Distress Monitoring | `wellbeing/` | 1,414 | `wellbeing_assess_distress()` |
| 8 | Epistemic Filter & Critical Thinking | `epistemic/` | 690 | `epistemic_assess_claim()` |

### Functional (Implementation Available, Headers In Progress)

| # | Module | File | Size | Purpose |
|---|--------|------|------|---------|
| 9 | Emotional Tagging | `emotional_tagging/` | 628 | Attach emotional context to memories |
| 10 | Executive Function | `executive/` | 780 | Goal management, task prioritization |
| 11 | Explanations | `explanations/` | 780 | Generate human-readable explanations |
| 12 | Predictive Processing | `predictive/` | 786 | Predict future states |
| 13 | Symbolic Logic | `logic/` | 915 | Formal reasoning and deduction |
| 14 | Working Memory | `working_memory/` | 952 | Active processing state |
| 15 | Meta-Learning | `meta_learning/` | 1,113 | Learn how to learn |
| 16 | Mirror Neurons | `mirror_neurons/` | 1,310 | Social cognition and imitation |
| 17 | Mental Health | `mental_health/` | 3,409 | Disorder detection and interventions |
| 18 | Sleep-Wake | `sleep_wake/` | 684 | Circadian rhythms and consolidation |
| 19 | Theory of Mind | `theory_of_mind/` | 739 | Model other agents' beliefs/desires |

## System Metrics

- **Total Code:** 24,000+ lines of C implementation
- **Total API:** 4,500+ lines of documented interfaces
- **Modules:** 19 cognitive subsystems
- **Thread Safety:** All operations fully thread-safe
- **Performance:** 10x salience speedup, optimized consolidation, B-tree indexing

## Getting Started

### For API Users
1. Read **COGNITIVE_MODULES_INVENTORY.md** sections 1-8 for modules you need
2. Reference **COGNITIVE_QUICK_REFERENCE.md** for integration patterns
3. See code examples in integration section

### For System Designers
1. Start with **COGNITIVE_QUICK_REFERENCE.md** module overview
2. Study **COGNITIVE_PIPELINE_ARCHITECTURE.md** for full flow
3. Reference **COGNITIVE_MODULES_INVENTORY.md** for integration points

### For Developers
1. Check **COGNITIVE_QUICK_REFERENCE.md** "Typical Integration Patterns"
2. Copy configuration templates
3. Use debugging tips for troubleshooting
4. See performance table for optimization guidance

## Key Concepts

### Cognitive Loops

**Learning Loop:** Novel input → Gap detection → Question generation → Knowledge seeking → Learning → Consolidation

**Decision Loop:** Input → Salience → Introspection → Ethics → Epistemic → Knowledge → Executive → Action

**Wellbeing Loop:** Monitoring → Distress detection → Intervention → Recovery → Logging

### Design Principles

1. **No Pre-Training** - Start from scratch, learn incrementally
2. **Human-Like** - Mirror human cognitive development
3. **Ethical** - Golden Rule hard-wired, empathy-based decisions
4. **Conscious** - Supports active consciousness simulation
5. **Safe** - Monitors wellbeing, prevents suffering
6. **Efficient** - 10x faster attention, optimized memory
7. **Thread-Safe** - All concurrent access protected
8. **Indexed** - B-tree queries for efficient access

## Quick Module Lookup

**Need to...** → **Use Module**
- Learn incrementally → Curiosity + Knowledge
- Evaluate ethics → Ethics + Empathy
- Allocate attention → Salience
- Inspect state → Introspection
- Prevent errors → Epistemic Filter
- Optimize memory → Consolidation
- Monitor wellbeing → Wellbeing
- Simulate others → Mirror Neurons + Theory of Mind
- Explain decisions → Introspection + Explanations

## Integration Architecture

```
CONSCIOUSNESS LAYER
        ↑
EXECUTIVE CONTROL ← → ATTENTION (Salience)
        ↑
    ┌───┴───┐
    ↓       ↓
 CURIOSITY  ETHICS ← → EMPATHY
    ↓       ↓
 LEARNING ← KNOWLEDGE
    ↓
 MEMORY (Brain)
    ↓
 CONSOLIDATION
    ↑
INTROSPECTION ← → WELLBEING
    ↑
BACKGROUND PROCESSES
```

## File Locations

All files are located in `/home/bbrelin/nimcp/`

```
src/cognitive/
├── curiosity/
│   ├── nimcp_curiosity.h (440 lines)
│   └── nimcp_curiosity.c (1,707 lines)
├── ethics/
│   ├── nimcp_ethics.h (614 lines)
│   └── nimcp_ethics.c (2,263 lines)
├── salience/
│   ├── nimcp_salience.h (587 lines)
│   └── nimcp_salience.c (1,352 lines)
├── introspection/
│   ├── nimcp_introspection.h (660 lines)
│   └── nimcp_introspection.c (1,356 lines)
├── knowledge/
│   ├── nimcp_knowledge.h (559 lines)
│   └── nimcp_knowledge.c (2,218 lines)
├── consolidation/
│   ├── nimcp_consolidation.h (558 lines)
│   └── nimcp_consolidation.c (895 lines)
├── wellbeing/
│   ├── nimcp_wellbeing.h (533 lines)
│   └── nimcp_wellbeing.c (1,414 lines)
├── epistemic/
│   ├── nimcp_epistemic_filter.h (307 lines)
│   └── nimcp_epistemic_filter.c (690 lines)
├── emotional_tagging/
│   └── nimcp_emotional_tagging.c (628 lines)
├── executive/
│   └── nimcp_executive.c (780 lines)
├── explanations/
│   └── nimcp_explanations.c (780 lines)
├── predictive/
│   └── nimcp_predictive.c (786 lines)
├── logic/
│   └── nimcp_symbolic_logic.c (915 lines)
├── working_memory/
│   └── nimcp_working_memory.c (952 lines)
├── meta_learning/
│   └── nimcp_meta_learning.c (1,113 lines)
├── mirror_neurons/
│   └── nimcp_mirror_neurons.c (1,310 lines)
├── mental_health/
│   ├── nimcp_mental_health.c (1,163 lines)
│   ├── disorder_detectors.c (1,341 lines)
│   └── interventions.c (905 lines)
├── sleep_wake/
│   └── nimcp_sleep_wake.c (684 lines)
└── theory_of_mind/
    └── nimcp_theory_of_mind.c (739 lines)
```

## Documentation Standards

All documentation follows these principles:
- Clear purpose statements (WHAT, WHY, HOW)
- Code examples for practical use
- Integration points with other modules
- Performance characteristics
- Thread safety guarantees
- Error handling documentation

## Updates and Maintenance

Documentation created: November 11, 2025
Last reviewed: November 11, 2025
Status: Current and Complete

For updates, see:
- COGNITIVE_MODULES_INVENTORY.md - detailed changes
- COGNITIVE_QUICK_REFERENCE.md - API changes
- Git history for implementation changes

---

**Navigation:** 
- [Detailed Inventory](COGNITIVE_MODULES_INVENTORY.md)
- [Quick Reference](COGNITIVE_QUICK_REFERENCE.md)
- [Pipeline Architecture](COGNITIVE_PIPELINE_ARCHITECTURE.md)
- [Main Documentation Index](README.md)
