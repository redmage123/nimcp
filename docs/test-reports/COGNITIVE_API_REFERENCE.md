# Cognitive Modules API Reference

**Date:** 2025-11-28
**Purpose:** Document correct API usage for cognitive modules

## Summary of Correct APIs

### 1. Network Analyzer

```c
network_analyzer_t* network_analyzer_create(brain_t brain);
```

- **Parameters:**
  - `brain` - Brain instance (REQUIRES non-NULL)
- **Returns:** `network_analyzer_t*` or NULL on error
- **Notes:** Returns NULL if brain is NULL (validation failure)

**Example:**
```c
brain_config_t config = brain_default_config();
brain_t brain = brain_create_custom(&config);
network_analyzer_t* analyzer = network_analyzer_create(brain);
if (analyzer) {
    network_analyzer_destroy(analyzer);
}
brain_destroy(brain);
```

---

### 2. Global Workspace

```c
global_workspace_t* global_workspace_create(void);
```

- **Parameters:** NONE
- **Returns:** `global_workspace_t*` or NULL on error
- **Notes:** Use `global_workspace_create_custom(config*)` for custom configuration

**Example:**
```c
global_workspace_t* workspace = global_workspace_create();
if (workspace) {
    global_workspace_destroy(workspace);
}
```

---

### 3. Knowledge System

```c
knowledge_system_t knowledge_system_create(const char* learner_name);
```

- **Parameters:**
  - `learner_name` - String name for the learner
- **Returns:** `knowledge_system_t` (opaque handle) or NULL on error
- **Notes:** Internally creates a brain, so inherits brain initialization requirements

**Example:**
```c
knowledge_system_t system = knowledge_system_create("my_learner");
if (system) {
    knowledge_system_destroy(system);
}
```

---

### 4. Mirror Neurons

```c
mirror_neurons_t mirror_neurons_create(const mirror_neuron_config_t* config);
mirror_neuron_config_t mirror_neurons_get_default_config(void);
```

- **Parameters:**
  - `config` - Configuration pointer (NULL = use defaults)
- **Returns:** `mirror_neurons_t` (typedef for `mirror_neurons_system_t*`)
- **Notes:** Type is `mirror_neurons_t`, NOT `mirror_neurons_system_t*`

**Example:**
```c
mirror_neuron_config_t config = mirror_neurons_get_default_config();
config.num_mirror_neurons = 500;
config.learning_rate = 0.02f;

mirror_neurons_t mirror = mirror_neurons_create(&config);
if (mirror) {
    mirror_neurons_destroy(mirror);
}
```

---

### 5. Predictive Network

```c
predictive_network_t predictive_create(const predictive_config_t* config);
predictive_config_t predictive_default_config(void);
```

- **Parameters:**
  - `config` - Configuration pointer (NULL = use defaults)
- **Returns:** `predictive_network_t` (opaque handle) or NULL on error

**Example:**
```c
predictive_config_t config = predictive_default_config();
// Customize config if needed

predictive_network_t net = predictive_create(&config);
if (net) {
    predictive_destroy(net);
}
```

---

## Common Mistakes to Avoid

### ❌ WRONG:
```c
// DON'T: Pass wrong arguments
global_workspace_t* ws = global_workspace_create(config);  // WRONG - takes NO args

// DON'T: Use wrong type
mirror_neurons_system_t* mirror = mirror_neurons_create(config);  // WRONG type

// DON'T: Expect NULL brain to work
network_analyzer_t* analyzer = network_analyzer_create(NULL);  // Returns NULL
```

### ✅ CORRECT:
```c
// DO: Use correct signature
global_workspace_t* ws = global_workspace_create();  // NO args

// DO: Use correct type
mirror_neurons_t mirror = mirror_neurons_create(&config);  // mirror_neurons_t

// DO: Create brain first
brain_t brain = brain_create_custom(&brain_config);
network_analyzer_t* analyzer = network_analyzer_create(brain);
```

---

## API Compatibility Matrix

| Module | Requires Brain | Config Parameter | Returns Handle |
|--------|----------------|------------------|----------------|
| network_analyzer | Yes (non-NULL) | No | `network_analyzer_t*` |
| global_workspace | No | Optional (via create_custom) | `global_workspace_t*` |
| knowledge_system | No (creates internally) | No | `knowledge_system_t` |
| mirror_neurons | No | Optional (NULL=defaults) | `mirror_neurons_t` |
| predictive | No | Optional (NULL=defaults) | `predictive_network_t` |

---

## Test Environment Challenges

Some modules have initialization challenges in isolated test environments:

1. **network_analyzer** - Requires full brain setup
2. **knowledge_system** - Internally creates brain, hits initialization issues
3. **predictive** - Stack allocation issues in predictive_default_config()

For integration testing, ensure proper environment setup including:
- Unified memory initialization
- Logging system initialization
- Platform-specific threading setup

---

## Quick Reference Card

```c
// Module creation patterns:

// 1. No arguments
global_workspace_t* ws = global_workspace_create();

// 2. String argument
knowledge_system_t ks = knowledge_system_create("learner_name");

// 3. Config pointer (NULL=defaults)
mirror_neurons_t mn = mirror_neurons_create(&config);  // or NULL
predictive_network_t pn = predictive_create(&config);  // or NULL

// 4. Requires brain
brain_t brain = brain_create_custom(&brain_config);
network_analyzer_t* na = network_analyzer_create(brain);
```

---

## Version Information

- **NIMCP Version:** 2.8+
- **Last Updated:** 2025-11-28
- **Maintainer:** NIMCP Development Team
