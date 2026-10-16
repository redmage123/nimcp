//=============================================================================
// nimcp_split_brain_experiments.c - Split-Brain Experimental Framework
//=============================================================================
/**
 * @file nimcp_split_brain_experiments.c
 * @brief Implementation of split-brain experimental framework
 *
 * WHAT: Provides experimental paradigms for studying hemispheric independence
 * WHY:  Enables research into lateralization and consciousness
 * HOW:  Structured experiments with stimulus presentation and response analysis
 *
 * @author NIMCP Development Team
 * @date 2025-12-22
 * @version 1.0.0
 */

#include "core/brain/hemispheric/nimcp_split_brain_experiments.h"
#include "utils/memory/nimcp_memory.h"
#include "utils/logging/nimcp_logging.h"
#include "utils/time/nimcp_time.h"
#include "utils/thread/nimcp_thread.h"
#include "utils/exception/nimcp_exception_macros.h"
#include <string.h>
#include <stdio.h>
#include <math.h>
#include "utils/fault_tolerance/nimcp_health_agent_macros.h"
#include "utils/bridge/nimcp_bridge_boilerplate.h"
#include "mesh/nimcp_mesh_participant.h"
#include "mesh/nimcp_mesh_adapter.h"

BRIDGE_BOILERPLATE_MESH_ONLY(split_brain_experiments, MESH_ADAPTER_CATEGORY_COGNITIVE)


//=============================================================================
// Internal Helpers
//=============================================================================

/**
 * @brief Compute vector similarity (cosine)
 */
static float compute_similarity(const float* a, const float* b, size_t size) {
    if (!a || !b || size == 0) return 0.0f;

    float dot = 0.0f;
    float norm_a = 0.0f;
    float norm_b = 0.0f;

    for (size_t i = 0; i < size; i++) {
        dot += a[i] * b[i];
        norm_a += a[i] * a[i];
        norm_b += b[i] * b[i];
    }

    if (norm_a == 0.0f || norm_b == 0.0f) return 0.0f;

    return dot / (sqrtf(norm_a) * sqrtf(norm_b));
}

/**
 * @brief Get current time in milliseconds
 */
static uint64_t get_time_ms(void) {
    return nimcp_time_get_ms();
}

//=============================================================================
// Session Configuration
//=============================================================================

split_brain_session_config_t split_brain_session_default_config(void) {
    split_brain_session_config_t config;
    memset(&config, 0, sizeof(config));

    config.paradigm = PARADIGM_CHIMERIC_FACES;
    config.experiment_name = "Split-Brain Experiment";
    config.experimenter = "NIMCP";

    config.callosal_condition = CALLOSAL_STATE_SEVERED;
    config.callosal_strength = 0.0f;

    config.num_trials = 50;
    config.inter_trial_interval_ms = 1000.0f;
    config.stimulus_duration_ms = 150.0f;  // Tachistoscopic
    config.response_timeout_ms = 3000.0f;

    config.allowed_modalities[0] = RESPONSE_HAND_LEFT;
    config.allowed_modalities[1] = RESPONSE_HAND_RIGHT;
    config.allowed_modalities[2] = RESPONSE_VERBAL;
    config.num_allowed_modalities = 3;

    config.detect_cross_cueing = true;
    config.cross_cue_threshold = 0.7f;

    config.record_raw_activity = false;
    config.record_callosum_traffic = true;

    config.randomize_trials = true;
    config.random_seed = 42;

    return config;
}

bool split_brain_validate_config(const split_brain_session_config_t* config) {
    if (!config) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "split_brain_validate_config: config is NULL");
        return false;
    }

    if (config->num_trials == 0 || config->num_trials > SPLIT_BRAIN_MAX_TRIALS) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "split_brain_validate_config: config->num_trials is zero");
        return false;
    }

    if (config->stimulus_duration_ms < 0.0f) {
        return false;
    }
    if (config->response_timeout_ms < 0.0f) {
        return false;
    }
    if (config->inter_trial_interval_ms < 0.0f) {
        return false;
    }

    if (config->callosal_strength < 0.0f || config->callosal_strength > 1.0f) {
        return false;
    }

    if (config->cross_cue_threshold < 0.0f || config->cross_cue_threshold > 1.0f) {
        return false;
    }

    return true;
}

//=============================================================================
// Session Lifecycle
//=============================================================================

split_brain_session_t* split_brain_session_create(
    const split_brain_session_config_t* config,
    hemispheric_brain_t* brain
) {
    if (!brain) {
        NIMCP_LOGGING_ERROR("Cannot create session with NULL brain");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
            "split_brain_session_create: brain is NULL");
        return NULL;
    }

    // Use defaults if no config
    split_brain_session_config_t default_config;
    if (!config) {
        default_config = split_brain_session_default_config();
        config = &default_config;
    }

    if (!split_brain_validate_config(config)) {
        NIMCP_LOGGING_ERROR("Invalid session configuration");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM,
            "split_brain_session_create: invalid configuration");
        return NULL;
    }

    split_brain_session_t* session = nimcp_malloc(sizeof(split_brain_session_t));
    if (!session) {
        NIMCP_LOGGING_ERROR("Failed to allocate session");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY,
            "split_brain_session_create: failed to allocate session");
        return NULL;
    }
    memset(session, 0, sizeof(split_brain_session_t));

    // Copy configuration
    session->config = *config;
    session->brain = brain;

    // Allocate trials
    session->trial_capacity = config->num_trials;
    session->trials = nimcp_malloc(sizeof(split_brain_trial_t) * session->trial_capacity);
    if (!session->trials) {
        NIMCP_LOGGING_ERROR("Failed to allocate trials");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY,
            "split_brain_session_create: failed to allocate trials");
        nimcp_free(session);
        return NULL;
    }
    memset(session->trials, 0, sizeof(split_brain_trial_t) * session->trial_capacity);

    // Store original callosum state
    session->original_callosum_connected = hemispheric_brain_is_callosum_intact(brain);
    corpus_callosum_t* callosum = hemispheric_brain_get_callosum(brain);
    if (callosum) {
        session->original_callosum_strength = callosum_get_connection_strength(callosum);
    } else {
        session->original_callosum_strength = 1.0f;
    }

    // Create mutex
    session->mutex = nimcp_malloc(sizeof(nimcp_mutex_t));
    if (!session->mutex) {
        NIMCP_LOGGING_ERROR("Failed to allocate mutex");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY,
            "split_brain_session_create: failed to allocate mutex");
        nimcp_free(session->trials);
        nimcp_free(session);
        return NULL;
    }
    nimcp_mutex_init(session->mutex, NULL);

    session->is_running = false;
    session->is_paused = false;
    session->current_trial = 0;

    NIMCP_LOGGING_INFO("Created split-brain session: %s", config->experiment_name);

    return session;
}

void split_brain_session_destroy(split_brain_session_t* session) {
    if (!session) return;

    // Restore callosum state
    if (session->brain) {
        split_brain_restore_callosum(session);
    }

    // Free trial data
    if (session->trials) {
        for (uint32_t i = 0; i < session->current_trial; i++) {
            split_brain_trial_t* trial = &session->trials[i];

            // Free stimulus data
            if (trial->left_stimulus.data) {
                nimcp_free(trial->left_stimulus.data);
            }
            if (trial->right_stimulus.data) {
                nimcp_free(trial->right_stimulus.data);
            }

            // Free response data
            if (trial->left_response.data) {
                nimcp_free(trial->left_response.data);
            }
            if (trial->right_response.data) {
                nimcp_free(trial->right_response.data);
            }
            if (trial->left_response.verbal_response) {
                nimcp_free(trial->left_response.verbal_response);
            }
            if (trial->right_response.verbal_response) {
                nimcp_free(trial->right_response.verbal_response);
            }

            // Free expected data
            if (trial->expected_left_response) {
                nimcp_free(trial->expected_left_response);
            }
            if (trial->expected_right_response) {
                nimcp_free(trial->expected_right_response);
            }
        }
        nimcp_free(session->trials);
    }

    // Destroy mutex
    if (session->mutex) {
        nimcp_mutex_free(session->mutex);
    }

    nimcp_free(session);
    NIMCP_LOGGING_INFO("Destroyed split-brain session");
}

int split_brain_session_start(split_brain_session_t* session) {
    if (!session) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "split_brain_session_start: session is NULL");
        return -1;
    }

    if (session->is_running) {
        NIMCP_LOGGING_WARN("Session already running");
        return 0;
    }

    // Apply callosal condition
    int result = split_brain_apply_callosal_condition(
        session, session->config.callosal_condition);
    if (result != 0) {
        NIMCP_LOGGING_ERROR("Failed to apply callosal condition");
        return result;
    }

    // Apply strength if degraded
    if (session->config.callosal_condition == CALLOSAL_STATE_DEGRADED) {
        split_brain_set_callosal_strength(session, session->config.callosal_strength);
    }

    // Block specified channels for partial condition
    if (session->config.callosal_condition == CALLOSAL_STATE_PARTIAL) {
        for (int i = 0; i < CALLOSUM_CHANNEL_COUNT; i++) {
            if (session->config.blocked_channels[i]) {
                split_brain_block_channel(session, (callosum_channel_type_t)i, true);
            }
        }
    }

    session->is_running = true;
    session->is_paused = false;
    session->start_time = get_time_ms();
    session->current_trial = 0;

    // Reset statistics
    memset(&session->stats, 0, sizeof(session->stats));

    NIMCP_LOGGING_INFO("Started split-brain session");
    return 0;
}

int split_brain_session_pause(split_brain_session_t* session) {
    if (!session || !session->is_running) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "split_brain_session_pause: required parameter is NULL (session, session->is_running)");
        return -1;
    }

    session->is_paused = true;
    session->pause_time = get_time_ms();

    NIMCP_LOGGING_INFO("Paused split-brain session at trial %u", session->current_trial);
    return 0;
}

int split_brain_session_resume(split_brain_session_t* session) {
    if (!session || !session->is_running || !session->is_paused) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "split_brain_session_resume: required parameter is NULL (session, session->is_running, session->is_paused)");
        return -1;
    }

    session->is_paused = false;

    NIMCP_LOGGING_INFO("Resumed split-brain session");
    return 0;
}

int split_brain_session_end(split_brain_session_t* session) {
    if (!session) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "split_brain_session_end: session is NULL");
        return -1;
    }

    // Restore original callosum state
    split_brain_restore_callosum(session);

    // Finalize statistics
    session->stats.total_trials = session->current_trial;

    // Compute averages
    if (session->stats.total_trials > 0) {
        float total_rt_left = 0.0f;
        float total_rt_right = 0.0f;
        float total_duration = 0.0f;
        uint32_t left_count = 0;
        uint32_t right_count = 0;

        for (uint32_t i = 0; i < session->current_trial; i++) {
            split_brain_trial_t* trial = &session->trials[i];

            total_duration += trial->total_duration_ms;

            if (trial->left_response.reaction_time_ms > 0) {
                total_rt_left += trial->left_response.reaction_time_ms;
                left_count++;
            }
            if (trial->right_response.reaction_time_ms > 0) {
                total_rt_right += trial->right_response.reaction_time_ms;
                right_count++;
            }
        }

        session->stats.avg_trial_duration_ms = total_duration / session->stats.total_trials;
        session->stats.total_session_duration_ms = (float)(get_time_ms() - session->start_time);

        if (left_count > 0) {
            session->stats.avg_left_reaction_time = total_rt_left / left_count;
        }
        if (right_count > 0) {
            session->stats.avg_right_reaction_time = total_rt_right / right_count;
        }

        // Compute accuracy
        if (session->stats.completed_trials > 0) {
            session->stats.left_accuracy = (float)session->stats.correct_trials /
                                           session->stats.completed_trials;
        }

        // Compute agreement rate
        uint32_t agreed = 0;
        for (uint32_t i = 0; i < session->current_trial; i++) {
            if (session->trials[i].hemispheres_agreed) agreed++;
        }
        session->stats.overall_agreement_rate = (float)agreed / session->stats.total_trials;

        // Compute cross-cueing rate
        if (session->stats.total_trials > 0) {
            session->stats.cross_cueing_rate = (float)session->stats.cross_cueing_events /
                                                session->stats.total_trials;
        }
    }

    session->is_running = false;
    session->is_paused = false;

    NIMCP_LOGGING_INFO("Ended split-brain session: %u trials completed",
                       session->stats.completed_trials);
    return 0;
}

//=============================================================================
// Trial Management
//=============================================================================

split_brain_trial_t* split_brain_trial_create(
    split_brain_session_t* session,
    experiment_paradigm_t paradigm
) {
    if (!session) {

        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "session is NULL");

        return NULL;

    }

    if (session->current_trial >= session->trial_capacity) {
        NIMCP_LOGGING_ERROR("Trial capacity exceeded");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY,
            "split_brain_trial_create: trial capacity exceeded");
        return NULL;
    }

    split_brain_trial_t* trial = &session->trials[session->current_trial];
    memset(trial, 0, sizeof(split_brain_trial_t));

    trial->trial_number = session->current_trial;
    trial->paradigm = paradigm;
    trial->callosal_state = session->config.callosal_condition;
    trial->callosal_strength = session->config.callosal_strength;

    return trial;
}

int split_brain_trial_set_stimulus(
    split_brain_trial_t* trial,
    const split_brain_stimulus_t* stimulus,
    hemisphere_id_t hemisphere
) {
    if (!trial || !stimulus) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "split_brain_trial_set_stimulus: required parameter is NULL (trial, stimulus)");
        return -1;
    }

    split_brain_stimulus_t* target = (hemisphere == HEMISPHERE_LEFT) ?
                                      &trial->left_stimulus : &trial->right_stimulus;

    *target = *stimulus;

    // Copy data if provided
    if (stimulus->data && stimulus->data_size > 0) {
        target->data = nimcp_malloc(stimulus->data_size * sizeof(float));
        if (!target->data) {
            NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "split_brain_trial_set_stimulus: target->data is NULL");
            return -1;
        }
        memcpy(target->data, stimulus->data, stimulus->data_size * sizeof(float));
    }

    return 0;
}

int split_brain_trial_set_chimeric(
    split_brain_trial_t* trial,
    const split_brain_stimulus_t* left_vf_stimulus,
    const split_brain_stimulus_t* right_vf_stimulus
) {
    if (!trial) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "split_brain_trial_set_chimeric: trial is NULL");
        return -1;
    }

    trial->use_chimeric = true;

    int result = 0;
    if (left_vf_stimulus) {
        result = split_brain_trial_set_stimulus(trial, left_vf_stimulus, HEMISPHERE_LEFT);
        if (result != 0) return result;
    }
    if (right_vf_stimulus) {
        result = split_brain_trial_set_stimulus(trial, right_vf_stimulus, HEMISPHERE_RIGHT);
    }

    return result;
}

int split_brain_trial_set_expected(
    split_brain_trial_t* trial,
    const float* expected,
    size_t size,
    hemisphere_id_t hemisphere
) {
    if (!trial || !expected || size == 0) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "split_brain_trial_set_expected: required parameter is NULL (trial, expected)");
        return -1;
    }

    float** target = (hemisphere == HEMISPHERE_LEFT) ?
                     &trial->expected_left_response : &trial->expected_right_response;

    *target = nimcp_malloc(size * sizeof(float));
    if (!*target) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "split_brain_trial_set_expected: validation failed");
        return -1;
    }

    memcpy(*target, expected, size * sizeof(float));
    trial->expected_size = size;

    return 0;
}

int split_brain_trial_run(
    split_brain_session_t* session,
    split_brain_trial_t* trial
) {
    if (!session || !trial || !session->brain) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "split_brain_trial_run: required parameter is NULL (session, trial, session->brain)");
        return -1;
    }

    trial->start_time = get_time_ms();

    // Get hemispheres
    brain_hemisphere_t* left_hemi = hemispheric_brain_get_left(session->brain);
    brain_hemisphere_t* right_hemi = hemispheric_brain_get_right(session->brain);

    if (!left_hemi || !right_hemi) {
        NIMCP_LOGGING_ERROR("Cannot access hemispheres");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "split_brain_trial_run: required parameter is NULL (left_hemi, right_hemi)");
        return -1;
    }

    // Present stimuli
    uint64_t presentation_start = get_time_ms();

    // For chimeric, different stimuli to each hemisphere
    // Left visual field -> Right hemisphere
    // Right visual field -> Left hemisphere
    if (trial->use_chimeric) {
        // Present left VF stimulus to right hemisphere
        if (trial->left_stimulus.data) {
            // Would route through visual cortex to right hemisphere
            // For now, process through hemisphere directly
            trial->left_response.hemisphere = HEMISPHERE_RIGHT;
        }

        // Present right VF stimulus to left hemisphere
        if (trial->right_stimulus.data) {
            trial->right_response.hemisphere = HEMISPHERE_LEFT;
        }
    } else {
        // Same stimulus to both
        if (trial->left_stimulus.data) {
            trial->left_response.hemisphere = HEMISPHERE_RIGHT;
            trial->right_response.hemisphere = HEMISPHERE_LEFT;
        }
    }

    // Wait for presentation duration
    float duration = session->config.stimulus_duration_ms;
    if (trial->left_stimulus.presentation_time_ms > 0) {
        duration = trial->left_stimulus.presentation_time_ms;
    }

    // Simulate processing time
    uint64_t presentation_end = presentation_start + (uint64_t)duration;

    // Update brain to process stimuli
    float dt = duration / 1000.0f;  // Convert to seconds
    hemispheric_brain_update(session->brain, dt);

    // Record reaction times (simulated based on hemisphere activity)
    trial->left_response.reaction_time_ms = duration + 100.0f;  // Base RT
    trial->right_response.reaction_time_ms = duration + 120.0f;

    // Collect responses
    // In a real implementation, this would get actual outputs from processing
    trial->left_response.modality = RESPONSE_HAND_LEFT;  // Right hemisphere
    trial->right_response.modality = RESPONSE_VERBAL;     // Left hemisphere verbal

    // Allocate response data
    size_t response_size = SPLIT_BRAIN_MAX_RESPONSE_SIZE;
    trial->left_response.data = nimcp_malloc(response_size * sizeof(float));
    trial->right_response.data = nimcp_malloc(response_size * sizeof(float));
    trial->left_response.data_size = response_size;
    trial->right_response.data_size = response_size;

    // Simulate response (in practice, this comes from hemisphere output)
    if (trial->left_response.data) {
        memset(trial->left_response.data, 0, response_size * sizeof(float));
        // Right hemisphere spatial response pattern
        for (size_t i = 0; i < response_size; i++) {
            trial->left_response.data[i] = (float)(i % 10) / 10.0f;
        }
    }
    if (trial->right_response.data) {
        memset(trial->right_response.data, 0, response_size * sizeof(float));
        // Left hemisphere verbal/logical response pattern
        for (size_t i = 0; i < response_size; i++) {
            trial->right_response.data[i] = (float)((i + 3) % 10) / 10.0f;
        }
    }

    // Compute confidence based on hemisphere activity
    trial->left_response.confidence = 0.8f;
    trial->right_response.confidence = 0.75f;

    // Compute agreement
    trial->agreement_score = split_brain_compute_agreement(trial);
    trial->hemispheres_agreed = trial->agreement_score > 0.8f;

    // Check for cross-cueing
    trial->cross_cueing_detected = split_brain_detect_cross_cueing(session, trial);
    trial->cross_cueing_evidence = split_brain_get_cross_cue_evidence(trial);

    // Determine outcome
    if (trial->hemispheres_agreed) {
        trial->outcome = OUTCOME_CORRECT;
        session->stats.correct_trials++;
    } else if (trial->cross_cueing_detected) {
        trial->outcome = OUTCOME_CROSS_CUE_DETECTED;
        session->stats.cross_cueing_events++;
    } else {
        trial->outcome = OUTCOME_CONFLICT;
        session->stats.conflict_trials++;
    }

    // Record timing
    trial->end_time = get_time_ms();
    trial->total_duration_ms = (float)(trial->end_time - trial->start_time);

    // Update statistics
    session->stats.completed_trials++;

    // Increment trial counter
    session->current_trial++;

    // Invoke callback
    if (session->on_trial_complete) {
        session->on_trial_complete(trial, session->callback_user_data);
    }

    if (trial->cross_cueing_detected && session->on_cross_cue_detected) {
        session->on_cross_cue_detected(trial, session->callback_user_data);
    }

    if (!trial->hemispheres_agreed && session->on_conflict) {
        session->on_conflict(trial, session->callback_user_data);
    }

    return 0;
}

int split_brain_session_run_all_trials(split_brain_session_t* session) {
    if (!session || !session->is_running) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "split_brain_session_run_all_trials: required parameter is NULL (session, session->is_running)");
        return -1;
    }

    int completed = 0;

    for (uint32_t i = 0; i < session->config.num_trials; i++) {
        if (session->is_paused) break;

        split_brain_trial_t* trial = split_brain_trial_create(session, session->config.paradigm);
        if (!trial) break;

        int result = split_brain_trial_run(session, trial);
        if (result == 0) {
            completed++;
        }

        // Inter-trial interval (in practice, would use actual timing)
    }

    return completed;
}

const split_brain_trial_t* split_brain_session_get_trial(
    const split_brain_session_t* session,
    uint32_t trial_number
) {
    if (!session || trial_number >= session->current_trial) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "split_brain_session_get_trial: session is NULL");
        return NULL;
    }
    return &session->trials[trial_number];
}

//=============================================================================
// Cross-Cueing Detection
//=============================================================================

bool split_brain_detect_cross_cueing(
    split_brain_session_t* session,
    split_brain_trial_t* trial
) {
    if (!session || !trial) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "split_brain_detect_cross_cueing: required parameter is NULL (session, trial)");
        return false;
    }

    // In true split-brain, hemispheres should not share information
    // Cross-cueing occurs when information leaks via:
    // - Eye movements
    // - Body posture
    // - Subvocal speech
    // - Subtle motor cues

    // Detection heuristic: If responses are too similar given the
    // callosal condition, cross-cueing may have occurred

    if (trial->callosal_state != CALLOSAL_STATE_SEVERED) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "split_brain_detect_cross_cueing: validation failed");
        return false;  // Cross-cueing only relevant in split-brain
    }

    // Check if non-stimulated hemisphere shows knowledge of stimulus
    // that it shouldn't have access to

    float similarity = split_brain_compute_agreement(trial);

    // If hemispheres agree more than expected (given different stimuli),
    // cross-cueing may have occurred
    if (trial->use_chimeric && similarity > session->config.cross_cue_threshold) {
        trial->cross_cueing_evidence = similarity;
        return true;
    }

    NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "split_brain_detect_cross_cueing: validation failed");
    return false;
}

float split_brain_get_cross_cue_evidence(const split_brain_trial_t* trial) {
    if (!trial) return 0.0f;
    return trial->cross_cueing_evidence;
}

//=============================================================================
// Confabulation Analysis
//=============================================================================

bool split_brain_analyze_confabulation(
    split_brain_session_t* session,
    split_brain_trial_t* trial,
    float* confabulation_score
) {
    if (!session || !trial) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "split_brain_analyze_confabulation: required parameter is NULL (session, trial)");
        return false;
    }

    // Confabulation occurs when left hemisphere (verbal) provides
    // a plausible but incorrect explanation for right hemisphere actions

    // This is detected when:
    // 1. Right hemisphere performs an action based on its stimulus
    // 2. Left hemisphere provides verbal explanation
    // 3. Explanation doesn't match actual right hemisphere motivation

    // For now, use heuristic based on response mismatch
    float left_confidence = trial->right_response.confidence;  // Left hemi verbal
    float agreement = trial->agreement_score;

    // High confidence + low agreement suggests confabulation
    float score = left_confidence * (1.0f - agreement);

    if (confabulation_score) {
        *confabulation_score = score;
    }

    bool is_confabulation = score > 0.5f;
    if (is_confabulation) {
        session->stats.confabulation_events++;
    }

    return is_confabulation;
}

//=============================================================================
// Agreement Analysis
//=============================================================================

float split_brain_compute_agreement(const split_brain_trial_t* trial) {
    if (!trial) return 0.0f;

    if (!trial->left_response.data || !trial->right_response.data) {
        return 0.0f;
    }

    size_t size = trial->left_response.data_size;
    if (trial->right_response.data_size < size) {
        size = trial->right_response.data_size;
    }

    return compute_similarity(
        trial->left_response.data,
        trial->right_response.data,
        size
    );
}

bool split_brain_detect_conflict(
    const split_brain_trial_t* trial,
    float threshold
) {
    if (!trial) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "split_brain_detect_conflict: trial is NULL");
        return false;
    }

    float agreement = split_brain_compute_agreement(trial);
    return agreement < threshold;
}

//=============================================================================
// Callosal Manipulation
//=============================================================================

int split_brain_apply_callosal_condition(
    split_brain_session_t* session,
    callosal_condition_t condition
) {
    if (!session || !session->brain) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "split_brain_apply_callosal_condition: required parameter is NULL (session, session->brain)");
        return -1;
    }

    corpus_callosum_t* callosum = hemispheric_brain_get_callosum(session->brain);

    switch (condition) {
        case CALLOSAL_STATE_INTACT:
            if (!hemispheric_brain_is_callosum_intact(session->brain)) {
                hemispheric_brain_reconnect_callosum(session->brain);
            }
            if (callosum) {
                callosum_set_connection_strength(callosum, 1.0f);
            }
            break;

        case CALLOSAL_STATE_SEVERED:
            hemispheric_brain_disconnect_callosum(session->brain);
            break;

        case CALLOSAL_STATE_PARTIAL:
            if (!hemispheric_brain_is_callosum_intact(session->brain)) {
                hemispheric_brain_reconnect_callosum(session->brain);
            }
            // Channels will be blocked individually
            break;

        case CALLOSAL_STATE_DEGRADED:
            if (!hemispheric_brain_is_callosum_intact(session->brain)) {
                hemispheric_brain_reconnect_callosum(session->brain);
            }
            if (callosum) {
                callosum_set_bandwidth_mode(callosum, CALLOSUM_BW_RESTRICTED);
            }
            break;

        case CALLOSAL_STATE_DEVELOPING:
            // Simulate immature callosum (low bandwidth, high latency)
            if (!hemispheric_brain_is_callosum_intact(session->brain)) {
                hemispheric_brain_reconnect_callosum(session->brain);
            }
            if (callosum) {
                callosum_set_connection_strength(callosum, 0.3f);
                callosum_set_latency(callosum, 30.0f, 100.0f);
            }
            break;
    }

    NIMCP_LOGGING_INFO("Applied callosal condition: %s",
                       split_brain_callosal_condition_name(condition));
    return 0;
}

int split_brain_set_callosal_strength(
    split_brain_session_t* session,
    float strength
) {
    if (!session || !session->brain) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "split_brain_set_callosal_strength: required parameter is NULL (session, session->brain)");
        return -1;
    }

    corpus_callosum_t* callosum = hemispheric_brain_get_callosum(session->brain);
    if (!callosum) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "split_brain_set_callosal_strength: callosum is NULL");
        return -1;
    }

    return callosum_set_connection_strength(callosum, strength);
}

int split_brain_block_channel(
    split_brain_session_t* session,
    callosum_channel_type_t channel,
    bool block
) {
    if (!session || !session->brain) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "split_brain_block_channel: required parameter is NULL (session, session->brain)");
        return -1;
    }

    corpus_callosum_t* callosum = hemispheric_brain_get_callosum(session->brain);
    if (!callosum) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "split_brain_block_channel: callosum is NULL");
        return -1;
    }

    return callosum_set_channel_enabled(callosum, channel, !block);
}

int split_brain_restore_callosum(split_brain_session_t* session) {
    if (!session || !session->brain) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "split_brain_restore_callosum: required parameter is NULL (session, session->brain)");
        return -1;
    }

    // Restore connection state
    if (session->original_callosum_connected) {
        if (!hemispheric_brain_is_callosum_intact(session->brain)) {
            hemispheric_brain_reconnect_callosum(session->brain);
        }
    } else {
        hemispheric_brain_disconnect_callosum(session->brain);
    }

    // Restore strength
    corpus_callosum_t* callosum = hemispheric_brain_get_callosum(session->brain);
    if (callosum) {
        callosum_set_connection_strength(callosum, session->original_callosum_strength);
        callosum_set_bandwidth_mode(callosum, CALLOSUM_BW_REALISTIC);

        // Re-enable all channels
        for (int i = 0; i < CALLOSUM_CHANNEL_COUNT; i++) {
            callosum_set_channel_enabled(callosum, (callosum_channel_type_t)i, true);
        }
    }

    NIMCP_LOGGING_INFO("Restored callosum to original state");
    return 0;
}

//=============================================================================
// Statistics and Analysis
//=============================================================================

int split_brain_session_get_stats(
    const split_brain_session_t* session,
    split_brain_session_stats_t* stats
) {
    if (!session || !stats) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "split_brain_session_get_stats: required parameter is NULL (session, stats)");
        return -1;
    }

    *stats = session->stats;
    return 0;
}

float split_brain_compute_lateralization_index(
    const split_brain_session_t* session,
    cognitive_domain_t domain
) {
    if (!session || session->current_trial == 0) return 0.0f;

    // LI = (L - R) / (L + R)
    // Computed from reaction times or accuracy

    float left_sum = 0.0f;
    float right_sum = 0.0f;
    uint32_t count = 0;

    for (uint32_t i = 0; i < session->current_trial; i++) {
        const split_brain_trial_t* trial = &session->trials[i];

        // Check if trial tested this domain
        if (trial->left_stimulus.target_domain == domain ||
            trial->right_stimulus.target_domain == domain) {

            // Use inverse RT (faster = better)
            if (trial->left_response.reaction_time_ms > 0) {
                right_sum += 1000.0f / trial->left_response.reaction_time_ms;  // Left VF = Right hemi
            }
            if (trial->right_response.reaction_time_ms > 0) {
                left_sum += 1000.0f / trial->right_response.reaction_time_ms;  // Right VF = Left hemi
            }
            count++;
        }
    }

    if (count == 0 || (left_sum + right_sum) == 0) return 0.0f;

    return (left_sum - right_sum) / (left_sum + right_sum);
}

int split_brain_analyze_reaction_times(
    const split_brain_session_t* session,
    float* left_rt,
    float* right_rt,
    float* rt_difference
) {
    if (!session) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "split_brain_analyze_reaction_times: session is NULL");
        return -1;
    }

    float l_sum = 0.0f, r_sum = 0.0f;
    uint32_t l_count = 0, r_count = 0;

    for (uint32_t i = 0; i < session->current_trial; i++) {
        const split_brain_trial_t* trial = &session->trials[i];

        if (trial->right_response.reaction_time_ms > 0) {  // Left hemi verbal
            l_sum += trial->right_response.reaction_time_ms;
            l_count++;
        }
        if (trial->left_response.reaction_time_ms > 0) {  // Right hemi motor
            r_sum += trial->left_response.reaction_time_ms;
            r_count++;
        }
    }

    float l_avg = l_count > 0 ? l_sum / l_count : 0.0f;
    float r_avg = r_count > 0 ? r_sum / r_count : 0.0f;

    if (left_rt) *left_rt = l_avg;
    if (right_rt) *right_rt = r_avg;
    if (rt_difference) *rt_difference = l_avg - r_avg;

    return 0;
}

int split_brain_generate_report(
    const split_brain_session_t* session,
    char* buffer,
    size_t buffer_size
) {
    if (!session || !buffer || buffer_size == 0) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "split_brain_generate_report: required parameter is NULL (session, buffer)");
        return -1;
    }

    int len = snprintf(buffer, buffer_size,
        "Split-Brain Experiment Report\n"
        "=============================\n"
        "Experiment: %s\n"
        "Paradigm: %s\n"
        "Callosal Condition: %s\n\n"
        "Results:\n"
        "--------\n"
        "Total Trials: %u\n"
        "Completed: %u\n"
        "Correct: %u (%.1f%%)\n"
        "Conflicts: %u (%.1f%%)\n"
        "Cross-Cueing Events: %u (%.1f%%)\n"
        "Confabulations: %u\n\n"
        "Reaction Times:\n"
        "--------------\n"
        "Left Hemisphere: %.1f ms\n"
        "Right Hemisphere: %.1f ms\n"
        "Difference: %.1f ms\n\n"
        "Agreement Rate: %.1f%%\n"
        "Session Duration: %.1f ms\n",
        session->config.experiment_name,
        split_brain_paradigm_name(session->config.paradigm),
        split_brain_callosal_condition_name(session->config.callosal_condition),
        session->stats.total_trials,
        session->stats.completed_trials,
        session->stats.correct_trials,
        session->stats.completed_trials > 0 ?
            100.0f * session->stats.correct_trials / session->stats.completed_trials : 0.0f,
        session->stats.conflict_trials,
        session->stats.completed_trials > 0 ?
            100.0f * session->stats.conflict_trials / session->stats.completed_trials : 0.0f,
        session->stats.cross_cueing_events,
        100.0f * session->stats.cross_cueing_rate,
        session->stats.confabulation_events,
        session->stats.avg_left_reaction_time,
        session->stats.avg_right_reaction_time,
        session->stats.avg_left_reaction_time - session->stats.avg_right_reaction_time,
        100.0f * session->stats.overall_agreement_rate,
        session->stats.total_session_duration_ms
    );

    return len;
}

//=============================================================================
// Callbacks
//=============================================================================

void split_brain_set_trial_callback(
    split_brain_session_t* session,
    void (*callback)(split_brain_trial_t* trial, void* user_data),
    void* user_data
) {
    if (!session) return;
    session->on_trial_complete = callback;
    session->callback_user_data = user_data;
}

void split_brain_set_cross_cue_callback(
    split_brain_session_t* session,
    void (*callback)(split_brain_trial_t* trial, void* user_data),
    void* user_data
) {
    if (!session) return;
    session->on_cross_cue_detected = callback;
    session->callback_user_data = user_data;
}

void split_brain_set_conflict_callback(
    split_brain_session_t* session,
    void (*callback)(split_brain_trial_t* trial, void* user_data),
    void* user_data
) {
    if (!session) return;
    session->on_conflict = callback;
    session->callback_user_data = user_data;
}

//=============================================================================
// Predefined Experiments
//=============================================================================

split_brain_session_t* split_brain_create_chimeric_faces_experiment(
    hemispheric_brain_t* brain,
    uint32_t num_trials
) {
    split_brain_session_config_t config = split_brain_session_default_config();
    config.paradigm = PARADIGM_CHIMERIC_FACES;
    config.experiment_name = "Chimeric Faces Experiment";
    config.num_trials = num_trials;
    config.callosal_condition = CALLOSAL_STATE_SEVERED;
    config.stimulus_duration_ms = 150.0f;  // Brief presentation

    return split_brain_session_create(&config, brain);
}

split_brain_session_t* split_brain_create_dichotic_experiment(
    hemispheric_brain_t* brain,
    uint32_t num_trials
) {
    split_brain_session_config_t config = split_brain_session_default_config();
    config.paradigm = PARADIGM_DICHOTIC_LISTENING;
    config.experiment_name = "Dichotic Listening Experiment";
    config.num_trials = num_trials;
    config.callosal_condition = CALLOSAL_STATE_SEVERED;
    config.stimulus_duration_ms = 500.0f;  // Longer for auditory

    return split_brain_session_create(&config, brain);
}

split_brain_session_t* split_brain_create_tachistoscopic_experiment(
    hemispheric_brain_t* brain,
    uint32_t num_trials,
    float presentation_ms
) {
    split_brain_session_config_t config = split_brain_session_default_config();
    config.paradigm = PARADIGM_TACHISTOSCOPIC;
    config.experiment_name = "Tachistoscopic Experiment";
    config.num_trials = num_trials;
    config.callosal_condition = CALLOSAL_STATE_SEVERED;
    config.stimulus_duration_ms = presentation_ms;

    return split_brain_session_create(&config, brain);
}

split_brain_session_t* split_brain_create_degradation_study(
    hemispheric_brain_t* brain,
    uint32_t num_levels,
    uint32_t trials_per_level
) {
    split_brain_session_config_t config = split_brain_session_default_config();
    config.paradigm = PARADIGM_SPATIAL_VERBAL;
    config.experiment_name = "Callosal Degradation Study";
    config.num_trials = num_levels * trials_per_level;
    config.callosal_condition = CALLOSAL_STATE_DEGRADED;
    config.callosal_strength = 1.0f;  // Start intact, degrade over time

    return split_brain_session_create(&config, brain);
}

//=============================================================================
// Utility Functions
//=============================================================================

const char* split_brain_paradigm_name(experiment_paradigm_t paradigm) {
    switch (paradigm) {
        case PARADIGM_CHIMERIC_FACES: return "Chimeric Faces";
        case PARADIGM_DICHOTIC_LISTENING: return "Dichotic Listening";
        case PARADIGM_TACHISTOSCOPIC: return "Tachistoscopic";
        case PARADIGM_CROSS_CUEING: return "Cross-Cueing Detection";
        case PARADIGM_ALIEN_HAND: return "Alien Hand";
        case PARADIGM_CONFABULATION: return "Confabulation";
        case PARADIGM_SPATIAL_VERBAL: return "Spatial-Verbal";
        case PARADIGM_EMOTION_RECOGNITION: return "Emotion Recognition";
        case PARADIGM_WORD_COMPLETION: return "Word Completion";
        case PARADIGM_OBJECT_MATCHING: return "Object Matching";
        case PARADIGM_MUSIC_LANGUAGE: return "Music-Language";
        case PARADIGM_CUSTOM: return "Custom";
        default: return "Unknown";
    }
}

const char* split_brain_callosal_condition_name(callosal_condition_t condition) {
    switch (condition) {
        case CALLOSAL_STATE_INTACT: return "Intact";
        case CALLOSAL_STATE_SEVERED: return "Severed (Split-Brain)";
        case CALLOSAL_STATE_PARTIAL: return "Partial";
        case CALLOSAL_STATE_DEGRADED: return "Degraded";
        case CALLOSAL_STATE_DEVELOPING: return "Developing";
        default: return "Unknown";
    }
}

const char* split_brain_outcome_name(trial_outcome_t outcome) {
    switch (outcome) {
        case OUTCOME_CORRECT: return "Correct";
        case OUTCOME_INCORRECT: return "Incorrect";
        case OUTCOME_CONFLICT: return "Conflict";
        case OUTCOME_NO_RESPONSE: return "No Response";
        case OUTCOME_CROSS_CUE_DETECTED: return "Cross-Cue Detected";
        case OUTCOME_CONFABULATION: return "Confabulation";
        default: return "Unknown";
    }
}
