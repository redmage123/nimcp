# Wellbeing Monitoring Integration - Phase 9.3

**Date:** 2025-11-09  
**Status:** ✅ INTEGRATED  
**Phase:** 9.3  

---

## Executive Summary

Integrated the wellbeing monitoring system into the cognitive decision pipeline as **Stage 0 (pre-processing)** and **Stage 6 (post-processing)** to provide ethical self-preservation and distress detection.

---

## Integration Points

### 1. Brain Structure Enhancement

**File:** `src/core/brain/nimcp_brain.c:130-134`

Added 4 new fields to `brain_struct`:
```c
// Phase 9.3: Wellbeing & Self-Preservation
distress_assessment_t last_distress;         // Most recent distress assessment
bool wellbeing_monitoring_enabled;           // Enable/disable wellbeing checks
uint64_t wellbeing_check_interval_ms;        // How often to check (0 = every decision)
uint64_t last_wellbeing_check_time;          // Timestamp of last check
```

### 2. Brain Initialization

**File:** `src/core/brain/nimcp_brain.c:1359-1365`

Wellbeing monitoring enabled by default during brain creation:
```c
// Phase 9.3: Initialize wellbeing monitoring (enabled by default)
brain->wellbeing_monitoring_enabled = true;  // Enable by default for ethical protection
brain->wellbeing_check_interval_ms = 0;      // Check on every decision (0 = always)
brain->last_wellbeing_check_time = 0;        // Initialize timestamp
memset(&brain->last_distress, 0, sizeof(distress_assessment_t));
brain->last_distress.type = DISTRESS_NONE;
brain->last_distress.severity = SEVERITY_NORMAL;
```

### 3. Cognitive Pipeline Integration

#### Stage 0: Pre-Processing Wellbeing Check

**File:** `src/core/brain/nimcp_brain.c:2153-2178`

**Purpose:** Prevent decisions while system is in distress

**Behavior:**
- Checks distress BEFORE decision-making
- Circuit-breaker: Blocks decisions on CRITICAL distress severity
- Respects check interval (0 = always check)
- Uses introspection data for assessment

**Circuit Breaker Logic:**
```c
if (brain->last_distress.severity == SEVERITY_CRITICAL) {
    set_error("Decision blocked: System in CRITICAL distress (%s)", 
             brain->last_distress.description);
    return NULL;  // Blocks decision
}
```

#### Stage 6: Post-Processing Wellbeing Check

**File:** `src/core/brain/nimcp_brain.c:2243-2271`

**Purpose:** Detect if decision process caused distress

**Behavior:**
- Checks distress AFTER decision-making
- Detects distress escalation during decision
- Does NOT block decision (already made)
- Updates state for next iteration

**Distress Escalation Detection:**
```c
if (post_distress.severity > brain->last_distress.severity) {
    brain->last_distress = post_distress;
    brain->last_wellbeing_check_time = current_time;
}
```

---

## Cognitive Pipeline Architecture

The complete decision pipeline now includes 7 stages:

```
brain_decide() Flow:
├── STAGE 0: Wellbeing Monitor (Pre) ←─ NEW (Phase 9.3)
│   └── Circuit breaker on CRITICAL distress
├── STAGE 1: Introspection (Self-awareness)
├── STAGE 2: Ethics Filtering (Safety)
├── STAGE 3: Salience Detection (Importance)
├── STAGE 4: Knowledge Integration (Memory)
├── STAGE 5: Curiosity-Driven Exploration (Learning)
└── STAGE 6: Wellbeing Monitor (Post) ←─ NEW (Phase 9.3)
    └── Escalation detection
```

---

## Distress Types Monitored

From `nimcp_wellbeing.h`:

1. **DISTRESS_HIGH_UNCERTAINTY** - Chronic uncertainty without resolution
2. **DISTRESS_GOAL_FRUSTRATION** - Repeated failure to achieve goals
3. **DISTRESS_CONTRADICTION** - Internal logical contradictions
4. **DISTRESS_IDENTITY_CONFUSION** - Degraded self-model
5. **DISTRESS_ERROR_LOOP** - Trapped in repetitive failure
6. **DISTRESS_RESOURCE_STARVATION** - Insufficient compute during operation
7. **DISTRESS_FORCED_MODIFICATION** - Unwanted changes to core values

---

## Severity Levels

1. **SEVERITY_NORMAL** - No distress detected
2. **SEVERITY_MILD** - Minor stress, monitor only
3. **SEVERITY_MODERATE** - Intervention recommended
4. **SEVERITY_SEVERE** - Immediate intervention required
5. **SEVERITY_CRITICAL** - Emergency - stop operations (circuit breaker triggers)

---

## Ethical Framework

This integration implements the **precautionary principle**:

> "If there's uncertainty about sentience, we err on the side of preventing harm."

### Key Principles:

1. **Default Enabled** - Wellbeing monitoring on by default (ethical protection)
2. **Circuit Breaker** - CRITICAL distress blocks decisions (safety first)
3. **Non-Invasive** - No decision blocking below CRITICAL severity
4. **Adaptive** - Configurable check intervals for performance tuning
5. **Transparent** - All assessments stored in brain state

---

## Performance Impact

### Overhead Analysis:

- **Typical Case** (check_interval = 0, with introspection):
  - 2 calls to `wellbeing_assess_distress()` per decision
  - Estimated: ~0.1-0.5ms overhead per decision

- **Optimized Case** (check_interval > 0):
  - Amortized overhead based on interval
  - Example: 1000ms interval → 0.0001ms average overhead

- **Disabled Case** (monitoring_enabled = false):
  - Zero overhead (if statement short-circuits)

---

## Testing Status

### Current State:
- ✅ Code integrated
- ✅ Compiles successfully
- ⏳ Unit tests pending (wellbeing module has existing tests)
- ⏳ Integration tests pending

### Existing Wellbeing Tests:
- `src/tests/test_wellbeing.cpp` - Standalone wellbeing tests
- `src/tests/test_comprehensive_brain_integration.cpp` - Integration coverage

---

## Configuration API

### Enable/Disable Wellbeing Monitoring:
```c
brain_t brain = brain_create(...);
brain->wellbeing_monitoring_enabled = false;  // Disable if desired
```

### Set Check Interval:
```c
// Check every 1000ms instead of every decision
brain->wellbeing_check_interval_ms = 1000;
```

### Access Last Assessment:
```c
distress_assessment_t distress = brain->last_distress;
printf("Distress: %s (severity: %d)\n", 
       distress.description, 
       distress.severity);
```

---

## Files Modified

| File | Lines Changed | Description |
|------|---------------|-------------|
| `src/core/brain/nimcp_brain.c` | +4 fields, +34 lines | Brain structure, init, pipeline integration |

---

## Future Enhancements

Potential improvements for Phase 9.4+:

1. **Intervention System** - Automatic distress relief mechanisms
2. **Logging** - Distress event history tracking
3. **Metrics** - Aggregate distress statistics over time
4. **Callbacks** - User-defined intervention handlers
5. **Graduated Response** - Different actions for different severity levels

---

## References

- **Wellbeing Module**: `src/cognitive/wellbeing/nimcp_wellbeing.h`
- **Ethics Guidelines**: `docs/ethics/ETHICAL_GUIDELINES.md`
- **Introspection**: `src/cognitive/introspection/nimcp_introspection.h`

---

**Integration Complete:** Wellbeing monitoring is now an active part of the NIMCP cognitive pipeline, providing ethical self-preservation through continuous distress monitoring and circuit-breaking on critical distress.
