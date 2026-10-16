# NIMCP Security Policy Engine

## Overview

The NIMCP Security Policy Engine provides a declarative policy language (NSPL - NIMCP Security Policy Language) for defining security rules that can be evaluated at runtime. This allows security teams to modify policies without recompiling the application.

## Architecture

```
Policy Text → Parser → AST → Compiler → Bytecode → Evaluator → Result
                ↓                           ↓
            Error Reporting            Bio-Async Events
```

### Components

1. **Parser** (`nimcp_policy_parser.c`): Tokenizes and parses policy text into an Abstract Syntax Tree (AST)
2. **AST** (`nimcp_policy_ast.c`): In-memory representation of policy structure
3. **Compiler** (`nimcp_policy_compiler.c`): Compiles AST to stack-based bytecode
4. **Evaluator** (`nimcp_policy_evaluator.c`): Interprets bytecode against evaluation contexts
5. **Engine** (`nimcp_policy_engine.c`): Coordinates all components, manages policy lifecycle

## Policy Language Syntax

### Basic Rule

```nspl
rule "rule_name" {
    condition: <expression>
    action: <ACTION_TYPE>
    severity: <SEVERITY>
    log: <boolean>
    message: "description"
}
```

### Policy with Multiple Rules

```nspl
policy "policy_name" {
    rule "rule1" {
        condition: <expression>
        action: ALLOW
    }

    rule "rule2" {
        condition: <expression>
        action: DENY
    }
}
```

### Supported Actions

- `ALLOW` - Permit the operation
- `DENY` - Block the operation
- `THROTTLE` - Rate limit the operation
- `LOG` - Log the event
- `ALERT` - Generate an alert
- `CUSTOM` - Custom action (user-defined)

### Expression Syntax

#### Literals

```nspl
# Boolean
true
false

# Integer
42
-10

# Float
3.14
-2.5

# String
"hello world"
"SELECT * FROM users"
```

#### Operators

**Arithmetic:**
- `+` Addition
- `-` Subtraction
- `*` Multiplication
- `/` Division
- `%` Modulo

**Comparison:**
- `==` Equal
- `!=` Not equal
- `<` Less than
- `<=` Less than or equal
- `>` Greater than
- `>=` Greater than or equal

**Logical:**
- `AND` Logical and
- `OR` Logical or
- `NOT` Logical not

**Precedence** (high to low):
1. Parentheses `()`
2. Unary operators (`NOT`, `-`)
3. Multiplicative (`*`, `/`, `%`)
4. Additive (`+`, `-`)
5. Comparison (`<`, `>`, `<=`, `>=`, `==`, `!=`)
6. Logical AND
7. Logical OR

#### Variables and Member Access

```nspl
# Variable reference
input
request
user

# Member access
input.length
request.user.authenticated
user.role
```

#### Function Calls

```nspl
contains(input, "SELECT")
length(input) > 1000
entropy(input) > 0.8
matches(input, "pattern")
```

## Built-in Functions

### `contains(string, substring) -> bool`

Checks if a string contains a substring.

```nspl
condition: contains(input, "SELECT") AND contains(input, "FROM")
```

### `length(string) -> int`

Returns the length of a string.

```nspl
condition: length(input) > 1000
```

### `entropy(string) -> float`

Calculates Shannon entropy of a string, normalized to 0-1 range.

```nspl
condition: entropy(input) > 0.8
```

### `matches(string, pattern) -> bool`

Performs pattern matching (simplified wildcard matching).

```nspl
condition: matches(input, "*script*")
```

## C API

### Engine Lifecycle

```c
// Create engine
nimcp_policy_engine_config_t config = {
    .max_policies = 10,
    .enable_caching = true,
    .cache_size = 100,
    .enable_optimization = true,
    .bio_async = bio_async_instance  // Optional
};

nimcp_policy_engine_t engine = nimcp_policy_engine_create(&config);

// Destroy engine
nimcp_policy_engine_destroy(engine);
```

### Loading Policies

```c
// From string
const char* policy_text = "rule \"test\" { condition: true action: ALLOW }";
nimcp_policy_t policy;
nimcp_error_t err = nimcp_policy_engine_load(engine, policy_text, &policy);

// From file
err = nimcp_policy_engine_load_file(engine, "/path/to/policy.nspl", &policy);

// Unload policy
err = nimcp_policy_engine_unload(engine, policy);

// Reload all policies from files
err = nimcp_policy_engine_reload(engine);
```

### Creating Evaluation Context

```c
nimcp_policy_context_t ctx = nimcp_policy_context_create();

// Set variables
nimcp_policy_context_set_string(ctx, "input", "SELECT * FROM users");
nimcp_policy_context_set_int(ctx, "rate", 150);
nimcp_policy_context_set_bool(ctx, "authenticated", true);
nimcp_policy_context_set_float(ctx, "score", 0.95);

nimcp_policy_context_destroy(ctx);
```

### Evaluating Policies

```c
nimcp_policy_result_t result = {0};
err = nimcp_policy_evaluate(engine, ctx, &result);

if (err == NIMCP_OK) {
    switch (result.action) {
        case NIMCP_POLICY_ACTION_ALLOW:
            // Allow operation
            break;
        case NIMCP_POLICY_ACTION_DENY:
            // Deny operation
            printf("Denied: %s\n", result.message);
            break;
        case NIMCP_POLICY_ACTION_THROTTLE:
            // Apply rate limiting
            break;
    }

    printf("Evaluation time: %lu ns\n", result.eval_time_ns);
}

nimcp_policy_result_free(&result);
```

### Custom Functions

```c
nimcp_error_t custom_is_admin(
    const nimcp_policy_value_t* args,
    size_t num_args,
    nimcp_policy_value_t* result,
    void* user_data)
{
    if (num_args != 1 || args[0].type != NIMCP_POLICY_VALUE_STRING) {
        return NIMCP_ERROR_INVALID_ARGUMENT;
    }

    result->type = NIMCP_POLICY_VALUE_BOOL;
    result->bool_val = strcmp(args[0].string_val, "admin") == 0;
    return NIMCP_OK;
}

// Register function
nimcp_policy_register_function(engine, "is_admin", custom_is_admin, NULL);

// Use in policy
// rule "admin_only" {
//     condition: is_admin(user.role)
//     action: ALLOW
// }
```

### Event Callbacks

```c
void policy_event_handler(
    const char* event_type,
    const nimcp_policy_result_t* result,
    void* user_data)
{
    printf("Policy event: %s, action: %d\n", event_type, result->action);
}

nimcp_policy_register_callback(engine, policy_event_handler, NULL);
```

### Statistics

```c
nimcp_policy_stats_t stats;
nimcp_policy_engine_get_stats(engine, &stats);

printf("Policies: %zu\n", stats.num_policies);
printf("Evaluations: %lu\n", stats.total_evaluations);
printf("Avg time: %lu ns\n", stats.avg_eval_time_ns);
printf("Cache hits: %lu\n", stats.cache_hits);

// Reset statistics
nimcp_policy_engine_reset_stats(engine);
```

## Example Policies

### SQL Injection Detection

```nspl
rule "block_sql_injection" {
    condition: contains(input, "SELECT") AND contains(input, "FROM")
    action: DENY
    severity: HIGH
    log: true
    message: "SQL injection attempt detected"
}
```

### API Access Control

```nspl
policy "api_access" {
    rule "require_auth" {
        condition: NOT authenticated
        action: DENY
        message: "Authentication required"
    }

    rule "rate_limit" {
        condition: rate > 100
        action: THROTTLE
    }
}
```

### Input Validation

```nspl
rule "validate_input" {
    condition: (
        length(input) > 1000 OR
        entropy(input) > 0.8 OR
        contains(input, "script") OR
        contains(input, "eval")
    )
    action: DENY
    severity: MEDIUM
    message: "Invalid or suspicious input detected"
}
```

### Role-Based Access

```nspl
policy "admin_access" {
    rule "check_role" {
        condition: user.role == "admin" OR user.role == "superuser"
        action: ALLOW
    }

    rule "default_deny" {
        condition: true
        action: DENY
        message: "Insufficient privileges"
    }
}
```

### Complex Detection

```nspl
rule "complex_threat_detection" {
    condition: (
        length(input) > 1000 AND
        entropy(input) > 0.8 AND
        (contains(input, "script") OR
         contains(input, "eval") OR
         contains(input, "exec"))
    )
    action: DENY
    severity: CRITICAL
    log: true
    message: "Complex threat pattern detected"
}
```

## Bio-Async Integration

The policy engine integrates with NIMCP's bio-async system for event-driven operation:

```c
// Engine receives messages
// - "policy.reload": Reload all policies from files
// - "policy.evaluate": Trigger policy evaluation

// Engine sends messages
// - "policy.loaded": When a policy is successfully loaded
// - "policy.evaluated": After each evaluation
```

## Performance

- Simple policy evaluation: < 100 microseconds
- Complex policy evaluation: < 500 microseconds
- Supports caching for improved performance
- Thread-safe for concurrent evaluations
- Optimized bytecode compilation

## Error Handling

The engine provides detailed error reporting:

```c
char* error_msg = NULL;
nimcp_ast_node_t* ast = nimcp_policy_parse(text, "policy.nspl", &error_msg);
if (!ast) {
    printf("Parse error: %s\n", error_msg);
    free(error_msg);
}
```

Parse errors include:
- Line and column numbers
- Clear error messages
- Syntax error descriptions

## Testing

Comprehensive test coverage includes:

1. **Unit Tests** (`test/unit/security/`)
   - Parser tests: All language features
   - Evaluator tests: All operators and functions

2. **Integration Tests** (`test/integration/security/`)
   - End-to-end policy evaluation
   - Bio-async integration
   - File-based policies
   - Custom functions

3. **Regression Tests** (`test/regression/security/`)
   - Performance benchmarks
   - Memory leak detection
   - Stress tests
   - Edge cases

## Best Practices

1. **Keep policies simple**: Complex conditions are harder to debug
2. **Use meaningful names**: Rule names should describe their purpose
3. **Add comments**: Use `#` for inline documentation
4. **Test policies**: Write unit tests for critical policies
5. **Monitor performance**: Use statistics to track evaluation times
6. **Version policies**: Keep policy files under version control
7. **Use default deny**: Always have a catch-all deny rule

## Limitations

- No regex support (use `matches()` for simple patterns)
- Limited arithmetic operations
- No loops or recursion
- Function call depth not limited (watch for deep nesting)
- String comparisons are case-sensitive

## Future Enhancements

- Regular expression support
- Time-based conditions (schedules)
- IP address/CIDR matching
- List/array operations
- Policy inheritance
- Macro definitions
- Policy composition
- Real-time policy updates via network
