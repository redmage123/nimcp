# Neural Logic Connectivity API Reference

## Overview

The neural logic connectivity API enables building functional logic circuits by connecting individual logic gates with weighted synapses. This allows construction of complex computational structures from primitive AND/OR/NOT/XOR/IMPLIES gates.

## Quick Start

```c
#include "core/neuron_types/nimcp_neural_logic.h"

// 1. Create network
neural_logic_config_t config = neural_logic_default_config(100);
neural_logic_network_t network = neural_logic_create(&config);

// 2. Create gates
uint32_t and_gate = neural_logic_create_gate(network, LOGIC_GATE_AND, 1.8f);
uint32_t or_gate = neural_logic_create_gate(network, LOGIC_GATE_OR, 1.0f);

// 3. Connect gates
neural_logic_connect(network, and_gate, or_gate, 1.0f);

// 4. Evaluate circuit
float inputs[] = {1.0f, 1.0f};
float output;
neural_logic_evaluate(network, and_gate, inputs, 2, &output);

// 5. Simulate propagation
neural_logic_update(network, timestamp, delta_t);

// 6. Cleanup
neural_logic_destroy(network);
```

## Core Function: neural_logic_connect()

### Signature
```c
bool neural_logic_connect(
    neural_logic_network_t network,
    uint32_t source_id,
    uint32_t target_id,
    float weight
);
```

### Parameters
- `network`: Neural logic network instance
- `source_id`: Source neuron ID (from `neural_logic_create_gate()`)
- `target_id`: Target neuron ID (from `neural_logic_create_gate()`)
- `weight`: Synaptic weight (positive=excitatory, negative=inhibitory)

### Returns
- `true`: Connection created successfully
- `false`: Error (invalid IDs, invalid weight, allocation failure)

### Constraints
- Source and target IDs must be valid (< neurons_count)
- Weight must be finite (not NaN or Infinity)
- Network must be non-NULL

### Behavior
- Creates weighted synapse from source to target
- Supports multiple connections between same pair (parallel synapses)
- Supports self-connections (source == target)
- Supports bidirectional connections (A→B and B→A)
- **Complexity**: O(1) amortized

### Examples

#### Basic Connection
```c
uint32_t gate_a = neural_logic_create_gate(network, LOGIC_GATE_AND, 1.8f);
uint32_t gate_b = neural_logic_create_gate(network, LOGIC_GATE_OR, 1.0f);

// Excitatory connection (positive weight)
neural_logic_connect(network, gate_a, gate_b, 1.0f);
```

#### Inhibitory Connection
```c
uint32_t excitatory = neural_logic_create_gate(network, LOGIC_GATE_AND, 1.8f);
uint32_t inhibitory = neural_logic_create_gate(network, LOGIC_GATE_NOT, 0.5f);

// Inhibitory connection (negative weight)
neural_logic_connect(network, inhibitory, excitatory, -1.0f);
```

#### Fan-Out (One-to-Many)
```c
uint32_t source = neural_logic_create_gate(network, LOGIC_GATE_AND, 1.8f);
uint32_t target1 = neural_logic_create_gate(network, LOGIC_GATE_OR, 1.0f);
uint32_t target2 = neural_logic_create_gate(network, LOGIC_GATE_XOR, 1.5f);
uint32_t target3 = neural_logic_create_gate(network, LOGIC_GATE_NOT, 0.5f);

// One source broadcasts to three targets
neural_logic_connect(network, source, target1, 1.0f);
neural_logic_connect(network, source, target2, 0.8f);
neural_logic_connect(network, source, target3, 0.5f);
```

#### Fan-In (Many-to-One)
```c
uint32_t input_a = neural_logic_create_gate(network, LOGIC_GATE_OR, 0.1f);
uint32_t input_b = neural_logic_create_gate(network, LOGIC_GATE_OR, 0.1f);
uint32_t input_c = neural_logic_create_gate(network, LOGIC_GATE_OR, 0.1f);
uint32_t output = neural_logic_create_gate(network, LOGIC_GATE_AND, 1.8f);

// Three inputs converge to one output
neural_logic_connect(network, input_a, output, 1.0f);
neural_logic_connect(network, input_b, output, 1.0f);
neural_logic_connect(network, input_c, output, 0.5f);
```

#### Recurrent Connection (Feedback Loop)
```c
uint32_t gate_a = neural_logic_create_gate(network, LOGIC_GATE_AND, 1.8f);
uint32_t gate_b = neural_logic_create_gate(network, LOGIC_GATE_OR, 1.0f);

// Bidirectional (A↔B feedback loop)
neural_logic_connect(network, gate_a, gate_b, 1.0f);
neural_logic_connect(network, gate_b, gate_a, 0.5f);
```

#### Self-Connection (Autosynapse)
```c
uint32_t gate = neural_logic_create_gate(network, LOGIC_GATE_OR, 1.0f);

// Self-connection (maintains activity)
neural_logic_connect(network, gate, gate, 0.3f);
```

## Signal Propagation

### Mechanism
After `neural_logic_update()`:
1. Each neuron computes output based on gate type
2. Active neurons (output > 0.5) propagate signals
3. For each outgoing synapse:
   - Weighted output accumulated in target inputs
   - Load balanced across input_a / input_b channels

### Propagation Equation
```
target.input_a += source.output * synapse.weight
```

### Temporal Dynamics
- Inputs decay exponentially: `I(t) = I(0) * exp(-t/τ)`
- Tau (integration window) typically 5ms
- Synaptic delays: ~1 timestep (100μs)

### Example: Multi-Step Propagation
```c
// Create 3-gate chain: A → B → C
uint32_t gate_a = neural_logic_create_gate(network, LOGIC_GATE_OR, 0.1f);
uint32_t gate_b = neural_logic_create_gate(network, LOGIC_GATE_OR, 1.0f);
uint32_t gate_c = neural_logic_create_gate(network, LOGIC_GATE_OR, 1.0f);

neural_logic_connect(network, gate_a, gate_b, 1.0f);
neural_logic_connect(network, gate_b, gate_c, 1.0f);

// Evaluate gate A to inject signal
float inputs[] = {1.0f};
float output;
neural_logic_evaluate(network, gate_a, inputs, 1, &output);

// Simulate propagation (signal reaches C after ~2 timesteps)
uint64_t timestamp = 0;
for (int i = 0; i < 10; i++) {
    neural_logic_update(network, timestamp, 100);
    timestamp += 100;
}

// Check if signal reached gate C
logic_neuron_state_t state_c;
neural_logic_get_state(network, gate_c, &state_c);
printf("Gate C output: %.2f\n", state_c.output_state);
```

## Common Circuit Patterns

### Half Adder
```c
// Sum = A XOR B
// Carry = A AND B
uint32_t sum = neural_logic_create_gate(network, LOGIC_GATE_XOR, 0.5f);
uint32_t carry = neural_logic_create_gate(network, LOGIC_GATE_AND, 1.8f);

// Test: 1 + 1 = Sum:0, Carry:1
float inputs[] = {1.0f, 1.0f};
float sum_out, carry_out;
neural_logic_evaluate(network, sum, inputs, 2, &sum_out);      // 0.0
neural_logic_evaluate(network, carry, inputs, 2, &carry_out);  // 1.0
```

### Full Adder
```c
// Sum = A XOR B XOR Cin
// Cout = (A AND B) OR (Cin AND (A XOR B))
uint32_t xor_ab = neural_logic_create_gate(network, LOGIC_GATE_XOR, 0.5f);
uint32_t xor_sum = neural_logic_create_gate(network, LOGIC_GATE_XOR, 0.5f);
uint32_t and_ab = neural_logic_create_gate(network, LOGIC_GATE_AND, 1.8f);
uint32_t and_cin = neural_logic_create_gate(network, LOGIC_GATE_AND, 1.8f);
uint32_t or_cout = neural_logic_create_gate(network, LOGIC_GATE_OR, 1.0f);

neural_logic_connect(network, xor_ab, xor_sum, 1.0f);
neural_logic_connect(network, xor_ab, and_cin, 1.0f);
neural_logic_connect(network, and_ab, or_cout, 1.0f);
neural_logic_connect(network, and_cin, or_cout, 1.0f);
```

### NAND Gate (Universal Gate)
```c
// NAND = NOT(AND)
uint32_t and_gate = neural_logic_create_gate(network, LOGIC_GATE_AND, 1.8f);
uint32_t not_gate = neural_logic_create_gate(network, LOGIC_GATE_NOT, 0.5f);

neural_logic_connect(network, and_gate, not_gate, 1.0f);

// All logic can be built from NAND gates
```

### 2:1 Multiplexer
```c
// Out = (A AND NOT(S)) OR (B AND S)
uint32_t not_s = neural_logic_create_gate(network, LOGIC_GATE_NOT, 0.5f);
uint32_t and_a = neural_logic_create_gate(network, LOGIC_GATE_AND, 1.8f);
uint32_t and_b = neural_logic_create_gate(network, LOGIC_GATE_AND, 1.8f);
uint32_t or_out = neural_logic_create_gate(network, LOGIC_GATE_OR, 1.0f);

neural_logic_connect(network, not_s, and_a, 1.0f);  // NOT(S) → AND_A
neural_logic_connect(network, and_a, or_out, 1.0f); // AND_A → OR
neural_logic_connect(network, and_b, or_out, 1.0f); // AND_B → OR
```

### SR Latch (Memory)
```c
// Cross-coupled NOR gates
// Q = NOR(R, Q')
// Q' = NOR(S, Q)

uint32_t nor_q = neural_logic_create_gate(network, LOGIC_GATE_OR, 1.0f);
uint32_t nor_qbar = neural_logic_create_gate(network, LOGIC_GATE_OR, 1.0f);
uint32_t not_q = neural_logic_create_gate(network, LOGIC_GATE_NOT, 0.5f);
uint32_t not_qbar = neural_logic_create_gate(network, LOGIC_GATE_NOT, 0.5f);

// Cross-couple
neural_logic_connect(network, nor_q, not_q, 1.0f);
neural_logic_connect(network, nor_qbar, not_qbar, 1.0f);
neural_logic_connect(network, not_q, nor_qbar, 1.0f);    // Q → NOR_QBAR
neural_logic_connect(network, not_qbar, nor_q, 1.0f);    // Q' → NOR_Q
```

## Error Handling

### Invalid Neuron ID
```c
if (!neural_logic_connect(network, 999, 1000, 1.0f)) {
    // Error: neuron IDs out of range
}
```

### Invalid Weight
```c
if (!neural_logic_connect(network, source, target, NAN)) {
    // Error: weight is not finite
}

if (!neural_logic_connect(network, source, target, INFINITY)) {
    // Error: weight is not finite
}
```

### Null Network
```c
if (!neural_logic_connect(NULL, 0, 1, 1.0f)) {
    // Error: network is NULL
}
```

## Performance Characteristics

| Operation | Complexity | Notes |
|-----------|-----------|-------|
| `neural_logic_connect()` | O(1) | Prepend to linked list |
| Signal propagation | O(E) | E = active synapses |
| Network update | O(N + E) | N neurons + E synapses |
| Memory per synapse | 16 bytes | target_id + weight + next pointer |
| Memory per network | O(N + E) | Linear in neurons + synapses |

## Debugging Tips

### Visualize Connectivity
```c
// Get statistics
uint32_t total_gates = 0;
neural_logic_get_stats(network, &total_gates, NULL, NULL, NULL, NULL);
printf("Total gates: %u\n", total_gates);

// Inspect individual neurons
for (uint32_t i = 0; i < total_gates; i++) {
    logic_neuron_state_t state;
    neural_logic_get_state(network, i, &state);
    printf("Gate %u: type=%s, output=%.2f\n",
           i,
           neural_logic_gate_name(state.gate_type),
           state.output_state);
}
```

### Enable Debug Logging
```c
// In nimcp_neural_logic.c, connections are logged:
// NIMCP_LOGGING_DEBUG("Synapse connected: %u -> %u (weight=%.3f)", ...)
```

### Verify Signal Flow
```c
// Evaluate source, update network, check target
float inputs[] = {1.0f};
float output;
neural_logic_evaluate(network, source_id, inputs, 1, &output);

neural_logic_update(network, 0, 100);

logic_neuron_state_t target_state;
neural_logic_get_state(network, target_id, &target_state);
printf("Target input A: %.2f\n", target_state.input_a_activity);
printf("Target input B: %.2f\n", target_state.input_b_activity);
```

## Best Practices

1. **Validate connections**: Always check return value of `neural_logic_connect()`
2. **Use meaningful weights**: 1.0 for standard excitatory, -1.0 for inhibitory
3. **Avoid cycles**: Unless implementing recurrent networks (e.g., latches)
4. **Test incrementally**: Build and test small subcircuits before combining
5. **Document topology**: Comment circuit structure for maintainability
6. **Use helper functions**: Extract common patterns (adders, muxes) into functions

## See Also

- `nimcp_neural_logic.h`: Full API documentation
- `test_neural_logic_connectivity.cpp`: Unit test examples
- `test_neural_logic_circuits.cpp`: Circuit construction examples
- `NEURAL_LOGIC_CONNECTIVITY_IMPLEMENTATION.md`: Implementation details

## Support

For questions or issues with the connectivity API:
- Check test files for usage examples
- Review NIMCP logging output for connection errors
- Verify neuron IDs are within valid range
- Ensure weights are finite (not NaN/Infinity)
