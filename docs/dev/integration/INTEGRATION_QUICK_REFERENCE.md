# Integration Quick Reference Card

## Module ID Quick Lookup

### Buffering (0x0510-0x0514)
- 0x0510  nimcp_circular_buffer
- 0x0511  nimcp_integration_buffer
- 0x0512  nimcp_phase_coded_buffer
- 0x0513  nimcp_sliding_window
- 0x0514  nimcp_temporal_accumulator

### Cognitive (0x0515-0x0516)
- 0x0515  nimcp_cognitive_adapters
- 0x0516  nimcp_working_memory_adapter

### Encoding (0x0517-0x0519)
- 0x0517  nimcp_population_coding
- 0x0518  nimcp_rate_coding
- 0x0519  nimcp_temporal_coding

### Integration (0x051B-0x051F)
- 0x051B  nimcp_executive_middleware_adapter
- 0x051C  nimcp_flow_tracker
- 0x051D  nimcp_middleware_controller
- 0x051E  nimcp_quantum_command_propagator
- 0x051F  nimcp_shannon_monitor

### Routing (0x0529-0x052C)
- 0x0529  nimcp_attention_gate
- 0x052A  nimcp_routing_table
- 0x052B  nimcp_signal_wrapper
- 0x052C  nimcp_thalamic_router

### IO (0x052D-0x052F)
- 0x052D  nimcp_dataio
- 0x052E  nimcp_serialization
- 0x052F  nimcp_stream

## Standard Template

```c
#include "async/nimcp_bio_async.h"
#include "async/nimcp_bio_router.h"
#include "async/nimcp_bio_messages.h"
#include "utils/logging/nimcp_logging.h"
#include "utils/memory/nimcp_unified_memory.h"

#define LOG_MODULE "module_name"
#define LOG_MODULE_ID 0xXXXX
```

## Memory: malloc → nimcp_malloc, free → nimcp_free
## Logging: LOG_MODULE_INFO(MODULE, "message")
## Status: ✅ 37/37 files integrated (100%)
