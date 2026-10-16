# Coding Standards and Protocols

## Coding Standards

- **Documentation**: WHAT/WHY/HOW comments on all functions
- **Guard clauses**: No nested ifs, early returns for validation
- **Single Responsibility**: Functions do one thing, < 50 lines
- **Biological basis**: Document neural/cognitive grounding
- **Memory safety**: Use nimcp_malloc/nimcp_free, proper cleanup

## Critical API Usage Rule

**NEVER make assumptions about API methods.** Always verify function signatures by reading the actual header files before writing test code or implementations. This includes:
- Function parameter count and types
- Parameter order
- Return types and output parameters
- Enum/constant names

---

## CRITICAL WARNING - PAST VIOLATIONS

**Claude has repeatedly ignored the MANDATORY Test Writing Protocol in this file.**

In December 2024, Claude wrote numerous test files without reading header files first, resulting in:
- Invented function names that don't exist (e.g., `attention_system_t` instead of `multihead_attention_t`)
- Wrong parameter counts and types
- Non-existent struct members
- Mismatched enum types (e.g., `BRAIN_CYTOKINE_IL10` vs `cytokine_type_t`)
- Hours of debugging compilation errors that should never have happened

**Before Claude writes ANY test code, the user MUST require Claude to:**
1. Show the complete header file content that was read
2. List the exact function signatures to be used
3. Only then approve test code writing

**Do not trust Claude to follow the protocol without explicit verification.**

---

## MANDATORY Test Writing Protocol

> **VIOLATION HISTORY**: Claude has ignored this protocol multiple times, causing significant rework. User must verify compliance before any test code is written. See warning above.

**BEFORE writing ANY test code, you MUST:**
1. Read the COMPLETE header file for the module being tested
2. List the exact function signatures you will call in a code block
3. Show the struct definitions for any types you'll use
4. Only THEN write test code

**NEVER assume or infer:**
- Function names - READ the header
- Parameter counts/types - READ the header
- Return types - READ the header
- Struct member names - READ the header
- Default config values - READ the implementation

**If you write test code that fails to compile due to API mismatch:**
- STOP and acknowledge you violated this protocol
- Read the actual header before proceeding

**Alternative approach:** Write implementation first, then derive tests from what was actually written - rather than writing tests for APIs you haven't verified.

---

## MANDATORY Regex Testing Protocol

**BEFORE applying ANY regex to the codebase, you MUST:**
1. Test the regex on representative sample input first
2. Print both input and output to verify the transformation is correct
3. Check edge cases (what if the pattern appears multiple times? what about nested parentheses?)

**Example validation approach:**
```python
import re

# ALWAYS test before applying to files
test_cases = [
    "nimcp_mutex_lock((pthread_mutex_t*)&bridge->base.mutex);",
    "pthread_mutex_init(&bridge->base.mutex, NULL)",
]

pattern = r'your_pattern_here'
replacement = 'your_replacement'

for test in test_cases:
    result = re.sub(pattern, replacement, test)
    print(f"Input:  {test}")
    print(f"Output: {result}")
    print()
```

**If a regex produces unexpected output:**
- STOP and fix the pattern before running on real files
- Never assume a regex works without testing it

**Common regex pitfalls to watch for:**
- Greedy matching consuming too much
- Not accounting for the full context (e.g., function call parentheses)
- Forgetting to escape special characters
- Patterns that match partial identifiers

---

## SRP File Splitting Convention

Large source files are split using `#include`-based SRP (Single Responsibility Principle) splits:

```c
// nimcp_lnn_gradient.c — main file with includes and declarations
#include "nimcp_lnn_gradient_part_lifecycle.c"  // lifecycle functions
#include "nimcp_lnn_gradient_part_core.c"       // core computation
#include "nimcp_lnn_gradient_part_accessors.c"  // getters/setters
#include "nimcp_lnn_gradient_part_helpers.c"     // helper functions
```

Part files have a standard header:
```c
// nimcp_lnn_gradient_part_lifecycle.c - lifecycle functions
// Part of nimcp_lnn_gradient.c (SRP #include-based split)
// DO NOT compile separately - #included from nimcp_lnn_gradient.c
```

## Guard Clause Pattern

Both braces AND return required. `NIMCP_THROW_TO_IMMUNE` alone does NOT halt execution:
```c
if (!ptr) {
    NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "ptr is NULL");
    return NULL;  // REQUIRED — throw doesn't stop flow
}
```

## Health Agent Macros

Per-module health monitoring via atomic health agents:
```c
#include "utils/fault_tolerance/nimcp_health_agent_macros.h"
NIMCP_DECLARE_HEALTH_AGENT_ATOMIC(module_name)
```

## Bridge Boilerplate

Standard bridge initialization with mesh adapter:
```c
#include "utils/bridge/nimcp_bridge_boilerplate.h"
BRIDGE_BOILERPLATE_MESH_ONLY(bridge_name, MESH_ADAPTER_CATEGORY_SYSTEM)
```

## setjmp/longjmp Rule

Variables modified between `setjmp` and `longjmp` MUST be declared `volatile`.

## Return Value Conventions

### Standard NIMCP Functions

Most NIMCP functions return `nimcp_error_t` enum values:
- `NIMCP_SUCCESS` (0) for success
- `NIMCP_ERROR_*` codes for errors (e.g., `NIMCP_ERROR_NULL_POINTER`, `NIMCP_ERROR_NO_MEMORY`)

Use appropriate error codes for different failure scenarios:
- `NIMCP_ERROR_INVALID_PARAM` - Invalid parameter (NULL or invalid value)
- `NIMCP_ERROR_NULL_POINTER` - Specifically NULL pointer passed
- `NIMCP_ERROR_NO_MEMORY` - Memory allocation failure
- `NIMCP_ERROR_INVALID_STATE` - Invalid object state (e.g., wrong magic number)
- `NIMCP_ERROR_NOT_FOUND` - Requested item not found
- `NIMCP_ERROR_NOT_INITIALIZED` - System/module not initialized
- `NIMCP_ERROR_ALREADY_EXISTS` - Item already exists (duplicate)
- `NIMCP_ERROR_BUFFER_OVERFLOW` - Buffer/queue full
- `NIMCP_ERROR_TIMEOUT` - Operation timed out
- `NIMCP_ERROR_CANCELLED` - Operation cancelled (e.g., shutdown)
- `NIMCP_ERROR_MUTEX_INIT` - Mutex initialization failure
- `NIMCP_ERROR_OPERATION_FAILED` - Generic operation failure

### FEP Bridge Functions

**Convention**: FEP (Free Energy Principle) bridge functions return `int`:
- `0` for success
- `-1` for errors

This convention is intentional because:
1. FEP bridges integrate with external predictive coding frameworks
2. Many FEP bridge functions return counts or indices where -1 indicates error
3. Maintains consistency with legacy FEP integration code

**When to use each convention:**
- New core NIMCP functions: Use `nimcp_error_t`
- FEP bridge functions: Use `int` (0/-1)
- Internal functions returning counts: Use `int` where -1 = error, >= 0 = count

### Internal Count-Returning Functions

Some internal functions return `int` with special semantics:
- `>= 0` indicates success with a count
- `-1` indicates error

Example: `bio_router_kg_dispatch_internal()` returns the number of handlers dispatched, or -1 on error.

Document such functions clearly in the header/source to avoid confusion.
