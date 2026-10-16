# Brain Modules Quick Reference

**Integration Date:** 2025-11-28  
**Status:** ✅ COMPLETE

---

## Module ID Reference

All brain submodules use IDs in range **0x0110-0x012F**

```c
/* Brain submodules (defined in include/async/nimcp_bio_messages.h) */
BIO_MODULE_BRAIN_BIOLOGICAL         = 0x0110  // Glial, neuromodulators, multimodal
BIO_MODULE_BRAIN_ACCESSORS          = 0x0111  // Safe accessor methods
BIO_MODULE_BRAIN_OSCILLATIONS       = 0x0112  // Complex phasor oscillations
BIO_MODULE_BRAIN_PROCESSING         = 0x0113  // Processing subsystem
BIO_MODULE_BRAIN_BROCA              = 0x0114  // Broca's area (language)
BIO_MODULE_BRAIN_LEARNING           = 0x0115  // Learning subsystem
BIO_MODULE_BRAIN_COGNITIVE          = 0x0116  // Cognitive integration
BIO_MODULE_BRAIN_ANALYSIS           = 0x0117  // Topology analysis
BIO_MODULE_BRAIN_PRETRAINED         = 0x0118  // Pretrained models
BIO_MODULE_BRAIN_INFORMATION        = 0x0119  // Information theory (Shannon)
BIO_MODULE_BRAIN_DISTRIBUTED        = 0x011A  // Distributed brain
BIO_MODULE_BRAIN_STRATEGY           = 0x011B  // Strategy pattern
BIO_MODULE_BRAIN_FACTORY            = 0x011C  // Brain factory
BIO_MODULE_BRAIN_FACTORY_INIT       = 0x011D  // Factory initialization
BIO_MODULE_BRAIN_FACTORY_VALIDATION = 0x011E  // Factory validation
BIO_MODULE_BRAIN_PERSISTENCE        = 0x011F  // Persistence layer
BIO_MODULE_BRAIN_INFERENCE          = 0x0120  // Inference engine
BIO_MODULE_BRAIN_LANGUAGE_PRODUCTION= 0x0121  // Language production
BIO_MODULE_BRAIN_SYNTAX             = 0x0122  // Syntax processor
BIO_MODULE_BRAIN_SPEECH_MOTOR       = 0x0123  // Speech motor
BIO_MODULE_BRAIN_PHONOLOGICAL       = 0x0124  // Phonological
BIO_MODULE_BRAIN_MULTIMODAL         = 0x0125  // Multimodal integrator
BIO_MODULE_BRAIN_SENSORY            = 0x0126  // Sensory extractor
BIO_MODULE_BRAIN_CIRCUIT_COMPILATION= 0x0127  // Circuit compilation
BIO_MODULE_BRAIN_REASONING          = 0x0128  // Reasoning learning
BIO_MODULE_BRAIN_ASSOCIATION        = 0x0129  // Association learning
BIO_MODULE_BRAIN_RULE               = 0x012A  // Rule learning
```

---

## LOG_MODULE Reference

| File | LOG_MODULE |
|------|------------|
| `nimcp_brain_biological.c` | `BRAIN_BIOLOGICAL` |
| `nimcp_brain_accessors.c` | `BRAIN_ACCESSORS` |
| `nimcp_brain_complex_oscillations.c` | `BRAIN_OSCILLATIONS` |
| `cognitive_processor.c` | `BRAIN_PROC_COG` |
| `multimodal_integrator.c` | `BRAIN_PROC_MM` |
| `sensory_extractor.c` | `BRAIN_PROC_SENS` |
| `nimcp_language_production_bridge.c` | `BROCA_LANG_PROD` |
| `nimcp_syntax_processor.c` | `BROCA_SYNTAX` |
| `nimcp_speech_motor.c` | `BROCA_SPEECH` |
| `nimcp_phonological.c` | `BROCA_PHONO` |
| `nimcp_broca_adapter.c` | `BRAIN_LEARNING` |
| `nimcp_brain_learning.c` | `BRAIN_LEARNING` |
| `nimcp_circuit_compilation.c` | `BRAIN_LEARN_CIRC` |
| `nimcp_reasoning_learning.c` | `BRAIN_LEARN_REASON` |
| `nimcp_association_learning.c` | `BRAIN_LEARN_ASSOC` |
| `nimcp_rule_learning.c` | `BRAIN_LEARN_RULE` |
| `nimcp_brain_cognitive.c` | `BRAIN_COGNITIVE` |
| `nimcp_brain_topology.c` | `BRAIN_TOPOLOGY` |
| `nimcp_brain_pretrained.c` | `BRAIN_PRETRAINED` |
| `nimcp_brain_shannon.c` | `BRAIN_INFO` |
| `nimcp_brain_distributed.c` | `BRAIN_DISTRIBUTED` |
| `nimcp_brain_strategy.c` | `BRAIN_STRATEGY` |
| `nimcp_brain_factory.c` | `BRAIN_FACTORY` |
| `nimcp_brain_init.c` | `BRAIN_INIT` |
| `nimcp_brain_validation.c` | `BRAIN_VALID` |
| `nimcp_brain_persistence.c` | `BRAIN_PERSIST` |
| `nimcp_brain_inference.c` | `BRAIN_INFERENCE` |

---

## Common Patterns

### 1. Logging

```c
LOG_DEBUG("Function entry: %s", __func__);
LOG_INFO("Initialized %s with capacity=%u", component_name, capacity);
LOG_WARN("Performance degradation: latency=%fms exceeds threshold", latency);
LOG_ERROR("Failed to allocate memory: size=%zu", requested_size);
```

### 2. Memory Allocation

```c
// Allocate
my_struct_t* obj = nimcp_malloc(sizeof(my_struct_t));
if (!obj) {
    LOG_ERROR("Failed to allocate my_struct");
    return NULL;
}

// Allocate and zero
float* buffer = nimcp_calloc(count, sizeof(float));

// Free
nimcp_free(obj);
```

### 3. Bio-Router Registration (Future)

```c
bio_module_info_t info = {
    .module_id = BIO_MODULE_BRAIN_XXX,
    .module_name = "brain_xxx",
    .inbox_capacity = 100,
    .user_data = brain
};

bio_module_context_t ctx = bio_router_register_module(&info);
if (!ctx) {
    LOG_ERROR("Failed to register with bio-router");
    return false;
}

// Register message handlers
bio_router_register_handler(ctx, BIO_MSG_BRAIN_STATE_QUERY, handle_state_query);
```

---

## File Locations

### Source Files
- `/home/bbrelin/nimcp/src/core/brain/` - All brain modules

### Headers
- `/home/bbrelin/nimcp/include/async/nimcp_bio_messages.h` - Module IDs and message types
- `/home/bbrelin/nimcp/include/async/nimcp_bio_async.h` - Bio-async core
- `/home/bbrelin/nimcp/include/async/nimcp_bio_router.h` - Message router
- `/home/bbrelin/nimcp/include/utils/logging/nimcp_logging.h` - Logging macros
- `/home/bbrelin/nimcp/include/utils/memory/nimcp_unified_memory.h` - Memory functions

### Documentation
- `/home/bbrelin/nimcp/BRAIN_BIO_ASYNC_INTEGRATION_SUMMARY.md` - Integration summary
- `/home/bbrelin/nimcp/BRAIN_INTEGRATION_VERIFICATION.md` - Verification report
- `/home/bbrelin/nimcp/BRAIN_MODULE_QUICK_REFERENCE.md` - This file

### Scripts
- `/home/bbrelin/nimcp/scripts/integrate_brain_bio_async.py` - Batch integration tool
- `/home/bbrelin/nimcp/scripts/integrate_brain_bio_async_part2.py` - Secondary integration
- `/home/bbrelin/nimcp/scripts/integrate_brain_module.sh` - Shell integration helper

---

## Verification Commands

```bash
# Check bio-async integration
grep -l "nimcp_bio_async.h" src/core/brain/**/*.c | wc -l
# Expected: 30+

# Check logging integration
grep -l "LOG_MODULE" src/core/brain/**/*.c | wc -l
# Expected: 30+

# Check unified memory usage
grep -c "nimcp_malloc\|nimcp_calloc\|nimcp_free" src/core/brain/**/*.c
# Expected: 200+

# Compile brain modules
cd build && make -j4
# Expected: 0 errors in brain modules
```

---

## Next Steps

1. **Implement Message Handlers**
   - Add bio-router registration in initialization functions
   - Implement handlers for BIO_MSG_BRAIN_* messages
   - Subscribe to relevant neuromodulator channels

2. **Add Comprehensive Logging**
   - LOG_DEBUG at function entry points
   - LOG_INFO for major state changes
   - LOG_ERROR for all error conditions
   - LOG_WARN for performance/threshold issues

3. **Implement Predictive Signaling**
   - Publish brain activity changes via dopamine channel
   - Publish attention shifts via acetylcholine channel
   - Use predictive coding for efficient messaging

---

**Last Updated:** 2025-11-28  
**Maintained By:** NIMCP Development Team
