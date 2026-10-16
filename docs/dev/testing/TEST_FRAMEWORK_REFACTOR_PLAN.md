# Test Framework Refactor Plan

## Overview

Transform NIMCP's testing infrastructure into a fully automated, self-healing system where the Code Surgeon orchestrates all testing, debugging, and fixing with 100% code coverage.

## Goals

1. ✅ **Unified Test Structure** - Single `test/` directory at root level
2. ✅ **Automated Test-Fix Loop** - Tests → Failures → Debug → Fix → Retest → Iterate
3. ✅ **100% Code Coverage** - Unit, Integration, E2E, Regression, Lint
4. ✅ **Code Surgeon as Orchestrator** - Central test automation system
5. ✅ **Claude Integration** - AI-powered automatic fixing
6. ✅ **Modular Architecture** - Clean separation of concerns

## Current State

**Problems:**
- Tests scattered in `src/tests/` (mixed with source code)
- Manual debugging workflow (ad-hoc tools)
- No automated fix loop
- Incomplete coverage (~60-70%)
- Lint errors not automatically fixed
- No integration between testing and debugging tools

**Existing Assets:**
- `debug_suite.py` - Debugging tool suite (845 lines)
- `src/tests/` - 40+ test files (unit, integration, cognitive, etc.)
- `src/fuzz/` - 8 fuzzing targets
- GTest framework
- Valgrind, GDB, rr integration

## New Architecture

### Directory Structure

```
/home/bbrelin/nimcp/
├── src/                          # Source code only (no tests)
│   ├── core/
│   ├── cognitive/
│   ├── lib/
│   └── ...
│
├── test/                         # NEW: All tests here
│   ├── unit/                     # Unit tests (per module)
│   │   ├── core/
│   │   │   ├── test_brain.cpp
│   │   │   ├── test_neuron.cpp
│   │   │   └── ...
│   │   ├── cognitive/
│   │   ├── utils/
│   │   └── CMakeLists.txt
│   │
│   ├── integration/              # Integration tests (cross-module)
│   │   ├── test_brain_with_glial.cpp
│   │   ├── test_cognitive_pipeline.cpp
│   │   └── CMakeLists.txt
│   │
│   ├── e2e/                      # End-to-end tests (full system)
│   │   ├── test_full_brain_lifecycle.cpp
│   │   ├── test_multimodal_processing.cpp
│   │   └── CMakeLists.txt
│   │
│   ├── regression/               # Regression tests (bug reproduction)
│   │   ├── test_btree_double_free.cpp
│   │   ├── test_btree_count_mismatch.cpp
│   │   └── CMakeLists.txt
│   │
│   ├── fuzz/                     # Fuzzing targets (moved from src/fuzz/)
│   │   ├── fuzz_btree.cpp
│   │   ├── fuzz_brain_serialization.cpp
│   │   └── CMakeLists.txt
│   │
│   ├── fixtures/                 # Test data and fixtures
│   │   ├── sample_brains/
│   │   ├── test_inputs/
│   │   └── expected_outputs/
│   │
│   ├── mocks/                    # Mock objects
│   │   ├── mock_neuron.hpp
│   │   └── mock_synapse.hpp
│   │
│   ├── utils/                    # Test utilities
│   │   ├── test_helpers.cpp
│   │   ├── assertions.cpp
│   │   └── comparators.cpp
│   │
│   └── CMakeLists.txt           # Top-level test build
│
├── tools/                        # Development tools
│   ├── code_surgeon/            # NEW: Test orchestrator
│   │   ├── code_surgeon.py      # Main orchestrator
│   │   ├── test_runner.py       # Test execution
│   │   ├── failure_analyzer.py  # Failure analysis
│   │   ├── auto_fixer.py        # Automatic fixing with Claude
│   │   ├── coverage_analyzer.py # Coverage analysis
│   │   ├── lint_runner.py       # Lint checking/fixing
│   │   ├── debug_tools.py       # Valgrind, GDB, rr integration
│   │   ├── claude_client.py     # Claude API integration
│   │   └── config.yaml          # Configuration
│   │
│   └── scripts/                 # Helper scripts
│       ├── migrate_tests.py     # Migration script
│       └── generate_coverage.sh # Coverage report generator
│
├── .code_surgeon/               # Code Surgeon state
│   ├── test_results/            # Test run history
│   ├── fixes_applied/           # Applied fix history
│   ├── coverage_reports/        # Coverage reports
│   └── lint_reports/            # Lint reports
│
└── ...
```

### Code Surgeon Architecture

```
┌─────────────────────────────────────────────────────────┐
│           CODE SURGEON (Test Orchestrator)              │
├─────────────────────────────────────────────────────────┤
│                                                         │
│  ┌──────────────┐    ┌──────────────┐    ┌──────────┐ │
│  │ Test Runner  │───▶│ Coverage     │───▶│ Lint     │ │
│  │              │    │ Analyzer     │    │ Runner   │ │
│  └──────┬───────┘    └──────────────┘    └────┬─────┘ │
│         │                                      │       │
│         ▼                                      ▼       │
│  ┌──────────────┐                      ┌──────────┐   │
│  │ Failure      │◀────────────────────▶│ Auto     │   │
│  │ Analyzer     │                      │ Fixer    │   │
│  └──────┬───────┘                      └────┬─────┘   │
│         │                                   │         │
│         ▼                                   ▼         │
│  ┌──────────────┐                   ┌──────────────┐ │
│  │ Debug Tools  │                   │ Claude API   │ │
│  │ (GDB/rr/etc) │                   │ Integration  │ │
│  └──────────────┘                   └──────────────┘ │
│                                                       │
└───────────────────────────────────────────────────────┘
```

## Automated Test-Fix Loop

### Workflow

```
1. TEST DISCOVERY
   ├─ Scan test/ directory
   ├─ Identify all test executables
   └─ Group by category (unit, integration, e2e, regression)

2. TEST EXECUTION
   ├─ Run all tests
   ├─ Capture results (pass/fail/crash/timeout)
   ├─ Collect output and error messages
   └─ Log execution time

3. FAILURE ANALYSIS (if any failures)
   ├─ Categorize failure type:
   │  ├─ Assertion failure
   │  ├─ Crash (segfault, abort)
   │  ├─ Timeout
   │  ├─ Memory error (Valgrind)
   │  └─ Undefined behavior
   │
   ├─ Run appropriate debugging tool:
   │  ├─ Memory errors → Valgrind
   │  ├─ Crashes → GDB backtrace
   │  ├─ Race conditions → ThreadSanitizer
   │  └─ Deadlocks → Helgrind
   │
   └─ Extract debugging info:
      ├─ Stack traces
      ├─ Variable values
      ├─ Memory state
      └─ Error messages

4. AUTO-FIXING (Claude Integration)
   ├─ Prepare context for Claude:
   │  ├─ Failure description
   │  ├─ Stack trace
   │  ├─ Relevant source files
   │  ├─ Test code
   │  └─ Debugging output
   │
   ├─ Send to Claude API:
   │  ├─ Request: "Fix this test failure"
   │  ├─ Include full context
   │  └─ Specify constraints (coding standards, etc.)
   │
   ├─ Receive fix from Claude:
   │  ├─ Code changes (diffs)
   │  ├─ Explanation
   │  └─ Confidence level
   │
   └─ Apply fix:
      ├─ Validate syntax
      ├─ Apply changes to source
      ├─ Commit with message
      └─ Log fix in .code_surgeon/fixes_applied/

5. REBUILD & RETEST
   ├─ Rebuild affected targets
   ├─ Rerun failed tests
   └─ Check if fixed

6. ITERATE
   ├─ If still failing → Go back to step 3
   ├─ If passing → Continue to next failure
   └─ If max iterations reached → Report to user

7. COVERAGE ANALYSIS
   ├─ Run tests with coverage enabled
   ├─ Generate coverage report
   ├─ Identify uncovered code
   └─ If < 100%:
      ├─ Generate new tests (via Claude)
      ├─ Add to test suite
      └─ Rerun coverage

8. LINT CHECKING
   ├─ Run clang-tidy on all source
   ├─ Run cppcheck
   ├─ Identify issues
   └─ If issues found:
      ├─ Auto-fix with clang-tidy --fix
      ├─ Or request fix from Claude
      └─ Rerun lint

9. FINAL REPORT
   ├─ All tests passing: ✅
   ├─ 100% coverage: ✅
   ├─ No lint errors: ✅
   ├─ Summary of fixes applied
   └─ Time taken
```

### Example Session

```bash
$ ./tools/code_surgeon/code_surgeon.py --mode full --target 100%

Code Surgeon v2.0 - Automated Test Orchestration
================================================

[DISCOVERY] Found 156 tests across 5 categories:
  - Unit tests: 89
  - Integration tests: 34
  - E2E tests: 12
  - Regression tests: 13
  - Fuzz tests: 8

[EXECUTION] Running 156 tests...
  ✅ Unit tests: 87/89 passing (2 failures)
  ✅ Integration tests: 34/34 passing
  ✅ E2E tests: 11/12 passing (1 failure)
  ✅ Regression tests: 13/13 passing
  ✅ Fuzz tests: 8/8 passing

[FAILURE ANALYSIS] Analyzing 3 failures...

  Failure 1: test_btree_stress
  ├─ Type: Assertion failure
  ├─ Message: "Expected 500, got 508"
  ├─ Running Valgrind... Found: Double-free in predecessor removal
  └─ Stack trace: nimcp_btree.c:502

  Failure 2: test_working_memory_decay
  ├─ Type: Assertion failure
  ├─ Message: "Expected 0.5, got 0.7"
  └─ Running GDB... Found: Incorrect decay calculation

  Failure 3: test_multimodal_integration
  ├─ Type: Crash (SIGSEGV)
  ├─ Running GDB... Found: NULL pointer dereference
  └─ Stack trace: multimodal_integrator.c:234

[AUTO-FIXING] Requesting fixes from Claude...

  Fix 1: test_btree_stress
  ├─ Claude analysis: "should_free_key parameter not passed correctly"
  ├─ Applied fix: nimcp_btree.c:502
  ├─ Rebuilding...
  └─ Retesting... ✅ PASS

  Fix 2: test_working_memory_decay
  ├─ Claude analysis: "Decay rate should be exponential, not linear"
  ├─ Applied fix: nimcp_working_memory.c:145
  ├─ Rebuilding...
  └─ Retesting... ✅ PASS

  Fix 3: test_multimodal_integration
  ├─ Claude analysis: "Missing NULL check before dereferencing audio_cortex"
  ├─ Applied fix: multimodal_integrator.c:230
  ├─ Rebuilding...
  └─ Retesting... ✅ PASS

[COVERAGE] Analyzing code coverage...
  Current coverage: 87%
  Target: 100%
  Gap: 13%

[COVERAGE] Generating tests for uncovered code...
  ├─ Requesting test generation from Claude...
  ├─ Generated 23 new tests
  ├─ Adding to test suite...
  └─ Rerunning coverage... 98%

[COVERAGE] Iteration 2...
  ├─ Generated 8 more tests
  └─ Rerunning coverage... 100% ✅

[LINT] Running lint checks...
  ├─ clang-tidy: 45 issues found
  ├─ Auto-fixing 32 issues...
  ├─ Requesting Claude fixes for 13 complex issues...
  └─ All fixed ✅

[SUMMARY]
========================================
✅ All 179 tests passing (23 new tests added)
✅ 100% code coverage achieved
✅ 0 lint errors
✅ 5 bugs fixed automatically
✅ 23 new tests generated
========================================
Time: 47 minutes
Fixes applied: 5 (see .code_surgeon/fixes_applied/)
Coverage report: .code_surgeon/coverage_reports/2025-01-10.html
```

## Implementation Phases

### Phase 1: Directory Structure (Week 1)
- [ ] Create `test/` directory structure
- [ ] Create `tools/code_surgeon/` directory
- [ ] Migrate existing tests from `src/tests/` to `test/`
- [ ] Update CMakeLists.txt for new structure
- [ ] Verify all tests still build and run

### Phase 2: Code Surgeon Core (Week 2)
- [ ] Refactor `debug_suite.py` → `code_surgeon.py`
- [ ] Implement `test_runner.py` (test discovery & execution)
- [ ] Implement `failure_analyzer.py` (failure categorization)
- [ ] Integrate existing debug tools (Valgrind, GDB, rr)
- [ ] Create test result logging

### Phase 3: Claude Integration (Week 2-3)
- [ ] Implement `claude_client.py` (API wrapper)
- [ ] Implement `auto_fixer.py` (fix generation & application)
- [ ] Create fix validation system
- [ ] Add fix logging and history
- [ ] Test auto-fix loop with sample failures

### Phase 4: Coverage Analysis (Week 3)
- [ ] Implement `coverage_analyzer.py`
- [ ] Integrate gcov/lcov
- [ ] Generate HTML coverage reports
- [ ] Identify uncovered code
- [ ] Generate tests for uncovered code (via Claude)

### Phase 5: Lint Integration (Week 3-4)
- [ ] Implement `lint_runner.py`
- [ ] Integrate clang-tidy
- [ ] Integrate cppcheck
- [ ] Auto-fix simple lint issues
- [ ] Claude-fix complex lint issues

### Phase 6: Test Suite Expansion (Week 4-5)
- [ ] Achieve 100% unit test coverage
- [ ] Add comprehensive integration tests
- [ ] Add E2E tests for all major workflows
- [ ] Add regression tests for all known bugs
- [ ] Modularize all tests

### Phase 7: Polish & Documentation (Week 5)
- [ ] Add comprehensive logging
- [ ] Create user documentation
- [ ] Add configuration system
- [ ] Performance optimization
- [ ] Final testing

## Configuration System

### config.yaml

```yaml
code_surgeon:
  # Test execution
  test_discovery:
    test_root: "./test"
    exclude_patterns:
      - "*.backup"
      - "*~"

  test_execution:
    timeout_per_test: 300  # 5 minutes
    parallel_jobs: 8
    repeat_failed_tests: 3

  # Auto-fixing
  auto_fix:
    enabled: true
    max_iterations: 5
    claude_api_key: "${ANTHROPIC_API_KEY}"
    claude_model: "claude-sonnet-4"
    max_context_size: 50000

  # Coverage
  coverage:
    target: 100
    exclude_dirs:
      - "test/"
      - "external/"

  # Lint
  lint:
    enabled: true
    auto_fix: true
    tools:
      - clang-tidy
      - cppcheck

  # Debug tools
  debug:
    valgrind_enabled: true
    gdb_enabled: true
    rr_enabled: true
    sanitizers:
      - address
      - undefined
      - thread

  # Reporting
  reports:
    output_dir: ".code_surgeon"
    generate_html: true
    save_history: true
```

## Benefits

### For Developers
- ✅ **Zero Manual Debugging** - Code Surgeon handles everything
- ✅ **Instant Feedback** - Tests run and fix automatically
- ✅ **100% Coverage Guarantee** - No code goes untested
- ✅ **Lint-Free Code** - Automatic style compliance
- ✅ **Regression Protection** - Every bug becomes a test

### For Code Quality
- ✅ **Higher Reliability** - More tests = fewer bugs
- ✅ **Faster Development** - No time wasted on debugging
- ✅ **Better Architecture** - Modular tests enforce modularity
- ✅ **Documentation via Tests** - Tests show how to use APIs
- ✅ **Continuous Improvement** - Test suite grows automatically

### For CI/CD
- ✅ **Automated PR Testing** - Code Surgeon validates all PRs
- ✅ **Pre-Merge Fixes** - Bugs fixed before merge
- ✅ **Coverage Enforcement** - PRs rejected if coverage drops
- ✅ **Lint Enforcement** - PRs rejected if lint fails
- ✅ **Regression Prevention** - All bugs become tests

## Risk Mitigation

### Risks
1. **Claude API Costs** - Many API calls for fixing
   - Mitigation: Cache fixes, batch requests, use cheaper models for simple fixes

2. **False Fixes** - Claude might introduce bugs
   - Mitigation: Validate all fixes, require tests to pass, review complex fixes

3. **Coverage Gaming** - Tests that don't actually test
   - Mitigation: Review generated tests, require meaningful assertions

4. **Migration Complexity** - Moving 40+ test files
   - Mitigation: Phased migration, automated migration script

5. **Performance** - Running all tests + coverage + lint takes time
   - Mitigation: Parallel execution, smart test selection, caching

## Success Metrics

- ✅ **100% Code Coverage** - All lines covered by tests
- ✅ **0 Lint Errors** - Perfect code style
- ✅ **< 1 min Test Run** - Fast feedback loop (with caching)
- ✅ **> 95% Auto-Fix Success** - Most bugs fixed automatically
- ✅ **0 Regressions** - Every bug has a test

## Next Steps

**Approval needed before proceeding:**

1. ✅ Directory structure looks good?
2. ✅ Code Surgeon architecture makes sense?
3. ✅ Auto-fix loop is acceptable?
4. ✅ 100% coverage is the right goal?
5. ✅ Timeline (5 weeks) is reasonable?

**After approval, I will:**

1. Create the new `test/` directory structure
2. Start migrating tests from `src/tests/`
3. Refactor `debug_suite.py` into `code_surgeon.py`
4. Implement the test runner
5. Add Claude API integration
6. Begin the auto-fix loop implementation

**Questions for you:**

1. Do you have an Anthropic API key for Claude integration?
2. Which test category should we prioritize first (unit/integration/e2e)?
3. Should Code Surgeon run automatically on every commit, or manual trigger?
4. Any specific lint rules or coding standards to enforce?
5. Should we keep the fuzzing tests separate or integrate them fully?
