# Pass 4 Remaining P2 Remediation Plan

## Scope
Fix ~120 remaining P2 findings from the Pass 4 walkthrough that are NOT false-positive throws or wrong error codes (those were fixed in d6ac0abc9).

## Batches (ordered by safety/impact)

### Batch 1: Wrong Function Names in Error Messages (~50 fixes)
Copy-paste errors where THROW messages reference wrong function name. Mechanical find-and-fix.

**Files:**
- `src/async/nimcp_protocol_metrics.c` - 9 instances all say "protocol_metrics_destroy"
- `src/async/nimcp_semantic_compression.c` - 3 instances say wrong function
- `src/middleware/events/nimcp_event_queue.c` - 2 instances
- `src/perception/nimcp_omni_sensory_bridge.c` - 2 instances
- `src/security/logging/nimcp_security_logging_bridge.c` - 1 instance
- `src/security/logging/nimcp_security_logging_fep_bridge.c` - multiple
- `src/language/bridges/` - 6+ bridge files with wrong names
- `src/glial/integration/nimcp_glial_integration.c` - 2 instances
- `src/integration/knowledge/nimcp_sensory_kg_wiring.c` - 3 instances
- `src/swarm/nimcp_swarm_flocking.c` - throughout
- `src/snn/nimcp_snn_encoding.c` - 7 instances

### Batch 2: One-Liner Safety Fixes (~25 fixes)
Small, mechanical changes that prevent crashes/corruption.

- **Duplicate LOG_MODULE**: Remove duplicates in nimcp_brain.c, nimcp_neuralnet.c, nimcp_brain_lifecycle.c, nimcp_executive.c
- **Division by zero guards**: adaptive_fep_bridge.c:185, hodgkin_huxley.c:100-130, neurovascular.c:114, astrocytes.c:1472
- **Unsigned underflow guards**: neurogenesis.c:192, executive_bridge.c:626, serialization.c:86
- **Stats overwrite fix**: async_fep_bridge.c:418, memory_fep_bridge.c:465 (avg_free_energy = avg_surprise)
- **ftell error checks**: model_loader.c:815,965
- **arch_size fix**: model_loader.c:1438 (36→38)
- **Boolean return fix**: cognitive_api.c:379,395 (-1→false)
- **velocity OOB**: dragonfly_tracking.c:158 (velocity[3]→velocity[0])
- **Subscription count**: security_bio_async_bridge.c:830-833
- **Layer coordinator**: layer_coordinator.c:155 (only increment on OK)
- **GPU stubs**: gpu_stubs.c div-by-zero:1209, log NaN:1549, sqrt NaN:1573, NULL host:871
- **localtime_r**: security_logging_bridge.c:1977
- **compare_priorities overflow**: exception_handlers.c:86
- **Worker precision guard**: distributed_training_fep_bridge.c:467-468

### Batch 3: Memory Leaks / Resource Cleanup (~15 fixes)
- **predictive_default_config**: predictive.c:176-196 - use static array instead of malloc
- **predictive_immune leak**: predictive_immune.c:201 - free before overwrite
- **brain longterm_memory**: brain_lifecycle.c:740-1026 - add cleanup in destroy
- **portia_learning mutex**: portia_learning.c:255-261
- **synapse_plasticity mutex**: synapse_plasticity_bridge.c:203-215
- **pattern_db_immune mappings**: pattern_db_immune_bridge.c:239
- **security_memory_fep base**: security_memory_fep_bridge.c:227-231
- **distributed_training_fep precisions**: distributed_training_fep_bridge.c:323-330
- **5 adapter partial alloc leaks**: calcium, metabolic, neurotransmitter, ephaptic, hh adapters
- **hippocampus partial alloc**: language_hippocampus_bridge.c:440-448
- **log_context leaks**: language_motor_bridge.c:379, language_cingulate_bridge.c:405
- **used_memory tracking**: language_gpu_bridge.c:349-366

### Batch 4: Buffer Overflows / NULL Derefs / Use-After-Free (~15 fixes)
- **escape_string 4x→6x**: security_language_bridge.c:260
- **thermodynamics bounds**: thermodynamics.c:84-86 (module_id < 256)
- **language orchestrator off-by-one**: language_orchestrator.c:647-656
- **language orchestrator shallow copies**: language_orchestrator.c:714-717, 732-735 - deep copy
- **speech_jepa stack overflow**: speech_jepa_bridge.c:414,531,672,720 - dynamic alloc or clamp
- **omni_sensory NULL mutex**: omni_sensory_bridge.c:701,710,722,731 - use base.mutex
- **omni_sensory dim overflow**: omni_sensory_bridge.c:110,115 - compare before set
- **rcog NULL score**: security_rcog_bridge.c:1387 - guard dereference
- **game_theory FEP NULL**: security_game_theory_fep_bridge.c:349
- **async FEP NULL**: security_async_fep_bridge.c:376
- **collective FEP NULL**: security_collective_fep_bridge.c:321
- **time_dilation UAF**: time_dilation.c:991-998 - copy before free
- **dataio UAF**: dataio.c:2130-2133 - check before free
- **hyperthymesia bounds**: hyperthymesia.c:331,343 - month/day bounds check

### Batch 5: Deadlocks (~5 fixes)
Create `_unlocked()` helper variants.
- **adversarial_training**: adversarial_training.c:632,982
- **swarm_immune**: swarm_immune.c:1381
- **game_theory_fep**: security_game_theory_fep_bridge.c:424
- **exception_metrics**: exception_metrics.c:1052
- **signal handlers**: page_cow.c:282, signal_handler.c:632 - remove THROW from signal context

### Batch 6: Integer Overflow Checks (~12 fixes)
Add overflow guards before multiplications.
- nimcp_memory_guards.c:275 (nmemb * size)
- nimcp_nlp.c:140 (vocab_size * embedding_dim)
- nimcp_graph_metrics.c:279,624 (n * n)
- nimcp_gpu_health.c:968,1031
- nimcp_quantum_statistics.c:479-480
- nimcp_multivariate.c:509,650
- nimcp_ml_statistics.c:711
- nimcp_integration.c:179
- nimcp_tensor_kernels.cu:126-131
- nimcp_int8_inference.cu:922-926
- nimcp_brain.c:917-918

## Out of Scope (Architectural / High Risk)
These require deeper redesign and are deferred:
- Data races with static globals (swarm_brain, swarm_memory, swarm_consciousness, snn_network, multimodal_nlp)
- GPU CUDA_RECOVER chain cleanup pattern (60+ P1s, needs wrapper pattern)
- Duplicate brain.c / brain_lifecycle.c consolidation
- Unaligned memory access in distributed_cow serialization
- Command injection in vcs_integration / recompiler (needs full shell escaping)
- Lock ordering in page_cow spinlocks
- Language bio_async dangling pointers in async messages
- Thread-unsafe ring_buffer for multiple producers
- NULL byte detection dead code in logging FEP (strlen issue)
- brain_pools wrong pool release
- Hash table MurmurHash3 unaligned access
- Pattern DB regex deep copy leak
- Tensor arange/linspace dtype mismatch

## Execution Plan
6 parallel agents, one per batch. Each agent verifies build after changes.
Total estimated fixes: ~120 across ~60 files.
