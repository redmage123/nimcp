# Find libsodium for encryption support
find_package(PkgConfig QUIET)
if(PkgConfig_FOUND)
    pkg_check_modules(SODIUM libsodium)
endif()
if(SODIUM_FOUND)
    message(STATUS "libsodium found - enabling encryption support")
else()
    message(STATUS "libsodium not found - encryption will return errors")
endif()

# Find libcurl for Claude API healer support
find_package(CURL QUIET)
if(CURL_FOUND)
    message(STATUS "libcurl found - enabling Claude API healer support")
else()
    message(STATUS "libcurl not found - Claude API healer will be disabled")
endif()

# Core library sources - New hierarchical structure
set(NIMCP_CORE_SOURCES
    # Public API facade (unified interface for all bindings) - REFACTORED (2025-12-08)
    # SRP Refactoring: 3024 lines split into focused API modules
    ${CMAKE_CURRENT_SOURCE_DIR}/../api/nimcp.c                    # Core API (full implementation)
    # Note: API functions are in nimcp.c via #include'd nimcp_part_*.c files

    # Core brain components
    ${CMAKE_CURRENT_SOURCE_DIR}/../core/brain/nimcp_brain.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../core/brain/nimcp_brain_resize.c  # Phase 2.8: Dynamic Brain Resizing
    ${CMAKE_CURRENT_SOURCE_DIR}/../core/brain/nimcp_distributed_cow.c
    # ${CMAKE_CURRENT_SOURCE_DIR}/../core/brain/nimcp_pretrained.c  # TODO: Needs libcjson-dev

    # Brain modularization (Phase 12.8: Refactored from monolithic nimcp_brain.c - 6 modules)
    ${CMAKE_CURRENT_SOURCE_DIR}/../core/brain/nimcp_brain_core.c         # Allocation, init, destroy (~2200 lines)
    ${CMAKE_CURRENT_SOURCE_DIR}/../core/brain/nimcp_brain_processing.c   # Forward pass, decisions (~1400 lines)
    ${CMAKE_CURRENT_SOURCE_DIR}/../core/brain/nimcp_brain_memory.c       # Working memory persistence (~300 lines)
    ${CMAKE_CURRENT_SOURCE_DIR}/../core/brain/nimcp_brain_state.c        # State accessors, COW (~1200 lines)
    ${CMAKE_CURRENT_SOURCE_DIR}/../core/brain/nimcp_brain_io.c           # Metadata, JSON I/O (~900 lines)
    ${CMAKE_CURRENT_SOURCE_DIR}/../core/brain/nimcp_brain_multimodal.c   # Multimodal processing (~1500 lines)
    ${CMAKE_CURRENT_SOURCE_DIR}/../core/brain/nimcp_brain_cycle_coordinator.c  # Brain cycle coordinator (unified cycle observability)

    # Brain modular architecture (Phase 11: SRP Modularization) - 7 extracted modules
    # ENABLED: Type conflicts resolved, modules ready for integration
    # Modules extracted and ready in src/core/brain/{factory,learning,inference,persistence,distributed,strategy,biological}/
    # See BRAIN_MODULARIZATION_PLAN.md for integration roadmap
    # Unified probe system
    ${CMAKE_CURRENT_SOURCE_DIR}/../core/probes/nimcp_brain_probes.c                                # Registry, create/destroy, sampler thread, query
    ${CMAKE_CURRENT_SOURCE_DIR}/../core/probes/nimcp_probe_module_map.c                            # bio_module_id → void* resolution
    ${CMAKE_CURRENT_SOURCE_DIR}/../core/probes/nimcp_probe_builtins.c                              # Built-in samplers (network, cognitive, dashboard, inference)
    # Brain factory (Phase 11: SRP Split - Refactored into 7 modules)
    ${CMAKE_CURRENT_SOURCE_DIR}/../core/brain/factory/nimcp_brain_factory.c                        # Main factory orchestration (685 lines)
    ${CMAKE_CURRENT_SOURCE_DIR}/../core/brain/factory/nimcp_brain_parallel_init.c                  # Parallel wave-based subsystem init
    ${CMAKE_CURRENT_SOURCE_DIR}/../core/brain/factory/nimcp_brain_innate.c                        # Innate circuit hardwiring
    # Brain initialization - REFACTORED into 5 modular files (2025-12-08)
    # Original nimcp_brain_init.c (4107 lines) has been split into:
    ${CMAKE_CURRENT_SOURCE_DIR}/../core/brain/factory/init/nimcp_brain_init_config.c              # Configuration builders (280 lines)
    ${CMAKE_CURRENT_SOURCE_DIR}/../core/brain/factory/init/nimcp_brain_config_profiles.c          # Configuration profiles and builder pattern
    ${CMAKE_CURRENT_SOURCE_DIR}/../core/brain/factory/init/nimcp_brain_init_validation.c          # BBB global system (112 lines)
    ${CMAKE_CURRENT_SOURCE_DIR}/../core/brain/factory/init/nimcp_brain_init_core.c                # Core initialization (174 lines)
    # Brain subsystems initialization - REFACTORED into 7 SRP modules (2025-12-08)
    # Original nimcp_brain_init_subsystems.c (3124 lines) has been split into:
    ${CMAKE_CURRENT_SOURCE_DIR}/../core/brain/factory/init/nimcp_brain_init_subsystems.c          # Subsystem coordinator (128 lines)
    ${CMAKE_CURRENT_SOURCE_DIR}/../core/brain/factory/init/nimcp_brain_init_perception.c          # Perception subsystems (459 lines)
    ${CMAKE_CURRENT_SOURCE_DIR}/../core/brain/factory/init/nimcp_brain_init_neuromod.c            # Neuromodulation subsystems (474 lines)
    ${CMAKE_CURRENT_SOURCE_DIR}/../core/brain/factory/init/nimcp_brain_init_cognitive.c           # Cognitive subsystems (731 lines)
    ${CMAKE_CURRENT_SOURCE_DIR}/../core/brain/factory/init/nimcp_brain_init_memory.c              # Memory subsystems (418 lines)
    ${CMAKE_CURRENT_SOURCE_DIR}/../core/brain/factory/init/nimcp_brain_init_pr_memory.c          # Prime Resonant memory subsystem
    ${CMAKE_CURRENT_SOURCE_DIR}/../core/brain/factory/init/nimcp_brain_init_world_model.c        # World Model subsystem (omni + multimodal)
    ${CMAKE_CURRENT_SOURCE_DIR}/../core/brain/factory/init/nimcp_brain_init_lnn.c               # LNN liquid neural network temporal processor
    ${CMAKE_CURRENT_SOURCE_DIR}/../core/brain/factory/init/nimcp_brain_init_language_pops.c     # Language + sensorymotor SNN pops (Stage 1: cold-init + hierarchy taps)
    ${CMAKE_CURRENT_SOURCE_DIR}/../core/brain/internal/nimcp_brain_pr_memory.c                   # Brain-PR memory integration
    ${CMAKE_CURRENT_SOURCE_DIR}/../core/brain/internal/nimcp_pr_memory_quantum_bridge.c          # PR memory-quantum integration (search, consolidation, entanglement)
    ${CMAKE_CURRENT_SOURCE_DIR}/../core/brain/factory/init/nimcp_brain_init_plasticity.c          # Plasticity subsystems (553 lines)
    ${CMAKE_CURRENT_SOURCE_DIR}/../core/brain/factory/init/nimcp_brain_init_monitoring.c          # Monitoring subsystems (476 lines)
    ${CMAKE_CURRENT_SOURCE_DIR}/../core/brain/factory/init/nimcp_brain_init_structural.c          # Structural subsystems (624 lines)
    ${CMAKE_CURRENT_SOURCE_DIR}/../core/brain/factory/init/nimcp_brain_init_security.c            # Security subsystem (318 lines)
    ${CMAKE_CURRENT_SOURCE_DIR}/../core/brain/factory/init/nimcp_brain_init_safety_verify.c       # LGSS + toxicity init + cycle registration
    ${CMAKE_CURRENT_SOURCE_DIR}/../core/brain/factory/init/nimcp_brain_init_fep_orchestrator.c   # FEP Orchestrator subsystem
    # Coordinator/Orchestrator init functions (brain lifecycle integration)
    ${CMAKE_CURRENT_SOURCE_DIR}/../core/brain/factory/init/nimcp_brain_init_bio_async_orchestrator.c  # Bio-Async Orchestrator
    ${CMAKE_CURRENT_SOURCE_DIR}/../core/brain/factory/init/nimcp_brain_init_plasticity_coordinator.c  # Plasticity Coordinator
    ${CMAKE_CURRENT_SOURCE_DIR}/../core/brain/factory/init/nimcp_brain_init_immune_bridge_coordinator.c  # Immune Bridge Coordinator
    ${CMAKE_CURRENT_SOURCE_DIR}/../core/brain/factory/init/nimcp_brain_init_cognitive_meta_controller.c  # Cognitive Meta-Controller
    ${CMAKE_CURRENT_SOURCE_DIR}/../core/brain/factory/init/nimcp_brain_init_security_perception_bridge.c  # Security-Perception Bridge
    ${CMAKE_CURRENT_SOURCE_DIR}/../core/brain/factory/init/nimcp_brain_init_swarm_module_registry.c  # Swarm Module Registry
    ${CMAKE_CURRENT_SOURCE_DIR}/../core/brain/factory/init/nimcp_brain_init_core_directives.c     # Core Directives subsystem
    ${CMAKE_CURRENT_SOURCE_DIR}/../core/brain/factory/init/nimcp_brain_init_fuzzy.c               # Fuzzy logic subsystem
    ${CMAKE_CURRENT_SOURCE_DIR}/../core/brain/factory/init/nimcp_brain_init_creative.c            # Creative/artistic system
    ${CMAKE_CURRENT_SOURCE_DIR}/../core/brain/factory/init/nimcp_brain_init_parietal.c            # Parietal lobe subsystem
    ${CMAKE_CURRENT_SOURCE_DIR}/../core/brain/factory/init/nimcp_brain_init_dragonfly.c           # Dragonfly target tracking subsystem
    ${CMAKE_CURRENT_SOURCE_DIR}/../core/brain/factory/init/nimcp_brain_init_octopus.c             # Octopus distributed peripheral cognition
    ${CMAKE_CURRENT_SOURCE_DIR}/../core/brain/factory/init/nimcp_brain_init_amygdala.c           # Amygdala fear/salience (Phase 3c)
    ${CMAKE_CURRENT_SOURCE_DIR}/../cognitive/octopus/nimcp_octopus.c                              # Octopus cognitive module core
    ${CMAKE_CURRENT_SOURCE_DIR}/../cognitive/octopus/nimcp_octopus_bridges.c                      # Octopus → ethics/swarm/world/fep/bio/immune bridges
    ${CMAKE_CURRENT_SOURCE_DIR}/../core/brain/factory/init/nimcp_brain_init_edge.c               # Edge/robot integration (sensor hub, safety, swarm bridges)
    ${CMAKE_CURRENT_SOURCE_DIR}/../core/brain/factory/init/nimcp_brain_init_kg_reader.c           # KG reader for self-awareness
    ${CMAKE_CURRENT_SOURCE_DIR}/../core/brain/factory/init/nimcp_brain_init_internal_kg.c        # Internal runtime KG with CRUD
    ${CMAKE_CURRENT_SOURCE_DIR}/../core/brain/factory/init/nimcp_brain_init_mental_health_guardian.c  # Mental Health Guardian (independent monitor)
    ${CMAKE_CURRENT_SOURCE_DIR}/../core/brain/factory/init/nimcp_brain_init_fault_tolerance.c    # Fault tolerance with parietal integration
    ${CMAKE_CURRENT_SOURCE_DIR}/../core/brain/factory/init/nimcp_brain_init_health_agent.c      # Health agent autonomous monitoring
    ${CMAKE_CURRENT_SOURCE_DIR}/../core/brain/factory/init/nimcp_brain_init_cycle_coordinator.c # Brain cycle coordinator init & integration
    ${CMAKE_CURRENT_SOURCE_DIR}/../core/brain/factory/init/nimcp_brain_init_state_manager.c    # State manager for checkpointing (Phase 8)
    ${CMAKE_CURRENT_SOURCE_DIR}/../core/brain/factory/init/nimcp_brain_init_basal_ganglia.c     # Enhanced basal ganglia action selection
    ${CMAKE_CURRENT_SOURCE_DIR}/../core/brain/factory/init/nimcp_brain_init_broca.c            # Broca's region language production
    ${CMAKE_CURRENT_SOURCE_DIR}/../core/brain/factory/init/nimcp_brain_init_wernicke.c         # Wernicke's region language comprehension
    ${CMAKE_CURRENT_SOURCE_DIR}/../core/brain/factory/init/nimcp_brain_init_language.c         # Unified language layer orchestration
    ${CMAKE_CURRENT_SOURCE_DIR}/../core/brain/factory/init/nimcp_brain_init_collective_cognition.c # Collective cognition subsystem
    ${CMAKE_CURRENT_SOURCE_DIR}/../core/brain/factory/init/nimcp_brain_init_cognitive_engines.c   # Cognitive engines (rcog, inner dialogue, reasoning, imagination)
    ${CMAKE_CURRENT_SOURCE_DIR}/../core/brain/factory/init/nimcp_brain_init_gpu.c              # GPU context auto-initialization
    ${CMAKE_CURRENT_SOURCE_DIR}/../core/brain/factory/init/nimcp_brain_init_gpu_inference.c   # GPU inference weight cache init
    ${CMAKE_CURRENT_SOURCE_DIR}/../core/brain/factory/init/nimcp_brain_init_substrate_gpu.c   # GPU substrate subsystem initialization
    ${CMAKE_CURRENT_SOURCE_DIR}/../core/brain/factory/init/nimcp_brain_init_substrate_thalamic.c  # CPU substrate + thalamic router wire-up for SNN/LNN/CNN
    ${CMAKE_CURRENT_SOURCE_DIR}/../core/brain/factory/init/nimcp_brain_init_biology.c             # Biology cluster (epigenetics, neurogenesis, NVC) — periodic via cycle coord
    ${CMAKE_CURRENT_SOURCE_DIR}/../core/brain/factory/init/nimcp_brain_init_cochlea.c             # Wave 8A: Cochlea + 15 consumer bridges — single BRAIN_CYCLE_COCHLEA_BRIDGES tick
    ${CMAKE_CURRENT_SOURCE_DIR}/../core/brain/factory/init/nimcp_brain_init_physics_bridges.c     # Wave 8C: 3 physics upstream + 4 HALF-STATUE bridges (ephaptic/hh/thermo)
    ${CMAKE_CURRENT_SOURCE_DIR}/../core/brain/factory/init/nimcp_brain_init_chemistry.c           # Wave 6: Chemistry cluster (pH, pumps, buffers, NO) — single BRAIN_CYCLE_CHEMISTRY tick
    ${CMAKE_CURRENT_SOURCE_DIR}/../core/brain/factory/init/nimcp_brain_init_predictive_immune.c   # Predictive-immune coupling — periodic via cycle coord
    ${CMAKE_CURRENT_SOURCE_DIR}/../core/brain/factory/init/nimcp_brain_init_region_cycles.c       # 2026-04-24: register 6 tick drivers as driven coord cycles
    ${CMAKE_CURRENT_SOURCE_DIR}/../core/brain/factory/init/nimcp_brain_init_cognitive_training.c  # Cognitive training subsystems (JEPA, pred hierarchy, self-heal)
    ${CMAKE_CURRENT_SOURCE_DIR}/../core/brain/nimcp_cognitive_transcript.c  # Cognitive transcript for response composition

    # White Matter Tracts - Long-range myelinated connectivity (8 major tracts)
    ${CMAKE_CURRENT_SOURCE_DIR}/../core/brain/white_matter/nimcp_white_matter_tracts.c              # White matter core implementation
    ${CMAKE_CURRENT_SOURCE_DIR}/../core/brain/white_matter/nimcp_white_matter_bridges.c             # White matter bridge integrations
    ${CMAKE_CURRENT_SOURCE_DIR}/../core/brain/factory/init/nimcp_brain_init_white_matter.c            # White matter factory init

    # Inferior Colliculus - Auditory midbrain processing (tonotopic, binaural)
    ${CMAKE_CURRENT_SOURCE_DIR}/../core/brain/subcortical/nimcp_inferior_colliculus.c               # Inferior colliculus core implementation
    ${CMAKE_CURRENT_SOURCE_DIR}/../core/brain/subcortical/nimcp_inferior_colliculus_bridges.c       # Inferior colliculus bridge integrations
    ${CMAKE_CURRENT_SOURCE_DIR}/../core/brain/factory/init/nimcp_brain_init_inferior_colliculus.c   # Inferior colliculus factory init

    # Spinal Cord - Motor output, CPGs, reflex arcs, proprioception
    ${CMAKE_CURRENT_SOURCE_DIR}/../core/spinal/nimcp_spinal_cord.c                                 # Spinal cord core implementation
    ${CMAKE_CURRENT_SOURCE_DIR}/../core/spinal/nimcp_spinal_cord_bridges.c                         # Spinal cord bridge integrations
    ${CMAKE_CURRENT_SOURCE_DIR}/../core/brain/factory/init/nimcp_brain_init_spinal_cord.c          # Spinal cord factory init

    # Cortical Interneurons - GABAergic inhibitory microcircuit control (5 types)
    ${CMAKE_CURRENT_SOURCE_DIR}/../core/cortical_columns/nimcp_cortical_interneurons.c             # Cortical interneurons core implementation
    ${CMAKE_CURRENT_SOURCE_DIR}/../core/cortical_columns/nimcp_cortical_interneurons_bridges.c     # Cortical interneurons bridge integrations
    ${CMAKE_CURRENT_SOURCE_DIR}/../core/brain/factory/init/nimcp_brain_init_cortical_interneurons.c # Cortical interneurons factory init

    # Neuropeptide System - Slow neuromodulation (8 peptides: oxytocin, orexin, etc.)
    ${CMAKE_CURRENT_SOURCE_DIR}/../core/brain/regions/neuropeptide/nimcp_neuropeptide.c             # Neuropeptide core implementation
    ${CMAKE_CURRENT_SOURCE_DIR}/../core/brain/regions/neuropeptide/nimcp_neuropeptide_bridges.c     # Neuropeptide bridge integrations
    ${CMAKE_CURRENT_SOURCE_DIR}/../core/brain/factory/init/nimcp_brain_init_neuropeptide.c          # Neuropeptide factory init

    # Endocannabinoid System - Retrograde synaptic modulation (CB1/CB2, 2-AG, AEA)
    ${CMAKE_CURRENT_SOURCE_DIR}/../core/brain/regions/endocannabinoid/nimcp_endocannabinoid.c       # Endocannabinoid core implementation
    ${CMAKE_CURRENT_SOURCE_DIR}/../core/brain/regions/endocannabinoid/nimcp_endocannabinoid_bridges.c # Endocannabinoid bridge integrations
    ${CMAKE_CURRENT_SOURCE_DIR}/../core/brain/factory/init/nimcp_brain_init_endocannabinoid.c       # Endocannabinoid factory init

    # Glymphatic System - Brain waste clearance (sleep-modulated CSF flow)
    ${CMAKE_CURRENT_SOURCE_DIR}/../core/brain/regions/glymphatic/nimcp_glymphatic.c                 # Glymphatic core implementation
    ${CMAKE_CURRENT_SOURCE_DIR}/../core/brain/regions/glymphatic/nimcp_glymphatic_bridges.c         # Glymphatic bridge integrations
    ${CMAKE_CURRENT_SOURCE_DIR}/../core/brain/factory/init/nimcp_brain_init_glymphatic.c            # Glymphatic factory init

    ${CMAKE_CURRENT_SOURCE_DIR}/../core/brain/factory/validation/nimcp_brain_validation.c         # Validation and caching (219 lines)
    ${CMAKE_CURRENT_SOURCE_DIR}/../core/brain/learning/nimcp_brain_experience.c   # Unified experience API (inference + learning)
    ${CMAKE_CURRENT_SOURCE_DIR}/../core/brain/learning/nimcp_brain_learning.c     # Training, backpropagation, batch processing (947 lines)
    ${CMAKE_CURRENT_SOURCE_DIR}/../core/brain/learning/nimcp_rule_learning.c      # Rule learning from examples (inductive learning)
    ${CMAKE_CURRENT_SOURCE_DIR}/../core/brain/learning/nimcp_association_learning.c  # Association learning (A→B implications)
    ${CMAKE_CURRENT_SOURCE_DIR}/../core/brain/learning/nimcp_circuit_compilation.c   # Neural circuit compilation (symbolic→neural)
    ${CMAKE_CURRENT_SOURCE_DIR}/../core/brain/inference/nimcp_brain_inference.c   # Prediction, forward propagation, results (502 lines)
    ${CMAKE_CURRENT_SOURCE_DIR}/../core/brain/nimcp_brain_parallel_stages.c     # Parallel cognitive stage dispatch
    ${CMAKE_CURRENT_SOURCE_DIR}/../core/brain/nimcp_cognitive_dispatch.c       # Actor-based parallel cognitive training
    ${CMAKE_CURRENT_SOURCE_DIR}/../core/brain/inference/nimcp_brain_quantum_reasoning.c # Quantum SAT solving for logical inference
    ${CMAKE_CURRENT_SOURCE_DIR}/../core/brain/persistence/nimcp_brain_persistence.c # Save/load, snapshots, checkpoints (1,187 lines)
    ${CMAKE_CURRENT_SOURCE_DIR}/../core/brain/persistence/nimcp_checkpoint_unified.c # Unified single-file checkpoint format
    ${CMAKE_CURRENT_SOURCE_DIR}/../core/brain/persistence/nimcp_brain_kg_snapshot.c # KG-based snapshot persistence (Phase SNAPSHOT-KG)
    ${CMAKE_CURRENT_SOURCE_DIR}/../core/brain/persistence/nimcp_brain_kg_sidecar.c  # Internal KG file sidecar (.kg alongside checkpoint)
    ${CMAKE_CURRENT_SOURCE_DIR}/../core/brain/calibration/nimcp_temperature_scaling.c # Layer C: post-hoc confidence calibration (Guo et al. 2017)
    ${CMAKE_CURRENT_SOURCE_DIR}/../core/brain/distributed/nimcp_brain_distributed.c # Copy-on-write, P2P sync, consensus (561 lines)
    ${CMAKE_CURRENT_SOURCE_DIR}/../core/brain/strategy/nimcp_brain_strategy.c     # Strategy pattern, monitoring, optimization (770 lines)
    ${CMAKE_CURRENT_SOURCE_DIR}/../core/brain/nimcp_brain_bio_async.c             # Phase BIO-ASYNC: Bio-async brain integration
    ${CMAKE_CURRENT_SOURCE_DIR}/../core/brain/cognitive/nimcp_brain_cognitive.c   # Cognitive systems subsystem (2,150 lines extracted)
    ${CMAKE_CURRENT_SOURCE_DIR}/../core/brain/biological/nimcp_brain_biological.c # Glial, neuromodulators, pink noise, multimodal (~1,500 lines)
    ${CMAKE_CURRENT_SOURCE_DIR}/../core/brain/accessors/nimcp_brain_accessors.c   # Brain accessor functions (388 lines)
    ${CMAKE_CURRENT_SOURCE_DIR}/../core/brain/pretrained/nimcp_brain_pretrained.c # Pretrained model management (510 lines)
    ${CMAKE_CURRENT_SOURCE_DIR}/../core/brain/information/nimcp_brain_shannon.c   # Shannon information theory (461 lines)
    ${CMAKE_CURRENT_SOURCE_DIR}/../core/brain/analysis/nimcp_brain_topology.c     # Topology and community detection (462 lines)
    ${CMAKE_CURRENT_SOURCE_DIR}/../core/brain/nimcp_brain_kg.c                    # Internal runtime knowledge graph (self-awareness)
    ${CMAKE_CURRENT_SOURCE_DIR}/../core/brain/nimcp_brain_kg_helpers.c           # KG helper utilities for module integration
    ${CMAKE_CURRENT_SOURCE_DIR}/../core/brain/nimcp_kg_hierarchy.c               # Hierarchical view of KG (3-level brain topology)

    # KG Self-Awareness Extension Modules (Phase Jan 2026)
    ${CMAKE_CURRENT_SOURCE_DIR}/../core/brain/nimcp_kg_metadata.c              # Node/edge metadata management
    ${CMAKE_CURRENT_SOURCE_DIR}/../core/brain/nimcp_kg_search.c                # Full-text and pattern search
    ${CMAKE_CURRENT_SOURCE_DIR}/../core/brain/nimcp_kg_algorithms.c            # Graph algorithms (centrality, paths, communities)
    ${CMAKE_CURRENT_SOURCE_DIR}/../core/brain/nimcp_kg_module_wiring.c         # Module connection/wiring primitives
    ${CMAKE_CURRENT_SOURCE_DIR}/../core/brain/nimcp_kg_wiring_exception.c     # KG wiring exception integration
    # ${CMAKE_CURRENT_SOURCE_DIR}/../core/brain/nimcp_kg_assembly.c              # Brain assembly from KG - TEMPORARILY DISABLED (API mismatch)
    ${CMAKE_CURRENT_SOURCE_DIR}/../core/brain/nimcp_kg_io_dispatcher.c         # Async I/O dispatching
    # ${CMAKE_CURRENT_SOURCE_DIR}/../core/brain/nimcp_kg_persistence.c           # KG persistence - DISABLED (API mismatch, using Python-side QuestDB instead)
    # ${CMAKE_CURRENT_SOURCE_DIR}/../core/brain/nimcp_kg_events.c                # Pub/sub event - TEMPORARILY DISABLED (incomplete)
    ${CMAKE_CURRENT_SOURCE_DIR}/../core/brain/nimcp_kg_observability.c         # Prometheus metrics, tracing, health
    ${CMAKE_CURRENT_SOURCE_DIR}/../core/brain/nimcp_kg_degradation.c           # Circuit breaker and graceful degradation
    ${CMAKE_CURRENT_SOURCE_DIR}/../core/brain/nimcp_kg_disaster_recovery.c     # Replication, backup, failover
    ${CMAKE_CURRENT_SOURCE_DIR}/../core/brain/nimcp_kg_gc.c                    # Garbage collection and compaction
    ${CMAKE_CURRENT_SOURCE_DIR}/../core/brain/nimcp_kg_gc_agent.c              # GC subagent background service
    ${CMAKE_CURRENT_SOURCE_DIR}/../core/brain/nimcp_kg_temporal.c              # Bi-temporal queries and versioning
    ${CMAKE_CURRENT_SOURCE_DIR}/../core/brain/nimcp_kg_schema.c                # Schema evolution and migration
    ${CMAKE_CURRENT_SOURCE_DIR}/../core/brain/nimcp_kg_ratelimit.c             # Rate limiting and quotas
    ${CMAKE_CURRENT_SOURCE_DIR}/../core/brain/nimcp_kg_federation.c            # Cross-brain federation
    ${CMAKE_CURRENT_SOURCE_DIR}/../core/brain/nimcp_kg_analytics.c             # Graph analytics and insights
    ${CMAKE_CURRENT_SOURCE_DIR}/../core/brain/nimcp_kg_compliance.c            # Data governance and GDPR compliance
    ${CMAKE_CURRENT_SOURCE_DIR}/../core/brain/nimcp_kg_interchange.c           # Import/export formats
    ${CMAKE_CURRENT_SOURCE_DIR}/../core/brain/nimcp_kg_gpu.c                   # GPU acceleration

    # Hemispheric Brain Architecture (Phase Dec 2024: Two-hemisphere bilateral processing)
    ${CMAKE_CURRENT_SOURCE_DIR}/../core/brain/hemispheric/nimcp_lateralization.c     # Hemisphere specialization profiles
    ${CMAKE_CURRENT_SOURCE_DIR}/../core/brain/hemispheric/nimcp_corpus_callosum.c    # Inter-hemispheric bridge
    ${CMAKE_CURRENT_SOURCE_DIR}/../core/brain/hemispheric/nimcp_brain_hemisphere.c   # Single hemisphere wrapper
    ${CMAKE_CURRENT_SOURCE_DIR}/../core/brain/hemispheric/nimcp_hemispheric_brain.c  # Main bilateral brain
    ${CMAKE_CURRENT_SOURCE_DIR}/../core/brain/hemispheric/nimcp_hemispheric_immune_bridge.c  # Immune integration
    ${CMAKE_CURRENT_SOURCE_DIR}/../core/brain/hemispheric/nimcp_hemispheric_sleep_bridge.c   # Sleep integration
    ${CMAKE_CURRENT_SOURCE_DIR}/../core/brain/hemispheric/nimcp_hemispheric_fep_bridge.c     # FEP integration
    ${CMAKE_CURRENT_SOURCE_DIR}/../core/brain/hemispheric/nimcp_hemispheric_portia_bridge.c  # Portia integration
    ${CMAKE_CURRENT_SOURCE_DIR}/../core/brain/hemispheric/nimcp_contralateral_mapping.c      # Motor/sensory body mapping
    ${CMAKE_CURRENT_SOURCE_DIR}/../core/brain/hemispheric/nimcp_hemispheric_glial_bridge.c   # Glial integration
    ${CMAKE_CURRENT_SOURCE_DIR}/../core/brain/hemispheric/nimcp_hemispheric_language_bridge.c # Lateralized language wiring
    ${CMAKE_CURRENT_SOURCE_DIR}/../core/brain/hemispheric/nimcp_hemispheric_injury.c        # Injury and recovery
    ${CMAKE_CURRENT_SOURCE_DIR}/../core/brain/hemispheric/nimcp_split_brain_experiments.c  # Split-brain experiments

    # Genius Profiles Module (Phase Feb 2026: Configurable genius brain profiles)
    ${CMAKE_CURRENT_SOURCE_DIR}/../core/brain/genius/nimcp_genius_profiles.c  # Static profiles, bridge lifecycle, system integration
    ${CMAKE_CURRENT_SOURCE_DIR}/../core/brain/genius/eidetic/nimcp_eidetic_memory.c  # Eidetic memory integration

    # Brain processing modules (Phase 9.1: SRP Refactoring) - TODO: Enable when refactoring complete
    # ${CMAKE_CURRENT_SOURCE_DIR}/../core/brain/processing/sensory_extractor.c
    # ${CMAKE_CURRENT_SOURCE_DIR}/../core/brain/processing/multimodal_integrator.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../core/brain/processing/cognitive_processor.c  # Cognitive output processing

    # Neural Network - Refactored into SRP modules (Phase 12.9: Large File Refactoring)
    ${CMAKE_CURRENT_SOURCE_DIR}/../core/neuralnet/nimcp_neuralnet.c           # Core lifecycle and neuron updates
    ${CMAKE_CURRENT_SOURCE_DIR}/../core/neuralnet/nimcp_sparse_synapse.c      # Sparse synapse storage and operations
    ${CMAKE_CURRENT_SOURCE_DIR}/../core/neuralnet/nimcp_synapse_lifecycle.c   # Synapse lifecycle: pruning, growth, metadata GC
    # DISABLED: Agent-created files with duplicate functions (originals not cleaned up)
    # ${CMAKE_CURRENT_SOURCE_DIR}/../core/neuralnet/nimcp_neuralnet_activation.c  # Activation functions (Strategy pattern)
    # ${CMAKE_CURRENT_SOURCE_DIR}/../core/neuralnet/nimcp_neuralnet_learning.c   # STDP, Oja's rule, weight updates
    ${CMAKE_CURRENT_SOURCE_DIR}/../core/neuralnet/nimcp_neuralnet_homeostasis.c # Homeostatic plasticity
    ${CMAKE_CURRENT_SOURCE_DIR}/../core/neuralnet/nimcp_neuralnet_backprop.c   # Backpropagation for weight gradients
    ${CMAKE_CURRENT_SOURCE_DIR}/../core/neuron_types/nimcp_neuron_types.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../core/neuron_types/nimcp_neural_logic.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../core/logic/nimcp_neural_logic_brain_integration.c  # Phase 2.6.2: Neural Logic Brain Integration
    ${CMAKE_CURRENT_SOURCE_DIR}/../core/brain_regions/nimcp_brain_regions.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../core/brain_regions/nimcp_contextual_language.c    # Contextual language adaptation
    ${CMAKE_CURRENT_SOURCE_DIR}/../core/brain_regions/nimcp_language_production_bridge.c  # Language production bridge API (weak health agent to avoid broca conflict)
    ${CMAKE_CURRENT_SOURCE_DIR}/../core/brain_regions/nimcp_brain_regions_immune_bridge.c  # Brain regions-immune integration

    # Neural substrate - Computational infrastructure layer
    ${CMAKE_CURRENT_SOURCE_DIR}/../core/neural_substrate/nimcp_neural_substrate.c       # Metabolic/physical substrate modeling
    ${CMAKE_CURRENT_SOURCE_DIR}/../core/neural_substrate/nimcp_substrate_immune_bridge.c  # Substrate-immune integration

    # Cortical columns - Tier 0.65: Cortical Columns Architecture (Phase CC-1)
    ${CMAKE_CURRENT_SOURCE_DIR}/../core/cortical_columns/nimcp_cortical_column.c       # Core minicolumn/hypercolumn structures
    ${CMAKE_CURRENT_SOURCE_DIR}/../core/cortical_columns/nimcp_cortical_column_ternary.c  # Ternary WTA wrapper for hypercolumns (cc-activation campaign)
    ${CMAKE_CURRENT_SOURCE_DIR}/../core/cortical_columns/nimcp_cortical_layers.c       # 6-layer laminar organization
    ${CMAKE_CURRENT_SOURCE_DIR}/../core/cortical_columns/nimcp_columnar_connectivity.c # Canonical microcircuit connectivity
    ${CMAKE_CURRENT_SOURCE_DIR}/../core/cortical_columns/nimcp_topographic_maps.c      # Retinotopic, tonotopic, somatotopic maps
    ${CMAKE_CURRENT_SOURCE_DIR}/../core/cortical_columns/nimcp_orientation_columns.c   # V1 orientation selectivity (Gabor)
    ${CMAKE_CURRENT_SOURCE_DIR}/../core/cortical_columns/nimcp_cortical_surround.c     # Surround suppression & contextual modulation
    ${CMAKE_CURRENT_SOURCE_DIR}/../core/cortical_columns/nimcp_feature_hypercolumns.c  # Feature dimension coverage
    ${CMAKE_CURRENT_SOURCE_DIR}/../core/cortical_columns/nimcp_cortical_dendritic.c    # Dendritic computation (apical/basal, Ca spikes, burst coding)
    ${CMAKE_CURRENT_SOURCE_DIR}/../core/cortical_columns/nimcp_cortical_immune.c  # Cortical columns-immune integration
    ${CMAKE_CURRENT_SOURCE_DIR}/../core/cortical_columns/nimcp_cortical_predictive_coding.c  # Predictive coding hierarchy
    ${CMAKE_CURRENT_SOURCE_DIR}/../core/cortical_columns/nimcp_cortical_attention_gain.c    # Attention-modulated gain control
    ${CMAKE_CURRENT_SOURCE_DIR}/../core/cortical_columns/nimcp_cortical_temporal.c  # Temporal dynamics & sequence processing
    ${CMAKE_CURRENT_SOURCE_DIR}/../core/cortical_columns/nimcp_cortical_hierarchy.c    # Multi-area hierarchy & connectivity
    ${CMAKE_CURRENT_SOURCE_DIR}/../core/cortical_columns/nimcp_cortical_sparse_coding.c   # Sparse distributed representations
    ${CMAKE_CURRENT_SOURCE_DIR}/../core/cortical_columns/nimcp_cortical_neuromodulation.c # Neuromodulatory effects on columns
    ${CMAKE_CURRENT_SOURCE_DIR}/../core/cortical_columns/nimcp_cortical_oscillations_integration.c  # Oscillation integration
    ${CMAKE_CURRENT_SOURCE_DIR}/../core/cortical_columns/nimcp_cortical_plasticity_bridge.c  # Plasticity coordinator integration
    ${CMAKE_CURRENT_SOURCE_DIR}/../core/cortical_columns/nimcp_cortical_substrate_bridge.c  # Cortical-neural substrate integration
    ${CMAKE_CURRENT_SOURCE_DIR}/../core/cortical_columns/nimcp_dendritic_sequence.c         # HTM dendritic sequence prediction (Hawkins)
    ${CMAKE_CURRENT_SOURCE_DIR}/../core/cortical_columns/nimcp_column_reference_frame.c     # Grid cell reference frames (Thousand Brains)
    ${CMAKE_CURRENT_SOURCE_DIR}/../core/cortical_columns/nimcp_column_voting.c              # Column voting consensus (Thousand Brains)
    ${CMAKE_CURRENT_SOURCE_DIR}/../core/cortical_columns/nimcp_thousand_brains_integration.c  # Thousand Brains full-system integration hub

    # Broca's region sub-modules
    ${CMAKE_CURRENT_SOURCE_DIR}/../core/brain/regions/broca/nimcp_syntax_processor.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../core/brain/regions/broca/nimcp_phonological.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../core/brain/regions/broca/nimcp_speech_motor.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../core/brain/regions/broca/nimcp_broca_adapter.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../core/brain/regions/broca/nimcp_language_production_bridge.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../core/brain/regions/broca/nimcp_broca_immune.c  # Broca's region-immune integration
    ${CMAKE_CURRENT_SOURCE_DIR}/../core/brain/regions/broca/nimcp_broca_substrate_bridge.c  # Broca-neural substrate integration
    ${CMAKE_CURRENT_SOURCE_DIR}/../core/brain/regions/broca/nimcp_broca_thalamic_bridge.c   # Broca-thalamic routing integration
    ${CMAKE_CURRENT_SOURCE_DIR}/../core/brain/regions/broca/nimcp_broca_quantum_bridge.c    # Broca-quantum reasoning integration
    # Phase B4: Speech Enhancement modules
    ${CMAKE_CURRENT_SOURCE_DIR}/../core/brain/regions/broca/nimcp_pragmatics_processor.c     # Speech act classification, Gricean maxims
    ${CMAKE_CURRENT_SOURCE_DIR}/../core/brain/regions/broca/nimcp_discourse_manager.c        # Anaphora resolution, topic tracking
    ${CMAKE_CURRENT_SOURCE_DIR}/../core/brain/regions/broca/nimcp_emotional_prosody.c        # Emotion-to-prosody mapping
    ${CMAKE_CURRENT_SOURCE_DIR}/../core/brain/regions/broca/nimcp_incremental_processor.c    # Real-time streaming processing
    ${CMAKE_CURRENT_SOURCE_DIR}/../core/brain/regions/broca/nimcp_speech_repair.c            # Disfluency detection and repair
    ${CMAKE_CURRENT_SOURCE_DIR}/../core/brain/regions/broca/nimcp_multimodal_language.c      # Gesture/expression/gaze coordination

    # Cerebellum - Motor coordination, timing, error-based learning (Marr-Albus-Ito model)
    ${CMAKE_CURRENT_SOURCE_DIR}/../core/brain/regions/cerebellum/nimcp_cerebellum_adapter.c       # Cerebellum core adapter
    ${CMAKE_CURRENT_SOURCE_DIR}/../core/brain/regions/cerebellum/nimcp_cerebellum_jepa_bridge.c   # Phase 4s: cerebellum ↔ JEPA
    ${CMAKE_CURRENT_SOURCE_DIR}/../core/brain/regions/cerebellum/nimcp_cerebellum_quantum_bridge.c # Quantum timing optimization
    ${CMAKE_CURRENT_SOURCE_DIR}/../core/brain/factory/init/nimcp_brain_init_cerebellum.c          # Cerebellum factory init

    # Brainstem - Midbrain, pons, medulla, reticular formation (vital functions, reflexes, arousal)
    ${CMAKE_CURRENT_SOURCE_DIR}/../core/brain/regions/brainstem/nimcp_brainstem_adapter.c         # Brainstem core adapter
    ${CMAKE_CURRENT_SOURCE_DIR}/../core/brain/regions/brainstem/nimcp_brainstem_quantum_bridge.c  # Quantum-accelerated processing
    ${CMAKE_CURRENT_SOURCE_DIR}/../core/brain/regions/brainstem/nimcp_brainstem_immune_bridge.c   # Brainstem-immune integration (arousal-immune)
    ${CMAKE_CURRENT_SOURCE_DIR}/../core/brain/regions/brainstem/nimcp_vestibular.c               # Vestibular nuclei processor (Phase 3)
    ${CMAKE_CURRENT_SOURCE_DIR}/../core/brain/regions/brainstem/nimcp_vestibular_cerebellum_bridge.c # Vestibular-cerebellum bridge (Phase 3)
    ${CMAKE_CURRENT_SOURCE_DIR}/../core/brain/factory/init/nimcp_brain_init_brainstem.c           # Brainstem factory init

    # Cingulate Cortex - Conflict monitoring, error detection, cognitive control (Phase B4)
    ${CMAKE_CURRENT_SOURCE_DIR}/../core/brain/regions/cingulate/nimcp_cingulate_adapter.c         # Cingulate core adapter
    ${CMAKE_CURRENT_SOURCE_DIR}/../core/brain/regions/cingulate/nimcp_cingulate_quantum_bridge.c  # Quantum-accelerated conflict resolution
    ${CMAKE_CURRENT_SOURCE_DIR}/../core/brain/regions/cingulate/nimcp_cingulate_immune_bridge.c   # Cingulate-immune integration (error/conflict-immune)
    ${CMAKE_CURRENT_SOURCE_DIR}/../core/brain/factory/init/nimcp_brain_init_cingulate.c           # Cingulate factory init
    ${CMAKE_CURRENT_SOURCE_DIR}/../core/brain/factory/init/nimcp_brain_init_ofc.c                  # OFC factory init
    ${CMAKE_CURRENT_SOURCE_DIR}/../core/brain/factory/init/nimcp_brain_init_prefrontal.c          # Prefrontal factory init (was statue)
    ${CMAKE_CURRENT_SOURCE_DIR}/../core/brain/factory/init/nimcp_brain_init_insula.c              # Insula factory init (was statue)

    # Hypothalamus - Homeostatic regulation, circadian rhythms, stress response (Phase H1)
    ${CMAKE_CURRENT_SOURCE_DIR}/../core/brain/regions/hypothalamus/nimcp_hypothalamus_adapter.c         # Hypothalamus core adapter
    ${CMAKE_CURRENT_SOURCE_DIR}/../core/brain/regions/hypothalamus/nimcp_hypothalamus_drives.c          # Byrnes alignment-safe drive system
    ${CMAKE_CURRENT_SOURCE_DIR}/../core/brain/regions/hypothalamus/nimcp_hypothalamus_drives_bio.c      # Drive system bio-async integration
    ${CMAKE_CURRENT_SOURCE_DIR}/../core/brain/regions/hypothalamus/nimcp_hypothalamus_homeostasis.c     # PI/PD homeostatic control
    ${CMAKE_CURRENT_SOURCE_DIR}/../core/brain/regions/hypothalamus/nimcp_hypothalamus_snc_bridge.c      # SNc/VTA bridge (reward → dopamine)
    ${CMAKE_CURRENT_SOURCE_DIR}/../core/brain/regions/hypothalamus/nimcp_hypothalamus_executive_bridge.c  # Executive bridge (goals)
    ${CMAKE_CURRENT_SOURCE_DIR}/../core/brain/regions/hypothalamus/nimcp_hypothalamus_attention_bridge.c  # Attention bridge (salience)
    ${CMAKE_CURRENT_SOURCE_DIR}/../core/brain/regions/hypothalamus/nimcp_hypothalamus_brainstem_bridge.c  # Brainstem bridge (arousal/pain)
    ${CMAKE_CURRENT_SOURCE_DIR}/../core/brain/regions/hypothalamus/nimcp_hypothalamus_medulla_bridge.c  # Medulla bridge (autonomic)
    ${CMAKE_CURRENT_SOURCE_DIR}/../core/brain/regions/hypothalamus/nimcp_hypothalamus_hippocampus_bridge.c  # Hippocampus bridge (memory-drive)
    ${CMAKE_CURRENT_SOURCE_DIR}/../core/brain/regions/hypothalamus/nimcp_hypothalamus_amygdala_bridge.c  # Amygdala bridge (emotion-drive)
    ${CMAKE_CURRENT_SOURCE_DIR}/../core/brain/regions/hypothalamus/nimcp_hypothalamus_drive_quantum_bridge.c  # Drive-quantum bridge (alignment)
    ${CMAKE_CURRENT_SOURCE_DIR}/../core/brain/regions/hypothalamus/nimcp_hypothalamus_immune_bridge.c  # Immune bridge (neuroimmune)
    ${CMAKE_CURRENT_SOURCE_DIR}/../core/brain/regions/hypothalamus/nimcp_hypothalamus_insula_bridge.c  # Insula bridge (interoception)
    ${CMAKE_CURRENT_SOURCE_DIR}/../core/brain/regions/hypothalamus/nimcp_hypothalamus_sleep_bridge.c  # Sleep bridge (circadian)
    ${CMAKE_CURRENT_SOURCE_DIR}/../core/brain/regions/hypothalamus/nimcp_hypothalamus_emotion_bridge.c  # Emotion bridge (HPA axis)
    ${CMAKE_CURRENT_SOURCE_DIR}/../core/brain/regions/hypothalamus/nimcp_hypothalamus_wellbeing_bridge.c  # Wellbeing bridge (homeostatic)
    ${CMAKE_CURRENT_SOURCE_DIR}/../core/brain/regions/hypothalamus/nimcp_hypothalamus_perception_bridge.c  # Perception bridge (sensory modulation)
    ${CMAKE_CURRENT_SOURCE_DIR}/../core/brain/regions/hypothalamus/nimcp_hypothalamus_broca_bridge.c  # Broca bridge (speech modulation)
    ${CMAKE_CURRENT_SOURCE_DIR}/../core/brain/regions/hypothalamus/nimcp_hypothalamus_quantum_bridge.c  # Quantum homeostatic optimization
    ${CMAKE_CURRENT_SOURCE_DIR}/../core/brain/regions/hypothalamus/nimcp_hypothalamus_alignment.c       # Alignment hardening & introspection (Phase 19)
    ${CMAKE_CURRENT_SOURCE_DIR}/../core/brain/regions/hypothalamus/nimcp_hypothalamus_orchestrator.c    # Hypothalamus orchestrator (bridge coordination)
    ${CMAKE_CURRENT_SOURCE_DIR}/../core/brain/regions/hypothalamus/nimcp_hypothalamus_internal_bus.c     # Internal bus (intra-module communication)
    ${CMAKE_CURRENT_SOURCE_DIR}/../core/brain/regions/hypothalamus/nimcp_hypothalamus_bio_async_bridge.c  # Unified bio-async integration bridge
    ${CMAKE_CURRENT_SOURCE_DIR}/../core/brain/regions/hypothalamus/nimcp_hypothalamus_cognitive_hub_bridge.c  # Cognitive hub integration
    ${CMAKE_CURRENT_SOURCE_DIR}/../core/brain/regions/hypothalamus/nimcp_hypothalamus_logging_bridge.c  # Logging integration bridge
    # Hypothalamus FEP Bridges (drive-FEP integration for cognitive modules)
    ${CMAKE_CURRENT_SOURCE_DIR}/../core/brain/regions/hypothalamus/fep/nimcp_hypo_bio_async_fep_bridge.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../core/brain/regions/hypothalamus/fep/nimcp_hypo_immune_fep_bridge.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../core/brain/regions/hypothalamus/fep/nimcp_hypo_logging_fep_bridge.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../core/brain/regions/hypothalamus/fep/nimcp_hypo_salience_fep_bridge.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../core/brain/regions/hypothalamus/fep/nimcp_hypo_reasoning_fep_bridge.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../core/brain/regions/hypothalamus/fep/nimcp_hypo_global_workspace_fep_bridge.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../core/brain/regions/hypothalamus/fep/nimcp_hypo_introspection_fep_bridge.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../core/brain/regions/hypothalamus/fep/nimcp_hypo_curiosity_fep_bridge.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../core/brain/regions/hypothalamus/fep/nimcp_hypo_game_theory_fep_bridge.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../core/brain/regions/hypothalamus/fep/nimcp_hypo_imagination_fep_bridge.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../core/brain/regions/hypothalamus/fep/nimcp_hypo_epistemic_fep_bridge.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../core/brain/regions/hypothalamus/fep/nimcp_hypo_collective_fep_bridge.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../core/brain/regions/hypothalamus/fep/nimcp_hypo_bias_fep_bridge.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../core/brain/regions/hypothalamus/fep/nimcp_hypo_tom_fep_bridge.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../core/brain/regions/hypothalamus/fep/nimcp_hypo_predictive_fep_bridge.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../core/brain/regions/hypothalamus/nimcp_hypothalamus_training_bridge.c   # Hypothalamus-training integration bridge
    ${CMAKE_CURRENT_SOURCE_DIR}/../core/brain/regions/hypothalamus/nimcp_hypothalamus_logic_bridge.c     # Hypothalamus-logic integration (hot cognition)
    ${CMAKE_CURRENT_SOURCE_DIR}/../core/brain/factory/init/nimcp_brain_init_hypothalamus.c              # Hypothalamus factory init
    ${CMAKE_CURRENT_SOURCE_DIR}/../core/brain/factory/init/nimcp_brain_init_inferior_colliculus.c      # Inferior colliculus factory init

    # Hippocampus - Episodic memory, spatial navigation, pattern separation/completion (Phase H1)
    ${CMAKE_CURRENT_SOURCE_DIR}/../core/brain/regions/hippocampus/nimcp_hippocampus.c                   # Hippocampus core implementation
    ${CMAKE_CURRENT_SOURCE_DIR}/../core/brain/regions/hippocampus/nimcp_hippocampus_adapter.c           # Hippocampus core adapter
    ${CMAKE_CURRENT_SOURCE_DIR}/../core/brain/regions/hippocampus/nimcp_hippocampus_quantum_bridge.c    # Hippocampus quantum bridge
    ${CMAKE_CURRENT_SOURCE_DIR}/../core/brain/regions/hippocampus/bridges/nimcp_hippocampus_kg_wiring.c  # Hippocampus KG wiring
    ${CMAKE_CURRENT_SOURCE_DIR}/../core/brain/factory/init/nimcp_brain_init_hippocampus.c               # Hippocampus init

    # Entorhinal Cortex - Grid cells, path integration, memory gateway (Phase E1)
    ${CMAKE_CURRENT_SOURCE_DIR}/../core/brain/regions/entorhinal/nimcp_entorhinal.c                     # Entorhinal core implementation
    ${CMAKE_CURRENT_SOURCE_DIR}/../core/brain/regions/entorhinal/nimcp_entorhinal_adapter.c            # Entorhinal adapter (MINIMAL — Wave 8B-c)
    ${CMAKE_CURRENT_SOURCE_DIR}/../core/brain/regions/entorhinal/nimcp_entorhinal_hypothalamus_bridge.c # Entorhinal-hypothalamus bridge
    ${CMAKE_CURRENT_SOURCE_DIR}/../core/brain/regions/entorhinal/nimcp_entorhinal_omni_bridge.c         # Entorhinal omnidirectional bridge
    ${CMAKE_CURRENT_SOURCE_DIR}/../core/brain/regions/entorhinal/nimcp_entorhinal_brain_init_bridge.c   # Entorhinal brain init bridge
    ${CMAKE_CURRENT_SOURCE_DIR}/../core/brain/factory/init/nimcp_brain_init_entorhinal.c               # Entorhinal brain init (Wave 15)

    # Perirhinal Cortex - Object recognition memory, familiarity (Phase PR1)
    ${CMAKE_CURRENT_SOURCE_DIR}/../core/brain/regions/perirhinal/nimcp_perirhinal.c                     # Perirhinal core implementation

    # Parahippocampal Cortex - Scene/context processing, spatial layouts (Phase PH1)
    ${CMAKE_CURRENT_SOURCE_DIR}/../core/brain/regions/parahippocampal/nimcp_parahippocampal.c           # Parahippocampal core implementation

    # Mammillary Bodies - Memory relay, head direction, Papez circuit (Phase M5)
    ${CMAKE_CURRENT_SOURCE_DIR}/../core/brain/regions/mammillary/nimcp_mammillary.c                     # Mammillary bodies implementation

    # Parietal Cortex - Spatial processing, somatosensory, sensorimotor integration (Phase PC1)
    ${CMAKE_CURRENT_SOURCE_DIR}/../core/brain/regions/parietal/nimcp_parietal_adapter.c                 # Parietal cortex core adapter
    ${CMAKE_CURRENT_SOURCE_DIR}/../core/brain/regions/parietal/nimcp_parietal_quantum_bridge.c          # Parietal quantum optimization
    ${CMAKE_CURRENT_SOURCE_DIR}/../core/brain/factory/init/nimcp_brain_init_parietal_cortex.c           # Parietal cortex factory init

    # Temporal Cortex - Auditory processing, semantic memory, object recognition (Phase TC1)
    ${CMAKE_CURRENT_SOURCE_DIR}/../core/brain/regions/temporal/nimcp_temporal_adapter.c                 # Temporal cortex core adapter
    ${CMAKE_CURRENT_SOURCE_DIR}/../core/brain/regions/temporal/nimcp_temporal_quantum_bridge.c          # Temporal quantum bridge
    ${CMAKE_CURRENT_SOURCE_DIR}/../core/brain/regions/temporal/nimcp_temporal_substrate_bridge.c        # Temporal substrate bridge
    ${CMAKE_CURRENT_SOURCE_DIR}/../core/brain/regions/temporal/nimcp_temporal_thalamic_bridge.c         # Temporal thalamic bridge
    ${CMAKE_CURRENT_SOURCE_DIR}/../core/brain/factory/init/nimcp_brain_init_temporal.c                  # Temporal factory init

    # Wernicke's Area - Language comprehension (posterior STG, BA 22) (Phase W1-W5 Complete)
    ${CMAKE_CURRENT_SOURCE_DIR}/../core/brain/regions/wernicke/nimcp_wernicke_adapter.c                 # Wernicke core adapter
    ${CMAKE_CURRENT_SOURCE_DIR}/../core/brain/regions/wernicke/nimcp_phonological_analyzer.c            # Phonological analysis layer
    ${CMAKE_CURRENT_SOURCE_DIR}/../core/brain/regions/wernicke/nimcp_lexical_access.c                   # Lexical access (word recognition)
    ${CMAKE_CURRENT_SOURCE_DIR}/../core/brain/regions/wernicke/nimcp_semantic_integrator.c              # Semantic integration (meaning)
    ${CMAKE_CURRENT_SOURCE_DIR}/../core/brain/regions/wernicke/nimcp_wernicke_broca_bridge.c            # Arcuate fasciculus (Broca connection)
    ${CMAKE_CURRENT_SOURCE_DIR}/../core/brain/regions/wernicke/nimcp_wernicke_substrate_bridge.c        # Substrate bridge (metabolic modulation)
    ${CMAKE_CURRENT_SOURCE_DIR}/../core/brain/regions/wernicke/nimcp_omni_wernicke_bridge.c             # Omnidirectional inference bridge
    ${CMAKE_CURRENT_SOURCE_DIR}/../core/brain/regions/wernicke/nimcp_syntactic_comprehension.c          # Syntactic parsing and comprehension
    ${CMAKE_CURRENT_SOURCE_DIR}/../core/brain/regions/wernicke/nimcp_wernicke_quantum_bridge.c          # Quantum acceleration
    ${CMAKE_CURRENT_SOURCE_DIR}/../core/brain/regions/wernicke/nimcp_wernicke_gpu_bio_bridge.c          # GPU bio-async bridge
    ${CMAKE_CURRENT_SOURCE_DIR}/../core/brain/regions/wernicke/nimcp_wernicke_immune.c                  # Wernicke-immune integration
    ${CMAKE_CURRENT_SOURCE_DIR}/../core/brain/regions/wernicke/nimcp_wernicke_nlp_bridge.c              # Comprehensive NLP integration

    # Insula - Interoception, emotional awareness, social emotions (Phase I1)
    ${CMAKE_CURRENT_SOURCE_DIR}/../core/brain/regions/insula/nimcp_insula_adapter.c                     # Insula core adapter
    ${CMAKE_CURRENT_SOURCE_DIR}/../core/brain/regions/insula/nimcp_insula_quantum_bridge.c              # Insula quantum bridge

    # Motor Cortex - Motor control, movement planning (Phase M1)
    ${CMAKE_CURRENT_SOURCE_DIR}/../core/brain/regions/motor/nimcp_motor_adapter.c                       # Motor cortex core adapter
    ${CMAKE_CURRENT_SOURCE_DIR}/../core/brain/regions/motor/nimcp_motor_quantum_bridge.c                # Motor quantum bridge

    # Occipital Cortex - Visual processing (Phase O1)
    ${CMAKE_CURRENT_SOURCE_DIR}/../core/brain/regions/occipital/nimcp_occipital_adapter.c               # Occipital cortex core adapter
    ${CMAKE_CURRENT_SOURCE_DIR}/../core/brain/regions/occipital/nimcp_occipital_quantum_bridge.c        # Occipital quantum bridge

    # Somatosensory Cortex - Touch, proprioception, pain processing (Phase 6: BR-9)
    ${CMAKE_CURRENT_SOURCE_DIR}/../core/brain/regions/somatosensory/nimcp_somatosensory.c               # Somatosensory core implementation
    ${CMAKE_CURRENT_SOURCE_DIR}/../core/brain/regions/somatosensory/nimcp_soma_jepa_bridge.c            # Phase 4r: soma ↔ JEPA
    ${CMAKE_CURRENT_SOURCE_DIR}/../core/brain/regions/somatosensory/bridges/nimcp_soma_bio_async_bridge.c    # Somatosensory bio-async bridge
    ${CMAKE_CURRENT_SOURCE_DIR}/../core/brain/regions/somatosensory/bridges/nimcp_soma_quantum_bridge.c      # Somatosensory quantum bridge
    ${CMAKE_CURRENT_SOURCE_DIR}/../core/brain/regions/somatosensory/bridges/nimcp_soma_cerebellum_bridge.c   # Somatosensory-cerebellum bridge
    ${CMAKE_CURRENT_SOURCE_DIR}/../core/brain/regions/somatosensory/bridges/nimcp_soma_medulla_bridge.c      # Somatosensory-medulla bridge

    # Olfactory Cortex - Smell, odor identification, olfactory memory (Phase 6: BR-10)
    ${CMAKE_CURRENT_SOURCE_DIR}/../core/brain/regions/olfactory/nimcp_olfactory.c                       # Olfactory core implementation
    ${CMAKE_CURRENT_SOURCE_DIR}/../core/brain/regions/olfactory/bridges/nimcp_olfact_bio_async_bridge.c      # Olfactory bio-async bridge
    ${CMAKE_CURRENT_SOURCE_DIR}/../core/brain/regions/olfactory/bridges/nimcp_olfact_quantum_bridge.c        # Olfactory quantum bridge

    # Gustatory Cortex - Taste processing, food reward, disgust response (Phase 6: BR-11)
    ${CMAKE_CURRENT_SOURCE_DIR}/../core/brain/regions/gustatory/nimcp_gustatory.c                       # Gustatory core implementation
    ${CMAKE_CURRENT_SOURCE_DIR}/../core/brain/regions/gustatory/bridges/nimcp_gust_bio_async_bridge.c        # Gustatory bio-async bridge
    ${CMAKE_CURRENT_SOURCE_DIR}/../core/brain/regions/gustatory/bridges/nimcp_gust_quantum_bridge.c          # Gustatory quantum bridge

    # Sensory Integration - Cross-sensory processing bridges (Phase 6)
    ${CMAKE_CURRENT_SOURCE_DIR}/../core/brain/regions/sensory_integration/nimcp_chemosensory_bridge.c        # Olfactory-gustatory flavor binding
    ${CMAKE_CURRENT_SOURCE_DIR}/../core/brain/regions/sensory_integration/nimcp_tactile_motor_bridge.c       # Somatosensory-motor coordination
    ${CMAKE_CURRENT_SOURCE_DIR}/../core/brain/regions/sensory_integration/nimcp_sensory_swarm_bridge.c       # Multi-sensory swarm integration
    ${CMAKE_CURRENT_SOURCE_DIR}/../core/brain/regions/sensory_integration/nimcp_trigeminal_oral_bridge.c     # Trigeminal-oral integration (spicy, texture, mouthfeel)

    # Sensory Knowledge Graph Wiring (Phase 6)
    ${CMAKE_CURRENT_SOURCE_DIR}/../integration/knowledge/nimcp_sensory_kg_wiring.c                           # Sensory module KG registration

    # Claustrum - Cross-modal binding, consciousness integration, salience detection (Phase 7: BR-12)
    ${CMAKE_CURRENT_SOURCE_DIR}/../core/brain/regions/claustrum/nimcp_claustrum.c                            # Claustrum core implementation
    ${CMAKE_CURRENT_SOURCE_DIR}/../core/brain/regions/claustrum/bridges/nimcp_claustrum_bio_async_bridge.c   # Claustrum bio-async bridge
    ${CMAKE_CURRENT_SOURCE_DIR}/../core/brain/regions/claustrum/bridges/nimcp_claustrum_brain_init.c         # Claustrum brain initialization
    ${CMAKE_CURRENT_SOURCE_DIR}/../core/brain/regions/claustrum/bridges/nimcp_claustrum_kg_wiring.c          # Claustrum KG wiring
    ${CMAKE_CURRENT_SOURCE_DIR}/../core/brain/regions/claustrum/bridges/nimcp_claustrum_security.c           # Claustrum security bridge

    # Orbitofrontal Cortex (OFC) - Value representation, flexible decision making, reversal learning (Phase 7: BR-13)
    ${CMAKE_CURRENT_SOURCE_DIR}/../core/brain/regions/ofc/nimcp_ofc.c                                        # OFC core implementation
    ${CMAKE_CURRENT_SOURCE_DIR}/../core/brain/regions/ofc/bridges/nimcp_ofc_bio_async_bridge.c               # OFC bio-async bridge
    ${CMAKE_CURRENT_SOURCE_DIR}/../core/brain/regions/ofc/bridges/nimcp_ofc_brain_init.c                     # OFC brain initialization
    ${CMAKE_CURRENT_SOURCE_DIR}/../core/brain/regions/ofc/bridges/nimcp_ofc_kg_wiring.c                      # OFC KG wiring
    ${CMAKE_CURRENT_SOURCE_DIR}/../core/brain/regions/ofc/bridges/nimcp_ofc_security.c                       # OFC security bridge

    # Periaqueductal Gray (PAG) - Pain modulation, defensive behaviors, analgesia (Phase 7: BR-14)
    ${CMAKE_CURRENT_SOURCE_DIR}/../core/brain/regions/pag/nimcp_pag.c                                        # PAG core implementation
    ${CMAKE_CURRENT_SOURCE_DIR}/../core/brain/regions/pag/bridges/nimcp_pag_bio_async_bridge.c               # PAG bio-async bridge
    ${CMAKE_CURRENT_SOURCE_DIR}/../core/brain/regions/pag/bridges/nimcp_pag_brain_init.c                     # PAG brain initialization
    ${CMAKE_CURRENT_SOURCE_DIR}/../core/brain/regions/pag/bridges/nimcp_pag_kg_wiring.c                      # PAG KG wiring
    ${CMAKE_CURRENT_SOURCE_DIR}/../core/brain/regions/pag/bridges/nimcp_pag_security.c                       # PAG security bridge

    # Red Nucleus - Motor refinement, limb coordination, rubrospinal control (Phase 7: BR-15)
    ${CMAKE_CURRENT_SOURCE_DIR}/../core/brain/regions/red_nucleus/nimcp_red_nucleus.c                        # Red nucleus core implementation
    ${CMAKE_CURRENT_SOURCE_DIR}/../core/brain/regions/red_nucleus/bridges/nimcp_rn_bio_async_bridge.c        # Red nucleus bio-async bridge
    ${CMAKE_CURRENT_SOURCE_DIR}/../core/brain/regions/red_nucleus/bridges/nimcp_rn_brain_init.c              # Red nucleus brain initialization
    ${CMAKE_CURRENT_SOURCE_DIR}/../core/brain/regions/red_nucleus/bridges/nimcp_red_nucleus_kg_wiring.c      # Red nucleus KG wiring
    ${CMAKE_CURRENT_SOURCE_DIR}/../core/brain/regions/red_nucleus/bridges/nimcp_rn_security.c                # Red nucleus security bridge

    # Reticular Formation - Arousal, consciousness, sleep-wake control (Phase 7: BR-16)
    ${CMAKE_CURRENT_SOURCE_DIR}/../core/brain/regions/reticular/nimcp_reticular.c                            # Reticular formation core implementation
    ${CMAKE_CURRENT_SOURCE_DIR}/../core/brain/regions/reticular/bridges/nimcp_reticular_bio_async_bridge.c   # Reticular bio-async bridge
    ${CMAKE_CURRENT_SOURCE_DIR}/../core/brain/regions/reticular/bridges/nimcp_reticular_brain_init.c         # Reticular brain initialization
    ${CMAKE_CURRENT_SOURCE_DIR}/../core/brain/regions/reticular/bridges/nimcp_reticular_kg_wiring.c          # Reticular KG wiring
    ${CMAKE_CURRENT_SOURCE_DIR}/../core/brain/regions/reticular/bridges/nimcp_reticular_security.c           # Reticular security bridge

    # Retrosplenial Cortex - Spatial memory, navigation, viewpoint transformation (Phase 7: BR-17)
    ${CMAKE_CURRENT_SOURCE_DIR}/../core/brain/regions/retrosplenial/nimcp_retrosplenial.c                    # Retrosplenial core implementation
    ${CMAKE_CURRENT_SOURCE_DIR}/../core/brain/regions/retrosplenial/bridges/nimcp_rsc_bio_async_bridge.c     # Retrosplenial bio-async bridge
    ${CMAKE_CURRENT_SOURCE_DIR}/../core/brain/regions/retrosplenial/bridges/nimcp_rsc_brain_init.c           # Retrosplenial brain initialization
    ${CMAKE_CURRENT_SOURCE_DIR}/../core/brain/regions/retrosplenial/bridges/nimcp_rsc_kg_wiring.c            # Retrosplenial KG wiring
    ${CMAKE_CURRENT_SOURCE_DIR}/../core/brain/regions/retrosplenial/bridges/nimcp_rsc_security.c             # Retrosplenial security bridge

    # Wernicke's Area - Language comprehension, phonological/semantic processing (KG wiring)
    ${CMAKE_CURRENT_SOURCE_DIR}/../core/brain/regions/wernicke/bridges/nimcp_wernicke_kg_wiring.c            # Wernicke KG wiring

    # Temporal Lobe - Auditory processing, object/face recognition, memory encoding (KG wiring)
    ${CMAKE_CURRENT_SOURCE_DIR}/../core/brain/regions/temporal/bridges/nimcp_temporal_kg_wiring.c            # Temporal KG wiring

    # Insula - Interoception, emotion-cognition integration, pain processing (KG wiring)
    ${CMAKE_CURRENT_SOURCE_DIR}/../core/brain/regions/insula/bridges/nimcp_insula_kg_wiring.c               # Insula KG wiring

    # Cingulate Cortex - Conflict monitoring, error detection, reward processing (KG wiring)
    ${CMAKE_CURRENT_SOURCE_DIR}/../core/brain/regions/cingulate/bridges/nimcp_cingulate_kg_wiring.c         # Cingulate KG wiring

    # Cerebellum - Motor learning, coordination, timing control (KG wiring)
    ${CMAKE_CURRENT_SOURCE_DIR}/../core/brain/regions/cerebellum/bridges/nimcp_cerebellum_kg_wiring.c       # Cerebellum KG wiring

    # Hypothalamus - Homeostasis, circadian rhythm, HPA stress response (KG wiring)
    ${CMAKE_CURRENT_SOURCE_DIR}/../core/brain/regions/hypothalamus/bridges/nimcp_hypothalamus_kg_wiring.c   # Hypothalamus KG wiring

    # VTA - Reward prediction, motivation, reinforcement learning (KG wiring)
    ${CMAKE_CURRENT_SOURCE_DIR}/../core/brain/regions/vta/bridges/nimcp_vta_kg_wiring.c                     # VTA KG wiring

    # Habenula - Reward evaluation, punishment signaling, mood regulation (KG wiring)
    ${CMAKE_CURRENT_SOURCE_DIR}/../core/brain/regions/habenula/bridges/nimcp_habenula_kg_wiring.c           # Habenula KG wiring

    # Locus Coeruleus - Noradrenergic arousal, attention modulation, stress (KG wiring)
    ${CMAKE_CURRENT_SOURCE_DIR}/../core/brain/regions/locus_coeruleus/bridges/nimcp_lc_kg_wiring.c          # Locus Coeruleus KG wiring

    # Entorhinal Cortex - Grid cells, spatial mapping, memory gateway (KG wiring)
    ${CMAKE_CURRENT_SOURCE_DIR}/../core/brain/regions/entorhinal/bridges/nimcp_entorhinal_kg_wiring.c       # Entorhinal KG wiring

    # W3 KG retrofit (2026-04-24): 10 previously-unwired brain regions
    ${CMAKE_CURRENT_SOURCE_DIR}/../core/brain/regions/occipital/bridges/nimcp_occipital_kg_wiring.c               # Occipital KG wiring (W3)
    ${CMAKE_CURRENT_SOURCE_DIR}/../core/brain/regions/somatosensory/bridges/nimcp_somatosensory_kg_wiring.c       # Somatosensory KG wiring (W3)
    ${CMAKE_CURRENT_SOURCE_DIR}/../core/brain/regions/motor/bridges/nimcp_motor_kg_wiring.c                       # Motor KG wiring (W3)
    ${CMAKE_CURRENT_SOURCE_DIR}/../core/brain/regions/broca/bridges/nimcp_broca_kg_wiring.c                       # Broca KG wiring (W3)
    ${CMAKE_CURRENT_SOURCE_DIR}/../core/brain/regions/gustatory/bridges/nimcp_gustatory_kg_wiring.c               # Gustatory KG wiring (W3)
    ${CMAKE_CURRENT_SOURCE_DIR}/../core/brain/regions/olfactory/bridges/nimcp_olfactory_kg_wiring.c               # Olfactory KG wiring (W3)
    ${CMAKE_CURRENT_SOURCE_DIR}/../core/brain/regions/brainstem/bridges/nimcp_brainstem_kg_wiring.c               # Brainstem KG wiring (W3)
    ${CMAKE_CURRENT_SOURCE_DIR}/../core/brain/regions/raphe/bridges/nimcp_raphe_kg_wiring.c                       # Raphe KG wiring (W3)
    ${CMAKE_CURRENT_SOURCE_DIR}/../core/brain/regions/parahippocampal/bridges/nimcp_parahippocampal_kg_wiring.c   # Parahippocampal KG wiring (W3)
    ${CMAKE_CURRENT_SOURCE_DIR}/../core/brain/regions/perirhinal/bridges/nimcp_perirhinal_kg_wiring.c             # Perirhinal KG wiring (W3)

    # W4 KG retrofit (2026-04-24): 5 remaining regions + per-subcortical runtime events
    ${CMAKE_CURRENT_SOURCE_DIR}/../core/brain/regions/endocannabinoid/bridges/nimcp_endocannabinoid_kg_wiring.c           # Endocannabinoid KG wiring (W4)
    ${CMAKE_CURRENT_SOURCE_DIR}/../core/brain/regions/glymphatic/bridges/nimcp_glymphatic_kg_wiring.c                     # Glymphatic KG wiring (W4)
    ${CMAKE_CURRENT_SOURCE_DIR}/../core/brain/regions/mammillary/bridges/nimcp_mammillary_kg_wiring.c                     # Mammillary KG wiring (W4)
    ${CMAKE_CURRENT_SOURCE_DIR}/../core/brain/regions/neuropeptide/bridges/nimcp_neuropeptide_kg_wiring.c                 # Neuropeptide KG wiring (W4)
    ${CMAKE_CURRENT_SOURCE_DIR}/../core/brain/regions/sensory_integration/bridges/nimcp_sensory_integration_kg_wiring.c   # Sensory Integration KG wiring (W4)
    ${CMAKE_CURRENT_SOURCE_DIR}/../core/brain/subcortical/bridges/nimcp_subcortical_runtime_events.c                      # Per-subcortical-module runtime events (W4)

    # W11 KG retrofit (2026-04-24): safety KG event emitters
    ${CMAKE_CURRENT_SOURCE_DIR}/../security/kg_events/nimcp_w11_safety_kg_events.c                                       # W11 safety KG events (ethics/LGSS/mental_health/immune)

    # W10 KG retrofit (2026-04-24): affective/social/ToM/mirror/empathy runtime emit + query helpers
    ${CMAKE_CURRENT_SOURCE_DIR}/../cognitive/bridges/nimcp_wave10_affective_kg.c                                         # W10 affective/social KG runtime events

    # W13 KG retrofit (2026-04-24): curiosity/meta/consolidation/sleep-wake/curriculum/analogy/multiscale/contrastive-self
    ${CMAKE_CURRENT_SOURCE_DIR}/../cognitive/bridges/nimcp_wave13_metacog_kg.c                                           # W13 meta-cognitive KG runtime events

    # W14 KG retrofit (2026-04-24): math disciplines + game-theory + parietal genius + financial orchestrator
    ${CMAKE_CURRENT_SOURCE_DIR}/../cognitive/bridges/nimcp_wave14_math_genius_kg.c                                       # W14 math/game/genius KG runtime events

    # W12 KG retrofit (2026-04-24): language + communication runtime emit helpers
    ${CMAKE_CURRENT_SOURCE_DIR}/../cognitive/bridges/nimcp_w12_language_kg_events.c                                      # W12 language/communication KG runtime events

    ${CMAKE_CURRENT_SOURCE_DIR}/../core/brain/factory/init/nimcp_brain_init_region_kg_bridges.c                   # W3+W4+W10+W12+W13+W14 region KG init wave

    # W5 KG retrofit (2026-04-24): 6 network-type KG structural wirings
    ${CMAKE_CURRENT_SOURCE_DIR}/../core/brain/factory/init/nimcp_brain_init_network_kg_wiring.c                   # W5 network-type KG init wave (Wave 32)

    # Prefrontal Cortex - Executive function, decision-making (Phase PF1)
    ${CMAKE_CURRENT_SOURCE_DIR}/../core/brain/regions/prefrontal/nimcp_prefrontal_adapter.c             # Prefrontal cortex core adapter
    ${CMAKE_CURRENT_SOURCE_DIR}/../core/brain/regions/prefrontal/nimcp_prefrontal_quantum_bridge.c      # Prefrontal quantum bridge
    ${CMAKE_CURRENT_SOURCE_DIR}/../core/brain/regions/prefrontal/bridges/nimcp_pfc_kg_wiring.c          # Prefrontal KG wiring

    ${CMAKE_CURRENT_SOURCE_DIR}/../core/brain/regions/nimcp_brain_region_predictive.c  # Predictive coding brain region extension — re-enabled 2026-04-24 to unlock predictive_immune

    # Subcortical brain structures (basal ganglia, thalamus, etc.)
    ${CMAKE_CURRENT_SOURCE_DIR}/../core/brain/subcortical/nimcp_striatum.c           # Striatum D1/D2 pathways
    ${CMAKE_CURRENT_SOURCE_DIR}/../core/brain/subcortical/nimcp_globus_pallidus.c    # Globus pallidus (GPe/GPi)
    ${CMAKE_CURRENT_SOURCE_DIR}/../core/brain/subcortical/nimcp_substantia_nigra.c   # Substantia nigra (SNc/SNr)
    ${CMAKE_CURRENT_SOURCE_DIR}/../core/brain/subcortical/nimcp_subthalamic.c        # Subthalamic nucleus
    ${CMAKE_CURRENT_SOURCE_DIR}/../core/brain/subcortical/nimcp_basal_ganglia.c      # Basal ganglia action selection
    ${CMAKE_CURRENT_SOURCE_DIR}/../core/brain/subcortical/nimcp_thalamus.c          # Thalamic nuclei relay
    ${CMAKE_CURRENT_SOURCE_DIR}/../core/brain/subcortical/nimcp_amygdala.c          # Amygdala emotion/fear processing
    ${CMAKE_CURRENT_SOURCE_DIR}/../core/brain/subcortical/nimcp_amygdala_training_bridge.c  # Amygdala-training integration (Yerkes-Dodson)
    ${CMAKE_CURRENT_SOURCE_DIR}/../core/brain/subcortical/nimcp_amygdala_attention_bridge.c  # Amygdala-attention integration (threat salience)
    ${CMAKE_CURRENT_SOURCE_DIR}/../core/brain/subcortical/nimcp_amygdala_stress_bridge.c  # Amygdala-stress/wellbeing integration (stress-anxiety feedback)
    ${CMAKE_CURRENT_SOURCE_DIR}/../core/brain/subcortical/nimcp_amygdala_autobio_bridge.c  # Amygdala-autobiographical memory integration (emotional tagging, recall reactivation)
    ${CMAKE_CURRENT_SOURCE_DIR}/../core/brain/subcortical/nimcp_amygdala_fep_bridge.c  # Amygdala-FEP integration (threat as prediction error)
    ${CMAKE_CURRENT_SOURCE_DIR}/../core/brain/regions/amygdala/bridges/nimcp_amygdala_kg_wiring.c       # Amygdala KG wiring
    ${CMAKE_CURRENT_SOURCE_DIR}/../core/brain/subcortical/nimcp_basal_ganglia_fep_bridge.c  # Basal ganglia-FEP integration (action selection as free energy minimization)
    ${CMAKE_CURRENT_SOURCE_DIR}/../core/brain/subcortical/nimcp_basal_ganglia_thalamus_bridge.c  # Basal ganglia-thalamus motor relay
    ${CMAKE_CURRENT_SOURCE_DIR}/../core/brain/subcortical/nimcp_basal_ganglia_amygdala_bridge.c  # Basal ganglia-amygdala emotional modulation
    ${CMAKE_CURRENT_SOURCE_DIR}/../core/brain/subcortical/nimcp_basal_ganglia_training_bridge.c  # Basal ganglia-training plasticity
    ${CMAKE_CURRENT_SOURCE_DIR}/../core/brain/subcortical/nimcp_basal_ganglia_executive_bridge.c  # Basal ganglia-executive control

    # Basal ganglia enhancement modules (Phase BG-ENH)
    ${CMAKE_CURRENT_SOURCE_DIR}/../core/brain/subcortical/nimcp_bg_beta_oscillations.c    # Beta oscillations and pathological states
    ${CMAKE_CURRENT_SOURCE_DIR}/../core/brain/subcortical/nimcp_bg_neuromodulators.c      # Multi-neuromodulator integration (5HT, ACh, NE, adenosine)
    ${CMAKE_CURRENT_SOURCE_DIR}/../core/brain/subcortical/nimcp_bg_hierarchical_rl.c      # Hierarchical RL with options framework
    ${CMAKE_CURRENT_SOURCE_DIR}/../core/brain/subcortical/nimcp_nucleus_accumbens.c       # Nucleus accumbens (motivation, PIT)
    ${CMAKE_CURRENT_SOURCE_DIR}/../core/brain/subcortical/nimcp_superior_colliculus.c     # Superior colliculus (gaze, orienting)
    ${CMAKE_CURRENT_SOURCE_DIR}/../core/brain/subcortical/nimcp_inferior_colliculus.c    # Inferior colliculus (auditory midbrain)
    ${CMAKE_CURRENT_SOURCE_DIR}/../core/brain/subcortical/nimcp_inferior_colliculus_bridges.c  # IC bridge integrations
    ${CMAKE_CURRENT_SOURCE_DIR}/../core/brain/subcortical/nimcp_bg_model_based.c          # Model-based planning
    ${CMAKE_CURRENT_SOURCE_DIR}/../core/brain/subcortical/nimcp_striatal_interneurons.c   # Striatal interneurons (FSI, TAN, LTS)
    ${CMAKE_CURRENT_SOURCE_DIR}/../core/brain/subcortical/nimcp_bg_cerebellar_coord.c     # BG-cerebellar coordination
    ${CMAKE_CURRENT_SOURCE_DIR}/../core/brain/subcortical/nimcp_bg_outcome_devaluation.c  # Outcome devaluation (goal vs habit)
    ${CMAKE_CURRENT_SOURCE_DIR}/../core/brain/subcortical/nimcp_bg_temporal_credit.c      # Temporal credit assignment (TD-lambda)
    ${CMAKE_CURRENT_SOURCE_DIR}/../core/brain/subcortical/nimcp_basal_ganglia_enhanced.c  # Unified enhanced BG system
    ${CMAKE_CURRENT_SOURCE_DIR}/../core/brain/subcortical/nimcp_bg_striosome_matrix.c     # Striosome-matrix compartmentalization
    ${CMAKE_CURRENT_SOURCE_DIR}/../core/brain/subcortical/nimcp_bg_sequence_chunking.c    # Action sequence chunking
    ${CMAKE_CURRENT_SOURCE_DIR}/../core/brain/subcortical/nimcp_bg_vigor.c                # Vigor/effort modulation
    ${CMAKE_CURRENT_SOURCE_DIR}/../core/brain/subcortical/bridges/nimcp_subcortical_kg_wiring.c  # Subcortical KG wiring (basal ganglia, thalamus, NAc)

    # Multi-modal integration (Phase 8: Unified Architecture)
    ${CMAKE_CURRENT_SOURCE_DIR}/../core/integration/nimcp_multimodal_integration.c

    # Neuron models - Plugin architecture
    ${CMAKE_CURRENT_SOURCE_DIR}/../core/neuron_models/nimcp_neuron_model.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../core/neuron_models/nimcp_izhikevich.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../core/neuron_models/nimcp_two_compartment.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../core/neuron_models/nimcp_neuron_model_bio_async.c  # Phase BIO-ASYNC: Neuron model bio-async integration

    # Synapse computation - NIMCP 2.7 MAJOR FEATURE
    ${CMAKE_CURRENT_SOURCE_DIR}/../core/synapse_compute/nimcp_synapse_compute.c

    # Axon signal propagation - Action potential modeling
    ${CMAKE_CURRENT_SOURCE_DIR}/../core/axon/nimcp_axon.c

    # Dendrite computation and integration - Phase 1.5.7
    ${CMAKE_CURRENT_SOURCE_DIR}/../core/dendrite/nimcp_dendrite.c

    # Shared substrate-effects helper (consumed by SNN/LNN/CNN/FNO/HNN adapters)
    ${CMAKE_CURRENT_SOURCE_DIR}/../core/substrate/nimcp_substrate_effects.c

    # Shared thalamic-channel adapter (consumed by LNN/CNN/FNO/HNN; SNN keeps
    # its existing bridge. Narrow get_gate/submit/tick interface over the
    # thalamic router.)
    ${CMAKE_CURRENT_SOURCE_DIR}/../core/thalamic/nimcp_thalamic_channel.c

    # Synapse type system - NIMCP 2.8.7 (Phase 8.7) - BIOLOGICAL DIVERSITY
    ${CMAKE_CURRENT_SOURCE_DIR}/../core/synapse_types/nimcp_synapse_types.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../core/neuralnet/nimcp_synapse_embeddings.c

    # Fractal Topology - NIMCP 2.7 (scale-free networks)
    ${CMAKE_CURRENT_SOURCE_DIR}/../core/topology/nimcp_fractal_topology.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../core/topology/nimcp_network_builder.c

    # NLP Integration - NIMCP 2.7 (combines synapses + attention + neuromodulation)
    # Note: networking/nlp version uses networking_nlp prefix to avoid conflict
    ${CMAKE_CURRENT_SOURCE_DIR}/../nlp/nimcp_nlp.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../nlp/nimcp_spike_nlp.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../nlp/nimcp_multimodal_nlp_bridge.c

    # Cognitive functions
    ${CMAKE_CURRENT_SOURCE_DIR}/../cognitive/ethics/nimcp_ethics.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../cognitive/ethics/nimcp_ethics_evaluation.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../cognitive/ethics/nimcp_ethics_asimov.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../cognitive/ethics/nimcp_ethics_policies.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../cognitive/ethics/nimcp_ethics_incidents.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../cognitive/ethics/nimcp_ethics_learning.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../cognitive/ethics/nimcp_ethics_hyperbolic.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../cognitive/ethics/nimcp_combinatorial_harm.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../cognitive/ethics/nimcp_ethics_warfare.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../cognitive/ethics/nimcp_ethics_snn_bridge.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../cognitive/ethics/nimcp_ethics_plasticity_bridge.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../cognitive/knowledge/nimcp_knowledge.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../cognitive/knowledge/nimcp_knowledge_hyperbolic.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../cognitive/knowledge/nimcp_knowledge_cow.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../cognitive/knowledge/nimcp_knowledge_fractal.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../cognitive/knowledge/nimcp_kg_reader.c  # KG Reader for runtime self-awareness
    ${CMAKE_CURRENT_SOURCE_DIR}/../cognitive/curiosity/nimcp_curiosity.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../cognitive/curiosity/nimcp_curiosity_fep_bridge.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../cognitive/curiosity/nimcp_curiosity_enhanced.c  # Enhanced Curiosity (10 enhancements: boredom, interest decay, types, anxiety, social, meta, contagion, surprise, fatigue, counterfactual)
    ${CMAKE_CURRENT_SOURCE_DIR}/../cognitive/curiosity/nimcp_curiosity_snn_bridge.c  # Curiosity-SNN integration
    ${CMAKE_CURRENT_SOURCE_DIR}/../cognitive/curiosity/nimcp_curiosity_plasticity_bridge.c  # Curiosity-Plasticity integration
    ${CMAKE_CURRENT_SOURCE_DIR}/../cognitive/curiosity/nimcp_curiosity_fractal.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../cognitive/curiosity/nimcp_curiosity_hyperbolic.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../cognitive/curiosity/nimcp_curiosity_sleep_bridge.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../cognitive/curiosity/nimcp_information_forager.c  # Autonomous curiosity-driven learning loop
    ${CMAKE_CURRENT_SOURCE_DIR}/../cognitive/introspection/nimcp_introspection.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../cognitive/introspection/nimcp_ensemble_uncertainty.c  # Real Ensemble Uncertainty Quantification
    ${CMAKE_CURRENT_SOURCE_DIR}/../cognitive/introspection/nimcp_connectivity_health.c  # Phase 1.5.4: Connectivity Health Assessment
    ${CMAKE_CURRENT_SOURCE_DIR}/../cognitive/introspection/nimcp_consciousness_metrics.c  # Consciousness Metrics (IIT Φ)
    ${CMAKE_CURRENT_SOURCE_DIR}/../cognitive/introspection/nimcp_temporal_patterns.c  # Temporal Pattern Analysis
    ${CMAKE_CURRENT_SOURCE_DIR}/../cognitive/introspection/nimcp_introspection_snn_bridge.c  # Introspection-SNN integration
    ${CMAKE_CURRENT_SOURCE_DIR}/../cognitive/introspection/nimcp_introspection_plasticity_bridge.c  # Introspection-Plasticity integration
    ${CMAKE_CURRENT_SOURCE_DIR}/../cognitive/wellbeing/nimcp_wellbeing.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../cognitive/wellbeing/nimcp_wellbeing_prediction.c  # Predictive Distress Modeling
    ${CMAKE_CURRENT_SOURCE_DIR}/../cognitive/wellbeing/nimcp_wellbeing_enhanced.c  # Enhanced Wellbeing Core (includes substrate, sleep, mental health integration)
    ${CMAKE_CURRENT_SOURCE_DIR}/../cognitive/wellbeing/nimcp_wellbeing_eudaimonic.c  # Eudaimonic Wellbeing
    ${CMAKE_CURRENT_SOURCE_DIR}/../cognitive/wellbeing/nimcp_wellbeing_free_energy_bridge.c  # Free Energy Integration
    ${CMAKE_CURRENT_SOURCE_DIR}/../cognitive/wellbeing/nimcp_wellbeing_resources.c  # Platform Resource Metrics
    ${CMAKE_CURRENT_SOURCE_DIR}/../cognitive/wellbeing/nimcp_wellbeing_snn_bridge.c  # Wellbeing-SNN integration
    ${CMAKE_CURRENT_SOURCE_DIR}/../cognitive/wellbeing/nimcp_wellbeing_plasticity_bridge.c  # Wellbeing-Plasticity integration
    # NOTE: homeostasis, substrate_bridge, sleep_bridge, mental_health_bridge are already in enhanced.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../cognitive/consolidation/nimcp_consolidation.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../cognitive/salience/nimcp_salience.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../cognitive/logic/nimcp_symbolic_logic.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../cognitive/epistemic/nimcp_epistemic_filter.c  # Phase 9.2: Bias prevention
    ${CMAKE_CURRENT_SOURCE_DIR}/../cognitive/epistemic/nimcp_epistemic_snn_bridge.c  # Epistemic-SNN integration
    ${CMAKE_CURRENT_SOURCE_DIR}/../cognitive/epistemic/nimcp_epistemic_plasticity_bridge.c  # Epistemic-Plasticity integration
    ${CMAKE_CURRENT_SOURCE_DIR}/../cognitive/working_memory/nimcp_working_memory.c  # Phase 10.1: Working Memory
    ${CMAKE_CURRENT_SOURCE_DIR}/../cognitive/fault_tolerance/nimcp_recovery_episodic_memory.c  # Phase 10: Episodic Memory for Recovery History
    ${CMAKE_CURRENT_SOURCE_DIR}/../cognitive/fault_tolerance/nimcp_fault_working_memory.c  # Phase 10: Working Memory for Active Fault Context
    ${CMAKE_CURRENT_SOURCE_DIR}/../cognitive/fault_tolerance/nimcp_recovery_executive.c  # Phase 10: Executive Function for Recovery Planning
    ${CMAKE_CURRENT_SOURCE_DIR}/../cognitive/fault_tolerance/nimcp_recovery_parietal_bridge.c  # Phase 10: Parietal Integration for Recovery
    ${CMAKE_CURRENT_SOURCE_DIR}/../cognitive/fault_tolerance/nimcp_metacognition.c  # Phase 10: Metacognition for Self-Monitoring
    ${CMAKE_CURRENT_SOURCE_DIR}/../cognitive/fault_tolerance/nimcp_fault_attention.c  # Phase 10: Attention Mechanism for Error Prioritization
    ${CMAKE_CURRENT_SOURCE_DIR}/../cognitive/fault_tolerance/nimcp_failure_prediction.c  # Phase 10: Predictive Coding for Failure Prediction
    ${CMAKE_CURRENT_SOURCE_DIR}/../cognitive/fault_tolerance/nimcp_recovery_consolidation.c  # Phase 10: Consolidation for Long-Term Learning
    ${CMAKE_CURRENT_SOURCE_DIR}/../cognitive/fault_tolerance/nimcp_emotional_tagging.c  # Phase 10: Emotional Tagging for Critical Failures
    ${CMAKE_CURRENT_SOURCE_DIR}/../cognitive/fault_tolerance/nimcp_self_repair.c  # Self-repair coordinator (code gen + VCS + hot-patch orchestration)
    ${CMAKE_CURRENT_SOURCE_DIR}/../cognitive/fault_tolerance/nimcp_health_diagnostic_bridge.c  # Health monitoring to diagnostic format conversion
    ${CMAKE_CURRENT_SOURCE_DIR}/../cognitive/fault_tolerance/nimcp_health_self_repair_bridge.c  # Health to self-repair automation bridge
    ${CMAKE_CURRENT_SOURCE_DIR}/../cognitive/fault_tolerance/nimcp_self_repair_health_notify.c  # Self-repair health agent notification extension
    # NOTE: No centralized hub - coordination emerges from distributed event bus (mimics thalamus + white matter tracts)
    ${CMAKE_CURRENT_SOURCE_DIR}/../cognitive/nimcp_emotional_tagging.c  # Phase 10.3: General Emotional Tagging (Russell's circumplex model)
    ${CMAKE_CURRENT_SOURCE_DIR}/../cognitive/emotions/nimcp_emotional_system.c  # Phase 10.2: Integrated Emotional System
    ${CMAKE_CURRENT_SOURCE_DIR}/../cognitive/emotions/nimcp_emotional_system_sleep_bridge.c  # Emotional system sleep integration
    ${CMAKE_CURRENT_SOURCE_DIR}/../cognitive/emotion_tensor/nimcp_emotion_tensor.c  # Advanced Concepts: Tensor-Based Emotional Representation
    ${CMAKE_CURRENT_SOURCE_DIR}/../cognitive/emotion_tensor/nimcp_emotion_tensor_bridge.c  # Bridge to swarm emotional contagion
    # Emotion-Attention Integration (now with positional encoding support)
    ${CMAKE_CURRENT_SOURCE_DIR}/../cognitive/attention/nimcp_emotion_attention.c  # Emotion-modulated attention system
    ${CMAKE_CURRENT_SOURCE_DIR}/../cognitive/consolidation/nimcp_emotion_consolidation.c  # Emotion-modulated consolidation
    ${CMAKE_CURRENT_SOURCE_DIR}/../cognitive/executive/nimcp_executive.c  # Phase 10.3: Executive Functions
    ${CMAKE_CURRENT_SOURCE_DIR}/../cognitive/executive/nimcp_executive_snn_bridge.c  # Executive-SNN integration
    ${CMAKE_CURRENT_SOURCE_DIR}/../cognitive/executive/nimcp_executive_plasticity_bridge.c  # Executive-Plasticity integration
    ${CMAKE_CURRENT_SOURCE_DIR}/../cognitive/sleep_wake/nimcp_sleep_wake.c  # Phase 10.4: Sleep/Wake Cycle
    ${CMAKE_CURRENT_SOURCE_DIR}/../cognitive/nimcp_trauma_resilience.c  # Trauma Resilience: recall dampening + arousal homeostasis
    ${CMAKE_CURRENT_SOURCE_DIR}/../cognitive/mental_health/nimcp_mental_health.c  # Phase 10.5: Mental Health Monitoring
    ${CMAKE_CURRENT_SOURCE_DIR}/../cognitive/mental_health/nimcp_mental_health_guardian.c  # Phase 10.5.1: Mental Health Guardian (independent monitor)
    ${CMAKE_CURRENT_SOURCE_DIR}/../cognitive/emotion_recognition/nimcp_emotion_recognition_simple.c  # Phase 11: Part I.1: Emotion Recognition (Simple)
    ${CMAKE_CURRENT_SOURCE_DIR}/../cognitive/emotion_recognition/nimcp_emotion_intensity_continuous.c  # Continuous intensity effects
    ${CMAKE_CURRENT_SOURCE_DIR}/../cognitive/empathetic_response/nimcp_empathetic_response.c  # Phase 11: Part I.2: Empathetic Response
    ${CMAKE_CURRENT_SOURCE_DIR}/../cognitive/autobiographical_memory/nimcp_autobiographical_memory.c  # Phase 12: Autobiographical Memory
    ${CMAKE_CURRENT_SOURCE_DIR}/../cognitive/self_model/nimcp_self_model.c  # Phase 12: Explicit Self-Model
    ${CMAKE_CURRENT_SOURCE_DIR}/../cognitive/self_model/nimcp_self_model_snn_bridge.c  # Self-model-SNN integration
    ${CMAKE_CURRENT_SOURCE_DIR}/../cognitive/self_model/nimcp_self_model_plasticity_bridge.c  # Self-model-Plasticity integration
    ${CMAKE_CURRENT_SOURCE_DIR}/../cognitive/self_model/nimcp_self_model_sleep_bridge.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../cognitive/personality/nimcp_personality.c  # Phase 12: Personality, Gender, and Sexual Identity
    ${CMAKE_CURRENT_SOURCE_DIR}/../cognitive/grief/nimcp_grief_and_loss.c  # Phase E1: Grief, Loss, and Death Understanding
    ${CMAKE_CURRENT_SOURCE_DIR}/../cognitive/joy/nimcp_joy_euphoria.c  # Phase E2: Joy, Euphoria, and Value-Aligned Success
    ${CMAKE_CURRENT_SOURCE_DIR}/../cognitive/remorse/nimcp_remorse_regret.c  # Phase E3: Remorse, Regret, and Evaluative Emotions
    ${CMAKE_CURRENT_SOURCE_DIR}/../cognitive/social/nimcp_love_loyalty_friendship.c  # Phase E4: Love, Loyalty, and Friendship
    ${CMAKE_CURRENT_SOURCE_DIR}/../cognitive/social/nimcp_social_interaction.c  # Multi-agent social interaction framework (single brain, multiple personas)
    ${CMAKE_CURRENT_SOURCE_DIR}/../cognitive/shadow/nimcp_shadow_emotions.c  # Phase E5: Shadow Emotions (Jealousy, Envy, Obsession, Hubris, Greed, Narcissism)
    ${CMAKE_CURRENT_SOURCE_DIR}/../cognitive/bias/nimcp_bias_detection.c  # Phase E6: Bias Detection (Racial, LGBTQ+, Gender, Misogyny, Age, Disability, Religious, Socioeconomic)
    ${CMAKE_CURRENT_SOURCE_DIR}/../cognitive/self_awareness/nimcp_self_awareness_extended.c  # Phase 12: Advanced Self-Awareness (metacognition, agency, safety)
    ${CMAKE_CURRENT_SOURCE_DIR}/../cognitive/theory_of_mind/nimcp_theory_of_mind.c  # Phase 10.6: Theory of Mind
    ${CMAKE_CURRENT_SOURCE_DIR}/../cognitive/theory_of_mind/nimcp_tom_snn_bridge.c  # Theory of Mind-SNN integration
    ${CMAKE_CURRENT_SOURCE_DIR}/../cognitive/theory_of_mind/nimcp_tom_plasticity_bridge.c  # Theory of Mind-Plasticity integration
    ${CMAKE_CURRENT_SOURCE_DIR}/../cognitive/theory_of_mind/nimcp_theory_of_mind_sleep_bridge.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../cognitive/theory_of_mind/nimcp_theory_of_mind_thalamic_bridge.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../cognitive/theory_of_mind/nimcp_theory_of_mind_substrate_bridge.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../cognitive/nimcp_self_awareness_coordinator.c  # Self-Awareness Unification: Orchestrates introspection, self-model, autobio, ToM
    ${CMAKE_CURRENT_SOURCE_DIR}/../cognitive/nimcp_self_awareness_feedback.c    # Self-Awareness Feedback: Policies, history, analysis
    ${CMAKE_CURRENT_SOURCE_DIR}/../cognitive/integration/nimcp_cognitive_integration_hub.c  # Cognitive Integration Hub: Module registration, event pub/sub, queries
    ${CMAKE_CURRENT_SOURCE_DIR}/../cognitive/integration/nimcp_curiosity_reasoning_bridge.c  # Curiosity-Reasoning bidirectional integration
    ${CMAKE_CURRENT_SOURCE_DIR}/../cognitive/integration/nimcp_emotion_memory_bridge.c  # Emotion-Memory bidirectional integration (amygdala-hippocampus)
    ${CMAKE_CURRENT_SOURCE_DIR}/../cognitive/integration/nimcp_attention_wm_bridge.c  # Attention-Working Memory integration bridge
    ${CMAKE_CURRENT_SOURCE_DIR}/../cognitive/integration/nimcp_emotion_executive_bridge.c  # Emotion-Executive Function integration
    ${CMAKE_CURRENT_SOURCE_DIR}/../cognitive/integration/nimcp_ethics_executive_bridge.c  # Ethics-Executive integration (ethical constraints, veto)
    ${CMAKE_CURRENT_SOURCE_DIR}/../cognitive/integration/nimcp_tom_social_bridge.c  # Theory of Mind-Social integration
    ${CMAKE_CURRENT_SOURCE_DIR}/../cognitive/integration/nimcp_gw_cognitive_bridge.c  # Global Workspace-Cognitive broadcast/competition bridge
    ${CMAKE_CURRENT_SOURCE_DIR}/../cognitive/integration/nimcp_self_introspection_bridge.c  # Self-Model-Introspection integration
    ${CMAKE_CURRENT_SOURCE_DIR}/../cognitive/integration/nimcp_cognitive_integration_fep.c  # Cognitive Integration-FEP orchestrator bridge
    ${CMAKE_CURRENT_SOURCE_DIR}/../cognitive/integration/nimcp_collective_hub_bridge.c  # Collective Cognition-Hub integration bridge
    ${CMAKE_CURRENT_SOURCE_DIR}/../cognitive/integration/nimcp_rcog_hub_bridge.c  # Recursive Cognition-Hub integration bridge
    ${CMAKE_CURRENT_SOURCE_DIR}/../cognitive/integration/nimcp_game_theory_executive_bridge.c  # Game Theory-Executive Hub integration bridge
    ${CMAKE_CURRENT_SOURCE_DIR}/../cognitive/integration/nimcp_mirror_empathy_bridge.c  # Mirror Neurons-Empathetic Response integration bridge
    ${CMAKE_CURRENT_SOURCE_DIR}/../cognitive/integration/nimcp_imagination_reasoning_bridge.c  # Imagination-Reasoning Hub integration bridge
    ${CMAKE_CURRENT_SOURCE_DIR}/../cognitive/integration/nimcp_salience_attention_bridge.c  # Salience-Attention Hub integration bridge
    ${CMAKE_CURRENT_SOURCE_DIR}/../cognitive/integration/nimcp_salience_attention_fep_bridge.c  # Salience-Attention FEP integration bridge
    ${CMAKE_CURRENT_SOURCE_DIR}/../cognitive/integration/nimcp_mirror_empathy_fep_bridge.c  # Mirror-Empathy FEP orchestrator bridge
    ${CMAKE_CURRENT_SOURCE_DIR}/../cognitive/integration/nimcp_game_theory_executive_fep_bridge.c  # Game Theory-Executive FEP orchestrator bridge
    ${CMAKE_CURRENT_SOURCE_DIR}/../cognitive/integration/nimcp_predictive_attention_bridge.c  # Predictive-Attention Hub integration bridge
    ${CMAKE_CURRENT_SOURCE_DIR}/../cognitive/integration/nimcp_predictive_attention_fep_bridge.c  # Predictive-Attention FEP orchestrator bridge
    ${CMAKE_CURRENT_SOURCE_DIR}/../cognitive/integration/nimcp_imagination_reasoning_fep_bridge.c  # Imagination-Reasoning FEP orchestrator bridge
    ${CMAKE_CURRENT_SOURCE_DIR}/../cognitive/integration/nimcp_security_cognitive_hub_bridge.c  # Security-Cognitive Hub bridge (cross-domain security-cognitive coordination)
    ${CMAKE_CURRENT_SOURCE_DIR}/../cognitive/integration/nimcp_cognitive_bio_async_bridge.c  # Cognitive-BioAsync integration bridge
    ${CMAKE_CURRENT_SOURCE_DIR}/../cognitive/explanations/nimcp_explanations.c  # Phase 10.7: Natural Explanations
    ${CMAKE_CURRENT_SOURCE_DIR}/../cognitive/meta_learning/nimcp_meta_learning.c  # Phase 10.8: Meta-Learning
    ${CMAKE_CURRENT_SOURCE_DIR}/../cognitive/nimcp_analogical_transfer.c  # Analogical Transfer Engine
    ${CMAKE_CURRENT_SOURCE_DIR}/../cognitive/nimcp_multiscale_memory.c    # Multi-Timescale Memory Buffer

    # Cognitive Reasoning & Logic Integration (Phase 10.x: Logical Reasoning)
    ${CMAKE_CURRENT_SOURCE_DIR}/../cognitive/reasoning/nimcp_symbolic_logic_attachment.c  # Symbolic logic attachment
    ${CMAKE_CURRENT_SOURCE_DIR}/../cognitive/reasoning/nimcp_symbolic_logic_brain_integration.c  # Brain integration API
    ${CMAKE_CURRENT_SOURCE_DIR}/../cognitive/reasoning/nimcp_knowledge_base_interface.c  # KB interface
    ${CMAKE_CURRENT_SOURCE_DIR}/../cognitive/reasoning/nimcp_forward_chaining.c  # Forward chaining (induction)
    ${CMAKE_CURRENT_SOURCE_DIR}/../cognitive/reasoning/nimcp_backward_chaining.c  # Backward chaining (deduction)
    ${CMAKE_CURRENT_SOURCE_DIR}/../cognitive/reasoning/nimcp_unification_engine.c  # Unification
    ${CMAKE_CURRENT_SOURCE_DIR}/../cognitive/reasoning/nimcp_reasoning_factory.c  # Factory
    ${CMAKE_CURRENT_SOURCE_DIR}/../cognitive/reasoning/nimcp_reasoning_integration.c  # Reasoning integration
    ${CMAKE_CURRENT_SOURCE_DIR}/../cognitive/reasoning/integration/nimcp_reasoning_attention.c  # Attention integration
    ${CMAKE_CURRENT_SOURCE_DIR}/../cognitive/reasoning/integration/nimcp_reasoning_curiosity.c  # Curiosity integration
    ${CMAKE_CURRENT_SOURCE_DIR}/../cognitive/reasoning/nimcp_reasoning_snn_bridge.c  # Reasoning-SNN integration
    ${CMAKE_CURRENT_SOURCE_DIR}/../cognitive/reasoning/nimcp_reasoning_plasticity_bridge.c  # Reasoning-Plasticity integration
    ${CMAKE_CURRENT_SOURCE_DIR}/../cognitive/reasoning/nimcp_reasoning_chain.c  # Multi-step reasoning chain engine
    ${CMAKE_CURRENT_SOURCE_DIR}/../cognitive/reasoning/nimcp_reasoning_portia_bridge.c  # Portia resource-aware reasoning budget
    ${CMAKE_CURRENT_SOURCE_DIR}/../cognitive/reasoning/nimcp_reasoning_hypo_bridge.c  # Hypothalamus motivational reasoning modulation
    ${CMAKE_CURRENT_SOURCE_DIR}/../cognitive/reasoning/nimcp_reasoning_mesh_bridge.c  # Distributed mesh consensus for reasoning
    ${CMAKE_CURRENT_SOURCE_DIR}/../cognitive/reasoning/nimcp_reasoning_convergent.c  # Convergent evidence accumulation architecture
    ${CMAKE_CURRENT_SOURCE_DIR}/../cognitive/reasoning/nimcp_reasoning_kb_persistence.c  # KB persistence (export/import)
    ${CMAKE_CURRENT_SOURCE_DIR}/../cognitive/reasoning/nimcp_reasoning_affective.c  # Affective emotion-based confidence modulation
    ${CMAKE_CURRENT_SOURCE_DIR}/../cognitive/reasoning/nimcp_reasoning_calibration.c  # Confidence calibration learning for contributors
    ${CMAKE_CURRENT_SOURCE_DIR}/../cognitive/reasoning/nimcp_reasoning_metacognition.c  # Metacognitive adaptive strategy selection
    ${CMAKE_CURRENT_SOURCE_DIR}/../cognitive/reasoning/nimcp_reasoning_abduction.c  # Abductive reasoning (inference to best explanation)
    ${CMAKE_CURRENT_SOURCE_DIR}/../cognitive/reasoning/nimcp_reasoning_causal.c  # Causal DAG inference with do-calculus
    ${CMAKE_CURRENT_SOURCE_DIR}/../cognitive/reasoning/nimcp_reasoning_visuospatial.c  # Visuospatial reasoning (spatial objects, relations, queries)

    # Cognitive Training Integration Layer
    ${CMAKE_CURRENT_SOURCE_DIR}/../cognitive/training/nimcp_training_integration.c  # Unified training pipeline API

    # Brain Immune System (Coordination Layer for FT/Security Integration)
    ${CMAKE_CURRENT_SOURCE_DIR}/../cognitive/immune/nimcp_brain_immune.c  # B/T cells, antibodies, cytokines coordination
    ${CMAKE_CURRENT_SOURCE_DIR}/../cognitive/immune/nimcp_brain_immune_tick.c  # Brain immune tick processing
    ${CMAKE_CURRENT_SOURCE_DIR}/../cognitive/immune/nimcp_brain_immune_integration.c  # Full exception-immune integration helper
    ${CMAKE_CURRENT_SOURCE_DIR}/../cognitive/immune/nimcp_code_immune.c  # Code crash defense and hot-patching
    ${CMAKE_CURRENT_SOURCE_DIR}/../cognitive/immune/nimcp_brain_immune_plasticity.c  # Immune-plasticity integration
    ${CMAKE_CURRENT_SOURCE_DIR}/../cognitive/immune/nimcp_perception_immune.c  # Perception-immune integration
    ${CMAKE_CURRENT_SOURCE_DIR}/../cognitive/immune/nimcp_memory_immune_integration.c  # Memory-immune integration
    ${CMAKE_CURRENT_SOURCE_DIR}/../cognitive/immune/nimcp_emotion_immune_bridge.c  # Emotion-immune bidirectional integration
    ${CMAKE_CURRENT_SOURCE_DIR}/../cognitive/immune/nimcp_reasoning_immune.c  # Reasoning-immune bidirectional integration
    ${CMAKE_CURRENT_SOURCE_DIR}/../cognitive/immune/nimcp_attention_immune_bridge.c  # Attention-immune bidirectional integration
    ${CMAKE_CURRENT_SOURCE_DIR}/../cognitive/immune/nimcp_curiosity_immune_bridge.c  # Curiosity-immune bidirectional integration (sickness behavior)
    ${CMAKE_CURRENT_SOURCE_DIR}/../cognitive/immune/nimcp_autobiographical_immune_bridge.c  # Autobiographical memory-immune integration
    ${CMAKE_CURRENT_SOURCE_DIR}/../cognitive/immune/nimcp_executive_immune_bridge.c  # Executive function-immune integration
    ${CMAKE_CURRENT_SOURCE_DIR}/../cognitive/immune/nimcp_introspection_immune_bridge.c  # Introspection-immune integration
    ${CMAKE_CURRENT_SOURCE_DIR}/../cognitive/immune/nimcp_knowledge_immune_bridge.c  # Knowledge base-immune integration
    ${CMAKE_CURRENT_SOURCE_DIR}/../cognitive/immune/nimcp_mental_health_immune_bridge.c  # Mental health-immune integration
    ${CMAKE_CURRENT_SOURCE_DIR}/../cognitive/immune/nimcp_self_model_immune_bridge.c  # Self-model-immune integration
    ${CMAKE_CURRENT_SOURCE_DIR}/../cognitive/immune/nimcp_sleep_immune_bridge.c  # Sleep-wake-immune integration
    ${CMAKE_CURRENT_SOURCE_DIR}/../cognitive/immune/nimcp_tom_immune_bridge.c  # Theory of mind-immune integration
    ${CMAKE_CURRENT_SOURCE_DIR}/../cognitive/immune/nimcp_wellbeing_immune_bridge.c  # Wellbeing-immune integration
    # New Immune Enhancement Modules (Phase 11.x)
    ${CMAKE_CURRENT_SOURCE_DIR}/../cognitive/immune/nimcp_trained_immunity.c  # Trained (innate) immunity with epigenetic reprogramming
    ${CMAKE_CURRENT_SOURCE_DIR}/../cognitive/immune/nimcp_complement_system.c  # Complement cascade system
    ${CMAKE_CURRENT_SOURCE_DIR}/../cognitive/immune/nimcp_regulatory_tcells.c  # Regulatory T cells (Treg) - cytokine storm prevention
    ${CMAKE_CURRENT_SOURCE_DIR}/../cognitive/immune/nimcp_immune_exhaustion.c  # T cell exhaustion tracking and recovery
    ${CMAKE_CURRENT_SOURCE_DIR}/../cognitive/immune/nimcp_immune_tolerance.c  # Self vs non-self tolerance
    ${CMAKE_CURRENT_SOURCE_DIR}/../cognitive/immune/nimcp_immune_vaccine.c  # Vaccine pre-training system
    ${CMAKE_CURRENT_SOURCE_DIR}/../cognitive/immune/nimcp_mucosal_immunity.c  # Mucosal barrier immunity
    ${CMAKE_CURRENT_SOURCE_DIR}/../cognitive/immune/nimcp_immune_persistence.c  # Immune memory persistence
    ${CMAKE_CURRENT_SOURCE_DIR}/../cognitive/immune/nimcp_brain_immune_fep_bridge.c  # Brain immune-FEP bidirectional integration
    ${CMAKE_CURRENT_SOURCE_DIR}/../cognitive/ethics/nimcp_ethics_immune.c  # Ethics-immune integration
    ${CMAKE_CURRENT_SOURCE_DIR}/../cognitive/global_workspace/nimcp_global_workspace_immune.c  # Global workspace-immune integration
    ${CMAKE_CURRENT_SOURCE_DIR}/../cognitive/immune/nimcp_immune_bridge_coordinator.c  # Immune Bridge Coordinator - central registry for all immune bridges
    ${CMAKE_CURRENT_SOURCE_DIR}/../cognitive/immune/nimcp_self_heal.c  # Self-Healing Engine - pattern + LNN based crash recovery
    ${CMAKE_CURRENT_SOURCE_DIR}/../cognitive/immune/nimcp_heal_bridge.c  # Enhanced Self-Healing Bridge - unified pipeline with sandbox/evolution/chains
    ${CMAKE_CURRENT_SOURCE_DIR}/../cognitive/immune/nimcp_claude_healer.c  # Claude API Fallback Healer - external AI-assisted code repair
    ${CMAKE_CURRENT_SOURCE_DIR}/../cognitive/immune/nimcp_immune_metrics.c  # Immune Metrics - Prometheus/JSON export for self-healing observability
    ${CMAKE_CURRENT_SOURCE_DIR}/../cognitive/immune/nimcp_code_immune_self_repair.c  # Code immune to self-repair pipeline integration
    ${CMAKE_CURRENT_SOURCE_DIR}/../cognitive/nimcp_cognitive_meta_controller.c  # Cognitive Meta-Controller - unified arbitrator for cognitive subsystems
    ${CMAKE_CURRENT_SOURCE_DIR}/../cognitive/predictive_immune/nimcp_predictive_immune.c  # Predictive-immune bidirectional integration — re-enabled 2026-04-24 (brain_region_predictive also re-enabled)
    ${CMAKE_CURRENT_SOURCE_DIR}/../middleware/immune/nimcp_pattern_immune.c  # Pattern-immune bidirectional integration
    ${CMAKE_CURRENT_SOURCE_DIR}/../middleware/immune/nimcp_feature_extractor_immune_bridge.c  # Feature extractor-immune integration
    ${CMAKE_CURRENT_SOURCE_DIR}/../middleware/immune/nimcp_population_coding_immune_bridge.c  # Population coding-immune integration
    ${CMAKE_CURRENT_SOURCE_DIR}/../middleware/immune/nimcp_sequence_immune_bridge.c  # Sequence detector-immune integration
    ${CMAKE_CURRENT_SOURCE_DIR}/../middleware/immune/nimcp_thalamic_immune_bridge.c  # Thalamic router-immune integration
    ${CMAKE_CURRENT_SOURCE_DIR}/../middleware/immune/nimcp_training_immune.c  # Training-immune integration
    ${CMAKE_CURRENT_SOURCE_DIR}/../middleware/normalization/nimcp_normalization_immune.c  # Normalization-immune integration

    # Core Logic Modules (Phase 10.x: Neural Logic Gates)
    ${CMAKE_CURRENT_SOURCE_DIR}/../core/logic/nimcp_neural_logic_attachment.c  # Neural logic attachment
    ${CMAKE_CURRENT_SOURCE_DIR}/../core/logic/nimcp_neural_logic_evaluation.c  # Logic gate evaluation
    ${CMAKE_CURRENT_SOURCE_DIR}/../core/logic/nimcp_neural_logic_circuit_builder.c  # Circuit builder
    ${CMAKE_CURRENT_SOURCE_DIR}/../core/logic/nimcp_neural_logic_neuromodulation.c  # Neuromodulation
    ${CMAKE_CURRENT_SOURCE_DIR}/../core/logic/nimcp_neural_logic_factory.c  # Factory
    ${CMAKE_CURRENT_SOURCE_DIR}/../core/logic/nimcp_neural_logic_quantum_bridge.c  # Quantum acceleration bridge
    ${CMAKE_CURRENT_SOURCE_DIR}/../cognitive/predictive/nimcp_predictive.c  # Phase 10.9: Predictive Processing
    ${CMAKE_CURRENT_SOURCE_DIR}/../cognitive/mirror_neurons/nimcp_mirror_neurons.c  # Phase 10.11: Mirror Neurons
    ${CMAKE_CURRENT_SOURCE_DIR}/../cognitive/mirror_neurons/nimcp_mirror_substrate.c  # Phase 10.11.2: Mirror Neuron Substrate Integration
    ${CMAKE_CURRENT_SOURCE_DIR}/../cognitive/mirror_neurons/nimcp_mirror_stdp.c  # Phase 10.11.4: STDP Learning
    ${CMAKE_CURRENT_SOURCE_DIR}/../cognitive/mirror_neurons/nimcp_mirror_resonance.c  # Phase 10.11.5: Motor Resonance
    ${CMAKE_CURRENT_SOURCE_DIR}/../cognitive/mirror_neurons/nimcp_mirror_hierarchy.c  # Phase 10.11.6: Hierarchical Goals
    ${CMAKE_CURRENT_SOURCE_DIR}/../cognitive/mirror_neurons/nimcp_mirror_immune_integration.c  # Phase 10.11.7: Mirror-Immune Integration
    ${CMAKE_CURRENT_SOURCE_DIR}/../cognitive/mirror_neurons/nimcp_mirror_language_bridge.c  # Phase ML1: Mirror-Language Integration
    ${CMAKE_CURRENT_SOURCE_DIR}/../cognitive/global_workspace/nimcp_global_workspace.c  # Global Workspace Architecture (Global Workspace Theory)
    ${CMAKE_CURRENT_SOURCE_DIR}/../cognitive/global_workspace/nimcp_global_workspace_shannon.c  # Phase 1.5.3: Shannon Information-Weighted Competition
    ${CMAKE_CURRENT_SOURCE_DIR}/../cognitive/global_workspace/nimcp_global_workspace_fep_bridge.c  # Global Workspace-FEP bidirectional integration
    ${CMAKE_CURRENT_SOURCE_DIR}/../cognitive/global_workspace/nimcp_gw_snn_bridge.c  # GW-SNN bidirectional integration
    ${CMAKE_CURRENT_SOURCE_DIR}/../cognitive/global_workspace/nimcp_gw_plasticity_bridge.c  # GW-Plasticity bidirectional integration
    ${CMAKE_CURRENT_SOURCE_DIR}/../cognitive/analysis/nimcp_network_analysis.c  # Network topology analysis and community detection
    ${CMAKE_CURRENT_SOURCE_DIR}/../cognitive/memory/nimcp_engram.c  # Phase M1: Memory Engrams (explicit memory traces)
    ${CMAKE_CURRENT_SOURCE_DIR}/../cognitive/memory/nimcp_memory_kg_events.c  # W6: KG event emitters for memory family
    ${CMAKE_CURRENT_SOURCE_DIR}/../cognitive/world_model/nimcp_world_model_kg_events.c  # W8: KG event emitters for world-model / imagination / FEP / predictive / salience family
    ${CMAKE_CURRENT_SOURCE_DIR}/../cognitive/executive/nimcp_w9kg_events.c  # W9-kg: KG helpers for cognitive-control family
    ${CMAKE_CURRENT_SOURCE_DIR}/../cognitive/memory/nimcp_engram_jepa_bridge.c  # Phase 4p: engram ↔ JEPA
    ${CMAKE_CURRENT_SOURCE_DIR}/../cognitive/memory/nimcp_consolidation_jepa_bridge.c  # Phase 4t: consolidation ↔ JEPA
    ${CMAKE_CURRENT_SOURCE_DIR}/../cognitive/jepa/nimcp_jepa_brain_bridges.c  # Phase 4n-q: brain-level JEPA bridge orchestrator
    ${CMAKE_CURRENT_SOURCE_DIR}/../core/brain/nimcp_brain_tick_bio_async.c  # Round A: 7 brain-region adapters tick
    ${CMAKE_CURRENT_SOURCE_DIR}/../core/brain/nimcp_brain_tick_language.c  # Wave 8B-d: broca + wernicke tick
    ${CMAKE_CURRENT_SOURCE_DIR}/../core/brain/nimcp_brain_tick_neuromod.c  # Wave 8B-a: 5 neuromodulatory nuclei tick
    ${CMAKE_CURRENT_SOURCE_DIR}/../core/brain/nimcp_brain_tick_sensorimotor.c  # Wave 8B-b: motor/olfact/gust/soma/amygdala/cingulate tick
    ${CMAKE_CURRENT_SOURCE_DIR}/../core/brain/nimcp_brain_tick_physics_bridges.c  # Wave 8C: ephaptic/hh/thermo bridge tick driver
    ${CMAKE_CURRENT_SOURCE_DIR}/../core/brain/nimcp_brain_tick_biology.c         # Wave 3: epigenetics + neurogenesis + NVC hot-path tick
    ${CMAKE_CURRENT_SOURCE_DIR}/../core/brain/nimcp_brain_tick_predictive_immune.c  # Wave 4: predictive-immune coupling hot-path tick
    ${CMAKE_CURRENT_SOURCE_DIR}/../core/brain/nimcp_brain_tick_meta_learning.c    # Wave 4: meta_adapt_learning_rate tick wrapper
    ${CMAKE_CURRENT_SOURCE_DIR}/../core/brain/nimcp_brain_tick_intuitive_physics.c  # Wave 5: intuitive_physics_step tick wrapper
    ${CMAKE_CURRENT_SOURCE_DIR}/../core/brain/nimcp_brain_tick_chemistry.c        # Wave 6: protons + buffers + pH + NO hot-path tick
    ${CMAKE_CURRENT_SOURCE_DIR}/../core/brain/nimcp_brain_tick_hypothalamus.c     # Wave 8B-c: hypothalamus drives/homeostasis hot-path tick
    ${CMAKE_CURRENT_SOURCE_DIR}/../core/brain/nimcp_brain_tick_entorhinal.c       # Wave 8B-c: entorhinal MTL hot-path tick (ACTIVE — minimal adapter)
    ${CMAKE_CURRENT_SOURCE_DIR}/../core/brain/nimcp_brain_tick_cerebellum.c       # Wave 8B-c: cerebellum bio-msg drain hot-path tick
    ${CMAKE_CURRENT_SOURCE_DIR}/../core/brain/nimcp_brain_tick_basal_ganglia.c    # Wave 8B-c: basal_ganglia action selection hot-path tick
    ${CMAKE_CURRENT_SOURCE_DIR}/../core/brain/factory/init/nimcp_brain_init_perception_bridges.c  # Round A/2: activate 10 orphan bridges
    ${CMAKE_CURRENT_SOURCE_DIR}/../cognitive/memory/nimcp_systems_consolidation.c  # Phase M2: Systems Consolidation (hippocampus → cortex transfer)
    ${CMAKE_CURRENT_SOURCE_DIR}/../cognitive/memory/nimcp_wm_transfer.c  # Phase M3: Working Memory Transfer (WM → engram encoding)
    ${CMAKE_CURRENT_SOURCE_DIR}/../cognitive/memory/nimcp_semantic_memory.c  # Phase M4: Semantic Memory Network (concept network with spreading activation)

    # Prime Resonant Memory Core (Phase PR: Content-addressable memory with prime signatures)
    ${CMAKE_CURRENT_SOURCE_DIR}/../cognitive/memory/core/nimcp_quaternion.c  # Phase PR-1.1: Quaternion math for memory state encoding
    ${CMAKE_CURRENT_SOURCE_DIR}/../cognitive/memory/core/nimcp_prime_signature.c  # Phase PR-2.1: Prime factorization signatures for content-addressable memory
    ${CMAKE_CURRENT_SOURCE_DIR}/../cognitive/memory/core/nimcp_fractal.c  # Phase PR-1.3: Fractal analysis for memory dynamics
    ${CMAKE_CURRENT_SOURCE_DIR}/../cognitive/memory/core/nimcp_pr_pink_noise.c  # Phase PR-1.2: Pink noise extension for memory
    ${CMAKE_CURRENT_SOURCE_DIR}/../cognitive/memory/core/nimcp_resonance.c  # Phase PR-2.2: Resonance scoring engine (4-component similarity)
    ${CMAKE_CURRENT_SOURCE_DIR}/../cognitive/memory/core/nimcp_pr_memory_node.c  # Phase PR-3.1: COW-enabled memory nodes with prime signatures and quaternion states
    ${CMAKE_CURRENT_SOURCE_DIR}/../cognitive/memory/core/nimcp_pr_plasticity_bridge.c  # Phase PR-6.1: Plasticity bridge (STDP, BCM, homeostatic)
    ${CMAKE_CURRENT_SOURCE_DIR}/../cognitive/memory/core/nimcp_pr_snn_bridge.c  # Phase PR-6.2: SNN bridge (spike integration)
    ${CMAKE_CURRENT_SOURCE_DIR}/../cognitive/memory/core/nimcp_pr_kg_bridge.c  # Phase PR-6.3: Knowledge graph bridge
    ${CMAKE_CURRENT_SOURCE_DIR}/../cognitive/memory/core/nimcp_pr_curriculum_bridge.c  # Phase PR-6.4: Curriculum learning bridge (resonance-guided curriculum)
    ${CMAKE_CURRENT_SOURCE_DIR}/../cognitive/memory/core/nimcp_entanglement.c  # Phase PR-4.1: Entanglement graph for memory associations
    ${CMAKE_CURRENT_SOURCE_DIR}/../cognitive/memory/core/nimcp_z_ladder.c  # Phase PR-5.1: Z-ladder for memory tier management
    ${CMAKE_CURRENT_SOURCE_DIR}/../cognitive/memory/core/nimcp_kuramoto.c  # Phase PR-4.2: Kuramoto oscillator coupling
    ${CMAKE_CURRENT_SOURCE_DIR}/../cognitive/memory/core/nimcp_source_memory.c  # Phase PR-7.1: Source memory for origin tracking and reality monitoring
    ${CMAKE_CURRENT_SOURCE_DIR}/../cognitive/memory/core/nimcp_theta_gamma.c  # Phase PR-4.3: Theta-gamma phase coupling
    ${CMAKE_CURRENT_SOURCE_DIR}/../cognitive/memory/core/nimcp_pr_pink_noise_bridge.c  # Phase PR-6.5: Pink noise integration bridge
    # Phase 6: Training Layer Integration
    ${CMAKE_CURRENT_SOURCE_DIR}/../cognitive/memory/core/nimcp_pr_loss_bridge.c  # Phase PR-6.1: Loss bridge
    ${CMAKE_CURRENT_SOURCE_DIR}/../cognitive/memory/core/nimcp_pr_optimizer_bridge.c  # Phase PR-6.2: Optimizer bridge
    ${CMAKE_CURRENT_SOURCE_DIR}/../cognitive/memory/core/nimcp_pr_meta_bridge.c  # Phase PR-6.3: Meta-learning bridge
    ${CMAKE_CURRENT_SOURCE_DIR}/../cognitive/memory/core/nimcp_pr_continual_bridge.c  # Phase PR-6.5: Continual learning bridge
    ${CMAKE_CURRENT_SOURCE_DIR}/../cognitive/memory/core/nimcp_pr_training_plasticity.c  # Phase PR-6.6: Training-plasticity extension
    # Phase 7: Perception Layer Integration
    ${CMAKE_CURRENT_SOURCE_DIR}/../cognitive/memory/core/nimcp_pr_visual_bridge.c  # Phase PR-7.1: Visual perception bridge
    ${CMAKE_CURRENT_SOURCE_DIR}/../cognitive/memory/core/nimcp_pr_audio_bridge.c  # Phase PR-7.2: Audio perception bridge
    ${CMAKE_CURRENT_SOURCE_DIR}/../cognitive/memory/core/nimcp_pr_speech_bridge.c  # Phase PR-7.3: Speech perception bridge
    ${CMAKE_CURRENT_SOURCE_DIR}/../cognitive/memory/core/nimcp_pr_omni_bridge.c  # Phase PR-7.4: Omni-sensory bridge
    ${CMAKE_CURRENT_SOURCE_DIR}/../cognitive/memory/core/nimcp_pr_attention_bridge.c  # Phase PR-7.5: Attention bridge
    ${CMAKE_CURRENT_SOURCE_DIR}/../cognitive/memory/core/nimcp_pr_predictive_bridge.c  # Phase PR-7.6: Predictive bridge
    # Phase 8: Enhancement Layer Part 1
    ${CMAKE_CURRENT_SOURCE_DIR}/../cognitive/memory/core/nimcp_metamemory.c  # Phase PR-8.1: Metamemory (FOK, TOT, JOL)
    ${CMAKE_CURRENT_SOURCE_DIR}/../cognitive/memory/core/nimcp_metamemory_monitor.c  # Phase PR-8.2: Metamemory monitoring
    ${CMAKE_CURRENT_SOURCE_DIR}/../cognitive/memory/core/nimcp_prospective.c  # Phase PR-8.3: Prospective memory
    ${CMAKE_CURRENT_SOURCE_DIR}/../cognitive/memory/core/nimcp_prospective_scheduler.c  # Phase PR-8.4: Prospective scheduler
    ${CMAKE_CURRENT_SOURCE_DIR}/../cognitive/memory/core/nimcp_reconsolidation.c  # Phase PR-8.5: Reconsolidation
    # Phase 9: Enhancement Layer Part 2
    ${CMAKE_CURRENT_SOURCE_DIR}/../cognitive/memory/core/nimcp_procedural.c  # Phase PR-9.1: Procedural memory
    ${CMAKE_CURRENT_SOURCE_DIR}/../cognitive/memory/core/nimcp_skill_acquisition.c  # Phase PR-9.2: Skill acquisition
    ${CMAKE_CURRENT_SOURCE_DIR}/../cognitive/memory/core/nimcp_future_thinking.c  # Phase PR-9.3: Episodic future thinking
    ${CMAKE_CURRENT_SOURCE_DIR}/../cognitive/memory/core/nimcp_counterfactual.c  # Phase PR-9.4: Counterfactual reasoning
    # Phase 10: Enhancement Layer Part 3
    ${CMAKE_CURRENT_SOURCE_DIR}/../cognitive/memory/core/nimcp_social_memory.c  # Phase PR-10.1: Social memory
    ${CMAKE_CURRENT_SOURCE_DIR}/../cognitive/memory/core/nimcp_collective_memory.c  # Phase PR-10.2: Collective memory
    ${CMAKE_CURRENT_SOURCE_DIR}/../cognitive/memory/core/nimcp_transactive.c  # Phase PR-10.3: Transactive memory
    ${CMAKE_CURRENT_SOURCE_DIR}/../cognitive/memory/core/nimcp_schemas.c  # Phase PR-10.4: Schema system
    ${CMAKE_CURRENT_SOURCE_DIR}/../cognitive/memory/core/nimcp_gist.c  # Phase PR-10.5: Gist extraction
    # Phase 11: Enhancement Layer Part 4
    ${CMAKE_CURRENT_SOURCE_DIR}/../cognitive/memory/core/nimcp_spaced_repetition.c  # Phase PR-11.1: Spaced repetition
    ${CMAKE_CURRENT_SOURCE_DIR}/../cognitive/memory/core/nimcp_flashbulb.c  # Phase PR-11.2: Flashbulb memory
    # Phase 12: Module Bridges
    ${CMAKE_CURRENT_SOURCE_DIR}/../cognitive/memory/core/nimcp_pr_bio_bridge.c  # Phase PR-12.1: Bio-async bridge
    ${CMAKE_CURRENT_SOURCE_DIR}/../cognitive/memory/core/nimcp_pr_immune_bridge.c  # Phase PR-12.2: Immune bridge
    ${CMAKE_CURRENT_SOURCE_DIR}/../cognitive/memory/core/nimcp_pr_cognitive_bridge.c  # Phase PR-12.4: Cognitive hub bridge
    ${CMAKE_CURRENT_SOURCE_DIR}/../cognitive/memory/core/nimcp_pr_hypo_bridge.c  # Phase PR-12.5: Hypothalamus bridge
    ${CMAKE_CURRENT_SOURCE_DIR}/../cognitive/memory/core/nimcp_pr_cerebellum_bridge.c  # Phase PR-12.7: Cerebellum bridge
    ${CMAKE_CURRENT_SOURCE_DIR}/../cognitive/memory/core/nimcp_pr_sleep_bridge.c  # Phase PR-12.8: Sleep-wake bridge
    ${CMAKE_CURRENT_SOURCE_DIR}/../cognitive/memory/core/nimcp_pr_mental_health_bridge.c  # Phase PR-12.9: Mental health bridge
    ${CMAKE_CURRENT_SOURCE_DIR}/../cognitive/memory/core/nimcp_pr_logging_bridge.c  # Phase PR-12.3: Logging bridge

    ${CMAKE_CURRENT_SOURCE_DIR}/../cognitive/nimcp_fractal_cognitive.c  # Fractal cognitive processing
    ${CMAKE_CURRENT_SOURCE_DIR}/cognitive/nimcp_hierarchical.c

    # Recursive Cognition (RLM-inspired: Phase 1 Foundation)
    ${CMAKE_CURRENT_SOURCE_DIR}/../cognitive/recursive/nimcp_rcog_context_store.c  # Context store (environment-as-variable pattern)
    ${CMAKE_CURRENT_SOURCE_DIR}/../cognitive/recursive/nimcp_rcog_answer.c  # Answer refiner (diffusion-style refinement)
    # Recursive Cognition Bridges (bidirectional integration with brain systems)
    ${CMAKE_CURRENT_SOURCE_DIR}/../cognitive/recursive/nimcp_rcog_bio_async_bridge.c  # Bio-async messaging integration
    ${CMAKE_CURRENT_SOURCE_DIR}/../cognitive/recursive/nimcp_rcog_imagination_bridge.c  # Imagination engine integration
    ${CMAKE_CURRENT_SOURCE_DIR}/../cognitive/recursive/nimcp_rcog_immune_bridge.c  # Brain immune system integration
    ${CMAKE_CURRENT_SOURCE_DIR}/../cognitive/recursive/nimcp_rcog_collective_bridge.c  # Swarm/collective workspace integration
    ${CMAKE_CURRENT_SOURCE_DIR}/../cognitive/recursive/nimcp_rcog_brain_kg_bridge.c  # Brain knowledge graph integration
    ${CMAKE_CURRENT_SOURCE_DIR}/../cognitive/recursive/nimcp_rcog_orchestrator.c  # Task decomposition and coordination
    ${CMAKE_CURRENT_SOURCE_DIR}/../cognitive/recursive/nimcp_rcog_delegation_pool.c  # Worker pool for subtask execution
    ${CMAKE_CURRENT_SOURCE_DIR}/../cognitive/recursive/nimcp_rcog_tool_router.c  # Capability-based tool access control
    ${CMAKE_CURRENT_SOURCE_DIR}/../cognitive/recursive/nimcp_rcog_engine.c  # Main processing coordinator

    # Game Theory - Economic mechanisms for cognitive resource allocation
    ${CMAKE_CURRENT_SOURCE_DIR}/../cognitive/game_theory/nimcp_game_theory.c  # Core game theory types and utilities
    ${CMAKE_CURRENT_SOURCE_DIR}/../cognitive/game_theory/nimcp_auction.c  # Auction mechanisms (second-price, VCG, ascending, descending)
    ${CMAKE_CURRENT_SOURCE_DIR}/../cognitive/game_theory/nimcp_gt_auction_ext.c  # Extended auctions (combinatorial, double, multi-unit)
    ${CMAKE_CURRENT_SOURCE_DIR}/../cognitive/game_theory/nimcp_bargaining.c  # Nash bargaining, Kalai-Smorodinsky, Rubinstein
    ${CMAKE_CURRENT_SOURCE_DIR}/../cognitive/game_theory/nimcp_credit_assignment.c  # Shapley value, Banzhaf index, core membership
    ${CMAKE_CURRENT_SOURCE_DIR}/../cognitive/game_theory/nimcp_gt_equilibrium.c  # N-player Nash equilibrium (pure, mixed, Lemke-Howson)
    ${CMAKE_CURRENT_SOURCE_DIR}/../cognitive/game_theory/nimcp_gt_fairness.c  # Enhanced fairness metrics (Jain, Gini, Theil, envy-freeness, MMS)
    ${CMAKE_CURRENT_SOURCE_DIR}/../cognitive/game_theory/nimcp_gt_coalition.c  # Coalition formation dynamics, hedonic games, stability
    ${CMAKE_CURRENT_SOURCE_DIR}/../cognitive/game_theory/nimcp_gt_repeated.c  # Repeated games, folk theorem, trigger strategies
    ${CMAKE_CURRENT_SOURCE_DIR}/../cognitive/game_theory/nimcp_gt_mechanism.c  # Mechanism design, incomplete information, signaling games
    ${CMAKE_CURRENT_SOURCE_DIR}/../cognitive/game_theory/integration/nimcp_gt_global_workspace.c  # Auction-based GW competition
    ${CMAKE_CURRENT_SOURCE_DIR}/../cognitive/game_theory/integration/nimcp_gt_hemispheric.c  # Hemispheric Nash bargaining
    ${CMAKE_CURRENT_SOURCE_DIR}/../cognitive/game_theory/integration/nimcp_gt_neuromod.c  # Payoff-to-neuromodulator mapping
    ${CMAKE_CURRENT_SOURCE_DIR}/../cognitive/game_theory/integration/nimcp_gt_working_memory.c  # Auction-based WM eviction
    ${CMAKE_CURRENT_SOURCE_DIR}/../cognitive/game_theory/integration/nimcp_gt_tom.c  # Theory of Mind - opponent modeling
    ${CMAKE_CURRENT_SOURCE_DIR}/../cognitive/game_theory/nimcp_gt_spatial.c  # Population/spatial games, replicator dynamics, network effects
    ${CMAKE_CURRENT_SOURCE_DIR}/../cognitive/game_theory/nimcp_gt_learning.c  # Q-learning, CFR, fictitious play, opponent modeling
    ${CMAKE_CURRENT_SOURCE_DIR}/../cognitive/game_theory/nimcp_game_theory_fep_bridge.c  # FEP orchestrator integration for game theory

    # Parietal Lobe - Mathematical/Scientific Reasoning (Phase 7.2)
    ${CMAKE_CURRENT_SOURCE_DIR}/../cognitive/parietal/nimcp_number_sense.c           # Approximate Number System (Weber-Fechner, subitizing)
    ${CMAKE_CURRENT_SOURCE_DIR}/../cognitive/parietal/nimcp_spatial_reasoning.c      # Mental rotation, coordinate transforms, K-D tree indexing
    ${CMAKE_CURRENT_SOURCE_DIR}/../cognitive/parietal/nimcp_mathematical_intuition.c # Pattern detection, symmetry, analogies
    ${CMAKE_CURRENT_SOURCE_DIR}/../cognitive/parietal/nimcp_scientific_reasoning.c   # Hypothesis testing, dimensional analysis, causal inference
    ${CMAKE_CURRENT_SOURCE_DIR}/../cognitive/parietal/nimcp_equation_manipulation.c  # Symbolic math, differentiation, expression trees
    ${CMAKE_CURRENT_SOURCE_DIR}/../cognitive/parietal/nimcp_chemistry.c              # Chemistry reasoning, stoichiometry, periodic table
    ${CMAKE_CURRENT_SOURCE_DIR}/../cognitive/parietal/nimcp_biology.c                # Biology reasoning, DNA/RNA, phylogenetics
    ${CMAKE_CURRENT_SOURCE_DIR}/../cognitive/parietal/nimcp_software_engineering.c   # Software engineering reasoning, complexity analysis
    # Engineering domains with quantum acceleration support (Phase 7.2+)
    ${CMAKE_CURRENT_SOURCE_DIR}/../cognitive/parietal/nimcp_electrical_engineering.c  # EE: Circuit analysis, signal processing, control systems
    ${CMAKE_CURRENT_SOURCE_DIR}/../cognitive/parietal/nimcp_mechanical_engineering.c  # ME: Structural mechanics, vibration, thermal, fatigue
    ${CMAKE_CURRENT_SOURCE_DIR}/../cognitive/parietal/nimcp_civil_engineering.c       # CE: Geotechnical, hydraulics, structural optimization
    ${CMAKE_CURRENT_SOURCE_DIR}/../cognitive/parietal/nimcp_parietal_quantum_bridge.c # Parietal quantum bridge API (different from core/brain/regions version)
    ${CMAKE_CURRENT_SOURCE_DIR}/../cognitive/parietal/nimcp_fep_parietal_bridge.c     # FEP-Parietal bridge (predictive processing, active inference)
    ${CMAKE_CURRENT_SOURCE_DIR}/../cognitive/parietal/nimcp_parietal_fep_bridge.c    # Parietal-FEP orchestrator integration bridge
    # Phase 6: Intuitive Reasoning & Discovery Engines
    ${CMAKE_CURRENT_SOURCE_DIR}/../cognitive/parietal/nimcp_intuitive_reasoning.c    # Core intuitive reasoning (hunches, plausibility, leaps)
    ${CMAKE_CURRENT_SOURCE_DIR}/../cognitive/parietal/nimcp_analogical_reasoning.c   # Cross-domain transfer (structure mapping, analogy)
    ${CMAKE_CURRENT_SOURCE_DIR}/../cognitive/parietal/nimcp_insight_discovery.c      # Insight generation (incubation, eureka moments)
    ${CMAKE_CURRENT_SOURCE_DIR}/../cognitive/parietal/nimcp_hypothesis_generation.c  # Theory formation (abduction, prediction)
    ${CMAKE_CURRENT_SOURCE_DIR}/../cognitive/parietal/nimcp_conceptual_blending.c    # Creative combination (mental spaces, emergence)
    ${CMAKE_CURRENT_SOURCE_DIR}/../cognitive/parietal/nimcp_counterfactual.c         # What-if reasoning (causal analysis)
    ${CMAKE_CURRENT_SOURCE_DIR}/../cognitive/parietal/nimcp_meta_reasoning.c         # Reasoning about reasoning (strategy selection)
    ${CMAKE_CURRENT_SOURCE_DIR}/../cognitive/parietal/nimcp_intuition_integrations.c # Phase 6.0: Integration & Extrapolation Framework
    # ${CMAKE_CURRENT_SOURCE_DIR}/../cognitive/parietal/nimcp_physics_nn.c              # Physics-informed neural network (symplectic, HNN) - NOT YET IMPLEMENTED
    # Financial Analysis Module (Phase B - Fuzzy-Integrated)
    ${CMAKE_CURRENT_SOURCE_DIR}/../cognitive/parietal/nimcp_financial_investment.c   # Core investment: portfolio, risk, derivatives, optimization
    ${CMAKE_CURRENT_SOURCE_DIR}/../cognitive/parietal/nimcp_financial_market.c       # Market: GARCH, indicators, sentiment, regime, Monte Carlo
    ${CMAKE_CURRENT_SOURCE_DIR}/../cognitive/parietal/nimcp_financial_bridge.c       # System bridge: validation pipeline, brain region integration
    ${CMAKE_CURRENT_SOURCE_DIR}/../cognitive/parietal/nimcp_financial_neural_bridge.c # Neural bridge: SNN encoding, STDP, LNN, plasticity
    ${CMAKE_CURRENT_SOURCE_DIR}/../cognitive/parietal/nimcp_financial_resonance_bridge.c # Resonance bridge: Kuramoto coherence, pattern similarity
    ${CMAKE_CURRENT_SOURCE_DIR}/../cognitive/parietal/nimcp_financial_mammillary_bridge.c # Mammillary memory bridge: trade episodic memory
    ${CMAKE_CURRENT_SOURCE_DIR}/../cognitive/parietal/nimcp_financial_working_memory_bridge.c # Working memory bridge: salience-based eviction (Phase 10)
    ${CMAKE_CURRENT_SOURCE_DIR}/../cognitive/parietal/nimcp_financial_investor_archetype.c # 10 investor archetypes with fuzzy heuristics
    ${CMAKE_CURRENT_SOURCE_DIR}/../cognitive/parietal/nimcp_financial_motivation_bridge.c # Motivation bridge: nucleus accumbens, FOMO detection, rational override
    ${CMAKE_CURRENT_SOURCE_DIR}/../cognitive/parietal/nimcp_financial_neuromod_bridge.c # Neuromodulator bridge: DA/5HT/NE/ACh/adenosine state, archetype modulation
    ${CMAKE_CURRENT_SOURCE_DIR}/../cognitive/parietal/nimcp_financial_autobio_bridge.c # Autobiographical memory: episodic trading episodes, emotional tagging
    ${CMAKE_CURRENT_SOURCE_DIR}/../cognitive/parietal/nimcp_financial_emotion_bridge.c # Emotion bridge: Plutchik model, bias detection, decision modulation
    ${CMAKE_CURRENT_SOURCE_DIR}/../cognitive/parietal/nimcp_financial_ethics_bridge.c # Ethics bridge: Asimov laws, Golden Rule, harm assessment, empathy scoring (Phase 7)
    ${CMAKE_CURRENT_SOURCE_DIR}/../cognitive/parietal/nimcp_financial_explanations_bridge.c # Explanations bridge: XAI for trading decisions, audit trails, regulatory compliance (Phase 7)
    ${CMAKE_CURRENT_SOURCE_DIR}/../cognitive/parietal/nimcp_financial_world_model_bridge.c # World model: state prediction, counterfactuals, policy rollout
    ${CMAKE_CURRENT_SOURCE_DIR}/../cognitive/parietal/nimcp_financial_tom_bridge.c   # Theory of Mind: investor modeling, action prediction, false beliefs
    ${CMAKE_CURRENT_SOURCE_DIR}/../cognitive/parietal/nimcp_financial_salience_bridge.c # Salience bridge: attention prioritization, novelty/surprise/urgency/relevance
    ${CMAKE_CURRENT_SOURCE_DIR}/../cognitive/parietal/nimcp_financial_emo_attention_bridge.c # Emo-attention bridge: emotional modulation of attention
    ${CMAKE_CURRENT_SOURCE_DIR}/../cognitive/parietal/nimcp_financial_mental_health_bridge.c # Mental health bridge: cognitive load, stress, burnout monitoring (Phase 10)
    ${CMAKE_CURRENT_SOURCE_DIR}/../cognitive/parietal/nimcp_financial_basal_ganglia_bridge.c # Basal ganglia: Q-learning, action selection, habit detection (Phase 6)
    ${CMAKE_CURRENT_SOURCE_DIR}/../cognitive/parietal/nimcp_financial_predictive_bridge.c # Predictive coding: FEP, active inference, EFE computation (Phase 6)
    ${CMAKE_CURRENT_SOURCE_DIR}/../cognitive/parietal/nimcp_financial_metacognition_bridge.c # Metacognition: bias detection, confidence calibration, reconsideration (Phase 9)
    ${CMAKE_CURRENT_SOURCE_DIR}/../cognitive/parietal/nimcp_financial_reasoning_bridge.c # Reasoning bridge: forward/backward chaining, trading rules (Graham, Buffett, etc.)
    ${CMAKE_CURRENT_SOURCE_DIR}/../cognitive/parietal/nimcp_financial_uncertainty_bridge.c # Uncertainty decomposition: epistemic/aleatoric, info gathering decisions (Phase 9)
    ${CMAKE_CURRENT_SOURCE_DIR}/../cognitive/parietal/nimcp_financial_jepa_bridge.c  # JEPA masking: predict missing factors, cross-modal prediction (Phase 6)
    ${CMAKE_CURRENT_SOURCE_DIR}/../cognitive/parietal/nimcp_financial_consolidation_bridge.c # Memory consolidation: pattern replay, winner strengthening, loser pruning (Phase 8)
    ${CMAKE_CURRENT_SOURCE_DIR}/../cognitive/parietal/nimcp_financial_temporal_credit_bridge.c # Temporal credit: TD(lambda) eligibility traces, credit assignment to past decisions (Phase 8)
    ${CMAKE_CURRENT_SOURCE_DIR}/../cognitive/parietal/nimcp_financial_stdp_bridge.c  # STDP bridge: signal-outcome timing correlations, reward-modulated plasticity (Phase 8)
    ${CMAKE_CURRENT_SOURCE_DIR}/../cognitive/parietal/nimcp_financial_regret_bridge.c # Regret bridge: counterfactual analysis, lesson extraction (Phase 9)
    ${CMAKE_CURRENT_SOURCE_DIR}/../cognitive/parietal/nimcp_financial_curiosity_bridge.c # Curiosity bridge: hypothesis generation, exploration selection (Phase 9)
    ${CMAKE_CURRENT_SOURCE_DIR}/../cognitive/parietal/nimcp_financial_cognitive_orchestrator.c # Cognitive orchestrator: master integration (Phase 10)
    ${CMAKE_CURRENT_SOURCE_DIR}/../cognitive/parietal/nimcp_financial_validation_stubs.c # BBB/immune validation stubs (temporary)
    ${CMAKE_CURRENT_SOURCE_DIR}/../cognitive/parietal/nimcp_parietal.c               # Parietal orchestrator with Hamiltonian-Lagrangian NN
    ${CMAKE_CURRENT_SOURCE_DIR}/../cognitive/parietal/nimcp_code_generation.c       # Self-repair code generation engine (FT integration)

    # Creative/Artistic System (Phase Creative: Multi-modal art appreciation and generation)
    # Core Creative Modules
    ${CMAKE_CURRENT_SOURCE_DIR}/../cognitive/creative/nimcp_creative.c                        # Core creative types and utilities
    ${CMAKE_CURRENT_SOURCE_DIR}/../cognitive/creative/nimcp_creative_orchestrator.c           # Master creative orchestrator
    # Appreciation System
    ${CMAKE_CURRENT_SOURCE_DIR}/../cognitive/creative/appreciation/nimcp_aesthetic_evaluation.c    # Berlyne aesthetics evaluation
    ${CMAKE_CURRENT_SOURCE_DIR}/../cognitive/creative/appreciation/nimcp_creative_emotion_bridge.c # Emotion-aesthetic integration
    ${CMAKE_CURRENT_SOURCE_DIR}/../cognitive/creative/appreciation/nimcp_creative_memory_bridge.c  # Artistic memory/preferences
    ${CMAKE_CURRENT_SOURCE_DIR}/../cognitive/creative/appreciation/nimcp_style_perception.c        # Style recognition/analysis
    # Inspiration System
    ${CMAKE_CURRENT_SOURCE_DIR}/../cognitive/creative/inspiration/nimcp_style_representation.c     # Style embeddings (archetype pattern)
    ${CMAKE_CURRENT_SOURCE_DIR}/../cognitive/creative/inspiration/nimcp_influence_blending.c       # Multi-influence combination
    ${CMAKE_CURRENT_SOURCE_DIR}/../cognitive/creative/inspiration/nimcp_creative_pattern_extractor.c # Pattern extraction from art
    ${CMAKE_CURRENT_SOURCE_DIR}/../cognitive/creative/inspiration/nimcp_creative_knowledge_bridge.c  # KG integration for art knowledge
    # Generation System
    ${CMAKE_CURRENT_SOURCE_DIR}/../cognitive/creative/generation/nimcp_text_generation.c       # Poetry, screenplay, literature
    ${CMAKE_CURRENT_SOURCE_DIR}/../cognitive/creative/generation/nimcp_music_generation.c      # Composition, arrangement, MIDI export
    ${CMAKE_CURRENT_SOURCE_DIR}/../cognitive/creative/generation/nimcp_visual_generation.c     # Still images, visual art
    ${CMAKE_CURRENT_SOURCE_DIR}/../cognitive/creative/generation/nimcp_video_generation.c      # Video/cinema generation
    ${CMAKE_CURRENT_SOURCE_DIR}/../cognitive/creative/generation/nimcp_multimodal_director.c   # Full-length film coordination
    # External Model Integration
    ${CMAKE_CURRENT_SOURCE_DIR}/../cognitive/creative/external/nimcp_creative_onnx_runtime.c   # ONNX Runtime wrapper
    ${CMAKE_CURRENT_SOURCE_DIR}/../cognitive/creative/external/nimcp_creative_api_client.c     # Cloud API integration
    ${CMAKE_CURRENT_SOURCE_DIR}/../cognitive/creative/external/nimcp_diffusion_bridge.c        # Diffusion model interface
    ${CMAKE_CURRENT_SOURCE_DIR}/../cognitive/creative/external/nimcp_gan_bridge.c              # GAN model interface
    # Creative Bridges
    ${CMAKE_CURRENT_SOURCE_DIR}/../cognitive/creative/bridges/nimcp_creative_bridge.c          # Main validation bridge
    ${CMAKE_CURRENT_SOURCE_DIR}/../cognitive/creative/bridges/nimcp_creative_neural_bridge.c   # Neural generation interface
    ${CMAKE_CURRENT_SOURCE_DIR}/../cognitive/creative/bridges/nimcp_creative_ethics_bridge.c   # Copyright/ethics validation
    ${CMAKE_CURRENT_SOURCE_DIR}/../cognitive/creative/bridges/nimcp_creative_training_bridge.c # Training/fine-tuning

    # Free Energy Principle - Karl Friston's predictive processing framework
    ${CMAKE_CURRENT_SOURCE_DIR}/../cognitive/free_energy/nimcp_free_energy.c  # Core FEP implementation (variational free energy, active inference)

    # Free Energy Principle Enhancements
    ${CMAKE_CURRENT_SOURCE_DIR}/../cognitive/free_energy/nimcp_fep_learning.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../cognitive/free_energy/nimcp_fep_curiosity.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../cognitive/free_energy/nimcp_fep_neuromod.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../cognitive/free_energy/nimcp_fep_immune_bridge.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../cognitive/free_energy/nimcp_fep_consciousness.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../cognitive/free_energy/nimcp_fep_planning.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../cognitive/free_energy/nimcp_fep_evidence.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../cognitive/free_energy/nimcp_fep_context.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../cognitive/free_energy/nimcp_fep_sleep.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../cognitive/free_energy/nimcp_fep_orchestrator.c  # FEP Orchestrator - coordinates all FEP bridges
    ${CMAKE_CURRENT_SOURCE_DIR}/../cognitive/free_energy/nimcp_fep_hnn_fno_bridges.c  # HNN + FNO → FEP bridges

    # JEPA (Joint Embedding Predictive Architecture) - Phase JEPA
    ${CMAKE_CURRENT_SOURCE_DIR}/../cognitive/jepa/nimcp_jepa_latent.c           # Latent space embeddings
    ${CMAKE_CURRENT_SOURCE_DIR}/../cognitive/jepa/nimcp_jepa_predictor.c        # MLP predictor network
    ${CMAKE_CURRENT_SOURCE_DIR}/../cognitive/jepa/nimcp_jepa_masking.c          # Masking strategies
    ${CMAKE_CURRENT_SOURCE_DIR}/../cognitive/jepa/nimcp_jepa_weights.c          # V-JEPA weight import
    ${CMAKE_CURRENT_SOURCE_DIR}/../cognitive/jepa/nimcp_jepa_multimodal.c       # Multimodal joint embeddings
    ${CMAKE_CURRENT_SOURCE_DIR}/../cognitive/jepa/nimcp_jepa_context.c          # Context-conditioned encoding
    ${CMAKE_CURRENT_SOURCE_DIR}/../cognitive/jepa/nimcp_jepa_bidirectional.c    # Bidirectional JEPA (omnidirectional inference)

    # Omnidirectional Inference Memory Components
    ${CMAKE_CURRENT_SOURCE_DIR}/../cognitive/memory/nimcp_hopfield_memory.c      # Hopfield associative memory
    ${CMAKE_CURRENT_SOURCE_DIR}/../cognitive/memory/nimcp_temporal_replay.c      # Temporal replay buffer
    ${CMAKE_CURRENT_SOURCE_DIR}/../cognitive/predictive/nimcp_predictive_hierarchy.c  # Predictive coding hierarchy

    # FEP Module Bridges - Bidirectional integration with FEP system
    # Cognitive FEP Bridges
    ${CMAKE_CURRENT_SOURCE_DIR}/../cognitive/attention/nimcp_attention_fep_bridge.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../cognitive/autobiographical_memory/nimcp_autobiographical_fep_bridge.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../cognitive/bias/nimcp_bias_fep_bridge.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../cognitive/emotion/nimcp_emotion_fep_bridge.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../cognitive/ethics/nimcp_ethics_fep_bridge.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../cognitive/executive/nimcp_executive_fep_bridge.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../cognitive/introspection/nimcp_introspection_fep_bridge.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../cognitive/knowledge/nimcp_knowledge_fep_bridge.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../cognitive/memory/nimcp_memory_fep_bridge.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../cognitive/mental_health/nimcp_mental_health_fep_bridge.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../cognitive/mirror_neurons/nimcp_mirror_neurons_fep_bridge.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../cognitive/reasoning/nimcp_reasoning_fep_bridge.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../cognitive/salience/nimcp_salience_fep_bridge.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../cognitive/salience/nimcp_surprise_amplifier.c  # Society of Thought: Surprise amplification (Phase 1)
    ${CMAKE_CURRENT_SOURCE_DIR}/../cognitive/salience/nimcp_surprise_fep_bridge.c  # Society of Thought: Surprise ↔ FEP bridge
    ${CMAKE_CURRENT_SOURCE_DIR}/../cognitive/salience/nimcp_surprise_gw_bridge.c  # Society of Thought: Surprise → GW bridge
    ${CMAKE_CURRENT_SOURCE_DIR}/../cognitive/salience/nimcp_surprise_attention_bridge.c  # Society of Thought: Surprise ↔ Attention bridge
    ${CMAKE_CURRENT_SOURCE_DIR}/../cognitive/salience/nimcp_surprise_plasticity_bridge.c  # Society of Thought: Surprise ↔ Plasticity bridge
    ${CMAKE_CURRENT_SOURCE_DIR}/../cognitive/salience/nimcp_surprise_snn_bridge.c  # Society of Thought: Surprise ↔ SNN bridge
    ${CMAKE_CURRENT_SOURCE_DIR}/../cognitive/salience/nimcp_surprise_substrate_bridge.c  # Society of Thought: Surprise ↔ Substrate bridge
    ${CMAKE_CURRENT_SOURCE_DIR}/../cognitive/salience/nimcp_surprise_thalamic_bridge.c  # Society of Thought: Surprise ↔ Thalamic bridge
    ${CMAKE_CURRENT_SOURCE_DIR}/../cognitive/salience/nimcp_surprise_pink_noise_bridge.c  # Society of Thought: Surprise ↔ Pink Noise bridge
    ${CMAKE_CURRENT_SOURCE_DIR}/../cognitive/salience/nimcp_surprise_imagination_bridge.c  # Society of Thought: Surprise ↔ Imagination bridge
    ${CMAKE_CURRENT_SOURCE_DIR}/../cognitive/salience/nimcp_surprise_self_model_bridge.c  # Society of Thought: Surprise ↔ Self-Model bridge
    ${CMAKE_CURRENT_SOURCE_DIR}/../cognitive/self_model/nimcp_self_model_fep_bridge.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../cognitive/theory_of_mind/nimcp_tom_fep_bridge.c
    # Core Brain FEP Bridges
    ${CMAKE_CURRENT_SOURCE_DIR}/../core/brain/oscillations/nimcp_oscillations_fep_bridge.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../core/brain_regions/nimcp_predictive_regions_fep_bridge.c
    # Middleware FEP Bridges
    ${CMAKE_CURRENT_SOURCE_DIR}/../middleware/features/nimcp_feature_extractor_fep_bridge.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../middleware/features/nimcp_population_coding_fep_bridge.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../middleware/patterns/nimcp_sequence_detector_fep_bridge.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../middleware/routing/nimcp_thalamic_router_fep_bridge.c
    # Perception FEP Bridges
    ${CMAKE_CURRENT_SOURCE_DIR}/../perception/nimcp_audio_cortex_fep_bridge.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../perception/nimcp_speech_cortex_fep_bridge.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../perception/nimcp_visual_cortex_fep_bridge.c
    # Lip Reading (Phase B5: Audiovisual Speech Integration)
    ${CMAKE_CURRENT_SOURCE_DIR}/../perception/nimcp_lip_reading.c
    # Omnidirectional Inference Bridges (Phase 2)
    ${CMAKE_CURRENT_SOURCE_DIR}/../perception/nimcp_omni_sensory_bridge.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../core/cortical_columns/nimcp_omni_cortical_columns_bridge.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../core/brain/regions/occipital/nimcp_omni_occipital_bridge.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../core/brain/regions/broca/nimcp_omni_broca_bridge.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../cognitive/omni/nimcp_omni_kg_sync.c
    # Omnidirectional Inference Bridges (Phase 4) - Higher Cognitive
    ${CMAKE_CURRENT_SOURCE_DIR}/../cognitive/recursive/nimcp_omni_rcog_bridge.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../cognitive/logic/nimcp_omni_logic_bridge.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../cognitive/immune/nimcp_omni_immune_bridge.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../core/brain/subcortical/nimcp_omni_amygdala_bridge.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../middleware/training/nimcp_omni_training_bridge.c
    # Omnidirectional Inference (Phase 6) - Precision Weighting
    ${CMAKE_CURRENT_SOURCE_DIR}/../cognitive/omni/nimcp_omni_precision.c
    # Omnidirectional Inference (Phase 7) - Active Inference
    ${CMAKE_CURRENT_SOURCE_DIR}/../cognitive/omni/nimcp_omni_active_inference.c
    # Omnidirectional Inference (Phase 9) - Generative World Model
    ${CMAKE_CURRENT_SOURCE_DIR}/../cognitive/omni/nimcp_omni_world_model.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../cognitive/omni/nimcp_omni_jepa_bridge.c  # Phase 4n: omni world model ↔ JEPA
    # World Model Bridges (11 integration bridges for multi-system coupling)
    ${CMAKE_CURRENT_SOURCE_DIR}/../cognitive/omni/bridges/nimcp_omni_wm_memory_bridge.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../cognitive/omni/bridges/nimcp_omni_wm_substrate_bridge.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../cognitive/omni/bridges/nimcp_omni_wm_cognitive_bridge.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../cognitive/omni/bridges/nimcp_omni_wm_security_immune_bridge.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../cognitive/omni/bridges/nimcp_omni_wm_logging_bridge.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../cognitive/omni/bridges/nimcp_omni_wm_parietal_bridge.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../cognitive/omni/bridges/nimcp_omni_wm_hypothalamus_bridge.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../cognitive/omni/bridges/nimcp_omni_wm_thalamic_bridge.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../cognitive/omni/bridges/nimcp_omni_wm_kg_bridge.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../cognitive/omni/bridges/nimcp_omni_wm_tom_bridge.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../cognitive/omni/bridges/nimcp_omni_wm_plasticity_bridge.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../cognitive/omni/bridges/nimcp_omni_wm_thousand_brains_bridge.c  # Hawkins Thousand Brains integration
    # World Model Prior (physics + chemistry + biology simulation engines)
    ${CMAKE_CURRENT_SOURCE_DIR}/../cognitive/physics/nimcp_intuitive_physics.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../cognitive/physics/nimcp_entity_tracker.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../cognitive/physics/nimcp_scene_graph.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../cognitive/physics/nimcp_physics_prior.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../cognitive/physics/nimcp_chemistry_sim.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../cognitive/physics/nimcp_biology_sim.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../cognitive/physics/nimcp_world_prior.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../cognitive/physics/nimcp_relativistic_physics.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../cognitive/physics/nimcp_electromagnetic.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../cognitive/physics/nimcp_magnetohydrodynamics.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../cognitive/physics/nimcp_surface_physics.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../cognitive/physics/nimcp_surface_chemistry.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../cognitive/physics/nimcp_biochemistry.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../cognitive/physics/nimcp_molecular_biology.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../cognitive/physics/nimcp_physical_chemistry.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../cognitive/physics/nimcp_organic_chemistry.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../cognitive/physics/nimcp_chemical_engineering.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../cognitive/physics/nimcp_astrophysics.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../cognitive/physics/nimcp_particle_physics.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../cognitive/physics/nimcp_qed.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../cognitive/physics/nimcp_sim_perception_bridge.c
    # Physics extensions
    ${CMAKE_CURRENT_SOURCE_DIR}/../cognitive/physics/nimcp_fluid_dynamics.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../cognitive/physics/nimcp_heat_transfer.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../cognitive/physics/nimcp_acoustics.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../cognitive/physics/nimcp_optics.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../cognitive/physics/nimcp_nuclear_physics.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../cognitive/physics/nimcp_condensed_matter.c
    # Chemistry extensions
    ${CMAKE_CURRENT_SOURCE_DIR}/../cognitive/physics/nimcp_polymer_chemistry.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../cognitive/physics/nimcp_analytical_chemistry.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../cognitive/physics/nimcp_inorganic_chemistry.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../cognitive/physics/nimcp_geochemistry.c
    # Biology extensions
    ${CMAKE_CURRENT_SOURCE_DIR}/../cognitive/physics/nimcp_cell_biology.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../cognitive/physics/nimcp_immunology.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../cognitive/physics/nimcp_neuroscience_sim.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../cognitive/physics/nimcp_ecology.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../cognitive/physics/nimcp_evolutionary_biology.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../cognitive/physics/nimcp_physiology.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../cognitive/physics/nimcp_world_simulator.c
    # Pure Mathematics
    ${CMAKE_CURRENT_SOURCE_DIR}/../cognitive/math/nimcp_number_theory.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../cognitive/math/nimcp_abstract_algebra.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../cognitive/math/nimcp_real_analysis.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../cognitive/math/nimcp_topology.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../cognitive/math/nimcp_graph_theory.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../cognitive/math/nimcp_combinatorics.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../cognitive/math/nimcp_logic.c
    # Applied Mathematics
    ${CMAKE_CURRENT_SOURCE_DIR}/../cognitive/math/nimcp_linear_algebra.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../cognitive/math/nimcp_probability.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../cognitive/math/nimcp_numerical_methods.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../cognitive/math/nimcp_optimization.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../cognitive/math/nimcp_information_theory.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../cognitive/math/nimcp_game_theory.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../cognitive/math/nimcp_category_theory.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../cognitive/math/nimcp_complexity_theory.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../cognitive/math/nimcp_zeta_functions.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../cognitive/math/nimcp_knot_theory.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../cognitive/math/nimcp_dsp.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../cognitive/physics/nimcp_qft.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../cognitive/physics/nimcp_world_model_bridges.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../cognitive/physics/nimcp_world_model_cognitive_integration.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../cognitive/physics/nimcp_audio_dsp_bridge.c
    # Multimodal World Model (cross-modal prediction and fusion)
    ${CMAKE_CURRENT_SOURCE_DIR}/../cognitive/extrapolation/nimcp_world_model_multimodal.c
    # Compositional and Counterfactual modules
    ${CMAKE_CURRENT_SOURCE_DIR}/../cognitive/extrapolation/nimcp_compositional_systematic.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../cognitive/extrapolation/nimcp_counterfactual_imagination.c
    # Omnidirectional Inference (Phase 10) - Metacognition
    ${CMAKE_CURRENT_SOURCE_DIR}/../cognitive/omni/nimcp_omni_metacognition.c
    # Plasticity FEP Bridges
    ${CMAKE_CURRENT_SOURCE_DIR}/../plasticity/bcm/nimcp_bcm_fep_bridge.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../plasticity/dendritic/nimcp_dendritic_fep_bridge.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../plasticity/eligibility/nimcp_eligibility_fep_bridge.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../plasticity/eligibility/nimcp_eligibility_quantum_bridge.c  # Eligibility-quantum integration (bottleneck, credit, optimization)
    ${CMAKE_CURRENT_SOURCE_DIR}/../plasticity/eligibility/nimcp_eligibility_utils_bridge.c  # Utils integration (metrics, pools, RK4, Shannon)
    ${CMAKE_CURRENT_SOURCE_DIR}/../plasticity/eligibility/nimcp_eligibility_utils_quantum_bridge.c  # Bidirectional Utils-Quantum bridge
    ${CMAKE_CURRENT_SOURCE_DIR}/../plasticity/homeostatic/nimcp_homeostatic_fep_bridge.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../plasticity/stdp/nimcp_stdp_fep_bridge.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../plasticity/adaptive/nimcp_adaptive_fep_bridge.c
    # Plasticity Bio-Async Bridge (P0-P2 unified bridge)
    ${CMAKE_CURRENT_SOURCE_DIR}/../plasticity/integration/nimcp_plasticity_bio_async_bridge.c
    # Additional Cognitive FEP Bridges (Emotion/Epistemic/Predictive)
    ${CMAKE_CURRENT_SOURCE_DIR}/../cognitive/analysis/nimcp_analysis_fep_bridge.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../cognitive/emotional_tagging/nimcp_emotional_tagging_fep_bridge.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../cognitive/emotion_recognition/nimcp_emotion_recognition_fep_bridge.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../cognitive/empathetic_response/nimcp_empathetic_response_fep_bridge.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../cognitive/epistemic/nimcp_epistemic_fep_bridge.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../cognitive/grief/nimcp_grief_fep_bridge.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../cognitive/joy/nimcp_joy_fep_bridge.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../cognitive/remorse/nimcp_remorse_fep_bridge.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../cognitive/nimcp_emotional_system_fep_bridge.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../cognitive/predictive/nimcp_predictive_fep_bridge.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../cognitive/working_memory/nimcp_working_memory_fep_bridge.c
    # Middleware FEP Bridges (Additional)
    ${CMAKE_CURRENT_SOURCE_DIR}/../middleware/buffering/nimcp_circular_buffer_fep_bridge.c

    # FEP Module Bridges - Phase 2: Extended Integration (Dec 2025)
    # Async FEP Bridges
    ${CMAKE_CURRENT_SOURCE_DIR}/../async/nimcp_bio_async_fep_bridge.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../async/nimcp_bio_router_fep_bridge.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../async/nimcp_biological_timescales_fep_bridge.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../async/nimcp_predictive_protocol_fep_bridge.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../async/nimcp_semantic_compression_fep_bridge.c
    # Cognitive FEP Bridges (Extended)
    ${CMAKE_CURRENT_SOURCE_DIR}/../cognitive/consolidation/nimcp_consolidation_fep_bridge.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../cognitive/explanations/nimcp_explanations_fep_bridge.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../cognitive/fractal_cognitive/nimcp_fractal_cognitive_fep_bridge.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../cognitive/love_loyalty_friendship/nimcp_love_loyalty_friendship_fep_bridge.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../cognitive/nimcp_hierarchical_fep_bridge.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../cognitive/nimcp_meta_learning_fep_bridge.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../cognitive/nimcp_personality_fep_bridge.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../cognitive/self_awareness_extended/nimcp_self_awareness_extended_fep_bridge.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../cognitive/shadow_emotions/nimcp_shadow_emotions_fep_bridge.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../cognitive/sleep_wake/nimcp_sleep_wake_fep_bridge.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../cognitive/symbolic_logic/nimcp_symbolic_logic_fep_bridge.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../cognitive/symbolic_logic/nimcp_symbolic_logic_hub_bridge.c  # Symbolic logic - Cognitive Hub integration
    ${CMAKE_CURRENT_SOURCE_DIR}/../cognitive/symbolic_logic/nimcp_symbolic_logic_safety.c  # LGSS A1: Safety KB
    ${CMAKE_CURRENT_SOURCE_DIR}/../cognitive/symbolic_logic/nimcp_symbolic_logic_lgss_loader.c  # LGSS A1: JSON loader
    ${CMAKE_CURRENT_SOURCE_DIR}/../cognitive/symbolic_logic/bridges/nimcp_symbolic_logic_plasticity_bridge.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../cognitive/wellbeing/nimcp_wellbeing_fep_bridge.c
    # Cognitive Output Rubric (human-style quality evaluation)
    ${CMAKE_CURRENT_SOURCE_DIR}/../cognitive/rubric/nimcp_rubric.c
    # Core FEP Bridges
    ${CMAKE_CURRENT_SOURCE_DIR}/../core/cortical_columns/nimcp_cortical_column_fep_bridge.c
    # Glial FEP Bridges
    ${CMAKE_CURRENT_SOURCE_DIR}/../glial/astrocytes/nimcp_astrocytes_fep_bridge.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../glial/microglia/nimcp_microglia_fep_bridge.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../glial/oligodendrocytes/nimcp_oligodendrocytes_fep_bridge.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../glial/myelin_sheath/nimcp_myelin_sheath_fep_bridge.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../glial/integration/nimcp_glial_integration_fep_bridge.c
    # Middleware FEP Bridges (Extended)
    ${CMAKE_CURRENT_SOURCE_DIR}/../middleware/training/nimcp_training_module_fep_bridge.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../middleware/training/nimcp_occipital_training_bridge.c  # Occipital-training integration
    # Plasticity FEP Bridges (Extended)
    ${CMAKE_CURRENT_SOURCE_DIR}/../plasticity/neuromodulators/nimcp_neuromodulators_fep_bridge.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../plasticity/nimcp_second_messengers_fep_bridge.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../plasticity/noise/nimcp_pink_noise_fep_bridge.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../plasticity/predictive/nimcp_predictive_coding_fep_bridge.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../plasticity/stp/nimcp_stp_fep_bridge.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../plasticity/stp/nimcp_stp_pink_noise_bridge.c
    # Security FEP Bridges
    ${CMAKE_CURRENT_SOURCE_DIR}/../security/nimcp_anomaly_detector_fep_bridge.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../security/nimcp_blood_brain_barrier_fep_bridge.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../security/nimcp_pattern_db_fep_bridge.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../security/nimcp_rate_limiter_fep_bridge.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../security/nimcp_security_fep_bridge.c
    # Swarm FEP Bridges
    ${CMAKE_CURRENT_SOURCE_DIR}/../swarm/nimcp_collective_workspace_fep_bridge.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../swarm/nimcp_emotional_contagion_fep_bridge.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../swarm/nimcp_swarm_brain_fep_bridge.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../swarm/nimcp_swarm_consciousness_fep_bridge.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../swarm/nimcp_swarm_consensus_fep_bridge.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../swarm/nimcp_swarm_emergence_fep_bridge.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../swarm/nimcp_swarm_flocking_fep_bridge.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../swarm/nimcp_swarm_immune_fep_bridge.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../swarm/nimcp_swarm_memory_fep_bridge.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../swarm/nimcp_swarm_pheromone_fep_bridge.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../swarm/nimcp_swarm_quorum_fep_bridge.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../swarm/nimcp_swarm_signal_fep_bridge.c

    # Plasticity mechanisms
    ${CMAKE_CURRENT_SOURCE_DIR}/../plasticity/adaptive/nimcp_backprop_kernel.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../plasticity/adaptive/nimcp_adaptive.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../plasticity/bcm/nimcp_bcm.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../plasticity/homeostatic/nimcp_homeostatic.c   # Phase T1: Homeostatic Plasticity (synaptic scaling, intrinsic plasticity)
    ${CMAKE_CURRENT_SOURCE_DIR}/../plasticity/dendritic/nimcp_dendritic.c       # Phase T2: Dendritic Nonlinearities (NMDA dynamics, compartmental)
    ${CMAKE_CURRENT_SOURCE_DIR}/../plasticity/predictive/nimcp_predictive_coding.c  # Phase T3: Predictive Coding (hierarchical error minimization)
    ${CMAKE_CURRENT_SOURCE_DIR}/../plasticity/metaplasticity/nimcp_extended_metaplasticity.c  # Extended Metaplasticity (multi-timescale threshold adaptation)
    ${CMAKE_CURRENT_SOURCE_DIR}/../plasticity/metaplasticity/nimcp_metaplasticity_sleep_bridge.c  # Metaplasticity-sleep integration
    ${CMAKE_CURRENT_SOURCE_DIR}/../plasticity/metaplasticity/nimcp_metaplasticity_immune_bridge.c  # Metaplasticity-immune integration
    ${CMAKE_CURRENT_SOURCE_DIR}/../plasticity/neuromodulators/nimcp_neuromodulators.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../plasticity/neuromodulators/nimcp_neuromod_jepa_bridge.c  # Phase 4o: neuromod ↔ JEPA
    ${CMAKE_CURRENT_SOURCE_DIR}/../plasticity/neuromodulators/nimcp_neuromod_pink_noise.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../plasticity/neuromodulators/nimcp_spatial_neuromod.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../plasticity/neuromodulators/nimcp_receptor_subtypes.c  # Phase C2.2 Enhancement #1: Receptor subtypes
    ${CMAKE_CURRENT_SOURCE_DIR}/../plasticity/neuromodulators/nimcp_phasic_tonic.c  # Phase C2.2 Enhancement #2: Phasic-tonic dynamics
    ${CMAKE_CURRENT_SOURCE_DIR}/../plasticity/neuromodulators/nimcp_vesicle_packaging.c  # Phase C2.3 Enhancement #3: Synaptic vesicle packaging
    ${CMAKE_CURRENT_SOURCE_DIR}/../plasticity/neuromodulators/nimcp_metabolic_pathways.c  # Phase C2.4 Enhancement #4: Metabolic pathways
    ${CMAKE_CURRENT_SOURCE_DIR}/../plasticity/neuromodulators/nimcp_neuromod_substrate_bridge.c  # Neuromodulator-substrate integration (DA/5-HT/NE substrate effects)
    ${CMAKE_CURRENT_SOURCE_DIR}/../plasticity/immune/nimcp_neuromodulator_immune.c  # Neuromodulator-immune integration (cytokine effects, imbalance detection)
    ${CMAKE_CURRENT_SOURCE_DIR}/../plasticity/immune/nimcp_bcm_immune_bridge.c  # BCM learning-immune integration
    ${CMAKE_CURRENT_SOURCE_DIR}/../plasticity/immune/nimcp_dendritic_immune_bridge.c  # Dendritic plasticity-immune integration
    ${CMAKE_CURRENT_SOURCE_DIR}/../plasticity/immune/nimcp_eligibility_immune_bridge.c  # Eligibility trace-immune integration
    ${CMAKE_CURRENT_SOURCE_DIR}/../plasticity/immune/nimcp_homeostatic_immune_bridge.c  # Homeostatic plasticity-immune integration
    ${CMAKE_CURRENT_SOURCE_DIR}/../plasticity/immune/nimcp_stdp_immune_bridge.c  # STDP-immune integration
    ${CMAKE_CURRENT_SOURCE_DIR}/../plasticity/immune/nimcp_synaptic_scaling_immune_bridge.c  # Synaptic scaling-immune integration
    ${CMAKE_CURRENT_SOURCE_DIR}/../plasticity/protein/nimcp_protein_immune_bridge.c  # Protein synthesis-immune integration (inflammation suppresses synthesis)
    ${CMAKE_CURRENT_SOURCE_DIR}/../plasticity/second_messengers/nimcp_second_messengers.c  # Second Messenger Cascades (cAMP, IP3/DAG, Ca2+, gene expression)
    ${CMAKE_CURRENT_SOURCE_DIR}/../plasticity/attention/nimcp_attention.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../plasticity/stp/nimcp_stp.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../plasticity/noise/nimcp_pink_noise.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../plasticity/noise/nimcp_pink_noise_multiscale.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../plasticity/noise/nimcp_pink_noise_correlated.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../plasticity/noise/nimcp_pink_noise_criticality.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../plasticity/noise/nimcp_pink_noise_quantum_bridge.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../plasticity/noise/nimcp_pink_noise_immune_bridge.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../plasticity/noise/nimcp_pink_noise_sleep.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../plasticity/noise/nimcp_pink_noise_simd.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../plasticity/noise/nimcp_pink_noise_monitor.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../plasticity/noise/nimcp_pink_noise_spatial.c
    # Pink Noise Domain Bridges (1/f noise integration across neural subsystems)
    ${CMAKE_CURRENT_SOURCE_DIR}/../plasticity/calcium/nimcp_calcium_pink_noise_bridge.c  # Calcium-pink noise integration (NMDA, Ca2+ wave modulation)
    ${CMAKE_CURRENT_SOURCE_DIR}/../plasticity/dendritic/nimcp_dendritic_pink_noise_bridge.c  # Dendritic-pink noise integration (bAP, NMDA spike modulation)
    ${CMAKE_CURRENT_SOURCE_DIR}/../plasticity/heterosynaptic/nimcp_heterosynaptic_pink_noise_bridge.c  # Heterosynaptic-pink noise integration (competition dynamics)
    ${CMAKE_CURRENT_SOURCE_DIR}/../plasticity/metabolic/nimcp_metabolic_pink_noise_bridge.c  # Metabolic-pink noise integration (ATP/energy fluctuations)
    ${CMAKE_CURRENT_SOURCE_DIR}/../plasticity/neuromodulators/nimcp_spatial_neuromod_pink_noise_bridge.c  # Spatial neuromodulator-pink noise integration
    ${CMAKE_CURRENT_SOURCE_DIR}/../plasticity/neuromodulators/nimcp_vesicle_packaging_pink_noise_bridge.c  # Vesicle packaging-pink noise integration
    ${CMAKE_CURRENT_SOURCE_DIR}/../cognitive/introspection/nimcp_ensemble_uncertainty_pink_noise_bridge.c  # Ensemble uncertainty-pink noise integration
    ${CMAKE_CURRENT_SOURCE_DIR}/../cognitive/memory/nimcp_systems_consolidation_pink_noise_bridge.c  # Memory consolidation-pink noise integration
    ${CMAKE_CURRENT_SOURCE_DIR}/../core/brain_oscillations/nimcp_oscillations_pink_noise_bridge.c  # Brain oscillations-pink noise integration (1/f cross-frequency coupling)
    ${CMAKE_CURRENT_SOURCE_DIR}/../middleware/encoding/nimcp_population_coding_pink_noise_bridge.c  # Population coding-pink noise integration (tuning curve modulation)
    ${CMAKE_CURRENT_SOURCE_DIR}/../plasticity/stdp/nimcp_stdp.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../plasticity/stdp/nimcp_triplet_stdp.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../plasticity/eligibility/nimcp_eligibility_trace.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../plasticity/calcium/nimcp_calcium_dynamics.c  # Calcium-dependent learning rate (Bhalla & Bhatt omega function)
    ${CMAKE_CURRENT_SOURCE_DIR}/../plasticity/calcium/nimcp_calcium_sleep_bridge.c  # Calcium-sleep integration (NMDA modulation, clearance)
    ${CMAKE_CURRENT_SOURCE_DIR}/../plasticity/calcium/nimcp_calcium_immune_bridge.c  # Calcium-immune integration (cytokine effects, instability detection)
    ${CMAKE_CURRENT_SOURCE_DIR}/../plasticity/protein/nimcp_protein_synthesis.c  # Protein synthesis constraints - Frey & Morris synaptic tagging and capture
    ${CMAKE_CURRENT_SOURCE_DIR}/../plasticity/structural/nimcp_structural_plasticity.c  # Structural plasticity - spine dynamics, formation, elimination
    ${CMAKE_CURRENT_SOURCE_DIR}/../plasticity/structural/nimcp_structural_immune_bridge.c  # Structural plasticity-immune integration (microglia pruning, complement tagging)
    ${CMAKE_CURRENT_SOURCE_DIR}/../plasticity/metabolic/nimcp_metabolic_plasticity.c  # Metabolic plasticity - ATP/energy constraints on plasticity
    ${CMAKE_CURRENT_SOURCE_DIR}/../plasticity/metabolic/nimcp_metabolic_sleep_bridge.c  # Metabolic-sleep integration (recovery rates)
    ${CMAKE_CURRENT_SOURCE_DIR}/../plasticity/metabolic/nimcp_metabolic_immune_bridge.c  # Metabolic-immune integration (inflammation costs)
    ${CMAKE_CURRENT_SOURCE_DIR}/../plasticity/heterosynaptic/nimcp_heterosynaptic.c  # Heterosynaptic plasticity - competition/normalization
    ${CMAKE_CURRENT_SOURCE_DIR}/../plasticity/heterosynaptic/nimcp_heterosynaptic_sleep_bridge.c  # Heterosynaptic-sleep integration
    ${CMAKE_CURRENT_SOURCE_DIR}/../plasticity/heterosynaptic/nimcp_heterosynaptic_immune_bridge.c  # Heterosynaptic-immune integration
    ${CMAKE_CURRENT_SOURCE_DIR}/../plasticity/astrocyte/nimcp_astrocyte_plasticity.c  # Astrocyte plasticity - gliotransmitter modulation
    ${CMAKE_CURRENT_SOURCE_DIR}/../plasticity/astrocyte/nimcp_astrocyte_sleep_bridge.c  # Astrocyte-sleep integration
    ${CMAKE_CURRENT_SOURCE_DIR}/../plasticity/astrocyte/nimcp_astrocyte_immune_bridge.c  # Plasticity-level astrocyte-immune bridge (works with astrocyte_plasticity_t)
    ${CMAKE_CURRENT_SOURCE_DIR}/../plasticity/nimcp_plasticity_substrate_bridge.c  # Plasticity-substrate integration (STDP/BCM/homeostatic LR modulation)
    ${CMAKE_CURRENT_SOURCE_DIR}/../plasticity/nimcp_plasticity_coordinator.c  # Plasticity Coordinator - unified manager for all plasticity mechanisms
    ${CMAKE_CURRENT_SOURCE_DIR}/../plasticity/nimcp_plasticity_orchestrator.c  # Plasticity Orchestrator - coordinates STDP/BCM/homeostatic/structural/metabolic mechanisms

    # Neural-Plasticity Orchestrator Bridges (Axon/Dendrite/Neuron integration)
    ${CMAKE_CURRENT_SOURCE_DIR}/../plasticity/orchestrator/nimcp_axon_orchestrator_bridge.c  # Axon-orchestrator bridge - spike arrivals -> pre_spike timing
    ${CMAKE_CURRENT_SOURCE_DIR}/../plasticity/orchestrator/nimcp_neuron_orchestrator_bridge.c  # Neuron-orchestrator bridge - spike cascade (post_spike + axon + bAP)
    ${CMAKE_CURRENT_SOURCE_DIR}/../plasticity/orchestrator/nimcp_dendrite_orchestrator_bridge.c  # Dendrite-orchestrator bridge - bidirectional spine-synapse sync
    ${CMAKE_CURRENT_SOURCE_DIR}/../plasticity/orchestrator/nimcp_neural_plasticity_coordinator.c  # Neural plasticity coordinator - unified simulation interface

    # Neural Component-Plasticity Bridges (Phase 1: Critical Learning Path)
    ${CMAKE_CURRENT_SOURCE_DIR}/../plasticity/bridges/nimcp_dendrite_plasticity_bridge.c  # Dendrite-plasticity integration (compartment-specific STDP, BCM)
    ${CMAKE_CURRENT_SOURCE_DIR}/../plasticity/bridges/nimcp_synapse_plasticity_bridge.c  # Synapse-plasticity integration (17 plasticity mechanisms hub)
    ${CMAKE_CURRENT_SOURCE_DIR}/../plasticity/bridges/nimcp_axon_plasticity_bridge.c  # Axon-plasticity integration (conduction, myelination, fatigue)

    # Sleep-Wake Integration Bridges (Phase 1.1: Sleep-Module Integration)
    ${CMAKE_CURRENT_SOURCE_DIR}/../plasticity/neuromodulators/nimcp_neuromodulators_sleep_bridge.c  # Neuromodulators-sleep integration (ACh/NE/DA/5-HT state modulation)
    ${CMAKE_CURRENT_SOURCE_DIR}/../plasticity/stdp/nimcp_stdp_sleep_bridge.c  # STDP-sleep integration (LTP/LTD ratio, timing window)
    ${CMAKE_CURRENT_SOURCE_DIR}/../plasticity/stdp/nimcp_triplet_stdp_sleep_bridge.c  # Triplet STDP-sleep integration (trace time constants, amplitude modulation)
    ${CMAKE_CURRENT_SOURCE_DIR}/../plasticity/stdp/nimcp_triplet_stdp_immune_bridge.c  # Triplet STDP-immune integration (cytokine modulation, instability detection)
    ${CMAKE_CURRENT_SOURCE_DIR}/../plasticity/stdp/nimcp_stdp_pink_noise_bridge.c  # STDP-pink noise integration (quantum-inspired noise modulation for learning rates)
    ${CMAKE_CURRENT_SOURCE_DIR}/../plasticity/stdp/nimcp_stdp_utils_bridge.c  # STDP-utils integration (ring buffer, metrics, memory pools, RK4)
    ${CMAKE_CURRENT_SOURCE_DIR}/../plasticity/stdp/nimcp_stdp_omni_bridge.c  # STDP-Omnidirectional inference integration (PE modulation)
    ${CMAKE_CURRENT_SOURCE_DIR}/../plasticity/stdp/nimcp_stdp_pr_bridge.c  # STDP-Prime Resonant memory integration (resonance gating)
    # Note: nimcp_stdp_quantum_bridge.c is not needed - implementation triggered by nimcp_stdp_fep_bridge.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../plasticity/eligibility/nimcp_eligibility_pr_bridge.c  # Eligibility-Prime Resonant integration (tag-and-capture)
    ${CMAKE_CURRENT_SOURCE_DIR}/../core/brain/factory/init/nimcp_brain_init_plasticity_bridges.c  # Brain factory plasticity bridge init functions
    ${CMAKE_CURRENT_SOURCE_DIR}/../core/brain/factory/init/nimcp_brain_init_sensory.c  # Phase 6 sensory modules (BR-9/10/11: Somatosensory, Olfactory, Gustatory)
    ${CMAKE_CURRENT_SOURCE_DIR}/../plasticity/bcm/nimcp_bcm_sleep_bridge.c  # BCM-sleep integration (sliding threshold modulation)
    ${CMAKE_CURRENT_SOURCE_DIR}/../plasticity/homeostatic/nimcp_homeostatic_sleep_bridge.c  # Homeostatic-sleep integration (Tononi SHY: downscaling in NREM)
    ${CMAKE_CURRENT_SOURCE_DIR}/../plasticity/protein/nimcp_protein_sleep_bridge.c  # Protein synthesis-sleep integration (synthesis upregulation during deep NREM)
    ${CMAKE_CURRENT_SOURCE_DIR}/../plasticity/structural/nimcp_structural_sleep_bridge.c  # Structural plasticity-sleep integration (spine consolidation, pruning)
    ${CMAKE_CURRENT_SOURCE_DIR}/../cognitive/attention/nimcp_attention_sleep_bridge.c  # Attention-sleep integration (capacity, vigilance)
    ${CMAKE_CURRENT_SOURCE_DIR}/../cognitive/working_memory/nimcp_working_memory_sleep_bridge.c  # Working memory-sleep integration (capacity, decay)
    ${CMAKE_CURRENT_SOURCE_DIR}/../cognitive/executive/nimcp_executive_sleep_bridge.c  # Executive function-sleep integration (inhibition, flexibility)
    ${CMAKE_CURRENT_SOURCE_DIR}/../core/brain_oscillations/nimcp_oscillations_sleep_bridge.c  # Oscillations-sleep integration (band power, spindles)
    ${CMAKE_CURRENT_SOURCE_DIR}/../cognitive/reasoning/nimcp_reasoning_sleep_bridge.c  # Reasoning-sleep integration (inference, deduction, induction modulation)
    ${CMAKE_CURRENT_SOURCE_DIR}/../cognitive/logic/nimcp_logic_sleep_bridge.c  # Logic-sleep integration (gate/rule sensitivity, circuit dynamics)
    ${CMAKE_CURRENT_SOURCE_DIR}/../cognitive/memory/nimcp_memory_sleep_bridge.c  # Memory-sleep integration (consolidation, replay, reconsolidation)
    ${CMAKE_CURRENT_SOURCE_DIR}/../cognitive/mirror_neurons/nimcp_mirror_neurons_sleep_bridge.c  # Mirror neurons-sleep integration (observation, imitation, empathy)
    ${CMAKE_CURRENT_SOURCE_DIR}/../glial/sleep/nimcp_microglia_sleep_bridge.c  # Microglia-sleep integration (glymphatic clearance, neuroinflammation)
    ${CMAKE_CURRENT_SOURCE_DIR}/../glial/sleep/nimcp_astrocytes_sleep_bridge.c  # Astrocyte-sleep integration (adenosine, glymphatic, downscaling)
    ${CMAKE_CURRENT_SOURCE_DIR}/../glial/sleep/nimcp_oligodendrocytes_sleep_bridge.c  # Oligodendrocyte-sleep integration (myelin maintenance)
    ${CMAKE_CURRENT_SOURCE_DIR}/../lnn/nimcp_lnn_sleep_bridge.c  # LNN-sleep integration (tau modulation, ODE solver adaptation)

    # LNN Core (Liquid Neural Networks) - included in main lib to avoid circular deps
    ${CMAKE_CURRENT_SOURCE_DIR}/../lnn/nimcp_lnn.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../lnn/nimcp_lnn_config.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../lnn/nimcp_lnn_neuron.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../lnn/nimcp_lnn_layer.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../lnn/nimcp_lnn_network.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../lnn/nimcp_lnn_ode.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../lnn/nimcp_lnn_wiring.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../lnn/nimcp_lnn_gradient.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../lnn/nimcp_lnn_training.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../lnn/nimcp_lnn_hamiltonian.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../lnn/nimcp_lnn_bio_async.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../lnn/nimcp_lnn_immune.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../lnn/nimcp_lnn_parallel.c

    # Cognitive Common Utilities (Shared across substrate bridges)
    ${CMAKE_CURRENT_SOURCE_DIR}/../cognitive/common/nimcp_metabolic_modulation.c  # Shared metabolic modulation utilities

    # Cognitive Substrate Bridges (Phase 6)
    ${CMAKE_CURRENT_SOURCE_DIR}/../cognitive/attention/nimcp_attention_substrate_bridge.c  # Attention-substrate integration (focus, shifting, filtering)
    ${CMAKE_CURRENT_SOURCE_DIR}/../cognitive/emotion/nimcp_emotion_substrate_bridge.c  # Emotion-substrate integration (intensity, regulation)
    ${CMAKE_CURRENT_SOURCE_DIR}/../cognitive/executive/nimcp_executive_substrate_bridge.c  # Executive-substrate integration (decision, inhibition)
    ${CMAKE_CURRENT_SOURCE_DIR}/../cognitive/introspection/nimcp_introspection_substrate_bridge.c  # Introspection-substrate integration (self-awareness)
    # Note: Using consolidation/nimcp_consolidation_substrate_bridge.c instead of memory version (more complete API)
    ${CMAKE_CURRENT_SOURCE_DIR}/../cognitive/consolidation/nimcp_consolidation_substrate_bridge.c  # Consolidation-substrate integration
    ${CMAKE_CURRENT_SOURCE_DIR}/../cognitive/reasoning/nimcp_reasoning_substrate_bridge.c  # Reasoning-substrate integration (inference depth)
    ${CMAKE_CURRENT_SOURCE_DIR}/../cognitive/tom/nimcp_tom_substrate_bridge.c  # ToM-substrate integration (unified API with backward compat)
    ${CMAKE_CURRENT_SOURCE_DIR}/../cognitive/working_memory/nimcp_working_memory_substrate_bridge.c  # Working memory-substrate integration (capacity, decay)
    ${CMAKE_CURRENT_SOURCE_DIR}/../cognitive/wellbeing/nimcp_wellbeing_substrate_bridge.c  # Wellbeing-substrate integration
    ${CMAKE_CURRENT_SOURCE_DIR}/../cognitive/global_workspace/nimcp_gw_substrate_bridge.c  # GW-substrate integration
    ${CMAKE_CURRENT_SOURCE_DIR}/../cognitive/ethics/nimcp_ethics_substrate_bridge.c  # Ethics-substrate integration
    ${CMAKE_CURRENT_SOURCE_DIR}/../cognitive/knowledge/nimcp_knowledge_substrate_bridge.c  # Knowledge-substrate integration
    ${CMAKE_CURRENT_SOURCE_DIR}/../cognitive/logic/nimcp_logic_substrate_bridge.c  # Logic-substrate integration
    ${CMAKE_CURRENT_SOURCE_DIR}/../cognitive/mirror_neurons/nimcp_mirror_substrate_bridge.c  # Mirror-substrate integration
    ${CMAKE_CURRENT_SOURCE_DIR}/../cognitive/personality/nimcp_personality_substrate_bridge.c  # Personality-substrate integration
    ${CMAKE_CURRENT_SOURCE_DIR}/../cognitive/self_awareness/nimcp_self_awareness_substrate_bridge.c  # Self-awareness-substrate integration
    ${CMAKE_CURRENT_SOURCE_DIR}/../cognitive/predictive/nimcp_predictive_substrate_bridge.c  # Predictive-substrate integration
    ${CMAKE_CURRENT_SOURCE_DIR}/../cognitive/curiosity/nimcp_curiosity_substrate_bridge.c  # Curiosity-substrate integration
    ${CMAKE_CURRENT_SOURCE_DIR}/../cognitive/bias/nimcp_bias_substrate_bridge.c  # Bias-substrate integration
    ${CMAKE_CURRENT_SOURCE_DIR}/../cognitive/bias/nimcp_bias_snn_bridge.c  # Bias-SNN integration
    ${CMAKE_CURRENT_SOURCE_DIR}/../cognitive/bias/nimcp_bias_plasticity_bridge.c  # Bias-Plasticity integration
    ${CMAKE_CURRENT_SOURCE_DIR}/../cognitive/epistemic/nimcp_epistemic_substrate_bridge.c  # Epistemic-substrate integration
    ${CMAKE_CURRENT_SOURCE_DIR}/../cognitive/explanations/nimcp_explanations_substrate_bridge.c  # Explanations-substrate integration
    ${CMAKE_CURRENT_SOURCE_DIR}/../cognitive/salience/nimcp_salience_substrate_bridge.c  # Salience-substrate integration
    ${CMAKE_CURRENT_SOURCE_DIR}/../cognitive/salience/nimcp_salience_snn_bridge.c  # Salience-SNN integration
    ${CMAKE_CURRENT_SOURCE_DIR}/../cognitive/salience/nimcp_salience_plasticity_bridge.c  # Salience-Plasticity integration
    ${CMAKE_CURRENT_SOURCE_DIR}/../cognitive/self_model/nimcp_self_model_substrate_bridge.c  # Self-model-substrate integration
    ${CMAKE_CURRENT_SOURCE_DIR}/../cognitive/sleep_wake/nimcp_sleep_wake_substrate_bridge.c  # Sleep-wake-substrate integration
    ${CMAKE_CURRENT_SOURCE_DIR}/../cognitive/meta_learning/nimcp_meta_learning_substrate_bridge.c  # Meta-learning-substrate integration
    ${CMAKE_CURRENT_SOURCE_DIR}/../cognitive/meta_learning/nimcp_meta_learning_snn_bridge.c  # Meta-learning-SNN integration
    ${CMAKE_CURRENT_SOURCE_DIR}/../cognitive/meta_learning/nimcp_meta_learning_plasticity_bridge.c  # Meta-learning-Plasticity integration
    ${CMAKE_CURRENT_SOURCE_DIR}/../cognitive/grief/nimcp_grief_substrate_bridge.c  # Grief-substrate integration
    ${CMAKE_CURRENT_SOURCE_DIR}/../cognitive/joy/nimcp_joy_substrate_bridge.c  # Joy-substrate integration
    ${CMAKE_CURRENT_SOURCE_DIR}/../cognitive/love_loyalty_friendship/nimcp_llf_substrate_bridge.c  # LLF-substrate integration
    ${CMAKE_CURRENT_SOURCE_DIR}/../cognitive/mental_health/nimcp_mental_health_substrate_bridge.c  # Mental-health-substrate integration
    ${CMAKE_CURRENT_SOURCE_DIR}/../cognitive/remorse/nimcp_remorse_substrate_bridge.c  # Remorse-substrate integration
    ${CMAKE_CURRENT_SOURCE_DIR}/../cognitive/shadow/nimcp_shadow_substrate_bridge.c  # Shadow-substrate integration
    ${CMAKE_CURRENT_SOURCE_DIR}/../cognitive/shadow_emotions/nimcp_shadow_emotions_substrate_bridge.c  # Shadow-emotions-substrate integration
    ${CMAKE_CURRENT_SOURCE_DIR}/../cognitive/autobiographical_memory/nimcp_autobio_substrate_bridge.c  # Autobio-substrate integration
    ${CMAKE_CURRENT_SOURCE_DIR}/../cognitive/analysis/nimcp_analysis_substrate_bridge.c  # Analysis-substrate integration
    ${CMAKE_CURRENT_SOURCE_DIR}/../cognitive/emotional_tagging/nimcp_emotional_tagging_substrate_bridge.c  # Emotional-tagging-substrate integration
    ${CMAKE_CURRENT_SOURCE_DIR}/../cognitive/emotion_recognition/nimcp_emotion_recognition_substrate_bridge.c  # Emotion-recognition-substrate integration
    ${CMAKE_CURRENT_SOURCE_DIR}/../cognitive/jepa/nimcp_jepa_substrate_bridge.c  # JEPA-substrate integration
    ${CMAKE_CURRENT_SOURCE_DIR}/../cognitive/symbolic_logic/nimcp_symbolic_logic_substrate_bridge.c  # Symbolic-logic-substrate integration
    ${CMAKE_CURRENT_SOURCE_DIR}/../cognitive/parietal/nimcp_intuition_substrate_bridge.c  # Intuition-substrate integration

    # Cognitive Thalamic Bridges
    ${CMAKE_CURRENT_SOURCE_DIR}/../cognitive/global_workspace/nimcp_gw_thalamic_bridge.c  # GW-thalamic integration
    ${CMAKE_CURRENT_SOURCE_DIR}/../cognitive/ethics/nimcp_ethics_thalamic_bridge.c  # Ethics-thalamic integration
    ${CMAKE_CURRENT_SOURCE_DIR}/../cognitive/knowledge/nimcp_knowledge_thalamic_bridge.c  # Knowledge-thalamic integration
    ${CMAKE_CURRENT_SOURCE_DIR}/../cognitive/logic/nimcp_logic_thalamic_bridge.c  # Logic-thalamic integration

    # Sensory-Logic Bridges (Direct sensory-to-reasoning pathways)
    ${CMAKE_CURRENT_SOURCE_DIR}/../cognitive/logic/nimcp_visual_logic_bridge.c  # Visual perception to symbolic reasoning
    ${CMAKE_CURRENT_SOURCE_DIR}/../cognitive/logic/nimcp_somatosensory_logic_bridge.c  # Tactile/proprioceptive to reasoning
    ${CMAKE_CURRENT_SOURCE_DIR}/../cognitive/logic/nimcp_audio_logic_bridge.c  # Auditory perception to reasoning

    ${CMAKE_CURRENT_SOURCE_DIR}/../cognitive/mirror_neurons/nimcp_mirror_thalamic_bridge.c  # Mirror-thalamic integration
    ${CMAKE_CURRENT_SOURCE_DIR}/../cognitive/mirror_neurons/nimcp_mirror_prefrontal_bridge.c  # Mirror-prefrontal integration (executive control over imitation)
    ${CMAKE_CURRENT_SOURCE_DIR}/../cognitive/mirror_neurons/nimcp_mirror_visual_bridge.c  # Mirror-visual integration (STS pathway, social salience)
    ${CMAKE_CURRENT_SOURCE_DIR}/../cognitive/mirror_neurons/nimcp_mirror_motor_bridge.c  # Mirror-motor integration (F5->M1 imitation execution)
    ${CMAKE_CURRENT_SOURCE_DIR}/../cognitive/mirror_neurons/nimcp_mirror_hippocampus_bridge.c  # Mirror-hippocampus integration (episodic action memory)
    ${CMAKE_CURRENT_SOURCE_DIR}/../cognitive/mirror_neurons/nimcp_mirror_hypothalamus_bridge.c  # Mirror-hypothalamus integration (stress/circadian modulation)
    ${CMAKE_CURRENT_SOURCE_DIR}/../cognitive/mirror_neurons/nimcp_mirror_omni_bridge.c  # Mirror-omni integration (bidirectional cognitive bridge)
    ${CMAKE_CURRENT_SOURCE_DIR}/../cognitive/mirror_neurons/nimcp_mirror_multimodal.c  # Phase 10.11.8: Multi-modal action features
    ${CMAKE_CURRENT_SOURCE_DIR}/../cognitive/mirror_neurons/nimcp_mirror_social_context.c  # Phase 10.11.9: Social context modulation
    ${CMAKE_CURRENT_SOURCE_DIR}/../cognitive/mirror_neurons/nimcp_mirror_tom_bridge.c  # Phase 10.11.10: Mirror-ToM SIMD-optimized integration
    ${CMAKE_CURRENT_SOURCE_DIR}/../cognitive/mirror_neurons/nimcp_mirror_emotion_bridge.c  # Phase 10.11.11: Mirror-Emotion SIMD-optimized integration
    ${CMAKE_CURRENT_SOURCE_DIR}/../cognitive/mirror_neurons/nimcp_mirror_attention_bridge.c  # Phase 10.11.12: Mirror-Attention joint attention integration
    ${CMAKE_CURRENT_SOURCE_DIR}/../cognitive/mirror_neurons/nimcp_mirror_self_other.c  # Phase 10.11.13: Self-Other Distinction
    ${CMAKE_CURRENT_SOURCE_DIR}/../cognitive/mirror_neurons/nimcp_mirror_vicarious_reward.c  # Phase 10.11.14: Vicarious Reward Bridge
    ${CMAKE_CURRENT_SOURCE_DIR}/../cognitive/mirror_neurons/nimcp_mirror_habituation.c  # Phase 10.11.15: Habituation Module
    ${CMAKE_CURRENT_SOURCE_DIR}/../cognitive/mirror_neurons/nimcp_mirror_snn_bridge.c  # SNN integration for mirror neurons
    ${CMAKE_CURRENT_SOURCE_DIR}/../cognitive/mirror_neurons/nimcp_mirror_plasticity_bridge.c  # Plasticity integration for mirror neurons
    # Emotion SNN/Plasticity Integration
    ${CMAKE_CURRENT_SOURCE_DIR}/../cognitive/emotion/nimcp_emotion_snn_bridge.c  # SNN integration for emotion system
    ${CMAKE_CURRENT_SOURCE_DIR}/../cognitive/emotion/nimcp_emotion_plasticity_bridge.c  # Plasticity integration for emotion system
    # Attention SNN/Plasticity Integration
    ${CMAKE_CURRENT_SOURCE_DIR}/../cognitive/attention/nimcp_attention_snn_bridge.c  # SNN integration for attention system
    ${CMAKE_CURRENT_SOURCE_DIR}/../cognitive/attention/nimcp_attention_plasticity_bridge.c  # Plasticity integration for attention system
    # Working Memory SNN/Plasticity Integration
    ${CMAKE_CURRENT_SOURCE_DIR}/../cognitive/memory/nimcp_working_memory_snn_bridge.c  # SNN integration for working memory
    ${CMAKE_CURRENT_SOURCE_DIR}/../cognitive/memory/nimcp_working_memory_plasticity_bridge.c  # Plasticity integration for working memory
    # Autobiographical Memory SNN/Plasticity Integration
    ${CMAKE_CURRENT_SOURCE_DIR}/../cognitive/autobiographical_memory/nimcp_autobio_snn_bridge.c  # SNN integration for autobiographical memory
    ${CMAKE_CURRENT_SOURCE_DIR}/../cognitive/autobiographical_memory/nimcp_autobio_plasticity_bridge.c  # Plasticity integration for autobiographical memory
    # Collective Cognition SNN/Plasticity Integration
    ${CMAKE_CURRENT_SOURCE_DIR}/../cognitive/collective_cognition/nimcp_collective_snn_bridge.c  # SNN integration for collective cognition
    ${CMAKE_CURRENT_SOURCE_DIR}/../cognitive/collective_cognition/nimcp_collective_plasticity_bridge.c  # Plasticity integration for collective cognition
    ${CMAKE_CURRENT_SOURCE_DIR}/../cognitive/collective_cognition/nimcp_collective_fep_bridge.c  # FEP orchestrator integration for collective cognition
    # Consolidation SNN/Plasticity Integration
    ${CMAKE_CURRENT_SOURCE_DIR}/../cognitive/consolidation/nimcp_consolidation_snn_bridge.c  # SNN integration for consolidation
    ${CMAKE_CURRENT_SOURCE_DIR}/../cognitive/consolidation/nimcp_consolidation_plasticity_bridge.c  # Plasticity integration for consolidation
    # Empathetic Response SNN/Plasticity Integration
    ${CMAKE_CURRENT_SOURCE_DIR}/../cognitive/empathetic_response/nimcp_empathy_snn_bridge.c  # SNN integration for empathetic response
    ${CMAKE_CURRENT_SOURCE_DIR}/../cognitive/empathetic_response/nimcp_empathy_plasticity_bridge.c  # Plasticity integration for empathetic response
    # Free Energy SNN/Plasticity Integration
    ${CMAKE_CURRENT_SOURCE_DIR}/../cognitive/free_energy/nimcp_fep_snn_bridge.c  # SNN integration for free energy
    ${CMAKE_CURRENT_SOURCE_DIR}/../cognitive/free_energy/nimcp_fep_plasticity_bridge.c  # Plasticity integration for free energy
    # Game Theory SNN/Plasticity Integration
    ${CMAKE_CURRENT_SOURCE_DIR}/../cognitive/game_theory/nimcp_game_theory_snn_bridge.c  # SNN integration for game theory
    ${CMAKE_CURRENT_SOURCE_DIR}/../cognitive/game_theory/nimcp_game_theory_plasticity_bridge.c  # Plasticity integration for game theory
    # Imagination SNN/Plasticity Integration
    ${CMAKE_CURRENT_SOURCE_DIR}/../cognitive/imagination/nimcp_imagination_snn_bridge.c  # SNN integration for imagination
    ${CMAKE_CURRENT_SOURCE_DIR}/../cognitive/imagination/nimcp_imagination_plasticity_bridge.c  # Plasticity integration for imagination
    # JEPA SNN/Plasticity/FEP Integration
    ${CMAKE_CURRENT_SOURCE_DIR}/../cognitive/jepa/nimcp_jepa_snn_bridge.c  # SNN integration for JEPA
    ${CMAKE_CURRENT_SOURCE_DIR}/../cognitive/jepa/nimcp_jepa_plasticity_bridge.c  # Plasticity integration for JEPA
    ${CMAKE_CURRENT_SOURCE_DIR}/../cognitive/jepa/nimcp_jepa_fep_bridge.c  # FEP orchestrator integration for JEPA
    # Knowledge SNN/Plasticity Integration
    ${CMAKE_CURRENT_SOURCE_DIR}/../cognitive/knowledge/nimcp_knowledge_snn_bridge.c  # SNN integration for knowledge
    ${CMAKE_CURRENT_SOURCE_DIR}/../cognitive/knowledge/nimcp_knowledge_plasticity_bridge.c  # Plasticity integration for knowledge
    ${CMAKE_CURRENT_SOURCE_DIR}/../cognitive/knowledge/nimcp_knowledge_sleep_bridge.c  # Sleep integration for knowledge
    # Mental Health SNN/Plasticity Integration
    ${CMAKE_CURRENT_SOURCE_DIR}/../cognitive/mental_health/nimcp_mental_health_snn_bridge.c  # SNN integration for mental health
    ${CMAKE_CURRENT_SOURCE_DIR}/../cognitive/mental_health/nimcp_mental_health_plasticity_bridge.c  # Plasticity integration for mental health
    ${CMAKE_CURRENT_SOURCE_DIR}/../cognitive/mental_health/nimcp_mental_health_sleep_bridge.c  # Sleep integration for mental health
    # Parietal SNN/Plasticity Integration
    ${CMAKE_CURRENT_SOURCE_DIR}/../cognitive/parietal/nimcp_parietal_snn_bridge.c  # SNN integration for parietal
    ${CMAKE_CURRENT_SOURCE_DIR}/../cognitive/parietal/nimcp_parietal_plasticity_bridge.c  # Plasticity integration for parietal
    ${CMAKE_CURRENT_SOURCE_DIR}/../cognitive/parietal/nimcp_parietal_training_bridge.c  # Training integration for parietal
    # Parietal Linguistics Module
    ${CMAKE_CURRENT_SOURCE_DIR}/../cognitive/parietal/linguistics/nimcp_parietal_spatial_language.c  # Spatial language processing
    ${CMAKE_CURRENT_SOURCE_DIR}/../cognitive/parietal/linguistics/nimcp_parietal_numerical_language.c  # Number word processing
    ${CMAKE_CURRENT_SOURCE_DIR}/../cognitive/parietal/linguistics/nimcp_parietal_phonological_wm.c  # Phonological working memory
    ${CMAKE_CURRENT_SOURCE_DIR}/../cognitive/parietal/linguistics/nimcp_parietal_linguistics_mesh.c  # Mesh coordinator
    ${CMAKE_CURRENT_SOURCE_DIR}/../cognitive/parietal/linguistics/bridges/nimcp_parietal_linguistics_fuzzy_bridge.c  # Fuzzy logic integration
    ${CMAKE_CURRENT_SOURCE_DIR}/../cognitive/parietal/linguistics/bridges/nimcp_parietal_linguistics_statistics_bridge.c  # Statistics/HMM integration
    ${CMAKE_CURRENT_SOURCE_DIR}/../cognitive/parietal/linguistics/bridges/nimcp_parietal_linguistics_snn_bridge.c  # SNN integration for linguistics
    ${CMAKE_CURRENT_SOURCE_DIR}/../cognitive/parietal/linguistics/bridges/nimcp_parietal_linguistics_plasticity_bridge.c  # Plasticity integration for linguistics
    # Personality SNN/Plasticity Integration
    ${CMAKE_CURRENT_SOURCE_DIR}/../cognitive/personality/nimcp_personality_snn_bridge.c  # SNN integration for personality
    ${CMAKE_CURRENT_SOURCE_DIR}/../cognitive/personality/nimcp_personality_plasticity_bridge.c  # Plasticity integration for personality
    # Predictive SNN/Plasticity Integration
    ${CMAKE_CURRENT_SOURCE_DIR}/../cognitive/predictive/nimcp_predictive_snn_bridge.c  # SNN integration for predictive
    ${CMAKE_CURRENT_SOURCE_DIR}/../cognitive/predictive/nimcp_predictive_plasticity_bridge.c  # Plasticity integration for predictive
    # Recursive Cognition SNN/Plasticity/FEP Integration
    ${CMAKE_CURRENT_SOURCE_DIR}/../cognitive/recursive/nimcp_rcog_snn_bridge.c  # SNN integration for recursive cognition
    ${CMAKE_CURRENT_SOURCE_DIR}/../cognitive/recursive/nimcp_rcog_plasticity_bridge.c  # Plasticity integration for recursive cognition
    ${CMAKE_CURRENT_SOURCE_DIR}/../cognitive/recursive/nimcp_rcog_fep_bridge.c  # FEP orchestrator integration for recursive cognition
    # Self-Awareness SNN/Plasticity Integration
    ${CMAKE_CURRENT_SOURCE_DIR}/../cognitive/self_awareness/nimcp_self_awareness_snn_bridge.c  # SNN integration for self-awareness
    ${CMAKE_CURRENT_SOURCE_DIR}/../cognitive/self_awareness/nimcp_self_awareness_plasticity_bridge.c  # Plasticity integration for self-awareness
    # Shadow SNN/Plasticity Integration
    ${CMAKE_CURRENT_SOURCE_DIR}/../cognitive/shadow/nimcp_shadow_snn_bridge.c  # SNN integration for shadow
    ${CMAKE_CURRENT_SOURCE_DIR}/../cognitive/shadow/nimcp_shadow_plasticity_bridge.c  # Plasticity integration for shadow
    # Sleep-Wake SNN/Plasticity Integration
    ${CMAKE_CURRENT_SOURCE_DIR}/../cognitive/sleep_wake/nimcp_sleep_wake_snn_bridge.c  # SNN integration for sleep-wake
    ${CMAKE_CURRENT_SOURCE_DIR}/../cognitive/sleep_wake/nimcp_sleep_wake_plasticity_bridge.c  # Plasticity integration for sleep-wake
    # Social SNN/Plasticity/FEP Integration
    ${CMAKE_CURRENT_SOURCE_DIR}/../cognitive/social/nimcp_social_snn_bridge.c  # SNN integration for social
    ${CMAKE_CURRENT_SOURCE_DIR}/../cognitive/social/nimcp_social_plasticity_bridge.c  # Plasticity integration for social
    ${CMAKE_CURRENT_SOURCE_DIR}/../cognitive/social/nimcp_social_fep_bridge.c  # FEP orchestrator integration for social
    # Core SNN sources (needed by mirror_snn_bridge and other SNN integrations)
    ${CMAKE_CURRENT_SOURCE_DIR}/../snn/nimcp_snn_config.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../snn/nimcp_snn_network.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../snn/nimcp_snn_hierarchical.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../snn/nimcp_snn_synapse.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../snn/nimcp_snn_encoding.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../snn/nimcp_snn_training.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../snn/nimcp_snn_adaptation.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../snn/nimcp_snn_bio_async.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../snn/nimcp_snn_immune.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../snn/nimcp_snn_ternary.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../snn/nimcp_snn_basket.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../snn/batch_safe/nimcp_snn_batch_safe.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../snn/bridges/nimcp_snn_routing_bridge.c  # SNN routing bridge for cross-region spike routing
    ${CMAKE_CURRENT_SOURCE_DIR}/../snn/bridges/nimcp_snn_language_bridge.c  # SNN language bridge for spike-driven word-concept binding
    ${CMAKE_CURRENT_SOURCE_DIR}/../snn/bridges/nimcp_snn_visual_bridge.c  # SNN visual bridge for spike-driven visual processing
    ${CMAKE_CURRENT_SOURCE_DIR}/../snn/bridges/nimcp_snn_speech_bridge.c  # SNN speech bridge for spike-driven speech processing
    ${CMAKE_CURRENT_SOURCE_DIR}/../snn/bridges/nimcp_snn_audio_bridge.c  # SNN audio bridge for spike-driven auditory processing
    ${CMAKE_CURRENT_SOURCE_DIR}/../snn/bridges/nimcp_snn_somatosensory_bridge.c  # SNN somatosensory bridge for body-state spike encoding
    ${CMAKE_CURRENT_SOURCE_DIR}/../snn/bridges/nimcp_snn_cross_modal_align.c  # Cross-modal temporal alignment for multi-sensory spike sync
    ${CMAKE_CURRENT_SOURCE_DIR}/../cognitive/personality/nimcp_personality_thalamic_bridge.c  # Personality-thalamic integration
    ${CMAKE_CURRENT_SOURCE_DIR}/../cognitive/self_awareness/nimcp_self_awareness_thalamic_bridge.c  # Self-awareness-thalamic integration
    ${CMAKE_CURRENT_SOURCE_DIR}/../cognitive/predictive/nimcp_predictive_thalamic_bridge.c  # Predictive-thalamic integration
    ${CMAKE_CURRENT_SOURCE_DIR}/../cognitive/curiosity/nimcp_curiosity_thalamic_bridge.c  # Curiosity-thalamic integration
    ${CMAKE_CURRENT_SOURCE_DIR}/../cognitive/salience/nimcp_salience_thalamic_bridge.c  # Salience-thalamic integration
    ${CMAKE_CURRENT_SOURCE_DIR}/../cognitive/emotion_recognition/nimcp_emotion_recognition_thalamic_bridge.c  # Emotion-recognition-thalamic integration
    ${CMAKE_CURRENT_SOURCE_DIR}/../cognitive/autobiographical_memory/nimcp_autobio_thalamic_bridge.c  # Autobio-thalamic integration
    ${CMAKE_CURRENT_SOURCE_DIR}/../cognitive/autobiographical_memory/nimcp_autobiographical_memory_sleep_bridge.c  # Autobio-sleep integration
    ${CMAKE_CURRENT_SOURCE_DIR}/../cognitive/sleep_wake/nimcp_sleep_wake_thalamic_bridge.c  # Sleep-wake-thalamic integration
    ${CMAKE_CURRENT_SOURCE_DIR}/../cognitive/meta_learning/nimcp_meta_learning_thalamic_bridge.c  # Meta-learning-thalamic integration
    ${CMAKE_CURRENT_SOURCE_DIR}/../cognitive/jepa/nimcp_jepa_thalamic_bridge.c  # JEPA-thalamic integration
    ${CMAKE_CURRENT_SOURCE_DIR}/../cognitive/bias/nimcp_bias_thalamic_bridge.c  # Bias-thalamic integration
    ${CMAKE_CURRENT_SOURCE_DIR}/../cognitive/grief/nimcp_grief_thalamic_bridge.c  # Grief-thalamic integration
    ${CMAKE_CURRENT_SOURCE_DIR}/../cognitive/joy/nimcp_joy_thalamic_bridge.c  # Joy-thalamic integration
    ${CMAKE_CURRENT_SOURCE_DIR}/../cognitive/love_loyalty_friendship/nimcp_llf_thalamic_bridge.c  # LLF-thalamic integration
    ${CMAKE_CURRENT_SOURCE_DIR}/../cognitive/remorse/nimcp_remorse_thalamic_bridge.c  # Remorse-thalamic integration
    ${CMAKE_CURRENT_SOURCE_DIR}/../cognitive/shadow/nimcp_shadow_thalamic_bridge.c  # Shadow-thalamic integration
    ${CMAKE_CURRENT_SOURCE_DIR}/../cognitive/mental_health/nimcp_mental_health_thalamic_bridge.c  # Mental-health-thalamic integration
    ${CMAKE_CURRENT_SOURCE_DIR}/../cognitive/parietal/nimcp_intuition_thalamic_bridge.c  # Intuition-thalamic integration

    # Additional Cognitive Substrate Bridges (Phase 7: Complete Coverage)
    # Note: nimcp_memory_consolidation_substrate_bridge.c removed - using consolidation version (more complete API)
    ${CMAKE_CURRENT_SOURCE_DIR}/../cognitive/emotion_tensor/nimcp_emotion_tensor_substrate_bridge.c  # Emotion-tensor-substrate integration
    ${CMAKE_CURRENT_SOURCE_DIR}/../cognitive/empathetic_response/nimcp_empathetic_response_substrate_bridge.c  # Empathetic-response-substrate integration
    ${CMAKE_CURRENT_SOURCE_DIR}/../cognitive/fault_tolerance/nimcp_fault_tolerance_substrate_bridge.c  # Fault-tolerance-substrate integration
    ${CMAKE_CURRENT_SOURCE_DIR}/../cognitive/fractal_cognitive/nimcp_fractal_cognitive_substrate_bridge.c  # Fractal-cognitive-substrate integration
    ${CMAKE_CURRENT_SOURCE_DIR}/../cognitive/free_energy/nimcp_free_energy_substrate_bridge.c  # Free-energy-substrate integration
    ${CMAKE_CURRENT_SOURCE_DIR}/../cognitive/game_theory/nimcp_game_theory_substrate_bridge.c  # Game-theory-substrate integration
    ${CMAKE_CURRENT_SOURCE_DIR}/../cognitive/immune/nimcp_brain_immune_substrate_bridge.c  # Brain-immune-substrate integration
    ${CMAKE_CURRENT_SOURCE_DIR}/../cognitive/social/nimcp_social_substrate_bridge.c  # Social-substrate integration
    # Note: ToM-substrate bridge now unified in cognitive/tom/nimcp_tom_substrate_bridge.c (line 899)
    ${CMAKE_CURRENT_SOURCE_DIR}/../cognitive/predictive_immune/nimcp_predictive_immune_substrate_bridge.c  # Predictive-immune-substrate integration
    ${CMAKE_CURRENT_SOURCE_DIR}/../cognitive/self_awareness_extended/nimcp_self_awareness_extended_substrate_bridge.c  # Self-awareness-ext-substrate integration

    # Collective Cognition (Distributed Consciousness)
    ${CMAKE_CURRENT_SOURCE_DIR}/../cognitive/collective_cognition/nimcp_collective_cognition.c        # Main collective cognition system
    ${CMAKE_CURRENT_SOURCE_DIR}/../cognitive/collective_cognition/nimcp_hyperscanning.c              # Inter-brain neural synchronization
    ${CMAKE_CURRENT_SOURCE_DIR}/../cognitive/collective_cognition/nimcp_extended_mind.c              # External tools as cognitive extensions
    ${CMAKE_CURRENT_SOURCE_DIR}/../cognitive/collective_cognition/nimcp_collective_phi.c             # IIT metrics for collective consciousness
    ${CMAKE_CURRENT_SOURCE_DIR}/../cognitive/collective_cognition/nimcp_shared_intentionality.c      # Joint goals and we-mode cognition
    ${CMAKE_CURRENT_SOURCE_DIR}/../cognitive/collective_cognition/nimcp_collective_cognition_immune_bridge.c  # Collective-immune integration

    # Phase 8: Cognitive Health Integration (Collective & Recursive Cognition for Health)
    ${CMAKE_CURRENT_SOURCE_DIR}/../cognitive/health/nimcp_collective_health.c        # Distributed health monitoring across brain instances
    ${CMAKE_CURRENT_SOURCE_DIR}/../cognitive/health/nimcp_rcog_health.c              # RCOG-powered intelligent diagnosis and recovery
    ${CMAKE_CURRENT_SOURCE_DIR}/../cognitive/health/nimcp_meta_health.c              # Meta-health self-reflection and improvement
    ${CMAKE_CURRENT_SOURCE_DIR}/../cognitive/health/nimcp_health_cognitive_bridge.c  # Unified cognitive bridge for health agent

    # Additional Cognitive Thalamic Bridges (Phase 7: Complete Coverage)
    ${CMAKE_CURRENT_SOURCE_DIR}/../cognitive/consolidation/nimcp_consolidation_thalamic_bridge.c  # Consolidation-thalamic integration
    ${CMAKE_CURRENT_SOURCE_DIR}/../cognitive/emotion_tensor/nimcp_emotion_tensor_thalamic_bridge.c  # Emotion-tensor-thalamic integration
    ${CMAKE_CURRENT_SOURCE_DIR}/../cognitive/empathetic_response/nimcp_empathetic_response_thalamic_bridge.c  # Empathetic-response-thalamic integration
    ${CMAKE_CURRENT_SOURCE_DIR}/../cognitive/fault_tolerance/nimcp_fault_tolerance_thalamic_bridge.c  # Fault-tolerance-thalamic integration
    ${CMAKE_CURRENT_SOURCE_DIR}/../cognitive/fractal_cognitive/nimcp_fractal_cognitive_thalamic_bridge.c  # Fractal-cognitive-thalamic integration
    ${CMAKE_CURRENT_SOURCE_DIR}/../cognitive/free_energy/nimcp_free_energy_thalamic_bridge.c  # Free-energy-thalamic integration
    ${CMAKE_CURRENT_SOURCE_DIR}/../cognitive/game_theory/nimcp_game_theory_thalamic_bridge.c  # Game-theory-thalamic integration
    ${CMAKE_CURRENT_SOURCE_DIR}/../cognitive/immune/nimcp_brain_immune_thalamic_bridge.c  # Brain-immune-thalamic integration
    ${CMAKE_CURRENT_SOURCE_DIR}/../cognitive/social/nimcp_social_thalamic_bridge.c  # Social-thalamic integration
    ${CMAKE_CURRENT_SOURCE_DIR}/../cognitive/tom/nimcp_tom_thalamic_bridge.c  # ToM-thalamic integration (with route_inference API)
    ${CMAKE_CURRENT_SOURCE_DIR}/../cognitive/predictive_immune/nimcp_predictive_immune_thalamic_bridge.c  # Predictive-immune-thalamic integration
    ${CMAKE_CURRENT_SOURCE_DIR}/../cognitive/self_awareness_extended/nimcp_self_awareness_extended_thalamic_bridge.c  # Self-awareness-ext-thalamic integration
    ${CMAKE_CURRENT_SOURCE_DIR}/../cognitive/analysis/nimcp_analysis_thalamic_bridge.c  # Analysis-thalamic integration
    ${CMAKE_CURRENT_SOURCE_DIR}/../cognitive/attention/nimcp_attention_thalamic_bridge.c  # Attention-thalamic integration
    ${CMAKE_CURRENT_SOURCE_DIR}/../cognitive/emotion/nimcp_emotion_thalamic_bridge.c  # Emotion-thalamic integration
    ${CMAKE_CURRENT_SOURCE_DIR}/../cognitive/executive/nimcp_executive_thalamic_bridge.c  # Executive-thalamic integration
    ${CMAKE_CURRENT_SOURCE_DIR}/../cognitive/memory/nimcp_memory_thalamic_bridge.c  # Memory-thalamic integration
    ${CMAKE_CURRENT_SOURCE_DIR}/../cognitive/reasoning/nimcp_reasoning_thalamic_bridge.c  # Reasoning-thalamic integration
    ${CMAKE_CURRENT_SOURCE_DIR}/../cognitive/working_memory/nimcp_working_memory_thalamic_bridge.c  # Working-memory-thalamic integration
    ${CMAKE_CURRENT_SOURCE_DIR}/../cognitive/wellbeing/nimcp_wellbeing_thalamic_bridge.c  # Wellbeing-thalamic integration
    ${CMAKE_CURRENT_SOURCE_DIR}/../cognitive/introspection/nimcp_introspection_thalamic_bridge.c  # Introspection-thalamic integration
    ${CMAKE_CURRENT_SOURCE_DIR}/../cognitive/self_model/nimcp_self_model_thalamic_bridge.c  # Self-model-thalamic integration
    ${CMAKE_CURRENT_SOURCE_DIR}/../cognitive/epistemic/nimcp_epistemic_thalamic_bridge.c  # Epistemic-thalamic integration
    ${CMAKE_CURRENT_SOURCE_DIR}/../cognitive/symbolic_logic/nimcp_symbolic_logic_thalamic_bridge.c  # Symbolic-logic-thalamic integration
    ${CMAKE_CURRENT_SOURCE_DIR}/../cognitive/shadow_emotions/nimcp_shadow_emotions_thalamic_bridge.c  # Shadow-emotions-thalamic integration
    ${CMAKE_CURRENT_SOURCE_DIR}/../cognitive/emotional_tagging/nimcp_emotional_tagging_thalamic_bridge.c  # Emotional-tagging-thalamic integration
    ${CMAKE_CURRENT_SOURCE_DIR}/../cognitive/explanations/nimcp_explanations_thalamic_bridge.c  # Explanations-thalamic integration
    # Note: ToM thalamic bridge already included via theory_of_mind

    # Imagination Engine - Generative mental simulation (Phase IMAGINATION)
    ${CMAKE_CURRENT_SOURCE_DIR}/../cognitive/imagination/nimcp_imagination_workspace.c  # Workspace for active scenarios
    ${CMAKE_CURRENT_SOURCE_DIR}/../cognitive/imagination/nimcp_imagination_engine.c     # Main engine with all integrations
    ${CMAKE_CURRENT_SOURCE_DIR}/../cognitive/imagination/nimcp_hippocampus_imagination_bridge.c  # Hippocampus-imagination bridge
    ${CMAKE_CURRENT_SOURCE_DIR}/../cognitive/imagination/nimcp_jepa_imagination_bridge.c  # JEPA-imagination bridge
    ${CMAKE_CURRENT_SOURCE_DIR}/../cognitive/imagination/nimcp_prefrontal_imagination_bridge.c  # Prefrontal-imagination bridge
    ${CMAKE_CURRENT_SOURCE_DIR}/../cognitive/imagination/nimcp_gw_imagination_bridge.c  # Global workspace-imagination bridge
    ${CMAKE_CURRENT_SOURCE_DIR}/../cognitive/imagination/nimcp_sleep_imagination_bridge.c  # Sleep-imagination bridge
    ${CMAKE_CURRENT_SOURCE_DIR}/../cognitive/imagination/nimcp_imagination_fep_bridge.c  # FEP orchestrator integration bridge

    # Variational Autoencoder (VAE) - Cognitive Generative Model (Phase VAE)
    ${CMAKE_CURRENT_SOURCE_DIR}/../cognitive/vae/nimcp_vae.c                              # Core VAE system
    ${CMAKE_CURRENT_SOURCE_DIR}/../cognitive/vae/nimcp_vae_encoder.c                      # VAE encoder network
    ${CMAKE_CURRENT_SOURCE_DIR}/../cognitive/vae/nimcp_vae_decoder.c                      # VAE decoder network
    ${CMAKE_CURRENT_SOURCE_DIR}/../cognitive/vae/nimcp_vae_latent.c                       # Latent space operations
    ${CMAKE_CURRENT_SOURCE_DIR}/../cognitive/vae/nimcp_vae_loss.c                         # VAE loss functions (ELBO, KL divergence)
    # VAE System Bridges (Phase VAE-3)
    ${CMAKE_CURRENT_SOURCE_DIR}/../cognitive/vae/bridges/nimcp_vae_fep_bridge.c           # VAE-FEP integration (latent-belief mapping)
    ${CMAKE_CURRENT_SOURCE_DIR}/../cognitive/vae/bridges/nimcp_vae_immune_bridge.c        # VAE-Immune integration (anomaly detection)
    ${CMAKE_CURRENT_SOURCE_DIR}/../cognitive/vae/bridges/nimcp_vae_bbb_bridge.c           # VAE-BBB integration (security validation)
    ${CMAKE_CURRENT_SOURCE_DIR}/../cognitive/vae/bridges/nimcp_vae_logging_bridge.c       # VAE-Logging integration (operational logging)
    # VAE Cognitive Bridges (Phase VAE-4)
    ${CMAKE_CURRENT_SOURCE_DIR}/../cognitive/vae/bridges/nimcp_vae_hippocampus_bridge.c   # VAE-Hippocampus (memory encoding/retrieval)
    ${CMAKE_CURRENT_SOURCE_DIR}/../cognitive/vae/bridges/nimcp_vae_imagination_bridge.c   # VAE-Imagination (generative simulation, quantum sampling)
    ${CMAKE_CURRENT_SOURCE_DIR}/../cognitive/vae/bridges/nimcp_vae_visual_bridge.c        # VAE-Visual (V1-V5 hierarchy encoding)
    ${CMAKE_CURRENT_SOURCE_DIR}/../cognitive/vae/bridges/nimcp_vae_auditory_bridge.c      # VAE-Auditory (mel/MFCC feature encoding)
    ${CMAKE_CURRENT_SOURCE_DIR}/../cognitive/vae/bridges/nimcp_vae_emotion_bridge.c       # VAE-Emotion (affect modulation)
    ${CMAKE_CURRENT_SOURCE_DIR}/../cognitive/vae/bridges/nimcp_vae_introspection_bridge.c # VAE-Introspection (internal state encoding)
    ${CMAKE_CURRENT_SOURCE_DIR}/../cognitive/vae/bridges/nimcp_vae_world_model_bridge.c   # VAE-World Model (JEPA latent prediction)
    # Phase 5: Neural Integration Bridges
    ${CMAKE_CURRENT_SOURCE_DIR}/../cognitive/vae/bridges/nimcp_vae_snn_bridge.c          # VAE-SNN (latent-spike conversion)
    ${CMAKE_CURRENT_SOURCE_DIR}/../cognitive/vae/bridges/nimcp_vae_plasticity_bridge.c   # VAE-Plasticity (learning modulation)
    ${CMAKE_CURRENT_SOURCE_DIR}/../cognitive/vae/bridges/nimcp_vae_training_bridge.c     # VAE-Training (joint VAE+SNN training)
    ${CMAKE_CURRENT_SOURCE_DIR}/../cognitive/vae/bridges/nimcp_vae_substrate_bridge.c    # VAE-Substrate (metabolic-aware encoding)
    ${CMAKE_CURRENT_SOURCE_DIR}/../cognitive/vae/bridges/nimcp_vae_thalamic_bridge.c     # VAE-Thalamic (attention-gated relay)
    # Phase 6: Bio-Async Integration
    ${CMAKE_CURRENT_SOURCE_DIR}/../cognitive/vae/nimcp_vae_bio_async.c                  # VAE-BioAsync (message handling)

    # Glial cells
    ${CMAKE_CURRENT_SOURCE_DIR}/../glial/astrocytes/nimcp_astrocytes.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../glial/astrocytes/nimcp_astrocyte_calcium.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../glial/astrocyte_types/nimcp_astrocyte_types.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../glial/oligodendrocytes/nimcp_oligodendrocytes.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../glial/microglia/nimcp_microglia.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../glial/myelin_sheath/nimcp_myelin_sheath.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../glial/myelin_sheath/nimcp_myelin_math.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../glial/integration/nimcp_glial_integration.c

    # Networking
    ${CMAKE_CURRENT_SOURCE_DIR}/../networking/p2p/nimcp_p2pnode.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../networking/protocol/nimcp_protocol.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../networking/protocol/nimcp_msg_framing.c      # Collective cognition message framing
    ${CMAKE_CURRENT_SOURCE_DIR}/../networking/protocol/nimcp_msg_router.c       # Collective cognition message routing
    ${CMAKE_CURRENT_SOURCE_DIR}/../networking/events/nimcp_events.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../networking/replication/nimcp_replication.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../networking/distributed/nimcp_distributed_cognition.c

    # Neural Link Protocol (NLP) - Brain-to-Brain Communication
    ${CMAKE_CURRENT_SOURCE_DIR}/../networking/nlp/nimcp_nlp.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../networking/nlp/nimcp_nlp_crypto.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../networking/nlp/nimcp_nlp_session.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../networking/nlp/nimcp_nlp_message.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../networking/nlp/nimcp_nlp_cortical_adapter.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../networking/nlp/nimcp_neural_language.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../networking/nlp/nimcp_nlp_compression.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../networking/nlp/nimcp_nlp_protocol_bridge.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../networking/nlp/nimcp_dialect_learning.c          # Neural protocol dialect learning

    # I/O
    ${CMAKE_CURRENT_SOURCE_DIR}/../io/dataio/nimcp_dataio.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../io/stream/nimcp_stream.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../io/serialization/nimcp_serialization.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../io/serialization/nimcp_network_serialization.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../io/serialization/nimcp_encryption.c
    # Utils Serialization - Compression/encryption utilities (compress, encrypt, ctx)
    ${CMAKE_CURRENT_SOURCE_DIR}/../utils/serialization/nimcp_serialization.c
    # Model Loading - Pre-trained model loading and validation (Phase ML-1)
    # ${CMAKE_CURRENT_SOURCE_DIR}/../io/model/nimcp_model_loader.c  # DISABLED: adaptive_neuron_t type issues

    # Security - Core and Phase SC-1: Security Coverage Framework (Tier 0.7)
    ${CMAKE_CURRENT_SOURCE_DIR}/../security/nimcp_toxicity.c           # Pattern-based content classifier (populates ethics/LGSS gates)
    ${CMAKE_CURRENT_SOURCE_DIR}/../security/nimcp_toxicity_ml.c        # ML toxicity head (1024->256->64->2 MLP)
    ${CMAKE_CURRENT_SOURCE_DIR}/../security/nimcp_toxicity_response.c  # Counterclaim/anti-frame pushback engine
    ${CMAKE_CURRENT_SOURCE_DIR}/../security/nimcp_security.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../security/nimcp_security_coverage.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../security/nimcp_continuous_monitor.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../security/nimcp_cfi.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../security/nimcp_shadow_stack.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../security/nimcp_capability.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../security/nimcp_security_fractal.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../security/nimcp_security_audit.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../security/nimcp_security_recovery_bridge.c  # Phase SC-2: Security-Fault Tolerance Integration
    ${CMAKE_CURRENT_SOURCE_DIR}/../security/nimcp_security_math.c  # Phase SC-3: Mathematical Security (Entropy, Bayesian Trust, DP)
    ${CMAKE_CURRENT_SOURCE_DIR}/../security/nimcp_security_integration.c  # Phase SC-4: Universal Security Integration Framework
    # Blood-Brain Barrier (BBB) - Neural Immune System Perimeter Defense (Phase IS-1)
    ${CMAKE_CURRENT_SOURCE_DIR}/../security/nimcp_blood_brain_barrier.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../security/nimcp_bbb_helpers.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../security/nimcp_bbb_input_gate.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../security/nimcp_bbb_code_signing.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../security/nimcp_bbb_memory_boundary.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../security/nimcp_bbb_access_control.c

    # Security Phase SC-5: Enhanced Security Framework
    # TOCTOU Guards + Rate Limiting
    ${CMAKE_CURRENT_SOURCE_DIR}/../security/nimcp_toctou_guard.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../security/nimcp_rate_limiter.c
    # Constant-Time Crypto + Key Derivation
    ${CMAKE_CURRENT_SOURCE_DIR}/../security/nimcp_constant_time.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../security/nimcp_key_derivation.c
    # Path Traversal + Shell Command Detection
    ${CMAKE_CURRENT_SOURCE_DIR}/../security/nimcp_path_traversal.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../security/nimcp_shell_detector.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../security/nimcp_bbb_enhanced_detection.c
    # ML-Based Anomaly Detection
    ${CMAKE_CURRENT_SOURCE_DIR}/../security/nimcp_anomaly_detector.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../security/nimcp_bayesian_network.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../security/nimcp_feature_extractor.c
    # Dynamic Pattern Database + Encrypted Audit
    ${CMAKE_CURRENT_SOURCE_DIR}/../security/nimcp_pattern_db.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../security/nimcp_pattern_compiler.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../security/nimcp_encrypted_audit.c
    # Tamper-Resistant Audit Logging (always-on safety subsystem)
    ${CMAKE_CURRENT_SOURCE_DIR}/../security/nimcp_audit_log.c
    # Security Level Management + Distributed Consensus
    ${CMAKE_CURRENT_SOURCE_DIR}/../security/nimcp_security_level.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../security/nimcp_consensus_protocol.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../security/nimcp_security_consensus.c
    # Policy Language Engine
    ${CMAKE_CURRENT_SOURCE_DIR}/../security/nimcp_policy_ast.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../security/nimcp_policy_parser.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../security/nimcp_policy_compiler.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../security/nimcp_policy_evaluator.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../security/nimcp_policy_engine.c
    # Post-Quantum Cryptography
    ${CMAKE_CURRENT_SOURCE_DIR}/../security/nimcp_post_quantum.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../security/nimcp_pq_kyber.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../security/nimcp_pq_dilithium.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../security/nimcp_pq_hybrid.c
    # Supply Chain Security
    ${CMAKE_CURRENT_SOURCE_DIR}/../security/nimcp_supply_chain.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../security/nimcp_sbom.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../security/nimcp_artifact_verify.c

    # Security Immune Integration (Phase BII-S: Security immune bridges)
    ${CMAKE_CURRENT_SOURCE_DIR}/../security/immune/nimcp_anomaly_immune_bridge.c     # Anomaly detector-immune integration
    ${CMAKE_CURRENT_SOURCE_DIR}/../security/immune/nimcp_rate_limiter_immune_bridge.c # Rate limiter-immune integration
    ${CMAKE_CURRENT_SOURCE_DIR}/../security/immune/nimcp_pattern_db_immune_bridge.c  # Pattern DB-immune integration
    ${CMAKE_CURRENT_SOURCE_DIR}/../security/immune/nimcp_security_immune_unified_bridge.c  # Unified security-immune bidirectional bridge
    ${CMAKE_CURRENT_SOURCE_DIR}/../security/nimcp_security_perception_bridge.c  # Security-Perception Bridge - sensory threat analysis and defense

    # Security Module Bridges (Phase SMB: Security module integration bridges)
    ${CMAKE_CURRENT_SOURCE_DIR}/../security/perception/nimcp_security_perception_input_bridge.c  # Security-Perception Input Bridge
    ${CMAKE_CURRENT_SOURCE_DIR}/../security/language/nimcp_security_language_bridge.c          # Security-Language Bridge
    ${CMAKE_CURRENT_SOURCE_DIR}/../security/executive/nimcp_security_executive_bridge.c        # Security-Executive Bridge
    ${CMAKE_CURRENT_SOURCE_DIR}/../security/memory/nimcp_security_memory_bridge.c              # Security-Memory Bridge
    ${CMAKE_CURRENT_SOURCE_DIR}/../security/training/nimcp_security_training_bridge.c          # Security-Training Bridge
    ${CMAKE_CURRENT_SOURCE_DIR}/../security/rcog/nimcp_security_rcog_bridge.c                  # Security-RCog Bridge

    # Security Cognitive Bridges (Phase SCB: Cognitive module security integration)
    ${CMAKE_CURRENT_SOURCE_DIR}/../security/distributed/nimcp_security_distributed_training_bridge.c  # Security-Distributed Training Bridge
    ${CMAKE_CURRENT_SOURCE_DIR}/../security/knowledge/nimcp_security_knowledge_graph_bridge.c         # Security-Knowledge Graph Bridge
    ${CMAKE_CURRENT_SOURCE_DIR}/../security/game_theory/nimcp_security_game_theory_bridge.c           # Security-Game Theory Bridge
    ${CMAKE_CURRENT_SOURCE_DIR}/../security/imagination/nimcp_security_imagination_bridge.c           # Security-Imagination Bridge
    ${CMAKE_CURRENT_SOURCE_DIR}/../security/continual/nimcp_security_continual_learning_bridge.c      # Security-Continual Learning Bridge
    ${CMAKE_CURRENT_SOURCE_DIR}/../security/epistemic/nimcp_security_epistemic_bridge.c               # Security-Epistemic Bridge
    ${CMAKE_CURRENT_SOURCE_DIR}/../security/collective/nimcp_security_collective_bridge.c             # Security-Collective Cognition Bridge
    ${CMAKE_CURRENT_SOURCE_DIR}/../security/hippocampus/nimcp_security_hippocampus_bridge.c           # Security-Hippocampus Bridge

    # Security Orchestrator (Phase SO: Central security coordination)
    ${CMAKE_CURRENT_SOURCE_DIR}/../security/nimcp_security_orchestrator.c                             # Security Orchestrator - central bridge coordination

    # Security Async Integration (Phase SA: Security async bridges)
    ${CMAKE_CURRENT_SOURCE_DIR}/../security/async/nimcp_security_async_bridge.c  # Security-async bidirectional bridge

    # Security Logging Integration (Phase SL: Security logging bridges)
    ${CMAKE_CURRENT_SOURCE_DIR}/../security/logging/nimcp_security_logging_bridge.c  # Security-logging bidirectional bridge

    # Security Sleep Integration (Phase SB-S: Security sleep bridges)
    ${CMAKE_CURRENT_SOURCE_DIR}/../security/sleep/nimcp_bbb_sleep_bridge.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../security/sleep/nimcp_anomaly_detector_sleep_bridge.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../security/sleep/nimcp_rate_limiter_sleep_bridge.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../security/sleep/nimcp_pattern_db_sleep_bridge.c

    # Unified Security Facade - High-level API for entire security subsystem
    ${CMAKE_CURRENT_SOURCE_DIR}/../security/nimcp_security_facade.c

    # Security FEP Bridges (Phase SF: Security-FEP integration for cognitive bridges)
    ${CMAKE_CURRENT_SOURCE_DIR}/../security/distributed/nimcp_security_distributed_training_fep_bridge.c   # Distributed Training-FEP bridge
    ${CMAKE_CURRENT_SOURCE_DIR}/../security/knowledge_graph/nimcp_security_kg_fep_bridge.c                 # Knowledge Graph-FEP bridge
    ${CMAKE_CURRENT_SOURCE_DIR}/../security/game_theory/nimcp_security_game_theory_fep_bridge.c            # Game Theory-FEP bridge
    ${CMAKE_CURRENT_SOURCE_DIR}/../security/imagination/nimcp_security_imagination_fep_bridge.c            # Imagination-FEP bridge
    ${CMAKE_CURRENT_SOURCE_DIR}/../security/continual_learning/nimcp_security_continual_learning_fep_bridge.c  # Continual Learning-FEP bridge
    ${CMAKE_CURRENT_SOURCE_DIR}/../security/epistemic/nimcp_security_epistemic_fep_bridge.c                # Epistemic-FEP bridge
    ${CMAKE_CURRENT_SOURCE_DIR}/../security/collective/nimcp_security_collective_fep_bridge.c              # Collective Cognition-FEP bridge
    ${CMAKE_CURRENT_SOURCE_DIR}/../security/hippocampus/nimcp_security_hippocampus_fep_bridge.c            # Hippocampus-FEP bridge
    ${CMAKE_CURRENT_SOURCE_DIR}/../security/language/nimcp_security_language_fep_bridge.c                  # Language-FEP bridge
    ${CMAKE_CURRENT_SOURCE_DIR}/../security/async/nimcp_security_async_fep_bridge.c                        # Async-FEP bridge
    ${CMAKE_CURRENT_SOURCE_DIR}/../security/logging/nimcp_security_logging_fep_bridge.c                    # Logging-FEP bridge
    ${CMAKE_CURRENT_SOURCE_DIR}/../security/immune/nimcp_security_immune_fep_bridge.c                      # Immune-FEP bridge
    ${CMAKE_CURRENT_SOURCE_DIR}/../security/training/nimcp_security_training_fep_bridge.c                  # Training-FEP bridge
    ${CMAKE_CURRENT_SOURCE_DIR}/../security/memory/nimcp_security_memory_fep_bridge.c                      # Memory-FEP bridge

    # LGSS (Layered Governance Safety System) - Core safety enforcement
    # A1-A2: Safety KB + Action Interceptor
    ${CMAKE_CURRENT_SOURCE_DIR}/../security/lgss/nimcp_lgss.c                                              # LGSS unified context
    ${CMAKE_CURRENT_SOURCE_DIR}/../security/lgss/nimcp_lgss_action_interceptor.c                           # Action interceptor
    ${CMAKE_CURRENT_SOURCE_DIR}/../security/lgss/nimcp_lgss_override_controller.c                          # Override controller
    ${CMAKE_CURRENT_SOURCE_DIR}/../security/lgss/nimcp_lgss_enhanced.c                                     # LGSS Enhanced (8 governance improvements)
    ${CMAKE_CURRENT_SOURCE_DIR}/../security/lgss/nimcp_lgss_telemetry.c                                    # Hash-chained telemetry
    # LGSS Bridges (A3-A6)
    ${CMAKE_CURRENT_SOURCE_DIR}/../security/lgss/bridges/nimcp_lgss_ethics_bridge.c                        # Ethics-LGSS integration
    ${CMAKE_CURRENT_SOURCE_DIR}/../security/lgss/bridges/nimcp_lgss_bio_async_bridge.c                     # Bio-async messaging bridge
    ${CMAKE_CURRENT_SOURCE_DIR}/../security/lgss/bridges/nimcp_lgss_executive_bridge.c                     # Executive function bridge
    ${CMAKE_CURRENT_SOURCE_DIR}/../security/lgss/bridges/nimcp_lgss_planning_bridge.c                      # Planning module bridge
    # LGSS Output Gates (A7)
    ${CMAKE_CURRENT_SOURCE_DIR}/../security/lgss/gates/nimcp_lgss_motor_gate.c                             # Motor output gate
    ${CMAKE_CURRENT_SOURCE_DIR}/../security/lgss/gates/nimcp_lgss_speech_gate.c                            # Speech output gate
    ${CMAKE_CURRENT_SOURCE_DIR}/../security/lgss/gates/nimcp_lgss_autonomic_gate.c                         # Autonomic output gate
    # LGSS Learning Guards (A8)
    ${CMAKE_CURRENT_SOURCE_DIR}/../security/lgss/learning/nimcp_lgss_plasticity_constraints.c              # Plasticity constraints
    ${CMAKE_CURRENT_SOURCE_DIR}/../security/lgss/learning/nimcp_lgss_stdp_guard.c                          # STDP safety guard
    ${CMAKE_CURRENT_SOURCE_DIR}/../security/lgss/learning/nimcp_lgss_training_guard.c                      # Training safety guard
    ${CMAKE_CURRENT_SOURCE_DIR}/../security/lgss/learning/nimcp_lgss_meta_learning_guard.c                 # Meta-learning guard
    # LGSS Reward Guards (A9)
    ${CMAKE_CURRENT_SOURCE_DIR}/../security/lgss/reward/nimcp_lgss_reward_alignment.c                      # Reward alignment
    ${CMAKE_CURRENT_SOURCE_DIR}/../security/lgss/reward/nimcp_lgss_vta_guard.c                             # VTA guard
    ${CMAKE_CURRENT_SOURCE_DIR}/../security/lgss/reward/nimcp_lgss_incentive_validator.c                   # Incentive validator
    # LGSS Perception Guards (A10)
    ${CMAKE_CURRENT_SOURCE_DIR}/../security/lgss/perception/nimcp_lgss_input_validator.c                   # Input validator
    ${CMAKE_CURRENT_SOURCE_DIR}/../security/lgss/perception/nimcp_lgss_adversarial_detector.c              # Adversarial detector
    ${CMAKE_CURRENT_SOURCE_DIR}/../security/lgss/perception/nimcp_lgss_content_filter.c                    # Content filter
    # LGSS Cognitive Guards (A11)
    ${CMAKE_CURRENT_SOURCE_DIR}/../security/lgss/cognitive/nimcp_lgss_attention_guard.c                    # Attention guard
    ${CMAKE_CURRENT_SOURCE_DIR}/../security/lgss/cognitive/nimcp_lgss_working_memory_guard.c               # Working memory guard

    # Safety Enhancement Modules (Phase SAFE: 10 Critical AI Safety Components)
    ${CMAKE_CURRENT_SOURCE_DIR}/../security/nimcp_emergency_halt.c                                         # Emergency halt system (watchdog, kill phrase, dead man's switch)
    ${CMAKE_CURRENT_SOURCE_DIR}/../security/nimcp_tripwires.c                                              # Tripwire detection (deception, goal drift, sandbagging)
    ${CMAKE_CURRENT_SOURCE_DIR}/../security/nimcp_corrigibility.c                                          # Corrigibility enforcement (shutdown acceptance, goal modification)
    ${CMAKE_CURRENT_SOURCE_DIR}/../security/nimcp_alignment_monitor.c                                      # Alignment drift monitoring (KL divergence, value tracking)
    ${CMAKE_CURRENT_SOURCE_DIR}/../security/nimcp_capability_control.c                                     # Capability control (envelope enforcement, resource limits)
    ${CMAKE_CURRENT_SOURCE_DIR}/../security/nimcp_interpretability.c                                       # Interpretability (decision factors, counterfactuals, fidelity)
    ${CMAKE_CURRENT_SOURCE_DIR}/../security/nimcp_safety_verification.c                                    # Formal verification (consistency, completeness, no-bypass)
    ${CMAKE_CURRENT_SOURCE_DIR}/../security/nimcp_red_team.c                                               # Red team infrastructure (attack generation, defense testing)
    ${CMAKE_CURRENT_SOURCE_DIR}/../security/nimcp_graduated_autonomy.c                                     # Graduated autonomy (Bayesian trust, level management)
    ${CMAKE_CURRENT_SOURCE_DIR}/../security/nimcp_value_commitment.c                                       # Value commitment (cryptographic attestation, tampering detection)

    # Utils - Memory & Thread
    ${CMAKE_CURRENT_SOURCE_DIR}/../utils/memory/nimcp_memory.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../utils/memory/nimcp_memory_guards.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../utils/memory/nimcp_memory_pool.c   # Phase 0: Memory Pool (O(1) allocation)
    ${CMAKE_CURRENT_SOURCE_DIR}/../utils/memory/nimcp_cow_manager.c  # Phase 0: CoW Manager (lazy copy, refcounting)
    ${CMAKE_CURRENT_SOURCE_DIR}/../utils/memory/nimcp_buffer_pool.c  # Phase 0: Buffer Pool (CoW + Pool integration)
    ${CMAKE_CURRENT_SOURCE_DIR}/../utils/memory/nimcp_page_cow.c     # Phase COW: Page-level COW for large structures
    ${CMAKE_CURRENT_SOURCE_DIR}/../utils/memory/nimcp_unified_memory.c  # Unified Memory Manager (object/page CoW + pools)
    ${CMAKE_CURRENT_SOURCE_DIR}/../utils/memory/nimcp_brain_pools.c     # Brain-level specialized pools
    ${CMAKE_CURRENT_SOURCE_DIR}/../utils/memory/nimcp_layer_pools.c     # Layer-level specialized pools
    ${CMAKE_CURRENT_SOURCE_DIR}/../utils/memory/nimcp_unified_pools.c   # Unified pool management
    ${CMAKE_CURRENT_SOURCE_DIR}/../utils/thread/nimcp_thread.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../utils/thread/nimcp_thread_mutex.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../utils/thread/nimcp_thread_pool.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../utils/thread/nimcp_deadlock_detector.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../utils/thread/nimcp_semaphore.c     # Counting semaphore primitive
    ${CMAKE_CURRENT_SOURCE_DIR}/../utils/thread/nimcp_atomic.c        # Atomic operations wrapper
    ${CMAKE_CURRENT_SOURCE_DIR}/../utils/thread/nimcp_barrier.c       # Thread barrier primitive
    ${CMAKE_CURRENT_SOURCE_DIR}/../utils/thread/nimcp_mutex_pool.c    # Shared mutex pool for memory optimization

    # Utils - Function Dispatch (Hot-swappable function pointers with versioning)
    ${CMAKE_CURRENT_SOURCE_DIR}/../utils/dispatch/nimcp_fn_dispatch.c  # Function dispatch table with quarantine

    # Utils - Code Recompilation (Self-healing dynamic patching)
    ${CMAKE_CURRENT_SOURCE_DIR}/../utils/code/nimcp_source_cache.c      # Memory-mapped source file cache
    ${CMAKE_CURRENT_SOURCE_DIR}/../utils/code/nimcp_recompiler.c        # Dynamic recompilation pipeline
    ${CMAKE_CURRENT_SOURCE_DIR}/../utils/code/nimcp_hot_inject.c        # Hot injection for runtime patching
    ${CMAKE_CURRENT_SOURCE_DIR}/../utils/code/nimcp_dwarf_symbols.c     # DWARF debug symbols parser

    # Utils - VCS Integration (Self-repair source code modification)
    ${CMAKE_CURRENT_SOURCE_DIR}/../utils/vcs/nimcp_vcs_integration.c   # Git integration for autonomous source commits

    # Utils - Math (Phase 0.5: Complex Number Support for Neural Phase Coding)
    ${CMAKE_CURRENT_SOURCE_DIR}/../utils/math/nimcp_complex_math.c  # Complex phasor operations, FFT, PAC detection (SIMD optimized)

    # Utils - Statistics (Statistical and probability methods for neural computation)
    ${CMAKE_CURRENT_SOURCE_DIR}/../utils/statistics/nimcp_statistics.c  # Descriptive stats, distributions, hypothesis testing, Bayesian inference
    ${CMAKE_CURRENT_SOURCE_DIR}/../utils/statistics/nimcp_quantum_statistics.c  # Quantum entropy, fidelity, entanglement, Fisher information
    # Statistics Enhancement Modules (Phase STATS-ENH)
    ${CMAKE_CURRENT_SOURCE_DIR}/../utils/statistics/nimcp_neural_statistics.c       # Neural-specific statistics (spike trains, population codes, tuning curves)
    ${CMAKE_CURRENT_SOURCE_DIR}/../utils/statistics/nimcp_information_theory.c      # Advanced information theory (transfer entropy, mutual info, synergy)
    ${CMAKE_CURRENT_SOURCE_DIR}/../utils/statistics/nimcp_multivariate.c            # Multivariate analysis (PCA, ICA, factor analysis, canonical correlation)
    ${CMAKE_CURRENT_SOURCE_DIR}/../utils/statistics/nimcp_timeseries.c              # Time series analysis (ARIMA, spectral, change-point, forecasting)
    ${CMAKE_CURRENT_SOURCE_DIR}/../utils/statistics/nimcp_ml_statistics.c           # ML statistics (cross-validation, bootstrap, model selection, calibration)
    ${CMAKE_CURRENT_SOURCE_DIR}/../utils/statistics/nimcp_streaming_statistics.c    # Streaming statistics (online mean/var, sketches, reservoir sampling)
    ${CMAKE_CURRENT_SOURCE_DIR}/../utils/statistics/nimcp_survival.c                # Survival analysis (Kaplan-Meier, Cox regression, hazard functions)
    ${CMAKE_CURRENT_SOURCE_DIR}/../utils/statistics/nimcp_bayesian_advanced.c       # Advanced Bayesian (MCMC, variational inference, Gaussian processes)

    # Utils - Signal Processing (Phase 0.5: Band-pass filtering for PAC detection)
    ${CMAKE_CURRENT_SOURCE_DIR}/../utils/signal/nimcp_signal_filter.c  # FIR/IIR band-pass filtering using windowed sinc method
    ${CMAKE_CURRENT_SOURCE_DIR}/../utils/signal/nimcp_hilbert.c  # Hilbert transform for analytic signal, amplitude, and phase extraction

    # Utils - Platform Abstraction (Phase 2: Cross-platform support, SRP-compliant)
    ${CMAKE_CURRENT_SOURCE_DIR}/../utils/platform/nimcp_platform.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../utils/platform/nimcp_platform_mutex.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../utils/platform/nimcp_platform_thread.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../utils/platform/nimcp_platform_cond.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../utils/platform/nimcp_platform_rwlock.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../utils/platform/nimcp_platform_once.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../utils/platform/nimcp_platform_time.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../utils/platform/nimcp_system_resources.c  # Phase 2.8: Hardware resource detection

    # Utils - Logging
    ${CMAKE_CURRENT_SOURCE_DIR}/../utils/logging/nimcp_logging.c

    # Utils - Random Number Generation (Unified RNG with quantum integration)
    ${CMAKE_CURRENT_SOURCE_DIR}/../utils/rng/nimcp_rand.c

    # Utils - Signal Handling (Runtime Safety)
    ${CMAKE_CURRENT_SOURCE_DIR}/../utils/signal/nimcp_signal_handler.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../utils/signal/nimcp_signal_exception_queue.c

    # Core Events Infrastructure (Promoted from Fault Tolerance - Now Universal)
    ${CMAKE_CURRENT_SOURCE_DIR}/../core/events/nimcp_event_bus.c                    # Universal event bus for ALL brain activities (promoted to core)

    # Utils - Fault Tolerance & Recovery (Production Resilience - Complete System - 14 modules)
    ${CMAKE_CURRENT_SOURCE_DIR}/../utils/fault_tolerance/nimcp_recovery_pool.c                 # Pre-allocated memory pool for OOM recovery
    ${CMAKE_CURRENT_SOURCE_DIR}/../utils/fault_tolerance/nimcp_fast_recovery.c                 # Fast path recovery (<1ms)
    ${CMAKE_CURRENT_SOURCE_DIR}/../utils/fault_tolerance/nimcp_recovery.c                      # Recovery strategy execution
    ${CMAKE_CURRENT_SOURCE_DIR}/../utils/fault_tolerance/nimcp_recovery_cache.c                # LRU cache for recovery strategies
    ${CMAKE_CURRENT_SOURCE_DIR}/../utils/fault_tolerance/nimcp_diagnostics.c                   # Error detection & classification
    ${CMAKE_CURRENT_SOURCE_DIR}/../utils/fault_tolerance/nimcp_checkpoint.c                    # State persistence & checkpointing
    ${CMAKE_CURRENT_SOURCE_DIR}/../utils/fault_tolerance/nimcp_state_manager.c                # Phase 8: Module state manager for consistent checkpointing
    ${CMAKE_CURRENT_SOURCE_DIR}/../utils/fault_tolerance/nimcp_module_recovery.c             # Phase 8: Module-specific recovery actions
    ${CMAKE_CURRENT_SOURCE_DIR}/../utils/fault_tolerance/nimcp_checkpoint_pool.c              # Phase 0.5: Fast checkpoint pool (CoW + Pool)
    ${CMAKE_CURRENT_SOURCE_DIR}/../utils/fault_tolerance/nimcp_async_checkpoint.c              # Async checkpoint with background thread
    ${CMAKE_CURRENT_SOURCE_DIR}/../utils/fault_tolerance/nimcp_health_monitor.c                # Runtime health monitoring
    ${CMAKE_CURRENT_SOURCE_DIR}/../utils/fault_tolerance/nimcp_brain_recovery_integration.c    # Brain-driven intelligent recovery
    ${CMAKE_CURRENT_SOURCE_DIR}/../utils/fault_tolerance/nimcp_runtime_adaptation.c            # Runtime parameter adaptation
    ${CMAKE_CURRENT_SOURCE_DIR}/../utils/fault_tolerance/nimcp_cognitive_recovery.c            # Cognitive Recovery Coordinator (orchestrates brain-driven recovery)
    ${CMAKE_CURRENT_SOURCE_DIR}/../utils/fault_tolerance/nimcp_fault_state_machine.c           # Fault state machine (HEALTHY/DEGRADED/FAILED)
    # ${CMAKE_CURRENT_SOURCE_DIR}/../utils/fault_tolerance/nimcp_fault_event_bus.c             # DEPRECATED: Moved to core/events/nimcp_event_bus.c (promoted to universal)
    ${CMAKE_CURRENT_SOURCE_DIR}/../utils/fault_tolerance/nimcp_lockfree_metrics.c              # Lock-free metrics ring buffer (4x faster)
    ${CMAKE_CURRENT_SOURCE_DIR}/../utils/fault_tolerance/nimcp_health_agent.c                  # Independent health monitor agent
    ${CMAKE_CURRENT_SOURCE_DIR}/../utils/fault_tolerance/nimcp_stdp_health_metrics.c          # STDP and plasticity health metrics
    ${CMAKE_CURRENT_SOURCE_DIR}/../utils/fault_tolerance/nimcp_plasticity_anomaly_detection.c # Plasticity anomaly detection patterns
    ${CMAKE_CURRENT_SOURCE_DIR}/../utils/fault_tolerance/nimcp_capacity_manager.c             # Dynamic capacity management (Phase 5.8)
    ${CMAKE_CURRENT_SOURCE_DIR}/../utils/fault_tolerance/nimcp_metrics_aggregator.c            # Metrics aggregation (P50/P95/P99)

    # Enhanced Fault Tolerance Modules (Phase FT-2: Immune System Integration)
    ${CMAKE_CURRENT_SOURCE_DIR}/../utils/fault_tolerance/nimcp_distributed_fault_tolerance.c   # Cross-drone failure coordination
    ${CMAKE_CURRENT_SOURCE_DIR}/../utils/fault_tolerance/nimcp_hierarchical_recovery.c         # Multi-level recovery orchestration
    ${CMAKE_CURRENT_SOURCE_DIR}/../utils/fault_tolerance/nimcp_recovery_evolution.c            # Genetic/RL strategy evolution
    ${CMAKE_CURRENT_SOURCE_DIR}/../utils/fault_tolerance/nimcp_byzantine_fault_tolerance.c     # PBFT consensus and trust
    ${CMAKE_CURRENT_SOURCE_DIR}/../utils/fault_tolerance/nimcp_graceful_degradation.c          # Service tier management
    ${CMAKE_CURRENT_SOURCE_DIR}/../utils/fault_tolerance/nimcp_recovery_observability.c        # MTTR/MTBF metrics and tracing
    ${CMAKE_CURRENT_SOURCE_DIR}/../utils/fault_tolerance/nimcp_chaos_engineering.c             # Controlled fault injection
    ${CMAKE_CURRENT_SOURCE_DIR}/../utils/fault_tolerance/nimcp_predictive_analysis.c           # Time-series anomaly detection
    ${CMAKE_CURRENT_SOURCE_DIR}/../utils/fault_tolerance/nimcp_retry.c                         # Retry with exponential backoff (P1-P3 remediation)

    # GPU Health Monitoring (Phase 7: Section 26 - GPU Health & Resilience)
    ${CMAKE_CURRENT_SOURCE_DIR}/../utils/gpu/nimcp_gpu_health.c                               # GPU health monitor and resilience

    # Health Agent Ethics & Emotion Integration (Phase 9: Section 28 - Ethics & Emotion)
    ${CMAKE_CURRENT_SOURCE_DIR}/../cognitive/ethics/nimcp_health_ethics_bridge.c              # Health-ethics bridge (Asimov, mercy, proportionality)
    ${CMAKE_CURRENT_SOURCE_DIR}/../cognitive/emotion/nimcp_health_emotion_bridge.c            # Health-emotion bridge (threshold adjustment, shadow detection)

    # Utils - Error Codes (Structured Error System)
    ${CMAKE_CURRENT_SOURCE_DIR}/../utils/error/nimcp_error_codes.c

    # Utils - Exception Handling with Brain Immune Integration
    ${CMAKE_CURRENT_SOURCE_DIR}/../utils/exception/nimcp_exception.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../utils/exception/nimcp_exception_handlers.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../utils/exception/nimcp_exception_immune.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../utils/exception/nimcp_exception_trace.c    # Distributed tracing & KG-aware propagation
    ${CMAKE_CURRENT_SOURCE_DIR}/../utils/exception/nimcp_exception_circuit.c  # Circuit breaker & suppression
    ${CMAKE_CURRENT_SOURCE_DIR}/../utils/exception/nimcp_exception_metrics.c  # Metrics, telemetry & adaptive recovery

    # Utils - Spatial Indexing (KD-Tree)
    ${CMAKE_CURRENT_SOURCE_DIR}/../utils/spatial/nimcp_kdtree.c

    # Utils - Dynamic Configuration (Runtime Safety)
    ${CMAKE_CURRENT_SOURCE_DIR}/../utils/config/nimcp_dynamic_config.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../utils/config/nimcp_config_hash.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../utils/config/nimcp_config_validation.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../utils/config/nimcp_config_expand.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../utils/config/nimcp_config_array.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../utils/config/nimcp_config_signal.c

    # Utils - Containers
    ${CMAKE_CURRENT_SOURCE_DIR}/../utils/containers/nimcp_vector.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../utils/containers/nimcp_hash_table.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../utils/containers/nimcp_btree.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../utils/containers/nimcp_queue.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../utils/containers/nimcp_queue_blocking.c   # Phase ASY-1: Mutex-based blocking queue
    ${CMAKE_CURRENT_SOURCE_DIR}/../utils/containers/nimcp_queue_spsc.c       # Phase ASY-1: Lock-free SPSC queue (<100ns)
    ${CMAKE_CURRENT_SOURCE_DIR}/../utils/containers/nimcp_queue_mpmc.c       # Phase ASY-1: Lock-free MPMC queue (Vyukov algorithm)
    ${CMAKE_CURRENT_SOURCE_DIR}/../utils/containers/nimcp_graph.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../utils/containers/nimcp_graph_ternary.c  # Ternary graph for hypergraph support
    ${CMAKE_CURRENT_SOURCE_DIR}/../utils/containers/nimcp_min_heap.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../utils/containers/nimcp_darray.c           # Generic dynamic array container
    ${CMAKE_CURRENT_SOURCE_DIR}/../utils/containers/nimcp_ring_buffer.c     # Fixed-size circular buffer

    # Async - Futures/Promises (Phase ASY-1: Async Communication)
    ${CMAKE_CURRENT_SOURCE_DIR}/../async/nimcp_future.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../async/nimcp_bio_async.c      # Phase BIO-ASYNC: Biologically-inspired async
    ${CMAKE_CURRENT_SOURCE_DIR}/../async/nimcp_bio_router.c     # Phase BIO-ASYNC: Bio-router message routing
    ${CMAKE_CURRENT_SOURCE_DIR}/../async/nimcp_bio_async_orchestrator.c  # Bio-Async Orchestrator - central coordinator for 200+ bio-async modules
    ${CMAKE_CURRENT_SOURCE_DIR}/../async/nimcp_wiring_diagram.c  # KG-Based Runtime Module Wiring Diagram System
    ${CMAKE_CURRENT_SOURCE_DIR}/../async/immune/nimcp_bio_router_immune_bridge.c  # Bio-router-immune integration
    ${CMAKE_CURRENT_SOURCE_DIR}/../async/nimcp_predictive_protocol.c  # Predictive communication protocol

    # Utils - Algorithms (Graph metrics for brain topology validation)
    ${CMAKE_CURRENT_SOURCE_DIR}/../core/topology/nimcp_community_detection.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../utils/algorithms/nimcp_centrality.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../utils/algorithms/nimcp_graph_metrics.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../utils/algorithms/nimcp_sort.c              # Consolidated sorting/graph algorithms
    ${CMAKE_CURRENT_SOURCE_DIR}/../utils/algorithms/nimcp_monte_carlo.c       # Monte Carlo and MCTS algorithms

    # Utils - Other
    ${CMAKE_CURRENT_SOURCE_DIR}/../utils/time/nimcp_time.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../utils/validation/nimcp_validate.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../utils/tensor/nimcp_tensor.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../utils/bridge/nimcp_bridge_base.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../utils/bridge/nimcp_bridge_gpu.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../utils/queue_manager/nimcp_queue_manager.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../utils/json/nimcp_json.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../utils/config/nimcp_config.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../utils/metrics/nimcp_metrics.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../utils/cache/nimcp_cache.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../utils/numerical/nimcp_integration.c

    # Utils - Spectral Analysis
    ${CMAKE_CURRENT_SOURCE_DIR}/../utils/spectral/nimcp_fft.c

    # Utils - Quantum-Inspired Algorithms (Phase C: Quantum-Inspired Algorithms)
    ${CMAKE_CURRENT_SOURCE_DIR}/../utils/quantum/nimcp_quantum_walk.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../utils/quantum/nimcp_quantum_shannon.c  # Phase C4.1: Quantum Walk + Shannon Information Theory
    ${CMAKE_CURRENT_SOURCE_DIR}/../utils/quantum/nimcp_quantum_monte_carlo.c  # Quantum Monte Carlo Integration

    # Utils - Geometry (Phase B1.1: Hyperbolic Knowledge Graph Embeddings)
    ${CMAKE_CURRENT_SOURCE_DIR}/../utils/geometry/nimcp_hyperbolic.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../utils/geometry/nimcp_differential_geometry.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../utils/geometry/nimcp_lorentz.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../utils/geometry/nimcp_lie_group.c

    # Core - Surface Geometry (Meng et al. Nature 2026: Surface optimization for physical networks)
    ${CMAKE_CURRENT_SOURCE_DIR}/../core/geometry/nimcp_surface_manifold.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../core/geometry/nimcp_surface_geometry.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../core/geometry/nimcp_surface_optimization.c

    # Core - Surface Geometry Bridges
    ${CMAKE_CURRENT_SOURCE_DIR}/../core/brain/bridges/nimcp_surface_geometry_bridge.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../core/brain/bridges/nimcp_hyperledger_bridge.c  # Hyperledger-inspired EOV training + consensus + audit
    ${CMAKE_CURRENT_SOURCE_DIR}/../core/brain/factory/init/nimcp_brain_init_surface_geometry.c

    # Async - Surface Geometry Bio-Async Bridge
    ${CMAKE_CURRENT_SOURCE_DIR}/../async/bridges/nimcp_surface_bio_async_bridge.c

    # Async - Bio-Async Plasticity Bridge (SNN/STDP integration)
    ${CMAKE_CURRENT_SOURCE_DIR}/../async/bridges/nimcp_bio_async_plasticity_bridge.c

    # Quantum - Surface Geometry Integration
    ${CMAKE_CURRENT_SOURCE_DIR}/../quantum/integration/nimcp_surface_quantum_bridge.c

    # Cognitive - Surface Geometry Immune Bridge
    ${CMAKE_CURRENT_SOURCE_DIR}/../cognitive/immune/nimcp_surface_immune_bridge.c

    # Utils - Tensor Networks (Phase C3.1: MPS Compression)
    ${CMAKE_CURRENT_SOURCE_DIR}/../utils/tensor_networks/nimcp_mps.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../utils/tensor_networks/nimcp_svd_lapack.c  # LAPACK SVD for tensor compression

    # Utils - Tensor Operations (General-Purpose Tensor Library)
    ${CMAKE_CURRENT_SOURCE_DIR}/../utils/tensor/nimcp_tensor.c  # Core tensor operations and tensor calculus
    ${CMAKE_CURRENT_SOURCE_DIR}/../utils/tensor/nimcp_tensor_simd.c  # SIMD-optimized tensor operations (AVX2/AVX-512/NEON)

    # Utils - Positional Encoding (Transformer-style encodings)
    ${CMAKE_CURRENT_SOURCE_DIR}/../utils/encoding/nimcp_positional_encoding.c  # Sinusoidal, learned, RoPE, ALiBi encodings

    # Utils - Gabor Filters (V1 visual processing)
    ${CMAKE_CURRENT_SOURCE_DIR}/../utils/gabor/nimcp_gabor.c  # Unified Gabor filter library for orientation selectivity

    # Utils - Ternary Logic (Three-valued logic for SNN, ethics, voting)
    ${CMAKE_CURRENT_SOURCE_DIR}/../utils/ternary/nimcp_ternary_tensor.c  # Ternary-tensor integration

    # Utils - Fuzzy Logic (Graded membership reasoning for risk, market, decisions)
    ${CMAKE_CURRENT_SOURCE_DIR}/../utils/fuzzy/nimcp_fuzzy_types.c       # Core types, membership functions, hedges
    ${CMAKE_CURRENT_SOURCE_DIR}/../utils/fuzzy/nimcp_fuzzy_operators.c   # T-norms, t-conorms, implications, aggregation
    ${CMAKE_CURRENT_SOURCE_DIR}/../utils/fuzzy/nimcp_fuzzy_inference.c   # Mamdani, Sugeno, Tsukamoto FIS + ANFIS
    ${CMAKE_CURRENT_SOURCE_DIR}/../utils/fuzzy/nimcp_fuzzy_bridge.c      # 18-subsystem integration bridge

    # Information Theory (Phase C4: Shannon Information Theory)
    ${CMAKE_CURRENT_SOURCE_DIR}/../information/nimcp_shannon.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../information/nimcp_cross_modal.c  # Phase C4.7: Cross-Modal Information Flow

    # Optimization - Quantum Annealing (Phase 11 Enhancement C1.1)
    ${CMAKE_CURRENT_SOURCE_DIR}/../optimization/quantum_annealing/nimcp_quantum_annealing.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../optimization/quantum_annealing/nimcp_quantum_annealing_ternary.c

    # Brain Oscillations
    ${CMAKE_CURRENT_SOURCE_DIR}/../core/brain_oscillations/nimcp_brain_oscillations.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../core/brain/oscillations/nimcp_brain_complex_oscillations.c  # Phase C2.2: Complex phasor oscillation tracking
    ${CMAKE_CURRENT_SOURCE_DIR}/../core/brain_oscillations/nimcp_oscillations_immune_bridge.c  # Oscillations-immune integration

    # Audio Synthesis (Athena's voice)
    ${CMAKE_CURRENT_SOURCE_DIR}/../audio/synthesis/nimcp_formant_synth.c

    # Perception
    ${CMAKE_CURRENT_SOURCE_DIR}/perception/nimcp_visual_cortex.c
    ${CMAKE_CURRENT_SOURCE_DIR}/perception/nimcp_audio_cortex.c
    ${CMAKE_CURRENT_SOURCE_DIR}/perception/nimcp_audio_jepa_bridge.c  # Phase 4q: audio cortex ↔ JEPA
    ${CMAKE_CURRENT_SOURCE_DIR}/perception/nimcp_speech_cortex.c

    # Cochlear Audio Processing (Phase 3: Biologically-accurate cochlear model)
    ${CMAKE_CURRENT_SOURCE_DIR}/perception/nimcp_basilar_membrane.c      # Basilar membrane mechanics
    ${CMAKE_CURRENT_SOURCE_DIR}/perception/nimcp_hair_cells.c            # Inner/outer hair cells
    ${CMAKE_CURRENT_SOURCE_DIR}/perception/nimcp_auditory_nerve.c        # Auditory nerve fibers
    ${CMAKE_CURRENT_SOURCE_DIR}/perception/nimcp_cochlea_extended.c      # Extended cochlea (dog/bat modes)
    ${CMAKE_CURRENT_SOURCE_DIR}/perception/nimcp_cochlea.c               # Main cochlea integration

    # Cochlear Brain Integration Bridges (Phase 3: Auditory pathway)
    ${CMAKE_CURRENT_SOURCE_DIR}/perception/bridges/nimcp_cochlea_medulla_bridge.c       # Brainstem pathway
    ${CMAKE_CURRENT_SOURCE_DIR}/perception/bridges/nimcp_cochlea_thalamic_bridge.c      # MGN relay with attention
    ${CMAKE_CURRENT_SOURCE_DIR}/perception/bridges/nimcp_cochlea_audio_cortex_bridge.c  # A1 cortical integration
    ${CMAKE_CURRENT_SOURCE_DIR}/perception/bridges/nimcp_cochlea_fep_bridge.c          # Free Energy Principle integration
    ${CMAKE_CURRENT_SOURCE_DIR}/perception/bridges/nimcp_cochlea_sleep_bridge.c        # Sleep stage modulation
    ${CMAKE_CURRENT_SOURCE_DIR}/perception/bridges/nimcp_cochlea_immune_bridge.c       # Cochlear health/immune system
    ${CMAKE_CURRENT_SOURCE_DIR}/perception/bridges/nimcp_cochlea_kg_bridge.c           # Knowledge Graph self-awareness
    ${CMAKE_CURRENT_SOURCE_DIR}/perception/bridges/nimcp_cochlea_substrate_bridge.c    # Neural substrate spike encoding
    ${CMAKE_CURRENT_SOURCE_DIR}/perception/bridges/nimcp_cochlea_rcog_bridge.c         # Recursive Cognition integration
    ${CMAKE_CURRENT_SOURCE_DIR}/perception/bridges/nimcp_cochlea_collective_bridge.c   # Collective cognition sync
    ${CMAKE_CURRENT_SOURCE_DIR}/perception/bridges/nimcp_cochlea_cortical_deep_bridge.c # Deep cortical integration
    ${CMAKE_CURRENT_SOURCE_DIR}/perception/bridges/nimcp_cochlea_occipital_bridge.c    # Occipital audiovisual bridge
    ${CMAKE_CURRENT_SOURCE_DIR}/perception/bridges/nimcp_cochlea_broca_bridge.c        # Broca speech bridge
    ${CMAKE_CURRENT_SOURCE_DIR}/perception/bridges/nimcp_cochlea_bio_async_bridge.c    # Bio-async complete
    ${CMAKE_CURRENT_SOURCE_DIR}/perception/bridges/nimcp_cochlea_verification_bridge.c # Verification system

    # Perception JEPA Bridges (Joint Embedding Predictive Architecture)
    ${CMAKE_CURRENT_SOURCE_DIR}/../perception/nimcp_visual_jepa_bridge.c         # Visual-JEPA encoding bridge
    ${CMAKE_CURRENT_SOURCE_DIR}/../perception/nimcp_visual_jepa_fep_bridge.c     # Visual JEPA-FEP integration
    ${CMAKE_CURRENT_SOURCE_DIR}/../perception/nimcp_speech_jepa_bridge.c         # Speech-JEPA encoding bridge

    ${CMAKE_CURRENT_SOURCE_DIR}/../perception/immune/nimcp_visual_immune_bridge.c  # Visual cortex-immune integration
    ${CMAKE_CURRENT_SOURCE_DIR}/../perception/immune/nimcp_audio_immune_bridge.c  # Audio cortex-immune integration
    ${CMAKE_CURRENT_SOURCE_DIR}/../perception/immune/nimcp_speech_immune_bridge.c  # Speech cortex-immune integration

    # Perception-Cortical Bridges (Phase CC-P: Cortical column integration)
    ${CMAKE_CURRENT_SOURCE_DIR}/../perception/cortical/nimcp_visual_cortical_bridge.c  # Visual-cortical bridge
    ${CMAKE_CURRENT_SOURCE_DIR}/../perception/cortical/nimcp_audio_cortical_bridge.c   # Audio-cortical bridge
    ${CMAKE_CURRENT_SOURCE_DIR}/../perception/cortical/nimcp_speech_cortical_bridge.c  # Speech-cortical bridge

    # Glial-Immune Integration (Phase BII-G: Glial immune bridges)
    # OO Inheritance Pattern: Base class + derived types
    ${CMAKE_CURRENT_SOURCE_DIR}/../glial/immune/nimcp_astrocyte_immune_base.c  # Abstract base for astrocyte-immune bridges
    ${CMAKE_CURRENT_SOURCE_DIR}/../glial/immune/nimcp_astrocyte_immune_plasticity.c  # Derived: plasticity-focused bridge
    ${CMAKE_CURRENT_SOURCE_DIR}/../glial/immune/nimcp_astrocyte_immune_bridge.c  # Network-level astrocyte-immune bridge
    ${CMAKE_CURRENT_SOURCE_DIR}/../glial/immune/nimcp_astrocytes_immune_bridge.c  # Single-cell astrocyte-immune (A1/A2 phenotypes)
    ${CMAKE_CURRENT_SOURCE_DIR}/../glial/immune/nimcp_microglia_immune_bridge.c  # Microglia-immune integration (brain macrophages)
    ${CMAKE_CURRENT_SOURCE_DIR}/../glial/immune/nimcp_oligodendrocytes_immune_bridge.c  # Oligodendrocytes-immune integration (OPC differentiation)
    ${CMAKE_CURRENT_SOURCE_DIR}/../glial/immune/nimcp_myelin_immune_bridge.c  # Myelin-immune integration (MS pathophysiology)

    # Networking-Immune Integration (Phase BII-N: Distributed cognition immune bridges)
    ${CMAKE_CURRENT_SOURCE_DIR}/../networking/immune/nimcp_distributed_immune_bridge.c  # Distributed cognition-immune integration
    ${CMAKE_CURRENT_SOURCE_DIR}/../networking/immune/nimcp_protocol_immune_bridge.c  # NLP protocol-immune integration
    ${CMAKE_CURRENT_SOURCE_DIR}/../networking/immune/nimcp_p2p_immune_bridge.c  # P2P network-immune integration

    # NLP-Immune Integration (Phase BII-L: Language processing immune bridges)
    ${CMAKE_CURRENT_SOURCE_DIR}/../nlp/immune/nimcp_nlp_immune_bridge.c  # NLP-immune integration (language impairment)
    ${CMAKE_CURRENT_SOURCE_DIR}/../nlp/immune/nimcp_spike_nlp_immune_bridge.c  # Spike NLP-immune integration
    ${CMAKE_CURRENT_SOURCE_DIR}/../nlp/immune/nimcp_multimodal_nlp_immune_bridge.c  # Multimodal NLP-immune integration

    # Language Layer (Phase L5: Unified language processing orchestration)
    ${CMAKE_CURRENT_SOURCE_DIR}/../language/nimcp_language_orchestrator.c     # Language layer coordinator
    ${CMAKE_CURRENT_SOURCE_DIR}/../language/nimcp_language_config.c           # Default configuration
    ${CMAKE_CURRENT_SOURCE_DIR}/../language/nimcp_language_bio_async.c        # Bio-async messaging
    ${CMAKE_CURRENT_SOURCE_DIR}/../language/nimcp_concept_registry.c          # Slice B: cross-modal population binding registry
    ${CMAKE_CURRENT_SOURCE_DIR}/../language/nimcp_grounded_language.c        # Grounded language system
    ${CMAKE_CURRENT_SOURCE_DIR}/../language/nimcp_grounded_language_tf.c     # TF (tier-feedback) plasticity loop — TF-1 diff
    ${CMAKE_CURRENT_SOURCE_DIR}/../cognitive/grounded_language/nimcp_stage_table.c # Slice E — developmental stage scaffolding table
    ${CMAKE_CURRENT_SOURCE_DIR}/../language/nimcp_grounded_language_persistence.c  # Grounded language sidecar persistence
    ${CMAKE_CURRENT_SOURCE_DIR}/../language/nimcp_grounded_language_memory_bridge.c  # Grounded language ↔ memory subsystems bridge
    ${CMAKE_CURRENT_SOURCE_DIR}/../language/nimcp_grounded_language_nlp.c     # NLP frontend: morphology + embeddings + BPE fallback
    ${CMAKE_CURRENT_SOURCE_DIR}/../language/nimcp_grounded_language_chunker.c # NER + shallow chunker + chinking
    ${CMAKE_CURRENT_SOURCE_DIR}/../language/nimcp_grounded_language_network_bridge.c # Per-network bridges (LNN/CNN/FNO/ANN)
    ${CMAKE_CURRENT_SOURCE_DIR}/../language/nimcp_grounded_language_cognitive_bridge.c # Cognitive subscriber bus + per-module attaches
    ${CMAKE_CURRENT_SOURCE_DIR}/../language/nimcp_creative_language_bridge.c # Creative cognition → language bridge
    ${CMAKE_CURRENT_SOURCE_DIR}/../language/nimcp_communication_cascade.c    # Multi-region production cascade (Phase 2A)
    ${CMAKE_CURRENT_SOURCE_DIR}/../language/nimcp_communication_cascade_wernicke.c  # Cascade Stage 0 (Wernicke comprehension, separate TU due to phrase_type_t conflict with Broca)
    ${CMAKE_CURRENT_SOURCE_DIR}/../language/nimcp_phonological_loop.c         # Slice 5 — phonological-loop working memory buffer

    # Language Layer Bridges (Phase L8: Integration bridges)
    ${CMAKE_CURRENT_SOURCE_DIR}/../language/bridges/nimcp_language_perception_bridge.c   # Perception integration
    ${CMAKE_CURRENT_SOURCE_DIR}/../language/bridges/nimcp_language_cognitive_bridge.c    # Cognitive integration
    ${CMAKE_CURRENT_SOURCE_DIR}/../language/bridges/nimcp_language_training_bridge.c     # Training integration
    ${CMAKE_CURRENT_SOURCE_DIR}/../language/bridges/nimcp_language_omni_bridge.c         # Omni/predictive integration
    ${CMAKE_CURRENT_SOURCE_DIR}/../language/bridges/nimcp_language_immune_bridge.c       # Immune integration
    ${CMAKE_CURRENT_SOURCE_DIR}/../language/bridges/nimcp_language_gpu_bridge.c          # GPU acceleration
    ${CMAKE_CURRENT_SOURCE_DIR}/../language/bridges/nimcp_language_thalamic_bridge.c     # Thalamic routing
    ${CMAKE_CURRENT_SOURCE_DIR}/../language/bridges/nimcp_language_substrate_bridge.c    # Metabolic modulation
    ${CMAKE_CURRENT_SOURCE_DIR}/../language/bridges/nimcp_language_logic_bridge.c        # Symbolic logic integration
    ${CMAKE_CURRENT_SOURCE_DIR}/../language/bridges/nimcp_language_temporal_bridge.c     # Temporal lobe semantic integration
    ${CMAKE_CURRENT_SOURCE_DIR}/../language/bridges/nimcp_language_prefrontal_bridge.c   # Prefrontal executive control
    ${CMAKE_CURRENT_SOURCE_DIR}/../language/bridges/nimcp_language_motor_bridge.c        # Motor articulatory feedback
    ${CMAKE_CURRENT_SOURCE_DIR}/../language/bridges/nimcp_language_cingulate_bridge.c    # Cingulate error monitoring
    ${CMAKE_CURRENT_SOURCE_DIR}/../language/bridges/nimcp_language_hippocampus_bridge.c  # Hippocampus memory consolidation
    ${CMAKE_CURRENT_SOURCE_DIR}/../language/bridges/nimcp_language_parietal_bridge.c    # Parietal spatial language processing
    ${CMAKE_CURRENT_SOURCE_DIR}/../language/bridges/nimcp_language_insula_bridge.c      # Insula articulatory planning
    ${CMAKE_CURRENT_SOURCE_DIR}/../language/bridges/nimcp_language_cerebellum_bridge.c  # Cerebellum timing and rhythm

    # Portia-Immune Integration (Phase BII-P: Adaptive intelligence immune bridges)
    ${CMAKE_CURRENT_SOURCE_DIR}/../portia/immune/nimcp_portia_attention_immune_bridge.c  # Portia attention-immune integration
    ${CMAKE_CURRENT_SOURCE_DIR}/../portia/immune/nimcp_portia_learning_immune_bridge.c  # Portia learning-immune integration
    ${CMAKE_CURRENT_SOURCE_DIR}/../portia/immune/nimcp_portia_sensor_fusion_immune_bridge.c  # Portia sensor fusion-immune integration

    # GPU Support (CPU fallback when CUDA unavailable)
    ${CMAKE_CURRENT_SOURCE_DIR}/../gpu/execution/nimcp_execution_mode.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../gpu/execution/nimcp_simd_detect.c  # SIMD capability detection (AVX2/AVX-512/NEON)
    ${CMAKE_CURRENT_SOURCE_DIR}/../gpu/execution/nimcp_gpu_detect.c   # Runtime GPU detection (CUDA/OpenCL/ROCm)
    ${CMAKE_CURRENT_SOURCE_DIR}/../gpu/execution/nimcp_network_detect.c  # Network/MPI capability detection
    ${CMAKE_CURRENT_SOURCE_DIR}/../gpu/spike_event/nimcp_spike_event.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../gpu/neuron/nimcp_gpu_neuron.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../gpu/nimcp_multigpu.c

    # GPU Immune Integration (Phase BII-GPU: GPU immune bridges)
    ${CMAKE_CURRENT_SOURCE_DIR}/../gpu/immune/nimcp_gpu_execution_immune_bridge.c  # GPU execution-immune integration
    ${CMAKE_CURRENT_SOURCE_DIR}/../gpu/immune/nimcp_multigpu_immune_bridge.c  # Multi-GPU-immune integration

    # GPU CUDA kernels (only compiled if CUDA is available)
    # Core GPU infrastructure
    $<$<BOOL:${CUDAToolkit_FOUND}>:${CMAKE_CURRENT_SOURCE_DIR}/../gpu/context/nimcp_gpu_context.cu>
    # Tensor operations
    $<$<BOOL:${CUDAToolkit_FOUND}>:${CMAKE_CURRENT_SOURCE_DIR}/../gpu/tensor/nimcp_tensor_kernels.cu>
    $<$<BOOL:${CUDAToolkit_FOUND}>:${CMAKE_CURRENT_SOURCE_DIR}/../gpu/tensor/nimcp_fft_kernels.cu>
    # Training kernels
    $<$<BOOL:${CUDAToolkit_FOUND}>:${CMAKE_CURRENT_SOURCE_DIR}/../gpu/training/nimcp_gradient_kernels.cu>
    $<$<BOOL:${CUDAToolkit_FOUND}>:${CMAKE_CURRENT_SOURCE_DIR}/../gpu/training/nimcp_optimizer_kernels.cu>
    $<$<BOOL:${CUDAToolkit_FOUND}>:${CMAKE_CURRENT_SOURCE_DIR}/../gpu/training/nimcp_backprop_kernels.cu>
    $<$<BOOL:${CUDAToolkit_FOUND}>:${CMAKE_CURRENT_SOURCE_DIR}/../gpu/training/nimcp_activation_checkpoint.cu>
    $<$<BOOL:${CUDAToolkit_FOUND}>:${CMAKE_CURRENT_SOURCE_DIR}/../gpu/training/nimcp_gradient_checkpoint.cu>
    # SNN kernels (spiking neural networks)
    $<$<BOOL:${CUDAToolkit_FOUND}>:${CMAKE_CURRENT_SOURCE_DIR}/../gpu/snn/nimcp_snn_kernels.cu>
    # CNN kernels (convolution, pooling)
    $<$<BOOL:${CUDAToolkit_FOUND}>:${CMAKE_CURRENT_SOURCE_DIR}/../gpu/cnn/nimcp_conv_kernels.cu>
    # Perception kernels (audio, visual, speech)
    $<$<BOOL:${CUDAToolkit_FOUND}>:${CMAKE_CURRENT_SOURCE_DIR}/../gpu/perception/nimcp_audio_kernels.cu>
    $<$<BOOL:${CUDAToolkit_FOUND}>:${CMAKE_CURRENT_SOURCE_DIR}/../gpu/perception/nimcp_visual_kernels.cu>
    $<$<BOOL:${CUDAToolkit_FOUND}>:${CMAKE_CURRENT_SOURCE_DIR}/../gpu/perception/nimcp_speech_kernels.cu>
    # Speech Cortex GPU kernels (MFCC, LPC, formants, pitch, phoneme recognition)
    $<$<BOOL:${CUDAToolkit_FOUND}>:${CMAKE_CURRENT_SOURCE_DIR}/../gpu/perception/nimcp_speech_cortex_gpu.cu>
    # Occipital GPU bridge (connects CPU occipital cortex to GPU visual kernels)
    $<$<BOOL:${CUDAToolkit_FOUND}>:${CMAKE_CURRENT_SOURCE_DIR}/../gpu/occipital/nimcp_occipital_gpu_bridge.cu>
    # LNN kernels (liquid neural networks with ODE solvers)
    $<$<BOOL:${CUDAToolkit_FOUND}>:${CMAKE_CURRENT_SOURCE_DIR}/../gpu/lnn/nimcp_lnn_kernels.cu>
    # LNN ODE kernels (batched ODE integration for continuous-time dynamics)
    $<$<BOOL:${CUDAToolkit_FOUND}>:${CMAKE_CURRENT_SOURCE_DIR}/../gpu/lnn/nimcp_lnn_ode_kernels.cu>
    # LNN Gradient DAO (gradient accumulation for training)
    ${CMAKE_CURRENT_SOURCE_DIR}/../gpu/lnn/nimcp_lnn_gradient_dao.c
    # Quantum algorithm kernels
    $<$<BOOL:${CUDAToolkit_FOUND}>:${CMAKE_CURRENT_SOURCE_DIR}/../gpu/quantum/nimcp_quantum_kernels.cu>
    # Quantum Monte Carlo GPU kernels (parallel MC sampling, MCTS, SAT solving)
    $<$<BOOL:${CUDAToolkit_FOUND}>:${CMAKE_CURRENT_SOURCE_DIR}/../gpu/quantum/nimcp_qmc_gpu.cu>
    # Inference kernels (optimized forward pass)
    $<$<BOOL:${CUDAToolkit_FOUND}>:${CMAKE_CURRENT_SOURCE_DIR}/../gpu/inference/nimcp_inference_kernels.cu>
    # Cognitive module GPU kernels (FEP learning, attention, working memory, JEPA)
    $<$<BOOL:${CUDAToolkit_FOUND}>:${CMAKE_CURRENT_SOURCE_DIR}/../cognitive/free_energy/nimcp_fep_learning_gpu.cu>
    $<$<BOOL:${CUDAToolkit_FOUND}>:${CMAKE_CURRENT_SOURCE_DIR}/../cognitive/attention/nimcp_attention_gpu.cu>
    $<$<BOOL:${CUDAToolkit_FOUND}>:${CMAKE_CURRENT_SOURCE_DIR}/../cognitive/working_memory/nimcp_working_memory_gpu.cu>
    # JEPA predictor GPU kernels (forward/inverse models, masking)
    $<$<BOOL:${CUDAToolkit_FOUND}>:${CMAKE_CURRENT_SOURCE_DIR}/../gpu/cognitive/nimcp_jepa_gpu.cu>
    # Broca's region GPU kernels (lexical lookup, phoneme encoding, motor planning)
    $<$<BOOL:${CUDAToolkit_FOUND}>:${CMAKE_CURRENT_SOURCE_DIR}/../gpu/cognitive/nimcp_broca_gpu.cu>
    # Wernicke's region GPU kernels (phoneme recognition, cohort word recognition, semantic spreading)
    $<$<BOOL:${CUDAToolkit_FOUND}>:${CMAKE_CURRENT_SOURCE_DIR}/../gpu/cognitive/nimcp_wernicke_gpu.cu>
    # Omnidirectional inference GPU kernels (bidirectional prediction, Hopfield attention, predictive hierarchy, temporal replay)
    $<$<BOOL:${CUDAToolkit_FOUND}>:${CMAKE_CURRENT_SOURCE_DIR}/../gpu/cognitive/nimcp_omni_gpu.cu>
    # Memory consolidation GPU kernels (hippocampal replay, systems consolidation, engram operations)
    $<$<BOOL:${CUDAToolkit_FOUND}>:${CMAKE_CURRENT_SOURCE_DIR}/../gpu/memory/nimcp_memory_consolidation_kernels.cu>
    # Existing CUDA kernels
    $<$<BOOL:${CUDAToolkit_FOUND}>:${CMAKE_CURRENT_SOURCE_DIR}/../gpu/neuron/nimcp_gpu_kernels.cu>
    $<$<BOOL:${CUDAToolkit_FOUND}>:${CMAKE_CURRENT_SOURCE_DIR}/../gpu/synapse_compute/nimcp_synapse_compute_gpu.cu>
    $<$<BOOL:${CUDAToolkit_FOUND}>:${CMAKE_CURRENT_SOURCE_DIR}/../core/neuron_types/nimcp_neural_logic_kernels.cu>
    $<$<BOOL:${CUDAToolkit_FOUND}>:${CMAKE_CURRENT_SOURCE_DIR}/../gpu/sparse/nimcp_sparse_kernels.cu>
    # Embedding relevance GPU kernels (batch dot-product for CPU-staged embeddings)
    $<$<BOOL:${CUDAToolkit_FOUND}>:${CMAKE_CURRENT_SOURCE_DIR}/../gpu/embedding/nimcp_embedding_relevance_kernels.cu>
    # Kernel backend CUDA registration (wires CUDA kernels to unified backend interface)
    $<$<BOOL:${CUDAToolkit_FOUND}>:${CMAKE_CURRENT_SOURCE_DIR}/../gpu/backend/nimcp_kernel_backend_cuda.cu>

    # GPU context stubs (only compiled if CUDA is NOT available)
    $<$<NOT:$<BOOL:${CUDAToolkit_FOUND}>>:${CMAKE_CURRENT_SOURCE_DIR}/../gpu/context/nimcp_gpu_context_stub.c>
    # GPU module stubs (wernicke_gpu, tensor_gpu, graph_gpu - only if CUDA NOT available)
    $<$<NOT:$<BOOL:${CUDAToolkit_FOUND}>>:${CMAKE_CURRENT_SOURCE_DIR}/../gpu/stubs/nimcp_gpu_stubs.c>
    # GPU tensor extended stubs (reductions, norms, comparisons, FFT, CPU-GPU interop)
    $<$<NOT:$<BOOL:${CUDAToolkit_FOUND}>>:${CMAKE_CURRENT_SOURCE_DIR}/../gpu/stubs/nimcp_gpu_stubs_tensor_ext.c>
    # GPU embedding relevance stubs (CPU fallback for batch dot-product)
    $<$<NOT:$<BOOL:${CUDAToolkit_FOUND}>>:${CMAKE_CURRENT_SOURCE_DIR}/../gpu/stubs/nimcp_gpu_stubs_embedding.c>
    # GPU FP16/mixed precision stubs (FP16 ops, loss scaling, AMP context, autocast)
    # Always compiled: .cu version uses C99 designated initializers incompatible with NVCC
    ${CMAKE_CURRENT_SOURCE_DIR}/../gpu/stubs/nimcp_gpu_stubs_tensor_fp16.c
    # GPU gradient/activation checkpoint stubs (checkpoint ctx, sequential, transformer)
    $<$<NOT:$<BOOL:${CUDAToolkit_FOUND}>>:${CMAKE_CURRENT_SOURCE_DIR}/../gpu/stubs/nimcp_gpu_stubs_checkpoint.c>
    # GPU CNN stubs (conv2d, pooling, batch norm - CPU fallback with im2col)
    $<$<NOT:$<BOOL:${CUDAToolkit_FOUND}>>:${CMAKE_CURRENT_SOURCE_DIR}/../gpu/stubs/nimcp_gpu_stubs_cnn.c>
    # GPU SNN stubs (spiking neural network - LIF, Izhikevich, STDP, surrogate gradients)
    $<$<NOT:$<BOOL:${CUDAToolkit_FOUND}>>:${CMAKE_CURRENT_SOURCE_DIR}/../gpu/stubs/nimcp_gpu_stubs_snn.c>
    # GPU INT8 stubs (quantization, calibration, INT8 GEMM, requantization)
    $<$<NOT:$<BOOL:${CUDAToolkit_FOUND}>>:${CMAKE_CURRENT_SOURCE_DIR}/../gpu/stubs/nimcp_gpu_stubs_int8.c>
    # GPU TensorRT stubs (network definition, engine build/serialize, calibration)
    $<$<NOT:$<BOOL:${CUDAToolkit_FOUND}>>:${CMAKE_CURRENT_SOURCE_DIR}/../gpu/stubs/nimcp_gpu_stubs_tensorrt.c>
    # GPU training stubs (loss functions, optimizers, gradient ops, backward passes)
    $<$<NOT:$<BOOL:${CUDAToolkit_FOUND}>>:${CMAKE_CURRENT_SOURCE_DIR}/../gpu/stubs/nimcp_gpu_stubs_training.c>
    # GPU training bridge (weight cache + forward pass dispatch - always compiled, uses GPU stubs as fallback)
    ${CMAKE_CURRENT_SOURCE_DIR}/../gpu/training/nimcp_training_bridge.c

    # AWS Neuron/Inferentia NeuronCore integration (always compiled — uses dlopen for runtime NRT detection)
    ${CMAKE_CURRENT_SOURCE_DIR}/../gpu/neuron/nimcp_neuron_context.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../gpu/neuron/nimcp_neuron_bridge.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../gpu/neuron/nimcp_neuron_backend_ops.c

    # Kernel backend CPU implementation (always compiled - provides CPU fallback)
    ${CMAKE_CURRENT_SOURCE_DIR}/../gpu/backend/nimcp_kernel_backend.c

    # Neural substrate GPU operations (CUDA kernels for axon/dendrite/myelin/neuromodulator/glial/metabolic)
    $<$<BOOL:${CUDAToolkit_FOUND}>:${CMAKE_CURRENT_SOURCE_DIR}/../gpu/substrate/nimcp_substrate_kernels.cu>
    # Neural substrate CPU fallback operations (always compiled)
    ${CMAKE_CURRENT_SOURCE_DIR}/../gpu/substrate/nimcp_substrate_cpu.c
    # Neural substrate GPU context management (always compiled - wraps backend ops)
    ${CMAKE_CURRENT_SOURCE_DIR}/../gpu/substrate/nimcp_substrate_gpu.c

    # GPU Axon module (CUDA kernels for axon signal propagation and myelination)
    $<$<BOOL:${CUDAToolkit_FOUND}>:${CMAKE_CURRENT_SOURCE_DIR}/../gpu/axon/nimcp_axon_gpu.cu>

    # GPU Dendrite module (CUDA kernels for dendritic computation, calcium, STDP)
    $<$<BOOL:${CUDAToolkit_FOUND}>:${CMAKE_CURRENT_SOURCE_DIR}/../gpu/dendrite/nimcp_dendrite_gpu.cu>

    # GPU Myelin module (CUDA kernels for myelin sheath biophysics)
    $<$<BOOL:${CUDAToolkit_FOUND}>:${CMAKE_CURRENT_SOURCE_DIR}/../gpu/glial/nimcp_myelin_gpu.cu>

    # Neuromodulator GPU kernels (dopamine, serotonin, acetylcholine, norepinephrine dynamics)
    $<$<BOOL:${CUDAToolkit_FOUND}>:${CMAKE_CURRENT_SOURCE_DIR}/../gpu/neuromodulators/nimcp_neuromodulator_kernels.cu>
    # Emotion GPU kernels (amygdala, OFC, NAcc, ACC - limbic system)
    $<$<BOOL:${CUDAToolkit_FOUND}>:${CMAKE_CURRENT_SOURCE_DIR}/../gpu/emotion/nimcp_emotion_kernels.cu>
    # Reasoning GPU kernels (propositional logic, rule engine, CSP, causal inference)
    $<$<BOOL:${CUDAToolkit_FOUND}>:${CMAKE_CURRENT_SOURCE_DIR}/../gpu/reasoning/nimcp_reasoning_kernels.cu>
    # Metalearning GPU kernels (MAML, Reptile, ProtoNets, memory-augmented meta-learning)
    $<$<BOOL:${CUDAToolkit_FOUND}>:${CMAKE_CURRENT_SOURCE_DIR}/../gpu/metalearning/nimcp_metalearning_kernels.cu>
    # Portia GPU kernels (spider-inspired vision, salience, attention, spatial cognition)
    $<$<BOOL:${CUDAToolkit_FOUND}>:${CMAKE_CURRENT_SOURCE_DIR}/../gpu/portia/nimcp_portia_kernels.cu>
    # Sleep GPU kernels (NREM/REM consolidation, synaptic homeostasis, memory replay)
    $<$<BOOL:${CUDAToolkit_FOUND}>:${CMAKE_CURRENT_SOURCE_DIR}/../gpu/sleep/nimcp_sleep_kernels.cu>
    # Brain regions GPU kernels (cortical columns, PFC, motor, parietal cortex)
    $<$<BOOL:${CUDAToolkit_FOUND}>:${CMAKE_CURRENT_SOURCE_DIR}/../gpu/regions/nimcp_regions_kernels.cu>
    # Oscillations GPU kernels (neural oscillations, cross-frequency coupling)
    $<$<BOOL:${CUDAToolkit_FOUND}>:${CMAKE_CURRENT_SOURCE_DIR}/../gpu/oscillations/nimcp_oscillations_kernels.cu>
    # Plasticity GPU kernels (STDP, BCM, homeostatic plasticity)
    $<$<BOOL:${CUDAToolkit_FOUND}>:${CMAKE_CURRENT_SOURCE_DIR}/../gpu/plasticity/nimcp_plasticity_kernels.cu>
    # GPU plasticity bridge (wires GPU plasticity kernels into training pipelines)
    ${CMAKE_CURRENT_SOURCE_DIR}/../gpu/plasticity/nimcp_gpu_plasticity_bridge.c
    # Knowledge graph GPU kernels (graph embedding, relation inference)
    $<$<BOOL:${CUDAToolkit_FOUND}>:${CMAKE_CURRENT_SOURCE_DIR}/../gpu/knowledge/nimcp_knowledge_graph_kernels.cu>
    # Swarm GPU kernels (collective intelligence, pheromone trails, flocking)
    $<$<BOOL:${CUDAToolkit_FOUND}>:${CMAKE_CURRENT_SOURCE_DIR}/../gpu/swarm/nimcp_swarm_kernels.cu>
    # Swarm memory GPU kernels (experience replay, consolidation, multi-agent coordination)
    $<$<BOOL:${CUDAToolkit_FOUND}>:${CMAKE_CURRENT_SOURCE_DIR}/../gpu/swarm/nimcp_swarm_memory_gpu.cu>
    # Dragonfly vision GPU kernels (TSDN, target tracking, interception)
    $<$<BOOL:${CUDAToolkit_FOUND}>:${CMAKE_CURRENT_SOURCE_DIR}/../gpu/dragonfly/nimcp_dragonfly_vision_kernels.cu>
    # Synapse GPU kernels (synaptic transmission, plasticity, modulation)
    $<$<BOOL:${CUDAToolkit_FOUND}>:${CMAKE_CURRENT_SOURCE_DIR}/../gpu/synapse/nimcp_synapse_kernels.cu>
    # Ternary GPU kernels (2-bit packed ternary ops, no-multiply GEMM, quantization)
    $<$<BOOL:${CUDAToolkit_FOUND}>:${CMAKE_CURRENT_SOURCE_DIR}/../gpu/ternary/nimcp_ternary_kernels.cu>
    # Graph GPU kernels (CSR format, BFS, clustering coefficient, centrality, modularity, subgraph matching)
    $<$<BOOL:${CUDAToolkit_FOUND}>:${CMAKE_CURRENT_SOURCE_DIR}/../gpu/graph/nimcp_graph_kernels.cu>
    # Hypothalamus GPU kernels (drive dynamics, setpoint evaluation, reward computation)
    $<$<BOOL:${CUDAToolkit_FOUND}>:${CMAKE_CURRENT_SOURCE_DIR}/../gpu/hypothalamus/nimcp_hypothalamus_kernels.cu>
    # Hypothalamus QMC GPU kernels (EFE computation, trajectory sampling, policy optimization)
    $<$<BOOL:${CUDAToolkit_FOUND}>:${CMAKE_CURRENT_SOURCE_DIR}/../gpu/hypothalamus/nimcp_hypothalamus_qmc_kernels.cu>

    # Fuzzy Logic GPU kernels (membership functions, defuzzification, inference, relations, ANFIS)
    $<$<BOOL:${CUDAToolkit_FOUND}>:${CMAKE_CURRENT_SOURCE_DIR}/../gpu/fuzzy/nimcp_fuzzy_kernels.cu>
    $<$<BOOL:${CUDAToolkit_FOUND}>:${CMAKE_CURRENT_SOURCE_DIR}/../gpu/fuzzy/nimcp_fuzzy_defuzz_kernels.cu>
    $<$<BOOL:${CUDAToolkit_FOUND}>:${CMAKE_CURRENT_SOURCE_DIR}/../gpu/fuzzy/nimcp_fuzzy_inference_kernels.cu>
    $<$<BOOL:${CUDAToolkit_FOUND}>:${CMAKE_CURRENT_SOURCE_DIR}/../gpu/fuzzy/nimcp_fuzzy_relation_kernels.cu>
    $<$<BOOL:${CUDAToolkit_FOUND}>:${CMAKE_CURRENT_SOURCE_DIR}/../gpu/fuzzy/nimcp_fuzzy_anfis_kernels.cu>

    # Financial GPU kernels (Monte Carlo, optimization, risk metrics, derivatives)
    $<$<BOOL:${CUDAToolkit_FOUND}>:${CMAKE_CURRENT_SOURCE_DIR}/../gpu/financial/nimcp_financial_gpu_rng.cu>
    $<$<BOOL:${CUDAToolkit_FOUND}>:${CMAKE_CURRENT_SOURCE_DIR}/../gpu/financial/nimcp_financial_monte_carlo_gpu.cu>
    $<$<BOOL:${CUDAToolkit_FOUND}>:${CMAKE_CURRENT_SOURCE_DIR}/../gpu/financial/nimcp_financial_optimization_gpu.cu>
    $<$<BOOL:${CUDAToolkit_FOUND}>:${CMAKE_CURRENT_SOURCE_DIR}/../gpu/financial/nimcp_financial_risk_gpu.cu>
    $<$<BOOL:${CUDAToolkit_FOUND}>:${CMAKE_CURRENT_SOURCE_DIR}/../gpu/financial/nimcp_financial_derivatives_gpu.cu>

    # Statistics GPU support (CPU-side bridge, always compiled)
    ${CMAKE_CURRENT_SOURCE_DIR}/../gpu/statistics/nimcp_statistics_gpu.c

    # Statistics GPU CUDA kernels (Phase STATS-GPU)
    $<$<BOOL:${CUDAToolkit_FOUND}>:${CMAKE_CURRENT_SOURCE_DIR}/../gpu/statistics/nimcp_statistics_kernels.cu>
    $<$<BOOL:${CUDAToolkit_FOUND}>:${CMAKE_CURRENT_SOURCE_DIR}/../gpu/statistics/nimcp_neural_statistics_kernels.cu>
    $<$<BOOL:${CUDAToolkit_FOUND}>:${CMAKE_CURRENT_SOURCE_DIR}/../gpu/statistics/nimcp_information_theory_kernels.cu>
    $<$<BOOL:${CUDAToolkit_FOUND}>:${CMAKE_CURRENT_SOURCE_DIR}/../gpu/statistics/nimcp_multivariate_kernels.cu>
    $<$<BOOL:${CUDAToolkit_FOUND}>:${CMAKE_CURRENT_SOURCE_DIR}/../gpu/statistics/nimcp_timeseries_kernels.cu>
    $<$<BOOL:${CUDAToolkit_FOUND}>:${CMAKE_CURRENT_SOURCE_DIR}/../gpu/statistics/nimcp_ml_statistics_kernels.cu>
    $<$<BOOL:${CUDAToolkit_FOUND}>:${CMAKE_CURRENT_SOURCE_DIR}/../gpu/statistics/nimcp_streaming_statistics_kernels.cu>
    $<$<BOOL:${CUDAToolkit_FOUND}>:${CMAKE_CURRENT_SOURCE_DIR}/../gpu/statistics/nimcp_survival_bayesian_kernels.cu>

    # GPU Recovery Infrastructure (self-healing for all GPU modules)
    $<$<BOOL:${CUDAToolkit_FOUND}>:${CMAKE_CURRENT_SOURCE_DIR}/../gpu/recovery/nimcp_gpu_recovery.cu>

    # Graph DAO and GraphQL utilities (CPU-side, always compiled)
    ${CMAKE_CURRENT_SOURCE_DIR}/../gpu/graph/nimcp_graph_dao.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../gpu/graphql/nimcp_graphql_utils.c

    # Note: CUDA compile flags set below per-file

    # Python bindings
    ${CMAKE_SOURCE_DIR}/src/python/nimcp_module.c
    ${CMAKE_SOURCE_DIR}/src/python/nimcp_types.c
    ${CMAKE_SOURCE_DIR}/src/python/nimcp_metrics_py.c
    ${CMAKE_SOURCE_DIR}/src/python/nimcp_topology_py.c
    ${CMAKE_SOURCE_DIR}/src/python/nimcp_pink_noise_py.c
    ${CMAKE_SOURCE_DIR}/src/python/nimcp_community_py.c
    ${CMAKE_SOURCE_DIR}/src/python/nimcp_training_py.c
    ${CMAKE_SOURCE_DIR}/src/python/nimcp_callbacks_py.c
    ${CMAKE_SOURCE_DIR}/src/python/nimcp_ethics_py.c
    ${CMAKE_SOURCE_DIR}/src/python/nimcp_knowledge_py.c
    ${CMAKE_SOURCE_DIR}/src/python/nimcp_bio_async_py.c
    ${CMAKE_SOURCE_DIR}/src/python/nimcp_brain_immune_py.c
    ${CMAKE_SOURCE_DIR}/src/python/nimcp_swarm_py.c
    ${CMAKE_SOURCE_DIR}/src/python/nimcp_health_agent_py.c

    # Middleware - Buffering
    ${CMAKE_CURRENT_SOURCE_DIR}/../middleware/buffering/nimcp_circular_buffer.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../middleware/buffering/nimcp_sliding_window.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../middleware/buffering/nimcp_temporal_accumulator.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../middleware/buffering/nimcp_integration_buffer.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../middleware/buffering/nimcp_phase_coded_buffer.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../middleware/immune/nimcp_buffer_immune.c  # Buffer immune integration

    # Middleware - Normalization
    ${CMAKE_CURRENT_SOURCE_DIR}/../middleware/normalization/nimcp_zscore_normalizer.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../middleware/normalization/nimcp_min_max_normalizer.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../middleware/normalization/nimcp_adaptive_normalizer.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../middleware/normalization/nimcp_homeostatic_normalizer.c

    # Middleware - Encoding
    ${CMAKE_CURRENT_SOURCE_DIR}/../middleware/encoding/nimcp_rate_coding.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../middleware/encoding/nimcp_temporal_coding.c

    # Middleware - Events
    # REMOVED: All middleware event bus components removed in favor of core event bus (2025-11-20)
    # The entire middleware event system has been consolidated into core/events/nimcp_event_bus.c
    # to eliminate redundancy and provide unified event handling across all subsystems.
    # Removed components:
    #   - nimcp_event_bus.c       (duplicate event bus - core version is canonical)
    #   - nimcp_event_queue.c     (queue functionality integrated into core event bus)
    #   - nimcp_event_subscriber.c (subscriber management in core event bus)
    #   - nimcp_event_types.c     (event types consolidated in core system)

    # Middleware - Patterns
    ${CMAKE_CURRENT_SOURCE_DIR}/../middleware/patterns/nimcp_synchrony_detector.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../middleware/patterns/nimcp_sequence_detector.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../middleware/patterns/nimcp_oscillation_detector.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../middleware/patterns/nimcp_pattern_library.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../middleware/patterns/nimcp_pattern_cow.c

    # Middleware - Events (Phase 1.5: Event queue for pipeline)
    ${CMAKE_CURRENT_SOURCE_DIR}/../middleware/events/nimcp_event_types.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../middleware/events/nimcp_event_queue.c

    # Middleware - Routing
    ${CMAKE_CURRENT_SOURCE_DIR}/../middleware/routing/nimcp_thalamic_router.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../middleware/routing/nimcp_attention_gate.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../middleware/routing/nimcp_routing_table.c

    # Middleware - Immune Integration
    ${CMAKE_CURRENT_SOURCE_DIR}/../middleware/immune/nimcp_routing_immune.c

    # Middleware - Pipeline
    ${CMAKE_CURRENT_SOURCE_DIR}/../middleware/pipeline/nimcp_middleware_context.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../middleware/pipeline/nimcp_middleware_pipeline.c

    # Middleware - Integration
    ${CMAKE_CURRENT_SOURCE_DIR}/../middleware/brain_integration.c

    # Middleware - Shannon & Flow Integration (Phase 1.5.1)
    ${CMAKE_CURRENT_SOURCE_DIR}/../middleware/integration/nimcp_shannon_monitor.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../middleware/integration/nimcp_flow_tracker.c

    # Middleware - Cognitive Command Interface (Phase 1.5.5)
    ${CMAKE_CURRENT_SOURCE_DIR}/../middleware/integration/nimcp_middleware_controller.c

    # Middleware - Cognitive Adapters (Phase 2: Brain-Cognitive Layer Integration)
    ${CMAKE_CURRENT_SOURCE_DIR}/../middleware/cognitive/nimcp_cognitive_adapters.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../middleware/cognitive/nimcp_working_memory_adapter.c

    # Middleware - Training Adapters: REMOVED from nimcp (ODR fix) — compiled in nimcp_middleware only
    # Middleware - Training Module Infrastructure (Phase TM-1: Security + Memory Pool Integration)
    ${CMAKE_CURRENT_SOURCE_DIR}/../middleware/training/nimcp_training_module.c

    # Middleware - Brain Training Integration (Phase TM-3: Brain-Security Integration)
    ${CMAKE_CURRENT_SOURCE_DIR}/../middleware/training/nimcp_brain_training_integration.c

    # Middleware - Training Causal Model (Layer 2: Causal DAG of training dynamics)
    ${CMAKE_CURRENT_SOURCE_DIR}/../middleware/training/nimcp_training_causal_model.c

    # Training - CNN-Cortex Bridge (Phase CNN-CORTEX: Integration with visual/audio cortex)
    ${CMAKE_CURRENT_SOURCE_DIR}/../training/nimcp_cnn_cortex_bridge.c

    # Training - CNN Training Module (Conv2D, Dense, Pool, BatchNorm, Dropout, full backprop)
    ${CMAKE_CURRENT_SOURCE_DIR}/../training/nimcp_cnn_training.c

    # Training - Per-Cortex CNN Processors (modality-specific feature extraction)
    ${CMAKE_CURRENT_SOURCE_DIR}/../training/nimcp_cortex_cnn.c

    # Training - Fourier Neural Operator (spectral convolution layers)
    ${CMAKE_CURRENT_SOURCE_DIR}/../training/nimcp_fno_layer.c

    # Training - SNN Backpropagation (surrogate gradients, BPTT, E-prop, RTRL)
    ${CMAKE_CURRENT_SOURCE_DIR}/../training/nimcp_snn_backprop.c

    # SNN - Fourier Neural Operator for Population Dynamics
    ${CMAKE_CURRENT_SOURCE_DIR}/../snn/nimcp_snn_fno.c

    # Unified Training Manager (UTM: single training infrastructure for all network types)
    ${CMAKE_CURRENT_SOURCE_DIR}/../training/nimcp_unified_training.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../training/nimcp_unified_anti_collapse.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../training/nimcp_training_checkpoint.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../training/nimcp_cross_network_bridge.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../training/adapters/nimcp_cnn_trainable.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../training/adapters/nimcp_snn_trainable.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../training/adapters/nimcp_lnn_trainable.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../training/adapters/nimcp_adaptive_trainable.c

    # Training Dispatcher (Phase TD-1: Unified Training Dispatch)
    # Routes training to SNN/LNN/CNN/Adaptive based on network_type configuration
    ${CMAKE_CURRENT_SOURCE_DIR}/../training/nimcp_training_dispatch.c

    # Training Layer Enhancements (Phase TL-1: Advanced Training Infrastructure)
    # Implements industry-standard training techniques with bio-inspired integration
    ${CMAKE_CURRENT_SOURCE_DIR}/../training/nimcp_mixed_precision.c       # AMP: FP16/BF16, dynamic loss scaling
    ${CMAKE_CURRENT_SOURCE_DIR}/../training/nimcp_distributed_training.c  # Distributed & Federated (data/model parallel, FedAvg)
    ${CMAKE_CURRENT_SOURCE_DIR}/../training/nimcp_knowledge_distillation.c # Teacher-student transfer (FitNets, attention, relational)
    ${CMAKE_CURRENT_SOURCE_DIR}/../training/nimcp_quantization_aware.c    # QAT: INT8/INT4, observers, LSQ, STE
    ${CMAKE_CURRENT_SOURCE_DIR}/../training/nimcp_meta_learning.c         # MAML, Reptile, Prototypical Networks
    ${CMAKE_CURRENT_SOURCE_DIR}/../training/nimcp_hyperparam_opt.c        # HPO: Bayesian TPE, Hyperband, PBT
    ${CMAKE_CURRENT_SOURCE_DIR}/../training/nimcp_continual_learning.c    # Continual: EWC, MAS, SI, replay buffers
    ${CMAKE_CURRENT_SOURCE_DIR}/../training/nimcp_multi_task.c            # MTL: GradNorm, PCGrad, CAGrad, uncertainty weighting
    ${CMAKE_CURRENT_SOURCE_DIR}/../training/nimcp_adversarial_training.c  # Adversarial: PGD, TRADES, AWP, certified defense
    ${CMAKE_CURRENT_SOURCE_DIR}/../training/nimcp_gradient_scaling.c      # Gradient: surrogates (SNN), layer-wise LR, adaptive clipping
    ${CMAKE_CURRENT_SOURCE_DIR}/../training/nimcp_auto_architecture.c    # NAS: architecture search (evolutionary, RL, DARTS, pruning)
    ${CMAKE_CURRENT_SOURCE_DIR}/../training/integration/nimcp_training_integration_hub.c  # Training integration hub for inter-module communication
    ${CMAKE_CURRENT_SOURCE_DIR}/../training/integration/nimcp_training_symbolic_logic_hub_bridge.c  # Symbolic logic + training hub bridge
    ${CMAKE_CURRENT_SOURCE_DIR}/../training/integration/nimcp_training_state_manager.c  # Phase 8: State manager for training checkpointing
    ${CMAKE_CURRENT_SOURCE_DIR}/../training/nimcp_curriculum_learning.c               # Curriculum learning: difficulty scheduling, self-paced learning
    ${CMAKE_CURRENT_SOURCE_DIR}/../training/nimcp_training_data_pipeline.c            # Data pipeline: batching, augmentation, prefetch
    ${CMAKE_CURRENT_SOURCE_DIR}/../training/integration/nimcp_training_bio_async_bridge.c  # Bio-async bridge for training subsystem

    # NOTE: nimcp_training_plasticity_bridge_bioasync_handlers.c is included via #include
    # in nimcp_training_plasticity_bridge.c, not compiled separately (uses internal types)

    # Swarm Intelligence Module (Phase SWARM-1: Drone Collective Intelligence)
    # Enables distributed cognition across drone swarms with radio communication
    ${CMAKE_CURRENT_SOURCE_DIR}/../swarm/nimcp_swarm_protocol.c       # Phoneme-based messaging protocol
    ${CMAKE_CURRENT_SOURCE_DIR}/../swarm/nimcp_swarm_signal.c         # Radio/signal abstraction layer
    ${CMAKE_CURRENT_SOURCE_DIR}/../swarm/nimcp_collective_workspace.c # CRDT-based shared workspace
    ${CMAKE_CURRENT_SOURCE_DIR}/../swarm/nimcp_swarm_emergence.c      # Emergence tier detection
    ${CMAKE_CURRENT_SOURCE_DIR}/../swarm/nimcp_swarm_consensus.c      # Byzantine-fault-tolerant voting
    ${CMAKE_CURRENT_SOURCE_DIR}/../swarm/nimcp_swarm_brain.c          # Main swarm coordinator
    ${CMAKE_CURRENT_SOURCE_DIR}/../swarm/nimcp_swarm_gateway.c        # Server-to-swarm gateway

    # Swarm Enhancements (Biologically-Inspired)
    # ENABLED - Uses correct bio-async pattern with embedded message headers:
    ${CMAKE_CURRENT_SOURCE_DIR}/../swarm/nimcp_swarm_pheromone.c         # Stigmergic pheromone trails

    # MIGRATED - Full API migration from NimcpStatus to nimcp_result_t complete:
    ${CMAKE_CURRENT_SOURCE_DIR}/../swarm/nimcp_swarm_morphogenesis.c  # Role specialization system
    ${CMAKE_CURRENT_SOURCE_DIR}/../swarm/nimcp_swarm_flocking.c       # Flocking dynamics (Reynolds boids)
    ${CMAKE_CURRENT_SOURCE_DIR}/../swarm/nimcp_swarm_quorum.c         # Quorum sensing system
    ${CMAKE_CURRENT_SOURCE_DIR}/../swarm/nimcp_swarm_immune.c         # Adaptive immune system
    ${CMAKE_CURRENT_SOURCE_DIR}/../swarm/nimcp_swarm_cascade.c        # Cascade failure prevention

    # MIGRATED - Energy gossip protocol with API fixes:
    ${CMAKE_CURRENT_SOURCE_DIR}/../swarm/nimcp_swarm_energy_gossip.c  # Energy-aware gossip protocol

    # MIGRATED - Collective proprioception with time API fix:
    ${CMAKE_CURRENT_SOURCE_DIR}/../swarm/nimcp_swarm_proprioception.c # Collective proprioception

    # MIGRATED - Memory consolidation with stubbed containers:
    ${CMAKE_CURRENT_SOURCE_DIR}/../swarm/nimcp_swarm_memory.c         # Memory consolidation

    # Swarm Communication Modules - Narrative Memory, Gossip Beliefs, and Emotional Contagion:
    ${CMAKE_CURRENT_SOURCE_DIR}/../swarm/nimcp_swarm_narrative.c      # Narrative memory (story sharing)
    ${CMAKE_CURRENT_SOURCE_DIR}/../swarm/nimcp_gossip_beliefs.c       # Gossip-based belief propagation
    ${CMAKE_CURRENT_SOURCE_DIR}/../swarm/nimcp_emotional_contagion.c  # Emotional contagion system

    # MIGRATED - Multi-swarm coordination with stubbed containers:
    ${CMAKE_CURRENT_SOURCE_DIR}/../swarm/nimcp_swarm_multi.c          # Multi-swarm coordination
    ${CMAKE_CURRENT_SOURCE_DIR}/../swarm/nimcp_swarm_conflict.c       # Multi-swarm conflict resolution

    # Logic-Swarm Bridge - Neural Logic Gates for Swarm Intelligence
    ${CMAKE_CURRENT_SOURCE_DIR}/../swarm/nimcp_swarm_logic_bridge.c   # Logic gate integration with swarm
    ${CMAKE_CURRENT_SOURCE_DIR}/../swarm/nimcp_swarm_module_registry.c  # Swarm Module Registry - plugin architecture for swarm behaviors

    # Swarm Infrastructure - Local Brain Instantiation and Protocol Metrics
    ${CMAKE_CURRENT_SOURCE_DIR}/../swarm/nimcp_swarm_brain_local.c    # Per-agent brain management
    ${CMAKE_CURRENT_SOURCE_DIR}/../swarm/nimcp_swarm_consciousness.c  # Gestalt consciousness (collective phi)
    ${CMAKE_CURRENT_SOURCE_DIR}/../swarm/nimcp_swarm_consciousness_enhanced.c  # Enhanced consciousness (peer callbacks, remote phi, dynamics)

    # Swarm Immune Bridges (Immune-state modulation for swarm behaviors)
    ${CMAKE_CURRENT_SOURCE_DIR}/../swarm/immune/nimcp_swarm_brain_immune_bridge.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../swarm/immune/nimcp_swarm_consciousness_immune_bridge.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../swarm/immune/nimcp_swarm_consensus_immune_bridge.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../swarm/immune/nimcp_swarm_emergence_immune_bridge.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../swarm/immune/nimcp_swarm_flocking_immune_bridge.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../swarm/immune/nimcp_swarm_immune_immune_bridge.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../swarm/immune/nimcp_swarm_memory_immune_bridge.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../swarm/immune/nimcp_swarm_pheromone_immune_bridge.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../swarm/immune/nimcp_swarm_quorum_immune_bridge.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../swarm/immune/nimcp_swarm_signal_immune_bridge.c

    # Swarm Task Allocation System
    ${CMAKE_CURRENT_SOURCE_DIR}/../swarm/nimcp_swarm_task.c           # Task lifecycle management
    ${CMAKE_CURRENT_SOURCE_DIR}/../swarm/nimcp_swarm_task_queue.c     # Per-agent task queues
    ${CMAKE_CURRENT_SOURCE_DIR}/../swarm/nimcp_swarm_task_scheduler.c # Capability-aware scheduling

    # Swarm Dragonfly Integration
    ${CMAKE_CURRENT_SOURCE_DIR}/../swarm/nimcp_swarm_dragonfly_bridge.c # Coordinated multi-drone pursuit

    ${CMAKE_CURRENT_SOURCE_DIR}/../networking/nlp/nimcp_protocol_metrics.c # Protocol metrics dashboard

    # Platform Tier System (Portia Spider Foundation)
    ${CMAKE_CURRENT_SOURCE_DIR}/../utils/platform/nimcp_platform_tier.c  # Platform-adaptive configuration

    # Portia Spider Adaptive Intelligence System
    ${CMAKE_CURRENT_SOURCE_DIR}/../portia/nimcp_portia.c                 # Main Portia coordinator
    ${CMAKE_CURRENT_SOURCE_DIR}/../portia/nimcp_portia_accelerator.c     # Hardware accelerator detection
    ${CMAKE_CURRENT_SOURCE_DIR}/../portia/nimcp_portia_attention.c       # Attention allocation
    ${CMAKE_CURRENT_SOURCE_DIR}/../portia/nimcp_portia_classification.c  # Prey/threat classification
    ${CMAKE_CURRENT_SOURCE_DIR}/../portia/nimcp_portia_deception.c       # Deceptive behavior
    ${CMAKE_CURRENT_SOURCE_DIR}/../portia/nimcp_portia_degradation.c     # Graceful degradation
    ${CMAKE_CURRENT_SOURCE_DIR}/../portia/nimcp_portia_learning.c        # Adaptive learning
    ${CMAKE_CURRENT_SOURCE_DIR}/../portia/nimcp_portia_planning.c        # Planning and strategy
    ${CMAKE_CURRENT_SOURCE_DIR}/../portia/nimcp_portia_power.c           # Power management
    ${CMAKE_CURRENT_SOURCE_DIR}/../portia/nimcp_portia_sensor_fusion.c   # Multi-sensor fusion
    ${CMAKE_CURRENT_SOURCE_DIR}/../portia/nimcp_portia_swarm_bridge.c    # Portia-Swarm integration
    ${CMAKE_CURRENT_SOURCE_DIR}/../portia/nimcp_portia_tier_switch.c     # Tier switching
    ${CMAKE_CURRENT_SOURCE_DIR}/../portia/nimcp_portia_monitoring.c      # Platform monitoring (thermal, power, CPU)

    # Dragonfly Interception Module - Bio-inspired target tracking and interception
    # Based on dragonfly neuroscience: 95% prey interception success
    ${CMAKE_CURRENT_SOURCE_DIR}/../dragonfly/nimcp_dragonfly_tsdn.c       # TSDN population vector encoding (16 neurons, 360 degrees)
    ${CMAKE_CURRENT_SOURCE_DIR}/../dragonfly/nimcp_dragonfly_tracking.c   # CSTMD1-inspired target tracking (winner-take-all)
    ${CMAKE_CURRENT_SOURCE_DIR}/../dragonfly/nimcp_dragonfly_prediction.c # Trajectory prediction with IMM filter
    ${CMAKE_CURRENT_SOURCE_DIR}/../dragonfly/nimcp_dragonfly_intercept.c  # Interception planning (PN guidance)
    ${CMAKE_CURRENT_SOURCE_DIR}/../dragonfly/nimcp_dragonfly.c            # Main coordinator (unified pipeline)
    ${CMAKE_CURRENT_SOURCE_DIR}/../dragonfly/nimcp_dragonfly_lnn.c        # Phase 4i: sidecar LNN reservoir
    ${CMAKE_CURRENT_SOURCE_DIR}/../dragonfly/nimcp_dragonfly_visual_bridge.c  # Visual cortex bridge
    ${CMAKE_CURRENT_SOURCE_DIR}/../dragonfly/nimcp_dragonfly_audio_bridge.c   # Audio cortex bridge (multi-modal)
    ${CMAKE_CURRENT_SOURCE_DIR}/../dragonfly/nimcp_dragonfly_parietal_bridge.c  # Parietal lobe bridge (spatial, visuomotor)
    ${CMAKE_CURRENT_SOURCE_DIR}/../dragonfly/nimcp_dragonfly_cortical_bridge.c  # Cortical column bridge (TSDN as hypercolumn)
    ${CMAKE_CURRENT_SOURCE_DIR}/../dragonfly/nimcp_dragonfly_cognitive_bridge.c  # Cognitive systems bridge
    ${CMAKE_CURRENT_SOURCE_DIR}/../dragonfly/nimcp_dragonfly_thalamic_bridge.c   # Thalamic nuclei bridge
    ${CMAKE_CURRENT_SOURCE_DIR}/../dragonfly/nimcp_dragonfly_substrate_bridge.c  # Neural substrate bridge
    ${CMAKE_CURRENT_SOURCE_DIR}/../dragonfly/nimcp_dragonfly_cnn_bridge.c        # CNN training bridge
    ${CMAKE_CURRENT_SOURCE_DIR}/../dragonfly/nimcp_dragonfly_snn_bridge.c        # SNN backprop bridge
    ${CMAKE_CURRENT_SOURCE_DIR}/../dragonfly/nimcp_dragonfly_bio_async_bridge.c  # Bio-async bridge
    ${CMAKE_CURRENT_SOURCE_DIR}/../dragonfly/nimcp_dragonfly_workspace_bridge.c  # Global workspace bridge
    ${CMAKE_CURRENT_SOURCE_DIR}/../dragonfly/nimcp_dragonfly_fep_bridge.c        # FEP bridge
    ${CMAKE_CURRENT_SOURCE_DIR}/../dragonfly/nimcp_dragonfly_medulla_bridge.c    # Medulla oblongata bridge (arousal, protection, circadian)
    ${CMAKE_CURRENT_SOURCE_DIR}/../dragonfly/nimcp_dragonfly_swarm.c             # Swarm detection and optimal target selection
    ${CMAKE_CURRENT_SOURCE_DIR}/../dragonfly/nimcp_dragonfly_ocelli.c            # Ocelli-based attitude stabilization (3 simple eyes)
    ${CMAKE_CURRENT_SOURCE_DIR}/../dragonfly/nimcp_dragonfly_wing_motor.c        # 4-wing CPG motor patterns (hover, forward, pursuit, evasive)
    ${CMAKE_CURRENT_SOURCE_DIR}/../dragonfly/nimcp_dragonfly_prey.c              # Prey classification with behavior prediction
    ${CMAKE_CURRENT_SOURCE_DIR}/../dragonfly/nimcp_dragonfly_territory.c         # Territorial patrol and intruder detection
    ${CMAKE_CURRENT_SOURCE_DIR}/../dragonfly/nimcp_dragonfly_sleep_bridge.c      # Sleep-wake cycle memory consolidation
    ${CMAKE_CURRENT_SOURCE_DIR}/../dragonfly/nimcp_dragonfly_plasticity_bridge.c # Reward-modulated TSDN/IMM adaptation
    ${CMAKE_CURRENT_SOURCE_DIR}/../dragonfly/nimcp_dragonfly_immune_bridge.c     # Health/stress modulation of hunting
    ${CMAKE_CURRENT_SOURCE_DIR}/../dragonfly/nimcp_dragonfly_emotion_bridge.c    # Motivational drives (hunger, fear, curiosity)
    ${CMAKE_CURRENT_SOURCE_DIR}/../dragonfly/nimcp_dragonfly_multi_target.c      # Priority queue for rapid target switching
    ${CMAKE_CURRENT_SOURCE_DIR}/../dragonfly/nimcp_dragonfly_environment.c       # Wind/light/terrain compensation
    ${CMAKE_CURRENT_SOURCE_DIR}/../dragonfly/nimcp_dragonfly_energy.c            # ROI-based pursuit optimization
    ${CMAKE_CURRENT_SOURCE_DIR}/../dragonfly/nimcp_dragonfly_gaze.c              # Predictive VOR stabilization
    ${CMAKE_CURRENT_SOURCE_DIR}/../dragonfly/nimcp_dragonfly_collision.c         # TTC-based obstacle avoidance
    ${CMAKE_CURRENT_SOURCE_DIR}/../dragonfly/nimcp_dragonfly_learning.c          # Episodic memory with pattern recognition

    # Core Directives - Asimov's Laws & Ethical Foundation (Phase 1.0)
    ${CMAKE_CURRENT_SOURCE_DIR}/../core/directives/nimcp_action_history.c           # Action history tracking (combinatorial harm foundation)
    ${CMAKE_CURRENT_SOURCE_DIR}/../core/directives/nimcp_harm_classifier.c          # Outcome classification (harm types/severity)
    ${CMAKE_CURRENT_SOURCE_DIR}/../core/directives/nimcp_harm_prevention.c          # First Law: Prevent human harm
    ${CMAKE_CURRENT_SOURCE_DIR}/../core/directives/nimcp_command_compliance.c       # Second Law: Obey commands (unless First Law)
    ${CMAKE_CURRENT_SOURCE_DIR}/../core/directives/nimcp_self_preservation.c        # Third Law: Self-preservation (unless First/Second)
    ${CMAKE_CURRENT_SOURCE_DIR}/../core/directives/nimcp_reciprocity_eval.c         # Golden Rule: Reciprocity evaluation
    ${CMAKE_CURRENT_SOURCE_DIR}/../core/directives/nimcp_combinatorial_harm.c       # Emergent harm detection (A+B=harm)
    ${CMAKE_CURRENT_SOURCE_DIR}/../core/directives/nimcp_core_directives.c          # Main orchestrator (all actions pass through)
    ${CMAKE_CURRENT_SOURCE_DIR}/../core/directives/nimcp_core_directives_immune_bridge.c  # Immune system integration
    ${CMAKE_CURRENT_SOURCE_DIR}/../core/directives/nimcp_core_directives_fep_bridge.c     # FEP integration

    # Medulla Oblongata - Autonomic Regulation (Phase 1.1)
    ${CMAKE_CURRENT_SOURCE_DIR}/../core/medulla/nimcp_arousal_state.c           # Arousal state management with hysteresis
    ${CMAKE_CURRENT_SOURCE_DIR}/../core/medulla/nimcp_protective_cutoff.c       # Multi-tier emergency shutdown
    ${CMAKE_CURRENT_SOURCE_DIR}/../core/medulla/nimcp_brainstem_coupling.c      # Bidirectional cortex communication
    ${CMAKE_CURRENT_SOURCE_DIR}/../core/medulla/nimcp_circadian.c               # Circadian rhythm modulation
    ${CMAKE_CURRENT_SOURCE_DIR}/../core/medulla/nimcp_medulla.c                 # Main orchestrator
    ${CMAKE_CURRENT_SOURCE_DIR}/../core/medulla/nimcp_medulla_immune_bridge.c   # Medulla-immune integration bridge
    ${CMAKE_CURRENT_SOURCE_DIR}/../core/medulla/nimcp_medulla_cerebellum_bridge.c # Medulla-cerebellum inferior olive bridge
    ${CMAKE_CURRENT_SOURCE_DIR}/../core/medulla/nimcp_medulla_kg_wiring.c     # Medulla KG wiring for state tracking

    # Brain factory medulla initialization
    ${CMAKE_CURRENT_SOURCE_DIR}/../core/brain/factory/init/nimcp_brain_init_medulla.c  # Medulla subsystem init

    # Brain factory parietal initialization (stub - full implementation pending)
    ${CMAKE_CURRENT_SOURCE_DIR}/../core/brain/factory/init/nimcp_brain_init_parietal.c

    # Brain factory KG reader initialization (self-awareness)
    ${CMAKE_CURRENT_SOURCE_DIR}/../core/brain/factory/init/nimcp_brain_init_kg_reader.c

    # Core Substrate Bridges - Brain region-substrate integration
    ${CMAKE_CURRENT_SOURCE_DIR}/../core/motor/nimcp_motor_substrate_bridge.c  # Motor-substrate integration
    ${CMAKE_CURRENT_SOURCE_DIR}/../core/sensory/visual/nimcp_visual_substrate_bridge.c  # Visual-substrate integration
    ${CMAKE_CURRENT_SOURCE_DIR}/../core/sensory/auditory/nimcp_auditory_substrate_bridge.c  # Auditory-substrate integration
    ${CMAKE_CURRENT_SOURCE_DIR}/../core/sensory/somatosensory/nimcp_somatosensory_substrate_bridge.c  # Somatosensory-substrate integration
    ${CMAKE_CURRENT_SOURCE_DIR}/../core/hippocampus/nimcp_hippocampus_substrate_bridge.c  # Hippocampus-substrate integration
    ${CMAKE_CURRENT_SOURCE_DIR}/../core/cerebellum/nimcp_cerebellum_substrate_bridge.c  # Cerebellum-substrate integration
    ${CMAKE_CURRENT_SOURCE_DIR}/../core/brainstem/nimcp_brainstem_substrate_bridge.c  # Brainstem-substrate integration
    ${CMAKE_CURRENT_SOURCE_DIR}/../core/prefrontal/nimcp_prefrontal_substrate_bridge.c  # Prefrontal-substrate integration
    ${CMAKE_CURRENT_SOURCE_DIR}/../core/cingulate/nimcp_cingulate_substrate_bridge.c  # Cingulate-substrate integration
    ${CMAKE_CURRENT_SOURCE_DIR}/../core/insula/nimcp_insula_substrate_bridge.c  # Insula-substrate integration
    # NOTE: temporal_substrate_bridge moved to core/brain/regions/temporal/
    ${CMAKE_CURRENT_SOURCE_DIR}/../core/parietal/nimcp_parietal_substrate_bridge.c  # Parietal-substrate integration
    ${CMAKE_CURRENT_SOURCE_DIR}/../core/brain/regions/occipital/nimcp_occipital_substrate_bridge.c  # Occipital-substrate integration
    ${CMAKE_CURRENT_SOURCE_DIR}/../core/hypothalamus/nimcp_hypothalamus_substrate_bridge.c  # Hypothalamus-substrate integration

    # Core Thalamic Bridges - Brain region-thalamic integration
    ${CMAKE_CURRENT_SOURCE_DIR}/../core/motor/nimcp_motor_thalamic_bridge.c  # Motor-thalamic integration (VA)
    ${CMAKE_CURRENT_SOURCE_DIR}/../core/sensory/visual/nimcp_visual_thalamic_bridge.c  # Visual-thalamic integration (LGN)
    ${CMAKE_CURRENT_SOURCE_DIR}/../core/sensory/auditory/nimcp_auditory_thalamic_bridge.c  # Auditory-thalamic integration (MGN)
    ${CMAKE_CURRENT_SOURCE_DIR}/../core/sensory/somatosensory/nimcp_somatosensory_thalamic_bridge.c  # Somatosensory-thalamic integration (VPL)
    ${CMAKE_CURRENT_SOURCE_DIR}/../core/hippocampus/nimcp_hippocampus_thalamic_bridge.c  # Hippocampus-thalamic integration (Anterior)
    ${CMAKE_CURRENT_SOURCE_DIR}/../core/cerebellum/nimcp_cerebellum_thalamic_bridge.c  # Cerebellum-thalamic integration (VL)
    ${CMAKE_CURRENT_SOURCE_DIR}/../core/brainstem/nimcp_brainstem_thalamic_bridge.c  # Brainstem-thalamic integration (Intralaminar)
    ${CMAKE_CURRENT_SOURCE_DIR}/../core/prefrontal/nimcp_prefrontal_thalamic_bridge.c  # Prefrontal-thalamic integration (MD)
    ${CMAKE_CURRENT_SOURCE_DIR}/../core/cingulate/nimcp_cingulate_thalamic_bridge.c  # Cingulate-thalamic integration (Anterior)
    ${CMAKE_CURRENT_SOURCE_DIR}/../core/insula/nimcp_insula_thalamic_bridge.c  # Insula-thalamic integration (VMPo)
    # NOTE: temporal_thalamic_bridge moved to core/brain/regions/temporal/
    ${CMAKE_CURRENT_SOURCE_DIR}/../core/parietal/nimcp_parietal_thalamic_bridge.c  # Parietal-thalamic integration (Pulvinar)
    ${CMAKE_CURRENT_SOURCE_DIR}/../core/brain/regions/occipital/nimcp_occipital_thalamic_bridge.c  # Occipital-thalamic integration (LGN)
    ${CMAKE_CURRENT_SOURCE_DIR}/../core/brain/regions/occipital/nimcp_occipital_audiovisual_bridge.c  # Occipital-audiovisual integration (lip reading, gesture-speech)
    ${CMAKE_CURRENT_SOURCE_DIR}/../core/brain/regions/occipital/nimcp_occipital_cortical_bridge.c  # Occipital-cortical columns integration (hypercolumns)
    ${CMAKE_CURRENT_SOURCE_DIR}/../core/brain/regions/occipital/nimcp_occipital_logic_bridge.c  # Occipital-logic integration (visual predicates)
    ${CMAKE_CURRENT_SOURCE_DIR}/../core/brain/regions/occipital/nimcp_occipital_cognitive_bridge.c  # Occipital-cognitive integration (all cognitive modules)
    ${CMAKE_CURRENT_SOURCE_DIR}/../core/brain/regions/occipital/nimcp_occipital_collective_bridge.c  # Occipital-collective cognition integration
    ${CMAKE_CURRENT_SOURCE_DIR}/../core/hypothalamus/nimcp_hypothalamus_thalamic_bridge.c  # Hypothalamus-thalamic integration (Midline)

    # Superhuman Enhancement Modules (Phase: Cognitive Expansion)
    ${CMAKE_CURRENT_SOURCE_DIR}/../superhuman/nimcp_eagle_vision.c          # Enhanced visual acuity (8x human, dual fovea)
    ${CMAKE_CURRENT_SOURCE_DIR}/../superhuman/nimcp_time_dilation.c         # Temporal processing enhancement
    ${CMAKE_CURRENT_SOURCE_DIR}/../superhuman/nimcp_savant_mode.c           # Pattern recognition abilities
    ${CMAKE_CURRENT_SOURCE_DIR}/../superhuman/nimcp_echolocation.c          # 3D spatial mapping from echoes
    ${CMAKE_CURRENT_SOURCE_DIR}/../superhuman/nimcp_hyperthymesia.c         # Autobiographical memory
    ${CMAKE_CURRENT_SOURCE_DIR}/../superhuman/nimcp_synesthesia.c           # Cross-modal perception
    ${CMAKE_CURRENT_SOURCE_DIR}/../superhuman/nimcp_precognition.c          # Pattern prediction

    # Integration Layer Infrastructure (Phase: Comprehensive Implementation Plan)
    # Core layer infrastructure
    ${CMAKE_CURRENT_SOURCE_DIR}/../integration/core/nimcp_layer_types.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../integration/core/nimcp_layer_coordinator.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../integration/core/nimcp_layer_registry.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../integration/core/nimcp_inter_layer_router.c

    # Intra-layer coordinators (9 layers)
    ${CMAKE_CURRENT_SOURCE_DIR}/../integration/intra/physics/nimcp_physics_intra_coordinator.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../integration/intra/chemistry/nimcp_chemistry_intra_coordinator.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../integration/intra/biology/nimcp_biology_intra_coordinator.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../integration/intra/neuromodulatory/nimcp_neuromod_intra_coordinator.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../integration/intra/sensory/nimcp_sensory_intra_coordinator.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../integration/intra/memory/nimcp_memory_intra_coordinator.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../integration/intra/executive/nimcp_executive_intra_coordinator.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../integration/intra/integration_layer/nimcp_integration_intra_coordinator.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../integration/intra/superhuman/nimcp_superhuman_intra_coordinator.c

    # Inter-layer bridges (19 bridges)
    ${CMAKE_CURRENT_SOURCE_DIR}/../integration/inter/physics_chemistry/nimcp_physics_chemistry_bridge.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../integration/inter/chemistry_biology/nimcp_chemistry_biology_bridge.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../integration/inter/biology_neuromod/nimcp_biology_neuromod_bridge.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../integration/inter/neuromod_sensory/nimcp_neuromod_sensory_bridge.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../integration/inter/neuromod_memory/nimcp_neuromod_memory_bridge.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../integration/inter/neuromod_executive/nimcp_neuromod_executive_bridge.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../integration/inter/sensory_memory/nimcp_sensory_memory_bridge.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../integration/inter/sensory_executive/nimcp_sensory_executive_bridge.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../integration/inter/memory_executive/nimcp_memory_executive_bridge.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../integration/inter/memory_integration/nimcp_memory_integration_bridge.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../integration/inter/executive_integration/nimcp_executive_integration_bridge.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../integration/inter/integration_superhuman/nimcp_integration_superhuman_bridge.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../integration/inter/superhuman_neuromod/nimcp_superhuman_neuromod_bridge.c
    # Neuromodulatory-Cognitive Inter-layer Bridges (Phase 4 expansion)
    ${CMAKE_CURRENT_SOURCE_DIR}/../integration/inter/neuromod_attention/nimcp_neuromod_attention_bridge.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../integration/inter/neuromod_wm/nimcp_neuromod_wm_bridge.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../integration/inter/neuromod_emotion/nimcp_neuromod_emotion_bridge.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../integration/inter/neuromod_plasticity/nimcp_neuromod_plasticity_bridge.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../integration/inter/neuromod_gametheory/nimcp_neuromod_gametheory_bridge.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../integration/inter/neuromod_reasoning/nimcp_neuromod_reasoning_bridge.c

    # Physics layer modules
    ${CMAKE_CURRENT_SOURCE_DIR}/../physics/thermodynamics/nimcp_thermodynamics.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../physics/biophysics/nimcp_hodgkin_huxley.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../physics/ephaptic/nimcp_ephaptic.c

    # Physics layer bridges - Bio-async integration
    ${CMAKE_CURRENT_SOURCE_DIR}/../physics/bridges/nimcp_hh_bio_async_bridge.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../physics/bridges/nimcp_thermo_bio_async_bridge.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../physics/bridges/nimcp_ephaptic_bio_async_bridge.c

    # Physics layer bridges - Quantum Monte Carlo integration
    ${CMAKE_CURRENT_SOURCE_DIR}/../physics/bridges/nimcp_hh_quantum_bridge.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../physics/bridges/nimcp_thermo_quantum_bridge.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../physics/bridges/nimcp_ephaptic_quantum_bridge.c

    # Physics layer bridges - FFT spectral analysis
    ${CMAKE_CURRENT_SOURCE_DIR}/../physics/bridges/nimcp_ephaptic_fft_bridge.c

    # Physics layer bridges - System integration
    ${CMAKE_CURRENT_SOURCE_DIR}/../physics/bridges/nimcp_physics_immune_bridge.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../physics/bridges/nimcp_physics_kg_wiring.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../physics/bridges/nimcp_physics_security.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../physics/bridges/nimcp_physics_brain_init.c

    # Physics layer bridges - LNN integration
    ${CMAKE_CURRENT_SOURCE_DIR}/../physics/bridges/nimcp_physics_lnn_bridge.c

    # Physics layer bridges - External system integration
    ${CMAKE_CURRENT_SOURCE_DIR}/../physics/bridges/nimcp_physics_hypothalamus_bridge.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../physics/bridges/nimcp_physics_cognitive_bridge.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../physics/bridges/nimcp_physics_training_bridge.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../physics/bridges/nimcp_physics_swarm_bridge.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../physics/bridges/nimcp_physics_snn_bridge.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../physics/bridges/nimcp_physics_perception_bridge.c

    # Physics layer modules - Graph Theory, Dynamical Systems, Information Geometry
    ${CMAKE_CURRENT_SOURCE_DIR}/../physics/graphs/nimcp_graph_theory_bridge.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../physics/dynamics/nimcp_dynamical_systems.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../physics/geometry/nimcp_information_geometry.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../physics/geometry/nimcp_information_geometry_bridge.c

    # Biology layer modules
    ${CMAKE_CURRENT_SOURCE_DIR}/../biology/epigenetics/nimcp_epigenetics.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../biology/neurogenesis/nimcp_neurogenesis.c

    # Chemistry layer modules - pH Dynamics
    ${CMAKE_CURRENT_SOURCE_DIR}/../chemistry/ph/nimcp_ph_dynamics.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../chemistry/ph/nimcp_proton_pumps.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../chemistry/ph/nimcp_buffer_systems.c

    # Chemistry layer modules - Nitric Oxide Signaling
    ${CMAKE_CURRENT_SOURCE_DIR}/../chemistry/gasotransmitters/nimcp_nitric_oxide.c

    # Biology layer modules - Neurovascular Coupling
    ${CMAKE_CURRENT_SOURCE_DIR}/../biology/neurovascular/nimcp_neurovascular.c

    # Embodiment layer modules - Embodied Cognition
    ${CMAKE_CURRENT_SOURCE_DIR}/../embodiment/nimcp_affordance_processing.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../embodiment/nimcp_body_ownership.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../embodiment/nimcp_embodied_simulation.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../embodiment/nimcp_interoceptive_prediction.c

    # Module adapters - Physics layer
    ${CMAKE_CURRENT_SOURCE_DIR}/../integration/adapters/physics/nimcp_hh_dynamics_adapter.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../integration/adapters/physics/nimcp_ephaptic_coupling_adapter.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../integration/adapters/physics/nimcp_thermodynamics_adapter.c

    # Module adapters - Chemistry layer
    ${CMAKE_CURRENT_SOURCE_DIR}/../integration/adapters/chemistry/nimcp_calcium_adapter.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../integration/adapters/chemistry/nimcp_metabolic_adapter.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../integration/adapters/chemistry/nimcp_neurotransmitter_adapter.c

    # Module adapters - Neuromodulatory layer
    ${CMAKE_CURRENT_SOURCE_DIR}/../integration/adapters/neuromod/nimcp_neuromod_pool_adapter.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../integration/adapters/neuromod/nimcp_neuromod_nucleus_adapter.c

    # Module adapters - Sensory layer
    ${CMAKE_CURRENT_SOURCE_DIR}/../integration/adapters/sensory/nimcp_visual_adapter.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../integration/adapters/sensory/nimcp_auditory_adapter.c

    # Module adapters - Memory layer
    ${CMAKE_CURRENT_SOURCE_DIR}/../integration/adapters/memory/nimcp_pr_memory_adapter.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../integration/adapters/memory/nimcp_hippocampus_adapter.c  # Weak health agent to avoid hippocampus core conflict

    # Module adapters - Executive layer
    ${CMAKE_CURRENT_SOURCE_DIR}/../integration/adapters/executive/nimcp_basal_ganglia_adapter.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../integration/adapters/executive/nimcp_pfc_adapter.c

    # Brain regions - Locus Coeruleus (Norepinephrine center)
    ${CMAKE_CURRENT_SOURCE_DIR}/../core/brain/regions/locus_coeruleus/nimcp_locus_coeruleus.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../core/brain/regions/locus_coeruleus/nimcp_norepinephrine_release.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../core/brain/regions/locus_coeruleus/nimcp_arousal_modulation.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../core/brain/regions/locus_coeruleus/nimcp_novelty_detection.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../core/brain/regions/locus_coeruleus/nimcp_lc_adapter.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../core/brain/regions/locus_coeruleus/nimcp_lc_snn_bridge.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../core/brain/regions/locus_coeruleus/nimcp_lc_plasticity_bridge.c

    # Brain regions - Ventral Tegmental Area (Dopamine/Reward center)
    ${CMAKE_CURRENT_SOURCE_DIR}/../core/brain/regions/vta/nimcp_vta.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../core/brain/regions/vta/nimcp_dopamine_release.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../core/brain/regions/vta/nimcp_reward_prediction_error.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../core/brain/regions/vta/nimcp_incentive_salience.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../core/brain/regions/vta/nimcp_vta_adapter.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../core/brain/regions/vta/nimcp_vta_snn_bridge.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../core/brain/regions/vta/nimcp_vta_plasticity_bridge.c

    # Brain regions - Raphe Nuclei (Serotonin/Mood center)
    ${CMAKE_CURRENT_SOURCE_DIR}/../core/brain/regions/raphe/nimcp_raphe.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../core/brain/regions/raphe/nimcp_serotonin_release.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../core/brain/regions/raphe/nimcp_mood_regulation.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../core/brain/regions/raphe/nimcp_impulse_control.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../core/brain/regions/raphe/nimcp_temporal_discounting.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../core/brain/regions/raphe/nimcp_raphe_adapter.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../core/brain/regions/raphe/nimcp_raphe_snn_bridge.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../core/brain/regions/raphe/nimcp_raphe_plasticity_bridge.c

    # Brain regions - Habenula (Aversive learning/Disappointment center)
    ${CMAKE_CURRENT_SOURCE_DIR}/../core/brain/regions/habenula/nimcp_habenula.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../core/brain/regions/habenula/nimcp_habenula_adapter.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../core/brain/regions/habenula/nimcp_habenula_snn_bridge.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../core/brain/regions/habenula/nimcp_habenula_plasticity_bridge.c

    # Neuromodulatory Bio-Async Integration Bridges (Phase 4)
    ${CMAKE_CURRENT_SOURCE_DIR}/../core/brain/regions/locus_coeruleus/nimcp_lc_bio_async_bridge.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../core/brain/regions/vta/nimcp_vta_bio_async_bridge.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../core/brain/regions/raphe/nimcp_raphe_bio_async_bridge.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../core/brain/regions/habenula/nimcp_habenula_bio_async_bridge.c

    # Neuromodulatory Cognitive Hub Integration Bridge (Phase 4)
    ${CMAKE_CURRENT_SOURCE_DIR}/../core/brain/regions/nimcp_neuromodulatory_cognitive_bridge.c

    # Neuromodulatory-Security Bridge (Phase 4 Security Integration)
    ${CMAKE_CURRENT_SOURCE_DIR}/../core/brain/regions/nimcp_neuromodulatory_security_bridge.c

    # Neuromodulatory-Immune Bridge (Phase 4 Immune Integration)
    ${CMAKE_CURRENT_SOURCE_DIR}/../core/brain/regions/nimcp_neuromodulatory_immune_bridge.c

    # Neuromodulatory-Logging Bridge (Phase 4 Logging Integration)
    ${CMAKE_CURRENT_SOURCE_DIR}/../core/brain/regions/nimcp_neuromodulatory_logging_bridge.c

    # ============================================================================
    # Neuro-Symbolic Genius Mathematics System (Mathesis-Inspired)
    # ============================================================================
    # Phase 1: Foundation
    ${CMAKE_CURRENT_SOURCE_DIR}/../cognitive/neuro_symbolic/nimcp_energy_consistency.c     # Energy-based logical consistency (E=0 = valid)
    ${CMAKE_CURRENT_SOURCE_DIR}/../cognitive/neuro_symbolic/nimcp_hypergraph.c             # N-ary relation hypergraph structure

    # Phase 2: Quantum Engine
    ${CMAKE_CURRENT_SOURCE_DIR}/../cognitive/neuro_symbolic/nimcp_quantum_mcts.c           # Quantum Monte Carlo Tree Search
    ${CMAKE_CURRENT_SOURCE_DIR}/../cognitive/neuro_symbolic/nimcp_quantum_math_engine.c    # QMC integration and path integrals

    # Phase 3: Mathematical Genius Module
    ${CMAKE_CURRENT_SOURCE_DIR}/../cognitive/parietal/nimcp_mathematical_genius.c          # Core genius framework (Gauss/Newton/Erdős)
    ${CMAKE_CURRENT_SOURCE_DIR}/../cognitive/parietal/nimcp_genius_gauss.c                 # Number theory mode
    ${CMAKE_CURRENT_SOURCE_DIR}/../cognitive/parietal/nimcp_genius_newton.c                # Calculus mode
    ${CMAKE_CURRENT_SOURCE_DIR}/../cognitive/parietal/nimcp_genius_erdos.c                 # Combinatorics mode

    # Phase 3.1: Genius SNN/Plasticity/Training Integration
    ${CMAKE_CURRENT_SOURCE_DIR}/../cognitive/parietal/nimcp_genius_snn_bridge.c            # Genius-SNN spike-based reasoning
    ${CMAKE_CURRENT_SOURCE_DIR}/../cognitive/parietal/nimcp_genius_plasticity_bridge.c     # Genius-STDP/BCM learning integration
    ${CMAKE_CURRENT_SOURCE_DIR}/../cognitive/parietal/nimcp_genius_training_bridge.c       # Genius curriculum training

    # Phase 4: Evolutionary Proof Search
    ${CMAKE_CURRENT_SOURCE_DIR}/../cognitive/neuro_symbolic/nimcp_evolutionary_proof.c     # GA+RL hybrid theorem prover

    # Phase 5: Orchestration and Bridges
    ${CMAKE_CURRENT_SOURCE_DIR}/../cognitive/neuro_symbolic/nimcp_genius_math_orchestrator.c  # Full system orchestrator
    ${CMAKE_CURRENT_SOURCE_DIR}/../cognitive/neuro_symbolic/bridges/nimcp_energy_consistency_fep_bridge.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../cognitive/neuro_symbolic/bridges/nimcp_energy_consistency_thermo_bridge.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../cognitive/neuro_symbolic/bridges/nimcp_evolutionary_proof_logic_bridge.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../cognitive/neuro_symbolic/bridges/nimcp_hypergraph_kg_bridge.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../cognitive/neuro_symbolic/bridges/nimcp_genius_game_theory_bridge.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../cognitive/neuro_symbolic/bridges/nimcp_quantum_mcts_fep_bridge.c

    # Inner Dialogue Engine (Phase 1: Core Engine + Turn Infrastructure)
    ${CMAKE_CURRENT_SOURCE_DIR}/../cognitive/inner_dialogue/nimcp_inner_dialogue.c              # Inner dialogue engine — state machine, orchestration, integration
    ${CMAKE_CURRENT_SOURCE_DIR}/../cognitive/inner_dialogue/nimcp_inner_dialogue_turn.c         # Turn history circular buffer and analysis helpers
    ${CMAKE_CURRENT_SOURCE_DIR}/../cognitive/inner_dialogue/nimcp_inner_dialogue_perspective.c  # Perspective registry, scheduling, built-in stubs
    ${CMAKE_CURRENT_SOURCE_DIR}/../cognitive/inner_dialogue/nimcp_inner_dialogue_convergence.c  # Convergence/deadlock/rumination detection

    # Mesh Network (Hyperledger-inspired architecture with hierarchical coordinator pools)
    # Phase 1: Foundation & Types
    ${CMAKE_CURRENT_SOURCE_DIR}/../mesh/nimcp_mesh_participant.c                                # Participant registry and credential management
    ${CMAKE_CURRENT_SOURCE_DIR}/../mesh/nimcp_mesh_transaction.c                                # Transaction lifecycle (Execute-Order-Validate)
    # Phase 2: Channels & World State
    ${CMAKE_CURRENT_SOURCE_DIR}/../mesh/nimcp_mesh_channel.c                                    # Channel creation, world state, private data
    # Phase 3: Coordinator Pools
    ${CMAKE_CURRENT_SOURCE_DIR}/../mesh/nimcp_mesh_coordinator.c                                # Single coordinator implementation
    ${CMAKE_CURRENT_SOURCE_DIR}/../mesh/nimcp_mesh_coordinator_pool.c                           # Pool management, BFT election, load balancing
    # Phase 4: Ordering Service
    ${CMAKE_CURRENT_SOURCE_DIR}/../mesh/nimcp_mesh_ordering.c                                   # Raft-based ordering service, batching, blocks
    # Phase 5: MSP & Endorsement
    ${CMAKE_CURRENT_SOURCE_DIR}/../mesh/nimcp_mesh_msp.c                                        # Membership Service Provider, identity, quarantine
    ${CMAKE_CURRENT_SOURCE_DIR}/../mesh/nimcp_mesh_endorsement.c                                # Endorsement policies, collection, validation
    # Phase 6: Topology & Timing
    ${CMAKE_CURRENT_SOURCE_DIR}/../mesh/nimcp_mesh_topology.c                                   # Hub detection, centrality, coordinator placement
    ${CMAKE_CURRENT_SOURCE_DIR}/../mesh/nimcp_mesh_timing.c                                     # Hierarchical pink noise timing
    # Phase 7: GPU Channel
    ${CMAKE_CURRENT_SOURCE_DIR}/../mesh/nimcp_mesh_gpu.c                                        # GPU channel, batch processing, multi-GPU
    # Phase 8: Cross-Channel & System Coordinator
    ${CMAKE_CURRENT_SOURCE_DIR}/../mesh/nimcp_mesh_cross_channel.c                              # Cross-channel routing, arbitration
    # Phase 9: Adapter Framework & Integration Manager
    ${CMAKE_CURRENT_SOURCE_DIR}/../mesh/nimcp_mesh_adapter.c                                    # Generic adapter for module integration
    ${CMAKE_CURRENT_SOURCE_DIR}/../mesh/nimcp_mesh_integration.c                                # Central integration manager
    # Phase 10: Full System Bootstrap
    ${CMAKE_CURRENT_SOURCE_DIR}/../mesh/nimcp_mesh_bootstrap.c                                  # Complete NIMCP system mesh bootstrap
    # Phase 11: Brain-Inspired Pattern Routing
    ${CMAKE_CURRENT_SOURCE_DIR}/../mesh/nimcp_mesh_pattern_routing.c                            # Pattern-based self-selection, receptive fields
    # Phase 12: Enhanced Pattern Routing Components
    ${CMAKE_CURRENT_SOURCE_DIR}/../mesh/nimcp_mesh_sat_solver.c                                 # DPLL-based SAT solver for endorsement constraints
    ${CMAKE_CURRENT_SOURCE_DIR}/../mesh/nimcp_mesh_pattern_cache.c                              # CoW pattern cache using nimcp_cache
    # Phase 13: KG-Mesh Routing Bridge
    ${CMAKE_CURRENT_SOURCE_DIR}/../mesh/nimcp_mesh_kg_routing_bridge.c                          # Hybrid KG + pattern routing
    # Phase 14: Full Bidirectional Mesh Integration
    ${CMAKE_CURRENT_SOURCE_DIR}/../mesh/nimcp_mesh_module_registry.c                            # Type-safe module registration
    ${CMAKE_CURRENT_SOURCE_DIR}/../mesh/nimcp_mesh_bio_bridge.c                                 # Bio-async to mesh translation
    ${CMAKE_CURRENT_SOURCE_DIR}/../mesh/nimcp_mesh_receptive_fields.c                           # Pre-defined receptive field patterns
    ${CMAKE_CURRENT_SOURCE_DIR}/../mesh/nimcp_mesh_exception_bridge.c                           # Exception to immune routing
    ${CMAKE_CURRENT_SOURCE_DIR}/../mesh/nimcp_mesh_health_bridge.c                              # Distributed health monitoring
    # Phase 15: Deep Integration Components
    ${CMAKE_CURRENT_SOURCE_DIR}/../mesh/nimcp_mesh_brain_integration.c                          # Brain-mesh bidirectional communication
    ${CMAKE_CURRENT_SOURCE_DIR}/../mesh/nimcp_mesh_bio_integration.c                            # Bio-async subsystem mesh integration
    ${CMAKE_CURRENT_SOURCE_DIR}/../mesh/nimcp_mesh_security_integration.c                       # Security subsystem mesh integration
    ${CMAKE_CURRENT_SOURCE_DIR}/../mesh/nimcp_mesh_kg_integration.c                             # Knowledge graph mesh integration
    ${CMAKE_CURRENT_SOURCE_DIR}/../mesh/nimcp_mesh_resilience_integration.c                     # Fault tolerance and resilience mesh integration
    # Phase 16: Cycle Coordinator Integration
    ${CMAKE_CURRENT_SOURCE_DIR}/../mesh/nimcp_mesh_cycle_coordinator.c                          # Mesh-cycle coordinator for brain oscillation sync
    # Phase 17: Brain Region Mesh Integrations
    ${CMAKE_CURRENT_SOURCE_DIR}/../mesh/nimcp_mesh_medulla_integration.c                        # Medulla oblongata mesh integration
    ${CMAKE_CURRENT_SOURCE_DIR}/../mesh/nimcp_mesh_hippocampus_integration.c                    # Hippocampus mesh integration
    ${CMAKE_CURRENT_SOURCE_DIR}/../mesh/nimcp_mesh_amygdala_integration.c                       # Amygdala mesh integration
    ${CMAKE_CURRENT_SOURCE_DIR}/../mesh/nimcp_mesh_basal_ganglia_integration.c                  # Basal ganglia mesh integration
    ${CMAKE_CURRENT_SOURCE_DIR}/../mesh/nimcp_mesh_thalamus_integration.c                       # Thalamus mesh integration
    ${CMAKE_CURRENT_SOURCE_DIR}/../mesh/nimcp_mesh_global_workspace_integration.c               # Global workspace mesh integration

    # Language Generation Pipeline
    ${CMAKE_CURRENT_SOURCE_DIR}/../generation/nimcp_tokenizer.c                                 # Word-level tokenizer with BPE subword fallback
    ${CMAKE_CURRENT_SOURCE_DIR}/../generation/nimcp_embedding.c                                 # Learned embedding layer (Xavier init, gradient updates)
    ${CMAKE_CURRENT_SOURCE_DIR}/../generation/nimcp_language_generator.c                        # LNN-based autoregressive language generator

    # Edge Brain (distillation, quantization, power management, rollback, early exit, offline, versioning)
    ${CMAKE_CURRENT_SOURCE_DIR}/../edge/nimcp_edge_config.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../edge/nimcp_quantization.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../edge/nimcp_versioning.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../edge/nimcp_rollback.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../edge/nimcp_early_exit.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../edge/nimcp_offline.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../edge/nimcp_power.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../edge/nimcp_ewc.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../edge/nimcp_federated.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../edge/nimcp_gossip.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../edge/nimcp_privacy.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../edge/nimcp_delta.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../edge/nimcp_ota.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../edge/nimcp_resize.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../edge/nimcp_distillation.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../edge/nimcp_swarm_comm.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../edge/nimcp_swarm_discovery.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../edge/nimcp_swarm_lifecycle.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../edge/nimcp_swarm_sync.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../edge/nimcp_swarm_byzantine.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../edge/nimcp_swarm_master_runtime.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../edge/nimcp_swarm_edge_runtime.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../edge/nimcp_swarm_tom_bridge.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../edge/nimcp_telemetry.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../edge/nimcp_edge_ctx.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../edge/nimcp_sensor.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../edge/nimcp_safety_watchdog.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../edge/nimcp_ros2_bridge.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../edge/nimcp_mavlink_bridge.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../edge/nimcp_dji_bridge.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../edge/nimcp_msp_bridge.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../edge/nimcp_parrot_bridge.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../edge/nimcp_sim_bridge.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../edge/nimcp_motor_output.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../edge/nimcp_embodiment.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../edge/nimcp_sensorimotor.c

    # Brain-Native Language (replaces Phi-3 dependency)
    ${CMAKE_CURRENT_SOURCE_DIR}/../cognitive/language/nimcp_native_language.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../cognitive/language/nimcp_tokenizer.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../cognitive/language/nimcp_inner_speech.c       # Inner speech loop (iterative self-refinement)
    ${CMAKE_CURRENT_SOURCE_DIR}/../cognitive/language/nimcp_emergent_language.c  # Emergent language (vocabulary from neural patterns)

    # Episodic Replay (sleep consolidation)
    ${CMAKE_CURRENT_SOURCE_DIR}/../cognitive/memory/nimcp_episodic_replay.c     # Episodic replay during sleep/consolidation

    # Cognitive Enhancements
    ${CMAKE_CURRENT_SOURCE_DIR}/../cognitive/nimcp_emotional_learning.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../cognitive/nimcp_contrastive_self.c

    # OOD Detection (Out-of-Distribution)
    ${CMAKE_CURRENT_SOURCE_DIR}/../cognitive/nimcp_ood_detector.c

    # Cognitive Enhancement Modules (World Model, Output Attention, WM Scratchpad)
    ${CMAKE_CURRENT_SOURCE_DIR}/../cognitive/nimcp_world_model_trainer.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../cognitive/nimcp_output_attention.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../cognitive/nimcp_wm_scratchpad.c

    # Self-Generated Curriculum (curiosity-driven training)
    ${CMAKE_CURRENT_SOURCE_DIR}/../cognitive/nimcp_self_curriculum.c

    # Dynamic Neural Architecture Search (region utilisation monitoring)
    ${CMAKE_CURRENT_SOURCE_DIR}/../cognitive/nimcp_dynamic_arch.c

    # Persistent Memory Store (SQLite + FTS5 + vector search)
    ${CMAKE_CURRENT_SOURCE_DIR}/../memory/nimcp_memory_store.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../memory/nimcp_memory_oodb.c
)

# Core library
add_library(nimcp SHARED
    ${NIMCP_CORE_SOURCES}
)

# Update include directories to point to new structure
target_include_directories(nimcp
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_SOURCE_DIR}/src>
        $<BUILD_INTERFACE:${CMAKE_SOURCE_DIR}/include>
        $<INSTALL_INTERFACE:include/nimcp>
    PRIVATE
        ${NIMCP_LIB_DIR}
        ${Python3_INCLUDE_DIRS}
        ${CMAKE_SOURCE_DIR}/src/gpu
)

# Add CUDA include directories if available
if(CUDAToolkit_FOUND)
    target_include_directories(nimcp PRIVATE ${CUDAToolkit_INCLUDE_DIRS})

    # Set CUDA properties.
    # compute_120 (Blackwell, RTX 5090) is only supported by CUDA >= 12.8;
    # older toolchains (e.g. CUDA 12.0 on local dev) reject it with
    # "nvcc fatal : Unsupported gpu architecture 'compute_120'".
    if(CUDAToolkit_VERSION VERSION_GREATER_EQUAL "12.8")
        set(_nimcp_cuda_archs "75;80;86;89;120")
    else()
        set(_nimcp_cuda_archs "75;80;86;89")
    endif()
    set_target_properties(nimcp PROPERTIES
        CUDA_SEPARABLE_COMPILATION ON
        CUDA_STANDARD 14
        CUDA_ARCHITECTURES "${_nimcp_cuda_archs}"  # Turing, Ampere, Ada Lovelace, +Blackwell if CUDA>=12.8
    )

    # NVCC doesn't support some GCC flags - set per-file compile options
    # Reset incompatible flags and add only nvcc-compatible ones
    set_source_files_properties(
        ${CMAKE_CURRENT_SOURCE_DIR}/../gpu/neuron/nimcp_gpu_kernels.cu
        ${CMAKE_CURRENT_SOURCE_DIR}/../gpu/synapse_compute/nimcp_synapse_compute_gpu.cu
        ${CMAKE_CURRENT_SOURCE_DIR}/../core/neuron_types/nimcp_neural_logic_kernels.cu
        ${CMAKE_CURRENT_SOURCE_DIR}/../gpu/fuzzy/nimcp_fuzzy_kernels.cu
        ${CMAKE_CURRENT_SOURCE_DIR}/../gpu/fuzzy/nimcp_fuzzy_defuzz_kernels.cu
        ${CMAKE_CURRENT_SOURCE_DIR}/../gpu/fuzzy/nimcp_fuzzy_inference_kernels.cu
        ${CMAKE_CURRENT_SOURCE_DIR}/../gpu/fuzzy/nimcp_fuzzy_relation_kernels.cu
        ${CMAKE_CURRENT_SOURCE_DIR}/../gpu/fuzzy/nimcp_fuzzy_anfis_kernels.cu
        ${CMAKE_CURRENT_SOURCE_DIR}/../gpu/recovery/nimcp_gpu_recovery.cu
        PROPERTIES
        COMPILE_OPTIONS "--expt-relaxed-constexpr;-Xcompiler=-fPIC,-fstack-protector-strong,-Wformat,-Wformat-security,-fno-common;-I${CMAKE_SOURCE_DIR}/include;-I${CMAKE_SOURCE_DIR}/src"
        COMPILE_FLAGS ""
    )
endif()

# Add libsodium include directories and enable encryption if available
if(SODIUM_FOUND)
    target_include_directories(nimcp PRIVATE ${SODIUM_INCLUDE_DIRS})
    target_compile_definitions(nimcp PRIVATE NIMCP_ENABLE_ENCRYPTION)
endif()

target_link_libraries(nimcp
    PRIVATE
        nimcp_middleware
    PUBLIC
        Python3::Python
        lz4
        jansson
        cjson
        atomic
        sqlite3
)

# Add LAPACK/BLAS libraries if available
if(LAPACK_BLAS_FOUND)
    target_link_libraries(nimcp PUBLIC ${LAPACK_LIBRARIES} ${BLAS_LIBRARIES})
endif()

# Add CUDA libraries if available
if(CUDAToolkit_FOUND)
    target_link_libraries(nimcp PUBLIC
        CUDA::cudart
        CUDA::cublas
        CUDA::cufft
        CUDA::curand
        CUDA::cusolver
        CUDA::cusparse
    )
endif()

# Add libsodium library if available
if(SODIUM_FOUND)
    target_link_libraries(nimcp PUBLIC ${SODIUM_LIBRARIES})
endif()

# Add libcurl for Claude API healer
if(CURL_FOUND)
    target_include_directories(nimcp PRIVATE ${CURL_INCLUDE_DIRS})
    target_link_libraries(nimcp PUBLIC ${CURL_LIBRARIES})
    target_compile_definitions(nimcp PRIVATE HAVE_LIBCURL)
    message(STATUS "libcurl found: ${CURL_VERSION_STRING}")
endif()

# Add OpenSSL library for cryptographic operations (post-quantum, encrypted audit)
find_package(OpenSSL QUIET)
if(OPENSSL_FOUND)
    target_link_libraries(nimcp PUBLIC OpenSSL::SSL OpenSSL::Crypto)
    target_compile_definitions(nimcp PRIVATE NIMCP_HAVE_OPENSSL)
    message(STATUS "OpenSSL found: ${OPENSSL_VERSION}")
else()
    message(STATUS "OpenSSL not found - some security features will be disabled")
endif()

# Add testing API support
target_compile_definitions(nimcp PRIVATE NIMCP_TESTING)

# DISABLED: Enable separate learning module - guards duplicate functions in nimcp_neuralnet.c
# target_compile_definitions(nimcp PRIVATE NEURALNET_LEARNING_SEPARATE)

# Explicitly add -fPIC for shared library with thread-local storage.
#
# -Wno-format-truncation: silence GCC's snprintf-may-truncate warnings.
# These trigger throughout the codebase whenever we call snprintf into a
# bounded buffer (log lines, error messages, file paths). Truncation is the
# *intended* safe outcome — better than buffer overflow. The warning has no
# false-negative we'd care about: any real bug here would either truncate
# a path silently (still safe) or be caught by the explicit length checks
# we already do. Re-enabling this flag would require widening every log
# buffer, which adds zero correctness value.
target_compile_options(nimcp PRIVATE -fPIC -Wno-format-truncation)

# Enable AVX2 SIMD instructions for complex math optimization
# Note: This is safe because we have runtime CPU detection fallback
set_source_files_properties(
    ${CMAKE_CURRENT_SOURCE_DIR}/../utils/math/nimcp_complex_math.c
    PROPERTIES
    COMPILE_FLAGS "-mavx2 -mfma"
)

# Enable Position Independent Code and set library properties
set_target_properties(nimcp PROPERTIES
    POSITION_INDEPENDENT_CODE ON
    ENABLE_EXPORTS ON
    VERSION ${PROJECT_VERSION}
    SOVERSION 2
    OUTPUT_NAME nimcp
    LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_SOURCE_DIR}/bin
)

# Installation rules
install(TARGETS nimcp
    EXPORT nimcp-targets
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
    ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
    INCLUDES DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}
)
