# Brain Learning Modules - Quick Reference Guide

## Setup Bio-Async Integration

```c
// 1. Create brain with bio-async context
brain_config_t config = {
    .num_inputs = 10,
    .num_outputs = 3,
    .learning_rate = 0.01f,
    // ... other config
};

brain_t brain = brain_create(&config);

// 2. Create and attach bio-async context
bio_ctx_t bio_ctx = bio_ctx_create();
brain->bio_ctx = bio_ctx;

// 3. Register learning module with bio-router
brain_learning_register_bio_async(brain);

// 4. Subscribe to learning events (optional)
bio_ctx_subscribe(bio_ctx, BIO_MSG_BRAIN_LEARN_COMPLETE,
                  my_handler, my_data);
```

---

## Basic Learning

```c
// Supervised learning from single example
float features[10] = {0.1f, 0.2f, ..., 1.0f};
const char* label = "class_A";
float confidence = 0.9f;

float loss = brain_learn_example(brain, features, 10, label, confidence);

if (loss < 0.0f) {
    // Learning failed - check logs for security/validation errors
} else {
    // Success - loss value indicates error magnitude
}

// Process bio-async events
bio_ctx_process(bio_ctx);
```

---

## Batch Learning

```c
// Learn from multiple examples efficiently
brain_example_t examples[10];
// ... populate examples

float avg_loss = brain_learn_batch(brain, examples, 10);
```

---

## Association Learning

```c
// Learn statistical associations
bool success = brain_learn_association(brain,
                                       "concept_A",
                                       "concept_B",
                                       10);  // cooccurrence count

// Query association strength
float strength = get_association_strength(brain, "concept_A", "concept_B");
// Returns: 0.0-1.0 (or -1.0 if not found)
```

---

## Rule Learning

```c
// Extract rules from examples
rule_example_t examples[100];
const char* labels[100];
// ... populate

int rules_learned = brain_learn_rule_from_examples(brain,
                                                    examples,
                                                    labels,
                                                    100);
```

---

## Async Learning

```c
// Non-blocking learning
nimcp_future_t future = nimcp_brain_learn_async(brain,
                                                  features, 10,
                                                  "label", 0.9f);

// Do other work...

// Wait for result
float loss;
nimcp_error_t err = nimcp_future_wait(future, &loss, 5000);  // 5s timeout

if (err == NIMCP_SUCCESS) {
    printf("Async learning completed: loss=%.4f\n", loss);
}

nimcp_future_destroy(future);
```

---

## Reward-Based Learning

```c
// Reinforcement learning workflow
float features[10] = {...};
float output[3];

// 1. Forward pass (builds eligibility traces)
brain_predict(brain, features, 10, output, 3);

// 2. Evaluate outcome
float reward = (outcome_good) ? 1.0f : -0.5f;

// 3. Apply reward to update weights
uint32_t synapses_modified = brain_apply_reward_learning(brain, reward);
```

---

## Security Best Practices

### DO:
- ✅ Validate inputs before passing to learning functions
- ✅ Check return values (negative = error)
- ✅ Monitor logs for security warnings
- ✅ Use confidence in range [0.0, 1.0]
- ✅ Keep labels under 256 characters

### DON'T:
- ❌ Pass NaN/Inf in feature vectors
- ❌ Use format strings in labels (%s, %n, etc.)
- ❌ Include SQL patterns in labels ('; DROP, etc.)
- ❌ Pass null pointers
- ❌ Ignore negative return values

### Example: Safe Input Validation

```c
bool validate_input(const float* features, uint32_t n,
                   const char* label, float confidence) {
    // Check pointers
    if (!features || !label) return false;

    // Check confidence range
    if (confidence < 0.0f || confidence > 1.0f) return false;

    // Check for NaN/Inf
    for (uint32_t i = 0; i < n; i++) {
        if (isnan(features[i]) || isinf(features[i])) return false;
    }

    // Check label length
    size_t len = strlen(label);
    if (len == 0 || len > 256) return false;

    // Check for malicious patterns
    if (strstr(label, "%s") || strstr(label, "';")) return false;

    return true;
}
```

---

## Bio-Async Event Handling

### Subscribing to Events

```c
void my_learning_handler(bio_ctx_t ctx, bio_msg_t msg, void* user_data) {
    // Extract data from message
    const char* label = bio_msg_get_string(msg, "label", NULL);
    float loss = bio_msg_get_float(msg, "loss", -1.0f);
    float confidence = bio_msg_get_float(msg, "confidence", 0.0f);

    // Process event
    printf("Learning complete: label=%s, loss=%.4f\n", label, loss);

    // Optionally send response
    if (user_data) {
        // ... custom logic
    }
}

// Subscribe
bio_ctx_subscribe(bio_ctx, BIO_MSG_BRAIN_LEARN_COMPLETE,
                  my_learning_handler, my_data);
```

### Message Types

| Message | When Published | Key Fields |
|---------|---------------|------------|
| **BIO_MSG_TRAINING_STEP** | Start of learning episode | label, num_features, confidence, step |
| **BIO_MSG_BRAIN_LEARN_COMPLETE** | End of learning episode | label, loss, confidence, dopamine_strength |
| **BIO_MSG_ASSOCIATION_FORMED** | Association created/updated | antecedent, consequent, strength, delta |
| **BIO_MSG_RULE_LEARNED** | Rule extracted | rule, label, confidence, support_count |
| **BIO_MSG_CIRCUIT_COMPILED** | Circuit compiled | rule_str, circuit_id, num_gates |

---

## Logging

### Log Levels

| Level | When to Use | Example |
|-------|-------------|---------|
| **TRACE** | Detailed debugging | Function entry/exit |
| **DEBUG** | Development info | "Learning from example..." |
| **INFO** | Important events | "Learned association: A → B" |
| **WARN** | Suspicious patterns | "Extreme feature value detected" |
| **ERROR** | Failures/attacks | "Format string attack detected" |

### Enabling Detailed Logs

```bash
# Set log level via environment
export NIMCP_LOG_LEVEL=TRACE

# Or at runtime (if supported)
nimcp_logging_set_level(NIMCP_LOG_LEVEL_TRACE);
```

### Log Output Example

```
[TRACE] [BRAIN_LEARNING] brain_learn_example: label='class_A', features=10, confidence=0.900
[DEBUG] [BRAIN_LEARNING] Learning from example: label='class_A', features=10, confidence=0.900
[INFO]  [BRAIN_LEARNING] Learned association: class_A → concept_X (strength: 0.750, Δ=0.050)
[WARN]  [BRAIN_LEARN_ASSOC] Suspicious cooccurrence count: 500000 (expected 1-1000000)
[ERROR] [BRAIN_LEARNING] Security: Invalid or malicious learning input detected
```

---

## Common Patterns

### Pattern 1: Training Loop with Events

```c
for (int epoch = 0; epoch < 100; epoch++) {
    for (int i = 0; i < num_examples; i++) {
        float loss = brain_learn_example(brain,
                                        examples[i].features,
                                        examples[i].num_features,
                                        examples[i].label,
                                        1.0f);

        if (loss < 0.0f) {
            fprintf(stderr, "Learning failed at epoch %d, example %d\n",
                    epoch, i);
            break;
        }
    }

    // Process events after each epoch
    bio_ctx_process(bio_ctx);

    // Check convergence
    if (epoch % 10 == 0) {
        float test_loss = evaluate_test_set(brain, test_examples);
        printf("Epoch %d: test_loss=%.4f\n", epoch, test_loss);
    }
}
```

### Pattern 2: Building Concept Graph

```c
// Learn concepts via supervised learning
brain_learn_example(brain, cat_features, 10, "cat", 1.0f);
brain_learn_example(brain, dog_features, 10, "dog", 1.0f);

// Build associations
brain_learn_association(brain, "cat", "mammal", 20);
brain_learn_association(brain, "dog", "mammal", 20);
brain_learn_association(brain, "cat", "pet", 15);
brain_learn_association(brain, "dog", "pet", 18);

// Query graph
float cat_mammal_strength = get_association_strength(brain, "cat", "mammal");
float dog_pet_strength = get_association_strength(brain, "dog", "pet");
```

### Pattern 3: Rule Extraction and Compilation

```c
// 1. Learn from examples
int rules = brain_learn_rule_from_examples(brain, examples, labels, 100);
printf("Extracted %d rules\n", rules);

// 2. Compile to neural circuits (if using neural logic)
learned_rule_t** learned_rules;
int num_rules;
brain_learn_logical_rule(brain, logical_examples, 50,
                        &learned_rules, &num_rules,
                        true);  // compile_to_neural=true

// 3. Execute neural rules
for (int i = 0; i < num_rules; i++) {
    neural_compilation_result_t* compiled;
    brain_compile_rule_to_neural(brain, learned_rules[i], &compiled);

    // Use compiled circuit...

    neural_compilation_result_destroy(compiled);
}
```

---

## Performance Tips

1. **Batch Learning:** Use `brain_learn_batch()` for multiple examples
2. **Event Processing:** Call `bio_ctx_process()` periodically, not per example
3. **Async Learning:** Use `nimcp_brain_learn_async()` for non-critical learning
4. **Logging:** Disable TRACE/DEBUG logs in production builds
5. **Security Checks:** Validate inputs once before batch, not per example

---

## Troubleshooting

### Issue: Learning returns -1.0

**Causes:**
- Null pointer (features, label, brain)
- Feature dimension mismatch
- Invalid confidence (not in [0,1])
- NaN/Inf in features
- Malicious label detected

**Solution:** Check logs for specific error message

### Issue: No bio-async events received

**Causes:**
- Bio-async context not created
- `brain_learning_register_bio_async()` not called
- No event subscribers registered
- `bio_ctx_process()` not called

**Solution:**
```c
// Verify setup
assert(brain->bio_ctx != NULL);
assert(bio_router_is_registered(brain->bio_ctx, BIO_MODULE_BRAIN_LEARNING));
```

### Issue: Association not found

**Causes:**
- Association never learned
- Wrong concept names (case-sensitive)
- Global store cleared

**Solution:**
```c
float strength = get_association_strength(brain, "A", "B");
if (strength < 0.0f) {
    // Not found - learn it first
    brain_learn_association(brain, "A", "B", 1);
}
```

---

## API Quick Reference

### Main Learning Functions

```c
// Single example learning
float brain_learn_example(brain_t brain,
                         const float* features,
                         uint32_t num_features,
                         const char* label,
                         float confidence);

// Batch learning
float brain_learn_batch(brain_t brain,
                       const brain_example_t* examples,
                       uint32_t num_examples);

// Reward-based learning
uint32_t brain_apply_reward_learning(brain_t brain, float reward);

// Async learning
nimcp_future_t nimcp_brain_learn_async(brain_t brain,
                                        const float* features,
                                        uint32_t num_features,
                                        const char* label,
                                        float confidence);
```

### Association Learning Functions

```c
bool brain_learn_association(brain_t brain,
                            const char* A,
                            const char* B,
                            uint32_t cooccurrence_count);

float get_association_strength(brain_t brain,
                              const char* A,
                              const char* B);

float update_association_strength(brain_t brain,
                                 const char* A,
                                 const char* B,
                                 float outcome);

uint32_t decay_all_associations(brain_t brain, float decay_factor);
```

### Rule Learning Functions

```c
int brain_learn_rule_from_examples(brain_t brain,
                                   const rule_example_t* examples,
                                   const char** labels,
                                   uint32_t count);

bool extract_rule_pattern(const rule_example_t* examples,
                         uint32_t count,
                         const char* label,
                         char* rule_out,
                         size_t rule_size);

float compute_rule_confidence(uint32_t support_count,
                             uint32_t total_count);
```

### Reasoning Learning Functions

```c
bool brain_learn_logical_rule(brain_t brain,
                             const logical_example_t* examples,
                             uint32_t num_examples,
                             learned_rule_t*** learned_rules,
                             int* num_learned_rules,
                             bool compile_to_neural);

bool brain_compile_rule_to_neural(brain_t brain,
                                 const learned_rule_t* rule,
                                 neural_compilation_result_t** result);

bool brain_refine_rule_confidence(brain_t brain,
                                 const char* rule_name,
                                 bool outcome,
                                 float learning_rate);
```

### Circuit Compilation Functions

```c
circuit_id_t compile_rule_to_circuit(brain_t brain,
                                    const char* rule_str);

bool optimize_circuit(brain_t brain, circuit_id_t circuit_id);

bool verify_circuit_correctness(brain_t brain,
                               circuit_id_t circuit_id,
                               const circuit_test_case_t* test_cases,
                               uint32_t num_cases);

uint32_t get_circuit_gate_count(brain_t brain, circuit_id_t circuit_id);

bool delete_circuit(brain_t brain, circuit_id_t circuit_id);
```

---

## Testing Your Integration

### Unit Test Template

```cpp
#include <gtest/gtest.h>
extern "C" {
#include "core/brain/nimcp_brain.h"
#include "core/brain/learning/nimcp_brain_learning.h"
}

TEST(MyLearningTest, BasicLearning) {
    brain_config_t config;
    memset(&config, 0, sizeof(config));
    config.num_inputs = 10;
    config.num_outputs = 3;

    brain_t brain = brain_create(&config);
    ASSERT_NE(brain, nullptr);

    float features[10] = {0.1f, 0.2f, ..., 1.0f};
    float loss = brain_learn_example(brain, features, 10, "test", 0.9f);

    EXPECT_GE(loss, 0.0f);

    brain_destroy(brain);
}
```

### Run Tests

```bash
cd /home/bbrelin/nimcp/build
ctest -R BrainLearning -V
```

---

**Last Updated:** 2025-12-05
**Version:** 2.0
**Author:** NIMCP Development Team
