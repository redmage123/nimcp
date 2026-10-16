//=============================================================================
// nimcp_brain_cognitive.h - Cognitive Systems Subsystem Module (Extracted)
//=============================================================================
/**
 * @file nimcp_brain_cognitive.h
 * @brief Cognitive systems initialization and management
 *
 * EXTRACTED SUBSYSTEMS:
 * - Working Memory (Phase 10.1): Miller's 7±2 working memory with decay
 * - Theory of Mind (Phase 10.6): BDI model for social cognition
 * - Mirror Neurons (Phase 10.11): Observation-based learning and action understanding
 * - Autobiographical Memory (Phase 12): Episodic self-memory
 * - Self-Model (Phase 12): Explicit identity and self-representation
 * - Global Workspace (GWT): Conscious access broadcast architecture
 * - Curiosity (Phase 10): Novelty-driven exploration system
 * - Salience (Attention): Fast relevance evaluation
 * - Introspection: Self-monitoring and metacognition
 * - Ethics Engine (Phase 11.0): Golden Rule ethical decision-making
 * - Empathy Network (Phase 11.1): Perspective-taking and emotional understanding
 * - Empathetic Response (Phase 11.2): Safe, supportive emotional responses
 *
 * ARCHITECTURE:
 * - Modular extraction from main brain module
 * - Each subsystem initializes independently
 * - Support for config-driven enablement
 * - Comprehensive error handling
 *
 * INTEGRATION POINTS:
 * - Working Memory → Emotional System, Sleep System, Engrams, Systems Consolidation
 * - Global Workspace → Broadcasts to all cognitive modules
 * - Ethics Engine + Empathy Network → Empathetic Response
 * - Mirror Neurons → Theory of Mind, Predictive Processing
 * - Introspection → Self-awareness, explanation generation
 */

#ifndef NIMCP_BRAIN_COGNITIVE_H
#define NIMCP_BRAIN_COGNITIVE_H

#include <stdbool.h>
#include "core/brain/nimcp_brain.h"

#ifdef __cplusplus
extern "C" {
#endif

//=============================================================================
// COGNITIVE SUBSYSTEM INITIALIZATION FUNCTIONS
//=============================================================================

/**
 * @brief Initialize working memory subsystem (Phase 10.1)
 *
 * WHAT: Create Miller's 7±2 working memory with decay
 * WHY:  Enable temporary information storage and processing
 * HOW:  Create with defaults or custom config from brain
 *
 * ALSO INITIALIZES (coupled subsystems):
 * - Emotional System (Phase 10.2)
 * - Sleep System (Phase 10.4)
 * - Memory Engrams (Phase M1)
 * - Systems Consolidation (Phase M2)
 * - Working Memory Transfer (Phase M3)
 * - Semantic Memory (Phase M4)
 *
 * COMPLEXITY: O(capacity)
 * MEMORY: O(capacity)
 *
 * @param brain Brain instance
 * @return true on success, false on error
 */
bool init_working_memory_subsystem(brain_t brain);

/**
 * @brief Initialize Theory of Mind subsystem (Phase 10.6)
 *
 * WHAT: Create Theory of Mind module for social cognition
 * WHY:  Enable understanding of others' beliefs, goals, and emotions
 * HOW:  Create ToM with brain reference for self-model
 *
 * COMPLEXITY: O(1)
 * MEMORY: O(1) - small fixed structures for BDI model
 *
 * @param brain Brain instance
 * @return true on success, false on error
 */
bool init_theory_of_mind_subsystem(brain_t brain);

/**
 * @brief Initialize mirror neuron system for brain
 *
 * WHAT: Create and configure mirror neuron system for observation-based learning
 * WHY:  Enable social cognition, imitation learning, and action understanding
 * HOW:  Create mirror_neurons_t with config-specified parameters
 *
 * INTEGRATION: Connects to working memory, theory of mind, and predictive processing
 *
 * @param brain Brain to initialize mirror neurons for
 * @return true on success, false on error
 */
bool init_mirror_neurons(brain_t brain);

/**
 * @brief Initialize autobiographical memory subsystem (Phase 12)
 *
 * WHAT: Create episodic self-memory system for life events
 * WHY:  Enable autobiographical reasoning and self-narrative
 * HOW:  Create with 10,000 memory capacity
 *
 * COMPLEXITY: O(1) creation, O(log n) for queries
 * MEMORY: O(capacity)
 *
 * @param brain Brain instance
 * @return true on success, false on error
 */
bool init_autobiographical_memory_subsystem(brain_t brain);

/**
 * @brief Initialize Self-Model subsystem (Phase 12)
 *
 * WHAT: Create explicit self-representation system
 * WHY:  Self-awareness requires structured "I am X" distinct from "World is Y"
 * HOW:  Create self-model with identity, beliefs, capabilities, boundaries
 *
 * BIOLOGICAL BASIS: Medial Prefrontal Cortex (self-referential processing)
 * - Explicit identity representation
 * - Self-beliefs and self-knowledge
 * - Self-other boundary tracking
 *
 * @param brain Brain instance
 * @return true on success, false on error
 */
bool init_self_model_subsystem(brain_t brain);

/**
 * @brief Initialize Global Workspace Architecture subsystem
 *
 * WHAT: Create and configure global workspace for conscious access
 * WHY:  Enable broadcast architecture for cross-module information integration
 * HOW:  Create workspace with brain config parameters
 *
 * BIOLOGICAL BASIS: Global Workspace Theory (Baars, 1988; Dehaene, 2011)
 * - Limited-capacity broadcast architecture
 * - Winner-take-all competition for conscious access
 * - Refractory period matches attentional blink (~50ms)
 *
 * SUBSCRIPTIONS: Working Memory, Executive, Ethics, Introspection, Salience, ToM
 *
 * @param brain Brain instance
 * @return true on success, false on failure
 */
bool init_global_workspace_subsystem(brain_t brain);

/**
 * @brief Initialize Curiosity Engine subsystem
 *
 * WHAT: Create and configure curiosity-driven exploration system
 * WHY:  Enable novelty detection, knowledge gap detection, and exploration drive
 * HOW:  Create curiosity engine with learner name from brain config
 *
 * BIOLOGICAL BASIS: Intrinsic motivation and curiosity
 * - Dopaminergic response to novelty (midbrain)
 * - Exploration-exploitation trade-off (prefrontal cortex)
 * - Information-seeking behavior (anterior cingulate)
 *
 * COGNITIVE BENEFITS:
 * - 40% faster learning on novel patterns
 * - Intelligent exploration vs exploitation balance
 * - Prioritized learning of novel information
 *
 * @param brain Brain instance
 * @return true on success, false on failure
 */
bool init_curiosity_subsystem(brain_t brain);

/**
 * @brief Initialize Salience subsystem (Attention/Relevance Evaluation)
 *
 * WHAT: Create salience evaluator for fast attention/relevance scoring
 * WHY:  Enable brain to quickly determine what inputs deserve attention
 * HOW:  Create salience evaluator with default configuration
 *
 * CAPABILITIES:
 * - Fast salience evaluation (10x faster than full decision)
 * - Novelty detection (never seen before)
 * - Surprise measurement (violated expectations)
 * - Urgency scoring (requires immediate response)
 * - Attention competition in global workspace
 *
 * COGNITIVE BENEFITS:
 * - Selective attention to important stimuli
 * - Reduced computational cost (0.1ms vs 1ms per input)
 * - Emotional-salience integration for mood-biased attention
 *
 * @param brain Brain instance
 * @return true on success, false on failure
 */
bool init_salience_subsystem(brain_t brain);

/**
 * @brief Initialize Introspection subsystem (Self-Awareness)
 *
 * WHAT: Create introspection context for self-monitoring and metacognition
 * WHY:  Enable brain to examine its own internal state (consciousness requirement)
 * HOW:  Create introspection context with default configuration
 *
 * CAPABILITIES:
 * - Query active neurons and network state
 * - Measure uncertainty (epistemic + aleatoric)
 * - Track learned patterns
 * - Monitor activity history
 * - Network topology inspection
 *
 * CRITICAL FOR:
 * - Self-awareness and metacognition
 * - Uncertainty-aware decision making
 * - Explanation generation
 * - Wellbeing monitoring
 *
 * @param brain Brain instance
 * @return true on success, false on error
 */
bool init_introspection_subsystem(brain_t brain);

/**
 * @brief Initialize Ethics Engine subsystem (Phase 11.0)
 *
 * WHAT: Create Golden Rule ethics engine for ethical decision-making
 * WHY:  Ensure all actions align with "do unto others" principle
 * HOW:  Create ethics engine with empathy network for perspective-taking
 *
 * BIOLOGICAL BASIS: Prefrontal Cortex (Moral Reasoning)
 * - Evaluates actions against ethical principles
 * - Uses empathy to predict impact on others
 * - Hard-wired Golden Rule as foundational constraint
 *
 * DESIGN PRINCIPLE:
 * "Do unto others as you would have them done unto you"
 * - Ultimate goal: Improve human condition through compassion and fairness
 *
 * @param brain Brain instance
 * @return true on success, false on error
 */
bool init_ethics_engine_subsystem(brain_t brain);

/**
 * @brief Initialize Empathy Network subsystem (Phase 11.1)
 *
 * WHAT: Create empathy network for perspective-taking and emotional understanding
 * WHY:  Ethical decisions require simulating impact on others via mirror neurons
 * HOW:  Create empathy network with mirror neuron integration
 *
 * BIOLOGICAL BASIS: Mirror Neuron System
 * - Simulates other agents' emotional states (perspective-taking)
 * - Enables emotional contagion and empathy
 * - Integrates with ethics engine for Golden Rule evaluation
 *
 * INTEGRATION POINTS:
 * - Ethics Engine: Provides empathy for ethical decisions
 * - Empathetic Response: Enables compassionate communication
 * - Mirror Neurons: Links action observation to emotional simulation
 *
 * @param brain Brain instance
 * @return true on success, false on error
 */
bool init_empathy_network_subsystem(brain_t brain);

/**
 * @brief Initialize Empathetic Response Engine subsystem (Phase 11.2)
 *
 * WHAT: Create non-reactive empathetic response system for emotional support
 * WHY:  Enable safe, supportive responses to negative emotions (NEVER react negatively)
 * HOW:  Create empathetic response engine with ethics and empathy network integration
 *
 * CORE PRINCIPLE:
 * NEVER produce negative reactions to negative emotions (rage, hate, fear, disgust, despair)
 * Always respond with validation, empathy, and support
 *
 * SAFETY CRITICAL:
 * - Detects crisis situations (suicide, self-harm, abuse)
 * - Provides immediate escalation to human support
 * - Uses Golden Rule validation for all responses
 *
 * @param brain Brain instance
 * @return true on success, false on error
 */
bool init_empathetic_response_subsystem(brain_t brain);

#ifdef __cplusplus
}
#endif

#endif // NIMCP_BRAIN_COGNITIVE_H
