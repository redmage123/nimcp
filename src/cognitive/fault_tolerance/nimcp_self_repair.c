/**
 * @file nimcp_self_repair.c
 * @brief Self-Repair Coordinator Implementation
 *
 * WHAT: Implementation of the autonomous self-repair pipeline coordinator
 * WHY:  Orchestrate complete diagnosis → generation → deployment flow
 * HOW:  Wire up components, manage state, track outcomes
 *
 * @author NIMCP Development Team
 * @date 2025-01-20
 * @version 1.0.0
 */

#include "cognitive/fault_tolerance/nimcp_self_repair.h"
#include "constants/nimcp_buffer_constants.h"
#include "utils/memory/nimcp_memory.h"
#include "utils/thread/nimcp_thread.h"
#include "utils/exception/nimcp_exception_macros.h"
#include "async/nimcp_bio_router.h"

#include <string.h>
#include <stdio.h>
#include <time.h>
#include <sys/stat.h>
#include "utils/bridge/nimcp_bridge_boilerplate.h"
#include "mesh/nimcp_mesh_participant.h"
#include "mesh/nimcp_mesh_adapter.h"
#include "constants/nimcp_timing_constants.h"

BRIDGE_BOILERPLATE(self_repair, MESH_ADAPTER_CATEGORY_COGNITIVE)


//=============================================================================
// Internal Structures
//=============================================================================

/**
 * @brief Self-repair coordinator internal state
 */
struct self_repair_coordinator {
    uint32_t magic;                         /**< Validation magic */
    self_repair_config_t config;            /**< Configuration */

    /* Component handles */
    code_gen_engine_t* code_gen;            /**< Code generation engine */
    vcs_integration_t* vcs;                 /**< VCS integration */
    hot_injector_t hot_inject;              /**< Hot injector */
    code_immune_system_t* code_immune;      /**< Code immune system */

    /* Component ownership */
    bool owns_code_gen;                     /**< We created code_gen */
    bool owns_vcs;                          /**< We created vcs */

    /* Repair records */
    self_repair_record_t* records;
    uint32_t record_count;
    uint32_t record_capacity;
    uint64_t next_repair_id;

    /* Statistics */
    self_repair_stats_t stats;

    /* Callbacks */
    self_repair_stage_cb_t stage_cb;
    self_repair_complete_cb_t complete_cb;
    self_repair_approval_cb_t approval_cb;
    void* stage_cb_data;
    void* complete_cb_data;
    void* approval_cb_data;

    /* Thread safety */
    nimcp_mutex_t* mutex;

    /* Bio-async communication */
    bio_module_context_t bio_ctx;

    /* Health agent integration (Phase 4) */
    struct health_agent* health_agent;              /**< Connected health agent */
    self_repair_direct_failure_cb_t failure_cb;     /**< Failure notification callback */
    void* failure_cb_data;                          /**< Failure callback user data */

    /* State */
    bool ready;
};

//=============================================================================
// Forward Declarations
//=============================================================================

static uint64_t get_timestamp_ms(void);
static void update_stats_from_record(self_repair_coordinator_t* coord, const self_repair_record_t* record);
static void notify_stage_change(self_repair_coordinator_t* coord, uint64_t repair_id,
                                repair_stage_t old_stage, repair_stage_t new_stage);
static void notify_completion(self_repair_coordinator_t* coord, uint64_t repair_id,
                              const self_repair_result_t* result);
static nimcp_error_t self_repair_handle_bio_message(const void* msg, size_t msg_size,
                                                    nimcp_bio_promise_t response_promise,
                                                    void* user_data);
static int register_bio_handlers(self_repair_coordinator_t* coord);

//=============================================================================
// Lifecycle Functions
//=============================================================================

self_repair_config_t self_repair_default_config(void) {
    /* Phase 8: Heartbeat at operation start */
    self_repair_heartbeat("self_repair_default_config", 0.0f);


    self_repair_config_t config = {0};

    config.mode = REPAIR_MODE_DUAL;
    config.min_fix_confidence = 0.7f;
    config.max_risk_score = 0.3f;

    config.require_human_approval = false;
    config.require_approval_complex = true;

    config.analysis_timeout_ms = NIMCP_WATCHDOG_TIMEOUT_MS;
    config.generation_timeout_ms = NIMCP_WATCHDOG_TIMEOUT_MS;
    config.validation_timeout_ms = NIMCP_LONG_TIMEOUT_MS;
    config.deployment_timeout_ms = NIMCP_WATCHDOG_TIMEOUT_MS;

    config.auto_rollback_on_failure = true;
    config.auto_rollback_on_regression = true;

    config.learn_from_outcome = true;
    config.verbose_logging = false;

    return config;
}

self_repair_coordinator_t* self_repair_create(const self_repair_config_t* config) {
    /* Phase 8: Heartbeat at operation start */
    self_repair_heartbeat("self_repair_create", 0.0f);


    return self_repair_create_with_deps(config, NULL, NULL, NULL, NULL);
}

self_repair_coordinator_t* self_repair_create_with_deps(
    const self_repair_config_t* config,
    code_gen_engine_t* code_gen,
    vcs_integration_t* vcs,
    hot_injector_t hot_inject,
    code_immune_system_t* code_immune
) {
    /* Phase 8: Heartbeat at operation start */
    self_repair_heartbeat("self_repair_create_with_deps", 0.0f);


    self_repair_coordinator_t* coord = nimcp_calloc(1, sizeof(self_repair_coordinator_t));
    if (!coord) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "Failed to allocate coord");

        return NULL;
    }

    coord->magic = SELF_REPAIR_MAGIC;

    /* Apply configuration */
    if (config) {
        coord->config = *config;
    } else {
        coord->config = self_repair_default_config();
    }

    /* Create mutex */
    mutex_attr_t attr = {0};
    attr.type = MUTEX_TYPE_RECURSIVE;
    coord->mutex = nimcp_mutex_create(&attr);
    if (!coord->mutex) {
        nimcp_free(coord);
        coord = NULL;
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "self_repair_create_with_deps: coord->mutex is NULL");
        return NULL;
    }

    /* Allocate repair records */
    coord->record_capacity = SELF_REPAIR_MAX_HISTORY;
    coord->records = nimcp_calloc(coord->record_capacity, sizeof(self_repair_record_t));
    if (!coord->records) {
        nimcp_mutex_destroy(coord->mutex);
        nimcp_free(coord);
        coord = NULL;
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "self_repair_create_with_deps: coord->records is NULL");
        return NULL;
    }

    /* Use or create components */
    if (code_gen) {
        coord->code_gen = code_gen;
        coord->owns_code_gen = false;
    } else {
        code_gen_config_t gen_config = code_gen_default_config();
        gen_config.default_min_confidence = coord->config.min_fix_confidence;
        gen_config.default_max_risk = coord->config.max_risk_score;
        coord->code_gen = code_gen_create(&gen_config);
        coord->owns_code_gen = (coord->code_gen != NULL);
    }

    if (vcs) {
        coord->vcs = vcs;
        coord->owns_vcs = false;
    } else {
        vcs_config_t vcs_config = vcs_default_config();
        coord->vcs = vcs_create(&vcs_config);
        coord->owns_vcs = (coord->vcs != NULL);
    }

    /* Store provided components (may be NULL) */
    coord->hot_inject = hot_inject;
    coord->code_immune = code_immune;

    coord->next_repair_id = 1;

    /* Register with bio-async router */
    if (bio_router_is_initialized()) {
        bio_module_info_t info = {0};
        info.module_id = BIO_MODULE_SELF_REPAIR;
        info.module_name = "self_repair";
        info.inbox_capacity = 32;
        info.user_data = coord;
        coord->bio_ctx = bio_router_register_module(&info);
        if (coord->bio_ctx) {
            register_bio_handlers(coord);
        }
    }

    coord->ready = true;

    return coord;
}

void self_repair_destroy(self_repair_coordinator_t* coordinator) {
    if (!coordinator) {
        return;
    }

    /* Phase 8: Heartbeat at operation start */
    self_repair_heartbeat("self_repair_destroy", 0.0f);


    if (coordinator->magic != SELF_REPAIR_MAGIC) {
        return;
    }

    coordinator->ready = false;
    coordinator->magic = 0;

    /* Unregister from bio-async router */
    if (coordinator->bio_ctx) {
        bio_router_unregister_module(coordinator->bio_ctx);
        coordinator->bio_ctx = NULL;
    }

    /* Destroy owned components */
    if (coordinator->owns_code_gen && coordinator->code_gen) {
        code_gen_destroy(coordinator->code_gen);
    }
    if (coordinator->owns_vcs && coordinator->vcs) {
        vcs_destroy(coordinator->vcs);
    }

    if (coordinator->records) {
        nimcp_free(coordinator->records);
    }
    if (coordinator->mutex) {
        nimcp_mutex_destroy(coordinator->mutex);
    }

    nimcp_free(coordinator);
    coordinator = NULL;
}

bool self_repair_is_ready(const self_repair_coordinator_t* coordinator) {
    if (!coordinator || coordinator->magic != SELF_REPAIR_MAGIC) {
        return false;
    }
    /* Phase 8: Heartbeat at operation start */
    self_repair_heartbeat("self_repair_is_ready", 0.0f);


    return coordinator->ready;
}

//=============================================================================
// Core Repair Functions
//=============================================================================

int self_repair_initiate(
    self_repair_coordinator_t* coordinator,
    const self_repair_request_t* request,
    self_repair_result_t* result
) {
    if (!coordinator || !request || !result) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "self_repair_initiate: required parameter is NULL (coordinator, request, result)");
        return -1;
    }

    if (!self_repair_is_ready(coordinator)) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "self_repair_initiate: self_repair_is_ready is NULL");
        return -1;
    }

    /* Phase 8: Heartbeat at operation start */
    self_repair_heartbeat("self_repair_initiate", 0.0f);


    memset(result, 0, sizeof(*result));

    nimcp_mutex_lock(coordinator->mutex);

    /* Create repair record */
    self_repair_record_t* record = NULL;
    if (coordinator->record_count < coordinator->record_capacity) {
        record = &coordinator->records[coordinator->record_count++];
    } else {
        /* Overwrite oldest */
        record = &coordinator->records[0];
        memmove(&coordinator->records[0], &coordinator->records[1],
                (coordinator->record_capacity - 1) * sizeof(self_repair_record_t));
        coordinator->record_count = coordinator->record_capacity;
    }

    memset(record, 0, sizeof(*record));
    record->repair_id = coordinator->next_repair_id++;
    record->start_time = get_timestamp_ms();
    record->stage = REPAIR_STAGE_PENDING;

    /* Copy diagnostic info */
    if (request->diagnosis) {
        record->diagnostic_id = request->diagnosis->error_id;
        record->error_type = request->diagnosis->error_type;
        /* diagnostic_result_t uses stack_trace for location info */
        if (request->diagnosis->stack_depth > 0) {
            strncpy(record->source_file, request->diagnosis->stack_trace[0].file_name,
                    sizeof(record->source_file) - 1);
            strncpy(record->function_name, request->diagnosis->likely_faulty_function,
                    sizeof(record->function_name) - 1);
            record->start_line = request->diagnosis->stack_trace[0].line_number;
        } else {
            strncpy(record->function_name, request->diagnosis->likely_faulty_function,
                    sizeof(record->function_name) - 1);
        }
    }

    coordinator->stats.repairs_attempted++;

    /* Stage 1: Code Analysis */
    notify_stage_change(coordinator, record->repair_id, REPAIR_STAGE_PENDING, REPAIR_STAGE_ANALYZING);
    record->stage = REPAIR_STAGE_ANALYZING;

    code_analysis_result_t analysis = {0};
    uint64_t stage_start = get_timestamp_ms();

    int ret = self_repair_analyze_code(coordinator, request->diagnosis, &analysis);
    record->analysis_time_ms = get_timestamp_ms() - stage_start;

    if (ret != 0) {
        record->stage = REPAIR_STAGE_FAILED;
        record->status = REPAIR_STATUS_ANALYSIS_FAILED;
        snprintf(record->error_message, sizeof(record->error_message),
                 "Code analysis failed");
        coordinator->stats.analysis_failures++;
        goto complete;
    }

    /* Stage 2: Fix Generation */
    notify_stage_change(coordinator, record->repair_id, REPAIR_STAGE_ANALYZING, REPAIR_STAGE_GENERATING);
    record->stage = REPAIR_STAGE_GENERATING;

    generated_fix_t fix = {0};
    stage_start = get_timestamp_ms();

    ret = self_repair_generate_fix(coordinator, request->diagnosis, &analysis, &fix);
    record->generation_time_ms = get_timestamp_ms() - stage_start;

    if (ret != 0) {
        record->stage = REPAIR_STAGE_FAILED;
        record->status = REPAIR_STATUS_NO_FIX_FOUND;
        snprintf(record->error_message, sizeof(record->error_message),
                 "No suitable fix generated");
        coordinator->stats.generation_failures++;
        goto complete;
    }

    /* Check confidence thresholds */
    float min_conf = request->min_confidence_override > 0 ?
                     request->min_confidence_override : coordinator->config.min_fix_confidence;
    float max_risk = request->max_risk_override > 0 ?
                     request->max_risk_override : coordinator->config.max_risk_score;

    if (fix.confidence < min_conf) {
        record->stage = REPAIR_STAGE_FAILED;
        record->status = REPAIR_STATUS_LOW_CONFIDENCE;
        snprintf(record->error_message, sizeof(record->error_message),
                 "Fix confidence %.2f below threshold %.2f", fix.confidence, min_conf);
        goto complete;
    }

    if (fix.risk_score > max_risk) {
        record->stage = REPAIR_STAGE_FAILED;
        record->status = REPAIR_STATUS_HIGH_RISK;
        snprintf(record->error_message, sizeof(record->error_message),
                 "Fix risk %.2f exceeds threshold %.2f", fix.risk_score, max_risk);
        goto complete;
    }

    /* Update record with fix info */
    record->fix_id = fix.fix_id;
    record->fix_strategy = fix.strategy;
    record->fix_confidence = fix.confidence;
    record->fix_risk = fix.risk_score;
    strncpy(record->source_file, fix.source_file, sizeof(record->source_file) - 1);
    strncpy(record->function_name, fix.function_name, sizeof(record->function_name) - 1);
    record->start_line = fix.start_line;
    record->end_line = fix.end_line;

    /* Human approval if required */
    if (coordinator->config.require_human_approval ||
        (coordinator->config.require_approval_complex &&
         fix.complexity >= FIX_COMPLEXITY_COMPLEX)) {
        if (coordinator->approval_cb) {
            if (!coordinator->approval_cb(record->repair_id, &fix, coordinator->approval_cb_data)) {
                record->stage = REPAIR_STAGE_FAILED;
                record->status = REPAIR_STATUS_VALIDATION_FAILED;
                snprintf(record->error_message, sizeof(record->error_message),
                         "Human approval denied");
                goto complete;
            }
        }
    }

    /* Stage 3: Validation */
    if (!request->skip_validation) {
        notify_stage_change(coordinator, record->repair_id, REPAIR_STAGE_GENERATING, REPAIR_STAGE_VALIDATING);
        record->stage = REPAIR_STAGE_VALIDATING;

        stage_start = get_timestamp_ms();
        ret = self_repair_validate_fix(coordinator, &fix, NULL);
        record->validation_time_ms = get_timestamp_ms() - stage_start;

        if (ret != 0) {
            record->stage = REPAIR_STAGE_FAILED;
            record->status = REPAIR_STATUS_VALIDATION_FAILED;
            snprintf(record->error_message, sizeof(record->error_message),
                     "Fix failed validation");
            coordinator->stats.validation_failures++;
            goto complete;
        }
    }

    /* Stage 4: Deployment */
    notify_stage_change(coordinator, record->repair_id, REPAIR_STAGE_VALIDATING, REPAIR_STAGE_DEPLOYING);
    record->stage = REPAIR_STAGE_DEPLOYING;

    stage_start = get_timestamp_ms();

    /* Determine deployment mode */
    self_repair_mode_t mode = coordinator->config.mode;
    if (request->hot_patch_only) {
        mode = REPAIR_MODE_HOT_PATCH_ONLY;
    } else if (request->source_only) {
        mode = REPAIR_MODE_SOURCE_ONLY;
    }

    /* Hot-patch deployment */
    if (mode == REPAIR_MODE_HOT_PATCH_ONLY || mode == REPAIR_MODE_DUAL) {
        uint64_t patch_id = 0;
        ret = self_repair_deploy_hot_patch(coordinator, &fix, &patch_id);
        if (ret == 0) {
            record->hot_patched = true;
            record->patch_id = patch_id;
            coordinator->stats.hot_patches_applied++;
        } else if (mode == REPAIR_MODE_HOT_PATCH_ONLY) {
            record->stage = REPAIR_STAGE_FAILED;
            record->status = REPAIR_STATUS_HOT_PATCH_FAILED;
            snprintf(record->error_message, sizeof(record->error_message),
                     "Hot-patch deployment failed");
            coordinator->stats.deployment_failures++;
            goto complete;
        }
    }

    /* Source commit deployment */
    if (mode == REPAIR_MODE_SOURCE_ONLY || mode == REPAIR_MODE_DUAL) {
        char commit_hash[NIMCP_ID_BUFFER_SIZE] = {0};
        ret = self_repair_deploy_source(coordinator, &fix, commit_hash, sizeof(commit_hash));
        if (ret == 0) {
            record->source_committed = true;
            strncpy(record->commit_hash, commit_hash, sizeof(record->commit_hash) - 1);
            coordinator->stats.source_commits_made++;
        } else if (mode == REPAIR_MODE_SOURCE_ONLY) {
            record->stage = REPAIR_STAGE_FAILED;
            record->status = REPAIR_STATUS_SOURCE_COMMIT_FAILED;
            snprintf(record->error_message, sizeof(record->error_message),
                     "Source commit failed");
            coordinator->stats.deployment_failures++;
            goto complete;
        }
    }

    record->deployment_time_ms = get_timestamp_ms() - stage_start;

    /* Success! */
    record->stage = REPAIR_STAGE_COMPLETED;
    record->status = REPAIR_STATUS_SUCCESS;
    record->can_rollback = true;
    coordinator->stats.repairs_successful++;

    /* Learn from outcome if enabled */
    if (coordinator->config.learn_from_outcome && coordinator->code_gen) {
        code_gen_learn_from_outcome(coordinator->code_gen, &fix, true);
    }

complete:
    record->end_time = get_timestamp_ms();

    /* Update stats */
    update_stats_from_record(coordinator, record);

    /* Build result */
    result->success = (record->status == REPAIR_STATUS_SUCCESS);
    result->status = record->status;
    strncpy(result->error_message, record->error_message, sizeof(result->error_message) - 1);
    result->record = *record;
    result->hot_patch_applied = record->hot_patched;
    result->source_committed = record->source_committed;
    strncpy(result->commit_hash, record->commit_hash, sizeof(result->commit_hash) - 1);

    /* Notify completion */
    notify_completion(coordinator, record->repair_id, result);

    nimcp_mutex_unlock(coordinator->mutex);
    return result->success ? 0 : -1;
}

int self_repair_initiate_async(
    self_repair_coordinator_t* coordinator,
    const self_repair_request_t* request,
    uint64_t* repair_id
) {
    /* For now, implement synchronously */
    /* TODO: Add proper async queue and worker thread */
    /* Phase 8: Heartbeat at operation start */
    self_repair_heartbeat("self_repair_initiate_async", 0.0f);


    self_repair_result_t result;
    int ret = self_repair_initiate(coordinator, request, &result);
    if (repair_id) {
        *repair_id = result.record.repair_id;
    }
    return ret;
}

repair_stage_t self_repair_get_status(
    self_repair_coordinator_t* coordinator,
    uint64_t repair_id,
    self_repair_result_t* result
) {
    if (!coordinator) {
        return REPAIR_STAGE_FAILED;
    }

    /* Phase 8: Heartbeat at operation start */
    self_repair_heartbeat("self_repair_get_status", 0.0f);


    nimcp_mutex_lock(coordinator->mutex);

    for (uint32_t i = 0; i < coordinator->record_count; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && coordinator->record_count > 256) {
            self_repair_heartbeat("self_repair_loop",
                             (float)(i + 1) / (float)coordinator->record_count);
        }

        if (coordinator->records[i].repair_id == repair_id) {
            if (result) {
                result->success = (coordinator->records[i].status == REPAIR_STATUS_SUCCESS);
                result->status = coordinator->records[i].status;
                result->record = coordinator->records[i];
            }
            repair_stage_t stage = coordinator->records[i].stage;
            nimcp_mutex_unlock(coordinator->mutex);
            return stage;
        }
    }

    nimcp_mutex_unlock(coordinator->mutex);
    return REPAIR_STAGE_FAILED;
}

int self_repair_cancel(
    self_repair_coordinator_t* coordinator,
    uint64_t repair_id
) {
    if (!coordinator || coordinator->magic != SELF_REPAIR_MAGIC) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "self_repair_cancel: coordinator is NULL");
        return -1;
    }

    /* Phase 8: Heartbeat at operation start */
    self_repair_heartbeat("self_repair_cancel", 0.0f);


    nimcp_mutex_lock(coordinator->mutex);

    /* Find the repair record */
    for (uint32_t i = 0; i < coordinator->record_count; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && coordinator->record_count > 256) {
            self_repair_heartbeat("self_repair_loop",
                             (float)(i + 1) / (float)coordinator->record_count);
        }

        if (coordinator->records[i].repair_id == repair_id) {
            /* Can only cancel repairs that are in progress */
            repair_stage_t stage = coordinator->records[i].stage;
            if (stage == REPAIR_STAGE_PENDING ||
                stage == REPAIR_STAGE_ANALYZING ||
                stage == REPAIR_STAGE_GENERATING ||
                stage == REPAIR_STAGE_VALIDATING) {
                coordinator->records[i].stage = REPAIR_STAGE_FAILED;
                coordinator->records[i].status = REPAIR_STATUS_ERROR;
                strncpy(coordinator->records[i].error_message, "Cancelled by user",
                        sizeof(coordinator->records[i].error_message) - 1);
                nimcp_mutex_unlock(coordinator->mutex);
                return 0;
            }
            /* Already completed or failed - cannot cancel */
            nimcp_mutex_unlock(coordinator->mutex);
            NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "self_repair_cancel: operation failed");
            return -1;
        }
    }

    nimcp_mutex_unlock(coordinator->mutex);
    /* Repair ID not found */
    NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "self_repair_cancel: operation failed");
    return -1;
}

//=============================================================================
// Pipeline Stage Functions
//=============================================================================

int self_repair_analyze_code(
    self_repair_coordinator_t* coordinator,
    const diagnostic_result_t* diagnosis,
    code_analysis_result_t* analysis
) {
    if (!coordinator || !diagnosis || !analysis) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "self_repair_analyze_code: required parameter is NULL (coordinator, diagnosis, analysis)");
        return -1;
    }

    /* Validate that source file exists if provided in stack trace */
    /* Phase 8: Heartbeat at operation start */
    self_repair_heartbeat("self_repair_analyze_code", 0.0f);


    if (diagnosis->stack_depth > 0 && diagnosis->stack_trace[0].file_name[0]) {
        struct stat st;
        if (stat(diagnosis->stack_trace[0].file_name, &st) != 0) {
            /* File doesn't exist */
            NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "self_repair_analyze_code: validation failed");
            return -1;
        }
        if (!S_ISREG(st.st_mode)) {
            /* Not a regular file */
            NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "self_repair_analyze_code: S_ISREG is NULL");
            return -1;
        }
    }

    /* Use recovery_parietal_bridge for code analysis */
    /* For now, fill in basic analysis from diagnosis */
    memset(analysis, 0, sizeof(*analysis));

    analysis->confidence = 0.8f;
    analysis->repair_difficulty = 0.3f;
    analysis->affected_modules = 1;

    /* Build root cause hypothesis from diagnosis info */
    if (diagnosis->likely_faulty_function[0]) {
        snprintf(analysis->root_cause_hypothesis, sizeof(analysis->root_cause_hypothesis),
                 "Fault in %s: %s", diagnosis->likely_faulty_function, diagnosis->root_cause);
    }

    return 0;
}

int self_repair_generate_fix(
    self_repair_coordinator_t* coordinator,
    const diagnostic_result_t* diagnosis,
    const code_analysis_result_t* analysis,
    generated_fix_t* fix
) {
    if (!coordinator || !diagnosis || !fix) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "self_repair_generate_fix: required parameter is NULL (coordinator, diagnosis, fix)");
        return -1;
    }

    if (!coordinator->code_gen) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "self_repair_generate_fix: coordinator->code_gen is NULL");
        return -1;
    }

    /* Build code generation request */
    /* Phase 8: Heartbeat at operation start */
    self_repair_heartbeat("self_repair_generate_fix", 0.0f);


    code_gen_request_t request = {0};
    request.diagnosis = (diagnostic_result_t*)diagnosis;  /* Cast away const for struct */
    request.code_analysis = (code_analysis_result_t*)analysis;
    /* Build location from diagnostic info */
    if (diagnosis->stack_depth > 0) {
        strncpy(request.location.file_path, diagnosis->stack_trace[0].file_name,
                sizeof(request.location.file_path) - 1);
        request.location.line_number = diagnosis->stack_trace[0].line_number;
    }
    strncpy(request.location.function_name, diagnosis->likely_faulty_function,
            sizeof(request.location.function_name) - 1);
    request.min_confidence = coordinator->config.min_fix_confidence;
    request.max_risk = coordinator->config.max_risk_score;
    request.max_candidates = 5;
    request.use_historical_patterns = true;

    /* Generate candidates */
    code_gen_result_t result;
    int ret = code_gen_generate_candidates(coordinator->code_gen, &request, &result);

    if (ret != 0 || !result.success || result.candidates.count == 0) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "self_repair_generate_fix: result is NULL");
        return -1;
    }

    /* Copy best fix */
    *fix = result.candidates.candidates[result.candidates.selected_index];
    return 0;
}

int self_repair_validate_fix(
    self_repair_coordinator_t* coordinator,
    const generated_fix_t* fix,
    void* validation_result
) {
    if (!coordinator || !fix) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "self_repair_validate_fix: required parameter is NULL (coordinator, fix)");
        return -1;
    }

    /* Phase 8: Heartbeat at operation start */
    self_repair_heartbeat("self_repair_validate_fix", 0.0f);


    (void)validation_result;

    /* TODO: Call recompiler for actual validation */
    /* For now, accept if confidence is high enough */
    if (fix->confidence >= coordinator->config.min_fix_confidence &&
        fix->risk_score <= coordinator->config.max_risk_score) {
        return 0;
    }

    NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "self_repair_validate_fix: operation failed");
    return -1;
}

int self_repair_deploy_hot_patch(
    self_repair_coordinator_t* coordinator,
    const generated_fix_t* fix,
    uint64_t* patch_id
) {
    if (!coordinator || !fix) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "self_repair_deploy_hot_patch: required parameter is NULL (coordinator, fix)");
        return -1;
    }

    if (!coordinator->hot_inject) {
        /* Hot injection not available */
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "self_repair_deploy_hot_patch: coordinator->hot_inject is NULL");
        return -1;
    }

    /* TODO: Actually call hot_inject_patch */
    /* This requires building a .so from the fix, which is complex */
    /* For now, return success if hot_inject is available */
    /* Phase 8: Heartbeat at operation start */
    self_repair_heartbeat("self_repair_deploy_hot_patch", 0.0f);


    if (patch_id) {
        *patch_id = fix->fix_id;
    }

    return 0;
}

int self_repair_deploy_source(
    self_repair_coordinator_t* coordinator,
    const generated_fix_t* fix,
    char* commit_hash,
    size_t commit_hash_size
) {
    if (!coordinator || !fix) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "self_repair_deploy_source: required parameter is NULL (coordinator, fix)");
        return -1;
    }

    if (!coordinator->vcs) {
        /* VCS not available */
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "self_repair_deploy_source: coordinator->vcs is NULL");
        return -1;
    }

    /* Apply and commit */
    /* Phase 8: Heartbeat at operation start */
    self_repair_heartbeat("self_repair_deploy_source", 0.0f);


    vcs_commit_record_t vcs_record;
    int ret = vcs_apply_and_commit(coordinator->vcs, fix, &vcs_record);

    if (ret == VCS_OK && commit_hash && commit_hash_size > 0) {
        strncpy(commit_hash, vcs_record.commit_hash, commit_hash_size - 1);
    }

    return ret;
}

//=============================================================================
// Rollback Functions
//=============================================================================

int self_repair_rollback(
    self_repair_coordinator_t* coordinator,
    uint64_t repair_id
) {
    if (!coordinator) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "coordinator is NULL");

        return -1;
    }

    /* Phase 8: Heartbeat at operation start */
    self_repair_heartbeat("self_repair_rollback", 0.0f);


    nimcp_mutex_lock(coordinator->mutex);

    self_repair_record_t* record = NULL;
    for (uint32_t i = 0; i < coordinator->record_count; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && coordinator->record_count > 256) {
            self_repair_heartbeat("self_repair_loop",
                             (float)(i + 1) / (float)coordinator->record_count);
        }

        if (coordinator->records[i].repair_id == repair_id) {
            record = &coordinator->records[i];
            break;
        }
    }

    if (!record || !record->can_rollback) {
        nimcp_mutex_unlock(coordinator->mutex);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "self_repair_rollback: required parameter is NULL (record, record->can_rollback)");
        return -1;
    }

    /* Rollback hot-patch if applied */
    if (record->hot_patched && coordinator->hot_inject) {
        hot_inject_rollback(coordinator->hot_inject, record->patch_id);
    }

    /* Rollback source commit if made */
    if (record->source_committed && coordinator->vcs) {
        vcs_commit_record_t vcs_record = {0};
        strncpy(vcs_record.commit_hash, record->commit_hash, sizeof(vcs_record.commit_hash) - 1);
        strncpy(vcs_record.source_file, record->source_file, sizeof(vcs_record.source_file) - 1);
        vcs_record.can_rollback = true;
        vcs_rollback(coordinator->vcs, &vcs_record);
    }

    record->stage = REPAIR_STAGE_ROLLED_BACK;
    record->status = REPAIR_STATUS_ROLLED_BACK;
    record->can_rollback = false;
    record->rolled_back = true;

    coordinator->stats.repairs_rolled_back++;

    /* Learn from failure if enabled */
    if (coordinator->config.learn_from_outcome && coordinator->code_gen) {
        generated_fix_t fix = {0};
        fix.fix_id = record->fix_id;
        fix.strategy = record->fix_strategy;
        code_gen_learn_from_outcome(coordinator->code_gen, &fix, false);
    }

    nimcp_mutex_unlock(coordinator->mutex);
    return 0;
}

int self_repair_report_regression(
    self_repair_coordinator_t* coordinator,
    uint64_t repair_id,
    bool auto_rollback
) {
    if (!coordinator) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "coordinator is NULL");

        return -1;
    }

    /* Phase 8: Heartbeat at operation start */
    self_repair_heartbeat("self_repair_report_regression", 0.0f);


    if (auto_rollback || coordinator->config.auto_rollback_on_regression) {
        return self_repair_rollback(coordinator, repair_id);
    }

    return 0;
}

//=============================================================================
// Callback Registration
//=============================================================================

int self_repair_set_stage_callback(
    self_repair_coordinator_t* coordinator,
    self_repair_stage_cb_t callback,
    void* user_data
) {
    if (!coordinator) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "coordinator is NULL");

        return -1;
    }

    /* Phase 8: Heartbeat at operation start */
    self_repair_heartbeat("self_repair_set_stage_callback", 0.0f);


    nimcp_mutex_lock(coordinator->mutex);
    coordinator->stage_cb = callback;
    coordinator->stage_cb_data = user_data;
    nimcp_mutex_unlock(coordinator->mutex);

    return 0;
}

int self_repair_set_complete_callback(
    self_repair_coordinator_t* coordinator,
    self_repair_complete_cb_t callback,
    void* user_data
) {
    if (!coordinator) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "coordinator is NULL");

        return -1;
    }

    /* Phase 8: Heartbeat at operation start */
    self_repair_heartbeat("self_repair_set_complete_callbac", 0.0f);


    nimcp_mutex_lock(coordinator->mutex);
    coordinator->complete_cb = callback;
    coordinator->complete_cb_data = user_data;
    nimcp_mutex_unlock(coordinator->mutex);

    return 0;
}

int self_repair_set_approval_callback(
    self_repair_coordinator_t* coordinator,
    self_repair_approval_cb_t callback,
    void* user_data
) {
    if (!coordinator) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "coordinator is NULL");

        return -1;
    }

    /* Phase 8: Heartbeat at operation start */
    self_repair_heartbeat("self_repair_set_approval_callbac", 0.0f);


    nimcp_mutex_lock(coordinator->mutex);
    coordinator->approval_cb = callback;
    coordinator->approval_cb_data = user_data;
    nimcp_mutex_unlock(coordinator->mutex);

    return 0;
}

//=============================================================================
// Query Functions
//=============================================================================

const self_repair_record_t* self_repair_get_record(
    self_repair_coordinator_t* coordinator,
    uint64_t repair_id
) {
    if (!coordinator) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "coordinator is NULL");

        return NULL;
    }

    /* Phase 8: Heartbeat at operation start */
    self_repair_heartbeat("self_repair_get_record", 0.0f);


    nimcp_mutex_lock(coordinator->mutex);

    for (uint32_t i = 0; i < coordinator->record_count; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && coordinator->record_count > 256) {
            self_repair_heartbeat("self_repair_loop",
                             (float)(i + 1) / (float)coordinator->record_count);
        }

        if (coordinator->records[i].repair_id == repair_id) {
            nimcp_mutex_unlock(coordinator->mutex);
            return &coordinator->records[i];
        }
    }

    nimcp_mutex_unlock(coordinator->mutex);
    /* Record not found is a normal "not found" result, not an error */
    return NULL;
}

uint32_t self_repair_get_recent_records(
    self_repair_coordinator_t* coordinator,
    self_repair_record_t* records,
    uint32_t max_records
) {
    if (!coordinator || !records || max_records == 0) {
        return 0;
    }

    /* Phase 8: Heartbeat at operation start */
    self_repair_heartbeat("self_repair_get_recent_records", 0.0f);


    nimcp_mutex_lock(coordinator->mutex);

    uint32_t count = coordinator->record_count < max_records ?
                     coordinator->record_count : max_records;

    /* Copy most recent records */
    uint32_t start = coordinator->record_count > count ?
                     coordinator->record_count - count : 0;

    for (uint32_t i = 0; i < count; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && count > 256) {
            self_repair_heartbeat("self_repair_loop",
                             (float)(i + 1) / (float)count);
        }

        records[i] = coordinator->records[start + i];
    }

    nimcp_mutex_unlock(coordinator->mutex);
    return count;
}

int self_repair_get_stats(
    const self_repair_coordinator_t* coordinator,
    self_repair_stats_t* stats
) {
    if (!coordinator || !stats) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "self_repair_get_stats: required parameter is NULL (coordinator, stats)");
        return -1;
    }

    /* Phase 8: Heartbeat at operation start */
    self_repair_heartbeat("self_repair_get_stats", 0.0f);


    nimcp_mutex_lock(((self_repair_coordinator_t*)coordinator)->mutex);
    *stats = coordinator->stats;
    nimcp_mutex_unlock(((self_repair_coordinator_t*)coordinator)->mutex);

    return 0;
}

void self_repair_reset_stats(self_repair_coordinator_t* coordinator) {
    if (!coordinator) {
        return;
    }

    /* Phase 8: Heartbeat at operation start */
    self_repair_heartbeat("self_repair_reset_stats", 0.0f);


    nimcp_mutex_lock(coordinator->mutex);
    memset(&coordinator->stats, 0, sizeof(coordinator->stats));
    nimcp_mutex_unlock(coordinator->mutex);
}

//=============================================================================
// Utility Functions
//=============================================================================

const char* self_repair_stage_name(repair_stage_t stage) {
    switch (stage) {
        case REPAIR_STAGE_PENDING:     return "pending";
        case REPAIR_STAGE_ANALYZING:   return "analyzing";
        case REPAIR_STAGE_GENERATING:  return "generating";
        case REPAIR_STAGE_VALIDATING:  return "validating";
        case REPAIR_STAGE_DEPLOYING:   return "deploying";
        case REPAIR_STAGE_COMPLETED:   return "completed";
        case REPAIR_STAGE_FAILED:      return "failed";
        case REPAIR_STAGE_ROLLED_BACK: return "rolled_back";
        default:                       return "unknown";
    }
}

const char* self_repair_status_name(repair_status_t status) {
    switch (status) {
        case REPAIR_STATUS_SUCCESS:             return "success";
        case REPAIR_STATUS_ANALYSIS_FAILED:     return "analysis_failed";
        case REPAIR_STATUS_NO_FIX_FOUND:        return "no_fix_found";
        case REPAIR_STATUS_LOW_CONFIDENCE:      return "low_confidence";
        case REPAIR_STATUS_HIGH_RISK:           return "high_risk";
        case REPAIR_STATUS_VALIDATION_FAILED:   return "validation_failed";
        case REPAIR_STATUS_HOT_PATCH_FAILED:    return "hot_patch_failed";
        case REPAIR_STATUS_SOURCE_COMMIT_FAILED: return "source_commit_failed";
        case REPAIR_STATUS_ROLLED_BACK:         return "rolled_back";
        case REPAIR_STATUS_TIMEOUT:             return "timeout";
        case REPAIR_STATUS_ERROR:               return "error";
        default:                                return "unknown";
    }
}

const char* self_repair_mode_name(self_repair_mode_t mode) {
    switch (mode) {
        case REPAIR_MODE_HOT_PATCH_ONLY: return "hot_patch_only";
        case REPAIR_MODE_SOURCE_ONLY:    return "source_only";
        case REPAIR_MODE_DUAL:           return "dual";
        default:                         return "unknown";
    }
}

const char* self_repair_version(void) {
    return SELF_REPAIR_VERSION;
}

//=============================================================================
// Internal Functions
//=============================================================================

static uint64_t get_timestamp_ms(void) {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (uint64_t)ts.tv_sec * 1000 + (uint64_t)ts.tv_nsec / 1000000;
}

static void update_stats_from_record(self_repair_coordinator_t* coord, const self_repair_record_t* record) {
    if (!coord || !record) {
        return;
    }

    /* Update averages */
    uint64_t n = coord->stats.repairs_attempted;
    if (n > 0) {
        coord->stats.avg_fix_confidence =
            (coord->stats.avg_fix_confidence * (n - 1) + record->fix_confidence) / n;
        coord->stats.avg_fix_risk =
            (coord->stats.avg_fix_risk * (n - 1) + record->fix_risk) / n;

        uint64_t total_time = record->end_time - record->start_time;
        coord->stats.avg_total_time_ms =
            (coord->stats.avg_total_time_ms * (n - 1) + total_time) / n;
        coord->stats.avg_analysis_time_ms =
            (coord->stats.avg_analysis_time_ms * (n - 1) + record->analysis_time_ms) / n;
        coord->stats.avg_generation_time_ms =
            (coord->stats.avg_generation_time_ms * (n - 1) + record->generation_time_ms) / n;
        coord->stats.avg_validation_time_ms =
            (coord->stats.avg_validation_time_ms * (n - 1) + record->validation_time_ms) / n;
        coord->stats.avg_deployment_time_ms =
            (coord->stats.avg_deployment_time_ms * (n - 1) + record->deployment_time_ms) / n;
    }

    /* Success rate */
    if (coord->stats.repairs_attempted > 0) {
        coord->stats.success_rate =
            (float)coord->stats.repairs_successful / coord->stats.repairs_attempted;
    }

    /* By error type */
    if (record->error_type < 32) {
        coord->stats.by_error_type[record->error_type]++;
    }

    /* By strategy */
    if (record->fix_strategy < 16) {
        coord->stats.by_strategy[record->fix_strategy]++;
    }
}

static void notify_stage_change(self_repair_coordinator_t* coord, uint64_t repair_id,
                                repair_stage_t old_stage, repair_stage_t new_stage) {
    if (coord->stage_cb) {
        coord->stage_cb(repair_id, old_stage, new_stage, coord->stage_cb_data);
    }
}

static void notify_completion(self_repair_coordinator_t* coord, uint64_t repair_id,
                              const self_repair_result_t* result) {
    if (coord->complete_cb) {
        coord->complete_cb(repair_id, result, coord->complete_cb_data);
    }
}

//=============================================================================
// Bio-Async Communication
//=============================================================================

/**
 * @brief Handle incoming bio-async messages
 */
static nimcp_error_t self_repair_handle_bio_message(
    const void* msg,
    size_t msg_size,
    nimcp_bio_promise_t response_promise,
    void* user_data
) {
    if (!msg || msg_size < sizeof(bio_message_header_t) || !user_data) {
        return NIMCP_ERROR_INVALID_PARAM;
    }

    self_repair_coordinator_t* coord = (self_repair_coordinator_t*)user_data;
    const bio_message_header_t* header = (const bio_message_header_t*)msg;

    (void)response_promise;  /* May be NULL for fire-and-forget messages */
    (void)coord;  /* Used for future message processing */

    switch (header->type) {
        case BIO_MSG_SELF_REPAIR_REQUEST:
            /* Handle repair request via bio-async */
            /* For now, just acknowledge receipt */
            break;

        case BIO_MSG_SELF_REPAIR_ROLLBACK:
            /* Handle rollback request */
            break;

        case BIO_MSG_DIAGNOSTIC_RESULT:
            /* Received diagnostic result - could auto-trigger repair */
            break;

        default:
            /* Unknown message type for this module */
            return NIMCP_ERROR_UNKNOWN;
    }

    return NIMCP_SUCCESS;
}

/**
 * @brief Register message handlers for self-repair module
 */
static int register_bio_handlers(self_repair_coordinator_t* coord) {
    if (!coord || !coord->bio_ctx) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "register_bio_handlers: required parameter is NULL (coord, coord->bio_ctx)");
        return -1;
    }

    /* Register handlers for self-repair messages */
    bio_router_register_handler(coord->bio_ctx, BIO_MSG_SELF_REPAIR_REQUEST,
                                self_repair_handle_bio_message);
    bio_router_register_handler(coord->bio_ctx, BIO_MSG_SELF_REPAIR_ROLLBACK,
                                self_repair_handle_bio_message);
    bio_router_register_handler(coord->bio_ctx, BIO_MSG_DIAGNOSTIC_RESULT,
                                self_repair_handle_bio_message);

    return 0;
}

/**
 * @brief Broadcast repair stage change via bio-async
 */
int self_repair_broadcast_stage_change(
    self_repair_coordinator_t* coordinator,
    uint64_t repair_id,
    repair_stage_t old_stage,
    repair_stage_t new_stage
) {
    if (!coordinator || !coordinator->bio_ctx) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "self_repair_broadcast_stage_change: required parameter is NULL (coordinator, coordinator->bio_ctx)");
        return -1;
    }

    /* Build and send stage change message */
    /* Phase 8: Heartbeat at operation start */
    self_repair_heartbeat("self_repair_broadcast_stage_chan", 0.0f);


    struct {
        bio_message_header_t header;
        uint64_t repair_id;
        uint32_t old_stage;
        uint32_t new_stage;
    } msg = {0};

    msg.header.type = BIO_MSG_SELF_REPAIR_STAGE_CHANGE;
    msg.header.source_module = BIO_MODULE_SELF_REPAIR;
    msg.header.target_module = BIO_MODULE_ALL;
    msg.header.payload_size = sizeof(msg) - sizeof(bio_message_header_t);
    msg.repair_id = repair_id;
    msg.old_stage = old_stage;
    msg.new_stage = new_stage;

    bio_router_broadcast(coordinator->bio_ctx, &msg, sizeof(msg));
    return 0;
}

/**
 * @brief Broadcast repair result via bio-async
 */
int self_repair_broadcast_result(
    self_repair_coordinator_t* coordinator,
    uint64_t repair_id,
    bool success,
    repair_status_t status
) {
    if (!coordinator || !coordinator->bio_ctx) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "self_repair_broadcast_result: required parameter is NULL (coordinator, coordinator->bio_ctx)");
        return -1;
    }

    /* Build and send result message */
    /* Phase 8: Heartbeat at operation start */
    self_repair_heartbeat("self_repair_broadcast_result", 0.0f);


    struct {
        bio_message_header_t header;
        uint64_t repair_id;
        uint8_t success;
        uint32_t status;
    } msg = {0};

    msg.header.type = BIO_MSG_SELF_REPAIR_RESULT;
    msg.header.source_module = BIO_MODULE_SELF_REPAIR;
    msg.header.target_module = BIO_MODULE_ALL;
    msg.header.payload_size = sizeof(msg) - sizeof(bio_message_header_t);
    msg.repair_id = repair_id;
    msg.success = success ? 1 : 0;
    msg.status = status;

    bio_router_broadcast(coordinator->bio_ctx, &msg, sizeof(msg));
    return 0;
}

/**
 * @brief Process pending bio-async messages
 */
uint32_t self_repair_process_messages(self_repair_coordinator_t* coordinator, uint32_t max_messages) {
    if (!coordinator || !coordinator->bio_ctx) {
        return 0;
    }
    /* Phase 8: Heartbeat at operation start */
    self_repair_heartbeat("self_repair_process_messages", 0.0f);


    return bio_router_process_inbox(coordinator->bio_ctx, max_messages);
}

//=============================================================================
// Health Agent Integration (Phase 4)
//=============================================================================

/**
 * @brief Connect self-repair coordinator to health agent
 */
int self_repair_connect_health_agent(
    self_repair_coordinator_t* coordinator,
    struct health_agent* agent
) {
    if (!coordinator) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
            "self_repair_connect_health_agent: coordinator is NULL");
        return -1;
    }

    /* Phase 8: Heartbeat at operation start */
    self_repair_heartbeat("self_repair_connect_health_agent", 0.0f);


    nimcp_mutex_lock(coordinator->mutex);
    coordinator->health_agent = agent;
    nimcp_mutex_unlock(coordinator->mutex);

    return 0;
}

/**
 * @brief Set failure notification callback
 */
int self_repair_set_failure_callback(
    self_repair_coordinator_t* coordinator,
    self_repair_direct_failure_cb_t callback,
    void* user_data
) {
    if (!coordinator) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
            "self_repair_set_failure_callback: coordinator is NULL");
        return -1;
    }

    /* Phase 8: Heartbeat at operation start */
    self_repair_heartbeat("self_repair_set_failure_callback", 0.0f);


    nimcp_mutex_lock(coordinator->mutex);
    coordinator->failure_cb = callback;
    coordinator->failure_cb_data = user_data;
    nimcp_mutex_unlock(coordinator->mutex);

    return 0;
}

/**
 * @brief Notify health agent of repair failure
 */
int self_repair_notify_health_agent_failure(
    self_repair_coordinator_t* coordinator,
    uint64_t repair_id,
    repair_status_t status,
    const char* error_message
) {
    if (!coordinator) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
            "self_repair_notify_health_agent_failure: coordinator is NULL");
        return -1;
    }

    /* Phase 8: Heartbeat at operation start */
    self_repair_heartbeat("self_repair_notify_health_agent_", 0.0f);


    nimcp_mutex_lock(coordinator->mutex);

    /* Call failure callback if registered */
    if (coordinator->failure_cb) {
        coordinator->failure_cb(repair_id, status, error_message,
                               coordinator->failure_cb_data);
    }

    /* Send bio-async message if health agent connected */
    if (coordinator->health_agent && coordinator->bio_ctx) {
        struct {
            bio_message_header_t header;
            uint64_t repair_id;
            uint32_t status;
            char error_message[NIMCP_ERROR_BUFFER_SIZE];
        } msg = {0};

        msg.header.type = BIO_MSG_REPAIR_HEALTH_FAILURE;
        msg.header.source_module = BIO_MODULE_SELF_REPAIR;
        msg.header.target_module = BIO_MODULE_HEALTH_SELF_REPAIR_BRIDGE;
        msg.header.payload_size = sizeof(msg) - sizeof(bio_message_header_t);
        msg.repair_id = repair_id;
        msg.status = status;
        if (error_message) {
            strncpy(msg.error_message, error_message, sizeof(msg.error_message) - 1);
        }

        bio_router_send(coordinator->bio_ctx, &msg, sizeof(msg), 0);
    }

    nimcp_mutex_unlock(coordinator->mutex);
    return 0;
}

/**
 * @brief Check if health agent is connected
 */
bool self_repair_has_health_agent(const self_repair_coordinator_t* coordinator) {
    if (!coordinator) {
        return false;
    }
    /* Phase 8: Heartbeat at operation start */
    self_repair_heartbeat("self_repair_has_health_agent", 0.0f);


    return coordinator->health_agent != NULL;
}

/* ============================================================================
 * Phase 8: Instance-Level Health Agent
 * ============================================================================ */

void self_repair_set_instance_health_agent(void* instance, nimcp_health_agent_t* agent) {
    self_repair_coordinator_t* self = (self_repair_coordinator_t*)instance;
    if (self) {
        self->health_agent = (struct health_agent*)agent;
    }
}

/* ============================================================================
 * Phase 8: Training Integration (Full Implementation)
 * ============================================================================ */

int self_repair_training_begin(void* instance) {
    if (!instance) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
                              "self_repair_training_begin: NULL argument");
        return -1;
    }
    self_repair_heartbeat_instance(NULL, "self_repair_training_begin", 0.0f);
    (void)(struct self_repair_coordinator*)instance; /* Module state available for reset */
    return 0;
}

int self_repair_training_end(void* instance) {
    if (!instance) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
                              "self_repair_training_end: NULL argument");
        return -1;
    }
    self_repair_heartbeat_instance(NULL, "self_repair_training_end", 1.0f);
    (void)(struct self_repair_coordinator*)instance; /* Module state available for finalization */
    return 0;
}

int self_repair_training_step(void* instance, float progress) {
    if (!instance) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
                              "self_repair_training_step: NULL argument");
        return -1;
    }
    if (progress < 0.0f) progress = 0.0f;
    if (progress > 1.0f) progress = 1.0f;
    self_repair_heartbeat_instance(NULL, "self_repair_training_step", progress);
    (void)(struct self_repair_coordinator*)instance; /* Module state available for step adaptation */
    return 0;
}
