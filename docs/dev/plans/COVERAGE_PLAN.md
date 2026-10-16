# NIMCP Code Coverage Action Plan

**Generated:** $(date)

## Current State
- **Total source files:** 149
- **Files with tests:** 121 (81.2%)
- **Files needing tests:** 28 (18.8%)

## Priority Order (by directory)

### cognitive (1/2 tested)

❌ `cognitive/mental_health_monitor.c`

✅ `cognitive/nimcp_fractal_cognitive.c`
   - Tests: 1 files

### core/brain (2/3 tested)

✅ `core/brain/nimcp_brain.c`
   - Tests: 126 files

✅ `core/brain/nimcp_distributed_cow.c`
   - Tests: 2 files

❌ `core/brain/nimcp_pretrained.c`

### core/neuralnet (1/2 tested)

✅ `core/neuralnet/nimcp_neuralnet.c`
   - Tests: 43 files

❌ `core/neuralnet/nimcp_synapse_embeddings.c`

### core/neuron_models (3/3 tested)

✅ `core/neuron_models/nimcp_izhikevich.c`
   - Tests: 4 files

✅ `core/neuron_models/nimcp_neuron_model.c`
   - Tests: 7 files

✅ `core/neuron_models/nimcp_two_compartment.c`
   - Tests: 1 files

### api (1/1 tested)

✅ `api/nimcp.c`
   - Tests: 751 files

### bindings/nodejs (0/1 tested)

❌ `bindings/nodejs/binding.c`

### cognitive/autobiographical_memory (0/1 tested)

❌ `cognitive/autobiographical_memory/nimcp_autobiographical_memory.c`

### cognitive/bias (1/1 tested)

✅ `cognitive/bias/nimcp_bias_detection.c`
   - Tests: 1 files

### cognitive/consolidation (1/1 tested)

✅ `cognitive/consolidation/nimcp_consolidation.c`
   - Tests: 4 files

### cognitive/curiosity (1/3 tested)

✅ `cognitive/curiosity/nimcp_curiosity.c`
   - Tests: 6 files

❌ `cognitive/curiosity/nimcp_curiosity_fractal.c`

❌ `cognitive/curiosity/nimcp_curiosity_hyperbolic.c`

### cognitive/emotion_recognition (0/1 tested)

❌ `cognitive/emotion_recognition/nimcp_emotion_recognition_simple.c`

### cognitive/emotional_tagging (1/1 tested)

✅ `cognitive/emotional_tagging/nimcp_emotional_tagging.c`
   - Tests: 8 files

### cognitive/emotions (1/1 tested)

✅ `cognitive/emotions/nimcp_emotional_system.c`
   - Tests: 3 files

### cognitive/empathetic_response (0/1 tested)

❌ `cognitive/empathetic_response/nimcp_empathetic_response.c`

### cognitive/epistemic (1/1 tested)

✅ `cognitive/epistemic/nimcp_epistemic_filter.c`
   - Tests: 1 files

### cognitive/ethics (1/2 tested)

✅ `cognitive/ethics/nimcp_ethics.c`
   - Tests: 6 files

❌ `cognitive/ethics/nimcp_ethics_hyperbolic.c`

### cognitive/executive (1/1 tested)

✅ `cognitive/executive/nimcp_executive.c`
   - Tests: 3 files

### cognitive/explanations (1/1 tested)

✅ `cognitive/explanations/nimcp_explanations.c`
   - Tests: 2 files

### cognitive/global_workspace (1/1 tested)

✅ `cognitive/global_workspace/nimcp_global_workspace.c`
   - Tests: 3 files

### cognitive/grief (1/1 tested)

✅ `cognitive/grief/nimcp_grief_and_loss.c`
   - Tests: 3 files

### cognitive/introspection (1/1 tested)

✅ `cognitive/introspection/nimcp_introspection.c`
   - Tests: 9 files

### cognitive/joy (1/1 tested)

✅ `cognitive/joy/nimcp_joy_euphoria.c`
   - Tests: 3 files

### cognitive/knowledge (1/3 tested)

✅ `cognitive/knowledge/nimcp_knowledge.c`
   - Tests: 10 files

❌ `cognitive/knowledge/nimcp_knowledge_fractal.c`

❌ `cognitive/knowledge/nimcp_knowledge_hyperbolic.c`

### cognitive/logic (1/1 tested)

✅ `cognitive/logic/nimcp_symbolic_logic.c`
   - Tests: 4 files

### cognitive/memory (4/4 tested)

✅ `cognitive/memory/nimcp_engram.c`
   - Tests: 7 files

✅ `cognitive/memory/nimcp_semantic_memory.c`
   - Tests: 1 files

✅ `cognitive/memory/nimcp_systems_consolidation.c`
   - Tests: 4 files

✅ `cognitive/memory/nimcp_wm_transfer.c`
   - Tests: 2 files

### cognitive/mental_health (1/3 tested)

❌ `cognitive/mental_health/disorder_detectors.c`

❌ `cognitive/mental_health/interventions.c`

✅ `cognitive/mental_health/nimcp_mental_health.c`
   - Tests: 3 files

### cognitive/meta_learning (1/1 tested)

✅ `cognitive/meta_learning/nimcp_meta_learning.c`
   - Tests: 3 files

### cognitive/mirror_neurons (1/1 tested)

✅ `cognitive/mirror_neurons/nimcp_mirror_neurons.c`
   - Tests: 4 files

### cognitive/personality (1/1 tested)

✅ `cognitive/personality/nimcp_personality.c`
   - Tests: 1 files

### cognitive/predictive (1/1 tested)

✅ `cognitive/predictive/nimcp_predictive.c`
   - Tests: 3 files

### cognitive/remorse (1/1 tested)

✅ `cognitive/remorse/nimcp_remorse_regret.c`
   - Tests: 1 files

### cognitive/salience (1/1 tested)

✅ `cognitive/salience/nimcp_salience.c`
   - Tests: 9 files

### cognitive/self_awareness (0/1 tested)

❌ `cognitive/self_awareness/nimcp_self_awareness_extended.c`

### cognitive/self_model (0/1 tested)

❌ `cognitive/self_model/nimcp_self_model.c`

### cognitive/shadow (1/1 tested)

✅ `cognitive/shadow/nimcp_shadow_emotions.c`
   - Tests: 1 files

### cognitive/sleep_wake (1/1 tested)

✅ `cognitive/sleep_wake/nimcp_sleep_wake.c`
   - Tests: 2 files

### cognitive/social (1/1 tested)

✅ `cognitive/social/nimcp_love_loyalty_friendship.c`
   - Tests: 3 files

### cognitive/theory_of_mind (1/1 tested)

✅ `cognitive/theory_of_mind/nimcp_theory_of_mind.c`
   - Tests: 3 files

### cognitive/wellbeing (1/1 tested)

✅ `cognitive/wellbeing/nimcp_wellbeing.c`
   - Tests: 7 files

### cognitive/working_memory (1/1 tested)

✅ `cognitive/working_memory/nimcp_working_memory.c`
   - Tests: 3 files

### core/brain/processing (0/3 tested)

❌ `core/brain/processing/cognitive_processor.c`

❌ `core/brain/processing/multimodal_integrator.c`

❌ `core/brain/processing/sensory_extractor.c`

### core/brain_oscillations (1/1 tested)

✅ `core/brain_oscillations/nimcp_brain_oscillations.c`
   - Tests: 2 files

### core/brain_regions (1/1 tested)

✅ `core/brain_regions/nimcp_brain_regions.c`
   - Tests: 6 files

### core/integration (1/1 tested)

✅ `core/integration/nimcp_multimodal_integration.c`
   - Tests: 2 files

### core/neuron_types (2/2 tested)

✅ `core/neuron_types/nimcp_neural_logic.c`
   - Tests: 3 files

✅ `core/neuron_types/nimcp_neuron_types.c`
   - Tests: 5 files

### core/synapse_compute (1/1 tested)

✅ `core/synapse_compute/nimcp_synapse_compute.c`
   - Tests: 3 files

### core/synapse_types (1/1 tested)

✅ `core/synapse_types/nimcp_synapse_types.c`
   - Tests: 1 files

### core/topology (2/2 tested)

✅ `core/topology/nimcp_fractal_topology.c`
   - Tests: 4 files

✅ `core/topology/nimcp_network_builder.c`
   - Tests: 1 files

### glial/astrocyte_types (1/1 tested)

✅ `glial/astrocyte_types/nimcp_astrocyte_types.c`
   - Tests: 2 files

### glial/astrocytes (1/2 tested)

❌ `glial/astrocytes/nimcp_astrocyte_calcium.c`

✅ `glial/astrocytes/nimcp_astrocytes.c`
   - Tests: 6 files

### glial/integration (1/1 tested)

✅ `glial/integration/nimcp_glial_integration.c`
   - Tests: 4 files

### glial/microglia (1/1 tested)

✅ `glial/microglia/nimcp_microglia.c`
   - Tests: 5 files

### glial/oligodendrocytes (1/1 tested)

✅ `glial/oligodendrocytes/nimcp_oligodendrocytes.c`
   - Tests: 5 files

### gpu (1/1 tested)

✅ `gpu/nimcp_multigpu.c`
   - Tests: 1 files

### gpu/execution (1/1 tested)

✅ `gpu/execution/nimcp_execution_mode.c`
   - Tests: 3 files

### gpu/neuron (1/1 tested)

✅ `gpu/neuron/nimcp_gpu_neuron.c`
   - Tests: 3 files

### gpu/spike_event (1/1 tested)

✅ `gpu/spike_event/nimcp_spike_event.c`
   - Tests: 3 files

### information (2/2 tested)

✅ `information/nimcp_cross_modal.c`
   - Tests: 3 files

✅ `information/nimcp_shannon.c`
   - Tests: 6 files

### io/dataio (1/1 tested)

✅ `io/dataio/nimcp_dataio.c`
   - Tests: 3 files

### io/serialization (3/3 tested)

✅ `io/serialization/nimcp_encryption.c`
   - Tests: 1 files

✅ `io/serialization/nimcp_network_serialization.c`
   - Tests: 1 files

✅ `io/serialization/nimcp_serialization.c`
   - Tests: 5 files

### io/stream (1/1 tested)

✅ `io/stream/nimcp_stream.c`
   - Tests: 4 files

### lib (0/1 tested)

❌ `lib/nimcp_distributed_cognition_impl.c`

### lib/cognitive (1/1 tested)

✅ `lib/cognitive/nimcp_hierarchical.c`
   - Tests: 1 files

### lib/perception (3/3 tested)

✅ `lib/perception/nimcp_audio_cortex.c`
   - Tests: 1 files

✅ `lib/perception/nimcp_speech_cortex.c`
   - Tests: 1 files

✅ `lib/perception/nimcp_visual_cortex.c`
   - Tests: 5 files

### networking/distributed (1/1 tested)

✅ `networking/distributed/nimcp_distributed_cognition.c`
   - Tests: 6 files

### networking/events (1/1 tested)

✅ `networking/events/nimcp_events.c`
   - Tests: 4 files

### networking/p2p (1/1 tested)

✅ `networking/p2p/nimcp_p2pnode.c`
   - Tests: 6 files

### networking/protocol (1/1 tested)

✅ `networking/protocol/nimcp_protocol.c`
   - Tests: 6 files

### networking/replication (1/1 tested)

✅ `networking/replication/nimcp_replication.c`
   - Tests: 4 files

### nlp (2/3 tested)

❌ `nlp/nimcp_multimodal_nlp_bridge.c`

✅ `nlp/nimcp_nlp.c`
   - Tests: 1 files

✅ `nlp/nimcp_spike_nlp.c`
   - Tests: 1 files

### optimization/quantum_annealing (1/1 tested)

✅ `optimization/quantum_annealing/nimcp_quantum_annealing.c`
   - Tests: 3 files

### plasticity/adaptive (1/1 tested)

✅ `plasticity/adaptive/nimcp_adaptive.c`
   - Tests: 9 files

### plasticity/attention (1/1 tested)

✅ `plasticity/attention/nimcp_attention.c`
   - Tests: 6 files

### plasticity/bcm (1/1 tested)

✅ `plasticity/bcm/nimcp_bcm.c`
   - Tests: 7 files

### plasticity/eligibility (1/1 tested)

✅ `plasticity/eligibility/nimcp_eligibility_trace.c`
   - Tests: 3 files

### plasticity/neuromodulators (6/7 tested)

✅ `plasticity/neuromodulators/nimcp_metabolic_pathways.c`
   - Tests: 3 files

❌ `plasticity/neuromodulators/nimcp_neuromod_pink_noise.c`

✅ `plasticity/neuromodulators/nimcp_neuromodulators.c`
   - Tests: 16 files

✅ `plasticity/neuromodulators/nimcp_phasic_tonic.c`
   - Tests: 5 files

✅ `plasticity/neuromodulators/nimcp_receptor_subtypes.c`
   - Tests: 1 files

✅ `plasticity/neuromodulators/nimcp_spatial_neuromod.c`
   - Tests: 12 files

✅ `plasticity/neuromodulators/nimcp_vesicle_packaging.c`
   - Tests: 3 files

### plasticity/noise (1/1 tested)

✅ `plasticity/noise/nimcp_pink_noise.c`
   - Tests: 4 files

### plasticity/stdp (1/1 tested)

✅ `plasticity/stdp/nimcp_stdp.c`
   - Tests: 2 files

### plasticity/stp (1/1 tested)

✅ `plasticity/stp/nimcp_stp.c`
   - Tests: 3 files

### security (1/1 tested)

✅ `security/nimcp_security.c`
   - Tests: 4 files

### utils/cache (1/1 tested)

✅ `utils/cache/nimcp_cache.c`
   - Tests: 5 files

### utils/config (2/2 tested)

✅ `utils/config/nimcp_config.c`
   - Tests: 4 files

✅ `utils/config/nimcp_dynamic_config.c`
   - Tests: 1 files

### utils/containers (6/6 tested)

✅ `utils/containers/nimcp_btree.c`
   - Tests: 4 files

✅ `utils/containers/nimcp_graph.c`
   - Tests: 4 files

✅ `utils/containers/nimcp_hash_table.c`
   - Tests: 5 files

✅ `utils/containers/nimcp_min_heap.c`
   - Tests: 2 files

✅ `utils/containers/nimcp_queue.c`
   - Tests: 10 files

✅ `utils/containers/nimcp_vector.c`
   - Tests: 5 files

### utils/error (0/1 tested)

❌ `utils/error/nimcp_error_codes.c`

### utils/geometry (1/1 tested)

✅ `utils/geometry/nimcp_hyperbolic.c`
   - Tests: 1 files

### utils/json (1/1 tested)

✅ `utils/json/nimcp_json.c`
   - Tests: 5 files

### utils/logging (1/1 tested)

✅ `utils/logging/nimcp_logging.c`
   - Tests: 4 files

### utils/memory (1/2 tested)

✅ `utils/memory/nimcp_memory.c`
   - Tests: 85 files

❌ `utils/memory/nimcp_memory_guards.c`

### utils/metrics (0/1 tested)

❌ `utils/metrics/nimcp_metrics.c`

### utils/numerical (1/1 tested)

✅ `utils/numerical/nimcp_integration.c`
   - Tests: 2 files

### utils/platform (7/7 tested)

✅ `utils/platform/nimcp_platform.c`
   - Tests: 25 files

✅ `utils/platform/nimcp_platform_cond.c`
   - Tests: 3 files

✅ `utils/platform/nimcp_platform_mutex.c`
   - Tests: 6 files

✅ `utils/platform/nimcp_platform_once.c`
   - Tests: 1 files

✅ `utils/platform/nimcp_platform_rwlock.c`
   - Tests: 3 files

✅ `utils/platform/nimcp_platform_thread.c`
   - Tests: 3 files

✅ `utils/platform/nimcp_platform_time.c`
   - Tests: 9 files

### utils/quantum (2/2 tested)

✅ `utils/quantum/nimcp_quantum_shannon.c`
   - Tests: 11 files

✅ `utils/quantum/nimcp_quantum_walk.c`
   - Tests: 3 files

### utils/queue_manager (1/1 tested)

✅ `utils/queue_manager/nimcp_queue_manager.c`
   - Tests: 5 files

### utils/signal (0/1 tested)

❌ `utils/signal/nimcp_signal_handler.c`

### utils/spectral (1/1 tested)

✅ `utils/spectral/nimcp_fft.c`
   - Tests: 4 files

### utils/tensor_networks (1/1 tested)

✅ `utils/tensor_networks/nimcp_mps.c`
   - Tests: 3 files

### utils/thread (2/3 tested)

❌ `utils/thread/nimcp_deadlock_detector.c`

✅ `utils/thread/nimcp_thread.c`
   - Tests: 11 files

✅ `utils/thread/nimcp_thread_pool.c`
   - Tests: 5 files

### utils/time (1/1 tested)

✅ `utils/time/nimcp_time.c`
   - Tests: 36 files

### utils/validation (1/1 tested)

✅ `utils/validation/nimcp_validate.c`
   - Tests: 4 files

