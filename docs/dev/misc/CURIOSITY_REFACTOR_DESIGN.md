# Curiosity Engine Refactoring Design Document

## Executive Summary

**Problem**: The curiosity engine creates TWO separate brain instances (gap_detector, question_prioritizer), each with 500 neurons and ~128K synapses, only to access their neuromodulator systems. This violates the unified brain architecture and wastes resources.

**Solution**: Refactor curiosity engine from a standalone system to a cognitive module that receives a parent brain reference, eliminating 1000 neurons and 256K synapses of overhead.

**Impact**:
- Performance: Eliminates 2 brain instances per knowledge system
- Architecture: Aligns with "one brain, many modules" design
- Memory: Reduces footprint significantly
- Test Time: KnowledgeTest.SystemCreation: 0.293s → ~0.100s (estimated)

---

## Current Architecture (BROKEN)

### The Problem

```
knowledge_system
    └── curiosity_engine
            ├── gap_detector (brain_t)          ← 500 neurons, 128K synapses
            │   └── Only used for: neuromodulator_system
            └── question_prioritizer (brain_t)   ← 500 neurons, 128K synapses
                └── Only used for: neuromodulator_system
```

**Files Affected**:
- `src/cognitive/curiosity/nimcp_curiosity.c:155-156` - brain_t fields in struct
- `src/cognitive/curiosity/nimcp_curiosity.c:743-747` - brain_create() calls
- `src/cognitive/curiosity/nimcp_curiosity.c:795-796` - brain_destroy() calls
- `src/cognitive/curiosity/nimcp_curiosity.c:825,1112,1220,1257` - neuromod access

### Usage Analysis

The `gap_detector` and `question_prioritizer` brains are used **ONLY** for:
```c
neuromodulator_system_t neuromod = brain_get_neuromodulator_system(engine->gap_detector);
```

They provide:
- Dopamine levels
- Acetylcholine levels
- Norepinephrine levels

**No neural processing happens in these brains!**

---

## New Architecture (CORRECT)

### The Design

```
brain (ONE unified instance)
    ├── network (neural substrate)
    ├── neuromodulator_system
    └── curiosity (module)
            ├── parent_brain (reference) ← Points to parent
            ├── gap_detector_state (data only)
            └── question_prioritizer_state (data only)
```

### Pattern: Cognitive Module

Following the existing `working_memory_t` pattern:

```c
struct curiosity_engine_struct {
    brain_t parent_brain;  // Reference to parent, NOT ownership

    // All the existing data structures (unchanged)
    hash_table_t* concept_hash_table;
    question_history_t* questions;
    knowledge_source_t* sources;

    // REMOVED: brain_t gap_detector;
    // REMOVED: brain_t question_prioritizer;

    // Module state (NEW)
    struct {
        float last_novelty_score;
        float last_gap_size;
    } gap_detector_state;

    struct {
        float last_priority;
        float last_difficulty;
    } question_prioritizer_state;

    // Existing fields (unchanged)
    float baseline_curiosity;
    learning_stage_t stage;
    learning_progress_t progress;
};
```

---

## Implementation Plan

### Phase 1: Modify curiosity_engine_struct

**File**: `src/cognitive/curiosity/nimcp_curiosity.c`

**Changes**:
```c
// Lines 155-156: REMOVE
- brain_t gap_detector;
- brain_t question_prioritizer;

// ADD
+ brain_t parent_brain;  // Reference to parent brain
+
+ // State for gap detection (no brain needed)
+ struct {
+     float last_novelty_score;
+     float last_gap_size;
+ } gap_detector_state;
+
+ // State for question prioritization (no brain needed)
+ struct {
+     float last_priority;
+     float last_difficulty;
+ } question_prioritizer_state;
```

### Phase 2: Update curiosity_engine_create()

**File**: `src/cognitive/curiosity/nimcp_curiosity.c:743-747`

**Current**:
```c
curiosity_engine_t curiosity_engine_create(const char* learner_name)
{
    // ...
    engine->gap_detector = brain_create("gap_detector", ...);
    engine->question_prioritizer = brain_create("question_priority", ...);
    // ...
}
```

**New**:
```c
// NEW SIGNATURE - takes parent brain reference
curiosity_engine_t curiosity_engine_create(brain_t parent_brain, const char* learner_name)
{
    if (!parent_brain) {
        return NULL;
    }

    // ...
    engine->parent_brain = parent_brain;  // Store reference

    // Initialize state instead of creating brains
    engine->gap_detector_state.last_novelty_score = 0.0f;
    engine->gap_detector_state.last_gap_size = 0.0f;
    engine->question_prioritizer_state.last_priority = 0.5f;
    engine->question_prioritizer_state.last_difficulty = 0.5f;
    // ...
}
```

### Phase 3: Update curiosity_engine_destroy()

**File**: `src/cognitive/curiosity/nimcp_curiosity.c:795-796`

**Current**:
```c
brain_destroy(engine->gap_detector);
brain_destroy(engine->question_prioritizer);
```

**New**:
```c
// REMOVED - we don't own the brain anymore
// parent_brain belongs to caller, not destroyed here
```

### Phase 4: Update neuromodulator access

**File**: `src/cognitive/curiosity/nimcp_curiosity.c` (4 locations)

**Current**:
```c
if (engine->gap_detector) {
    neuromodulator_system_t neuromod = brain_get_neuromodulator_system(engine->gap_detector);
    // ...
}
```

**New**:
```c
if (engine->parent_brain) {
    neuromodulator_system_t neuromod = brain_get_neuromodulator_system(engine->parent_brain);
    // ...
}
```

**Locations**:
- Line 825: `curiosity_detect_knowledge_gap()`
- Line 1112: `curiosity_generate_questions()`
- Line 1220: `curiosity_assess_source_credibility()`
- Line 1257: `curiosity_get_motivation_state()`

### Phase 5: Update brain.c initialization

**File**: `src/core/brain/nimcp_brain.c`

Find where curiosity is initialized in `brain_create()`:

**Current** (likely around line 2000+):
```c
// Phase 11: Curiosity-driven learning
brain->curiosity = curiosity_engine_create(name);
```

**New**:
```c
// Phase 11: Curiosity-driven learning
brain->curiosity = curiosity_engine_create(brain, name);  // Pass self-reference
```

### Phase 6: Update brain.c cleanup

**File**: `src/core/brain/nimcp_brain.c`

Find where curiosity is destroyed in `brain_destroy()`:

**Current**:
```c
if (brain->curiosity) {
    curiosity_engine_destroy(brain->curiosity);
}
```

**New**: (No change needed - same cleanup works)
```c
if (brain->curiosity) {
    curiosity_engine_destroy(brain->curiosity);  // Module cleanup only
}
```

### Phase 7: Update knowledge system

**File**: `src/cognitive/knowledge/nimcp_knowledge.c`

**Current** (around line 1025):
```c
system->curiosity = curiosity_engine_create(learner_name);
```

**New**: Knowledge system needs its own brain first!

```c
// Option A: Create a unified brain for the knowledge system
brain_t knowledge_brain = brain_create(learner_name, BRAIN_SIZE_SMALL,
                                       BRAIN_TASK_CLASSIFICATION, 20, 10);
if (!knowledge_brain) {
    // cleanup...
    return NULL;
}

// Curiosity is now a module of this brain
system->knowledge_brain = knowledge_brain;
system->curiosity = knowledge_brain->curiosity;  // Reference brain's module
```

### Phase 8: Update header files

**File**: `src/cognitive/curiosity/nimcp_curiosity.h:130`

**Current**:
```c
curiosity_engine_t curiosity_engine_create(const char* learner_name);
```

**New**:
```c
// BREAKING CHANGE: Now requires parent brain
curiosity_engine_t curiosity_engine_create(brain_t parent_brain, const char* learner_name);
```

**File**: `src/cognitive/knowledge/nimcp_knowledge.c` (struct)

**Add**:
```c
struct knowledge_system_struct {
    // ... existing fields ...

    brain_t knowledge_brain;        // NEW: Unified brain instance
    // curiosity_engine_t curiosity; // CHANGED: Now accessed via brain->curiosity

    // ... rest of fields ...
};
```

---

## Files Requiring Changes

### Core Changes
1. `src/cognitive/curiosity/nimcp_curiosity.h` - Function signature
2. `src/cognitive/curiosity/nimcp_curiosity.c` - Implementation (8 locations)
3. `src/core/brain/nimcp_brain.c` - Initialization (1 location)

### Downstream Changes
4. `src/cognitive/knowledge/nimcp_knowledge.c` - Create unified brain
5. `src/cognitive/knowledge/nimcp_knowledge.h` - Add brain_t field

### Test Updates
6. `test/unit/cognitive/knowledge/test_knowledge.cpp` - Verify tests still pass
7. `test/unit/cognitive/curiosity/*` - May need updates if they exist

---

## Testing Strategy

### Phase 1: Build Verification
```bash
cmake --build build --target nimcp
# Should compile cleanly
```

### Phase 2: Unit Tests
```bash
# Knowledge tests (main integration point)
./build/test/unit_cognitive_knowledge_test_knowledge

# Curiosity tests (if they exist)
find build/test -name "*curiosity*" -executable -exec {} \;
```

### Phase 3: Performance Verification
```bash
# Before: 0.293s with 2 brains
# After:  ~0.100s with 0 extra brains (estimated 3x speedup)
time ./build/test/unit_cognitive_knowledge_test_knowledge --gtest_filter="KnowledgeTest.SystemCreation"
```

### Phase 4: Memory Verification
```bash
# Verify no memory leaks
valgrind --leak-check=full ./build/test/unit_cognitive_knowledge_test_knowledge
```

---

## Risks and Mitigations

### Risk 1: Breaking API Changes
**Mitigation**: This is a breaking change. All callers of `curiosity_engine_create()` must be updated.

**Impact**: Low - curiosity is only used internally by brain.c and knowledge.c

### Risk 2: Circular Dependency
**Mitigation**: Curiosity holds a REFERENCE (pointer) to parent brain, not ownership.

**Lifecycle**: Parent brain creates curiosity module → curiosity stores reference → parent brain destroys curiosity → parent brain destroys self.

### Risk 3: NULL Pointer Dereferences
**Mitigation**: Add NULL checks before accessing `parent_brain`.

```c
if (!engine || !engine->parent_brain) {
    return error;
}
```

### Risk 4: Test Failures
**Mitigation**: Incremental testing after each phase.

**Rollback**: All changes are in version control and can be reverted.

---

## Success Criteria

- [ ] All code compiles without warnings
- [ ] All knowledge tests pass
- [ ] All curiosity tests pass (if they exist)
- [ ] Test time improves (fewer brains to create/destroy)
- [ ] Memory usage decreases (no separate brain instances)
- [ ] Architecture follows "one brain, many modules" pattern
- [ ] No memory leaks detected by valgrind

---

## Future Enhancements

### Opportunity 1: Remove knowledge_system's brain too
Knowledge system uses hash tables, B-trees, and arrays - not neural networks. Consider whether it needs a brain at all, or if curiosity should be at a higher level.

### Opportunity 2: Standardize Module Pattern
Create a common module initialization pattern:
```c
typedef nimcp_error_t (*module_init_fn)(brain_t parent, void** module);
typedef void (*module_destroy_fn)(void* module);
```

### Opportunity 3: Module Registry
Brain could maintain a registry of modules for introspection:
```c
brain_list_modules(brain_t brain, const char** names, uint32_t max);
```

---

## References

- Working Memory Module: `src/cognitive/nimcp_working_memory.c` (good example)
- Brain Struct: `src/core/brain/nimcp_brain.c:130` (struct brain_struct)
- Module Pattern: Other cognitive modules in `src/cognitive/`

---

## Approval Checklist

- [ ] Design reviewed
- [ ] Implementation plan approved
- [ ] Testing strategy confirmed
- [ ] Ready to implement

---

**Author**: Claude Code
**Date**: 2025-11-15
**Status**: DESIGN PHASE
