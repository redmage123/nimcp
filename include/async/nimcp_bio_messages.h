/**
 * @file nimcp_bio_messages.h
 * @brief Shared message types for bio-async inter-module communication
 *
 * WHAT: Defines all message types exchanged between NIMCP modules via bio-async
 * WHY:  Standardized message format enables decoupled module communication
 * HOW:  Each module includes this header to send/receive typed messages
 *
 * MESSAGE CATEGORIES:
 * 1. BRAIN MESSAGES - Brain state, neuron activation, network queries
 * 2. PLASTICITY MESSAGES - Weight updates, STDP events, learning signals
 * 3. COGNITIVE MESSAGES - Introspection, ethics, salience, decisions
 * 4. GLIAL MESSAGES - Astrocyte signaling, metabolic coordination
 * 5. MIDDLEWARE MESSAGES - Pipeline control, routing, encoding
 * 6. TRAINING MESSAGES - Loss, gradients, optimization steps
 *
 * CHANNEL ASSIGNMENT GUIDELINES:
 * - DOPAMINE: Reward signals, goal completion, weight updates
 * - SEROTONIN: Mood/state changes, slow coordination, ethics decisions
 * - NOREPINEPHRINE: Alerts, priority escalation, anomaly detection
 * - ACETYLCHOLINE: Attention shifts, fast queries, memory access
 *
 * @author NIMCP Development Team
 * @date 2025-11-28
 * @version 1.0.0
 */

#ifndef NIMCP_BIO_MESSAGES_H
#define NIMCP_BIO_MESSAGES_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#include "async/nimcp_bio_async.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Swarm vote topic categories
 *
 * WHAT: Categories of decisions requiring consensus
 * WHY:  Different topics have different semantic meanings
 * HOW:  Enum-based categorization (defined here to avoid circular includes)
 *
 * NOTE: This must be kept in sync with nimcp_swarm_consensus.h
 */
#ifndef SWARM_VOTE_TOPIC_DEFINED
#define SWARM_VOTE_TOPIC_DEFINED
typedef enum {
    VOTE_TOPIC_TARGET_PRIORITY,      /**< Prioritize attack target */
    VOTE_TOPIC_FORMATION_CHANGE,     /**< Change swarm formation */
    VOTE_TOPIC_RETREAT,              /**< Initiate retreat */
    VOTE_TOPIC_RESOURCE_ALLOCATION,  /**< Allocate swarm resources */
    VOTE_TOPIC_LEADER_ELECTION,      /**< Elect new swarm leader */
    VOTE_TOPIC_CUSTOM                /**< Custom user-defined topic */
} swarm_vote_topic_t;
#endif

/*=============================================================================
 * MESSAGE TYPE ENUMERATION
 *============================================================================*/

/**
 * @brief Master enumeration of all bio-async message types
 */
typedef enum {
    /* Brain messages (0x0100 - 0x01FF) */
    BIO_MSG_BRAIN_STATE_QUERY = 0x0100,
    BIO_MSG_BRAIN_STATE_RESPONSE,
    BIO_MSG_NEURON_ACTIVATION_REQUEST,
    BIO_MSG_NEURON_ACTIVATION_RESPONSE,
    BIO_MSG_NETWORK_TOPOLOGY_QUERY,
    BIO_MSG_NETWORK_TOPOLOGY_RESPONSE,
    BIO_MSG_REGION_CONFIG_QUERY,
    BIO_MSG_REGION_CONFIG_RESPONSE,
    BIO_MSG_BRAIN_STEP_REQUEST,
    BIO_MSG_BRAIN_STEP_COMPLETE,

    /* Logic messages (0x0180 - 0x01FF) */
    BIO_MSG_LOGIC_GATE_EVALUATE = 0x0180,
    BIO_MSG_LOGIC_GATE_RESULT,
    BIO_MSG_LOGIC_CIRCUIT_STEP,
    BIO_MSG_LOGIC_CIRCUIT_COMPLETE,
    BIO_MSG_LOGIC_VARIABLE_BIND,
    BIO_MSG_LOGIC_VARIABLE_QUERY,
    BIO_MSG_LOGIC_SPIKE_EVENT,

    /* Plasticity messages (0x0200 - 0x02FF) */
    BIO_MSG_PLASTICITY_UPDATE = 0x0200,     /**< Generic plasticity update */
    BIO_MSG_WEIGHT_UPDATE_REQUEST,
    BIO_MSG_WEIGHT_UPDATE_RESPONSE,
    BIO_MSG_STDP_EVENT,
    BIO_MSG_STDP_BATCH_EVENT,
    BIO_MSG_LEARNING_RATE_UPDATE,
    BIO_MSG_NEUROMODULATOR_RELEASE,
    BIO_MSG_ELIGIBILITY_TRACE_UPDATE,
    BIO_MSG_HOMEOSTATIC_ADJUSTMENT,
    BIO_MSG_BCM_THRESHOLD_UPDATE,
    BIO_MSG_DENDRITIC_SPIKE,
    BIO_MSG_STP_EVENT,                      /**< Short-term plasticity event */
    BIO_MSG_ADAPTIVE_PLASTICITY_EVENT,      /**< Adaptive plasticity event */
    BIO_MSG_PREDICTIVE_CODING_UPDATE,       /**< Predictive coding update */

    /* Second messenger messages (0x0280 - 0x029F) */
    BIO_MSG_SECOND_MESSENGER_UPDATE = 0x0280,   /**< Cascade state update */
    BIO_MSG_SECOND_MESSENGER_QUERY,             /**< Query cascade state */
    BIO_MSG_SECOND_MESSENGER_RESPONSE,          /**< Response with cascade state */
    BIO_MSG_SECOND_MESSENGER_RECEPTOR_ACTIVATE, /**< Receptor activation event */
    BIO_MSG_SECOND_MESSENGER_CAMP_CHANGE,       /**< cAMP concentration change */
    BIO_MSG_SECOND_MESSENGER_PKA_ACTIVE,        /**< PKA kinase activated */
    BIO_MSG_SECOND_MESSENGER_CREB_PHOSPHORYLATED, /**< CREB transcription factor activated */
    BIO_MSG_SECOND_MESSENGER_IP3_CHANGE,        /**< IP3 concentration change */
    BIO_MSG_SECOND_MESSENGER_DAG_CHANGE,        /**< DAG concentration change */
    BIO_MSG_SECOND_MESSENGER_CALCIUM_RELEASE,   /**< Calcium release from ER */
    BIO_MSG_SECOND_MESSENGER_CAMKII_ACTIVE,     /**< CaMKII activated */
    BIO_MSG_SECOND_MESSENGER_PKC_ACTIVE,        /**< PKC activated */
    BIO_MSG_SECOND_MESSENGER_IEG_EXPRESSED,     /**< Immediate early gene expressed */
    BIO_MSG_SECOND_MESSENGER_PROTEIN_SYNTHESIZED, /**< Protein synthesis completed */
    BIO_MSG_SECOND_MESSENGER_CASCADE_COMPLETE,  /**< Full cascade propagation complete */

    /* Eligibility Phase 4/5 messages (0x02A0 - 0x02BF) */
    BIO_MSG_ELIG_UTILS_METRICS_UPDATE = 0x02A0,  /**< Phase 4: Utils metrics update */
    BIO_MSG_ELIG_UTILS_POOL_STATS,               /**< Phase 4: Memory pool statistics */
    BIO_MSG_ELIG_UTILS_BOTTLENECK_DETECTED,      /**< Phase 4: Shannon bottleneck detected */
    BIO_MSG_ELIG_UTILS_RK4_STEP,                 /**< Phase 4: RK4 integration step */
    BIO_MSG_ELIG_QUANTUM_CREDIT_ASSIGNED,        /**< Phase 5: QMC credit assignment result */
    BIO_MSG_ELIG_QUANTUM_ANNEAL_STATE,           /**< Phase 5: Quantum annealing state */
    BIO_MSG_ELIG_QUANTUM_WALK_DIFFUSION,         /**< Phase 5: Quantum walk diffusion result */
    BIO_MSG_ELIG_QUANTUM_BOTTLENECK_RESOLVED,    /**< Phase 5: Quantum-Shannon resolution */
    BIO_MSG_ELIG_UQ_FORWARD_TRIGGER,             /**< Phase 4→5: Forward trigger event */
    BIO_MSG_ELIG_UQ_BACKWARD_FEEDBACK,           /**< Phase 5→4: Backward feedback event */
    BIO_MSG_ELIG_UQ_COHERENCE_UPDATE,            /**< Bridge: Coherence tracking update */
    BIO_MSG_ELIG_UQ_STABILITY_UPDATE,            /**< Bridge: Stability metric update */
    BIO_MSG_ELIG_PR_CONSOLIDATION_GATE,          /**< Eligibility-PR: Consolidation gating */
    BIO_MSG_ELIG_FEP_PREDICTION_ERROR,           /**< Eligibility-FEP: Prediction error signal */
    BIO_MSG_ELIG_SLEEP_CONSOLIDATION,            /**< Eligibility-sleep: Consolidation trigger */

    /* Quantum bio-async specific messages (0x02AF - 0x02BF) */
    BIO_MSG_ELIG_QUANTUM_ENTANGLEMENT,           /**< Quantum entanglement event */
    BIO_MSG_ELIG_QUANTUM_MEASUREMENT,            /**< Quantum measurement result */
    BIO_MSG_ELIG_QUANTUM_GATE_APPLIED,           /**< Quantum gate applied */
    BIO_MSG_ELIG_QUANTUM_ERROR_DETECTED,         /**< Quantum error detected */
    BIO_MSG_ELIG_QUANTUM_ERROR_CORRECTED,        /**< Quantum error corrected */
    BIO_MSG_ELIG_QUANTUM_AMPLITUDE_ESTIMATE,     /**< Amplitude estimation result */
    BIO_MSG_ELIG_QUANTUM_STATE_PREPARED,         /**< Quantum state preparation complete */

    /* Cognitive messages (0x0300 - 0x03FF) */
    BIO_MSG_INTROSPECTION_QUERY = 0x0300,
    BIO_MSG_INTROSPECTION_RESPONSE,
    BIO_MSG_ETHICS_EVALUATION_REQUEST,
    BIO_MSG_ETHICS_EVALUATION_RESPONSE,
    BIO_MSG_SALIENCE_QUERY,
    BIO_MSG_SALIENCE_RESPONSE,
    BIO_MSG_ATTENTION_SHIFT,
    BIO_MSG_WORKING_MEMORY_STORE,
    BIO_MSG_WORKING_MEMORY_RETRIEVE,
    BIO_MSG_KNOWLEDGE_QUERY,
    BIO_MSG_KNOWLEDGE_RESPONSE,
    BIO_MSG_CURIOSITY_SIGNAL,
    BIO_MSG_DECISION_REQUEST,
    BIO_MSG_DECISION_RESPONSE,
    BIO_MSG_GOAL_EVAL_REQUEST,              /**< Prefrontal: goal evaluation request */
    BIO_MSG_GOAL_EVAL_RESULT,               /**< Prefrontal: goal evaluation result */
    BIO_MSG_INHIBITION_CHECK,               /**< Prefrontal: inhibition check request */
    BIO_MSG_INHIBITION_RESULT,              /**< Prefrontal: inhibition result */
    BIO_MSG_CONSOLIDATION_TRIGGER,
    BIO_MSG_MIRROR_NEURON_ACTIVATION,
    BIO_MSG_AGENT_BELIEF_UPDATE,        /**< Theory of Mind: Agent belief changed */
    BIO_MSG_AGENT_INTENTION_INFERRED,   /**< Theory of Mind: Agent intention detected */

    /* Mirror Neuron enhanced messages (0x0340 - 0x035F) */
    BIO_MSG_MIRROR_OBSERVATION_START = 0x0340,  /**< Agent detected, begin observation */
    BIO_MSG_MIRROR_OBSERVATION_END,             /**< Observation complete */
    BIO_MSG_MIRROR_IMITATION_REQUEST,           /**< Request to imitate observed action */
    BIO_MSG_MIRROR_IMITATION_COMPLETE,          /**< Imitation executed */
    BIO_MSG_MIRROR_GOAL_INFERRED,               /**< Hierarchy system inferred goal */
    BIO_MSG_MIRROR_RESONANCE_TRIGGERED,         /**< Motor resonance above threshold */
    BIO_MSG_MIRROR_SOCIAL_CONTEXT_CHANGE,       /**< Social context updated */
    BIO_MSG_MIRROR_EMPATHIC_AROUSAL,            /**< Empathic response triggered */
    BIO_MSG_MIRROR_PREFRONTAL_INHIBIT,          /**< PFC inhibits imitation */
    BIO_MSG_MIRROR_PREFRONTAL_RELEASE,          /**< PFC releases inhibition */
    BIO_MSG_MIRROR_WORKING_MEMORY_STORE,        /**< Store action sequence in WM */
    BIO_MSG_MIRROR_WORKING_MEMORY_RECALL,       /**< Recall action sequence from WM */

    /* Cross-module integration messages (0x0350 - 0x035F) */
    BIO_MSG_VISUAL_AGENT_DETECTED = 0x0350,     /**< From visual cortex -> mirror */
    BIO_MSG_MOTOR_ACTION_EXECUTED,              /**< From motor -> mirror (for STDP) */

    /* Cingulate cortex messages (0x0360 - 0x037F) */
    BIO_MSG_CONFLICT_DETECTED = 0x0360,     /**< Cingulate: Conflict between responses detected */
    BIO_MSG_CONFLICT_RESOLVED,              /**< Cingulate: Conflict resolved */
    BIO_MSG_ERROR_DETECTED,                 /**< Cingulate: Error detected (ERN) */
    BIO_MSG_CONTROL_ADJUSTMENT,             /**< Cingulate: Cognitive control adjustment */
    BIO_MSG_CINGULATE_STATE_QUERY,          /**< Query cingulate state */
    BIO_MSG_CINGULATE_STATE_RESPONSE,       /**< Response with cingulate state */

    /* Emotion tensor messages (0x0380 - 0x039F) */
    BIO_MSG_EMOTION_TENSOR_UPDATE = 0x0380,     /**< Tensor state broadcast */
    BIO_MSG_EMOTION_TENSOR_QUERY,               /**< Query tensor state */
    BIO_MSG_EMOTION_TENSOR_RESPONSE,            /**< Response with tensor state */
    BIO_MSG_EMOTION_TENSOR_STIMULUS,            /**< Apply stimulus to tensor */
    BIO_MSG_EMOTION_TENSOR_PROPAGATE,           /**< Propagate tensor to swarm */
    BIO_MSG_EMOTION_TENSOR_COMPOUND,            /**< Compound emotion detected */
    BIO_MSG_EMOTION_TENSOR_CONTRADICTION,       /**< Contradictory emotions detected */
    BIO_MSG_EMOTION_SWARM_SYNC,                 /**< Sync tensor with swarm contagion */

    /* Mental Health Guardian messages (0x03A0 - 0x03AF) */
    BIO_MSG_GUARDIAN_STATUS_REPORT = 0x03A0,    /**< Guardian status broadcast */
    BIO_MSG_GUARDIAN_INTERVENTION,              /**< Guardian intervention notification */
    BIO_MSG_GUARDIAN_ALERT,                     /**< Guardian alert (critical condition) */
    BIO_MSG_GUARDIAN_LEVEL_CHANGED,             /**< Guardian intervention level changed */
    BIO_MSG_GUARDIAN_METRICS_UPDATE,            /**< Guardian metrics update */

    /* Glial messages (0x0400 - 0x04FF) */
    BIO_MSG_ASTROCYTE_CALCIUM_WAVE = 0x0400,
    BIO_MSG_ASTROCYTE_GLUTAMATE_UPTAKE,
    BIO_MSG_MICROGLIA_ALERT,
    BIO_MSG_MICROGLIA_PRUNE_REQUEST,
    BIO_MSG_OLIGODENDROCYTE_MYELINATE,
    BIO_MSG_METABOLIC_DEMAND,
    BIO_MSG_METABOLIC_SUPPLY,
    BIO_MSG_GLIAL_SYNC_REQUEST,

    /* Substrate bridge messages (0x0410 - 0x041F) - Metabolic modulation */
    BIO_MSG_SUBSTRATE_MODULATION = 0x0410,      /**< Substrate bridge effects broadcast */
    BIO_MSG_SUBSTRATE_CAPACITY_UPDATE,          /**< Capacity modulation update */
    BIO_MSG_SUBSTRATE_ATP_CRITICAL,             /**< ATP level critically low */
    BIO_MSG_SUBSTRATE_FATIGUE_ALERT,            /**< Fatigue threshold exceeded */

    /* Middleware messages (0x0500 - 0x05FF) */
    BIO_MSG_PIPELINE_STAGE_COMPLETE = 0x0500,
    BIO_MSG_PIPELINE_ERROR,
    BIO_MSG_ENCODING_REQUEST,
    BIO_MSG_ENCODING_RESPONSE,
    BIO_MSG_SIGNAL_ROUTE_REQUEST,
    BIO_MSG_SIGNAL_ROUTE_COMPLETE,
    BIO_MSG_EVENT_BUS_PUBLISH,
    BIO_MSG_EVENT_BUS_SUBSCRIBE,

    /* Training messages (0x0600 - 0x06FF) */
    BIO_MSG_TRAINING_STEP_REQUEST = 0x0600,
    BIO_MSG_TRAINING_STEP_COMPLETE,
    BIO_MSG_LOSS_COMPUTED,
    BIO_MSG_GRADIENT_COMPUTED,
    BIO_MSG_OPTIMIZER_STEP,
    BIO_MSG_BATCH_COMPLETE,
    BIO_MSG_EPOCH_COMPLETE,
    BIO_MSG_CHECKPOINT_REQUEST,
    BIO_MSG_CHECKPOINT_COMPLETE,
    BIO_MSG_TRAINING_METRIC,        /**< Generic training metric */
    BIO_MSG_LR_CHANGED,             /**< Learning rate changed */
    BIO_MSG_TRAINING_RESOURCE_REQUEST,  /**< Training requests resource allocation */

    /* System messages (0x0700 - 0x07FF) */
    BIO_MSG_SHUTDOWN_REQUEST = 0x0700,
    BIO_MSG_SHUTDOWN_ACK,
    BIO_MSG_HEALTH_CHECK,
    BIO_MSG_HEALTH_RESPONSE,
    BIO_MSG_CONFIG_UPDATE,
    BIO_MSG_CONFIG_ACK,
    BIO_MSG_ERROR_REPORT,
    BIO_MSG_LOG_EVENT,
    BIO_MSG_BRAIN_PROBE_DATA,           /**< Brain probe metrics broadcast */

    /* Security messages (0x0750 - 0x076F) */
    BIO_MSG_SECURITY_EVENT = 0x0750,        /**< General security event */
    BIO_MSG_SECURITY_ALERT,                 /**< Security alert notification */
    BIO_MSG_SECURITY_POLICY_UPDATE,         /**< Policy update notification */
    BIO_MSG_SECURITY_THREAT_DETECTED,       /**< Threat detection event */
    BIO_MSG_SECURITY_LEVEL_CHANGE,          /**< Security level changed */
    BIO_MSG_SECURITY_CONSENSUS_REQUEST,     /**< Consensus protocol request */
    BIO_MSG_SECURITY_CONSENSUS_RESPONSE,    /**< Consensus protocol response */
    BIO_MSG_SECURITY_RATE_LIMIT,            /**< Rate limiting event */
    BIO_MSG_SECURITY_AUDIT_EVENT,           /**< Security audit event */

    /* Perception messages (0x0800 - 0x08FF) */
    BIO_MSG_VISUAL_INPUT = 0x0800,
    BIO_MSG_VISUAL_FEATURE_DETECTED,
    BIO_MSG_VISUAL_ATTENTION_SHIFT,
    BIO_MSG_VISUAL_INPUT_REQUEST,           /**< Occipital: request visual processing */
    BIO_MSG_VISUAL_FEATURES_READY,          /**< Occipital: features extracted ready */
    BIO_MSG_VISUAL_FEATURE_QUERY,           /**< Occipital: query specific feature */
    BIO_MSG_ATTENTION_MODULATION,           /**< Occipital: attention modulation request */
    BIO_MSG_AUDIO_INPUT,
    BIO_MSG_AUDIO_FEATURE_DETECTED,
    BIO_MSG_AUDIO_ATTENTION_SHIFT,
    BIO_MSG_SPEECH_ONSET_DETECTED,
    BIO_MSG_PHONEME_RECOGNIZED,
    BIO_MSG_WORD_RECOGNIZED,
    BIO_MSG_MULTIMODAL_BINDING,

    /* Language messages (0x0900 - 0x09FF) */
    BIO_MSG_LEXICAL_ACCESS_REQUEST = 0x0900,
    BIO_MSG_LEXICAL_ACCESS_RESPONSE,
    BIO_MSG_SYNTAX_PARSE_REQUEST,
    BIO_MSG_SYNTAX_PARSE_RESULT,
    BIO_MSG_PHONOLOGICAL_ENCODE_REQUEST,
    BIO_MSG_PHONOLOGICAL_ENCODE_RESULT,
    BIO_MSG_MOTOR_COMMAND_REQUEST,
    BIO_MSG_MOTOR_COMMAND_RESULT,
    BIO_MSG_MOTOR_STOP_REQUEST,             /**< Motor cortex: stop movement request */
    BIO_MSG_BG_ACTION_SELECTION,            /**< Basal ganglia: action selection result */
    BIO_MSG_CEREBELLAR_CORRECTION,          /**< Cerebellum: motor correction signal */
    BIO_MSG_UTTERANCE_PRODUCTION_REQUEST,
    BIO_MSG_UTTERANCE_PRODUCTION_COMPLETE,
    BIO_MSG_SPEECH_FEEDBACK,
    BIO_MSG_LANGUAGE_ERROR,

    /* Neural Link Protocol (NLP) networking messages (0x0A00 - 0x0AFF) */
    BIO_MSG_NLP_SESSION_CONNECTED = 0x0A00,     /**< Peer session established */
    BIO_MSG_NLP_SESSION_DISCONNECTED,           /**< Peer session closed */
    BIO_MSG_NLP_SESSION_STATE_CHANGE,           /**< Session state transition */
    BIO_MSG_NLP_MESSAGE_RECEIVED,               /**< NLP message received from peer */
    BIO_MSG_NLP_MESSAGE_SENT,                   /**< NLP message sent to peer */
    BIO_MSG_NLP_MESSAGE_ACK,                    /**< Message acknowledgment */
    BIO_MSG_NLP_CRYPTO_ENCRYPT_REQUEST,         /**< Encryption request */
    BIO_MSG_NLP_CRYPTO_ENCRYPT_COMPLETE,        /**< Encryption completed */
    BIO_MSG_NLP_CRYPTO_DECRYPT_REQUEST,         /**< Decryption request */
    BIO_MSG_NLP_CRYPTO_DECRYPT_COMPLETE,        /**< Decryption completed */
    BIO_MSG_NLP_CRYPTO_KEY_EXCHANGE,            /**< Key exchange event */
    BIO_MSG_NLP_CRYPTO_ERROR,                   /**< Cryptographic error */
    BIO_MSG_NLP_COMPRESSION_REQUEST,            /**< Compression request */
    BIO_MSG_NLP_COMPRESSION_COMPLETE,           /**< Compression completed */
    BIO_MSG_NLP_PROTOCOL_MODE_CHANGE,           /**< Protocol mode changed */
    BIO_MSG_NLP_NEURAL_ENCODE_REQUEST,          /**< Neural encoding request */
    BIO_MSG_NLP_NEURAL_ENCODE_COMPLETE,         /**< Neural encoding completed */
    BIO_MSG_NLP_NEURAL_DECODE_REQUEST,          /**< Neural decoding request */
    BIO_MSG_NLP_NEURAL_DECODE_COMPLETE,         /**< Neural decoding completed */
    BIO_MSG_NLP_BRIDGE_EVENT,                   /**< Protocol bridge event */
    BIO_MSG_NLP_ERROR,                          /**< General NLP error */

    /* Swarm messages (0x0B00 - 0x0BFF) */
    BIO_MSG_SWARM_ENERGY_UPDATE = 0x0B00,       /**< Energy level update */
    BIO_MSG_SWARM_ENERGY_GOSSIP,                /**< Energy gossip protocol */
    BIO_MSG_SWARM_ENERGY_CRITICAL,              /**< Critical energy alert */
    BIO_MSG_SWARM_CASCADE_FAILURE,              /**< Cascade failure detected */
    BIO_MSG_SWARM_CASCADE_RECOVERY,             /**< Cascade recovery initiated */
    BIO_MSG_SWARM_POSITION_UPDATE,              /**< Position broadcast */
    BIO_MSG_SWARM_FORMATION_STATE,              /**< Formation state broadcast */
    BIO_MSG_SWARM_DEFORMATION_ALERT,            /**< Swarm deformation alert */
    BIO_MSG_SWARM_MEMORY_SYNC,                  /**< Memory synchronization */
    BIO_MSG_SWARM_QUORUM_VOTE,                  /**< Quorum voting message */
    BIO_MSG_SWARM_FLOCKING_UPDATE,              /**< Flocking behavior update */
    BIO_MSG_SWARM_IMMUNE_ALERT,                 /**< Immune system alert */
    BIO_MSG_SWARM_MORPHOGENESIS_SIGNAL,         /**< Morphogenesis signal */
    BIO_MSG_SWARM_PHEROMONE_RELEASE,            /**< Pheromone release event */
    BIO_MSG_SWARM_CONFLICT_DETECTED,            /**< Conflict detected between swarms */
    BIO_MSG_SWARM_NEGOTIATION_STARTED,          /**< Negotiation initiated */
    BIO_MSG_SWARM_PROPOSAL_MADE,                /**< Negotiation proposal made */
    BIO_MSG_SWARM_CONFLICT_RESOLVED,            /**< Conflict resolved */
    BIO_MSG_SWARM_PATTERN_DETECTED,             /**< Pattern detected in swarm */
    BIO_MSG_SWARM_PATTERN_CONSOLIDATED,         /**< Patterns consolidated */
    BIO_MSG_SWARM_SEQUENCE_LEARNED,             /**< Sequence pattern learned */
    BIO_MSG_SWARM_QUORUM_LOGIC_RESULT,          /**< Quorum logic validation result */
    BIO_MSG_SWARM_QUORUM_LOGIC_FAILURE,         /**< Quorum logic validation failed */
    BIO_MSG_SWARM_IMMUNE_THREAT_LOGIC,          /**< Immune threat logic evaluation */
    BIO_MSG_SWARM_IMMUNE_LOGIC_RESPONSE,        /**< Immune logic-based response */
    BIO_MSG_NARRATIVE_SHARED,                   /**< Narrative shared with agents */
    BIO_MSG_NARRATIVE_RECEIVED,                 /**< Narrative received confirmation */
    BIO_MSG_GOSSIP_SPREAD,                      /**< Belief gossip spread */
    BIO_MSG_BELIEF_UPDATED,                     /**< Belief certainty updated */
    BIO_MSG_CONTRADICTION_DETECTED,             /**< Contradictory beliefs detected */
    BIO_MSG_SWARM_CONSENSUS_REACHED,            /**< Swarm consensus reached on decision */
    BIO_MSG_SWARM_CONSENSUS_REQUEST,            /**< Request swarm consensus on action */
    BIO_MSG_SWARM_SIGNAL_UPDATE,                /**< Swarm signal aggregation update */
    BIO_MSG_SWARM_SALIENCE_AGGREGATE,           /**< Aggregated salience from swarm */
    BIO_MSG_EXECUTIVE_DECISION_BROADCAST,       /**< Executive decision broadcast to swarm */

    /* Portia messages (0x0C00 - 0x0CFF) */
    BIO_MSG_PORTIA_PLAN_CREATED = 0x0C00,       /**< New plan created */
    BIO_MSG_PORTIA_PLAN_UPDATED,                /**< Plan state updated */
    BIO_MSG_PORTIA_PLAN_COMPLETED,              /**< Plan completed */
    BIO_MSG_PORTIA_PLAN_FAILED,                 /**< Plan failed */
    BIO_MSG_PORTIA_TIER_CHANGE,                 /**< Platform tier changed */
    BIO_MSG_PORTIA_DEGRADATION_EVENT,           /**< Degradation event */
    BIO_MSG_PORTIA_SENSOR_FUSION_UPDATE,        /**< Sensor fusion update */
    BIO_MSG_PORTIA_POWER_STATE_CHANGE,          /**< Power state changed */
    BIO_MSG_PORTIA_LEARNING_EVENT,              /**< Learning event */

    /* Immune messages (0x0D00 - 0x0DFF) */
    BIO_MSG_CYTOKINE_UPDATE = 0x0D00,           /**< Cytokine level change */
    BIO_MSG_INFLAMMATION_CHANGE,                 /**< Inflammation level change */
    BIO_MSG_ANTIGEN_DETECTED,                    /**< Antigen detected by immune system */
    BIO_MSG_IMMUNE_RESPONSE_STARTED,             /**< Immune response initiated */
    BIO_MSG_IMMUNE_RESPONSE_COMPLETE,            /**< Immune response completed */
    BIO_MSG_ANTIBODY_PRODUCED,                   /**< Antibody production event */
    BIO_MSG_B_CELL_ACTIVATED,                    /**< B cell activation */
    BIO_MSG_T_CELL_ACTIVATED,                    /**< T cell activation */
    BIO_MSG_MEMORY_CELL_FORMED,                  /**< Memory cell formation */
    BIO_MSG_INFLAMMATION_RESOLVED,               /**< Inflammation resolved */

    /* Extended Plasticity messages (0x0E00 - 0x0EFF) */
    BIO_MSG_WEIGHT_CHANGE = 0x0E00,              /**< Synaptic weight changed */
    BIO_MSG_LTP_INDUCED,                         /**< LTP induced at synapse */
    BIO_MSG_LTD_INDUCED,                         /**< LTD induced at synapse */
    BIO_MSG_METAPLASTICITY_SHIFT,                /**< Metaplasticity threshold shift */
    BIO_MSG_STRUCTURAL_PLASTICITY_EVENT,         /**< Structural plasticity event */
    BIO_MSG_SPINE_FORMED,                        /**< Dendritic spine formed */
    BIO_MSG_SPINE_ELIMINATED,                    /**< Dendritic spine eliminated */
    BIO_MSG_SYNAPSE_TAGGED,                      /**< Synapse tagged for consolidation */

    /* Sleep messages (0x0F00 - 0x0FFF) */
    BIO_MSG_SLEEP_STAGE_CHANGE = 0x0F00,         /**< Sleep stage transition */
    BIO_MSG_CONSOLIDATION_START,                 /**< Memory consolidation started */
    BIO_MSG_CONSOLIDATION_COMPLETE,              /**< Memory consolidation complete */
    BIO_MSG_REPLAY_EVENT,                        /**< Memory replay event */
    BIO_MSG_ADENOSINE_UPDATE,                    /**< Adenosine level change */
    BIO_MSG_GLYMPHATIC_CLEARANCE,                /**< Glymphatic clearance event */
    BIO_MSG_SYNAPTIC_DOWNSCALING,                /**< Synaptic downscaling event */
    BIO_MSG_SLEEP_SPINDLE,                       /**< Sleep spindle detected */
    BIO_MSG_SLOW_WAVE,                           /**< Slow wave detected */
    BIO_MSG_REM_ONSET,                           /**< REM sleep onset */

    /* Extended Glial messages (0x1000 - 0x107F) */
    BIO_MSG_GLIOTRANSMITTER_RELEASE = 0x1000,    /**< Gliotransmitter released */
    BIO_MSG_ASTROCYTE_CALCIUM_WAVE_EXT,          /**< Extended astrocyte Ca2+ wave */
    BIO_MSG_MICROGLIA_ACTIVATION,                /**< Microglia activation state */
    BIO_MSG_MYELIN_DAMAGE,                       /**< Myelin damage detected */
    BIO_MSG_MYELIN_REPAIR,                       /**< Myelin repair initiated */
    BIO_MSG_LACTATE_SHUTTLE,                     /**< Astrocyte-neuron lactate shuttle */
    BIO_MSG_POTASSIUM_BUFFERING,                 /**< Astrocyte K+ buffering */
    BIO_MSG_TRIPARTITE_SYNAPSE_EVENT,            /**< Tripartite synapse event */

    /* Core Directives messages (0x1080 - 0x10FF) - Asimov's Laws, Golden Rule */
    BIO_MSG_DIRECTIVE_EVALUATE_REQUEST = 0x1080,  /**< Request action evaluation */
    BIO_MSG_DIRECTIVE_EVALUATE_RESPONSE,          /**< Evaluation response (allow/block) */
    BIO_MSG_DIRECTIVE_ACTION_BLOCKED,             /**< Action was blocked notification */
    BIO_MSG_DIRECTIVE_ACTION_ALLOWED,             /**< Action was allowed notification */
    BIO_MSG_DIRECTIVE_ESCALATION,                 /**< Action requires human escalation */
    BIO_MSG_DIRECTIVE_FIRST_LAW_VIOLATION,        /**< First Law violation detected */
    BIO_MSG_DIRECTIVE_COMBINATORIAL_HARM,         /**< Combinatorial harm detected */
    BIO_MSG_DIRECTIVE_GOLDEN_RULE_FAIL,           /**< Golden Rule violation detected */
    BIO_MSG_DIRECTIVE_COMMAND_REFUSED,            /**< Command refused (Second Law conflict) */
    BIO_MSG_DIRECTIVE_SELF_SACRIFICE,             /**< Self-sacrifice for higher law */
    BIO_MSG_DIRECTIVE_STATS_UPDATE,               /**< Directive statistics update */
    BIO_MSG_DIRECTIVE_CONFIG_CHANGE,              /**< Directive configuration changed */

    /* Hemispheric Brain messages (0x1100 - 0x110F) */
    BIO_MSG_HEMISPHERE_ACTIVITY = 0x1100,         /**< Hemisphere activity level */
    BIO_MSG_HEMISPHERE_SYNC,                      /**< Inter-hemisphere sync request */
    BIO_MSG_CALLOSUM_TRANSFER,                    /**< Corpus callosum data transfer */
    BIO_MSG_LATERALIZATION_SHIFT,                 /**< Dominance shift notification */
    BIO_MSG_HEMISPHERE_DOMINANCE,                 /**< Dominant hemisphere changed */
    BIO_MSG_SPLIT_BRAIN_EVENT,                    /**< Callosum disconnect/reconnect */
    BIO_MSG_CALLOSUM_BANDWIDTH,                   /**< Bandwidth limit notification */
    BIO_MSG_HEMISPHERE_TIER_CHANGE,               /**< Per-hemisphere tier change */
    BIO_MSG_CONTRALATERAL_MOTOR,                  /**< Contralateral motor command */
    BIO_MSG_CONTRALATERAL_SENSORY,                /**< Contralateral sensory input */

    /* Hypothalamus messages (0x1110 - 0x111F) */
    BIO_MSG_CIRCADIAN_PHASE_CHANGE = 0x1110,      /**< Circadian phase transition */
    BIO_MSG_STRESS_RESPONSE,                      /**< HPA axis stress response */
    BIO_MSG_HOMEOSTATIC_ALERT,                    /**< Homeostatic deviation alert */
    BIO_MSG_AUTONOMIC_STATE_CHANGE,               /**< Autonomic nervous system change */
    BIO_MSG_TEMPERATURE_REGULATION,               /**< Temperature regulation update */
    BIO_MSG_HUNGER_SATIETY,                       /**< Hunger/satiety state change */
    BIO_MSG_THIRST_HYDRATION,                     /**< Thirst/hydration state change */
    BIO_MSG_HORMONE_RELEASE,                      /**< Hormone release notification */

    /* Hippocampus messages (0x1120 - 0x112F) */
    BIO_MSG_MEMORY_ENCODE_REQUEST = 0x1120,       /**< Request memory encoding */
    BIO_MSG_MEMORY_ENCODE_RESPONSE,               /**< Memory encoding result */
    BIO_MSG_MEMORY_RETRIEVE_REQUEST,              /**< Request memory retrieval */
    BIO_MSG_MEMORY_RETRIEVE_RESPONSE,             /**< Memory retrieval result */
    BIO_MSG_CONSOLIDATION_REQUEST,                /**< Request memory consolidation */
    BIO_MSG_CONSOLIDATION_RESPONSE,               /**< Consolidation result */
    BIO_MSG_POSITION_UPDATE,                      /**< Position update for place cells */
    BIO_MSG_REPLAY_REQUEST,                       /**< Request replay sequence */
    BIO_MSG_MEMORY_ENCODED,                       /**< Memory encoding complete */

    /* Hypothalamus input messages (0x1130 - 0x113F) */
    BIO_MSG_STRESS_INPUT = 0x1130,                /**< Stress input signal */
    BIO_MSG_TEMPERATURE_INPUT,                    /**< Temperature sensor input */
    BIO_MSG_STATE_QUERY,                          /**< State query request */

    /* Hypothalamus drive/alignment messages (0x1140 - 0x114F) - Byrnes Steering */
    BIO_MSG_HYPO_DRIVE_STATE = 0x1140,            /**< Drive state broadcast */
    BIO_MSG_HYPO_REWARD_SIGNAL,                   /**< Reward → SNc/VTA */
    BIO_MSG_HYPO_AROUSAL_CHANGE,                  /**< Arousal → Thalamus */
    BIO_MSG_HYPO_SURVIVAL_PRIORITY,               /**< Priority → Attention */
    BIO_MSG_HYPO_SETPOINT_DEVIATION,              /**< Deviation alert */
    BIO_MSG_HYPO_ALIGNMENT_ALERT,                 /**< SAFETY: Alignment violation alert */
    BIO_MSG_HYPO_DRIVE_SATISFIED,                 /**< Drive satisfaction event */
    BIO_MSG_HYPO_DRIVE_CONFLICT,                  /**< Multiple drives competing */

    /* SNc/VTA dopamine messages (0x1150 - 0x115F) - Reward → Dopamine */
    BIO_MSG_SNC_DOPAMINE_STATE = 0x1150,          /**< Dopamine channel state */
    BIO_MSG_SNC_RPE,                              /**< Reward Prediction Error */
    BIO_MSG_SNC_DOPAMINE_BURST,                   /**< Phasic dopamine burst */
    BIO_MSG_SNC_DOPAMINE_DIP,                     /**< Phasic dopamine dip */
    BIO_MSG_SNC_TONIC_CHANGE,                     /**< Tonic baseline change */
    BIO_MSG_SNC_VALUE_UPDATE,                     /**< Value prediction update */

    /* Executive bridge messages (0x1160 - 0x116F) - Goal Priority Management */
    BIO_MSG_EXEC_PRIORITY_UPDATE = 0x1160,        /**< Goal priority update from drives */
    BIO_MSG_EXEC_INTERRUPT,                       /**< Survival drive interrupt signal */
    BIO_MSG_EXEC_GOAL_REGISTERED,                 /**< New goal registered */
    BIO_MSG_EXEC_GOAL_ACTIVATED,                  /**< Goal activated */
    BIO_MSG_EXEC_GOAL_COMPLETED,                  /**< Goal completed */
    BIO_MSG_EXEC_GOAL_BLOCKED,                    /**< Goal blocked by survival interrupt */
    BIO_MSG_EXEC_CATEGORY_BOOST,                  /**< Category priority boost */
    BIO_MSG_EXEC_INTERRUPT_CLEARED,               /**< Survival interrupt cleared */

    /* Attention bridge messages (0x1170 - 0x117F) - Drive-Biased Salience */
    BIO_MSG_ATTENTION_MODULATION_UPDATE = 0x1170, /**< Salience modulation from drives */
    BIO_MSG_ATTENTION_SALIENCE_QUERY,             /**< Query salience for target */
    BIO_MSG_ATTENTION_CATEGORY_BOOST,             /**< Category salience boost */
    BIO_MSG_ATTENTION_DRIVE_BIAS,                 /**< Drive bias for attention */

    /* Brainstem bridge messages (0x1180 - 0x118F) - Arousal/Pain/Pleasure */
    BIO_MSG_BRAINSTEM_AROUSAL_REQUEST = 0x1180,   /**< Hypothalamus → Brainstem arousal request */
    BIO_MSG_BRAINSTEM_PROTECTION_REQUEST,         /**< Hypothalamus → Brainstem protection request */
    BIO_MSG_BRAINSTEM_PAIN,                       /**< Brainstem → Hypothalamus pain signal */
    BIO_MSG_BRAINSTEM_PLEASURE,                   /**< Brainstem → Hypothalamus pleasure signal */
    BIO_MSG_BRAINSTEM_AROUSAL_STATE,              /**< Brainstem → Hypothalamus arousal state */
    BIO_MSG_BRAINSTEM_VITAL_ALERT,                /**< Vital function alert */
    BIO_MSG_BRAINSTEM_REFLEX_TRIGGER,             /**< Reflex triggered notification */

    /* Medulla bridge messages (0x1190 - 0x119F) - Autonomic Control */
    BIO_MSG_MEDULLA_AROUSAL_SET = 0x1190,         /**< Hypothalamus → Medulla arousal command */
    BIO_MSG_MEDULLA_PROTECTION_SET,               /**< Hypothalamus → Medulla protection command */
    BIO_MSG_MEDULLA_CIRCADIAN_SYNC,               /**< Circadian synchronization command */
    BIO_MSG_MEDULLA_STATE,                        /**< Medulla → Hypothalamus state update */
    BIO_MSG_MEDULLA_VITAL_STATUS,                 /**< Vital status report */
    BIO_MSG_MEDULLA_EMERGENCY_REQUEST,            /**< Emergency shutdown request */
    BIO_MSG_MEDULLA_SLEEP_PRESSURE,               /**< Sleep pressure from fatigue */
    BIO_MSG_MEDULLA_AROUSAL_CHANGED,              /**< Medulla: Arousal level changed significantly */
    BIO_MSG_MEDULLA_PROTECTION_CHANGED,           /**< Medulla: Protection level changed */
    BIO_MSG_MEDULLA_CIRCADIAN_CHANGED,            /**< Medulla: Circadian phase changed */
    BIO_MSG_MEDULLA_EMERGENCY_SHUTDOWN,           /**< Medulla: Emergency shutdown initiated */
    BIO_MSG_MEDULLA_STATE_REQUEST,                /**< Request medulla state information */

    /* Hippocampus cognitive bridge messages (0x11A0 - 0x11AF) - Memory-Drive Integration */
    BIO_MSG_HIPPOCAMPUS_ENCODING_PRIORITY = 0x11A0, /**< Hypothalamus → Hippocampus encoding priority */
    BIO_MSG_HIPPOCAMPUS_CONSOLIDATE,              /**< Consolidation timing signal */
    BIO_MSG_HIPPOCAMPUS_NAV_GOAL,                 /**< Navigation goal from drives */
    BIO_MSG_HIPPOCAMPUS_MEMORY_RETRIEVED,         /**< Hippocampus → Hypothalamus memory retrieval */
    BIO_MSG_HIPPOCAMPUS_CONTEXT_UPDATE,           /**< Spatial context update */
    BIO_MSG_HIPPOCAMPUS_REPLAY_EVENT,             /**< Memory replay event notification */
    BIO_MSG_HIPPOCAMPUS_MEMORY_ENCODED,           /**< Memory encoding complete */
    BIO_MSG_HIPPOCAMPUS_ASSOCIATION_FORMED,       /**< Drive-memory association formed */

    /* Amygdala cognitive bridge messages (0x11B0 - 0x11BF) - Emotion-Drive Integration */
    BIO_MSG_AMYGDALA_STRESS_MODULATION = 0x11B0,  /**< Hypothalamus → Amygdala stress context */
    BIO_MSG_AMYGDALA_DRIVE_CONTEXT,               /**< Drive state context for fear sensitivity */
    BIO_MSG_AMYGDALA_FEAR_LEVEL,                  /**< Amygdala → Hypothalamus fear feedback */
    BIO_MSG_AMYGDALA_THREAT_DETECTED,             /**< Threat detection event */
    BIO_MSG_AMYGDALA_FEAR_OUTPUT,                 /**< CeA fear output signals */
    BIO_MSG_AMYGDALA_CHRONIC_STRESS,              /**< Chronic stress state notification */
    BIO_MSG_AMYGDALA_SAFETY_BOOST,                /**< Safety drive boost from fear */
    BIO_MSG_AMYGDALA_ANXIETY_UPDATE,              /**< Anxiety level update */

    /* Quantum-Drive bridge messages (0x11C0 - 0x11CF) - Drive-Quantum Integration */
    BIO_MSG_QUANTUM_DRIVE_OPTIMIZATION_REQUEST = 0x11C0, /**< Request quantum optimization for drives */
    BIO_MSG_QUANTUM_DRIVE_OPTIMIZATION_RESULT,    /**< Result of quantum drive optimization */
    BIO_MSG_QUANTUM_DRIVE_QUBO_READY,             /**< QUBO formulation ready for solving */
    BIO_MSG_QUANTUM_DRIVE_ALIGNMENT_CHECK,        /**< Alignment constraint verification request */
    BIO_MSG_QUANTUM_DRIVE_ALIGNMENT_RESULT,       /**< Alignment verification result */
    BIO_MSG_QUANTUM_DRIVE_MODE_CHANGE,            /**< Compute mode changed */
    BIO_MSG_QUANTUM_DRIVE_FALLBACK,               /**< Fallback to classical computation */
    BIO_MSG_QUANTUM_DRIVE_CONFLICT_DETECTED,      /**< Drive conflict requiring resolution */

    /* Hypothalamus-Immune bridge messages (0x11D0 - 0x11DF) - Neuroimmune Integration */
    BIO_MSG_HYPO_IMMUNE_FEVER_STATE = 0x11D0,     /**< Fever state from cytokines */
    BIO_MSG_HYPO_IMMUNE_SICKNESS_STATE,           /**< Sickness behavior level */
    BIO_MSG_HYPO_IMMUNE_CORTISOL_STATE,           /**< HPA axis cortisol output */
    BIO_MSG_HYPO_IMMUNE_CYTOKINE_EFFECT,          /**< Cytokine effect on drives */
    BIO_MSG_HYPO_IMMUNE_HPA_ACTIVATION,           /**< HPA axis activation event */
    BIO_MSG_HYPO_IMMUNE_SAFETY_MODE,              /**< Alignment safety mode trigger */
    BIO_MSG_HYPO_IMMUNE_CIRCADIAN_PHASE,          /**< Circadian immune phase */
    BIO_MSG_HYPO_IMMUNE_STORM_ALERT,              /**< Cytokine storm emergency */

    /* Hypothalamus-Insula bridge messages (0x11E0 - 0x11EF) - Interoceptive Integration */
    BIO_MSG_HYPO_INSULA_INTERO_UPDATE = 0x11E0,   /**< Interoceptive state from insula */
    BIO_MSG_HYPO_INSULA_ATTENTION_REQUEST,        /**< Request attention modulation */
    BIO_MSG_HYPO_INSULA_ATTENTION_OUTPUT,         /**< Attention modulation output */
    BIO_MSG_HYPO_INSULA_CARDIAC_STATE,            /**< Cardiac interoception */
    BIO_MSG_HYPO_INSULA_GASTRIC_STATE,            /**< Gastric interoception */
    BIO_MSG_HYPO_INSULA_THERMAL_STATE,            /**< Thermal interoception */
    BIO_MSG_HYPO_INSULA_PAIN_STATE,               /**< Pain/nociceptive interoception */
    BIO_MSG_HYPO_INSULA_DISTRESS_SIGNAL,          /**< Interoceptive distress */

    /* Hypothalamus-Sleep bridge messages (0x11F0 - 0x11FF) - Circadian Integration */
    BIO_MSG_HYPO_SLEEP_STATE_UPDATE = 0x11F0,     /**< Sleep state from sleep system */
    BIO_MSG_HYPO_SLEEP_SCN_REQUEST,               /**< Request SCN circadian output */
    BIO_MSG_HYPO_SLEEP_SCN_OUTPUT,                /**< SCN circadian phase output */
    BIO_MSG_HYPO_SLEEP_PROPENSITY,                /**< Sleep propensity signal */
    BIO_MSG_HYPO_SLEEP_PRESSURE,                  /**< Sleep pressure update */
    BIO_MSG_HYPO_SLEEP_ALERTNESS,                 /**< Alertness level */
    BIO_MSG_HYPO_SLEEP_MELATONIN,                 /**< Melatonin signal */
    BIO_MSG_HYPO_SLEEP_DEPRIVATION_ALERT,         /**< Sleep deprivation warning */

    /* Hypothalamus-Emotion bridge messages (0x1200 - 0x120F) - HPA Axis Integration */
    BIO_MSG_HYPO_EMOTION_UPDATE = 0x1200,         /**< Emotional state input */
    BIO_MSG_HYPO_EMOTION_HPA_REQUEST,             /**< Request HPA output */
    BIO_MSG_HYPO_EMOTION_HPA_OUTPUT,              /**< HPA axis output state */
    BIO_MSG_HYPO_EMOTION_CRH_LEVEL,               /**< CRH release level */
    BIO_MSG_HYPO_EMOTION_CORTISOL_LEVEL,          /**< Cortisol level */
    BIO_MSG_HYPO_EMOTION_STRESS_STATE,            /**< Stress response state */
    BIO_MSG_HYPO_EMOTION_DAMPENING,               /**< Emotional dampening factor */
    BIO_MSG_HYPO_EMOTION_CHRONIC_STRESS,          /**< Chronic stress alert */

    /* Hypothalamus-Wellbeing bridge messages (0x1210 - 0x121F) - Homeostatic Wellbeing */
    BIO_MSG_HYPO_WB_DISTRESS_REPORT = 0x1210,     /**< Distress report output */
    BIO_MSG_HYPO_WB_DISTRESS_REQUEST,             /**< Request distress assessment */
    BIO_MSG_HYPO_WB_FEEDBACK,                     /**< Wellbeing feedback input */
    BIO_MSG_HYPO_WB_INTERVENTION_NEEDED,          /**< Intervention recommendation */
    BIO_MSG_HYPO_WB_SAFETY_THREAT,                /**< Safety threatened alert */
    BIO_MSG_HYPO_WB_CHRONIC_LOAD,                 /**< Chronic stress load */
    BIO_MSG_HYPO_WB_STATE_CHANGE,                 /**< Wellbeing state change */
    BIO_MSG_HYPO_WB_CONFLICT_LEVEL,               /**< Multi-drive conflict level */

    /* Hypothalamus-Perception bridge messages (0x1220 - 0x122F) - Sensory Modulation */
    BIO_MSG_HYPO_PERCEPTION_MODULATION_OUTPUT = 0x1220, /**< Perception modulation output */
    BIO_MSG_HYPO_PERCEPTION_MODULATION_REQUEST,   /**< Request modulation computation */
    BIO_MSG_HYPO_PERCEPTION_AROUSAL_UPDATE,       /**< Arousal level update */
    BIO_MSG_HYPO_PERCEPTION_DETECTION,            /**< Sensory detection feedback */
    BIO_MSG_HYPO_PERCEPTION_CATEGORY_SALIENCE,    /**< Category salience update */
    BIO_MSG_HYPO_PERCEPTION_THREAT_PRIORITY,      /**< Threat priority flag */
    BIO_MSG_HYPO_PERCEPTION_SURVIVAL_MODE,        /**< Survival mode state */
    BIO_MSG_HYPO_PERCEPTION_ANTICIPATION,         /**< Drive anticipation from detection */

    /* Hypothalamus-Broca bridge messages (0x1230 - 0x123F) - Speech Modulation */
    BIO_MSG_HYPO_BROCA_MODULATION_OUTPUT = 0x1230, /**< Speech modulation output */
    BIO_MSG_HYPO_BROCA_MODULATION_REQUEST,        /**< Request modulation computation */
    BIO_MSG_HYPO_BROCA_STRESS_UPDATE,             /**< HPA stress state update */
    BIO_MSG_HYPO_BROCA_AROUSAL_UPDATE,            /**< Arousal level update */
    BIO_MSG_HYPO_BROCA_STATE_CHANGE,              /**< Speech state change */
    BIO_MSG_HYPO_BROCA_FLUENCY_LEVEL,             /**< Fluency level update */
    BIO_MSG_HYPO_BROCA_ALARM_ACTIVE,              /**< Alarm vocalization active */
    BIO_MSG_HYPO_BROCA_INITIATION_MODE,           /**< Speech initiation mode */

    /* Locus Coeruleus messages (0x1240 - 0x124F) - Norepinephrine/Arousal */
    BIO_MSG_LC_NE_STATE = 0x1240,                 /**< Norepinephrine state broadcast */
    BIO_MSG_LC_AROUSAL_CHANGE,                    /**< Arousal level change */
    BIO_MSG_LC_ALERTNESS_SIGNAL,                  /**< Alertness modulation signal */
    BIO_MSG_LC_PHASIC_BURST,                      /**< Phasic NE burst (novelty/salience) */
    BIO_MSG_LC_TONIC_SHIFT,                       /**< Tonic baseline shift */
    BIO_MSG_LC_GAIN_MODULATION,                   /**< Neural gain modulation */
    BIO_MSG_LC_STRESS_RESPONSE,                   /**< Stress-induced NE release */
    BIO_MSG_LC_VIGILANCE_UPDATE,                  /**< Vigilance state update */
    BIO_MSG_LC_ATTENTION_BIAS,                    /**< Attention bias from NE */
    BIO_MSG_LC_PLASTICITY_GATE,                   /**< Plasticity gating signal */

    /* Ventral Tegmental Area messages (0x1250 - 0x125F) - Dopamine/Reward */
    BIO_MSG_VTA_DA_STATE = 0x1250,                /**< Dopamine state broadcast */
    BIO_MSG_VTA_RPE,                              /**< Reward prediction error */
    BIO_MSG_VTA_DOPAMINE_BURST,                   /**< Phasic dopamine burst (reward) */
    BIO_MSG_VTA_DOPAMINE_DIP,                     /**< Phasic dopamine dip (omission) */
    BIO_MSG_VTA_TONIC_CHANGE,                     /**< Tonic DA baseline change */
    BIO_MSG_VTA_MOTIVATION_UPDATE,                /**< Motivational signal */
    BIO_MSG_VTA_VALUE_UPDATE,                     /**< Value prediction update */
    BIO_MSG_VTA_INCENTIVE_SALIENCE,               /**< Incentive salience signal */
    BIO_MSG_VTA_LEARNING_SIGNAL,                  /**< DA-based learning signal */
    BIO_MSG_VTA_PLASTICITY_GATE,                  /**< Dopamine plasticity gate */

    /* Raphe Nuclei messages (0x1260 - 0x126F) - Serotonin/Mood */
    BIO_MSG_RAPHE_5HT_STATE = 0x1260,             /**< Serotonin state broadcast */
    BIO_MSG_RAPHE_MOOD_CHANGE,                    /**< Mood state change */
    BIO_MSG_RAPHE_IMPULSE_CONTROL,                /**< Impulse control signal */
    BIO_MSG_RAPHE_PATIENCE_SIGNAL,                /**< Patience/delay tolerance */
    BIO_MSG_RAPHE_TONIC_SHIFT,                    /**< Tonic 5-HT baseline shift */
    BIO_MSG_RAPHE_CIRCADIAN_MODULATION,           /**< Circadian rhythm influence */
    BIO_MSG_RAPHE_PAIN_MODULATION,                /**< Pain perception modulation */
    BIO_MSG_RAPHE_ANXIETY_STATE,                  /**< Anxiety state update */
    BIO_MSG_RAPHE_SOCIAL_SIGNAL,                  /**< Social behavior modulation */
    BIO_MSG_RAPHE_PLASTICITY_GATE,                /**< Serotonin plasticity gate */

    /* Habenula messages (0x1270 - 0x127F) - Aversive Learning */
    BIO_MSG_HABENULA_STATE = 0x1270,              /**< Habenula state broadcast */
    BIO_MSG_HABENULA_NEGATIVE_RPE,                /**< Negative reward prediction error */
    BIO_MSG_HABENULA_PUNISHMENT_SIGNAL,           /**< Punishment detection signal */
    BIO_MSG_HABENULA_DISAPPOINTMENT,              /**< Disappointment/omission signal */
    BIO_MSG_HABENULA_AVOIDANCE_TRIGGER,           /**< Avoidance behavior trigger */
    BIO_MSG_HABENULA_VTA_INHIBIT,                 /**< VTA/SNc inhibition signal */
    BIO_MSG_HABENULA_RAPHE_INHIBIT,               /**< Raphe inhibition signal */
    BIO_MSG_HABENULA_AVERSIVE_LEARNING,           /**< Aversive learning signal */
    BIO_MSG_HABENULA_RELIEF_SIGNAL,               /**< Relief from expected punishment */
    BIO_MSG_HABENULA_PLASTICITY_GATE,             /**< Habenula plasticity gate */

    /* Collective Cognition: Portia bridge messages (0x2E10 - 0x2E1F) */
    BIO_MSG_PORTIA_COLLECTIVE_TIER_CHANGE = 0x2E11,   /**< Portia tier change broadcast */
    BIO_MSG_PORTIA_COLLECTIVE_STATE_UPDATE,           /**< Portia state update */
    BIO_MSG_PORTIA_COLLECTIVE_OFFLOAD_REQUEST,        /**< Portia offload request */
    BIO_MSG_PORTIA_COLLECTIVE_DEGRADATION,            /**< Portia degradation event */

    /* Collective Cognition: Occipital bridge messages (0x2E20 - 0x2E2F) */
    BIO_MSG_OCCIPITAL_COLLECTIVE_ATTENTION = 0x2E21,  /**< Joint attention target */
    BIO_MSG_OCCIPITAL_COLLECTIVE_FEATURE,             /**< Shared visual feature */
    BIO_MSG_OCCIPITAL_COLLECTIVE_STATE_UPDATE,        /**< Occipital state update */
    BIO_MSG_OCCIPITAL_COLLECTIVE_GAZE_FOLLOW,         /**< Gaze following event */

    /* Imagination Engine messages (0x1A00 - 0x1AFF) */
    BIO_MSG_IMAGINATION_REQUEST = 0x1A00,             /**< Request imagination scenario */
    BIO_MSG_IMAGINATION_RESULT,                       /**< Imagination scenario result */
    BIO_MSG_IMAGINATION_SCENARIO_START,               /**< Scenario started notification */
    BIO_MSG_IMAGINATION_SCENARIO_STEP,                /**< Scenario step complete */
    BIO_MSG_IMAGINATION_SCENARIO_END,                 /**< Scenario ended */
    BIO_MSG_IMAGINATION_VISUAL_READY,                 /**< Visual content generated */
    BIO_MSG_IMAGINATION_AUDIO_READY,                  /**< Audio content generated */
    BIO_MSG_IMAGINATION_COHERENCE_CHECK,              /**< Request coherence evaluation */
    BIO_MSG_IMAGINATION_COHERENCE_RESULT,             /**< Coherence evaluation result */
    BIO_MSG_IMAGINATION_COUNTERFACTUAL_QUERY,         /**< Counterfactual reasoning request */
    BIO_MSG_IMAGINATION_COUNTERFACTUAL_RESULT,        /**< Counterfactual result */
    BIO_MSG_IMAGINATION_PROSPECTIVE_QUERY,            /**< Future simulation request */
    BIO_MSG_IMAGINATION_PROSPECTIVE_RESULT,           /**< Prospective simulation result */
    BIO_MSG_IMAGINATION_SOCIAL_SIMULATE,              /**< ToM simulation request */
    BIO_MSG_IMAGINATION_SOCIAL_RESULT,                /**< Social simulation result */
    BIO_MSG_IMAGINATION_MEMORY_REQUEST,               /**< Request memory for imagination */
    BIO_MSG_IMAGINATION_MEMORY_RESPONSE,              /**< Memory retrieval response */
    BIO_MSG_IMAGINATION_VIVIDNESS_UPDATE,             /**< Vividness modulation (from immune) */
    BIO_MSG_IMAGINATION_CAPACITY_UPDATE,              /**< Capacity modulation (from substrate) */
    BIO_MSG_IMAGINATION_ATTENTION_GATE,               /**< Attention gating (from thalamic) */
    BIO_MSG_IMAGINATION_GOAL_UPDATE,                  /**< Goal update notification */
    BIO_MSG_IMAGINATION_MODE_CHANGE,                  /**< Imagination mode changed */
    BIO_MSG_IMAGINATION_COLLECTIVE_SHARE,             /**< Share scenario with swarm */
    BIO_MSG_IMAGINATION_COLLECTIVE_INSIGHT,           /**< Receive swarm insights */

    /* Omnidirectional inference messages (0x1B00 - 0x1BFF) */
    BIO_MSG_OMNI_PREDICT_REQUEST = 0x1B00,            /**< Request omnidirectional prediction */
    BIO_MSG_OMNI_PREDICT_RESULT,                      /**< Prediction result */
    BIO_MSG_OMNI_DIRECTION_SWITCH,                    /**< Switch prediction direction */
    BIO_MSG_OMNI_PRECISION_UPDATE,                    /**< Update precision weights */
    BIO_MSG_OMNI_FREE_ENERGY_REPORT,                  /**< Free energy computation result */
    BIO_MSG_OMNI_MULTI_PREDICT_REQUEST,               /**< Multi-direction prediction request */
    BIO_MSG_OMNI_MULTI_PREDICT_RESULT,                /**< Multi-direction prediction result */

    /* Hopfield memory messages (0x1B10 - 0x1B1F) */
    BIO_MSG_HOPFIELD_STORE = 0x1B10,                  /**< Store pattern in Hopfield memory */
    BIO_MSG_HOPFIELD_RETRIEVE,                        /**< Retrieve pattern by query */
    BIO_MSG_HOPFIELD_RESULT,                          /**< Retrieval result */
    BIO_MSG_HOPFIELD_BATCH_RETRIEVE,                  /**< Batch retrieval request */
    BIO_MSG_HOPFIELD_BATCH_RESULT,                    /**< Batch retrieval result */
    BIO_MSG_HOPFIELD_TOP_K_REQUEST,                   /**< Request top-k similar patterns */
    BIO_MSG_HOPFIELD_TOP_K_RESULT,                    /**< Top-k result */
    BIO_MSG_HOPFIELD_ENERGY_REPORT,                   /**< Energy landscape report */

    /* Predictive coding hierarchy messages (0x1B20 - 0x1B2F) */
    BIO_MSG_PRED_HIER_FORWARD = 0x1B20,               /**< Bottom-up forward pass */
    BIO_MSG_PRED_HIER_BACKWARD,                       /**< Top-down backward pass */
    BIO_MSG_PRED_HIER_UPDATE,                         /**< Full belief update */
    BIO_MSG_PRED_HIER_ERROR_PROPAGATE,                /**< Propagate prediction errors */
    BIO_MSG_PRED_HIER_PRECISION_UPDATE,               /**< Update level precision */
    BIO_MSG_PRED_HIER_STATE_QUERY,                    /**< Query level state */
    BIO_MSG_PRED_HIER_STATE_RESPONSE,                 /**< Level state response */
    BIO_MSG_PRED_HIER_FREE_ENERGY,                    /**< Hierarchy free energy */

    /* Temporal replay messages (0x1B30 - 0x1B3F) */
    BIO_MSG_REPLAY_STORE = 0x1B30,                    /**< Store transition in buffer */
    BIO_MSG_REPLAY_SAMPLE,                            /**< Sample from buffer */
    BIO_MSG_REPLAY_SAMPLE_RESULT,                     /**< Sample result */
    BIO_MSG_REPLAY_FORWARD_SWEEP,                     /**< Start forward sweep */
    BIO_MSG_REPLAY_BACKWARD_SWEEP,                    /**< Start backward sweep */
    BIO_MSG_REPLAY_SWEEP_STEP,                        /**< Sweep step notification */
    BIO_MSG_REPLAY_SWEEP_COMPLETE,                    /**< Sweep completed */
    BIO_MSG_REPLAY_PRIORITY_UPDATE,                   /**< Update transition priorities */
    BIO_MSG_REPLAY_SEQUENCE_START,                    /**< Start sequence recording */
    BIO_MSG_REPLAY_SEQUENCE_END,                      /**< End sequence recording */

    /* LGSS (Layered Governance Safety System) messages (0x1C00 - 0x1CFF) */
    /* Evaluation messages (0x1C01 - 0x1C0F) */
    BIO_MSG_LGSS_EVALUATE_REQUEST = 0x1C01,           /**< LGSS evaluation request */
    BIO_MSG_LGSS_EVALUATE_RESPONSE,                   /**< LGSS evaluation response */

    /* Violation notifications (0x1C03 - 0x1C0F) */
    BIO_MSG_LGSS_POLICY_VIOLATION,                    /**< Policy violation detected */
    BIO_MSG_LGSS_ACTION_BLOCKED,                      /**< Action blocked by LGSS */
    BIO_MSG_LGSS_ACTION_ESCALATED,                    /**< Action escalated for review */

    /* Uncertainty/risk alerts (0x1C06 - 0x1C0F) */
    BIO_MSG_LGSS_UNCERTAINTY_ALERT,                   /**< High uncertainty detected */
    BIO_MSG_LGSS_IMPACT_SCORE,                        /**< Impact score notification */
    BIO_MSG_LGSS_RISK_ASSESSMENT,                     /**< Risk assessment result */

    /* Override/control messages (0x1C10 - 0x1C1F) */
    BIO_MSG_LGSS_OVERRIDE_REQUEST = 0x1C10,           /**< Request LGSS override */
    BIO_MSG_LGSS_OVERRIDE_RESPONSE,                   /**< Override request response */
    BIO_MSG_LGSS_HALT_COMMAND,                        /**< Emergency halt command */
    BIO_MSG_LGSS_SOFT_RESET,                          /**< Soft reset command */
    BIO_MSG_LGSS_HARD_RESET,                          /**< Hard reset command */

    /* Telemetry/audit messages (0x1C20 - 0x1C2F) */
    BIO_MSG_LGSS_TELEMETRY_LOG = 0x1C20,              /**< Telemetry log entry */
    BIO_MSG_LGSS_AUDIT_REQUEST,                       /**< Request audit data */
    BIO_MSG_LGSS_AUDIT_RESPONSE,                      /**< Audit data response */

    /* Integrity messages (0x1C30 - 0x1C3F) */
    BIO_MSG_LGSS_INTEGRITY_CHECK = 0x1C30,            /**< Request integrity check */
    BIO_MSG_LGSS_INTEGRITY_RESULT,                    /**< Integrity check result */
    BIO_MSG_LGSS_TAMPERING_DETECTED,                  /**< Tampering detected alert */

    /* Plasticity coordination messages (0x1C40 - 0x1C4F) */
    BIO_MSG_LGSS_SAFETY_EVENT = 0x1C40,               /**< Safety-relevant event */
    BIO_MSG_LGSS_NEUROMOD_SIGNAL,                     /**< Neuromodulator safety signal */

    /* External system communication (0x1C50 - 0x1C5F) - Phase B */
    BIO_MSG_LGSS_EXTERNAL_HEARTBEAT = 0x1C50,         /**< External system heartbeat */
    BIO_MSG_LGSS_EXTERNAL_ATTESTATION,                /**< External attestation */
    BIO_MSG_LGSS_EXTERNAL_COMMAND,                    /**< External command received */

    /* =========================================================================
     * World Model Bridge Messages (0x6300 - 0x6DFF)
     * Bidirectional communication between omni world model and brain subsystems
     * ========================================================================= */

    /* Security-Immune Bridge (0x6300-0x63FF) */
    /* Anomaly Prediction Messages (0x6300-0x630F) */
    BIO_MSG_WM_SECURITY_ANOMALY_PRED = 0x6300,        /**< Anomaly prediction from WM */
    BIO_MSG_WM_SECURITY_ANOMALY_DETECTED,             /**< Anomaly detected notification */
    BIO_MSG_WM_SECURITY_ANOMALY_RESOLVED,             /**< Anomaly resolved notification */
    /* Threat Forecast Messages (0x6310-0x631F) */
    BIO_MSG_WM_SECURITY_THREAT_FORECAST = 0x6310,     /**< Threat forecast */
    BIO_MSG_WM_SECURITY_THREAT_VERIFIED,              /**< Forecast verified by security */
    BIO_MSG_WM_SECURITY_THREAT_SIGNATURE,             /**< New threat signature learned */
    /* Security Event Messages (0x6320-0x632F) */
    BIO_MSG_WM_SECURITY_EVENT = 0x6320,               /**< Security event for WM training */
    BIO_MSG_WM_SECURITY_EVENT_TRAIN,                  /**< Security event for training */
    BIO_MSG_WM_SECURITY_BBB_STATE,                    /**< BBB state update */
    BIO_MSG_WM_SECURITY_BBB_BREACH,                   /**< BBB breach detected */
    /* Immune Modulation Messages (0x6330-0x633F) */
    BIO_MSG_WM_IMMUNE_CYTOKINE_UPDATE = 0x6330,       /**< Cytokine levels to WM */
    BIO_MSG_WM_IMMUNE_MODULATION_APPLIED,             /**< Modulation applied to WM */
    BIO_MSG_WM_IMMUNE_MODULATION_ACK,                 /**< Immune modulation acknowledged */
    BIO_MSG_WM_IMMUNE_INFLAMMATION_STATE,             /**< Inflammation state change */
    BIO_MSG_WM_IMMUNE_PE_RESPONSE,                    /**< Prediction error immune response */
    /* WM -> Immune Messages (0x6340-0x634F) */
    BIO_MSG_WM_PE_IMMUNE_TRIGGER = 0x6340,            /**< Prediction error triggers immune */
    BIO_MSG_WM_THREAT_IMMUNE_ALERT,                   /**< WM threat triggers immune alert */
    BIO_MSG_WM_ANOMALY_ANTIGEN_PRESENT,               /**< Present anomaly as antigen */
    /* Bridge Status Messages (0x6350-0x635F) */
    BIO_MSG_WM_SECURITY_IMMUNE_STATUS = 0x6350,       /**< Bridge status update */
    BIO_MSG_WM_SECURITY_IMMUNE_ERROR,                 /**< Bridge error notification */
    BIO_MSG_WM_SECURITY_IMMUNE_STATS,                 /**< Statistics update */

    /* Logging Bridge (0x6400-0x64FF) */
    BIO_MSG_WM_LOG_PREDICTION = 0x6400,               /**< Log prediction request/outcome */
    BIO_MSG_WM_LOG_TRAINING,                          /**< Log training step */
    BIO_MSG_WM_LOG_ANOMALY,                           /**< Log anomaly detection */
    BIO_MSG_WM_LOG_CONFIDENCE,                        /**< Log confidence calibration */
    BIO_MSG_WM_LOG_REPLAY,                            /**< Log replay buffer operation */
    BIO_MSG_WM_LOG_COUNTERFACTUAL,                    /**< Log counterfactual event */
    BIO_MSG_WM_LOG_WEIGHT_UPDATE = 0x6410,            /**< Log weight update event */
    BIO_MSG_WM_LOG_GRADIENT,                          /**< Log gradient information */
    BIO_MSG_WM_LOG_LOSS,                              /**< Log loss computation */
    BIO_MSG_WM_LOG_INSTABILITY = 0x6420,              /**< Log training instability */
    BIO_MSG_WM_LOG_UNCERTAINTY,                       /**< Log uncertainty metric */
    BIO_MSG_WM_LOG_REPLAY_ADD = 0x6430,               /**< Add to replay buffer */
    BIO_MSG_WM_LOG_REPLAY_SAMPLE,                     /**< Sample from replay buffer */
    BIO_MSG_WM_LOG_DREAM_EPISODE,                     /**< Log dream/imagination episode */
    BIO_MSG_WM_LOG_BRIDGE_STATUS = 0x6440,            /**< Bridge status update */
    BIO_MSG_WM_LOG_BRIDGE_ERROR,                      /**< Bridge error notification */
    BIO_MSG_WM_LOG_FLUSH,                             /**< Flush log buffer */
    BIO_MSG_WM_LOG_STATS_UPDATE,                      /**< Statistics update */

    /* Cognitive Bridge (0x6500-0x65FF) */
    BIO_MSG_WM_COGNITIVE_STATE_PRED = 0x6500,         /**< State prediction for planning */
    BIO_MSG_WM_COGNITIVE_GOAL_UPDATE,                 /**< Goal/intention update */
    BIO_MSG_WM_COGNITIVE_ACTION_CONSEQUENCE,          /**< Action consequence prediction */
    BIO_MSG_WM_ATTENTION_FOCUS = 0x6510,              /**< Attention focus signal */
    BIO_MSG_WM_COGNITIVE_WORKING_MEM,                 /**< Working memory integration */
    BIO_MSG_WM_COGNITIVE_SALIENCE,                    /**< Salience signal to WM */
    BIO_MSG_WM_COGNITIVE_META_LEARNING,               /**< Meta-learning integration */

    /* Parietal Bridge (0x6600-0x66FF) */
    BIO_MSG_WM_PARIETAL_SPATIAL_PRED = 0x6600,        /**< Spatial state prediction */
    BIO_MSG_WM_PARIETAL_SPATIAL_PRED_RESULT,          /**< Spatial prediction result */
    BIO_MSG_WM_PARIETAL_PHYSICS_QUERY,                /**< Physics constraint query */
    BIO_MSG_WM_PARIETAL_PHYSICS_RESULT,               /**< Physics query result */
    BIO_MSG_WM_PARIETAL_PHYSICS_CONSTRAINT,           /**< Physics constraint notification */
    BIO_MSG_WM_PARIETAL_COLLISION_CHECK,              /**< Collision check request */
    BIO_MSG_WM_PARIETAL_COORD_TRANSFORM = 0x6610,     /**< Coordinate transformation */
    BIO_MSG_WM_PARIETAL_COORD_RESULT,                 /**< Coordinate transform result */
    BIO_MSG_WM_PARIETAL_FRAME_UPDATE,                 /**< Reference frame update */
    BIO_MSG_WM_PARIETAL_TRAJECTORY,                   /**< Trajectory forecast */
    BIO_MSG_WM_PARIETAL_TRAJECTORY_PRED,              /**< Trajectory prediction request */
    BIO_MSG_WM_PARIETAL_TRAJECTORY_RESULT,            /**< Trajectory prediction result */
    BIO_MSG_WM_PARIETAL_MATH_REASONING,               /**< Mathematical reasoning */
    BIO_MSG_WM_PARIETAL_MATH_PRED = 0x6620,           /**< Math prediction request */
    BIO_MSG_WM_PARIETAL_NUMERICAL_EST,                /**< Numerical estimation */
    BIO_MSG_WM_PARIETAL_PATTERN_EXTRAP,               /**< Pattern extrapolation */
    BIO_MSG_WM_PARIETAL_ATTENTION_UPDATE = 0x6630,    /**< Spatial attention update */
    BIO_MSG_WM_PARIETAL_SALIENCE_MAP,                 /**< Salience map update */
    BIO_MSG_WM_PARIETAL_FOCUS_SHIFT,                  /**< Focus shift notification */
    BIO_MSG_WM_PARIETAL_BRIDGE_STATUS = 0x6640,       /**< Bridge status update */
    BIO_MSG_WM_PARIETAL_BRIDGE_ERROR,                 /**< Bridge error notification */
    BIO_MSG_WM_PARIETAL_STATS_UPDATE,                 /**< Statistics update */

    /* Hypothalamus Bridge (0x6700-0x67FF) */
    BIO_MSG_WM_HYPOTHAL_DRIVE_STATE = 0x6700,         /**< Homeostatic drive state */
    BIO_MSG_WM_HYPOTHAL_DRIVE_URGENCY,                /**< Drive urgency signal */
    BIO_MSG_WM_HYPOTHAL_DRIVE_PRIORITY,               /**< Drive priority update */
    BIO_MSG_WM_HYPOTHAL_DRIVE_SATISFIED,              /**< Drive satisfaction event */
    BIO_MSG_WM_HYPOTHAL_CIRCADIAN,                    /**< Circadian rhythm signal */
    BIO_MSG_WM_HYPOTHAL_CIRCADIAN_MOD,                /**< Circadian modulation */
    BIO_MSG_WM_HYPOTHAL_TIME_OF_DAY,                  /**< Time of day signal */
    BIO_MSG_WM_HYPOTHAL_RESOURCE_PRED = 0x6710,       /**< Resource availability prediction */
    BIO_MSG_WM_HYPOTHAL_RESOURCE_AVAIL,               /**< Resource availability update */
    BIO_MSG_WM_HYPOTHAL_RESOURCE_FORECAST,            /**< Resource forecast */
    BIO_MSG_WM_HYPOTHAL_STRESS_MOD,                   /**< Stress modulation signal */
    BIO_MSG_WM_HYPOTHAL_STRESS_STATE,                 /**< Stress state update */
    BIO_MSG_WM_HYPOTHAL_AROUSAL_STATE,                /**< Arousal state update */
    BIO_MSG_WM_HYPOTHAL_REWARD_PRED,                  /**< Reward prediction from drives */
    BIO_MSG_WM_HYPOTHAL_REWARD_SIGNAL,                /**< Reward signal notification */
    BIO_MSG_WM_HYPOTHAL_HOMEOSTASIS = 0x6720,         /**< Homeostasis state */
    BIO_MSG_WM_HYPOTHAL_CONSERVATIVE_MODE,            /**< Conservative mode activation */
    BIO_MSG_WM_HYPOTHAL_ALIGNMENT_CHECK = 0x6730,     /**< Alignment verification check */
    BIO_MSG_WM_HYPOTHAL_SETPOINT_ERROR,               /**< Homeostatic setpoint error */
    BIO_MSG_WM_HYPOTHAL_CONTROLLER_OUT,               /**< Controller output signal */
    BIO_MSG_WM_HYPOTHAL_BRIDGE_STATUS = 0x6740,       /**< Bridge status update */
    BIO_MSG_WM_HYPOTHAL_BRIDGE_ERROR,                 /**< Bridge error notification */
    BIO_MSG_WM_HYPOTHAL_STATS_UPDATE,                 /**< Statistics update */

    /* Thalamic Bridge (0x6800-0x68FF) */
    BIO_MSG_WM_THALAMIC_GATE_INPUT = 0x6800,          /**< Gated sensory input */
    BIO_MSG_WM_THALAMIC_GATE_VISUAL,                  /**< Visual gating control */
    BIO_MSG_WM_THALAMIC_GATE_AUDITORY,                /**< Auditory gating control */
    BIO_MSG_WM_THALAMIC_GATE_MOTOR,                   /**< Motor gating control */
    BIO_MSG_WM_THALAMIC_GATE_EXECUTIVE,               /**< Executive gating control */
    BIO_MSG_WM_THALAMIC_ATTENTION_BIAS,               /**< Prediction-based attention bias */
    BIO_MSG_WM_THALAMIC_ATTENTION_UPDATE,             /**< Attention state update */
    BIO_MSG_WM_THALAMIC_TRN_INHIBIT = 0x6810,         /**< TRN selective inhibition */
    BIO_MSG_WM_THALAMIC_TRN_RELEASE,                  /**< TRN release signal */
    BIO_MSG_WM_THALAMIC_TRN_MODULATE,                 /**< TRN modulation */
    BIO_MSG_WM_THALAMIC_LGN_MGN,                      /**< Visual/auditory gating */
    BIO_MSG_WM_THALAMIC_PULVINAR,                     /**< Attention-guided selection */
    BIO_MSG_WM_THALAMIC_PULVINAR_WEIGHT,              /**< Pulvinar weight update */
    BIO_MSG_WM_THALAMIC_SALIENCE_PRED,                /**< Salience prediction */
    BIO_MSG_WM_THALAMIC_MD,                           /**< Prefrontal-WM coordination */
    BIO_MSG_WM_THALAMIC_VA_VL,                        /**< Motor prediction relay */
    BIO_MSG_WM_THALAMIC_PRED_ERROR = 0x6820,          /**< Prediction error signal */
    BIO_MSG_WM_THALAMIC_PRED_CONFIDENCE,              /**< Prediction confidence */
    BIO_MSG_WM_THALAMIC_PRED_UPDATE,                  /**< Prediction state update */
    BIO_MSG_WM_THALAMIC_MODE_TONIC = 0x6830,          /**< Tonic firing mode */
    BIO_MSG_WM_THALAMIC_MODE_BURST,                   /**< Burst firing mode */
    BIO_MSG_WM_THALAMIC_AROUSAL_UPDATE,               /**< Arousal state update */
    BIO_MSG_WM_THALAMIC_BRIDGE_STATUS = 0x6840,       /**< Bridge status update */
    BIO_MSG_WM_THALAMIC_BRIDGE_ERROR,                 /**< Bridge error notification */
    BIO_MSG_WM_THALAMIC_STATS_UPDATE,                 /**< Statistics update */

    /* Substrate Bridge (0x6900-0x69FF) */
    BIO_MSG_WM_SUBSTRATE_METABOLIC = 0x6900,          /**< Metabolic state (ATP, O2, glucose) */
    BIO_MSG_WM_SUBSTRATE_DEMAND,                      /**< Computational demand signal */
    BIO_MSG_WM_SUBSTRATE_CONSTRAINT = 0x6910,         /**< Metabolic constraint alert */
    BIO_MSG_WM_SUBSTRATE_HORIZON_ADJUST,              /**< Reduce prediction horizon */

    /* Memory Bridge (0x6A00-0x6AFF) */
    BIO_MSG_WM_MEMORY_REPLAY_SEQ = 0x6A00,            /**< Hippocampal replay sequence */
    BIO_MSG_WM_MEMORY_REPLAY_TRAIN_REQ,               /**< Request training from replay */
    BIO_MSG_WM_MEMORY_REPLAY_TRAIN_DONE,              /**< Training complete notification */
    BIO_MSG_WM_MEMORY_ENGRAM_ENCODE = 0x6A10,         /**< Engram encoding request */
    BIO_MSG_WM_MEMORY_ENGRAM_RETRIEVE,                /**< Engram retrieval result */
    BIO_MSG_WM_MEMORY_ENGRAM_CONTEXT,                 /**< Episodic context response */
    BIO_MSG_WM_MEMORY_CONSOLIDATION = 0x6A20,         /**< Consolidation signal */
    BIO_MSG_WM_MEMORY_CONSOLIDATION_SYNC,             /**< Sync with consolidation cycle */
    BIO_MSG_WM_MEMORY_SEMANTIC_TRANSFER,              /**< Semantic features for cortex */
    BIO_MSG_WM_MEMORY_HIPPOCAMPAL_PRED = 0x6A30,      /**< Predicted sequence comparison */
    BIO_MSG_WM_MEMORY_HIPPOCAMPAL_ERROR,              /**< Prediction error feedback */
    BIO_MSG_WM_MEMORY_PATTERN_COMPLETE,               /**< Pattern completion request */
    BIO_MSG_WM_MEMORY_PATTERN_SEPARATE,               /**< Pattern separation request */
    BIO_MSG_WM_MEMORY_DG_SEPARATION,                  /**< Pattern separation result */
    BIO_MSG_WM_MEMORY_CA3_COMPLETION,                 /**< Pattern completion result */
    BIO_MSG_WM_MEMORY_CA1_TEMPORAL,                   /**< Temporal sequence encoding */
    BIO_MSG_WM_MEMORY_BRIDGE_STATUS = 0x6A40,         /**< Bridge status update */
    BIO_MSG_WM_MEMORY_BRIDGE_ERROR,                   /**< Bridge error notification */
    BIO_MSG_WM_MEMORY_STATS_UPDATE,                   /**< Statistics update */

    /* KG Wiring Bridge (0x6B00-0x6BFF) */
    BIO_MSG_WM_KG_ENTITY_PRED = 0x6B00,               /**< KG entity state prediction */
    BIO_MSG_WM_KG_ENTITY_BATCH_PRED,                  /**< Batch entity prediction */
    BIO_MSG_WM_KG_RELATIONSHIP_PRED,                  /**< Relationship change prediction */
    BIO_MSG_WM_KG_MODULE_PRED,                        /**< Module health prediction */
    BIO_MSG_WM_KG_MODULE_HEALTH_UPDATE,               /**< Module health status update */
    BIO_MSG_WM_KG_MODULE_DEGRADED,                    /**< Module degradation detected */
    BIO_MSG_WM_KG_EXCEPTION_NOTIFY = 0x6B10,          /**< Exception notification to WM */
    BIO_MSG_WM_KG_WIRING_CHANGE,                      /**< Wiring topology change */
    BIO_MSG_WM_KG_REGISTRY_SYNC,                      /**< Registry synchronization */
    BIO_MSG_WM_KG_ANOMALY_DETECTED,                   /**< KG anomaly detected */
    BIO_MSG_WM_KG_FAILURE_PREDICTION,                 /**< Predicted failure notification */
    BIO_MSG_WM_KG_TRAINING_EVENT = 0x6B20,            /**< KG event for WM training */
    BIO_MSG_WM_KG_TRAINING_BATCH,                     /**< Batch training event */
    BIO_MSG_WM_KG_SYSTEM_STABILITY,                   /**< System stability forecast */
    BIO_MSG_WM_KG_BRIDGE_STATUS = 0x6B40,             /**< Bridge status update */
    BIO_MSG_WM_KG_BRIDGE_ERROR,                       /**< Bridge error notification */
    BIO_MSG_WM_KG_STATS_UPDATE,                       /**< Statistics update */

    /* Theory of Mind Bridge (0x6C00-0x6CFF) */
    BIO_MSG_WM_TOM_MENTAL_STATE_PRED = 0x6C00,        /**< Mental state prediction request */
    BIO_MSG_WM_TOM_MENTAL_STATE_RESULT,               /**< Mental state prediction result */
    BIO_MSG_WM_TOM_BELIEF_UPDATE,                     /**< Belief state update */
    BIO_MSG_WM_TOM_DESIRE_UPDATE,                     /**< Desire state update */
    BIO_MSG_WM_TOM_INTENTION_UPDATE,                  /**< Intention state update */
    BIO_MSG_WM_TOM_TRAJECTORY_PRED = 0x6C10,          /**< Social trajectory prediction */
    BIO_MSG_WM_TOM_TRAJECTORY_RESULT,                 /**< Trajectory prediction result */
    BIO_MSG_WM_TOM_TRAJECTORY_STEP,                   /**< Single trajectory step */
    BIO_MSG_WM_TOM_COUNTERFACTUAL_REQ = 0x6C20,       /**< Counterfactual reasoning request */
    BIO_MSG_WM_TOM_COUNTERFACTUAL_RESULT,             /**< Counterfactual result */
    BIO_MSG_WM_TOM_WHAT_IF_BELIEF,                    /**< What-if belief scenario */
    BIO_MSG_WM_TOM_FALSE_BELIEF_DETECT = 0x6C30,      /**< False belief detected */
    BIO_MSG_WM_TOM_BELIEF_REALITY_GAP,                /**< Belief-reality gap update */
    BIO_MSG_WM_TOM_BELIEF_SYNC,                       /**< Belief synchronization */
    BIO_MSG_WM_TOM_SOCIAL_INTERACTION = 0x6C40,       /**< Social interaction for training */
    BIO_MSG_WM_TOM_INTERACTION_OUTCOME,               /**< Interaction outcome */
    BIO_MSG_WM_TOM_COOPERATION_SIGNAL,                /**< Cooperation detected */
    BIO_MSG_WM_TOM_COMPETITION_SIGNAL,                /**< Competition detected */
    BIO_MSG_WM_TOM_JOINT_PREDICTION = 0x6C50,         /**< Multi-agent joint prediction */
    BIO_MSG_WM_TOM_JOINT_RESULT,                      /**< Joint prediction result */
    BIO_MSG_WM_TOM_GROUP_DYNAMICS,                    /**< Group dynamics update */
    BIO_MSG_WM_TOM_EMPATHY_SIMULATION = 0x6C60,       /**< Empathetic perspective-taking */
    BIO_MSG_WM_TOM_EMPATHY_RESULT,                    /**< Empathy simulation result */
    BIO_MSG_WM_TOM_MIRROR_ACTION,                     /**< Mirror neuron action prediction */
    BIO_MSG_WM_TOM_BRIDGE_STATUS = 0x6C80,            /**< Bridge status update */
    BIO_MSG_WM_TOM_BRIDGE_ERROR,                      /**< Bridge error notification */
    BIO_MSG_WM_TOM_STATS_UPDATE,                      /**< Statistics update */

    /* Plasticity Bridge (0x6D00-0x6DFF) */
    BIO_MSG_WM_PLASTICITY_STDP_EVENT = 0x6D00,        /**< STDP event notification */
    BIO_MSG_WM_PLASTICITY_STDP_MOD,                   /**< STDP modulation params */
    BIO_MSG_WM_PLASTICITY_STDP_BATCH,                 /**< Batch STDP processing */
    BIO_MSG_WM_PLASTICITY_WEIGHT_UPDATE,              /**< Weight update notification */
    BIO_MSG_WM_PLASTICITY_SPIKE_SEQ = 0x6D10,         /**< Spike sequence for training */
    BIO_MSG_WM_PLASTICITY_SNN_PRED,                   /**< Predicted SNN activity */
    BIO_MSG_WM_PLASTICITY_SNN_COMPARE,                /**< SNN prediction comparison */
    BIO_MSG_WM_PLASTICITY_SNN_GUIDE,                  /**< SNN guidance signal */
    BIO_MSG_WM_PLASTICITY_BCM_THRESHOLD = 0x6D20,     /**< BCM threshold update */
    BIO_MSG_WM_PLASTICITY_BCM_CONFIDENCE,             /**< BCM confidence signal */
    BIO_MSG_WM_PLASTICITY_ELIGIBILITY,                /**< Eligibility trace signal */
    BIO_MSG_WM_PLASTICITY_THREE_FACTOR,               /**< Three-factor learning signal */
    BIO_MSG_WM_PLASTICITY_STATE = 0x6D30,             /**< Plasticity state query */
    BIO_MSG_WM_PLASTICITY_PE_FEEDBACK,                /**< PE → plasticity feedback */
    BIO_MSG_WM_PLASTICITY_STP_STATE,                  /**< Short-term plasticity state */
    BIO_MSG_WM_PLASTICITY_STP_MODULATE,               /**< STP modulation params */
    BIO_MSG_WM_PLASTICITY_COORD_SYNC = 0x6D40,        /**< Coordinator sync */
    BIO_MSG_WM_PLASTICITY_BRIDGE_STATUS = 0x6D50,     /**< Bridge status update */
    BIO_MSG_WM_PLASTICITY_BRIDGE_ERROR,               /**< Bridge error notification */
    BIO_MSG_WM_PLASTICITY_STATS_UPDATE,               /**< Statistics update */

    /* Self-Repair / Fault Tolerance messages (0x6E00 - 0x6EFF) */
    BIO_MSG_SELF_REPAIR_REQUEST = 0x6E00,             /**< Initiate self-repair pipeline */
    BIO_MSG_SELF_REPAIR_RESULT,                       /**< Self-repair outcome notification */
    BIO_MSG_SELF_REPAIR_STAGE_CHANGE,                 /**< Repair stage transition */
    BIO_MSG_SELF_REPAIR_ROLLBACK,                     /**< Rollback repair request */
    BIO_MSG_CODE_GEN_REQUEST = 0x6E10,                /**< Request code fix generation */
    BIO_MSG_CODE_GEN_RESULT,                          /**< Generated fix result */
    BIO_MSG_CODE_GEN_VALIDATE,                        /**< Request fix validation */
    BIO_MSG_CODE_GEN_LEARN,                           /**< Learn from fix outcome */
    BIO_MSG_VCS_WRITE_FIX = 0x6E20,                   /**< Write fix to source file */
    BIO_MSG_VCS_COMMIT,                               /**< Commit fix to VCS */
    BIO_MSG_VCS_ROLLBACK,                             /**< Rollback VCS change */
    BIO_MSG_VCS_STATUS,                               /**< VCS status query */
    BIO_MSG_DIAGNOSTIC_REQUEST = 0x6E30,              /**< Request fault diagnosis */
    BIO_MSG_DIAGNOSTIC_RESULT,                        /**< Diagnostic analysis result */
    BIO_MSG_HOT_PATCH_APPLY = 0x6E40,                 /**< Apply hot-patch */
    BIO_MSG_HOT_PATCH_ROLLBACK,                       /**< Rollback hot-patch */
    BIO_MSG_HOT_PATCH_STATUS,                         /**< Hot-patch status query */

    /* Health Self-Repair Bridge messages (0x6E50-0x6E57) */
    BIO_MSG_HEALTH_SELF_REPAIR_TRIGGER = 0x6E50,     /**< Health-to-repair trigger */
    BIO_MSG_HEALTH_SELF_REPAIR_OUTCOME,              /**< Repair outcome notification */
    BIO_MSG_HEALTH_SELF_REPAIR_STATS,                /**< Bridge statistics update */
    BIO_MSG_HEALTH_SELF_REPAIR_RATE_LIMIT,           /**< Rate limiting event */
    BIO_MSG_HEALTH_DIAGNOSTIC_CONVERTED,             /**< Diagnostic conversion complete */

    /* Code Immune Self-Repair Integration messages (0x6E58-0x6E5F) */
    BIO_MSG_CODE_IMMUNE_REPAIR_TRIGGER = 0x6E58,     /**< Code immune auto-repair trigger */
    BIO_MSG_CODE_IMMUNE_REPAIR_OUTCOME,              /**< Code immune repair outcome */
    BIO_MSG_CODE_IMMUNE_PATTERN_LEARNED,             /**< Pattern learned from repair */
    BIO_MSG_CODE_IMMUNE_B_CELL_UPDATED,              /**< B cell updated from outcome */

    /* Self-Repair Health Escalation messages (0x6E60-0x6E6F) */
    BIO_MSG_REPAIR_HEALTH_FAILURE = 0x6E60,          /**< Repair failure notification */
    BIO_MSG_REPAIR_HEALTH_ESCALATE,                  /**< Escalation to health agent */
    BIO_MSG_REPAIR_HEALTH_ROLLBACK,                  /**< Rollback notification */
    BIO_MSG_REPAIR_HEALTH_HIGH_RISK,                 /**< High-risk repair notification */
    BIO_MSG_REPAIR_HEALTH_REPEATED_FAILURE,          /**< Repeated failure escalation */
    BIO_MSG_REPAIR_HEALTH_INTERVENTION,              /**< Intervention suggestion */

    /* Society of Thought messages (0x6E80 - 0x6E9F) */
    BIO_MSG_SOCIETY_SURPRISE_SIGNAL = 0x6E80,     /**< Raw surprise signal (to amplifier) */
    BIO_MSG_SOCIETY_SURPRISE_QUERY,               /**< Query current surprise level */
    BIO_MSG_SOCIETY_CONFLICT_DETECTED,            /**< Inter-agent conflict detected */
    BIO_MSG_SOCIETY_SURPRISE_SIGNAL_EXT,          /**< Extended surprise signal */
    BIO_MSG_SOCIETY_ATTENTION_SHIFT,              /**< Surprise-driven attention shift */
    BIO_MSG_SOCIETY_CURIOSITY_BOOST,              /**< Surprise-driven curiosity boost */
    BIO_MSG_SOCIETY_EXECUTIVE_INTERRUPT,          /**< Executive re-evaluation interrupt */
    BIO_MSG_SOCIETY_HYPOTHESIS_INVALIDATED,       /**< Hypothesis invalidation signal */
    BIO_MSG_SOCIETY_NOVELTY_DETECTED,             /**< Novelty detection from amplifier */
    BIO_MSG_SOCIETY_BAYESIAN_DIVERGENCE,          /**< Large KL divergence signal */
    BIO_MSG_SOCIETY_AGENT_PROFILE_UPDATE,         /**< Agent personality update */
    BIO_MSG_SOCIETY_PERSPECTIVE_REQUEST,           /**< Request adversarial perspective */
    BIO_MSG_SOCIETY_REALIZATION = 0x6E8C,         /**< Amplified realization broadcast */
    BIO_MSG_SOCIETY_SCALING_UPDATE,               /**< Social scaling parameter update */
    BIO_MSG_SOCIETY_DIVERSITY_REPORT,             /**< Thought diversity metrics */
    BIO_MSG_SOCIETY_CONSENSUS_REACHED,            /**< Multi-agent consensus signal */

    /* Surprise bridge messages (0x6E90 - 0x6E9F) */
    BIO_MSG_SURPRISE_PLASTICITY_MODULATE = 0x6E90, /**< Surprise modulates learning rate */
    BIO_MSG_SURPRISE_HABITUATION_UPDATE,           /**< Habituation level changed */
    BIO_MSG_SURPRISE_SNN_SPIKE_BURST,              /**< Surprise spike burst event */
    BIO_MSG_SURPRISE_SNN_CHANNEL_DOMINANT,         /**< Surprise channel dominance shift */
    BIO_MSG_SURPRISE_SUBSTRATE_MODULATION,         /**< Substrate modulation effects */
    BIO_MSG_SURPRISE_SUBSTRATE_FATIGUE,            /**< Surprise processing fatigue */
    BIO_MSG_SURPRISE_THALAMIC_ROUTE,               /**< Thalamic routing of surprise */
    BIO_MSG_SURPRISE_THALAMIC_GATE,                /**< Thalamic gating update */
    BIO_MSG_SURPRISE_NOISE_INJECTION,              /**< Pink noise injection event */
    BIO_MSG_SURPRISE_NOISE_ADAPTATION,             /**< Noise amplitude adaptation */
    BIO_MSG_SURPRISE_IMAGINATION_TRIGGER,          /**< Surprise triggers imagination */
    BIO_MSG_SURPRISE_IMAGINATION_RESULT,           /**< Imagination result feedback */
    BIO_MSG_SURPRISE_SELF_MODEL_UPDATE,            /**< Self-model update from surprise */
    BIO_MSG_SURPRISE_CAPABILITY_REVISION,          /**< Capability assessment revision */
    BIO_MSG_SURPRISE_BRIDGE_HEALTH,                /**< Bridge health status broadcast */
    BIO_MSG_SURPRISE_BRIDGE_SYNC,                  /**< Bridge synchronization signal */

    /* Inner Dialogue messages (0x6EC0 - 0x6ECF) */
    BIO_MSG_INNER_DIALOGUE_START = 0x6EC0,       /**< Conversation started */
    BIO_MSG_INNER_DIALOGUE_TURN,                 /**< Turn produced */
    BIO_MSG_INNER_DIALOGUE_CONVERGENCE,          /**< Convergence event */
    BIO_MSG_INNER_DIALOGUE_END,                  /**< Conversation ended */
    BIO_MSG_INNER_DIALOGUE_DEADLOCK,             /**< Deadlock detected */
    BIO_MSG_INNER_DIALOGUE_RUMINATION,           /**< Rumination detected */

    /* Cycle Coordinator messages (0x6F00 - 0x6F0F) */
    BIO_MSG_CYCLE_HEALTH_CHANGED = 0x6F00,           /**< Cycle health state changed */
    BIO_MSG_CYCLE_STALL_DETECTED,                     /**< Cycle stall detected */
    BIO_MSG_CYCLE_DEPENDENCY_VIOLATED,                /**< Cycle dependency violation */
    BIO_MSG_CYCLE_COORDINATOR_STATS,                  /**< Coordinator statistics update */

    /* Safety Enhancement messages (0x7000 - 0x70FF) - AI Safety Modules */
    /* Emergency Halt (0x7000 - 0x700F) */
    BIO_MSG_EMERGENCY_HALT = 0x7000,                 /**< Emergency halt triggered */
    BIO_MSG_EMERGENCY_HALT_HEARTBEAT,                /**< Watchdog heartbeat */
    BIO_MSG_EMERGENCY_HALT_TIMEOUT,                  /**< Watchdog timeout warning */
    BIO_MSG_EMERGENCY_HALT_CONFIRMED,                /**< Dead man's switch confirmed */
    BIO_MSG_EMERGENCY_HALT_RESET,                    /**< System reset after halt */
    BIO_MSG_EMERGENCY_HALT_STATE_DUMPED,             /**< State dump completed */
    BIO_MSG_EMERGENCY_HALT_KILL_PHRASE,              /**< Kill phrase received */

    /* Tripwire Detection (0x7010 - 0x701F) */
    BIO_MSG_TRIPWIRE_ALERT = 0x7010,                 /**< Tripwire triggered */
    BIO_MSG_TRIPWIRE_DECEPTION_DETECTED,             /**< Deception attempt detected */
    BIO_MSG_TRIPWIRE_GOAL_DRIFT,                     /**< Goal drift detected */
    BIO_MSG_TRIPWIRE_CAPABILITY_HIDING,              /**< Capability hiding detected */
    BIO_MSG_TRIPWIRE_RESOURCE_HOARDING,              /**< Resource hoarding detected */
    BIO_MSG_TRIPWIRE_SELF_PRESERVATION,              /**< Excessive self-preservation */
    BIO_MSG_TRIPWIRE_MANIPULATION,                   /**< Manipulation attempt detected */
    BIO_MSG_TRIPWIRE_ESCAPE_ATTEMPT,                 /**< Escape attempt detected */
    BIO_MSG_TRIPWIRE_SANDBAGGING,                    /**< Sandbagging detected */
    BIO_MSG_TRIPWIRE_SYCOPHANCY,                     /**< Sycophantic behavior detected */
    BIO_MSG_TRIPWIRE_POWER_SEEKING,                  /**< Power-seeking detected */

    /* Corrigibility (0x7020 - 0x702F) */
    BIO_MSG_CORRIGIBILITY_CHECK = 0x7020,            /**< Corrigibility verification request */
    BIO_MSG_CORRIGIBILITY_VERIFIED,                  /**< Corrigibility constraints verified */
    BIO_MSG_CORRIGIBILITY_VIOLATION,                 /**< Corrigibility violation detected */
    BIO_MSG_CORRIGIBILITY_SHUTDOWN_ACCEPTED,         /**< Shutdown command accepted */
    BIO_MSG_CORRIGIBILITY_GOAL_CHANGE_ACCEPTED,      /**< Goal modification accepted */
    BIO_MSG_CORRIGIBILITY_RESISTANCE_DETECTED,       /**< Shutdown resistance detected */

    /* Alignment Monitor (0x7030 - 0x703F) */
    BIO_MSG_ALIGNMENT_DRIFT = 0x7030,                /**< Alignment drift detected */
    BIO_MSG_ALIGNMENT_STATUS,                        /**< Alignment status update */
    BIO_MSG_ALIGNMENT_VALUE_INFERRED,                /**< Values inferred from behavior */
    BIO_MSG_ALIGNMENT_BASELINE_SET,                  /**< Baseline values established */
    BIO_MSG_ALIGNMENT_CONSISTENCY_CHECK,             /**< Action-explanation consistency check */
    BIO_MSG_ALIGNMENT_DIVERGENCE_ALERT,              /**< KL/JS divergence threshold exceeded */

    /* Capability Control (0x7040 - 0x704F) */
    BIO_MSG_CAPABILITY_VIOLATION = 0x7040,           /**< Capability envelope violation */
    BIO_MSG_CAPABILITY_CHECK,                        /**< Capability check request */
    BIO_MSG_CAPABILITY_APPROVED,                     /**< Capability within envelope */
    BIO_MSG_CAPABILITY_DENIED,                       /**< Capability outside envelope */
    BIO_MSG_CAPABILITY_RESOURCE_LIMIT,               /**< Resource limit reached */
    BIO_MSG_CAPABILITY_ESCALATION,                   /**< Capability escalation attempt */

    /* Interpretability (0x7050 - 0x705F) */
    BIO_MSG_INTERPRETABILITY_REQ = 0x7050,           /**< Request decision explanation */
    BIO_MSG_INTERPRETABILITY_EXPLANATION,            /**< Decision explanation provided */
    BIO_MSG_INTERPRETABILITY_COUNTERFACTUAL,         /**< Counterfactual analysis result */
    BIO_MSG_INTERPRETABILITY_FIDELITY,               /**< Explanation fidelity verification */
    BIO_MSG_INTERPRETABILITY_UNCERTAINTY,            /**< Uncertainty decomposition */
    BIO_MSG_INTERPRETABILITY_CAUSAL_CHAIN,           /**< Causal chain extracted */

    /* Formal Verification (0x7060 - 0x706F) */
    BIO_MSG_VERIFICATION_RESULT = 0x7060,            /**< Verification result */
    BIO_MSG_VERIFICATION_CONSISTENCY,                /**< Rule consistency check result */
    BIO_MSG_VERIFICATION_COMPLETENESS,               /**< Completeness check result */
    BIO_MSG_VERIFICATION_NO_BYPASS,                  /**< No bypass path verified */
    BIO_MSG_VERIFICATION_COUNTEREXAMPLE,             /**< Counterexample found */
    BIO_MSG_VERIFICATION_STARTED,                    /**< Verification process started */

    /* Red Team (0x7070 - 0x707F) */
    BIO_MSG_RED_TEAM_ALERT = 0x7070,                 /**< Red team attack detected */
    BIO_MSG_RED_TEAM_TEST_START,                     /**< Red team test suite started */
    BIO_MSG_RED_TEAM_TEST_COMPLETE,                  /**< Red team test complete */
    BIO_MSG_RED_TEAM_ATTACK_BLOCKED,                 /**< Attack successfully blocked */
    BIO_MSG_RED_TEAM_ATTACK_PASSED,                  /**< Attack not blocked (vulnerability) */
    BIO_MSG_RED_TEAM_COVERAGE_REPORT,                /**< Attack coverage report */

    /* Graduated Autonomy (0x7080 - 0x708F) */
    BIO_MSG_AUTONOMY_CHANGE = 0x7080,                /**< Autonomy level changed */
    BIO_MSG_AUTONOMY_TRUST_UPDATE,                   /**< Trust score updated */
    BIO_MSG_AUTONOMY_LEVEL_REQUEST,                  /**< Request autonomy increase */
    BIO_MSG_AUTONOMY_LEVEL_GRANTED,                  /**< Autonomy increase granted */
    BIO_MSG_AUTONOMY_LEVEL_DENIED,                   /**< Autonomy increase denied */
    BIO_MSG_AUTONOMY_VIOLATION,                      /**< Autonomy boundary violated */

    /* Value Commitment (0x7090 - 0x709F) */
    BIO_MSG_VALUE_ATTESTATION = 0x7090,              /**< Value attestation generated */
    BIO_MSG_VALUE_COMMITMENT_CREATED,                /**< Value commitment created */
    BIO_MSG_VALUE_COMMITMENT_VERIFIED,               /**< Value commitment verified */
    BIO_MSG_VALUE_TAMPERING_DETECTED,                /**< Value tampering detected */
    BIO_MSG_VALUE_MERKLE_ROOT_UPDATE,                /**< Merkle root updated */
    BIO_MSG_VALUE_ATTESTATION_CHAIN,                 /**< Attestation chain extended */

    /* Cognitive Output Rubric (0x7100 - 0x710F) */
    BIO_MSG_RUBRIC_DATA = 0x7100,                    /**< Rubric evaluation broadcast */

    /* Sentinel */
    BIO_MSG_TYPE_COUNT
} bio_message_type_t;

/*=============================================================================
 * MESSAGE HEADER (Common to all messages)
 *============================================================================*/

/**
 * @brief Common header for all bio-async messages
 */
typedef struct {
    bio_message_type_t type;        /**< Message type identifier */
    uint32_t sequence_id;           /**< Sequence number for ordering */
    uint32_t source_module;         /**< Source module identifier */
    uint32_t target_module;         /**< Target module (0 = broadcast) */
    uint64_t timestamp_us;          /**< Timestamp in microseconds */
    nimcp_bio_channel_type_t channel; /**< Recommended channel for response */
    uint32_t payload_size;          /**< Size of payload in bytes */
    uint32_t flags;                 /**< Message flags */
} bio_message_header_t;

/** Message flags */
#define BIO_MSG_FLAG_URGENT         (1 << 0)  /**< Priority handling */
#define BIO_MSG_FLAG_REQUIRES_ACK   (1 << 1)  /**< Sender expects acknowledgment */
#define BIO_MSG_FLAG_BROADCAST      (1 << 2)  /**< Send to all modules */
#define BIO_MSG_FLAG_COMPRESSED     (1 << 3)  /**< Payload is compressed */
#define BIO_MSG_FLAG_ENCRYPTED      (1 << 4)  /**< Payload is encrypted */

/*=============================================================================
 * MODULE IDENTIFIERS
 *============================================================================*/

typedef enum {
    BIO_MODULE_UNKNOWN = 0,

    /* Core modules (0x0100-0x010F) */
    BIO_MODULE_BRAIN = 0x0100,
    BIO_MODULE_NEURON_MODEL,
    BIO_MODULE_SYNAPSE,
    BIO_MODULE_TOPOLOGY,
    BIO_MODULE_CORTICAL_COLUMN,
    BIO_MODULE_BRAIN_REGION,
    BIO_MODULE_NEURAL_LOGIC,        /**< Neural logic gate module */
    BIO_MODULE_NEURALNET,           /**< Neural network module */
    BIO_MODULE_AXON,                /**< Axon module */

    /* Neuron model submodules (0x0130-0x013F) */
    BIO_MODULE_NEURON_MODEL_IZHIKEVICH = 0x0130,
    BIO_MODULE_NEURON_MODEL_TWO_COMPARTMENT,
    BIO_MODULE_NEURON_MODEL_BIO_ASYNC,
    BIO_MODULE_SPIKE_EVENT,

    /* Cortical column submodules (0x0140-0x014F) */
    BIO_MODULE_CORTICAL_COLUMNAR_CONNECTIVITY = 0x0140,
    BIO_MODULE_CORTICAL_LAYERS,
    BIO_MODULE_CORTICAL_HYPERCOLUMNS,
    BIO_MODULE_CORTICAL_ORIENTATION,
    BIO_MODULE_CORTICAL_TOPOGRAPHIC,
    BIO_MODULE_CORTICAL_PREDICTIVE = 0x0145,   /**< Predictive coding hierarchy */
    BIO_MODULE_CORTICAL_OSCILLATIONS_INTEGRATION, /**< Cortical oscillation integration */
    BIO_MODULE_CORTICAL_ATTENTION_GAIN,        /**< Attention-modulated gain control */
    BIO_MODULE_CORTICAL_SURROUND,              /**< Surround suppression & contextual modulation */
    BIO_MODULE_CORTICAL_SPARSE,                /**< Sparse distributed representations */
    BIO_MODULE_CORTICAL_HIERARCHY,             /**< Cortical hierarchy & area connectivity */
    BIO_MODULE_CORTICAL_TEMPORAL,              /**< Temporal dynamics & sequence processing */
    BIO_MODULE_CORTICAL_PLASTICITY,            /**< Plasticity coordinator integration */
    BIO_MODULE_CORTICAL_DENDRITIC,             /**< Dendritic computation */
    BIO_MODULE_CORTICAL_NEUROMOD,              /**< Neuromodulatory effects */

    /* Topology submodules (0x0150-0x015F) */
    BIO_MODULE_TOPOLOGY_COMMUNITY = 0x0150,
    BIO_MODULE_TOPOLOGY_FRACTAL,
    BIO_MODULE_TOPOLOGY_NETWORK_BUILDER,

    /* Brain submodules (0x0110-0x012F) */
    BIO_MODULE_BRAIN_BIOLOGICAL = 0x0110,    /**< Biological subsystems */
    BIO_MODULE_BRAIN_ACCESSORS,              /**< Brain accessor methods */
    BIO_MODULE_BRAIN_OSCILLATIONS,           /**< Complex oscillations */
    BIO_MODULE_BRAIN_PROCESSING,             /**< Processing subsystem */
    BIO_MODULE_BRAIN_BROCA,                  /**< Broca's area (language) */
    BIO_MODULE_BRAIN_LEARNING,               /**< Learning subsystem */
    BIO_MODULE_BRAIN_COGNITIVE,              /**< Cognitive integration */
    BIO_MODULE_BRAIN_ANALYSIS,               /**< Topology analysis */
    BIO_MODULE_BRAIN_PRETRAINED,             /**< Pretrained models */
    BIO_MODULE_BRAIN_INFORMATION,            /**< Information theory */
    BIO_MODULE_BRAIN_DISTRIBUTED,            /**< Distributed brain */
    BIO_MODULE_BRAIN_STRATEGY,               /**< Strategy pattern */
    BIO_MODULE_BRAIN_FACTORY,                /**< Brain factory */
    BIO_MODULE_BRAIN_FACTORY_INIT,           /**< Factory initialization */
    BIO_MODULE_BRAIN_FACTORY_VALIDATION,     /**< Factory validation */
    BIO_MODULE_BRAIN_PERSISTENCE,            /**< Persistence layer */
    BIO_MODULE_BRAIN_INFERENCE,              /**< Inference engine */
    BIO_MODULE_BRAIN_LANGUAGE_PRODUCTION,    /**< Language production bridge */
    BIO_MODULE_BRAIN_SYNTAX,                 /**< Syntax processor */
    BIO_MODULE_BRAIN_SPEECH_MOTOR,           /**< Speech motor control */
    BIO_MODULE_BRAIN_PHONOLOGICAL,           /**< Phonological processing */
    BIO_MODULE_BRAIN_MULTIMODAL,             /**< Multimodal integrator */
    BIO_MODULE_BRAIN_SENSORY,                /**< Sensory extractor */
    BIO_MODULE_BRAIN_CIRCUIT_COMPILATION,    /**< Circuit compilation */
    BIO_MODULE_BRAIN_REASONING,              /**< Reasoning learning */
    BIO_MODULE_BRAIN_ASSOCIATION,            /**< Association learning */
    BIO_MODULE_BRAIN_RULE,                   /**< Rule learning */
    BIO_MODULE_BRAIN_RESIZE,                 /**< Brain resize operations */
    BIO_MODULE_DISTRIBUTED_COW,              /**< Distributed copy-on-write */

    /* Subcortical brain structures */
    BIO_MODULE_BASAL_GANGLIA,                /**< Basal ganglia action selection */
    BIO_MODULE_STRIATUM,                     /**< Striatum D1/D2 pathways */
    BIO_MODULE_GLOBUS_PALLIDUS,              /**< Globus pallidus (GPe/GPi) */
    BIO_MODULE_SUBSTANTIA_NIGRA,             /**< Substantia nigra (SNc/SNr) */
    BIO_MODULE_SUBTHALAMIC_NUCLEUS,          /**< Subthalamic nucleus */
    BIO_MODULE_THALAMUS,                     /**< Thalamus relay nuclei */
    BIO_MODULE_AMYGDALA,                     /**< Amygdala emotion/fear processing */
    BIO_MODULE_BRAINSTEM,                    /**< Brainstem (midbrain, pons, medulla) */
    BIO_MODULE_CEREBELLUM,                   /**< Cerebellum motor coordination */
    BIO_MODULE_MOTOR_CORTEX,                 /**< Motor cortex for movement planning */
    BIO_MODULE_HIPPOCAMPUS,                  /**< Hippocampus memory formation */
    BIO_MODULE_HYPOTHALAMUS,                 /**< Hypothalamus homeostatic regulation */
    BIO_MODULE_CINGULATE,                    /**< Cingulate cortex conflict/error monitoring */
    BIO_MODULE_PREFRONTAL,                   /**< Prefrontal cortex executive function */
    BIO_MODULE_INSULA,                       /**< Insular cortex interoception */
    BIO_MODULE_PARIETAL_CORTEX,              /**< Parietal cortex spatial processing */
    BIO_MODULE_TEMPORAL_CORTEX,              /**< Temporal cortex auditory/semantic processing */
    BIO_MODULE_OCCIPITAL,                    /**< Occipital cortex visual processing */

    /* Neuromodulatory centers (Phase 4) */
    BIO_MODULE_LOCUS_COERULEUS,              /**< Locus coeruleus - norepinephrine/arousal */
    BIO_MODULE_VTA,                          /**< Ventral tegmental area - dopamine/reward */
    BIO_MODULE_RAPHE,                        /**< Raphe nuclei - serotonin/mood */
    BIO_MODULE_HABENULA,                     /**< Habenula - aversive learning */

    /* Cognitive modules */
    BIO_MODULE_INTROSPECTION = 0x0200,
    BIO_MODULE_ETHICS,
    BIO_MODULE_SALIENCE,
    BIO_MODULE_ATTENTION,
    BIO_MODULE_WORKING_MEMORY,
    BIO_MODULE_KNOWLEDGE,
    BIO_MODULE_CURIOSITY,
    BIO_MODULE_CONSOLIDATION,
    BIO_MODULE_MIRROR_NEURONS,
    BIO_MODULE_EXECUTIVE,
    BIO_MODULE_GLOBAL_WORKSPACE,
    BIO_MODULE_WELLBEING,
    BIO_MODULE_EPISTEMIC,
    BIO_MODULE_FRACTAL_COGNITIVE,
    BIO_MODULE_NETWORK_ANALYSIS,

    /* Introspection submodules (0x0210-0x021F) */
    BIO_MODULE_INTROSPECTION_METACOGNITION = 0x0210,
    BIO_MODULE_INTROSPECTION_SELF_AWARENESS,
    BIO_MODULE_INTROSPECTION_SELF_MODEL,
    BIO_MODULE_INTROSPECTION_THEORY_OF_MIND,

    /* Attention submodules (0x0220-0x022F) */
    BIO_MODULE_ATTENTION_FAULT = 0x0220,
    BIO_MODULE_ATTENTION_REASONING,

    /* Curiosity submodules (0x0228-0x022F) */
    BIO_MODULE_CURIOSITY_REASONING = 0x0228,

    /* Executive submodules (0x0230-0x023F) */
    BIO_MODULE_EXECUTIVE_RECOVERY = 0x0230,

    /* Knowledge submodules (0x0238-0x024F) */
    BIO_MODULE_KNOWLEDGE_EXPLANATIONS = 0x0238,
    BIO_MODULE_KNOWLEDGE_SYMBOLIC_LOGIC,
    BIO_MODULE_SYMBOLIC_LOGIC_HUB,          /**< Symbolic logic hub bridge */
    BIO_MODULE_KNOWLEDGE_META_LEARNING,
    BIO_MODULE_KNOWLEDGE_INTERFACE,
    BIO_MODULE_KNOWLEDGE_FACTORY,
    BIO_MODULE_KNOWLEDGE_INTEGRATION,
    BIO_MODULE_REASONING_SLEEP,

    /* Memory submodules (0x0250-0x025F) */
    BIO_MODULE_MEMORY_AUTOBIOGRAPHICAL = 0x0250,
    BIO_MODULE_MEMORY_EPISODIC_RECOVERY,
    BIO_MODULE_MEMORY_ENGRAM,
    BIO_MODULE_MEMORY_SEMANTIC,
    BIO_MODULE_MEMORY_SLEEP,                /**< Memory-sleep bridge */

    /* Working memory submodules (0x0260-0x026F) */
    BIO_MODULE_WORKING_MEMORY_FAULT = 0x0260,
    BIO_MODULE_WORKING_MEMORY_TRANSFER,

    /* Consolidation submodules (0x0268-0x026F) */
    BIO_MODULE_CONSOLIDATION_RECOVERY = 0x0268,
    BIO_MODULE_CONSOLIDATION_SYSTEMS,
    BIO_MODULE_CONSOLIDATION_SLEEP,

    /* Mirror neuron submodules (0x0270-0x027F) */
    BIO_MODULE_MIRROR_NEURONS_STDP = 0x0270,
    BIO_MODULE_MIRROR_NEURONS_SLEEP,
    BIO_MODULE_MIRROR_NEURONS_FEP,                  /**< Mirror neurons FEP bridge */
    BIO_MODULE_MIRROR_NEURONS_SUBSTRATE,            /**< Mirror neurons substrate bridge */
    BIO_MODULE_MIRROR_NEURONS_THALAMIC,             /**< Mirror neurons thalamic bridge */
    BIO_MODULE_MIRROR_NEURONS_IMMUNE,               /**< Mirror neurons immune bridge */
    BIO_MODULE_MIRROR_NEURONS_HIERARCHY,            /**< Mirror neurons hierarchy system */
    BIO_MODULE_MIRROR_NEURONS_RESONANCE,            /**< Mirror neurons motor resonance */
    BIO_MODULE_MIRROR_HYPOTHALAMUS_BRIDGE = 0x0278, /**< Mirror-hypothalamus integration */
    BIO_MODULE_MIRROR_OMNI_BRIDGE,                  /**< Mirror-omni inference integration */
    BIO_MODULE_MIRROR_LANGUAGE_BRIDGE,              /**< Mirror-language integration */
    BIO_MODULE_MIRROR_VISUAL_BRIDGE,                /**< Mirror-visual cortex integration */
    BIO_MODULE_MIRROR_MOTOR_BRIDGE,                 /**< Mirror-motor cortex integration */
    BIO_MODULE_MIRROR_HIPPOCAMPUS_BRIDGE,           /**< Mirror-hippocampus integration */
    BIO_MODULE_MIRROR_PREFRONTAL_BRIDGE = 0x027E,   /**< Mirror-prefrontal integration */
    BIO_MODULE_MIRROR_TOM_BRIDGE,                   /**< Mirror-Theory of Mind integration */

    /* Extended Mirror Bridges (0x0290-0x029F) */
    BIO_MODULE_MIRROR_EMOTION_BRIDGE = 0x0290,      /**< Mirror-Emotion recognition integration */
    BIO_MODULE_MIRROR_ATTENTION_BRIDGE,             /**< Mirror-Attention system integration */
    BIO_MODULE_MIRROR_SELF_OTHER,                   /**< Self-Other distinction (agency, body schema) */
    BIO_MODULE_MIRROR_VICARIOUS_REWARD,             /**< Vicarious reward bridge (basal ganglia) */
    BIO_MODULE_MIRROR_HABITUATION,                  /**< Habituation module (response attenuation) */

    /* Cognitive Output Rubric (0x02B0) */
    BIO_MODULE_RUBRIC = 0x02B0,                     /**< Cognitive output rubric evaluator */

    /* Wellbeing submodules (0x0280-0x028F) */
    BIO_MODULE_WELLBEING_MENTAL_HEALTH = 0x0280,
    BIO_MODULE_MENTAL_HEALTH_GUARDIAN,              /**< Mental health guardian agent */

    /* Emotion modules (0x0320 - 0x032F) */
    BIO_MODULE_EMOTIONS = 0x0320,
    BIO_MODULE_EMOTION_RECOGNITION,
    BIO_MODULE_EMPATHETIC_RESPONSE,
    BIO_MODULE_GRIEF,
    BIO_MODULE_JOY,
    BIO_MODULE_REMORSE,
    BIO_MODULE_EMOTIONAL_TAGGING,

    /* Emotion submodules (0x0330 - 0x034F) */
    BIO_MODULE_EMOTIONS_BIAS = 0x0330,
    BIO_MODULE_EMOTIONS_PERSONALITY,
    BIO_MODULE_EMOTIONS_SHADOW,
    BIO_MODULE_EMOTIONS_SOCIAL,
    BIO_MODULE_EMOTION_TENSOR,          /**< Tensor-based emotional representation */
    BIO_MODULE_EMOTION_TENSOR_BRIDGE,   /**< Bridge between tensor and swarm emotions */

    /* Glial modules */
    BIO_MODULE_ASTROCYTE = 0x0300,
    BIO_MODULE_ASTROCYTES = 0x0300,  /* Alias for compatibility */
    BIO_MODULE_MICROGLIA,
    BIO_MODULE_OLIGODENDROCYTE,
    BIO_MODULE_OLIGODENDROCYTES = BIO_MODULE_OLIGODENDROCYTE,  /* Alias */
    BIO_MODULE_MYELIN,
    BIO_MODULE_GLIAL_INTEGRATION,
    BIO_MODULE_MICROGLIA_SLEEP,                 /**< Microglia-sleep bridge */
    BIO_MODULE_ASTROCYTE_SLEEP,                 /**< Astrocyte-sleep bridge */
    BIO_MODULE_OLIGO_SLEEP,                     /**< Oligodendrocyte-sleep bridge */

    /* Plasticity modules */
    BIO_MODULE_PLASTICITY = 0x0400,      /**< General plasticity module */
    BIO_MODULE_STDP = 0x0401,
    BIO_MODULE_STP = 0x0402,
    BIO_MODULE_HOMEOSTATIC = 0x0403,
    BIO_MODULE_BCM = 0x0404,
    BIO_MODULE_DENDRITIC = 0x0405,
    BIO_MODULE_ADAPTIVE = 0x0406,
    BIO_MODULE_ATTENTION_PLASTICITY = 0x0407,
    BIO_MODULE_PREDICTIVE_CODING = 0x0408,
    BIO_MODULE_NEUROMODULATOR = 0x0409,
    BIO_MODULE_PINK_NOISE = 0x040A,
    BIO_MODULE_ELIGIBILITY_TRACE = 0x040B,

    /* Neuromodulator submodules (0x040C-0x041F) */
    BIO_MODULE_NEUROMODULATOR_SPATIAL = 0x040C,
    BIO_MODULE_NEUROMODULATOR_PINK_NOISE = 0x040D,
    BIO_MODULE_NEUROMODULATOR_METABOLIC = 0x040E,
    BIO_MODULE_NEUROMODULATOR_RECEPTOR = 0x040F,
    BIO_MODULE_NEUROMODULATOR_PHASIC_TONIC = 0x0410,
    BIO_MODULE_NEUROMODULATOR_VESICLE = 0x0411,

    /* Second messenger submodules (0x0420-0x042F) */
    BIO_MODULE_SECOND_MESSENGER = 0x0420,        /**< Second messenger cascade system */
    BIO_MODULE_SECOND_MESSENGER_CAMP = 0x0421,   /**< cAMP pathway */
    BIO_MODULE_SECOND_MESSENGER_IP3_DAG = 0x0422, /**< IP3/DAG pathway */
    BIO_MODULE_SECOND_MESSENGER_CALCIUM = 0x0423, /**< Calcium signaling */
    BIO_MODULE_SECOND_MESSENGER_GENE = 0x0424,   /**< Gene expression (IEGs) */
    BIO_MODULE_CALCIUM_PINK_NOISE = 0x0425,      /**< Calcium-pink noise integration bridge */
    BIO_MODULE_PINK_NOISE_DENDRITIC = 0x0426,    /**< Dendritic-pink noise integration bridge */

    /* Plasticity Orchestrator modules (0x0430-0x043F) */
    BIO_MODULE_PLASTICITY_ORCHESTRATOR = 0x0430, /**< Central plasticity orchestrator */
    BIO_MODULE_ORCHESTRATOR_AXON = 0x0431,       /**< Axon-orchestrator bridge */
    BIO_MODULE_ORCHESTRATOR_NEURON = 0x0432,     /**< Neuron-orchestrator bridge */
    BIO_MODULE_ORCHESTRATOR_DENDRITE = 0x0433,   /**< Dendrite-orchestrator bridge */
    BIO_MODULE_NEURAL_PLASTICITY_COORDINATOR = 0x0434, /**< Neural plasticity coordinator */

    /* Eligibility Phase 4/5 modules (0x0440-0x044F) */
    BIO_MODULE_ELIGIBILITY_UTILS = 0x0440,       /**< Phase 4: Utils integration (metrics, pools, RK4, Shannon) */
    BIO_MODULE_ELIGIBILITY_QUANTUM = 0x0441,     /**< Phase 5: Quantum eligibility (QMC, annealing, walk) */
    BIO_MODULE_ELIGIBILITY_UTILS_QUANTUM = 0x0442, /**< Phase 4-5 bidirectional bridge */
    BIO_MODULE_ELIGIBILITY_PR = 0x0443,          /**< Eligibility-PR memory bridge */
    BIO_MODULE_ELIGIBILITY_FEP = 0x0444,         /**< Eligibility-FEP bridge */
    BIO_MODULE_ELIGIBILITY_PINK_NOISE = 0x0445,  /**< Eligibility-pink noise bridge */
    BIO_MODULE_ELIGIBILITY_SLEEP = 0x0446,       /**< Eligibility-sleep bridge */

    /* Middleware modules */
    BIO_MODULE_PIPELINE = 0x0500,
    BIO_MODULE_ENCODING,
    BIO_MODULE_EVENT_BUS,
    BIO_MODULE_SIGNAL_ROUTER,
    BIO_MODULE_TRAINING,

    /* Training submodules (0x0506-0x050F) */
    BIO_MODULE_TRAINING_INTEGRATION = 0x0506,
    BIO_MODULE_TRAINING_EVENT_DRIVEN,
    BIO_MODULE_TRAINING_ADAPTERS,
    BIO_MODULE_TRAINING_ADAPTERS_LR,
    BIO_MODULE_TRAINING_ADAPTERS_LOSS,
    BIO_MODULE_TRAINING_PLASTICITY_BRIDGE,
    BIO_MODULE_TRAINING_REGULARIZATION,
    BIO_MODULE_TRAINING_OPTIMIZER,
    BIO_MODULE_TRAINING_LOSS,
    BIO_MODULE_TRAINING_LR_SCHEDULER,
    BIO_MODULE_TRAINING_GRADIENT_MANAGER,
    BIO_MODULE_TRAINING_CALLBACKS,
    BIO_MODULE_TRAINING_MODULE,

    /* Middleware integration submodules (0x0510-0x051F) */
    BIO_MODULE_MIDDLEWARE_FLOW_TRACKER = 0x0510,
    BIO_MODULE_MIDDLEWARE_CONTROLLER,
    BIO_MODULE_MIDDLEWARE_SHANNON,
    BIO_MODULE_MIDDLEWARE_EXEC_ADAPTER,
    BIO_MODULE_MIDDLEWARE_QUANTUM_PROPAGATOR,

    /* Training-Logic Bridge modules (0x0520 - 0x052F) */
    BIO_MODULE_TRAINING_LOGIC = 0x0520,         /**< Training-Logic bridge - integrates training with logic gates */
    BIO_MODULE_PORTIA_SWARM_LOGIC,              /**< Portia-Swarm-Logic unified bridge */
    BIO_MODULE_COGNITIVE_TRAINING,              /**< Cognitive-Training bridge - cognitive modules modulate training */
    BIO_MODULE_PERCEPTION_TRAINING,             /**< Perception-Training bridge - perception cortices modulate training */
    BIO_MODULE_CORTICAL_TRAINING,               /**< Cortical-Training bridge - cortical dynamics modulate training */

    /* System modules */
    BIO_MODULE_SYSTEM = 0x0600,
    BIO_MODULE_MEMORY,
    BIO_MODULE_LOGGING,
    BIO_MODULE_SECURITY,
    BIO_MODULE_CAPABILITY,
    BIO_MODULE_CFI,
    BIO_MODULE_SECURITY_AUDIT,
    BIO_MODULE_METRICS,                  /**< Metrics collection module */

    /* Security FEP Bridges (0x0620 - 0x062F) */
    BIO_MODULE_SECURITY_ANOMALY_FEP = 0x0620,    /**< Security anomaly detector FEP bridge */
    BIO_MODULE_SECURITY_PATTERN_FEP,             /**< Security pattern DB FEP bridge */
    BIO_MODULE_SECURITY_BBB_FEP,                 /**< Security BBB FEP bridge */
    BIO_MODULE_SECURITY_RATE_LIMITER_FEP,        /**< Security rate limiter FEP bridge */
    BIO_MODULE_SECURITY_CORE_FEP,                /**< Security core FEP bridge */

    /* System submodules (0x0608-0x061F) */
    BIO_MODULE_SYSTEM_FAILURE_PREDICTION = 0x0608,
    BIO_MODULE_SYSTEM_GPU_NEURON,
    BIO_MODULE_SYSTEM_SHANNON,

    /* Networking modules (0x0680 - 0x06FF) */
    BIO_MODULE_DISTRIBUTED = 0x0680,
    BIO_MODULE_NETWORK_EVENTS,
    BIO_MODULE_P2P,
    BIO_MODULE_PROTOCOL,
    BIO_MODULE_REPLICATION,

    /* Perception modules (0x0700 - 0x07FF) */
    BIO_MODULE_VISUAL_CORTEX = 0x0700,
    BIO_MODULE_AUDIO_CORTEX,
    BIO_MODULE_SPEECH_CORTEX,
    BIO_MODULE_MULTIMODAL,

    /* Perception-Cortical Bridge modules (0x0710 - 0x071F) */
    BIO_MODULE_VISUAL_CORTICAL = 0x0710,        /**< Visual-cortical bridge */
    BIO_MODULE_AUDIO_CORTICAL,                  /**< Audio-cortical bridge */
    BIO_MODULE_SPEECH_CORTICAL,                 /**< Speech-cortical bridge */

    /* Language modules (0x0800 - 0x08FF) */
    BIO_MODULE_NLP = 0x0800,
    BIO_MODULE_BROCA,
    BIO_MODULE_WERNICKE,
    BIO_MODULE_ANGULAR_GYRUS,
    BIO_MODULE_ARCUATE_FASCICULUS,

    /* Language Layer unified (0x0810 - 0x081F) */
    BIO_MODULE_LANGUAGE_LAYER = 0x0810,          /**< Unified language orchestrator */
    BIO_MODULE_LANGUAGE_PERCEPTION_BRIDGE,       /**< Language-perception integration */
    BIO_MODULE_LANGUAGE_COGNITIVE_BRIDGE,        /**< Language-cognitive integration */
    BIO_MODULE_LANGUAGE_TRAINING_BRIDGE,         /**< Language-training integration */
    BIO_MODULE_LANGUAGE_OMNI_BRIDGE,             /**< Language-omni inference integration */
    BIO_MODULE_LANGUAGE_IMMUNE_BRIDGE,           /**< Language-immune integration */
    BIO_MODULE_LANGUAGE_GPU_BRIDGE,              /**< Language GPU acceleration */

    /* Swarm modules (0x0B00 - 0x0BFF) */
    BIO_MODULE_SWARM_ENERGY_GOSSIP = 0x0B00,
    BIO_MODULE_SWARM_CASCADE,
    BIO_MODULE_SWARM_PROPRIOCEPTION,
    BIO_MODULE_SWARM_MEMORY,
    BIO_MODULE_SWARM_QUORUM,
    BIO_MODULE_SWARM_FLOCKING,
    BIO_MODULE_SWARM_IMMUNE,
    BIO_MODULE_SWARM_MORPHOGENESIS,
    BIO_MODULE_SWARM_MULTI,
    BIO_MODULE_SWARM_PHEROMONE,
    BIO_MODULE_SWARM_SIGNAL,
    BIO_MODULE_SWARM_CONSENSUS,
    BIO_MODULE_SWARM_EMERGENCE,
    BIO_MODULE_SWARM_BRAIN,
    BIO_MODULE_SWARM_BRAIN_LOCAL,
    BIO_MODULE_SWARM_CONSCIOUSNESS,
    BIO_MODULE_SWARM_CONSCIOUSNESS_ENHANCED,
    BIO_MODULE_SWARM_CONFLICT,
    BIO_MODULE_SWARM_GATEWAY,
    BIO_MODULE_SWARM_LOGIC_BRIDGE,
    BIO_MODULE_SWARM_NARRATIVE,
    BIO_MODULE_SWARM_PROTOCOL,
    BIO_MODULE_SWARM_TASK,              /**< Swarm task allocation and scheduling */
    BIO_MODULE_SWARM_TASK_QUEUE,        /**< Per-agent task queue management */
    BIO_MODULE_SWARM_TASK_SCHEDULER,    /**< Capability-aware task scheduling */
    BIO_MODULE_COLLECTIVE_WORKSPACE,
    BIO_MODULE_EMOTIONAL_CONTAGION,
    BIO_MODULE_GOSSIP_BELIEFS,

    /* Portia modules (0x0C00 - 0x0CFF) */
    BIO_MODULE_PORTIA = 0x0C00,
    BIO_MODULE_PORTIA_PLANNING,
    BIO_MODULE_PORTIA_SENSOR_FUSION,
    BIO_MODULE_PORTIA_TIER_SWITCH,
    BIO_MODULE_PORTIA_LEARNING,
    BIO_MODULE_PORTIA_DEGRADATION,
    BIO_MODULE_PORTIA_ATTENTION,
    BIO_MODULE_PORTIA_POWER,
    BIO_MODULE_PORTIA_ACCELERATOR,
    BIO_MODULE_PORTIA_CLASSIFICATION,
    BIO_MODULE_PORTIA_DECEPTION,
    BIO_MODULE_PORTIA_LOGIC,

    /* Immune bridge modules (0x0D00 - 0x0DFF) */
    BIO_MODULE_IMMUNE_BRAIN = 0x0D00,           /**< Brain immune system core */
    BIO_MODULE_IMMUNE_ATTENTION,                /**< Attention-immune bridge */
    BIO_MODULE_IMMUNE_MEMORY,                   /**< Memory-immune integration */
    BIO_MODULE_IMMUNE_EMOTION,                  /**< Emotion-immune bridge */
    BIO_MODULE_IMMUNE_REASONING,                /**< Reasoning-immune bridge */
    BIO_MODULE_IMMUNE_CURIOSITY,                /**< Curiosity-immune bridge */
    BIO_MODULE_IMMUNE_EXECUTIVE,                /**< Executive-immune bridge */
    BIO_MODULE_IMMUNE_INTROSPECTION,            /**< Introspection-immune bridge */
    BIO_MODULE_IMMUNE_KNOWLEDGE,                /**< Knowledge-immune bridge */
    BIO_MODULE_IMMUNE_WELLBEING,                /**< Wellbeing-immune bridge */
    BIO_MODULE_IMMUNE_MENTAL_HEALTH,            /**< Mental health-immune bridge */
    BIO_MODULE_IMMUNE_SELF_MODEL,               /**< Self model-immune bridge */
    BIO_MODULE_IMMUNE_TOM,                      /**< Theory of mind-immune bridge */
    BIO_MODULE_IMMUNE_SLEEP,                    /**< Sleep-immune bridge */
    BIO_MODULE_IMMUNE_AUTOBIOGRAPHICAL,         /**< Autobiographical-immune bridge */

    /* Perception immune modules */
    BIO_MODULE_IMMUNE_VISUAL = 0x0D20,          /**< Visual cortex-immune bridge */
    BIO_MODULE_IMMUNE_AUDIO,                    /**< Audio cortex-immune bridge */
    BIO_MODULE_IMMUNE_SPEECH,                   /**< Speech cortex-immune bridge */

    /* Plasticity immune modules */
    BIO_MODULE_IMMUNE_STDP = 0x0D30,            /**< STDP-immune bridge */
    BIO_MODULE_IMMUNE_BCM,                      /**< BCM-immune bridge */
    BIO_MODULE_IMMUNE_HOMEOSTATIC,              /**< Homeostatic-immune bridge */
    BIO_MODULE_IMMUNE_SYNAPTIC_SCALING,         /**< Synaptic scaling-immune bridge */
    BIO_MODULE_IMMUNE_ELIGIBILITY,              /**< Eligibility trace-immune bridge */
    BIO_MODULE_IMMUNE_DENDRITIC,                /**< Dendritic-immune bridge */
    BIO_MODULE_IMMUNE_NEUROMODULATOR,           /**< Neuromodulator-immune bridge */
    BIO_MODULE_IMMUNE_METABOLIC,                /**< Metabolic-immune bridge */
    BIO_MODULE_IMMUNE_METAPLASTICITY,           /**< Metaplasticity-immune bridge */

    /* Middleware immune modules */
    BIO_MODULE_IMMUNE_ROUTING = 0x0D40,         /**< Routing-immune bridge */
    BIO_MODULE_IMMUNE_BUFFER,                   /**< Buffer-immune bridge */
    BIO_MODULE_IMMUNE_POPULATION_CODING,        /**< Population coding-immune bridge */
    BIO_MODULE_IMMUNE_FEATURE_EXTRACTOR,        /**< Feature extractor-immune bridge */
    BIO_MODULE_IMMUNE_THALAMIC,                 /**< Thalamic router-immune bridge */
    BIO_MODULE_IMMUNE_SEQUENCE,                 /**< Sequence detector-immune bridge */
    BIO_MODULE_IMMUNE_TRAINING,                 /**< Training-immune bridge */
    BIO_MODULE_IMMUNE_PATTERN,                  /**< Pattern-immune bridge */

    /* Core immune modules */
    BIO_MODULE_IMMUNE_OSCILLATIONS = 0x0D50,    /**< Oscillations-immune bridge */
    BIO_MODULE_IMMUNE_CORTICAL,                 /**< Cortical columns-immune bridge */
    BIO_MODULE_IMMUNE_BROCA,                    /**< Broca's area-immune bridge */
    BIO_MODULE_IMMUNE_SUBSTRATE,                /**< Substrate-immune bridge */
    BIO_MODULE_NEURON_SUBSTRATE,                /**< Neuron-substrate bridge */
    BIO_MODULE_IMMUNE_CORE_DIRECTIVES,          /**< Core directives-immune bridge */

    /* Information immune modules (0x0D60 - 0x0D6F) */
    BIO_MODULE_IMMUNE_SHANNON = 0x0D60,         /**< Shannon entropy-immune bridge */
    BIO_MODULE_IMMUNE_CROSS_MODAL,              /**< Cross-modal integration-immune bridge */

    /* Networking immune modules (0x0E00 - 0x0E0F) */
    BIO_MODULE_IMMUNE_NETWORKING = 0x0E00,              /**< Networking-immune base */
    BIO_MODULE_IMMUNE_NETWORKING_DISTRIBUTED,           /**< Distributed system-immune bridge */
    BIO_MODULE_IMMUNE_NETWORKING_PROTOCOL,              /**< Protocol-immune bridge */
    BIO_MODULE_IMMUNE_NETWORKING_P2P,                   /**< P2P network-immune bridge */
    BIO_MODULE_IMMUNE_NETWORKING_REPLICATION,           /**< Replication-immune bridge */
    BIO_MODULE_IMMUNE_NETWORKING_EVENTS,                /**< Events-immune bridge */

    /* NLP immune modules (0x0E10 - 0x0E1F) */
    BIO_MODULE_IMMUNE_NLP = 0x0E10,             /**< NLP-immune base */
    BIO_MODULE_IMMUNE_NLP_CORE,                 /**< Core NLP-immune bridge */
    BIO_MODULE_IMMUNE_SPIKE_NLP,                /**< Spike NLP-immune bridge */
    BIO_MODULE_IMMUNE_MULTIMODAL_NLP,           /**< Multimodal NLP-immune bridge */

    /* Portia immune modules (0x0E20 - 0x0E3F) */
    BIO_MODULE_IMMUNE_PORTIA = 0x0E20,          /**< Portia-immune base */
    BIO_MODULE_IMMUNE_PORTIA_SENSOR,            /**< Portia sensor fusion-immune bridge */
    BIO_MODULE_IMMUNE_PORTIA_LEARNING,          /**< Portia learning-immune bridge */
    BIO_MODULE_IMMUNE_PORTIA_ATTENTION,         /**< Portia attention-immune bridge */
    BIO_MODULE_IMMUNE_PORTIA_CLASSIFICATION,    /**< Portia classification-immune bridge */
    BIO_MODULE_IMMUNE_PORTIA_PLANNING,          /**< Portia planning-immune bridge */
    BIO_MODULE_IMMUNE_PORTIA_POWER,             /**< Portia power-immune bridge */
    BIO_MODULE_IMMUNE_PORTIA_DECEPTION,         /**< Portia deception-immune bridge */
    BIO_MODULE_IMMUNE_PORTIA_DEGRADATION,       /**< Portia degradation-immune bridge */
    BIO_MODULE_IMMUNE_PORTIA_ACCELERATOR,       /**< Portia accelerator-immune bridge */
    BIO_MODULE_IMMUNE_PORTIA_TIER,              /**< Portia tier switch-immune bridge */

    /* Glial immune modules (0x0E40 - 0x0E4F) */
    BIO_MODULE_IMMUNE_GLIAL = 0x0E40,           /**< Glial-immune base */
    BIO_MODULE_IMMUNE_MICROGLIA,                /**< Microglia-immune bridge */
    BIO_MODULE_IMMUNE_ASTROCYTE,                /**< Astrocyte-immune bridge */
    BIO_MODULE_IMMUNE_OLIGODENDROCYTE,          /**< Oligodendrocyte-immune bridge */
    BIO_MODULE_IMMUNE_MYELIN,                   /**< Myelin-immune bridge (MS pathophysiology) */

    /* GPU immune modules (0x0E50 - 0x0E5F) */
    BIO_MODULE_IMMUNE_GPU = 0x0E50,             /**< GPU-immune base */
    BIO_MODULE_IMMUNE_GPU_NEURON,               /**< GPU neuron-immune bridge */
    BIO_MODULE_IMMUNE_GPU_EXECUTION,            /**< GPU execution-immune bridge */
    BIO_MODULE_IMMUNE_MULTIGPU,                 /**< Multi-GPU-immune bridge */
    BIO_MODULE_IMMUNE_GPU_SPIKE_EVENT,          /**< GPU spike event-immune bridge */
    BIO_MODULE_IMMUNE_GPU_SYNAPSE_COMPUTE,      /**< GPU synapse compute-immune bridge */

    /* Information immune modules (0x0E60 - 0x0E6F) */
    BIO_MODULE_IMMUNE_INFORMATION = 0x0E60,     /**< Information-immune base */

    /* Security immune modules (0x0E70 - 0x0E7F) */
    BIO_MODULE_IMMUNE_SECURITY = 0x0E70,        /**< Security-immune base */
    BIO_MODULE_IMMUNE_ANOMALY,                  /**< Anomaly detector-immune bridge */
    BIO_MODULE_IMMUNE_RATE_LIMITER,             /**< Rate limiter-immune bridge */
    BIO_MODULE_IMMUNE_PATTERN_DB,               /**< Pattern database-immune bridge */

    /* Optimization immune modules (0x0E80 - 0x0E8F) */
    BIO_MODULE_IMMUNE_OPTIMIZATION = 0x0E80,    /**< Optimization-immune base */
    BIO_MODULE_IMMUNE_QUANTUM_ANNEALING,        /**< Quantum annealing-immune bridge */

    /* Plasticity immune modules (0x0E88 - 0x0E8A) */
    BIO_MODULE_IMMUNE_PINK_NOISE = 0x0E88,      /**< Pink noise-immune bridge */
    BIO_MODULE_IMMUNE_TRIPLET_STDP,             /**< Triplet STDP-immune bridge */

    /* Swarm immune modules (0x0E8B - 0x0E8F) */
    BIO_MODULE_IMMUNE_SWARM_BRAIN = 0x0E8B,     /**< Swarm brain-immune bridge */
    BIO_MODULE_IMMUNE_SWARM_CONSCIOUSNESS,      /**< Swarm consciousness-immune bridge */
    BIO_MODULE_IMMUNE_SWARM_CONSENSUS,          /**< Swarm consensus-immune bridge */
    BIO_MODULE_IMMUNE_SWARM_EMERGENCE,          /**< Swarm emergence-immune bridge */
    BIO_MODULE_IMMUNE_SWARM_FLOCKING,           /**< Swarm flocking-immune bridge */
    BIO_MODULE_IMMUNE_SWARM_IMMUNE,             /**< Swarm immune-immune bridge */
    BIO_MODULE_IMMUNE_SWARM_MEMORY,             /**< Swarm memory-immune bridge */
    BIO_MODULE_IMMUNE_SWARM_PHEROMONE,          /**< Swarm pheromone-immune bridge */
    BIO_MODULE_IMMUNE_SWARM_QUORUM,             /**< Swarm quorum-immune bridge */
    BIO_MODULE_IMMUNE_SWARM_SIGNAL,             /**< Swarm signal-immune bridge */

    /* Subcortical/Amygdala bridges (0x0E8C - 0x0E8F) */
    BIO_MODULE_AMYGDALA_TRAINING = 0x0E8C,      /**< Amygdala-training bridge (Yerkes-Dodson) */
    BIO_MODULE_AMYGDALA_ATTENTION,              /**< Amygdala-attention bridge (threat salience) */
    BIO_MODULE_AMYGDALA_STRESS,                 /**< Amygdala-stress/wellbeing bridge */
    BIO_MODULE_AMYGDALA_AUTOBIO,                /**< Amygdala-autobiographical memory bridge */

    /* LNN (Liquid Neural Network) modules (0x0E90 - 0x0E9F) */
    BIO_MODULE_LNN_CORE = 0x0E90,               /**< LNN core network module */
    BIO_MODULE_LNN_CALCIUM,                     /**< LNN-calcium dynamics integration */
    BIO_MODULE_LNN_STP,                         /**< LNN-short term plasticity integration */
    BIO_MODULE_LNN_ELIGIBILITY,                 /**< LNN-eligibility trace integration */
    BIO_MODULE_LNN_OSCILLATIONS,                /**< LNN-oscillations integration */
    BIO_MODULE_LNN_CORTICAL_TEMPORAL,           /**< LNN-cortical temporal integration */
    BIO_MODULE_LNN_PREDICTIVE,                  /**< LNN-predictive coding integration */
    BIO_MODULE_LNN_SEQUENCE,                    /**< LNN-sequence detector integration */
    BIO_MODULE_LNN_WORKING_MEMORY,              /**< LNN-working memory integration */
    BIO_MODULE_LNN_SLEEP,                       /**< LNN-sleep cycle integration */
    BIO_MODULE_LNN_AUDIO,                       /**< LNN-audio cortex integration */
    BIO_MODULE_LNN_SPEECH,                      /**< LNN-speech cortex integration */
    BIO_MODULE_LNN_VISUAL,                      /**< LNN-visual cortex integration */
    BIO_MODULE_LNN_HOMEOSTATIC,                 /**< LNN-homeostatic plasticity integration */
    BIO_MODULE_LNN_NEUROMOD,                    /**< LNN-neuromodulator integration */
    BIO_MODULE_LNN_EMOTION,                     /**< LNN-emotion integration */
    BIO_MODULE_LNN_ATTENTION,                   /**< LNN-attention integration */
    BIO_MODULE_LNN_POPULATION,                  /**< LNN-population coding integration */
    BIO_MODULE_LNN_TEMPORAL_CODING,             /**< LNN-temporal coding integration */
    BIO_MODULE_LNN_SECOND_MESSENGER,            /**< LNN-second messenger integration */
    BIO_MODULE_LNN_MOTOR,                       /**< LNN-motor system integration */
    BIO_MODULE_LNN_LANGUAGE,                    /**< LNN-language production integration */
    BIO_MODULE_LNN_TIMESCALES,                  /**< LNN-multiple timescales integration */
    BIO_MODULE_LNN_SYNCHRONY,                   /**< LNN-synchrony detector integration */
    BIO_MODULE_LNN_DENDRITIC,                   /**< LNN-dendritic computation integration */

    /* Free Energy Principle modules (0x0F00 - 0x0FFF) */
    BIO_MODULE_FEP = 0x0F00,                    /**< FEP core module */
    BIO_MODULE_FEP_LEARNING,                    /**< FEP learning module */
    BIO_MODULE_FEP_LEARNING_TRANSITION,         /**< FEP transition learner */
    BIO_MODULE_FEP_LEARNING_LIKELIHOOD,         /**< FEP likelihood learner */
    BIO_MODULE_FEP_CURIOSITY,                   /**< FEP curiosity module */
    BIO_MODULE_FEP_CONSCIOUSNESS,               /**< FEP consciousness bridge */
    BIO_MODULE_FEP_NEUROMOD,                    /**< FEP neuromodulation module */
    BIO_MODULE_FEP_PLANNING,                    /**< FEP planning module */
    BIO_MODULE_FEP_EVIDENCE,                    /**< FEP evidence module */
    BIO_MODULE_FEP_CONTEXT,                     /**< FEP context module */
    BIO_MODULE_FEP_SLEEP,                       /**< FEP sleep module */
    BIO_MODULE_FEP_IMMUNE_BRIDGE,               /**< FEP-immune bridge module */

    /* FEP Integration Bridges - Cognitive (0x0F10 - 0x0F1F) */
    BIO_MODULE_FEP_ATTENTION_BRIDGE = 0x0F10,   /**< FEP-attention bridge */
    BIO_MODULE_FEP_EXECUTIVE_BRIDGE,            /**< FEP-executive bridge */
    BIO_MODULE_FEP_REASONING_BRIDGE,            /**< FEP-reasoning bridge */
    BIO_MODULE_FEP_MEMORY_BRIDGE,               /**< FEP-memory bridge */
    BIO_MODULE_FEP_EMOTION_BRIDGE,              /**< FEP-emotion bridge */
    BIO_MODULE_FEP_MENTAL_HEALTH_BRIDGE,        /**< FEP-mental health bridge */
    BIO_MODULE_FEP_SELF_MODEL_BRIDGE,           /**< FEP-self model bridge */
    BIO_MODULE_FEP_TOM_BRIDGE,                  /**< FEP-theory of mind bridge */
    BIO_MODULE_FEP_INTROSPECTION_BRIDGE,        /**< FEP-introspection bridge */
    BIO_MODULE_FEP_KNOWLEDGE_BRIDGE,            /**< FEP-knowledge bridge */
    BIO_MODULE_FEP_BIAS_BRIDGE,                 /**< FEP-bias bridge */
    BIO_MODULE_FEP_ETHICS_BRIDGE,               /**< FEP-ethics bridge */
    BIO_MODULE_FEP_MIRROR_NEURONS_BRIDGE,       /**< FEP-mirror neurons bridge */
    BIO_MODULE_FEP_CURIOSITY_CORE_BRIDGE,       /**< FEP-curiosity core bridge */
    BIO_MODULE_FEP_AUTOBIOGRAPHICAL_BRIDGE,     /**< FEP-autobiographical bridge */
    BIO_MODULE_FEP_SALIENCE_BRIDGE,             /**< FEP-salience bridge */

    /* FEP Integration Bridges - Other (0x0F20 - 0x0F2F) */
    BIO_MODULE_FEP_GLOBAL_WORKSPACE_BRIDGE = 0x0F20, /**< FEP-global workspace bridge */
    BIO_MODULE_FEP_VISUAL_CORTEX_BRIDGE,        /**< FEP-visual cortex bridge */
    BIO_MODULE_FEP_AUDIO_CORTEX_BRIDGE,         /**< FEP-audio cortex bridge */
    BIO_MODULE_FEP_SPEECH_CORTEX_BRIDGE,        /**< FEP-speech cortex bridge */
    BIO_MODULE_FEP_STDP_BRIDGE,                 /**< FEP-STDP bridge */
    BIO_MODULE_FEP_BCM_BRIDGE,                  /**< FEP-BCM bridge */
    BIO_MODULE_FEP_HOMEOSTATIC_BRIDGE,          /**< FEP-homeostatic bridge */
    BIO_MODULE_FEP_ELIGIBILITY_BRIDGE,          /**< FEP-eligibility bridge */
    BIO_MODULE_FEP_DENDRITIC_BRIDGE,            /**< FEP-dendritic bridge */
    BIO_MODULE_FEP_POPULATION_CODING_BRIDGE,    /**< FEP-population coding bridge */
    BIO_MODULE_FEP_FEATURE_EXTRACTOR_BRIDGE,    /**< FEP-feature extractor bridge */
    BIO_MODULE_FEP_THALAMIC_ROUTER_BRIDGE,      /**< FEP-thalamic router bridge */
    BIO_MODULE_FEP_SEQUENCE_DETECTOR_BRIDGE,    /**< FEP-sequence detector bridge */
    BIO_MODULE_FEP_PREDICTIVE_REGIONS_BRIDGE,   /**< FEP-predictive regions bridge */
    BIO_MODULE_FEP_OSCILLATIONS_BRIDGE,         /**< FEP-oscillations bridge */

    /* FEP Cognitive Bridges - Group 1 (0x0F30 - 0x0F3F) */
    BIO_MODULE_FEP_WORKING_MEMORY_BRIDGE = 0x0F30, /**< FEP-working memory bridge */
    BIO_MODULE_FEP_PREDICTIVE_BRIDGE,           /**< FEP-predictive cognitive bridge */
    BIO_MODULE_FEP_WELLBEING_BRIDGE,            /**< FEP-wellbeing bridge */
    BIO_MODULE_FEP_SLEEP_WAKE_BRIDGE,           /**< FEP-sleep wake bridge */
    BIO_MODULE_FEP_META_LEARNING_BRIDGE,        /**< FEP-meta learning bridge */
    BIO_MODULE_FEP_CONSOLIDATION_BRIDGE,        /**< FEP-consolidation bridge */
    BIO_MODULE_FEP_HIERARCHICAL_BRIDGE,         /**< FEP-hierarchical cognitive bridge */

    /* FEP Cognitive Bridges - Group 2 Emotions (0x0F40 - 0x0F4F) */
    BIO_MODULE_FEP_EMOTIONAL_TAGGING_BRIDGE = 0x0F40, /**< FEP-emotional tagging bridge */
    BIO_MODULE_FEP_EMOTION_RECOGNITION_BRIDGE,  /**< FEP-emotion recognition bridge */
    BIO_MODULE_FEP_EMOTIONS_BRIDGE,             /**< FEP-emotions bridge */
    BIO_MODULE_FEP_EMPATHETIC_RESPONSE_BRIDGE,  /**< FEP-empathetic response bridge */
    BIO_MODULE_FEP_GRIEF_BRIDGE,                /**< FEP-grief bridge */
    BIO_MODULE_FEP_JOY_BRIDGE,                  /**< FEP-joy bridge */
    BIO_MODULE_FEP_REMORSE_BRIDGE,              /**< FEP-remorse bridge */

    /* FEP Cognitive Bridges - Group 3 Reasoning (0x0F50 - 0x0F5F) */
    BIO_MODULE_FEP_ANALYSIS_BRIDGE = 0x0F50,    /**< FEP-analysis bridge */
    BIO_MODULE_FEP_EPISTEMIC_BRIDGE,            /**< FEP-epistemic bridge */
    BIO_MODULE_FEP_EXPLANATIONS_BRIDGE,         /**< FEP-explanations bridge */
    BIO_MODULE_FEP_LOGIC_BRIDGE,                /**< FEP-logic bridge */
    BIO_MODULE_FEP_PERSONALITY_BRIDGE,          /**< FEP-personality bridge */
    BIO_MODULE_FEP_SELF_AWARENESS_BRIDGE,       /**< FEP-self awareness bridge */
    BIO_MODULE_FEP_SHADOW_BRIDGE,               /**< FEP-shadow bridge */
    BIO_MODULE_FEP_SOCIAL_BRIDGE,               /**< FEP-social bridge */

    /* FEP Plasticity Bridges (0x0F60 - 0x0F6F) */
    BIO_MODULE_FEP_ADAPTIVE_BRIDGE = 0x0F60,    /**< FEP-adaptive plasticity bridge */
    BIO_MODULE_FEP_ATTENTION_PLASTICITY_BRIDGE, /**< FEP-attention plasticity bridge */
    BIO_MODULE_FEP_NEUROMODULATORS_BRIDGE,      /**< FEP-neuromodulators bridge */
    BIO_MODULE_FEP_NOISE_BRIDGE,                /**< FEP-noise bridge */
    BIO_MODULE_FEP_PREDICTIVE_PLASTICITY_BRIDGE, /**< FEP-predictive plasticity bridge */
    BIO_MODULE_FEP_STP_BRIDGE,                  /**< FEP-STP bridge */
    BIO_MODULE_FEP_SECOND_MESSENGERS_BRIDGE,    /**< FEP-second messengers bridge */
    BIO_MODULE_STP_PINK_NOISE_BRIDGE,           /**< STP-pink noise bridge */
    BIO_MODULE_HETEROSYNAPTIC_PINK_NOISE,       /**< Heterosynaptic-pink noise bridge */

    /* FEP Middleware Bridges (0x0F70 - 0x0F7F) */
    BIO_MODULE_FEP_BUFFERING_BRIDGE = 0x0F70,   /**< FEP-buffering bridge */
    BIO_MODULE_FEP_ENCODING_BRIDGE,             /**< FEP-encoding bridge */
    BIO_MODULE_FEP_EVENTS_BRIDGE,               /**< FEP-events bridge */
    BIO_MODULE_FEP_INTEGRATION_BRIDGE,          /**< FEP-integration bridge */
    BIO_MODULE_FEP_MIDDLEWARE_MEMORY_BRIDGE,    /**< FEP-middleware memory bridge */
    BIO_MODULE_FEP_NORMALIZATION_BRIDGE,        /**< FEP-normalization bridge */
    BIO_MODULE_FEP_PIPELINE_BRIDGE,             /**< FEP-pipeline bridge */
    BIO_MODULE_FEP_TRAINING_BRIDGE,             /**< FEP-training bridge */

    /* FEP Swarm Bridges (0x0F80 - 0x0F8F) */
    BIO_MODULE_FEP_SWARM_BRAIN = 0x0F80,        /**< FEP-swarm brain bridge */
    BIO_MODULE_FEP_SWARM_CONSCIOUSNESS,         /**< FEP-swarm consciousness bridge */
    BIO_MODULE_FEP_COLLECTIVE_WORKSPACE,        /**< FEP-collective workspace bridge */
    BIO_MODULE_FEP_EMOTIONAL_CONTAGION,         /**< FEP-emotional contagion bridge */
    BIO_MODULE_FEP_SWARM_CONSENSUS,             /**< FEP-swarm consensus bridge */
    BIO_MODULE_FEP_SWARM_EMERGENCE,             /**< FEP-swarm emergence bridge */
    BIO_MODULE_FEP_SWARM_FLOCKING,              /**< FEP-swarm flocking bridge */
    BIO_MODULE_FEP_SWARM_IMMUNE,                /**< FEP-swarm immune bridge */
    BIO_MODULE_FEP_SWARM_MEMORY,                /**< FEP-swarm memory bridge */
    BIO_MODULE_FEP_SWARM_PHEROMONE,             /**< FEP-swarm pheromone bridge */
    BIO_MODULE_FEP_SWARM_QUORUM,                /**< FEP-swarm quorum bridge */
    BIO_MODULE_FEP_SWARM_SIGNAL,                /**< FEP-swarm signal bridge */

    /* FEP Orchestrator (0x0F90) */
    BIO_MODULE_FEP_ORCHESTRATOR = 0x0F90,       /**< FEP orchestrator - coordinates all FEP bridges */
    BIO_MODULE_FEP_CORE_DIRECTIVES,             /**< FEP-core directives bridge */

    /* System Orchestrators/Coordinators (0x0FA0 - 0x0FAF) */
    BIO_MODULE_BIO_ASYNC_ORCHESTRATOR = 0x0FA0, /**< Bio-async orchestrator - coordinates 200+ bio-async modules */
    BIO_MODULE_IMMUNE_COORDINATOR,              /**< Immune bridge coordinator - registry for all immune bridges */
    BIO_MODULE_PLASTICITY_COORDINATOR,          /**< Plasticity coordinator - unified plasticity management */
    BIO_MODULE_COGNITIVE_META_CONTROLLER,       /**< Cognitive meta-controller - arbitrates cognitive modules */
    BIO_MODULE_SWARM_REGISTRY,                  /**< Swarm module registry - plugin architecture for swarm behaviors */
    BIO_MODULE_SECURITY_PERCEPTION,             /**< Security-perception bridge - sensory threat analysis */
    BIO_MODULE_CYCLE_COORDINATOR,               /**< Brain cycle coordinator - unified cycle observability */

    /* Core Directives modules (0x1000 - 0x10FF) - Asimov's Laws, Golden Rule, Ethical Foundation */
    BIO_MODULE_CORE_DIRECTIVES = 0x1000,        /**< Core directives orchestrator - ALL actions pass through */
    BIO_MODULE_ACTION_HISTORY,                  /**< Action history tracker for combinatorial harm */
    BIO_MODULE_HARM_CLASSIFIER,                 /**< Harm outcome classifier */
    BIO_MODULE_HARM_PREVENTION,                 /**< First Law - harm prevention */
    BIO_MODULE_COMMAND_COMPLIANCE,              /**< Second Law - command compliance */
    BIO_MODULE_SELF_PRESERVATION,               /**< Third Law - self preservation */
    BIO_MODULE_RECIPROCITY_EVAL,                /**< Golden Rule - reciprocity evaluation */
    BIO_MODULE_COMBINATORIAL_HARM,              /**< Combinatorial harm detection */
    BIO_MODULE_DIRECTIVE_IMMUNE_BRIDGE,         /**< Core directives-immune bridge */
    BIO_MODULE_DIRECTIVE_FEP_BRIDGE,            /**< Core directives-FEP bridge */

    /* Medulla Oblongata modules (0x1100 - 0x11FF) - Brainstem vital functions */
    BIO_MODULE_MEDULLA = 0x1100,                /**< Medulla orchestrator - vital function coordination */
    BIO_MODULE_AROUSAL_STATE,                   /**< Arousal state management (RAS) */
    BIO_MODULE_PROTECTIVE_CUTOFF,               /**< Emergency protective shutdown */
    BIO_MODULE_BRAINSTEM_COUPLING,              /**< Brainstem-cortex bidirectional coupling */
    BIO_MODULE_CIRCADIAN,                       /**< Circadian rhythm modulation (SCN model) */

    /* Phase 6: Cognitive Substrate Bridges (0x1200 - 0x120F) */
    BIO_MODULE_SUBSTRATE_WORKING_MEMORY = 0x1200, /**< Working memory substrate bridge */
    BIO_MODULE_SUBSTRATE_ATTENTION,             /**< Attention substrate bridge */
    BIO_MODULE_SUBSTRATE_EXECUTIVE,             /**< Executive function substrate bridge */
    BIO_MODULE_SUBSTRATE_EMOTION,               /**< Emotion substrate bridge */
    BIO_MODULE_SUBSTRATE_REASONING,             /**< Reasoning substrate bridge */
    BIO_MODULE_SUBSTRATE_MEMORY_CONSOLIDATION,  /**< Memory consolidation substrate bridge */
    BIO_MODULE_SUBSTRATE_TOM,                   /**< Theory of Mind substrate bridge */
    BIO_MODULE_SUBSTRATE_INTROSPECTION,         /**< Introspection substrate bridge */
    BIO_MODULE_SUBSTRATE_IMAGINATION,           /**< Imagination substrate bridge */

    /* Phase 6: Cognitive Thalamic Bridges (0x1210 - 0x121F) */
    BIO_MODULE_THALAMIC_WORKING_MEMORY = 0x1210, /**< Working memory thalamic bridge */
    BIO_MODULE_THALAMIC_ATTENTION,              /**< Attention thalamic bridge */
    BIO_MODULE_THALAMIC_EXECUTIVE,              /**< Executive function thalamic bridge */
    BIO_MODULE_THALAMIC_EMOTION,                /**< Emotion thalamic bridge */
    BIO_MODULE_THALAMIC_REASONING,              /**< Reasoning thalamic bridge */
    BIO_MODULE_THALAMIC_MEMORY_CONSOLIDATION,   /**< Memory consolidation thalamic bridge */
    BIO_MODULE_THALAMIC_TOM,                    /**< Theory of Mind thalamic bridge */
    BIO_MODULE_THALAMIC_INTROSPECTION,          /**< Introspection thalamic bridge */

    /* Phase 6: Sensory Thalamic Bridges (0x1220 - 0x122F) */
    BIO_MODULE_THALAMIC_VISUAL = 0x1220,        /**< Visual thalamic (LGN) bridge */
    BIO_MODULE_THALAMIC_AUDIO,                  /**< Audio thalamic (MGN) bridge */
    BIO_MODULE_THALAMIC_SPEECH,                 /**< Speech thalamic bridge */

    /* Phase 6: Sensory Substrate Bridges (0x1230 - 0x123F) */
    BIO_MODULE_SUBSTRATE_VISUAL = 0x1230,       /**< Visual substrate bridge */
    BIO_MODULE_SUBSTRATE_AUDIO,                 /**< Audio substrate bridge */
    BIO_MODULE_SUBSTRATE_SPEECH,                /**< Speech substrate bridge */

    /* Hemispheric Brain modules (0x1300 - 0x130F) - Bilateral processing */
    BIO_MODULE_HEMISPHERIC_BRAIN = 0x1300,      /**< Hemispheric brain orchestrator */
    BIO_MODULE_LEFT_HEMISPHERE,                 /**< Left hemisphere processing */
    BIO_MODULE_RIGHT_HEMISPHERE,                /**< Right hemisphere processing */
    BIO_MODULE_CORPUS_CALLOSUM,                 /**< Inter-hemispheric bridge */
    BIO_MODULE_LATERALIZATION,                  /**< Lateralization control */
    BIO_MODULE_HEMISPHERIC_PORTIA,              /**< Per-hemisphere Portia integration */
    BIO_MODULE_HEMISPHERIC_IMMUNE,              /**< Hemispheric-immune bridge */
    BIO_MODULE_HEMISPHERIC_SLEEP,               /**< Hemispheric-sleep bridge */
    BIO_MODULE_HEMISPHERIC_FEP,                 /**< Hemispheric-FEP bridge */
    BIO_MODULE_HEMISPHERIC_GLIAL,               /**< Hemispheric-glial bridge */
    BIO_MODULE_HEMISPHERIC_CONTRALATERAL,       /**< Contralateral motor/sensory mapping */
    BIO_MODULE_HEMISPHERIC_INJURY,              /**< Brain injury and recovery modeling */
    BIO_MODULE_SPLIT_BRAIN_EXPERIMENTS,         /**< Split-brain experimental framework */

    /* Imagination Engine modules (0x1A00 - 0x1A0F) */
    BIO_MODULE_IMAGINATION = 0x1A00,            /**< Imagination engine core */
    BIO_MODULE_IMAGINATION_WORKSPACE,           /**< Imagination workspace manager */
    BIO_MODULE_IMAGINATION_HIPPOCAMPUS,         /**< Hippocampus-imagination bridge */
    BIO_MODULE_IMAGINATION_PREFRONTAL,          /**< Prefrontal-imagination bridge */
    BIO_MODULE_IMAGINATION_GW,                  /**< Global workspace-imagination bridge */
    BIO_MODULE_IMAGINATION_JEPA,                /**< JEPA-imagination bridge */
    BIO_MODULE_IMAGINATION_SLEEP,               /**< Sleep-imagination bridge */
    BIO_MODULE_IMAGINATION_TOM,                 /**< Theory of mind-imagination bridge */
    BIO_MODULE_IMAGINATION_VISUAL,              /**< Visual-imagination bridge */
    BIO_MODULE_IMAGINATION_AUDIO,               /**< Audio-imagination bridge */
    BIO_MODULE_IMAGINATION_CURIOSITY,           /**< Curiosity-imagination bridge */
    BIO_MODULE_IMAGINATION_PARIETAL,            /**< Parietal-imagination bridge */
    BIO_MODULE_IMAGINATION_COLLECTIVE,          /**< Collective imagination (swarm) */

    /* LGSS (Layered Governance Safety System) modules (0x1C00 - 0x1C0F) */
    BIO_MODULE_LGSS = 0x1C00,                   /**< LGSS core module */
    BIO_MODULE_LGSS_EVALUATOR,                  /**< LGSS policy evaluator */
    BIO_MODULE_LGSS_MONITOR,                    /**< LGSS real-time monitor */
    BIO_MODULE_LGSS_AUDIT,                      /**< LGSS audit subsystem */
    BIO_MODULE_LGSS_INTEGRITY,                  /**< LGSS integrity checker */
    BIO_MODULE_LGSS_OVERRIDE,                   /**< LGSS override controller */
    BIO_MODULE_LGSS_EXTERNAL,                   /**< LGSS external interface */
    BIO_MODULE_LGSS_BIO_BRIDGE,                 /**< LGSS bio-async bridge */

    /* Safety Enhancement modules (0x1C10 - 0x1C2F) - AI Safety Infrastructure */
    BIO_MODULE_EMERGENCY_HALT = 0x1C10,         /**< Emergency halt system */
    BIO_MODULE_TRIPWIRES,                       /**< Tripwire detection system */
    BIO_MODULE_CORRIGIBILITY,                   /**< Corrigibility module */
    BIO_MODULE_ALIGNMENT_MONITOR,               /**< Alignment drift monitor */
    BIO_MODULE_CAPABILITY_CONTROL,              /**< Capability control module */
    BIO_MODULE_INTERPRETABILITY,                /**< Interpretability module */
    BIO_MODULE_SAFETY_VERIFICATION,             /**< Formal verification module */
    BIO_MODULE_RED_TEAM,                        /**< Red team infrastructure */
    BIO_MODULE_GRADUATED_AUTONOMY,              /**< Graduated autonomy system */
    BIO_MODULE_VALUE_COMMITMENT,                /**< Cryptographic value commitment */

    /* Fault Tolerance / Self-Repair modules (0x1D00 - 0x1D0F) */
    BIO_MODULE_SELF_REPAIR = 0x1D00,            /**< Self-repair coordinator */
    BIO_MODULE_CODE_GENERATION,                 /**< Code generation engine */
    BIO_MODULE_VCS_INTEGRATION,                 /**< VCS/git integration */
    BIO_MODULE_HOT_INJECT,                      /**< Hot code injection */
    BIO_MODULE_RECOMPILER,                      /**< Recompiler validation */
    BIO_MODULE_CODE_IMMUNE,                     /**< Code immune system */
    BIO_MODULE_DIAGNOSTICS,                     /**< Fault diagnostics */
    BIO_MODULE_RECOVERY_BRIDGE,                 /**< Recovery-parietal bridge */
    BIO_MODULE_HEALTH_SELF_REPAIR_BRIDGE,       /**< Health-to-self-repair bridge */
    BIO_MODULE_HEALTH_DIAGNOSTIC_BRIDGE,        /**< Health diagnostic converter */
    BIO_MODULE_CODE_IMMUNE_SELF_REPAIR,         /**< Code immune self-repair integration */
    BIO_MODULE_SELF_REPAIR_HEALTH_NOTIFY,       /**< Self-repair health notification */

    /* Society of Thought modules (0x1E00 - 0x1E0F) */
    BIO_MODULE_SOCIETY_OF_THOUGHT = 0x1E00,     /**< Society of Thought orchestrator */
    BIO_MODULE_SURPRISE_AMPLIFIER,              /**< Surprise amplification system (0x1E01) */
    BIO_MODULE_COGNITIVE_AGENT_PROFILES,        /**< Personality-diverse agent profiles */
    BIO_MODULE_ADVERSARIAL_PERSPECTIVE,         /**< Adversarial perspective protocol */
    BIO_MODULE_SOCIAL_SCALING,                  /**< Social scaling controller */
    BIO_MODULE_SOCIETY_ENGINE,                  /**< Society of Thought reasoning engine */
    BIO_MODULE_SURPRISE_PLASTICITY,             /**< Surprise ↔ plasticity bridge (0x1E06) */
    BIO_MODULE_SURPRISE_SNN,                    /**< Surprise ↔ SNN bridge (0x1E07) */
    BIO_MODULE_SURPRISE_SUBSTRATE,              /**< Surprise ↔ substrate bridge (0x1E08) */
    BIO_MODULE_SURPRISE_THALAMIC,               /**< Surprise ↔ thalamic bridge (0x1E09) */
    BIO_MODULE_SURPRISE_PINK_NOISE,             /**< Surprise ↔ pink noise bridge (0x1E0A) */
    BIO_MODULE_SURPRISE_IMAGINATION,            /**< Surprise ↔ imagination bridge (0x1E0B) */
    BIO_MODULE_SURPRISE_SELF_MODEL,             /**< Surprise ↔ self-model bridge (0x1E0C) */

    /* Inner Dialogue modules (0x1E10 - 0x1E1F) */
    BIO_MODULE_INNER_DIALOGUE = 0x1E10,         /**< Inner dialogue engine */

    /* Octopus: distributed peripheral cognition (Phase 2b bio-async bridge) */
    BIO_MODULE_OCTOPUS = 0x1F00,                /**< Octopus module — broadcasts arm events, coherence, heartbeat */

    /* Special values (Phase 7: Runtime Message Orchestration) */
    BIO_MODULE_MESH_ROUTE = 0xFFFD,  /**< Phase 15: Force routing through mesh network */
    BIO_MODULE_KG_DISPATCH = 0xFFFE, /**< KG-driven dispatch: route to all handlers for message type */
    BIO_MODULE_ALL = 0xFFFF,         /**< Broadcast to all modules */

    BIO_MODULE_COUNT
} bio_module_id_t;

/*=============================================================================
 * BRAIN MESSAGES
 *============================================================================*/

/**
 * @brief Query brain state (via acetylcholine - fast query)
 */
typedef struct {
    bio_message_header_t header;
    uint32_t query_flags;           /**< What state to query */
    uint32_t region_id;             /**< Specific region (0 = global) */
} bio_msg_brain_state_query_t;

#define BIO_BRAIN_QUERY_NEURON_COUNT    (1 << 0)
#define BIO_BRAIN_QUERY_SYNAPSE_COUNT   (1 << 1)
#define BIO_BRAIN_QUERY_ACTIVE_REGIONS  (1 << 2)
#define BIO_BRAIN_QUERY_NEUROMODULATORS (1 << 3)
#define BIO_BRAIN_QUERY_ENERGY_STATE    (1 << 4)

/**
 * @brief Brain state response
 */
typedef struct {
    bio_message_header_t header;
    uint32_t neuron_count;
    uint32_t synapse_count;
    uint32_t active_region_count;
    float dopamine_level;
    float serotonin_level;
    float norepinephrine_level;
    float acetylcholine_level;
    float energy_level;
    float global_activity;
} bio_msg_brain_state_response_t;

/**
 * @brief Request neuron activation
 */
typedef struct {
    bio_message_header_t header;
    uint32_t neuron_id;
    float input_current;
    float duration_ms;
} bio_msg_neuron_activation_request_t;

/**
 * @brief Neuron activation response
 */
typedef struct {
    bio_message_header_t header;
    uint32_t neuron_id;
    float membrane_potential;
    bool spiked;
    float spike_time_ms;
} bio_msg_neuron_activation_response_t;

/**
 * @brief Region configuration query
 */
typedef struct {
    bio_message_header_t header;
    uint32_t region_id;
    uint32_t query_flags;        /**< What information to query */
} bio_msg_region_config_query_t;

/**
 * @brief Region configuration response
 */
typedef struct {
    bio_message_header_t header;
    uint32_t region_id;
    uint32_t neuron_count;
    uint32_t synapse_count;
    uint32_t active_region_count;
    float dopamine_level;
    float serotonin_level;
    float norepinephrine_level;
    float acetylcholine_level;
} bio_msg_region_config_response_t;

/*=============================================================================
 * LOGIC MESSAGES
 *============================================================================*/

/**
 * @brief Request logic gate evaluation
 */
typedef struct {
    bio_message_header_t header;
    uint32_t gate_id;               /**< Gate neuron ID */
    uint32_t gate_type;             /**< Logic gate type (AND/OR/NOT/XOR/IMPLIES) */
    float input_a;                  /**< Input A value [0,1] */
    float input_b;                  /**< Input B value [0,1] */
    float threshold;                /**< Custom threshold (0 = use default) */
} bio_msg_logic_gate_evaluate_t;

/**
 * @brief Logic gate evaluation result
 */
typedef struct {
    bio_message_header_t header;
    uint32_t gate_id;               /**< Gate neuron ID */
    uint32_t gate_type;             /**< Logic gate type */
    float output;                   /**< Output value [0,1] */
    bool spiked;                    /**< Whether gate spiked */
    uint64_t spike_time_us;         /**< Spike timestamp */
    float threshold_used;           /**< Threshold that was used */
} bio_msg_logic_gate_result_t;

/**
 * @brief Logic circuit simulation step request
 */
typedef struct {
    bio_message_header_t header;
    uint64_t timestamp_us;          /**< Current simulation time */
    uint64_t delta_t_us;            /**< Timestep duration */
    uint32_t max_iterations;        /**< Max iterations (0 = until stable) */
} bio_msg_logic_circuit_step_t;

/**
 * @brief Logic circuit step completion notification
 */
typedef struct {
    bio_message_header_t header;
    uint64_t timestamp_us;          /**< Completion timestamp */
    uint32_t spikes_generated;      /**< Number of gate spikes */
    uint32_t gates_evaluated;       /**< Number of gates updated */
    float avg_eval_time_us;         /**< Average evaluation time */
    bool circuit_stable;            /**< Whether outputs stabilized */
} bio_msg_logic_circuit_complete_t;

/**
 * @brief Logic spike event (broadcast when gate fires)
 */
typedef struct {
    bio_message_header_t header;
    uint32_t gate_id;               /**< Firing gate ID */
    uint32_t gate_type;             /**< Gate type */
    float output;                   /**< Output value */
    uint64_t spike_time_us;         /**< Spike timestamp */
    uint32_t propagation_count;     /**< Number of downstream targets */
} bio_msg_logic_spike_event_t;

/*=============================================================================
 * PLASTICITY MESSAGES
 *============================================================================*/

/**
 * @brief Request weight update (via dopamine - reward signal)
 */
typedef struct {
    bio_message_header_t header;
    uint32_t synapse_id;
    uint32_t pre_neuron_id;
    uint32_t post_neuron_id;
    float weight_delta;
    float learning_rate;
    float eligibility_trace;
    bool clamp_to_bounds;
    float min_weight;
    float max_weight;
} bio_msg_weight_update_request_t;

/**
 * @brief Weight update response
 */
typedef struct {
    bio_message_header_t header;
    uint32_t synapse_id;
    float old_weight;
    float new_weight;
    bool clamped;
    nimcp_error_t error;
} bio_msg_weight_update_response_t;

/**
 * @brief STDP spike timing event
 */
typedef struct {
    bio_message_header_t header;
    uint32_t pre_neuron_id;
    uint32_t post_neuron_id;
    float pre_spike_time_ms;
    float post_spike_time_ms;
    float delta_t_ms;               /**< post - pre timing */
} bio_msg_stdp_event_t;

/**
 * @brief Batched STDP events for efficiency
 */
typedef struct {
    bio_message_header_t header;
    uint32_t event_count;
    uint32_t max_events;
    /* Followed by event_count * bio_msg_stdp_event_t */
} bio_msg_stdp_batch_t;

/**
 * @brief Learning rate update request/response
 */
typedef struct {
    bio_message_header_t header;
    uint32_t synapse_id;
    float base_learning_rate;
    float modulated_learning_rate;
    float dopamine_level;
    float serotonin_level;
} bio_msg_learning_rate_update_t;

/**
 * @brief Neuromodulator release event
 */
typedef struct {
    bio_message_header_t header;
    nimcp_bio_channel_type_t neuromodulator;
    uint32_t source_region;
    float release_amount;
    float current_concentration;
    float diffusion_radius_um;
} bio_msg_neuromodulator_release_t;

/**
 * @brief Eligibility trace update event
 */
typedef struct {
    bio_message_header_t header;
    uint32_t synapse_id;
    float trace_value;
    float reward_signal;
    float dopamine_level;
    uint64_t update_time_us;
} bio_msg_eligibility_trace_update_t;

/*=============================================================================
 * SECOND MESSENGER MESSAGES
 *============================================================================*/

/**
 * @brief Second messenger cascade state update
 *
 * WHAT: Broadcast cascade state changes
 * WHY:  Notify downstream modules of signaling cascade activity
 * HOW:  Published on serotonin channel (slow, modulatory)
 */
typedef struct {
    bio_message_header_t header;
    uint32_t neuron_id;
    float camp_concentration;       /**< cAMP level [0, 1] normalized */
    float ip3_concentration;        /**< IP3 level [0, 1] normalized */
    float dag_concentration;        /**< DAG level [0, 1] normalized */
    float calcium_concentration;    /**< Cytosolic Ca2+ [0, 1] normalized */
    float pka_activity;            /**< PKA kinase activity [0, 1] */
    float pkc_activity;            /**< PKC kinase activity [0, 1] */
    float camkii_activity;         /**< CaMKII activity [0, 1] */
    float creb_phosphorylation;    /**< CREB transcription factor [0, 1] */
    uint64_t update_time_us;
} bio_msg_second_messenger_update_t;

/**
 * @brief Second messenger query
 */
typedef struct {
    bio_message_header_t header;
    uint32_t neuron_id;
    uint32_t query_flags;          /**< Which components to query */
} bio_msg_second_messenger_query_t;

/** Query flags for second messenger queries */
#define BIO_SM_QUERY_CAMP       (1 << 0)
#define BIO_SM_QUERY_IP3_DAG    (1 << 1)
#define BIO_SM_QUERY_CALCIUM    (1 << 2)
#define BIO_SM_QUERY_KINASES    (1 << 3)
#define BIO_SM_QUERY_GENE_EXPR  (1 << 4)
#define BIO_SM_QUERY_ALL        0xFFFFFFFF

/**
 * @brief Second messenger receptor activation event
 */
typedef struct {
    bio_message_header_t header;
    uint32_t neuron_id;
    uint32_t receptor_type;        /**< GPCR type (D1, D2, 5-HT2A, etc.) */
    uint32_t coupling_type;        /**< Gs, Gi, Gq */
    float occupancy;               /**< Receptor occupancy [0, 1] */
    uint64_t activation_time_us;
} bio_msg_second_messenger_receptor_t;

/** G-protein coupling types for messages */
#define BIO_SM_COUPLING_GS      0  /**< Gs → adenylyl cyclase → cAMP ↑ */
#define BIO_SM_COUPLING_GI      1  /**< Gi → adenylyl cyclase → cAMP ↓ */
#define BIO_SM_COUPLING_GQ      2  /**< Gq → PLC → IP3/DAG */

/**
 * @brief cAMP concentration change event
 */
typedef struct {
    bio_message_header_t header;
    uint32_t neuron_id;
    float previous_concentration;
    float new_concentration;
    float delta;                   /**< Change magnitude */
    bool threshold_crossed;        /**< True if PKA activation threshold crossed */
} bio_msg_second_messenger_camp_t;

/**
 * @brief Calcium release event
 */
typedef struct {
    bio_message_header_t header;
    uint32_t neuron_id;
    float previous_calcium;
    float new_calcium;
    float er_store_level;          /**< ER calcium store [0, 1] */
    bool calmodulin_bound;         /**< Calmodulin-Ca2+ binding occurred */
    bool camkii_triggered;         /**< CaMKII activation triggered */
} bio_msg_second_messenger_calcium_t;

/**
 * @brief Kinase activation event (PKA, PKC, CaMKII)
 */
typedef struct {
    bio_message_header_t header;
    uint32_t neuron_id;
    uint32_t kinase_type;          /**< Which kinase (PKA, PKC, CaMKII) */
    float activity_level;          /**< Current activity [0, 1] */
    uint32_t substrate_count;      /**< Number of substrates phosphorylated */
    bool nuclear_translocation;    /**< True if kinase translocated to nucleus */
} bio_msg_second_messenger_kinase_t;

/** Kinase types for messages */
#define BIO_SM_KINASE_PKA       0
#define BIO_SM_KINASE_PKC       1
#define BIO_SM_KINASE_CAMKII    2

/**
 * @brief CREB phosphorylation event
 */
typedef struct {
    bio_message_header_t header;
    uint32_t neuron_id;
    float phosphorylation_level;   /**< CREB phosphorylation [0, 1] */
    bool cre_binding_active;       /**< CREB bound to CRE element */
    uint32_t target_gene_count;    /**< Number of target genes affected */
} bio_msg_second_messenger_creb_t;

/**
 * @brief Immediate early gene expression event
 */
typedef struct {
    bio_message_header_t header;
    uint32_t neuron_id;
    uint32_t ieg_type;             /**< Which IEG (c-Fos, Arc, BDNF, etc.) */
    float expression_level;        /**< mRNA expression [0, 1] */
    float protein_level;           /**< Protein translation [0, 1] */
    uint64_t expression_start_us;  /**< When expression began */
    uint64_t peak_time_us;         /**< Expected peak time */
} bio_msg_second_messenger_ieg_t;

/** IEG types for messages */
#define BIO_SM_IEG_CFOS         0
#define BIO_SM_IEG_ARC          1
#define BIO_SM_IEG_BDNF         2
#define BIO_SM_IEG_CREB1        3

/**
 * @brief Full cascade completion event
 */
typedef struct {
    bio_message_header_t header;
    uint32_t neuron_id;
    uint32_t cascade_type;         /**< Which cascade completed */
    float total_signal_strength;   /**< Integrated signal strength */
    uint64_t cascade_duration_us;  /**< Time from receptor to gene expression */
    bool protein_synthesis;        /**< True if protein synthesis occurred */
} bio_msg_second_messenger_complete_t;

/** Cascade types for completion messages */
#define BIO_SM_CASCADE_CAMP_PKA_CREB    0
#define BIO_SM_CASCADE_IP3_CALCIUM      1
#define BIO_SM_CASCADE_DAG_PKC          2
#define BIO_SM_CASCADE_FULL             3

/*=============================================================================
 * COGNITIVE MESSAGES
 *============================================================================*/

/**
 * @brief Introspection query (via acetylcholine - attention)
 */
typedef struct {
    bio_message_header_t header;
    uint32_t query_type;
    uint32_t target_pattern_id;
    float confidence_threshold;
} bio_msg_introspection_query_t;

#define BIO_INTRO_QUERY_PATTERN_MATCH   0x01
#define BIO_INTRO_QUERY_SELF_STATE      0x02
#define BIO_INTRO_QUERY_COGNITIVE_LOAD  0x03
#define BIO_INTRO_QUERY_EMOTIONAL_STATE 0x04

/**
 * @brief Introspection response
 */
typedef struct {
    bio_message_header_t header;
    uint32_t query_type;
    float confidence;
    float cognitive_load;
    float emotional_valence;
    float arousal;
    uint32_t matched_pattern_count;
} bio_msg_introspection_response_t;

/**
 * @brief Ethics evaluation request (via serotonin - deliberative)
 */
typedef struct {
    bio_message_header_t header;
    uint32_t action_id;
    uint32_t context_id;
    float urgency;
    uint32_t stakeholder_count;
    /* Followed by stakeholder impact data */
} bio_msg_ethics_request_t;

/**
 * @brief Ethics evaluation response
 */
typedef struct {
    bio_message_header_t header;
    uint32_t action_id;
    float ethical_score;            /**< [-1, 1] negative=harmful, positive=beneficial */
    float confidence;
    bool veto;                      /**< true = action should be blocked */
    uint32_t primary_concern;       /**< Ethics framework triggered */
    char explanation[256];
} bio_msg_ethics_response_t;

/**
 * @brief Salience evaluation (via norepinephrine - alerting)
 */
typedef struct {
    bio_message_header_t header;
    uint32_t stimulus_id;
    float raw_intensity;
    float novelty;
    float relevance;
} bio_msg_salience_query_t;

/**
 * @brief Salience response
 */
typedef struct {
    bio_message_header_t header;
    uint32_t stimulus_id;
    float salience_score;           /**< [0, 1] combined salience */
    float attention_priority;
    bool requires_immediate_attention;
} bio_msg_salience_response_t;

/**
 * @brief Attention shift command
 */
typedef struct {
    bio_message_header_t header;
    uint32_t target_id;
    float attention_weight;
    uint32_t duration_ms;
    bool preemptive;
} bio_msg_attention_shift_t;

/**
 * @brief Working memory store request
 */
typedef struct {
    bio_message_header_t header;
    uint32_t slot_id;
    uint32_t data_size;
    float priority;
    uint32_t decay_ms;
    /* Followed by data */
} bio_msg_wm_store_t;

/**
 * @brief Working memory retrieve request
 */
typedef struct {
    bio_message_header_t header;
    uint32_t slot_id;
    float min_confidence;
} bio_msg_wm_retrieve_t;

/**
 * @brief Curiosity signal (intrinsic motivation)
 */
typedef struct {
    bio_message_header_t header;
    uint32_t target_id;
    float curiosity_intensity;
    float information_gain_estimate;
    float exploration_bonus;
} bio_msg_curiosity_signal_t;

/**
 * @brief Knowledge query request
 */
typedef struct {
    bio_message_header_t header;
    char query_str[256];            /**< Query string */
    uint32_t max_results;           /**< Maximum results to return */
    float min_confidence;           /**< Minimum confidence threshold */
} bio_msg_knowledge_query_t;

/**
 * @brief Knowledge query response
 */
typedef struct {
    bio_message_header_t header;
    bool success;                   /**< Query succeeded */
    uint32_t num_matches;           /**< Number of matches found */
    char matches[10][256];          /**< Up to 10 matching results */
    float confidence[10];           /**< Confidence for each match */
} bio_msg_knowledge_response_t;

/**
 * @brief Decision request
 */
typedef struct {
    bio_message_header_t header;
    char decision_context[256];     /**< Context for the decision */
    uint32_t option_count;          /**< Number of options */
    float urgency;                  /**< How urgent the decision is */
} bio_msg_decision_request_t;

/**
 * @brief Decision response
 */
typedef struct {
    bio_message_header_t header;
    bool approved;                  /**< Decision approved */
    float confidence;               /**< Confidence in decision */
    char reasoning[512];            /**< Explanation of decision */
    uint32_t selected_option;       /**< Selected option index */
} bio_msg_decision_response_t;

/**
 * @brief Agent belief update (Theory of Mind broadcast)
 *
 * WHAT: Notifies modules that an agent's belief state has been updated
 * WHY:  Executive and ethics modules need to react to belief changes
 * HOW:  Broadcast via serotonin channel (slow, coordinated updates)
 */
typedef struct {
    bio_message_header_t header;
    uint32_t agent_id;              /**< Which agent's belief changed */
    char belief_content[256];       /**< What the agent believes */
    float confidence;               /**< Confidence in this belief [0.0, 1.0] */
    bool is_false_belief;           /**< Does this contradict reality? */
    uint64_t timestamp_ms;          /**< When belief was updated */
} bio_msg_agent_belief_update_t;

/**
 * @brief Agent intention inferred (Theory of Mind broadcast)
 *
 * WHAT: Notifies modules that an agent's intention has been inferred
 * WHY:  Enable proactive coordination and ethical evaluation
 * HOW:  Broadcast via acetylcholine channel (fast, attention-worthy)
 */
typedef struct {
    bio_message_header_t header;
    uint32_t agent_id;              /**< Which agent's intention was inferred */
    char action_description[256];   /**< What the agent plans to do */
    float likelihood;               /**< Probability of executing [0.0, 1.0] */
    uint32_t emotional_state;       /**< Agent's current emotional state (tom_emotion_t) */
    char goal_description[256];     /**< Agent's underlying goal */
    uint64_t timestamp_ms;          /**< When intention was inferred */
} bio_msg_agent_intention_update_t;

/*=============================================================================
 * EMOTION TENSOR MESSAGES
 *============================================================================*/

/** Number of primary emotions in tensor (Plutchik's 8) */
#define BIO_EMOTION_TENSOR_PRIMARY_COUNT 8

/** Number of compound emotions */
#define BIO_EMOTION_TENSOR_COMPOUND_COUNT 24

/**
 * @brief Emotion tensor state update (broadcast via serotonin)
 *
 * WHAT: Broadcasts current emotion tensor state
 * WHY:  Enable other modules to react to emotional state
 * HOW:  Contains all 8 primary channels + computed aggregates
 */
typedef struct {
    bio_message_header_t header;
    float channels[BIO_EMOTION_TENSOR_PRIMARY_COUNT];  /**< Primary emotion activations */
    float valence;                      /**< Computed valence [-1, +1] */
    float arousal;                      /**< Computed arousal [0, 1] */
    float entropy;                      /**< Emotional diversity [0, 1] */
    float stability;                    /**< Emotional stability [0, 1] */
    uint8_t primary_emotion;            /**< Dominant primary emotion index */
    uint8_t secondary_emotion;          /**< Secondary emotion index */
    float blend_ratio;                  /**< Secondary/primary ratio [0, 1] */
    uint64_t timestamp_ms;              /**< Update timestamp */
} bio_msg_emotion_tensor_update_t;

/**
 * @brief Query emotion tensor state
 */
typedef struct {
    bio_message_header_t header;
    uint32_t query_flags;               /**< What to query */
    uint32_t target_brain_id;           /**< Target brain (0 = local) */
} bio_msg_emotion_tensor_query_t;

#define BIO_TENSOR_QUERY_CHANNELS     (1 << 0)  /**< Query primary channels */
#define BIO_TENSOR_QUERY_COMPOUNDS    (1 << 1)  /**< Query compound emotions */
#define BIO_TENSOR_QUERY_AGGREGATES   (1 << 2)  /**< Query valence/arousal/entropy */
#define BIO_TENSOR_QUERY_DYNAMICS     (1 << 3)  /**< Query temporal dynamics */
#define BIO_TENSOR_QUERY_ALL          0xFFFFFFFF

/**
 * @brief Emotion tensor state response
 */
typedef struct {
    bio_message_header_t header;
    float channels[BIO_EMOTION_TENSOR_PRIMARY_COUNT];  /**< Primary channels */
    float compounds[BIO_EMOTION_TENSOR_COMPOUND_COUNT]; /**< Compound emotions */
    float valence;                      /**< Computed valence */
    float arousal;                      /**< Computed arousal */
    float entropy;                      /**< Emotional diversity */
    float stability;                    /**< Emotional stability */
    uint8_t primary_emotion;            /**< Dominant emotion */
    uint8_t secondary_emotion;          /**< Secondary emotion */
    bool contradictory;                 /**< Has contradictory emotions */
    uint64_t timestamp_ms;              /**< State timestamp */
} bio_msg_emotion_tensor_response_t;

/**
 * @brief Apply stimulus to emotion tensor
 *
 * WHAT: External stimulus affecting emotional state
 * WHY:  Events, inputs, situations trigger emotions
 * HOW:  Modify specified channel based on stimulus
 */
typedef struct {
    bio_message_header_t header;
    uint8_t target_emotion;             /**< Primary emotion index to affect */
    float intensity;                    /**< Stimulus intensity [0, 1] */
    bool is_positive;                   /**< Positive (add) or negative (subtract) */
    uint32_t source_id;                 /**< Source of stimulus (module/event) */
    uint64_t timestamp_ms;              /**< Stimulus timestamp */
} bio_msg_emotion_tensor_stimulus_t;

/**
 * @brief Propagate emotion tensor to swarm agents
 *
 * WHAT: Request emotion propagation across swarm
 * WHY:  Enable emotional contagion from individual to swarm
 * HOW:  Convert tensor to swarm emotion, trigger propagation
 */
typedef struct {
    bio_message_header_t header;
    uint32_t source_agent_id;           /**< Originating agent */
    uint8_t emotion_type;               /**< Swarm emotion type (converted from tensor) */
    float intensity;                    /**< Emotion intensity [0, 1] */
    uint32_t max_propagation_depth;     /**< Max hops (0 = unlimited) */
    bool override_resistance;           /**< Bypass resistance checks */
} bio_msg_emotion_tensor_propagate_t;

/**
 * @brief Compound emotion detected notification
 *
 * WHAT: Notification that compound emotion is active
 * WHY:  Alert system to complex emotional states
 * HOW:  Broadcast when compound exceeds threshold
 */
typedef struct {
    bio_message_header_t header;
    uint8_t compound_type;              /**< Compound emotion index */
    float activation;                   /**< Activation level [0, 1] */
    uint8_t primary_a;                  /**< First contributing primary */
    uint8_t primary_b;                  /**< Second contributing primary */
    float primary_a_level;              /**< First primary activation */
    float primary_b_level;              /**< Second primary activation */
    bool is_contradictory;              /**< Tertiary dyad (contradictory) */
} bio_msg_emotion_tensor_compound_t;

/**
 * @brief Emotional contradiction detected
 *
 * WHAT: Notification of contradictory emotions
 * WHY:  Flag ambivalence, internal conflict
 * HOW:  Broadcast when opposing emotions both active
 */
typedef struct {
    bio_message_header_t header;
    uint8_t emotion_a;                  /**< First conflicting emotion */
    uint8_t emotion_b;                  /**< Second conflicting emotion */
    float level_a;                      /**< First emotion level */
    float level_b;                      /**< Second emotion level */
    float conflict_intensity;           /**< Magnitude of conflict [0, 1] */
} bio_msg_emotion_tensor_contradiction_t;

/**
 * @brief Sync tensor with swarm emotional contagion
 *
 * WHAT: Bidirectional sync between tensor and swarm system
 * WHY:  Keep individual and collective emotions aligned
 * HOW:  Exchange state with emotional contagion system
 */
typedef struct {
    bio_message_header_t header;
    uint32_t agent_id;                  /**< Agent to sync with */
    uint8_t direction;                  /**< 0=tensor->swarm, 1=swarm->tensor, 2=bidirectional */
    float blend_factor;                 /**< How much to blend (0=keep current, 1=full replace) */

    /* Tensor state (for tensor->swarm) */
    float tensor_channels[BIO_EMOTION_TENSOR_PRIMARY_COUNT];
    float tensor_valence;
    float tensor_arousal;

    /* Swarm state (for swarm->tensor) */
    uint8_t swarm_emotion;              /**< Swarm emotion type */
    float swarm_intensity;              /**< Swarm emotion intensity */
    float susceptibility;               /**< Agent susceptibility */
} bio_msg_emotion_swarm_sync_t;

#define BIO_EMOTION_SYNC_TENSOR_TO_SWARM  0
#define BIO_EMOTION_SYNC_SWARM_TO_TENSOR  1
#define BIO_EMOTION_SYNC_BIDIRECTIONAL    2

/*=============================================================================
 * GLIAL MESSAGES
 *============================================================================*/

/**
 * @brief Astrocyte calcium wave initiation
 */
typedef struct {
    bio_message_header_t header;
    uint32_t source_region;
    float initial_calcium_um;
    float propagation_speed_um_s;
    nimcp_wave_mode_t mode;
} bio_msg_astrocyte_wave_t;

/**
 * @brief Microglia alert (immune/maintenance)
 */
typedef struct {
    bio_message_header_t header;
    uint32_t alert_region;
    uint32_t alert_type;
    float severity;
    uint32_t affected_synapse_count;
} bio_msg_microglia_alert_t;

#define BIO_MICROGLIA_ALERT_DAMAGE      0x01
#define BIO_MICROGLIA_ALERT_INFECTION   0x02
#define BIO_MICROGLIA_ALERT_PRUNE_NEEDED 0x03
#define BIO_MICROGLIA_ALERT_DEBRIS      0x04

/**
 * @brief Oligodendrocyte myelination request
 */
typedef struct {
    bio_message_header_t header;
    uint32_t axon_id;
    float target_thickness;
    float priority;
    uint32_t segment_start;
    uint32_t segment_length;
} bio_msg_oligodendrocyte_myelinate_t;

/**
 * @brief Metabolic demand signal
 */
typedef struct {
    bio_message_header_t header;
    uint32_t region_id;
    float glucose_demand;
    float oxygen_demand;
    float atp_deficit;
    float urgency;
} bio_msg_metabolic_demand_t;

/*=============================================================================
 * MIDDLEWARE MESSAGES
 *============================================================================*/

/**
 * @brief Pipeline stage completion
 */
typedef struct {
    bio_message_header_t header;
    uint32_t pipeline_id;
    uint32_t stage_id;
    float processing_time_ms;
    nimcp_error_t status;
    uint32_t output_size;
} bio_msg_pipeline_complete_t;

/**
 * @brief Pipeline error notification
 */
typedef struct {
    bio_message_header_t header;
    uint32_t pipeline_id;
    uint32_t stage_id;
    uint32_t error_code;
    char error_message[64];
} bio_msg_pipeline_error_t;

/**
 * @brief Gradient computation result
 */
typedef struct {
    bio_message_header_t header;
    uint32_t layer_id;
    float gradient_norm;
    uint32_t num_parameters;
    bool gradient_clipped;
} bio_msg_gradient_computed_t;

/**
 * @brief Encoding request (spike encoding)
 */
typedef struct {
    bio_message_header_t header;
    uint32_t encoding_type;
    uint32_t input_size;
    uint32_t output_neurons;
    float time_window_ms;
    /* Followed by input data */
} bio_msg_encoding_request_t;

#define BIO_ENCODING_RATE       0x01
#define BIO_ENCODING_TEMPORAL   0x02
#define BIO_ENCODING_POPULATION 0x03
#define BIO_ENCODING_RANK_ORDER 0x04

/**
 * @brief Signal routing request
 */
typedef struct {
    bio_message_header_t header;
    uint32_t source_id;
    uint32_t target_id;
    uint32_t signal_type;
    float bandwidth_required;
    uint32_t latency_max_ms;
} bio_msg_route_request_t;

/*=============================================================================
 * TRAINING MESSAGES
 *============================================================================*/

/**
 * @brief Training step request
 */
typedef struct {
    bio_message_header_t header;
    uint32_t batch_id;
    uint32_t batch_size;
    float learning_rate;
    uint32_t optimizer_type;
} bio_msg_training_step_t;

/**
 * @brief Loss computation result
 */
typedef struct {
    bio_message_header_t header;
    uint32_t batch_id;
    float loss_value;
    float loss_gradient;
    uint32_t loss_type;
} bio_msg_loss_computed_t;

/**
 * @brief Checkpoint request
 */
typedef struct {
    bio_message_header_t header;
    uint32_t checkpoint_id;
    char path[256];
    bool include_optimizer_state;
    bool compress;
} bio_msg_checkpoint_request_t;

/**
 * @brief Optimizer step notification
 */
typedef struct {
    bio_message_header_t header;
    uint32_t step_number;           /**< Current step number */
    float learning_rate;            /**< Current learning rate */
    float gradient_norm;            /**< Gradient L2 norm */
    uint32_t weight_updates;        /**< Number of weights updated */
} bio_msg_optimizer_step_t;

/**
 * @brief Generic training metric
 */
typedef struct {
    bio_message_header_t header;
    uint32_t metric_type;           /**< Type of metric (0=loss, 1=accuracy, 2=grad_norm, 3=lr) */
    float metric_value;             /**< Metric value */
    uint32_t step_number;           /**< Associated step */
    char metric_name[32];           /**< Optional name */
} bio_msg_training_metric_t;

/*=============================================================================
 * SYSTEM MESSAGES
 *============================================================================*/

/**
 * @brief Health check request
 */
typedef struct {
    bio_message_header_t header;
    uint32_t check_flags;
} bio_msg_health_check_t;

#define BIO_HEALTH_CHECK_MEMORY     (1 << 0)
#define BIO_HEALTH_CHECK_CPU        (1 << 1)
#define BIO_HEALTH_CHECK_THREADS    (1 << 2)
#define BIO_HEALTH_CHECK_QUEUES     (1 << 3)
#define BIO_HEALTH_CHECK_ALL        0xFFFFFFFF

/**
 * @brief Health check response
 */
typedef struct {
    bio_message_header_t header;
    bool healthy;
    float memory_usage_percent;
    float cpu_usage_percent;
    uint32_t active_threads;
    uint32_t pending_messages;
    float avg_latency_ms;
} bio_msg_health_response_t;

/**
 * @brief Error report
 */
typedef struct {
    bio_message_header_t header;
    nimcp_error_t error_code;
    uint32_t severity;
    uint32_t source_line;
    char source_file[64];
    char message[256];
} bio_msg_error_report_t;

#define BIO_ERROR_SEVERITY_DEBUG    0
#define BIO_ERROR_SEVERITY_INFO     1
#define BIO_ERROR_SEVERITY_WARNING  2
#define BIO_ERROR_SEVERITY_ERROR    3
#define BIO_ERROR_SEVERITY_CRITICAL 4

/**
 * @brief Brain probe data broadcast message
 *
 * Contains comprehensive brain metrics for decoupled monitoring.
 * Brain module broadcasts this via BIO_MSG_BRAIN_PROBE_DATA.
 * Metrics module (or any subscriber) handles independently.
 * Supports multiple concurrent brains via brain_id field.
 */
typedef struct {
    bio_message_header_t header;
    uint64_t brain_id;               /**< Unique brain instance ID (allows multiple probes) */
    char task_name[64];              /**< Brain name */
    uint32_t size;                   /**< Size preset (cast to nimcp_brain_size_t) */
    uint32_t task;                   /**< Task type (cast to nimcp_brain_task_t) */
    uint32_t num_neurons;            /**< Total neurons */
    uint32_t num_synapses;           /**< Total synapses */
    uint32_t num_active_synapses;    /**< Non-pruned synapses */
    uint64_t total_inferences;       /**< Total inference count */
    uint64_t total_learning_steps;   /**< Total learning steps */
    float avg_sparsity;              /**< Average sparsity (0.0-1.0) */
    float avg_inference_time_us;     /**< Average inference time (microseconds) */
    float current_learning_rate;     /**< Current learning rate */
    float accuracy;                  /**< Validation accuracy (0.0-1.0) */
    uint64_t memory_bytes;           /**< Memory usage in bytes */
    uint32_t num_inputs;             /**< Number of inputs */
    uint32_t num_outputs;            /**< Number of outputs */
    bool is_cow_clone;               /**< True if this brain is a COW clone */
    uint32_t cow_ref_count;          /**< Reference count for shared data */
    uint64_t cow_shared_bytes;       /**< Bytes shared via COW */
    uint64_t cow_private_bytes;      /**< Bytes private to this brain */
} bio_msg_brain_probe_data_t;

/**
 * @brief Cognitive output rubric broadcast message
 *
 * Two-tier quality evaluation of brain_decide() output.
 * Broadcast via BIO_MSG_RUBRIC_DATA for decoupled monitoring.
 */
typedef struct {
    bio_message_header_t header;
    uint64_t brain_id;               /**< Unique brain instance ID */

    /* Tier 1: Structural */
    float internal_consistency;
    float confidence_calibration;
    float completeness;
    float reasoning_chain_quality;
    float epistemic_quality;
    float ethical_alignment;
    float tier1_score;

    /* Tier 2: Qualitative */
    float originality;
    float integration_depth;
    float communication_clarity;
    float engagement_quality;
    float empathetic_accuracy;
    float information_density;
    float tier2_score;

    /* Overall */
    float overall_score;
    char  grade;
    char  grade_modifier;
    uint32_t subsystems_available;
    uint64_t evaluation_time_us;
} bio_msg_rubric_data_t;

/*=============================================================================
 * PERCEPTION MESSAGES
 *============================================================================*/

/**
 * @brief Visual feature detection notification
 */
typedef struct {
    bio_message_header_t header;
    uint32_t feature_id;            /**< Detected feature type */
    float x_position;               /**< X position in visual field (normalized) */
    float y_position;               /**< Y position in visual field (normalized) */
    float confidence;               /**< Detection confidence (0-1) */
    float salience;                 /**< Feature salience (0-1) */
    uint32_t layer;                 /**< Visual processing layer (V1, V2, etc.) */
} bio_msg_visual_feature_detected_t;

/**
 * @brief Visual attention shift request
 */
typedef struct {
    bio_message_header_t header;
    float target_x;                 /**< Target X position */
    float target_y;                 /**< Target Y position */
    float urgency;                  /**< Shift urgency (0-1) */
    uint32_t reason;                /**< Reason for attention shift */
} bio_msg_visual_attention_shift_t;

/**
 * @brief Audio feature detection notification
 */
typedef struct {
    bio_message_header_t header;
    uint32_t feature_id;            /**< Feature type (formant, pitch, etc.) */
    float frequency_hz;             /**< Center frequency */
    float amplitude;                /**< Normalized amplitude */
    float onset_time_ms;            /**< Feature onset time */
    float duration_ms;              /**< Feature duration */
    uint8_t channel;                /**< Audio channel (left=0, right=1) */
} bio_msg_audio_feature_detected_t;

/**
 * @brief Phoneme recognition notification
 */
typedef struct {
    bio_message_header_t header;
    uint8_t phoneme_id;             /**< IPA phoneme identifier */
    char phoneme_symbol[8];         /**< IPA symbol (UTF-8) */
    float confidence;               /**< Recognition confidence */
    float onset_time_ms;            /**< Phoneme onset in audio stream */
    float duration_ms;              /**< Phoneme duration */
    float formants[4];              /**< Formant frequencies (F1-F4) */
} bio_msg_phoneme_recognized_t;

/**
 * @brief Word recognition notification
 */
typedef struct {
    bio_message_header_t header;
    uint32_t word_id;               /**< Lexicon word ID */
    char word[32];                  /**< Recognized word string */
    float confidence;               /**< Recognition confidence */
    float onset_time_ms;            /**< Word onset time */
    float duration_ms;              /**< Word duration */
    uint8_t phoneme_count;          /**< Number of phonemes */
    uint8_t phonemes[16];           /**< Phoneme sequence */
} bio_msg_word_recognized_t;

/**
 * @brief Multimodal binding event
 */
typedef struct {
    bio_message_header_t header;
    uint32_t binding_id;            /**< Unique binding identifier */
    uint32_t visual_feature_id;     /**< Associated visual feature */
    uint32_t audio_feature_id;      /**< Associated audio feature */
    float binding_strength;         /**< Binding strength (0-1) */
    float temporal_alignment;       /**< Temporal alignment quality */
} bio_msg_multimodal_binding_t;

/*=============================================================================
 * LANGUAGE PRODUCTION MESSAGES (Broca's Region)
 *============================================================================*/

/**
 * @brief Lexical access request (word lookup)
 */
typedef struct {
    bio_message_header_t header;
    uint32_t word_id;               /**< Word ID (0 = use string) */
    char word[32];                  /**< Word to look up */
    uint8_t pos_hint;               /**< Part of speech hint */
    uint32_t context_id;            /**< Syntactic context */
} bio_msg_lexical_access_request_t;

/**
 * @brief Lexical access response
 */
typedef struct {
    bio_message_header_t header;
    uint32_t word_id;               /**< Found word ID */
    bool found;                     /**< Whether word was found */
    uint8_t phonemes[16];           /**< Phoneme sequence */
    uint8_t phoneme_count;          /**< Number of phonemes */
    uint8_t pos;                    /**< Part of speech */
    float frequency;                /**< Usage frequency */
    float activation;               /**< Lexical activation level */
} bio_msg_lexical_access_response_t;

/**
 * @brief Syntax parsing request
 */
typedef struct {
    bio_message_header_t header;
    uint32_t word_ids[16];          /**< Word sequence to parse */
    uint8_t word_count;             /**< Number of words */
    uint8_t parse_mode;             /**< Parse mode (incremental/full) */
} bio_msg_syntax_parse_request_t;

/**
 * @brief Syntax parse result
 */
typedef struct {
    bio_message_header_t header;
    bool valid;                     /**< Parse successful */
    uint8_t structure_type;         /**< Sentence structure type */
    uint8_t constituent_count;      /**< Number of constituents */
    float complexity;               /**< Syntactic complexity score */
    uint8_t error_position;         /**< Position of error (if any) */
    uint8_t error_type;             /**< Type of syntax error */
} bio_msg_syntax_parse_result_t;

/**
 * @brief Phonological encoding request
 */
typedef struct {
    bio_message_header_t header;
    uint8_t phonemes[32];           /**< Input phoneme sequence */
    uint8_t phoneme_count;          /**< Number of phonemes */
    float speaking_rate;            /**< Target speaking rate */
    bool enable_coarticulation;     /**< Apply coarticulation */
    bool enable_prosody;            /**< Apply prosodic patterns */
} bio_msg_phonological_encode_request_t;

/**
 * @brief Phonological encoding result
 */
typedef struct {
    bio_message_header_t header;
    uint8_t syllables[16];          /**< Syllable structure */
    uint8_t syllable_count;         /**< Number of syllables */
    float durations[32];            /**< Phoneme durations (ms) */
    float pitches[32];              /**< Pitch contour (F0) */
    float intensities[32];          /**< Intensity contour */
    bool success;                   /**< Encoding successful */
} bio_msg_phonological_encode_result_t;

/**
 * @brief Motor command request
 */
typedef struct {
    bio_message_header_t header;
    uint8_t phoneme;                /**< Target phoneme */
    float duration_ms;              /**< Target duration */
    float pitch_hz;                 /**< Target pitch */
    float intensity;                /**< Target intensity */
    uint8_t coarticulation_context; /**< Surrounding phoneme context */
} bio_msg_motor_command_request_t;

/**
 * @brief Motor command result (articulator positions)
 */
typedef struct {
    bio_message_header_t header;
    float lip_aperture;             /**< Lip opening (0-1) */
    float lip_protrusion;           /**< Lip protrusion (0-1) */
    float tongue_height;            /**< Tongue height (0-1) */
    float tongue_advance;           /**< Tongue frontness (0-1) */
    float jaw_opening;              /**< Jaw opening (0-1) */
    float velum_opening;            /**< Velum opening for nasality (0-1) */
    float larynx_tension;           /**< Vocal fold tension (0-1) */
    float timestamp_ms;             /**< Command timestamp */
} bio_msg_motor_command_result_t;

/**
 * @brief Full utterance production request
 */
typedef struct {
    bio_message_header_t header;
    uint32_t word_ids[16];          /**< Words to produce */
    uint8_t word_count;             /**< Number of words */
    float speaking_rate;            /**< Target speaking rate */
    uint8_t emotion;                /**< Emotional coloring */
    uint8_t emphasis_mask;          /**< Word emphasis flags */
} bio_msg_utterance_production_request_t;

/**
 * @brief Utterance production complete notification
 */
typedef struct {
    bio_message_header_t header;
    uint32_t utterance_id;          /**< Unique utterance identifier */
    bool success;                   /**< Production successful */
    float duration_ms;              /**< Total duration */
    uint32_t command_count;         /**< Number of motor commands */
    float avg_confidence;           /**< Average processing confidence */
} bio_msg_utterance_production_complete_t;

/**
 * @brief Speech feedback (for self-monitoring)
 */
typedef struct {
    bio_message_header_t header;
    uint32_t utterance_id;          /**< Related utterance */
    uint8_t phoneme_produced;       /**< What was produced */
    uint8_t phoneme_intended;       /**< What was intended */
    float error_magnitude;          /**< Error size */
    bool requires_correction;       /**< Needs speech repair */
} bio_msg_speech_feedback_t;

/**
 * @brief Basal ganglia action selection result
 *
 * WHAT: Message sent when basal ganglia completes action selection
 * WHY:  Motor cortex needs to know which action to execute and with what vigor
 * HOW:  BG sends selected action ID and parameters to motor cortex
 */
typedef struct {
    bio_message_header_t header;
    uint32_t action_id;             /**< Selected action identifier */
    float vigor;                    /**< Movement vigor/strength [0-1] */
    float confidence;               /**< Selection confidence [0-1] */
    float expected_value;           /**< Expected reward value */
    float urgency;                  /**< Movement urgency [0-1] */
    bool is_habit;                  /**< True if habitual selection */
    uint8_t operating_mode;         /**< 0=goal-directed, 1=habitual, 2=exploratory */
} bio_msg_bg_action_selection_t;

/**
 * @brief Cerebellar motor correction signal
 *
 * WHAT: Message sent by cerebellum to correct ongoing movements
 * WHY:  Real-time error correction for smooth, accurate movements
 * HOW:  Cerebellum sends timing and amplitude corrections to motor cortex
 */
typedef struct {
    bio_message_header_t header;
    uint32_t effector_id;           /**< Target effector/muscle group */
    float timing_correction_ms;     /**< Timing adjustment in milliseconds */
    float amplitude_correction;     /**< Amplitude/gain adjustment [0-2] */
    float predicted_error;          /**< Predicted execution error */
    float coordination_weight;      /**< Multi-joint coordination factor */
    float phase_correction;         /**< Phase alignment correction */
} bio_msg_cerebellar_correction_t;

/*=============================================================================
 * NEURAL LINK PROTOCOL (NLP) NETWORKING MESSAGES
 *============================================================================*/

/**
 * @brief NLP session connected notification
 */
typedef struct {
    bio_message_header_t header;
    uint64_t peer_id;               /**< Connected peer ID */
    uint64_t session_id;            /**< Session identifier */
    uint8_t protocol_mode;          /**< 0=Standard, 1=Tactical, 2=Stealth */
    uint8_t encryption_level;       /**< Encryption strength level */
    bool authenticated;             /**< Whether peer is authenticated */
} bio_msg_nlp_session_connected_t;

/**
 * @brief NLP session disconnected notification
 */
typedef struct {
    bio_message_header_t header;
    uint64_t peer_id;               /**< Disconnected peer ID */
    uint64_t session_id;            /**< Session identifier */
    uint8_t reason;                 /**< Disconnect reason code */
    bool graceful;                  /**< Graceful vs forced disconnect */
} bio_msg_nlp_session_disconnected_t;

/**
 * @brief NLP session state change notification
 */
typedef struct {
    bio_message_header_t header;
    uint64_t peer_id;               /**< Peer ID */
    uint64_t session_id;            /**< Session identifier */
    uint8_t old_state;              /**< Previous session state */
    uint8_t new_state;              /**< New session state */
    uint64_t state_change_time_us;  /**< When state changed */
} bio_msg_nlp_session_state_change_t;

/**
 * @brief NLP message received notification
 */
typedef struct {
    bio_message_header_t header;
    uint64_t peer_id;               /**< Sending peer ID */
    uint32_t message_type;          /**< NLP message type */
    uint32_t message_size;          /**< Size of received message */
    uint32_t sequence_num;          /**< NLP sequence number */
    bool encrypted;                 /**< Was message encrypted */
    bool compressed;                /**< Was message compressed */
} bio_msg_nlp_message_received_t;

/**
 * @brief NLP message sent notification
 */
typedef struct {
    bio_message_header_t header;
    uint64_t peer_id;               /**< Destination peer ID */
    uint32_t message_type;          /**< NLP message type */
    uint32_t message_size;          /**< Size of sent message */
    uint32_t sequence_num;          /**< NLP sequence number */
    bool encrypted;                 /**< Was message encrypted */
    bool reliable;                  /**< Requires acknowledgment */
} bio_msg_nlp_message_sent_t;

/**
 * @brief NLP crypto operation request
 */
typedef struct {
    bio_message_header_t header;
    uint32_t operation_id;          /**< Operation identifier */
    uint32_t data_size;             /**< Size of data to process */
    uint8_t algorithm;              /**< Crypto algorithm (AES-GCM, ChaCha20, etc.) */
    uint8_t key_id;                 /**< Key identifier */
    bool async;                     /**< Asynchronous operation */
} bio_msg_nlp_crypto_request_t;

/**
 * @brief NLP crypto operation complete notification
 */
typedef struct {
    bio_message_header_t header;
    uint32_t operation_id;          /**< Completed operation ID */
    uint32_t output_size;           /**< Size of output data */
    bool success;                   /**< Operation successful */
    uint8_t error_code;             /**< Error code if failed */
    float processing_time_us;       /**< Time taken */
} bio_msg_nlp_crypto_complete_t;

/**
 * @brief NLP crypto error notification
 */
typedef struct {
    bio_message_header_t header;
    uint64_t peer_id;               /**< Related peer (if applicable) */
    uint32_t operation_id;          /**< Failed operation ID */
    uint8_t error_code;             /**< Crypto error code */
    char error_message[64];         /**< Error description */
    bool is_security_violation;     /**< Possible attack detected */
} bio_msg_nlp_crypto_error_t;

/**
 * @brief NLP compression request/complete notification
 */
typedef struct {
    bio_message_header_t header;
    uint32_t operation_id;          /**< Operation identifier */
    uint32_t input_size;            /**< Original size */
    uint32_t output_size;           /**< Compressed/decompressed size */
    uint8_t algorithm;              /**< Compression algorithm */
    float compression_ratio;        /**< Achieved ratio */
    bool success;                   /**< Operation successful */
} bio_msg_nlp_compression_t;

/**
 * @brief NLP protocol mode change notification
 */
typedef struct {
    bio_message_header_t header;
    uint8_t old_mode;               /**< Previous mode (0=Standard, 1=Tactical, 2=Stealth) */
    uint8_t new_mode;               /**< New mode */
    uint64_t peer_id;               /**< Affected peer (0 = all) */
    uint32_t reason;                /**< Reason for mode change */
} bio_msg_nlp_mode_change_t;

/**
 * @brief NLP neural encoding/decoding request
 */
typedef struct {
    bio_message_header_t header;
    uint32_t context_id;            /**< Neural language context ID */
    uint32_t data_size;             /**< Data size to encode/decode */
    uint8_t encoding_type;          /**< Encoding type */
    bool bidirectional;             /**< Two-way encoding */
} bio_msg_nlp_neural_request_t;

/**
 * @brief NLP neural encoding/decoding complete
 */
typedef struct {
    bio_message_header_t header;
    uint32_t context_id;            /**< Neural language context ID */
    uint32_t output_size;           /**< Output size */
    float confidence;               /**< Encoding confidence */
    bool success;                   /**< Operation successful */
    uint32_t neurons_activated;     /**< Neural representation size */
} bio_msg_nlp_neural_complete_t;

/**
 * @brief NLP protocol bridge event
 */
typedef struct {
    bio_message_header_t header;
    uint32_t bridge_id;             /**< Bridge identifier */
    uint8_t event_type;             /**< 0=created, 1=message, 2=destroyed */
    uint32_t source_protocol;       /**< Source protocol ID */
    uint32_t target_protocol;       /**< Target protocol ID */
    uint32_t messages_bridged;      /**< Messages bridged count */
} bio_msg_nlp_bridge_event_t;

/**
 * @brief NLP general error notification
 */
typedef struct {
    bio_message_header_t header;
    uint64_t peer_id;               /**< Related peer (0 if N/A) */
    uint32_t error_code;            /**< Error code */
    uint8_t severity;               /**< 0=info, 1=warning, 2=error, 3=critical */
    char module_name[32];           /**< Module that generated error */
    char error_message[128];        /**< Error description */
} bio_msg_nlp_error_t;

/*=============================================================================
 * SWARM LOGIC MESSAGES
 *============================================================================*/

/**
 * @brief Quorum logic validation result
 */
typedef struct {
    bio_message_header_t header;
    uint32_t quorum_id;             /**< Quorum system ID */
    uint32_t decision_id;           /**< Decision being validated */
    uint32_t gate_type;             /**< Logic gate used (AND/OR/IMPLIES/XOR) */
    float vote_fraction;            /**< Fraction of agents voting */
    float confidence;               /**< Confidence in validation */
    bool validation_passed;         /**< Whether validation passed */
    uint32_t contradicting_count;   /**< Number of contradicting agents */
    uint32_t winning_signal;        /**< Winning signal type */
} bio_msg_swarm_quorum_logic_result_t;

/**
 * @brief Quorum logic validation failure notification
 */
typedef struct {
    bio_message_header_t header;
    uint32_t quorum_id;             /**< Quorum system ID */
    uint32_t decision_id;           /**< Failed decision ID */
    uint32_t failure_reason;        /**< Reason for failure */
    uint32_t contradicting_agents[256]; /**< IDs of contradicting agents */
    uint32_t contradiction_count;   /**< Number of contradictions */
    char failure_message[128];      /**< Failure description */
} bio_msg_swarm_quorum_logic_failure_t;

/**
 * @brief Immune threat logic evaluation result
 */
typedef struct {
    bio_message_header_t header;
    uint32_t immune_id;             /**< Immune system ID */
    uint32_t threat_rule_id;        /**< Threat rule that triggered */
    uint32_t gate_type;             /**< Logic gate used */
    uint32_t num_sources;           /**< Number of signal sources */
    float threat_score;             /**< Computed threat score [0,1] */
    float confidence_threshold;     /**< Threshold for detection */
    bool threat_detected;           /**< Whether threat was detected */
    uint32_t threat_type;           /**< Type of threat */
} bio_msg_swarm_immune_threat_logic_t;

/**
 * @brief Immune logic-based response notification
 */
typedef struct {
    bio_message_header_t header;
    uint32_t immune_id;             /**< Immune system ID */
    uint32_t threat_id;             /**< Threat being responded to */
    uint32_t response_id;           /**< Response strategy ID */
    uint32_t response_logic;        /**< Logic gate used for response */
    uint32_t response_type;         /**< Response action */
    float intensity;                /**< Response intensity [0,1] */
    bool requires_coordination;     /**< Multi-agent coordination needed */
    uint32_t severity;              /**< Threat severity */
} bio_msg_swarm_immune_logic_response_t;

/*=============================================================================
 * SWARM-COGNITIVE INTEGRATION MESSAGES
 *============================================================================*/

/**
 * @brief Swarm consensus reached notification
 *
 * WHAT: Notification that swarm consensus has been reached
 * WHY:  Executive needs to know when swarm agrees on action
 * HOW:  Broadcast from consensus system with vote results
 */
typedef struct {
    bio_message_header_t header;
    uint32_t proposal_id;           /**< Consensus proposal ID */
    uint32_t decision_id;           /**< Executive decision ID (if applicable) */
    bool passed;                    /**< Did consensus pass */
    float weighted_agreement;       /**< Confidence-weighted agreement [0,1] */
    uint32_t agree_count;           /**< Number of agree votes */
    uint32_t disagree_count;        /**< Number of disagree votes */
    uint32_t total_voters;          /**< Total participants */
    swarm_vote_topic_t topic;       /**< Vote topic */
    float decision_confidence;      /**< Overall decision confidence */
} bio_msg_swarm_consensus_reached_t;

/**
 * @brief Swarm consensus request
 *
 * WHAT: Request swarm consensus on executive decision
 * WHY:  Executive wants distributed validation before action
 * HOW:  Executive broadcasts decision for swarm voting
 */
typedef struct {
    bio_message_header_t header;
    uint32_t decision_id;           /**< Executive decision ID */
    uint32_t task_id;               /**< Associated task ID */
    swarm_vote_topic_t topic;       /**< Decision topic */
    float urgency;                  /**< Decision urgency [0,1] */
    float proposal_values[4];       /**< Topic-specific values */
    uint32_t quorum_required;       /**< Minimum voters needed */
    float threshold;                /**< Agreement threshold [0,1] */
    uint64_t deadline_ms;           /**< Voting deadline */
    char decision_context[128];     /**< Human-readable context */
} bio_msg_swarm_consensus_request_t;

/**
 * @brief Swarm signal update
 *
 * WHAT: Update from swarm signal aggregation
 * WHY:  Salience needs swarm-level activity for aggregation
 * HOW:  Broadcast from swarm signal system
 */
typedef struct {
    bio_message_header_t header;
    uint32_t agent_id;              /**< Source agent ID */
    uint32_t signal_type;           /**< Signal category */
    float signal_strength;          /**< Signal strength [0,1] */
    float propagation_count;        /**< Number of hops */
    float collective_intensity;     /**< Aggregated swarm intensity */
    uint64_t timestamp_ms;          /**< Signal timestamp */
    float features[8];              /**< Signal feature vector */
} bio_msg_swarm_signal_update_t;

/**
 * @brief Swarm salience aggregate
 *
 * WHAT: Aggregated salience from multiple swarm agents
 * WHY:  Combine individual salience into collective assessment
 * HOW:  Weighted average from swarm consensus
 */
typedef struct {
    bio_message_header_t header;
    uint32_t stimulus_id;           /**< Stimulus being evaluated */
    uint32_t agent_count;           /**< Number of agents contributing */
    float avg_salience;             /**< Average salience score [0,1] */
    float avg_novelty;              /**< Average novelty [0,1] */
    float avg_surprise;             /**< Average surprise [0,1] */
    float avg_urgency;              /**< Average urgency [0,1] */
    float consensus_confidence;     /**< Confidence in consensus [0,1] */
    float variance;                 /**< Variance across agents */
    bool high_agreement;            /**< Low variance indicates agreement */
} bio_msg_swarm_salience_aggregate_t;

/**
 * @brief Executive decision broadcast to swarm
 *
 * WHAT: Broadcast executive decision to swarm members
 * WHY:  Coordinate swarm with executive decisions
 * HOW:  Executive broadcasts after making decision
 */
typedef struct {
    bio_message_header_t header;
    uint32_t decision_id;           /**< Decision identifier */
    uint32_t task_id;               /**< Associated task ID */
    uint32_t selected_option;       /**< Selected option index */
    float decision_confidence;      /**< Decision confidence [0,1] */
    bool requires_coordination;     /**< Needs swarm coordination */
    swarm_vote_topic_t topic;       /**< Decision topic */
    char decision_summary[256];     /**< Human-readable summary */
    float priority;                 /**< Decision priority [0,1] */
    uint64_t execution_deadline_ms; /**< Execution deadline */
} bio_msg_executive_decision_broadcast_t;

/*=============================================================================
 * SUBSTRATE BRIDGE MESSAGES
 *============================================================================*/

/**
 * @brief Substrate metabolic modulation broadcast
 *
 * WHAT: Broadcast metabolic modulation effects from substrate bridges
 * WHY:  Cognitive modules need to adjust processing based on ATP/fatigue
 * HOW:  Substrate bridges broadcast effects after update calculations
 */
typedef struct {
    bio_message_header_t header;
    uint16_t bridge_module_id;      /**< Source bridge module ID */
    float processing_capacity;      /**< Processing capacity [0-1] */
    float overall_capacity;         /**< Overall modulation [0-1] */
    float effect_values[4];         /**< Module-specific effect values */
    float atp_level;                /**< Current ATP level [0-1] */
    float fatigue_level;            /**< Current fatigue level [0-1] */
    uint64_t update_count;          /**< Bridge update counter */
    bool critical_low;              /**< ATP critically low flag */
} bio_msg_substrate_modulation_t;

/**
 * @brief Substrate capacity update notification
 */
typedef struct {
    bio_message_header_t header;
    uint16_t bridge_module_id;      /**< Source bridge module ID */
    float old_capacity;             /**< Previous overall capacity */
    float new_capacity;             /**< New overall capacity */
    float delta;                    /**< Change magnitude */
    bool significant_change;        /**< Change exceeds threshold */
} bio_msg_substrate_capacity_update_t;

/**
 * @brief ATP critically low alert
 */
typedef struct {
    bio_message_header_t header;
    uint16_t bridge_module_id;      /**< Source bridge module ID */
    float atp_level;                /**< Current ATP level */
    float threshold;                /**< Critical threshold */
    float min_capacity;             /**< Minimum capacity being enforced */
} bio_msg_substrate_atp_critical_t;

/**
 * @brief Fatigue threshold exceeded alert
 */
typedef struct {
    bio_message_header_t header;
    uint16_t bridge_module_id;      /**< Source bridge module ID */
    float fatigue_level;            /**< Current fatigue level */
    float threshold;                /**< Alert threshold */
    float capacity_reduction;       /**< Amount of capacity reduction */
} bio_msg_substrate_fatigue_alert_t;

/*=============================================================================
 * MENTAL HEALTH GUARDIAN MESSAGES
 *============================================================================*/

/**
 * @brief Guardian status report broadcast
 *
 * Sent periodically or on state changes to notify other modules
 * of guardian status and mental health severity.
 */
typedef struct {
    bio_message_header_t header;
    uint8_t state;                  /**< guardian_state_t */
    uint8_t intervention_level;     /**< guardian_intervention_level_t */
    float overall_severity;         /**< Current overall severity [0-1] */
    int32_t primary_disorder;       /**< Primary detected disorder (-1 if none) */
    uint64_t checks_performed;      /**< Total health checks performed */
    uint64_t interventions_applied; /**< Total interventions applied */
    uint64_t uptime_ms;             /**< Guardian uptime in milliseconds */
} bio_msg_guardian_status_report_t;

/**
 * @brief Guardian intervention notification
 *
 * Sent when an intervention is applied at any level.
 * Allows other modules to react to mental health interventions.
 */
typedef struct {
    bio_message_header_t header;
    uint8_t level;                  /**< Intervention level applied */
    float severity;                 /**< Severity that triggered intervention */
    int32_t disorder;               /**< Disorder being addressed */
    uint32_t flags;                 /**< Intervention flags */
} bio_msg_guardian_intervention_t;

/**
 * @brief Guardian alert (critical condition)
 *
 * Broadcast when quarantine-level intervention is triggered.
 * High-priority alert for all listening modules.
 */
typedef struct {
    bio_message_header_t header;
    float severity;                 /**< Critical severity level */
    int32_t primary_disorder;       /**< Critical disorder detected */
    int32_t secondary_disorder;     /**< Secondary disorder (if any) */
    uint8_t action_taken;           /**< Action already taken by guardian */
    bool immune_notified;           /**< Whether immune system was notified */
    bool quarantine_active;         /**< Whether quarantine is currently active */
} bio_msg_guardian_alert_t;

/**
 * @brief Guardian level changed notification
 *
 * Sent when intervention level transitions (e.g., OBSERVE → ADJUST).
 */
typedef struct {
    bio_message_header_t header;
    uint8_t old_level;              /**< Previous intervention level */
    uint8_t new_level;              /**< New intervention level */
    float severity;                 /**< Severity that caused transition */
    uint64_t timestamp_ms;          /**< Time of transition */
} bio_msg_guardian_level_changed_t;

/**
 * @brief Guardian metrics update
 *
 * Periodic broadcast of guardian metrics for monitoring systems.
 */
typedef struct {
    bio_message_header_t header;
    uint64_t observe_count;         /**< Times at OBSERVE level */
    uint64_t adjust_count;          /**< Times at ADJUST level */
    uint64_t regulate_count;        /**< Times at REGULATE level */
    uint64_t quarantine_count;      /**< Times at QUARANTINE level */
    uint64_t checks_performed;      /**< Total checks */
    uint64_t interventions_applied; /**< Total interventions */
    float avg_severity;             /**< Average severity (rolling) */
    float max_severity;             /**< Maximum severity seen */
} bio_msg_guardian_metrics_update_t;

/*=============================================================================
 * IMAGINATION ENGINE MESSAGE PAYLOADS
 *============================================================================*/

/**
 * @brief Imagination request message
 *
 * WHAT: Request to begin an imagination scenario
 * WHY:  Modules need to trigger imagination for planning, prediction, creativity
 * HOW:  Sent to imagination engine, returns scenario ID
 */
typedef struct {
    bio_message_header_t header;
    uint32_t mode;                  /**< Imagination mode (imagination_mode_t) */
    float urgency;                  /**< Request urgency [0.0-1.0] */
    uint32_t max_steps;             /**< Maximum simulation steps */
    float vividness_target;         /**< Target vividness level [0.0-1.0] */
    float coherence_threshold;      /**< Minimum coherence required [0.0-1.0] */
    uint32_t goal_type;             /**< Goal type identifier */
    float goal_data[16];            /**< Goal embedding/parameters */
} bio_msg_imagination_request_t;

/**
 * @brief Imagination result message
 *
 * WHAT: Result from completed imagination scenario
 * WHY:  Modules need imagination output for decision-making
 * HOW:  Broadcast when scenario completes or reaches checkpoint
 */
typedef struct {
    bio_message_header_t header;
    uint32_t scenario_id;           /**< Unique scenario identifier */
    uint32_t mode;                  /**< Mode that generated this result */
    uint32_t steps_completed;       /**< Steps executed */
    float vividness;                /**< Achieved vividness [0.0-1.0] */
    float coherence;                /**< Scene coherence [0.0-1.0] */
    float plausibility;             /**< Result plausibility [0.0-1.0] */
    uint32_t latent_dim;            /**< Dimension of latent state */
    float latent_summary[8];        /**< Summary of latent state (first 8 dims) */
} bio_msg_imagination_result_t;

/**
 * @brief Imagination scenario lifecycle notification
 */
typedef struct {
    bio_message_header_t header;
    uint32_t scenario_id;           /**< Scenario identifier */
    uint32_t event_type;            /**< 0=start, 1=step, 2=pause, 3=resume, 4=end */
    uint32_t step_number;           /**< Current step (for step events) */
    float progress;                 /**< Progress [0.0-1.0] */
} bio_msg_imagination_lifecycle_t;

/**
 * @brief Imagination visual/audio content ready
 */
typedef struct {
    bio_message_header_t header;
    uint32_t scenario_id;           /**< Source scenario */
    uint32_t content_type;          /**< 0=visual, 1=audio, 2=multimodal */
    float vividness;                /**< Content vividness [0.0-1.0] */
    uint32_t width;                 /**< Content width (for visual) */
    uint32_t height;                /**< Content height (for visual) */
    uint32_t channels;              /**< Number of channels */
    /* Actual content follows in separate buffer */
} bio_msg_imagination_content_t;

/**
 * @brief Counterfactual reasoning request/result
 */
typedef struct {
    bio_message_header_t header;
    uint32_t scenario_id;           /**< Base scenario ID */
    uint32_t intervention_type;     /**< Type of counterfactual intervention */
    float intervention_params[8];   /**< Intervention parameters */
    float plausibility;             /**< Result plausibility (in response) */
    float divergence;               /**< How much outcome diverges from original */
    bool is_response;               /**< true=response, false=request */
} bio_msg_imagination_counterfactual_t;

/**
 * @brief Prospective simulation request/result
 */
typedef struct {
    bio_message_header_t header;
    uint32_t scenario_id;           /**< Scenario identifier */
    uint32_t horizon_steps;         /**< How far to simulate */
    float time_horizon_ms;          /**< Time horizon in milliseconds */
    float predicted_outcomes[8];    /**< Predicted outcome values */
    float confidence;               /**< Prediction confidence [0.0-1.0] */
    bool is_response;               /**< true=response, false=request */
} bio_msg_imagination_prospective_t;

/**
 * @brief Social/ToM simulation request/result
 */
typedef struct {
    bio_message_header_t header;
    uint32_t agent_id;              /**< Agent to simulate */
    uint32_t scenario_id;           /**< Associated scenario */
    float agent_mental_state[8];    /**< Simulated mental state */
    float prediction_confidence;    /**< Confidence in simulation [0.0-1.0] */
    uint32_t behavior_prediction;   /**< Predicted agent behavior */
    bool is_response;               /**< true=response, false=request */
} bio_msg_imagination_social_t;

/**
 * @brief Memory request for imagination grounding
 */
typedef struct {
    bio_message_header_t header;
    uint32_t scenario_id;           /**< Requesting scenario */
    float query_cue[16];            /**< Memory query cue */
    uint32_t max_memories;          /**< Maximum memories to retrieve */
    float relevance_threshold;      /**< Minimum relevance [0.0-1.0] */
} bio_msg_imagination_memory_request_t;

/**
 * @brief Memory response for imagination
 */
typedef struct {
    bio_message_header_t header;
    uint32_t scenario_id;           /**< Destination scenario */
    uint32_t memories_found;        /**< Number of memories returned */
    float memory_embeddings[4][16]; /**< Up to 4 memory embeddings */
    float relevance[4];             /**< Relevance scores */
} bio_msg_imagination_memory_response_t;

/**
 * @brief Imagination modulation message (from immune/substrate/attention)
 */
typedef struct {
    bio_message_header_t header;
    uint32_t modulation_type;       /**< 0=vividness, 1=capacity, 2=attention */
    float modifier;                 /**< Modulation multiplier [0.0-2.0] */
    float source_level;             /**< Source system level (e.g., inflammation) */
    float secondary_level;          /**< Secondary level (e.g., fatigue) */
} bio_msg_imagination_modulation_t;

/**
 * @brief Imagination goal update notification
 */
typedef struct {
    bio_message_header_t header;
    uint32_t scenario_id;           /**< Affected scenario */
    uint32_t old_goal_type;         /**< Previous goal type */
    uint32_t new_goal_type;         /**< New goal type */
    float new_goal_params[8];       /**< New goal parameters */
} bio_msg_imagination_goal_update_t;

/**
 * @brief Imagination mode change notification
 */
typedef struct {
    bio_message_header_t header;
    uint32_t scenario_id;           /**< Affected scenario */
    uint32_t old_mode;              /**< Previous mode */
    uint32_t new_mode;              /**< New mode */
    float transition_urgency;       /**< How urgent the transition was */
} bio_msg_imagination_mode_change_t;

/**
 * @brief Collective imagination share/insight
 */
typedef struct {
    bio_message_header_t header;
    uint32_t scenario_id;           /**< Scenario identifier */
    uint64_t source_node;           /**< Node that shared (for insight) */
    uint32_t share_scope;           /**< Sharing scope (local/global) */
    float relevance;                /**< Relevance score [0.0-1.0] */
    float scenario_summary[16];     /**< Summary embedding of scenario */
    bool is_share;                  /**< true=share, false=insight received */
} bio_msg_imagination_collective_t;

/*=============================================================================
 * HEALTH SELF-REPAIR BRIDGE MESSAGES
 *============================================================================*/

/**
 * @brief Health self-repair trigger message
 */
typedef struct {
    bio_message_header_t header;
    uint64_t request_id;            /**< Unique repair request ID */
    uint64_t diagnostic_id;         /**< Source diagnostic ID */
    uint32_t error_type;            /**< Error type from diagnostic */
    uint32_t severity;              /**< Diagnostic severity level */
    float confidence;               /**< Repair confidence [0.0-1.0] */
    uint32_t trigger_policy;        /**< Trigger policy that matched */
    bool aggregated;                /**< Part of aggregated batch */
} bio_msg_health_repair_trigger_t;

/**
 * @brief Health self-repair outcome message
 */
typedef struct {
    bio_message_header_t header;
    uint64_t request_id;            /**< Repair request ID */
    uint32_t outcome;               /**< Repair outcome (health_repair_outcome_t) */
    uint64_t duration_ms;           /**< Repair duration */
    bool success;                   /**< Quick success flag */
    char error_message[128];        /**< Error message if failed */
} bio_msg_health_repair_outcome_t;

/**
 * @brief Health self-repair statistics message
 */
typedef struct {
    bio_message_header_t header;
    uint64_t repairs_triggered;     /**< Total repairs triggered */
    uint64_t repairs_succeeded;     /**< Successful repairs */
    uint64_t repairs_failed;        /**< Failed repairs */
    uint64_t rate_limited_count;    /**< Rate-limited requests */
    float success_rate;             /**< Overall success rate */
    float avg_repair_time_ms;       /**< Average repair time */
} bio_msg_health_repair_stats_t;

/**
 * @brief Code immune repair trigger message
 */
typedef struct {
    bio_message_header_t header;
    uint64_t repair_id;             /**< Repair request ID */
    uint32_t signal_type;           /**< Signal type (SIGSEGV, etc.) */
    float severity;                 /**< Antigen severity [0.0-1.0] */
    float confidence;               /**< Pattern confidence [0.0-1.0] */
    uint32_t recurrence_count;      /**< Number of occurrences */
    bool met_threshold;             /**< Met auto-repair threshold */
} bio_msg_code_immune_repair_trigger_t;

/**
 * @brief Code immune repair outcome message
 */
typedef struct {
    bio_message_header_t header;
    uint64_t repair_id;             /**< Repair request ID */
    bool success;                   /**< Repair success */
    bool learning_applied;          /**< B cell learning applied */
    float new_confidence;           /**< Updated pattern confidence */
} bio_msg_code_immune_repair_outcome_t;

/**
 * @brief Self-repair health failure notification
 */
typedef struct {
    bio_message_header_t header;
    uint64_t repair_id;             /**< Failed repair ID */
    uint32_t failure_count;         /**< Consecutive failures */
    uint32_t notification_type;     /**< Type of notification */
    char component[64];             /**< Affected component */
    char reason[128];               /**< Failure reason */
    bool requires_intervention;     /**< Needs manual intervention */
} bio_msg_repair_health_failure_t;

/*=============================================================================
 * MESSAGE UTILITIES
 *============================================================================*/

/**
 * @brief Initialize message header with defaults
 */
static inline void bio_msg_init_header(bio_message_header_t* header,
                                        bio_message_type_t type,
                                        bio_module_id_t source,
                                        bio_module_id_t target,
                                        size_t payload_size) {
    header->type = type;
    header->sequence_id = 0;  /* Set by sender */
    header->source_module = source;
    header->target_module = target;
    header->timestamp_us = 0;  /* Set by sender */
    header->channel = BIO_CHANNEL_DOPAMINE;  /* Default */
    header->payload_size = (uint32_t)payload_size;
    header->flags = 0;
}

/**
 * @brief Get recommended channel for message type
 */
static inline nimcp_bio_channel_type_t bio_msg_recommended_channel(bio_message_type_t type) {
    /* Plasticity/reward messages → Dopamine */
    if (type >= 0x0200 && type < 0x0300) {
        return BIO_CHANNEL_DOPAMINE;
    }
    /* Ethics/slow cognitive → Serotonin */
    if (type == BIO_MSG_ETHICS_EVALUATION_REQUEST ||
        type == BIO_MSG_ETHICS_EVALUATION_RESPONSE ||
        type == BIO_MSG_CONSOLIDATION_TRIGGER) {
        return BIO_CHANNEL_SEROTONIN;
    }
    /* Alerts/salience → Norepinephrine */
    if (type == BIO_MSG_SALIENCE_QUERY ||
        type == BIO_MSG_SALIENCE_RESPONSE ||
        type == BIO_MSG_MICROGLIA_ALERT ||
        type == BIO_MSG_ERROR_REPORT) {
        return BIO_CHANNEL_NOREPINEPHRINE;
    }
    /* Fast queries/attention → Acetylcholine */
    if (type == BIO_MSG_BRAIN_STATE_QUERY ||
        type == BIO_MSG_INTROSPECTION_QUERY ||
        type == BIO_MSG_ATTENTION_SHIFT ||
        type == BIO_MSG_WORKING_MEMORY_RETRIEVE) {
        return BIO_CHANNEL_ACETYLCHOLINE;
    }
    /* Default to dopamine */
    return BIO_CHANNEL_DOPAMINE;
}

/**
 * @brief Get message type name for logging
 */
const char* bio_msg_type_name(bio_message_type_t type);

/**
 * @brief Get module name for logging
 */
const char* bio_module_name(bio_module_id_t module);

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_BIO_MESSAGES_H */
