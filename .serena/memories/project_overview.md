# NIMCP Project Overview

## Purpose
NIMCP (Neural Information Management and Cognitive Processing) is a biologically-inspired AI framework written in C with C++ testing.

## Tech Stack
- **Language**: C (core library), C++ (tests)
- **Build System**: CMake
- **Testing**: Google Test (GTest)
- **Platform**: Linux

## Key Modules
- **Fault Tolerance**: Circuit breaker, retry with backoff, graceful degradation, health agents
- **Security**: Blood-Brain Barrier (BBB), access control, anomaly detection
- **Brain Regions**: Hippocampus, prefrontal cortex, amygdala, etc.
- **Neural Networks**: SNN, LNN, CNN
- **Memory**: Unified memory management with tracking

## Project Path
`/home/bbrelin/nimcp`

## E2E Test Patterns
- Use `e2e_test_framework.h` with E2E_PIPELINE_START, E2E_STAGE_BEGIN, etc.
- Tests go in `test/e2e/<category>/`
- CMakeLists.txt uses macro `add_e2e_fault_tolerance_test()` pattern
