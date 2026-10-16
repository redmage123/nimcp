# NIMCP Code Coverage Action Plan

**Generated:** 2025-11-15 04:00 AM
**Goal:** Achieve 100% code coverage across all source files

## Current State

- **Total source files:** 149
- **Files with tests:** 121 (81.2%)
- **Files needing tests:** 28 (18.8%)
- **Test files:** 293
- **Test executables:** 240

## Known Issues to Fix First

### Failing Tests
1. `unit_epistemic_filter_tests` - FAILING
2. `unit_pink_noise_tests` - FAILING

### Hanging Tests
- Brain integration tests timeout during `brain_create_custom()`
- Need to investigate why brain creation is so slow

### Coverage Infrastructure
- gcovr crashes with "GCOV returncode was 5" on some files
- Coverage data files (.gcda) get corrupted between runs
- Need to clean and rebuild coverage data properly

## Files Needing Tests (28 total)

### Priority 1: Core Systems (3 files)
These are critical infrastructure files:

1. `core/brain/nimcp_pretrained.c`
2. `core/neuralnet/nimcp_synapse_embeddings.c`
3. `glial/astrocytes/nimcp_astrocyte_calcium.c`

### Priority 2: Cognitive Systems (14 files)
Main cognitive functionality:

4. `cognitive/mental_health_monitor.c`
5. `cognitive/autobiographical_memory/nimcp_autobiographical_memory.c`
6. `cognitive/curiosity/nimcp_curiosity_fractal.c`
7. `cognitive/curiosity/nimcp_curiosity_hyperbolic.c`
8. `cognitive/emotion_recognition/nimcp_emotion_recognition_simple.c`
9. `cognitive/empathetic_response/nimcp_empathetic_response.c`
10. `cognitive/ethics/nimcp_ethics_hyperbolic.c`
11. `cognitive/knowledge/nimcp_knowledge_fractal.c`
12. `cognitive/knowledge/nimcp_knowledge_hyperbolic.c`
13. `cognitive/mental_health/disorder_detectors.c`
14. `cognitive/mental_health/interventions.c`
15. `cognitive/self_awareness/nimcp_self_awareness_extended.c`
16. `cognitive/self_model/nimcp_self_model.c`

### Priority 3: Brain Processing (3 files)
17. `core/brain/processing/cognitive_processor.c`
18. `core/brain/processing/multimodal_integrator.c`
19. `core/brain/processing/sensory_extractor.c`

### Priority 4: Utilities (6 files)
20. `utils/error/nimcp_error_codes.c`
21. `utils/memory/nimcp_memory_guards.c`
22. `utils/metrics/nimcp_metrics.c`
23. `utils/signal/nimcp_signal_handler.c`
24. `utils/thread/nimcp_deadlock_detector.c`

### Priority 5: Specialized/Optional (4 files)
25. `bindings/nodejs/binding.c`
26. `lib/nimcp_distributed_cognition_impl.c`
27. `nlp/nimcp_multimodal_nlp_bridge.c`
28. `plasticity/neuromodulators/nimcp_neuromod_pink_noise.c`

## Step-by-Step Plan

### Phase 1: Fix Test Infrastructure (CURRENT)
- [ ] Kill all background build processes
- [ ] Fix `unit_epistemic_filter_tests`
- [ ] Fix `unit_pink_noise_tests`
- [ ] Investigate and fix brain creation timeout
- [ ] Clean and rebuild all coverage data
- [ ] Verify gcovr works on all tested files

### Phase 2: Test Priority 1 Files (Core Systems)
- [ ] Create tests for `nimcp_pretrained.c`
- [ ] Create tests for `nimcp_synapse_embeddings.c`
- [ ] Create tests for `nimcp_astrocyte_calcium.c`
- [ ] Achieve 100% coverage on each

### Phase 3: Test Priority 2 Files (Cognitive)
- [ ] One file at a time, create comprehensive tests
- [ ] Achieve 100% coverage on each before moving to next

### Phase 4: Test Priority 3 Files (Brain Processing)
- [ ] Test all 3 brain processing modules

### Phase 5: Test Priority 4 Files (Utilities)
- [ ] Test all 6 utility modules

### Phase 6: Test Priority 5 Files (Optional)
- [ ] Test remaining specialized modules

### Phase 7: Verification
- [ ] Run full test suite
- [ ] Generate final coverage report
- [ ] Verify 100% coverage achieved
- [ ] Document any intentionally untested code paths

## File Organization

### Test Files Location
- Unit tests: `test/unit/<subsystem>/test_<module>.cpp`
- Integration tests: `test/integration/<subsystem>/test_<module>_integration.cpp`
- Regression tests: `test/regression/<subsystem>/test_<module>_regression.cpp`

### Coverage Data Location
- Build artifacts: `build/`
- Coverage files: `build/**/*.gcda`, `build/**/*.gcno`
- gcov output: `build/*.gcov`

### Source Code Organization
```
src/
├── api/                    # Public API (1 file, tested)
├── bindings/              # Language bindings (1 file, untested)
├── cognitive/             # Cognitive systems (14 files need tests)
├── core/                  # Core brain/neural systems (3 files need tests)
├── glial/                 # Glial cell systems (1 file needs tests)
├── lib/                   # Implementation libraries (1 file needs tests)
├── nlp/                   # NLP systems (1 file needs tests)
├── plasticity/            # Plasticity mechanisms (1 file needs tests)
└── utils/                 # Utilities (6 files need tests)
```

## Progress Tracking

| Phase | Status | Files Complete | Coverage |
|-------|--------|----------------|----------|
| Phase 1: Infrastructure | 🔴 IN PROGRESS | 0/2 failing tests fixed | N/A |
| Phase 2: Core Systems | ⚪ PENDING | 0/3 | 0% |
| Phase 3: Cognitive | ⚪ PENDING | 0/13 | 0% |
| Phase 4: Brain Processing | ⚪ PENDING | 0/3 | 0% |
| Phase 5: Utilities | ⚪ PENDING | 0/6 | 0% |
| Phase 6: Optional | ⚪ PENDING | 0/4 | 0% |
| Phase 7: Verification | ⚪ PENDING | N/A | 0% |

## Notes

- This document will be updated as progress is made
- Each file completion should be marked with current coverage percentage
- Any blockers or issues should be documented here
- Estimated completion: Unknown (depends on test complexity and issue resolution)
