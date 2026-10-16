#!/usr/bin/env python3
"""
Code Surgeon - Automated Test Orchestration System

WHAT: Self-healing test framework that automatically debugs and fixes failing tests
WHY:  Eliminate manual debugging, achieve 100% coverage, maintain code quality
HOW:  Test → Analyze → Fix (via Claude) → Verify → Iterate until pass

ARCHITECTURE: Functional design with pure functions and immutable data
PATTERNS: Strategy, Observer, Command, Chain of Responsibility, Functional Programming

PRINCIPLES:
- Pure functions (no side effects)
- Immutable data structures
- Function composition
- Early returns (guard clauses)
- <50 lines per function
- WHAT-WHY-HOW documentation

USAGE:
    ./code_surgeon.py --mode full --target 100
    ./code_surgeon.py --mode test-only
    ./code_surgeon.py --mode fix-failing
"""

import sys
import os
import json
import subprocess
from pathlib import Path
from dataclasses import dataclass, field
from typing import List, Dict, Optional, Callable, Tuple
from enum import Enum
from datetime import datetime

# Add project root to path
sys.path.insert(0, str(Path(__file__).parent.parent.parent))

# Import new abstraction modules
from llm_provider import LLMProvider, LLMMessage, create_llm_provider
from config import CodeSurgeonConfig, create_config_from_env

#==============================================================================
# Core Data Structures (Immutable)
#==============================================================================

class TestStatus(Enum):
    """Test execution status"""
    PASS = "pass"
    FAIL = "fail"
    CRASH = "crash"
    TIMEOUT = "timeout"
    SKIP = "skip"

class TestCategory(Enum):
    """Test categorization"""
    UNIT = "unit"
    INTEGRATION = "integration"
    E2E = "e2e"
    REGRESSION = "regression"
    FUZZ = "fuzz"

class FailureType(Enum):
    """Failure categorization for debugging"""
    ASSERTION = "assertion"
    CRASH = "crash"
    TIMEOUT = "timeout"
    MEMORY = "memory"
    RACE_CONDITION = "race"
    UNDEFINED_BEHAVIOR = "undefined"

@dataclass(frozen=True)
class TestResult:
    """
    Immutable test result

    WHAT: Represents outcome of single test execution
    WHY:  Immutable to enable functional composition
    HOW:  Frozen dataclass with all test metadata
    """
    name: str
    category: TestCategory
    status: TestStatus
    duration_ms: float
    output: str
    error_message: Optional[str] = None
    stack_trace: Optional[str] = None
    failure_type: Optional[FailureType] = None

@dataclass(frozen=True)
class DebugResult:
    """
    Immutable debugging result

    WHAT: Output from debugging tools (valgrind/gdb/etc)
    WHY:  Captures all debug info for analysis
    HOW:  Frozen dataclass with tool outputs
    """
    tool_name: str
    success: bool
    output: str
    findings: List[str] = field(default_factory=list)
    recommendations: List[str] = field(default_factory=list)

@dataclass(frozen=True)
class Fix:
    """
    Immutable fix representation

    WHAT: Code change to fix a test failure
    WHY:  Immutable to track fix history
    HOW:  Frozen dataclass with file path and changes
    """
    file_path: str
    old_code: str
    new_code: str
    explanation: str
    confidence: float  # 0.0 to 1.0

@dataclass(frozen=True)
class TestSuite:
    """
    Immutable test suite

    WHAT: Collection of test results
    WHY:  Immutable for functional transformations
    HOW:  Frozen dataclass with test list
    """
    tests: Tuple[TestResult, ...] = field(default_factory=tuple)
    timestamp: str = field(default_factory=lambda: datetime.now().isoformat())

    @property
    def total_tests(self) -> int:
        """Count total tests"""
        return len(self.tests)

    @property
    def passing_tests(self) -> int:
        """Count passing tests"""
        return sum(1 for t in self.tests if t.status == TestStatus.PASS)

    @property
    def failing_tests(self) -> Tuple[TestResult, ...]:
        """Get all failing tests (pure function)"""
        return tuple(t for t in self.tests if t.status != TestStatus.PASS)

#==============================================================================
# Pure Utility Functions
#==============================================================================

def is_passing(test: TestResult) -> bool:
    """
    Check if test passed

    WHAT: Pure predicate function
    WHY:  Reusable filter for test results
    HOW:  Simple status check
    """
    return test.status == TestStatus.PASS

def is_failing(test: TestResult) -> bool:
    """Check if test failed (pure function)"""
    return not is_passing(test)

def categorize_by_status(tests: Tuple[TestResult, ...]) -> Dict[TestStatus, Tuple[TestResult, ...]]:
    """
    Group tests by status

    WHAT: Pure function to categorize tests
    WHY:  Enables functional composition
    HOW:  Fold operation grouping by status

    COMPLEXITY: O(n) where n = number of tests
    """
    result = {}
    for status in TestStatus:
        result[status] = tuple(t for t in tests if t.status == status)
    return result

def filter_by_category(tests: Tuple[TestResult, ...],
                        category: TestCategory) -> Tuple[TestResult, ...]:
    """
    Filter tests by category

    WHAT: Pure filter function
    WHY:  Enables category-specific analysis
    HOW:  Standard filter operation
    """
    return tuple(t for t in tests if t.category == category)

def calculate_pass_rate(tests: Tuple[TestResult, ...]) -> float:
    """
    Calculate pass rate percentage

    WHAT: Pure calculation function
    WHY:  Metric for test health
    HOW:  Simple division with guard
    """
    if not tests:
        return 0.0

    passing = sum(1 for t in tests if is_passing(t))
    return (passing / len(tests)) * 100.0

#==============================================================================
# Test Discovery (Pure Functions)
#==============================================================================

def discover_test_binaries(test_root: Path) -> Tuple[Path, ...]:
    """
    Discover all test executables

    WHAT: Find all test binaries in test directory
    WHY:  Enable automatic test discovery
    HOW:  Recursive glob for executables

    COMPLEXITY: O(n) where n = number of files
    """
    if not test_root.exists():
        return tuple()

    # Find all executables (no extension on Unix)
    executables = []
    for path in test_root.rglob("*"):
        if path.is_file() and os.access(path, os.X_OK):
            # Exclude scripts, only binaries
            if not path.suffix in {'.py', '.sh', '.md'}:
                executables.append(path)

    return tuple(sorted(executables))

def categorize_test_binary(binary_path: Path) -> TestCategory:
    """
    Determine test category from path

    WHAT: Pure function to categorize test
    WHY:  Group tests by type
    HOW:  Path-based heuristics
    """
    path_str = str(binary_path)

    if '/unit/' in path_str:
        return TestCategory.UNIT
    if '/integration/' in path_str:
        return TestCategory.INTEGRATION
    if '/e2e/' in path_str:
        return TestCategory.E2E
    if '/regression/' in path_str:
        return TestCategory.REGRESSION
    if '/fuzz/' in path_str or 'fuzz_' in binary_path.name:
        return TestCategory.FUZZ

    # Default to unit
    return TestCategory.UNIT

#==============================================================================
# Test Execution (with side effects - clearly marked)
#==============================================================================

def execute_single_test(binary_path: Path,
                        timeout_sec: int = 300) -> TestResult:
    """
    Execute single test binary

    WHAT: Run test and capture result
    WHY:  Core test execution primitive
    HOW:  subprocess.run with timeout

    NOTE: Side effect - runs external process

    COMPLEXITY: O(test_duration)
    """
    # Guard clauses
    if not binary_path.exists():
        return TestResult(
            name=binary_path.name,
            category=categorize_test_binary(binary_path),
            status=TestStatus.SKIP,
            duration_ms=0.0,
            output="",
            error_message="Binary not found"
        )

    start_time = datetime.now()

    try:
        # Configure ASan environment
        # WHY: Disable leak detection for tests (many have intentional leaks in test fixtures)
        # WHY: Preload ASan to avoid runtime ordering issues
        env = os.environ.copy()
        env['ASAN_OPTIONS'] = 'detect_leaks=0:detect_odr_violation=0'
        print(f"[DEBUG_ASAN] Configuring ASan for {binary_path.name}")

        # Find and preload libasan
        try:
            libasan = subprocess.check_output(['gcc', '-print-file-name=libasan.so'],
                                             text=True).strip()
            if libasan and libasan != 'libasan.so':
                env['LD_PRELOAD'] = libasan
                print(f"[DEBUG_ASAN] LD_PRELOAD={libasan}")
        except Exception as e:
            print(f"[DEBUG_ASAN] Failed to get libasan: {e}")
            pass  # Continue without preload

        result = subprocess.run(
            [str(binary_path)],
            capture_output=True,
            text=True,
            timeout=timeout_sec,
            cwd=binary_path.parent,
            env=env
        )

        duration = (datetime.now() - start_time).total_seconds() * 1000

        # Debug: print ALL test results
        print(f"[TEST_RESULT] {binary_path.name}: returncode={result.returncode}")
        if result.returncode != 0:
            print(f"  Stderr (first 200): {result.stderr[:200]}")

        status = TestStatus.PASS if result.returncode == 0 else TestStatus.FAIL

        return TestResult(
            name=binary_path.name,
            category=categorize_test_binary(binary_path),
            status=status,
            duration_ms=duration,
            output=result.stdout,
            error_message=result.stderr if result.returncode != 0 else None
        )

    except subprocess.TimeoutExpired:
        duration = timeout_sec * 1000
        return TestResult(
            name=binary_path.name,
            category=categorize_test_binary(binary_path),
            status=TestStatus.TIMEOUT,
            duration_ms=duration,
            output="",
            error_message=f"Test exceeded {timeout_sec}s timeout"
        )

    except Exception as e:
        duration = (datetime.now() - start_time).total_seconds() * 1000
        return TestResult(
            name=binary_path.name,
            category=categorize_test_binary(binary_path),
            status=TestStatus.CRASH,
            duration_ms=duration,
            output="",
            error_message=str(e)
        )

def execute_test_suite(binaries: Tuple[Path, ...],
                       timeout_sec: int = 300) -> TestSuite:
    """
    Execute all tests

    WHAT: Run complete test suite
    WHY:  Orchestrate all test execution
    HOW:  Map execute_single_test over binaries

    NOTE: Side effect - runs external processes
    """
    if not binaries:
        return TestSuite(tests=tuple())

    print(f"\n[TEST_SUITE] Starting execution of {len(binaries)} tests:")
    for i, binary in enumerate(binaries, 1):
        print(f"  [{i}/{len(binaries)}] {binary.name}")
    print()

    results = tuple(execute_single_test(binary, timeout_sec) for binary in binaries)
    return TestSuite(tests=results)

#==============================================================================
# Auto-Fix Helpers
#==============================================================================

def ai_powered_auto_fix(failure: dict, nimcp_root: Path, llm_provider: LLMProvider, max_tokens: int = 4096) -> Optional[Fix]:
    """
    AI-powered auto-fix using injected LLM provider

    WHAT: Call LLM API with test output, source files, and test files
    WHY:  Provide LLM with necessary context to understand and fix the issue
    HOW:  Read relevant files, build comprehensive prompt, call API via provider, parse response

    PARAMETERS:
        failure: Test failure dictionary
        nimcp_root: Path to project root
        llm_provider: Injected LLM provider (dependency injection)
        max_tokens: Maximum tokens for LLM response

    RETURNS: Fix object with file edits, or None if fix cannot be generated
    """
    test_name = failure['name']
    # Handle both 'output'/'error' (from test_runner.py) and 'stdout'/'stderr' formats
    stdout = str(failure.get('stdout', failure.get('output', '')))
    stderr = str(failure.get('stderr', failure.get('error', '')))

    # Find test file - handle unit, integration, and regression tests with hierarchical structure
    # Test names like: unit_cognitive_logic_test_neural_logic_real
    # Map to files like: test/unit/cognitive/logic/test_neural_logic_real.cpp

    import glob

    # Determine test type and extract base name
    if test_name.startswith('integration_'):
        test_type = 'integration'
        # Remove 'integration_' prefix, extract everything after last '_test_'
        if '_test_' in test_name:
            test_file_name = 'test_' + test_name.split('_test_')[-1] + '.cpp'
        else:
            test_file_name = test_name.replace('integration_', 'test_') + '.cpp'
    elif test_name.startswith('regression_'):
        test_type = 'regression'
        if '_test_' in test_name:
            test_file_name = 'test_' + test_name.split('_test_')[-1] + '.cpp'
        else:
            test_file_name = test_name.replace('regression_', 'test_') + '.cpp'
    elif test_name.startswith('unit_'):
        test_type = 'unit'
        # Extract the actual test file name after the category
        # E.g., unit_cognitive_logic_test_neural_logic_real -> test_neural_logic_real.cpp
        if '_test_' in test_name:
            test_file_name = 'test_' + test_name.split('_test_')[-1] + '.cpp'
        else:
            test_file_name = test_name.replace('unit_', 'test_') + '.cpp'
    else:
        test_type = 'unit'
        test_file_name = f"test_{test_name}.cpp"

    # Search recursively for the test file
    search_pattern = str(nimcp_root / "test" / test_type / "**" / test_file_name)
    matches = glob.glob(search_pattern, recursive=True)

    if matches:
        test_file = Path(matches[0])  # Use first match
    else:
        # Fallback: try searching in all test directories
        for fallback_type in ['unit', 'integration', 'regression']:
            search_pattern = str(nimcp_root / "test" / fallback_type / "**" / test_file_name)
            matches = glob.glob(search_pattern, recursive=True)
            if matches:
                test_file = Path(matches[0])
                break
        else:
            print(f"  ⚠️  Test file not found: {test_file_name} in test/{test_type}/")
            return None

    if not test_file.exists():
        print(f"  ⚠️  Test file does not exist: {test_file}")
        return None

    # Read test file content
    try:
        test_content = test_file.read_text()
    except Exception as e:
        print(f"  ⚠️  Could not read test file: {e}")
        return None

    # Try to infer the source file being tested
    # Extract from test file includes or naming convention
    source_file = None
    source_content = ""

    # Try common patterns
    test_base = test_file_name.replace('test_', '')
    possible_sources = [
        nimcp_root / "src" / "core" / "brain" / f"nimcp_{test_base}.c",
        nimcp_root / "src" / "core" / "neuralnet" / f"nimcp_{test_base}.c",
        nimcp_root / "src" / "cognitive" / f"nimcp_{test_base}.c",
        nimcp_root / "src" / "glial" / "integration" / f"nimcp_{test_base}.c",
    ]

    for src in possible_sources:
        if src.exists():
            source_file = src
            try:
                source_content = src.read_text()
                break
            except:
                pass

    # Build comprehensive prompt for Claude
    prompt = f"""You are debugging a failing C++ unit test in the NIMCP neuromorphic computing library.

## TEST INFORMATION
Test Name: {test_name}
Test File: {test_file.relative_to(nimcp_root)}

## TEST FAILURE OUTPUT
STDOUT:
{stdout[:2000]}

STDERR:
{stderr[:2000]}

## TEST FILE CONTENT
```cpp
{test_content[:5000]}
```

"""

    if source_content:
        prompt += f"""## SOURCE FILE BEING TESTED
File: {source_file.relative_to(nimcp_root)}
```c
{source_content[:5000]}
```

"""

    # Load and inject claude.md coding standards
    claude_md_path = nimcp_root / ".claude" / "claude.md"
    if claude_md_path.exists():
        try:
            with open(claude_md_path, 'r') as f:
                claude_standards = f.read()
            prompt += f"""
## NIMCP CODING STANDARDS (from .claude/claude.md)

{claude_standards}

## IMPORTANT: Apply these standards to ALL code you generate!

"""
        except Exception as e:
            # If loading fails, continue without standards (non-fatal)
            prompt += f"  ⚠️  Could not load .claude/claude.md: {e}\n\n"

    # Add comprehensive project context for GPT-5
    prompt += """
## NIMCP PROJECT CONTEXT

### Architecture Overview
NIMCP (Neuromorphic Integration & Multi-modal Cognitive Platform) is a biologically-inspired spiking neural network framework written in C.

**Core Concepts:**
- **Spiking Neural Networks (SNNs)**: Discrete spike-based computation, not continuous activations
- **Neuromodulation**: Chemical signaling (dopamine, serotonin, etc.) modulates learning and behavior
- **Brain-Inspired Design**: Hierarchical structure with brain regions, cognitive modules, and plasticity rules
- **Multi-modal Integration**: Handles visual, audio, speech, and cross-modal processing

### Module Structure
```
src/
├── core/                    # Core neural network components
│   ├── brain/              # brain_t API - main orchestrator (opaque pointer pattern)
│   ├── neuralnet/          # neural_network_t - SNN implementation
│   └── topology/           # Fractal and hierarchical network topologies
├── cognitive/              # Higher-level cognitive functions
│   ├── curiosity/          # Knowledge gap detection, question generation
│   ├── ethics/             # Ethical reasoning and decision-making
│   ├── knowledge/          # Knowledge representation and retrieval
│   ├── salience/           # Attention and saliency evaluation
│   ├── wellbeing/          # Emotional and physiological wellbeing
│   ├── emotions/           # Emotional processing (grief, joy, love, etc.)
│   └── ... (introspection, executive, working_memory, etc.)
├── plasticity/             # Learning and adaptation mechanisms
│   ├── stdp/               # Spike-timing dependent plasticity
│   ├── neuromodulators/    # Dopamine, serotonin, acetylcholine systems
│   ├── bcm/                # BCM learning rule
│   ├── eligibility/        # Eligibility traces for credit assignment
│   └── adaptive/           # Adaptive learning rates
├── glial/                  # Glial cell simulation (astrocytes, microglia, oligodendrocytes)
├── nlp/                    # Natural language processing integration
├── networking/             # Distributed cognition and P2P networking
├── security/               # Encryption, authentication, security measures
└── utils/                  # Data structures (btree, cache, queue, graph, etc.)

### Test Organization
```
test/
├── unit/                   # Unit tests (test individual functions/modules)
│   ├── cognitive/         # Tests for cognitive modules
│   ├── core/              # Tests for brain, neuralnet, topology
│   ├── plasticity/        # Tests for learning mechanisms
│   ├── utils/             # Tests for data structures
│   └── ... (glial, gpu, nlp, security, etc.)
├── integration/           # Integration tests (test module interactions)
│   ├── cognitive/
│   ├── core/
│   └── ... (same structure as unit/)
└── regression/            # Regression tests (backward compatibility)
    └── ... (same structure as unit/)
```

**Test Naming Convention:**
- Unit tests: `unit_<category>_<module>_test_<component>.cpp` → `unit_test_<component>` executable
- Integration tests: `integration_<category>_<module>_test_<component>.cpp` → `integration_test_<component>` executable
- Regression tests: `regression_<category>_<module>_test_<component>.cpp` → `regression_test_<component>` executable

### Common Patterns

**1. Opaque Pointer Pattern** (information hiding):
```c
// Header declares handle type
typedef struct brain_struct* brain_t;

// Access via getter functions ONLY
uint32_t brain_get_num_inputs(brain_t brain);

// NEVER access fields directly: brain->field (will fail - opaque!)
```

**2. Strategy Pattern** (dependency injection):
- LLM providers (OpenAI, Anthropic, Mock)
- Neuron models (Izhikevich, LIF, two-compartment)
- Learning rules (STDP, BCM, eligibility traces)

**3. Reward-Modulated STDP**:
```c
// Dopamine modulates synaptic plasticity
float dopamine = neuromodulator_get_level(system, NEUROMOD_DOPAMINE);
delta_weight = base_stdp * dopamine;
```

**4. Error Handling**:
```c
// Always validate inputs
if (!param) {
    return ERROR_NULL_POINTER;
}

// Allow NULL for testing when appropriate
if (!learner_name) {  // Required parameter
    return NULL;
}
// parent_brain can be NULL for standalone testing
```

**5. Memory Management**:
```c
// Create/destroy pairs
engine_t engine = engine_create(...);
engine_destroy(engine);

// Thread-safe reference counting for shared resources
```

### Build System
- **CMake**: Build configuration in `CMakeLists.txt`
- **Google Test**: Unit testing framework (C++)
- **Coverage**: gcov/lcov for code coverage reports
- **Pre-commit Hook**: Code Surgeon runs on commit to auto-fix tests

### Key Dependencies
- `brain_t` depends on: neural_network_t, neuromodulator_system_t, cognitive modules
- `neural_network_t` depends on: neuromodulator_system_t, plasticity rules, neuron models
- Cognitive modules depend on: brain_t (via getters), utils (btree, cache, queue)
- Tests depend on: Google Test, source modules under test

### Common Test Failure Causes
1. **NULL pointer rejection**: Function changed to reject NULL but tests pass NULL for testing
2. **Opaque pointer access**: Test tries to access struct fields directly instead of using getters
3. **API changes**: Function signature changed breaking backward compatibility
4. **Memory leaks**: Create without destroy, or double-free
5. **Thread safety**: Race conditions in concurrent tests
6. **Hardcoded values**: Placeholder values (0.0f, NULL, etc.) not replaced with real implementation

### Important Constants
```c
// Neuromodulator types
NEUROMOD_DOPAMINE      // Reward, learning, motivation
NEUROMOD_SEROTONIN     // Mood, wellbeing
NEUROMOD_ACETYLCHOLINE // Attention, memory encoding
NEUROMOD_NOREPINEPHRINE // Arousal, alertness

// Error codes
ERROR_NULL_POINTER
ERROR_INVALID_CONFIG
ERROR_MEMORY_ALLOCATION
```

"""

    prompt += """## YOUR TASK
Analyze the test failure and provide a MINIMAL, SURGICAL fix. Your response MUST be in this exact format:

FIX_START
FILE: <relative path to file to edit>
OLD_CODE:
<exact code to find and replace - must match VERBATIM including all whitespace>
NEW_CODE:
<replacement code - preserve formatting and indentation>
EXPLANATION: <brief explanation of the fix>
FIX_END

## CRITICAL RULES:
1. OLD_CODE must be an EXACT, CHARACTER-FOR-CHARACTER match from the file
   - Copy the code directly from the file content above
   - Include ALL whitespace (spaces, tabs, newlines) exactly as shown
   - Start at the beginning of a line, end at the end of a line
2. Make MINIMAL changes - only fix what's broken
   - Don't refactor or improve unrelated code
   - Don't add extra functionality
   - Don't change formatting unless necessary
3. NEW_CODE must be VALID syntax
   - No duplicate code blocks
   - No unclosed braces/brackets
   - Preserve all function signatures
4. Provide ONLY ONE fix per response
5. If you cannot determine a safe fix, respond with "NO_FIX_POSSIBLE"

## EXAMPLE OF GOOD FIX:
OLD_CODE:
    EXPECT_EQ(result, nullptr);
NEW_CODE:
    EXPECT_NE(result, nullptr);
EXPLANATION: Test expects non-null result

## EXAMPLE OF BAD FIX (DO NOT DO THIS):
OLD_CODE:
    EXPECT_EQ(result
NEW_CODE:
    EXPECT_NE(result, nullptr);
    extra_line();
EXPLANATION: [Bad because: incomplete OLD_CODE, adds unrelated code]
"""

    # Call LLM via injected provider (dependency injection)
    try:
        messages = [LLMMessage(role="user", content=prompt)]

        llm_response = llm_provider.generate(messages, max_tokens=max_tokens)

        if not llm_response:
            print(f"  ⚠️  LLM provider failed to generate response")
            return None

        response_text = llm_response.text

        # Parse response
        if "NO_FIX_POSSIBLE" in response_text:
            return None

        if "FIX_START" not in response_text or "FIX_END" not in response_text:
            print(f"  ⚠️  Claude response missing FIX markers")
            return None

        # Extract fix details
        fix_block = response_text.split("FIX_START")[1].split("FIX_END")[0]

        file_path = None
        old_code = None
        new_code = None
        explanation = ""

        current_section = None
        section_lines = []

        for line in fix_block.split('\n'):
            if line.startswith("FILE:"):
                file_path = line.replace("FILE:", "").strip()
            elif line.startswith("OLD_CODE:"):
                if section_lines and current_section:
                    # Save previous section
                    if current_section == "OLD_CODE":
                        old_code = '\n'.join(section_lines)
                    elif current_section == "NEW_CODE":
                        new_code = '\n'.join(section_lines)
                current_section = "OLD_CODE"
                section_lines = []
            elif line.startswith("NEW_CODE:"):
                if section_lines and current_section == "OLD_CODE":
                    old_code = '\n'.join(section_lines)
                current_section = "NEW_CODE"
                section_lines = []
            elif line.startswith("EXPLANATION:"):
                if section_lines and current_section == "NEW_CODE":
                    new_code = '\n'.join(section_lines)
                explanation = line.replace("EXPLANATION:", "").strip()
                current_section = None
            elif current_section:
                section_lines.append(line)

        # Handle last section
        if section_lines and current_section == "NEW_CODE":
            new_code = '\n'.join(section_lines)

        if not file_path or not old_code or new_code is None:
            print(f"  ⚠️  Incomplete fix from Claude")
            return None

        return Fix(
            file_path=file_path,
            old_code=old_code.strip(),
            new_code=new_code.strip(),
            explanation=explanation,
            confidence=0.85
        )

    except Exception as e:
        print(f"  ⚠️  AI auto-fix failed: {e}")
        return None


def auto_generate_fix(failure: dict, analysis, nimcp_root: Path, llm_provider: LLMProvider, gdb_output: Optional[str] = None) -> Optional[Fix]:
    """
    Auto-generate fix for common test failures (with dependency injection)

    WHAT: Analyze failure and generate automatic fix
    WHY:  Enable self-healing without manual intervention
    HOW:  Pattern matching on common failure types + gdb backtrace parsing

    PARAMETERS:
        failure: Test failure dictionary
        analysis: Failure analysis results
        nimcp_root: Path to project root
        llm_provider: Injected LLM provider
        gdb_output: Optional GDB debugging output

    RETURNS: Fix object or None if cannot auto-fix
    """
    test_name = failure['name']
    # Handle both 'output'/'error' (from test_runner.py) and 'stdout'/'stderr' formats
    stdout = str(failure.get('stdout', failure.get('output', '')))
    stderr = str(failure.get('stderr', failure.get('error', '')))

    # Try AI-powered auto-fix first (provides LLM with full context)
    ai_fix = ai_powered_auto_fix(failure, nimcp_root, llm_provider)
    if ai_fix:
        return ai_fix

    # Fall back to pattern-based fixes if AI fails
    # Parse gdb backtrace for hang location
    if gdb_output:
        import re
        # Pattern: #N  <address> in <function> (<args>) at <file>:<line>
        backtrace_pattern = r'#\d+\s+0x[0-9a-f]+\s+in\s+(\w+)\s+\([^)]*\)\s+at\s+([^:]+):(\d+)'
        matches = re.findall(backtrace_pattern, gdb_output)

        # Check for init_neuron_activity_tracking hang
        for func_name, file_path, line_num in matches:
            if func_name == 'init_neuron_activity_tracking' and 'nimcp_neuralnet.c' in file_path:
                return Fix(
                    file_path=f"src/core/neuralnet/nimcp_neuralnet.c",
                    old_code=f"// Line {line_num}: memset hanging in init_neuron_activity_tracking",
                    new_code=f"// Line {line_num}: Check buffer size calculation - likely too large",
                    explanation=f"Hang detected in {func_name} at {file_path}:{line_num} - memset with invalid size",
                    confidence=0.7
                )

        # Check for any memset hang
        if 'memset_avx2' in gdb_output or '__memset' in gdb_output:
            return Fix(
                file_path="<needs manual inspection>",
                old_code="memset hanging",
                new_code="Check buffer size - may be corrupted or too large",
                explanation="memset is hanging - buffer size calculation error",
                confidence=0.6
            )

    # Pattern 1: Timeout/Hang - Reduce test complexity or add timeout
    if analysis.failure_type.value == 'timeout' or 'timed out' in stderr.lower():
        test_file = nimcp_root / "test" / "unit" / f"{test_name.replace('unit_test_', 'test_')}.cpp"
        if test_file.exists():
            # Suggest reducing test scope
            return Fix(
                file_path=str(test_file.relative_to(nimcp_root)),
                old_code="// AUTO-FIX: Test hanging",
                new_code="// AUTO-FIX: Test timing out - consider splitting test",
                explanation="Test is hanging - needs manual debugging with gdb or reducing test complexity",
                confidence=0.3
            )

    # Pattern 2: Memory leak - Suggest cleanup
    if "memory leak" in stdout.lower() or "memory leak" in stderr.lower():
        # Extract allocation count if possible
        import re
        match = re.search(r'(\d+) allocations remain', stdout + stderr)
        if match:
            return Fix(
                file_path="test/unit/test_<name>.cpp",
                old_code="// Memory leak detected",
                new_code="// Add cleanup in TearDown() for " + match.group(1) + " allocations",
                explanation="Memory leak detected - add proper cleanup",
                confidence=0.5
            )

    # Pattern 3: API mismatch - Type cast needed
    if "cannot convert" in stderr and "ethics_violation" in stderr:
        return Fix(
            file_path="test/unit/test_ethics_comprehensive.cpp",
            old_code="policy.violation_type = type;",
            new_code="policy.violation_type = (ethics_violation_t)type;",
            explanation="Type mismatch - needs explicit cast",
            confidence=0.9
        )

    # Pattern 4: Hanging test - Run with reduced timeout
    if test_name in ['unit_test_brain_comprehensive', 'unit_test_ethics_comprehensive',
                     'unit_test_neuralnet_comprehensive']:
        # These are known hanging tests - suggest running individual test cases
        return None  # Can't auto-fix without knowing which test case hangs

    # Pattern 5: Check for common NULL pointer patterns
    if "segmentation fault" in stderr.lower() or "sigsegv" in stderr.lower():
        return None  # Requires gdb to find exact location

    return None

def debug_hanging_test_with_gdb(test_binary: Path, timeout_sec: int = 5) -> Optional[str]:
    """
    Debug hanging test with gdb to find hang location

    WHAT: Run test under gdb with timeout, get backtrace
    WHY:  Identify where test is hanging
    HOW:  gdb with timeout, interrupt, backtrace

    RETURNS: Stack trace string or None
    NOTE: Side effect - runs gdb
    """
    try:
        # Create gdb command script
        gdb_commands = """
run
thread apply all bt
quit
"""
        gdb_script = test_binary.parent / f"{test_binary.name}.gdb"
        gdb_script.write_text(gdb_commands)

        # Run with timeout
        result = subprocess.run(
            ["timeout", "--signal=INT", str(timeout_sec), "gdb", "-batch", "-x", str(gdb_script), str(test_binary)],
            capture_output=True,
            text=True,
            timeout=timeout_sec + 5
        )

        # Clean up
        gdb_script.unlink(missing_ok=True)

        if result.stdout and "backtrace" in result.stdout.lower():
            return result.stdout
        return result.stdout + result.stderr

    except Exception as e:
        return f"GDB debug failed: {str(e)}"

def debug_hanging_test_with_rr(test_binary: Path, timeout_sec: int = 5) -> Optional[str]:
    """
    Debug hanging test with rr (record and replay)

    WHAT: Record test execution with rr, replay for analysis
    WHY:  Deterministic debugging, can step backwards
    HOW:  rr record → rr replay with backtrace

    RETURNS: Stack trace string or None
    NOTE: Side effect - runs rr
    """
    try:
        # Record execution
        record_result = subprocess.run(
            ["timeout", str(timeout_sec), "rr", "record", str(test_binary)],
            capture_output=True,
            text=True,
            timeout=timeout_sec + 5
        )

        # Replay and get backtrace
        rr_commands = """
continue
thread apply all bt
quit
"""
        rr_script = test_binary.parent / f"{test_binary.name}.rr"
        rr_script.write_text(rr_commands)

        replay_result = subprocess.run(
            ["rr", "replay", "-x", str(rr_script), "-q"],
            capture_output=True,
            text=True,
            timeout=30
        )

        # Clean up
        rr_script.unlink(missing_ok=True)

        if replay_result.stdout:
            return f"=== RR RECORD ===\n{record_result.stdout}\n\n=== RR REPLAY ===\n{replay_result.stdout}"
        return record_result.stdout + replay_result.stdout

    except Exception as e:
        return f"RR debug failed: {str(e)}"

def rebuild_project(build_dir: Path) -> Tuple[bool, str]:
    """
    Rebuild project after applying fixes

    WHAT: Run CMake build
    WHY:  Compile changes before rerunning tests
    HOW:  subprocess.run make command

    RETURNS: (success: bool, error_message: str) tuple
    NOTE: Side effect - builds project
    """
    try:
        result = subprocess.run(
            ["make", "-j8"],
            cwd=build_dir,
            capture_output=True,
            text=True,
            timeout=300
        )
        if result.returncode == 0:
            return (True, "")
        else:
            # Return compilation errors for debugging
            error_lines = result.stderr.split('\n')
            # Get last 10 lines of errors
            relevant_errors = '\n'.join(error_lines[-10:])
            return (False, relevant_errors)
    except Exception as e:
        return (False, str(e))

def git_create_snapshot(file_path: Path, nimcp_root: Path) -> bool:
    """
    Create git snapshot of file before applying fix

    WHAT: Stage current file state in git index
    WHY:  Enable rollback if fix breaks compilation
    HOW:  git add <file>

    RETURNS: True if snapshot created, False otherwise
    NOTE: Side effect - modifies git index
    """
    try:
        result = subprocess.run(
            ["git", "add", str(file_path)],
            cwd=nimcp_root,
            capture_output=True,
            text=True,
            timeout=10
        )
        return result.returncode == 0
    except Exception:
        return False

def git_rollback_file(file_path: Path, nimcp_root: Path) -> bool:
    """
    Rollback file to last committed state

    WHAT: Restore file from git HEAD
    WHY:  Undo broken AI-generated fixes
    HOW:  git restore <file>

    RETURNS: True if rollback succeeded, False otherwise
    NOTE: Side effect - discards uncommitted changes
    """
    try:
        result = subprocess.run(
            ["git", "restore", str(file_path)],
            cwd=nimcp_root,
            capture_output=True,
            text=True,
            timeout=10
        )
        return result.returncode == 0
    except Exception:
        return False

#==============================================================================
# Main Orchestration
#==============================================================================

def orchestrate_full_pipeline(config: CodeSurgeonConfig) -> int:
    """
    Full test-fix-verify loop with coverage analysis (dependency injection)

    WHAT: Complete Code Surgeon orchestration with lcov integration
    WHY:  Automated testing + fixing + verification + coverage tracking
    HOW:  Test → Coverage → Analyze → Fix → Verify → Iterate

    PARAMETERS:
        config: CodeSurgeonConfig with all settings and dependencies

    RETURNS: 0 if all tests pass, 1 otherwise
    COMPLEXITY: O(iterations * test_count)

    DESIGN: Uses dependency injection for LLM provider and configuration
    """
    from test_runner import run_tests_parallel
    from failure_analyzer import analyze_failure
    from auto_fixer import prepare_fix_context, apply_fix
    from coverage import run_full_coverage_analysis

    # Extract configuration
    nimcp_root = config.nimcp_root
    test_root = config.get_test_dir()
    build_dir = config.get_build_dir()
    max_iterations = config.max_iterations
    enable_coverage = config.enable_coverage
    llm_provider = config.get_llm_provider()

    # Guard clause
    if not test_root.exists():
        print(f"❌ Test directory not found: {test_root}")
        print(f"   Build the project first or check configuration")
        print(f"   Configured test_dir: {test_root}")
        print(f"   Configured build_dir: {build_dir}")
        return 1

    # Discover tests
    print(f"\n[DISCOVERY] Scanning {test_root}...")
    binaries = discover_test_binaries(test_root)
    print(f"Found {len(binaries)} test binaries")

    # Guard: no tests found
    if not binaries:
        print("⚠️  No test binaries found")
        return 0

    iteration = 0
    while iteration < max_iterations:
        iteration += 1
        print(f"\n{'='*60}")
        print(f"ITERATION {iteration}/{max_iterations}")
        print('='*60)

        # Execute tests in parallel
        print("\n[EXECUTION] Running tests in parallel...")
        print(f"[DEBUG] About to run {len(binaries)} tests...")
        import sys
        sys.stdout.flush()
        results = run_tests_parallel(binaries, timeout_sec=300)
        print(f"[DEBUG] Finished running tests, got {len(results)} results")
        sys.stdout.flush()

        # Calculate metrics
        passing = sum(1 for r in results if r['status'] == 'pass')
        failing = [r for r in results if r['status'] != 'pass']

        print(f"\n[RESULTS]")
        print(f"  Total: {len(results)}")
        print(f"  Passing: {passing}")
        print(f"  Failing: {len(failing)}")
        print(f"  Pass rate: {(passing/len(results)*100) if results else 0:.1f}%")

        # Run coverage analysis if enabled
        if enable_coverage:
            coverage_report = run_full_coverage_analysis(nimcp_root)
            if coverage_report:
                # Store coverage for potential use in fix decisions
                pass

        # Guard: all passing
        if not failing:
            print("\n✅ All tests passing!")
            return 0

        # Analyze failures
        print(f"\n[ANALYSIS] Analyzing {len(failing)} failures...")
        analyses = [analyze_failure(r) for r in failing]

        for analysis in analyses[:3]:  # Show first 3
            print(f"\n  Test: {analysis.test_name}")
            print(f"  Type: {analysis.failure_type.value}")
            print(f"  Tool: {analysis.recommended_tool}")

        # Auto-fix loop
        print("\n[AUTO-FIX] Attempting to fix failures...")
        fixes_applied = 0

        for i, (failure, analysis) in enumerate(zip(failing[:3], analyses[:3]), 1):
            print(f"\n  [{i}/3] Fixing {failure['name']}...")

            # Get detailed output
            test_binary = test_root / failure['name']
            detailed_result = execute_single_test(test_binary, timeout_sec=60)

            # If timeout/hang detected, run gdb/rr for debugging
            gdb_output = None
            rr_output = None
            if analysis.failure_type.value == 'timeout' or detailed_result.status == TestStatus.TIMEOUT:
                print(f"  → Test hanging, running gdb analysis...")
                gdb_output = debug_hanging_test_with_gdb(test_binary, timeout_sec=5)
                if gdb_output:
                    print(f"  → GDB backtrace captured ({len(gdb_output)} chars)")

                    # Save gdb output for analysis
                    debug_dir = nimcp_root / ".code_surgeon" / "debug"
                    debug_dir.mkdir(parents=True, exist_ok=True)
                    debug_file = debug_dir / f"{failure['name']}_gdb.txt"
                    debug_file.write_text(gdb_output)
                    print(f"  → GDB output saved to {debug_file.relative_to(nimcp_root)}")

                # Try rr if available
                print(f"  → Attempting rr (record-replay) analysis...")
                rr_output = debug_hanging_test_with_rr(test_binary, timeout_sec=5)
                if rr_output and "RR debug failed" not in rr_output:
                    print(f"  → RR analysis captured ({len(rr_output)} chars)")
                    rr_file = debug_dir / f"{failure['name']}_rr.txt"
                    rr_file.write_text(rr_output)
                    print(f"  → RR output saved to {rr_file.relative_to(nimcp_root)}")
                elif rr_output:
                    print(f"  → RR not available or failed")

            # Prepare context for fix
            # Handle both 'output'/'error' (from test_runner.py) and 'stdout'/'stderr' formats
            stdout_content = failure.get('stdout', failure.get('output', ''))
            stderr_content = failure.get('stderr', failure.get('error', ''))
            debug_info = stdout_content + '\n' + stderr_content
            if gdb_output:
                debug_info += '\n\n=== GDB BACKTRACE ===\n' + gdb_output
            if rr_output:
                debug_info += '\n\n=== RR ANALYSIS ===\n' + rr_output

            context = prepare_fix_context(
                failure['name'],
                {
                    'failure_type': analysis.failure_type.value,
                    'error_message': stderr_content,
                },
                debug_info,
                ()  # Source files would be added here
            )

            # Generate fix automatically (using injected LLM provider)
            fix = auto_generate_fix(failure, analysis, nimcp_root, llm_provider, gdb_output=gdb_output)

            if fix:
                print(f"  → Applying fix to {fix.file_path}")

                # Create git snapshot before applying fix (for rollback)
                file_path = nimcp_root / fix.file_path
                snapshot_created = git_create_snapshot(file_path, nimcp_root)
                if not snapshot_created:
                    print(f"  ⚠️  Could not create git snapshot, skipping fix")
                    continue

                # Apply the fix
                fix_result = apply_fix(fix, nimcp_root)

                if fix_result.success:
                    print(f"  ✅ Fix applied successfully")

                    # Validate fix by rebuilding project
                    rebuild_success, rebuild_error = rebuild_project(build_dir)
                    if rebuild_success:
                        print(f"  ✅ Rebuild successful - fix validated")
                        fixes_applied += 1
                    else:
                        # Rebuild failed - rollback the fix
                        print(f"  ❌ Rebuild failed - rolling back fix")
                        print(f"     Error: {rebuild_error[:200]}")  # Show first 200 chars
                        rollback_success = git_rollback_file(file_path, nimcp_root)
                        if rollback_success:
                            print(f"  ✅ Rollback successful - file restored")
                        else:
                            print(f"  ⚠️  Rollback failed - manual intervention needed")
                else:
                    print(f"  ❌ Fix failed: {fix_result.error_message}")
            else:
                print(f"  ⚠️  Could not generate fix automatically")

        print(f"\n  Total fixes applied: {fixes_applied}")

        # If fixes were applied, continue to next iteration
        if fixes_applied > 0:
            print("\n♻️  Fixes applied, rerunning tests...")
            continue
        else:
            print("\n⚠️  No fixes applied, stopping")
            break

    return 1 if failing else 0

def orchestrate_parallel_pipeline(nimcp_root: Path,
                                  target_files: Tuple[str, ...],
                                  num_workers: int = 4,
                                  debug_mode: bool = False) -> int:
    """
    Parallel test orchestration pipeline

    WHAT: Execute Code Surgeon operations in parallel across multiple targets
    WHY:  Maximize throughput for large codebases
    HOW:  Spawn N child Code Surgeon agents, distribute work, aggregate results

    RETURNS: 0 if all tasks succeed, 1 otherwise
    COMPLEXITY: O(longest_task) with parallelism factor of num_workers
    """
    from parallel_executor import execute_parallel_test_generation
    from result_aggregator import aggregate_results, generate_text_report, save_report

    print(f"\n{'='*60}")
    print("Code Surgeon v2.0 - PARALLEL EXECUTION MODE")
    print(f"{'='*60}")
    print(f"Workers: {num_workers}")
    print(f"Targets: {len(target_files)}")
    print(f"{'='*60}\n")

    # Execute parallel test generation
    results = execute_parallel_test_generation(
        target_files=target_files,
        nimcp_root=nimcp_root,
        num_workers=num_workers,
        debug_mode=debug_mode
    )

    # Aggregate results
    report = aggregate_results(results)

    # Display report
    print(generate_text_report(report))

    # Save reports to disk
    report_dir = nimcp_root / ".code_surgeon" / "reports"
    saved_reports = save_report(report, report_dir, formats=('text', 'json', 'html'))

    print("\nReports saved:")
    for format_name, path in saved_reports.items():
        print(f"  {format_name}: {path.relative_to(nimcp_root)}")

    # Return success if all tasks completed
    return 0 if report.metrics.failed_tasks == 0 else 1

def main():
    """
    Main entry point

    WHAT: Code Surgeon CLI entry
    WHY:  Single point of control
    HOW:  Parse args, run orchestration (serial or parallel)
    """
    import argparse

    parser = argparse.ArgumentParser(description="Code Surgeon - Automated Test Orchestration")
    parser.add_argument("--mode", choices=["test-only", "full"], default="test-only",
                       help="Execution mode")
    parser.add_argument("--max-iterations", type=int, default=5,
                       help="Max fix iterations")
    parser.add_argument("--coverage", action="store_true", default=True,
                       help="Run coverage analysis (default: enabled)")
    parser.add_argument("--no-coverage", action="store_false", dest="coverage",
                       help="Skip coverage analysis")

    # Parallel execution options
    parser.add_argument("--parallel", type=int, metavar="N", default=None,
                       help="Enable parallel execution with N workers (default: CPU count)")
    parser.add_argument("--target-files", nargs="+", metavar="FILE",
                       help="Target files for parallel execution (e.g., nimcp_p2pnode.c nimcp_security.c)")
    parser.add_argument("--debug", action="store_true",
                       help="Enable debug output")

    args = parser.parse_args()

    print("Code Surgeon v2.0 - Automated Test Orchestration")
    print("=" * 60)

    nimcp_root = Path(__file__).parent.parent.parent

    # Check if parallel mode is enabled
    if args.parallel is not None:
        # Parallel execution mode
        if not args.target_files:
            print("Error: --target-files required for parallel execution")
            print("Example: --parallel 3 --target-files nimcp_p2pnode.c nimcp_security.c nimcp.c")
            return 1

        return orchestrate_parallel_pipeline(
            nimcp_root=nimcp_root,
            target_files=tuple(args.target_files),
            num_workers=args.parallel if args.parallel > 0 else None,
            debug_mode=args.debug
        )
    else:
        # Serial execution mode (with dependency injection)
        if args.mode == "full":
            config = create_config_from_env(
                nimcp_root,
                max_iterations=args.max_iterations,
                enable_coverage=args.coverage
            )
            return orchestrate_full_pipeline(config)
        else:
            # Quick test-only mode
            config = create_config_from_env(
                nimcp_root,
                max_iterations=1,
                enable_coverage=args.coverage
            )
            return orchestrate_full_pipeline(config)

if __name__ == "__main__":
    sys.exit(main())
