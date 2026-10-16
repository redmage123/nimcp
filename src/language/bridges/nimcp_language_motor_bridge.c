#include <stddef.h>  /* for NULL */
//=============================================================================
// nimcp_language_motor_bridge.c - Language-Motor Articulatory Bridge
//=============================================================================
/**
 * @file nimcp_language_motor_bridge.c
 * @brief Implementation of Language-Motor articulatory bridge
 *
 * Provides bidirectional integration between Language Layer (Broca's area)
 * and Motor Cortex for speech articulation control and proprioceptive feedback.
 */

#include "utils/bridge/nimcp_bridge_base.h"
#include "language/bridges/nimcp_language_motor_bridge.h"
#include "core/brain/regions/broca/nimcp_broca_adapter.h"
#include "utils/memory/nimcp_unified_memory.h"
#include "utils/logging/nimcp_logging.h"
#include "utils/time/nimcp_time.h"
#include "utils/exception/nimcp_exception_macros.h"

#include <string.h>
#include <stdlib.h>
#include "utils/fault_tolerance/nimcp_health_agent_macros.h"

NIMCP_DECLARE_HEALTH_AGENT_ATOMIC(language_motor_bridge)

#define LOG_MODULE "LANGUAGE_MOTOR_BRIDGE"


//=============================================================================
// Internal Constants
//=============================================================================

#define MAX_PHONEME_PROGRAMS    256
#define MAX_PENDING_COMMANDS    128
#define MAX_FEEDBACK_BUFFER     64

//=============================================================================
// Internal Structures
//=============================================================================

/**
 * @brief Pending command entry
 */
typedef struct {
    articulator_command_t command;
    bool is_valid;
    bool is_sent;
} pending_command_t;

/**
 * @brief Phoneme program entry
 */
typedef struct {
    phoneme_motor_program_t program;
    bool is_registered;
} phoneme_program_entry_t;

/**
 * @brief Bridge internal structure
 */
struct language_motor_bridge {
    bridge_base_t base;              /**< MUST be first: base bridge infrastructure */
    /* Configuration */
    language_motor_config_t config;

    /* Connected modules */
    language_orchestrator_t* language;
    motor_adapter_t* motor;
    broca_adapter_t* broca;
    bio_router_t router;

    /* State */
    lm_bridge_state_t state;
    speech_execution_state_t exec_state;
    bool is_initialized;

    /* Articulator states */
    articulator_state_t articulators[ARTICULATOR_COUNT];

    /* Command queue */
    pending_command_t command_queue[MAX_PENDING_COMMANDS];
    uint32_t queue_head;
    uint32_t queue_tail;
    uint32_t queue_count;

    /* Phoneme programs */
    phoneme_program_entry_t phoneme_programs[MAX_PHONEME_PROGRAMS];

    /* Current production */
    speech_production_request_t current_request;
    bool has_active_request;
    uint32_t current_phoneme_index;
    float speech_rate;

    /* Timing */
    uint64_t last_update_ms;
    uint64_t production_start_ms;

    /* Statistics */
    language_motor_stats_t stats;

    /* Callbacks */
    lm_command_callback_t command_callback;
    void* command_callback_data;
    lm_feedback_callback_t feedback_callback;
    void* feedback_callback_data;
    lm_status_callback_t status_callback;
    void* status_callback_data;

    /* Logging */
    nimcp_log_context_t log_ctx;
};

//=============================================================================
// Helper Functions
//=============================================================================

static void init_articulators(language_motor_bridge_t* bridge)
{
    for (int i = 0; i < ARTICULATOR_COUNT; i++) {
        bridge->articulators[i].type = (articulator_type_t)i;
        bridge->articulators[i].position = 0.5f;  /* Neutral position */
        bridge->articulators[i].velocity = 0.0f;
        bridge->articulators[i].target_position = 0.5f;
        bridge->articulators[i].target_velocity = 0.0f;
        bridge->articulators[i].force = 0.0f;
        bridge->articulators[i].is_active = false;
        bridge->articulators[i].last_update_ms = 0;
    }
}

static bool queue_command(language_motor_bridge_t* bridge, const articulator_command_t* cmd)
{
    if (bridge->queue_count >= MAX_PENDING_COMMANDS) {
        return false;  /* Queue full - normal backpressure */
    }

    bridge->command_queue[bridge->queue_tail].command = *cmd;
    bridge->command_queue[bridge->queue_tail].is_valid = true;
    bridge->command_queue[bridge->queue_tail].is_sent = false;

    bridge->queue_tail = (bridge->queue_tail + 1) % MAX_PENDING_COMMANDS;
    bridge->queue_count++;

    return true;
}

static bool dequeue_command(language_motor_bridge_t* bridge, articulator_command_t* cmd)
{
    if (bridge->queue_count == 0) {
        return false;
    }

    if (!cmd) {
        return false;
    }

    *cmd = bridge->command_queue[bridge->queue_head].command;
    bridge->command_queue[bridge->queue_head].is_valid = false;

    bridge->queue_head = (bridge->queue_head + 1) % MAX_PENDING_COMMANDS;
    bridge->queue_count--;

    return true;
}

static motor_region_t articulator_to_motor_region(articulator_type_t artic)
{
    /* All speech articulators map to face region of motor cortex */
    (void)artic;
    return MOTOR_REGION_FACE;
}

static int send_to_motor_cortex(
    language_motor_bridge_t* bridge,
    const articulator_command_t* cmd)
{
    if (!bridge->motor) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "send_to_motor_cortex: bridge->motor is NULL");
        return -1;
    }

    /* Convert articulator command to motor goal */
    motor_goal_t goal;
    memset(&goal, 0, sizeof(goal));

    goal.region = articulator_to_motor_region(cmd->articulator);
    goal.target_position.x = cmd->target_value;
    goal.target_position.y = (float)cmd->articulator / (float)ARTICULATOR_COUNT;
    goal.target_position.z = cmd->phoneme_id / 256.0f;
    goal.target_velocity.x = 0.0f;
    goal.target_velocity.y = 0.0f;
    goal.target_velocity.z = 0.0f;
    goal.max_duration_ms = cmd->duration_ms;
    goal.precision_required = 0.9f;
    goal.type = MOVEMENT_TYPE_CONTINUOUS;
    goal.urgency = cmd->priority;

    if (!motor_plan_movement(bridge->motor, &goal)) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "send_to_motor_cortex: motor_plan_movement is NULL");
        return -1;
    }

    if (!motor_begin_execution(bridge->motor)) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "send_to_motor_cortex: motor_begin_execution is NULL");
        return -1;
    }

    bridge->stats.commands_sent++;

    /* Invoke callback */
    if (bridge->command_callback) {
        bridge->command_callback(cmd, bridge->command_callback_data);
    }

    return 0;
}

static void update_production_status(language_motor_bridge_t* bridge)
{
    if (!bridge->has_active_request) {
        return;
    }

    speech_production_status_t status;
    memset(&status, 0, sizeof(status));

    status.request_id = bridge->current_request.request_id;
    status.state = bridge->exec_state;
    status.phonemes_completed = bridge->current_phoneme_index;
    status.phonemes_total = bridge->current_request.phoneme_count;
    status.current_phoneme = bridge->current_phoneme_index < bridge->current_request.phoneme_count ?
        bridge->current_request.phoneme_sequence[bridge->current_phoneme_index] : 0;
    status.progress = bridge->current_request.phoneme_count > 0 ?
        (float)bridge->current_phoneme_index / (float)bridge->current_request.phoneme_count : 0.0f;
    status.timing_accuracy = bridge->stats.avg_timing_accuracy;
    status.position_accuracy = bridge->stats.avg_position_accuracy;
    status.has_errors = (bridge->exec_state == SPEECH_EXEC_ERROR);

    if (bridge->status_callback) {
        bridge->status_callback(&status, bridge->status_callback_data);
    }
}

static void process_pending_commands(language_motor_bridge_t* bridge, uint64_t now_ms)
{
    /* Process commands due for execution */
    while (bridge->queue_count > 0) {
        pending_command_t* pending = &bridge->command_queue[bridge->queue_head];

        if (!pending->is_valid) {
            dequeue_command(bridge, NULL);
            continue;
        }

        /* Check if command is due */
        if (pending->command.execution_time_ms > now_ms) {
            break;  /* Commands are ordered by time */
        }

        /* Send to motor cortex */
        articulator_command_t cmd;
        if (dequeue_command(bridge, &cmd)) {
            send_to_motor_cortex(bridge, &cmd);
        }
    }
}

static void execute_next_phoneme(language_motor_bridge_t* bridge)
{
    if (!bridge->has_active_request) {
        return;
    }

    if (bridge->current_phoneme_index >= bridge->current_request.phoneme_count) {
        /* Production complete */
        bridge->exec_state = SPEECH_EXEC_COMPLETING;
        return;
    }

    uint8_t phoneme_id = bridge->current_request.phoneme_sequence[bridge->current_phoneme_index];

    /* Look up motor program for this phoneme */
    if (phoneme_id < MAX_PHONEME_PROGRAMS &&
        bridge->phoneme_programs[phoneme_id].is_registered) {

        phoneme_motor_program_t* prog = &bridge->phoneme_programs[phoneme_id].program;
        uint64_t now_ms = nimcp_time_now_us() / 1000;
        float rate = bridge->speech_rate * bridge->current_request.speech_rate;

        /* Queue all commands for this phoneme */
        for (uint32_t i = 0; i < prog->num_commands; i++) {
            articulator_command_t cmd = prog->commands[i];
            cmd.duration_ms /= rate;
            cmd.execution_time_ms = now_ms + (uint64_t)(prog->commands[i].execution_time_ms / rate);
            queue_command(bridge, &cmd);
        }

        bridge->stats.phonemes_executed++;
    }

    bridge->current_phoneme_index++;
}

//=============================================================================
// Lifecycle Functions
//=============================================================================

language_motor_config_t language_motor_default_config(void)
{
    language_motor_config_t config;
    memset(&config, 0, sizeof(config));

    config.update_interval_ms = LM_DEFAULT_UPDATE_INTERVAL_MS;
    config.max_articulators = LM_DEFAULT_MAX_ARTICULATORS;
    config.max_command_queue = LM_DEFAULT_MAX_COMMAND_QUEUE;

    config.feedback_window_ms = LM_DEFAULT_FEEDBACK_WINDOW_MS;
    config.timing_tolerance_ms = LM_DEFAULT_TIMING_TOLERANCE_MS;
    config.position_tolerance = LM_DEFAULT_POSITION_TOLERANCE;

    config.enable_proprioceptive_feedback = true;
    config.enable_coarticulation = true;
    config.enable_timing_correction = true;
    config.enable_error_correction = true;
    config.enable_predictive_control = true;

    config.enable_bio_async = true;

    return config;
}

language_motor_bridge_t* language_motor_bridge_create(
    language_orchestrator_t* language,
    motor_adapter_t* motor,
    const language_motor_config_t* config)
{
    language_motor_bridge_t* bridge = nimcp_calloc(1, sizeof(language_motor_bridge_t));
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "bridge is NULL");

        return NULL;
    }

    /* Store configuration */
    if (config) {
        bridge->config = *config;
    } else {
        bridge->config = language_motor_default_config();
    }

    /* Store module references */
    bridge->language = language;
    bridge->motor = motor;
    bridge->broca = NULL;
    bridge->router = NULL;

    /* Initialize state */
    bridge->state = LM_STATE_IDLE;
    bridge->exec_state = SPEECH_EXEC_IDLE;
    bridge->speech_rate = 1.0f;

    /* Initialize articulators */
    init_articulators(bridge);

    /* Initialize command queue */
    bridge->queue_head = 0;
    bridge->queue_tail = 0;
    bridge->queue_count = 0;

    /* Initialize phoneme programs */
    memset(bridge->phoneme_programs, 0, sizeof(bridge->phoneme_programs));

    /* Initialize statistics */
    memset(&bridge->stats, 0, sizeof(bridge->stats));
    bridge->stats.avg_position_accuracy = 1.0f;
    bridge->stats.avg_timing_accuracy = 1.0f;

    /* Create logging context */
    bridge->log_ctx = nimcp_log_context_create(NULL, LANGUAGE_MOTOR_MODULE_NAME);

    bridge->is_initialized = true;

    if (bridge->log_ctx) {
        NIMCP_LOG_INFO("Language-Motor bridge created");
    }

    return bridge;
}

void language_motor_bridge_destroy(language_motor_bridge_t* bridge)
{
    if (!bridge) {
        return;
    }

    if (bridge->log_ctx) {
        NIMCP_LOG_INFO("Language-Motor bridge destroyed");
        nimcp_log_context_destroy(bridge->log_ctx);
    }

    nimcp_free(bridge);
}

int language_motor_bridge_reset(language_motor_bridge_t* bridge)
{
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "bridge is NULL");

        return -1;
    }

    /* Reset state */
    bridge->state = LM_STATE_IDLE;
    bridge->exec_state = SPEECH_EXEC_IDLE;

    /* Reset articulators */
    init_articulators(bridge);

    /* Clear command queue */
    bridge->queue_head = 0;
    bridge->queue_tail = 0;
    bridge->queue_count = 0;

    /* Clear current production */
    bridge->has_active_request = false;
    bridge->current_phoneme_index = 0;

    return 0;
}

//=============================================================================
// Connection Functions
//=============================================================================

int language_motor_connect_broca(
    language_motor_bridge_t* bridge,
    broca_adapter_t* broca)
{
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "bridge is NULL");

        return -1;
    }

    bridge->broca = broca;

    if (bridge->log_ctx) {
        NIMCP_LOG_INFO("Connected to Broca's area adapter");
    }

    return 0;
}

int language_motor_connect_bio_async(
    language_motor_bridge_t* bridge,
    bio_router_t router)
{
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "bridge is NULL");

        return -1;
    }

    bridge->router = router;

    if (bridge->log_ctx) {
        NIMCP_LOG_INFO("Connected to bio-async router");
    }

    return 0;
}

//=============================================================================
// Update Functions
//=============================================================================

int language_motor_bridge_update(
    language_motor_bridge_t* bridge,
    uint64_t timestamp_ms)
{
    if (!bridge || !bridge->is_initialized) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "language_motor_bridge_update: required parameter is NULL (bridge, bridge->is_initialized)");
        return -1;
    }

    bridge->last_update_ms = timestamp_ms;

    /* Process pending commands */
    process_pending_commands(bridge, timestamp_ms);

    /* Update motor cortex execution if connected */
    if (bridge->motor) {
        float dt_ms = (float)bridge->config.update_interval_ms;
        motor_update_execution(bridge->motor, dt_ms);
    }

    /* Check if we need to execute next phoneme */
    if (bridge->exec_state == SPEECH_EXEC_PRODUCING &&
        bridge->queue_count == 0) {
        execute_next_phoneme(bridge);
    }

    /* Check if production complete */
    if (bridge->exec_state == SPEECH_EXEC_COMPLETING &&
        bridge->queue_count == 0) {
        bridge->exec_state = SPEECH_EXEC_IDLE;
        bridge->has_active_request = false;
        bridge->state = LM_STATE_IDLE;
    }

    /* Update production status */
    update_production_status(bridge);

    /* Update state */
    bridge->stats.bridge_state = bridge->state;
    bridge->stats.exec_state = bridge->exec_state;

    return 0;
}

//=============================================================================
// Command Functions (Broca -> Motor)
//=============================================================================

int language_motor_send_command(
    language_motor_bridge_t* bridge,
    const articulator_command_t* command)
{
    if (!bridge || !command) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "language_motor_send_command: required parameter is NULL (bridge, command)");
        return -1;
    }

    if (!queue_command(bridge, command)) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "language_motor_send_command: queue_command is NULL");
        return -1;
    }

    bridge->state = LM_STATE_COMMANDING;

    return 0;
}

int language_motor_send_phoneme_program(
    language_motor_bridge_t* bridge,
    const phoneme_motor_program_t* program)
{
    if (!bridge || !program) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "language_motor_send_phoneme_program: required parameter is NULL (bridge, program)");
        return -1;
    }

    uint64_t now_ms = nimcp_time_now_us() / 1000;

    /* Queue all commands from the program */
    for (uint32_t i = 0; i < program->num_commands; i++) {
        articulator_command_t cmd = program->commands[i];
        cmd.execution_time_ms = now_ms + (uint64_t)program->commands[i].execution_time_ms;

        if (!queue_command(bridge, &cmd)) {
            NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "language_motor_send_phoneme_program: queue_command is NULL");
            return -1;
        }
    }

    bridge->state = LM_STATE_COMMANDING;
    bridge->stats.phonemes_executed++;

    return 0;
}

int language_motor_request_speech_production(
    language_motor_bridge_t* bridge,
    const speech_production_request_t* request)
{
    if (!bridge || !request || !request->phoneme_sequence) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "language_motor_request_speech_production: required parameter is NULL (bridge, request, request->phoneme_sequence)");
        return -1;
    }

    if (bridge->has_active_request) {
        /* Already producing - stop first */
        language_motor_stop_production(bridge);
    }

    /* Store the request */
    bridge->current_request = *request;
    bridge->has_active_request = true;
    bridge->current_phoneme_index = 0;
    bridge->production_start_ms = nimcp_time_now_us() / 1000;

    /* Start production */
    bridge->exec_state = SPEECH_EXEC_PREPARING;
    bridge->state = LM_STATE_COMMANDING;

    /* Execute first phoneme */
    bridge->exec_state = SPEECH_EXEC_PRODUCING;
    execute_next_phoneme(bridge);

    if (bridge->log_ctx) {
        NIMCP_LOG_DEBUG("Started speech production: %u phonemes",
                        request->phoneme_count);
    }

    return 0;
}

int language_motor_pause_production(language_motor_bridge_t* bridge)
{
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "bridge is NULL");

        return -1;
    }

    if (bridge->exec_state == SPEECH_EXEC_PRODUCING) {
        bridge->exec_state = SPEECH_EXEC_PAUSED;
    }

    return 0;
}

int language_motor_resume_production(language_motor_bridge_t* bridge)
{
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "bridge is NULL");

        return -1;
    }

    if (bridge->exec_state == SPEECH_EXEC_PAUSED) {
        bridge->exec_state = SPEECH_EXEC_PRODUCING;
    }

    return 0;
}

int language_motor_stop_production(language_motor_bridge_t* bridge)
{
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "bridge is NULL");

        return -1;
    }

    bridge->exec_state = SPEECH_EXEC_IDLE;
    bridge->has_active_request = false;
    bridge->current_phoneme_index = 0;

    /* Clear command queue */
    bridge->queue_head = 0;
    bridge->queue_tail = 0;
    bridge->queue_count = 0;

    bridge->state = LM_STATE_IDLE;

    return 0;
}

//=============================================================================
// Feedback Functions (Motor -> Broca)
//=============================================================================

int language_motor_receive_feedback(
    language_motor_bridge_t* bridge,
    const proprioceptive_feedback_t* feedback)
{
    if (!bridge || !feedback) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "language_motor_receive_feedback: required parameter is NULL (bridge, feedback)");
        return -1;
    }

    /* Update articulator state */
    if (feedback->articulator < ARTICULATOR_COUNT) {
        articulator_state_t* state = &bridge->articulators[feedback->articulator];
        state->position = feedback->current_position;
        state->velocity = feedback->current_velocity;
        state->last_update_ms = feedback->timestamp_ms;
    }

    bridge->stats.feedback_received++;

    /* Calculate error metrics */
    float pos_error = feedback->position_error;
    float time_error = feedback->timing_error_ms;

    /* Update running averages */
    float alpha = 0.1f;
    bridge->stats.avg_timing_error_ms =
        (1.0f - alpha) * bridge->stats.avg_timing_error_ms + alpha * time_error;

    float pos_accuracy = 1.0f - (pos_error > 1.0f ? 1.0f : pos_error);
    bridge->stats.avg_position_accuracy =
        (1.0f - alpha) * bridge->stats.avg_position_accuracy + alpha * pos_accuracy;

    float time_accuracy = 1.0f - (time_error / bridge->config.timing_tolerance_ms);
    if (time_accuracy < 0.0f) time_accuracy = 0.0f;
    bridge->stats.avg_timing_accuracy =
        (1.0f - alpha) * bridge->stats.avg_timing_accuracy + alpha * time_accuracy;

    /* Apply error correction if enabled */
    if (bridge->config.enable_error_correction &&
        pos_error > bridge->config.position_tolerance) {
        bridge->state = LM_STATE_CORRECTING;
        bridge->stats.corrections_applied++;
    }

    /* Apply timing correction if enabled */
    if (bridge->config.enable_timing_correction &&
        time_error > bridge->config.timing_tolerance_ms) {
        bridge->stats.timing_adjustments++;
    }

    /* Invoke callback */
    if (bridge->feedback_callback) {
        bridge->feedback_callback(feedback, bridge->feedback_callback_data);
    }

    return 0;
}

int language_motor_get_articulator_state(
    const language_motor_bridge_t* bridge,
    articulator_type_t articulator,
    articulator_state_t* state)
{
    if (!bridge || !state || articulator >= ARTICULATOR_COUNT) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "language_motor_get_articulator_state: required parameter is NULL (bridge, state)");
        return -1;
    }

    *state = bridge->articulators[articulator];
    return 0;
}

int language_motor_get_all_articulator_states(
    const language_motor_bridge_t* bridge,
    articulator_state_t* states,
    uint32_t max_states)
{
    if (!bridge || !states) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "language_motor_get_all_articulator_states: required parameter is NULL (bridge, states)");
        return -1;
    }

    uint32_t count = max_states < ARTICULATOR_COUNT ? max_states : ARTICULATOR_COUNT;
    memcpy(states, bridge->articulators, count * sizeof(articulator_state_t));

    return (int)count;
}

//=============================================================================
// Production Status
//=============================================================================

int language_motor_get_production_status(
    const language_motor_bridge_t* bridge,
    speech_production_status_t* status)
{
    if (!bridge || !status) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "language_motor_get_production_status: required parameter is NULL (bridge, status)");
        return -1;
    }

    memset(status, 0, sizeof(*status));

    if (bridge->has_active_request) {
        status->request_id = bridge->current_request.request_id;
        status->state = bridge->exec_state;
        status->phonemes_completed = bridge->current_phoneme_index;
        status->phonemes_total = bridge->current_request.phoneme_count;
        status->current_phoneme = bridge->current_phoneme_index < bridge->current_request.phoneme_count ?
            bridge->current_request.phoneme_sequence[bridge->current_phoneme_index] : 0;
        status->progress = bridge->current_request.phoneme_count > 0 ?
            (float)bridge->current_phoneme_index / (float)bridge->current_request.phoneme_count : 0.0f;
        status->timing_accuracy = bridge->stats.avg_timing_accuracy;
        status->position_accuracy = bridge->stats.avg_position_accuracy;
        status->has_errors = (bridge->exec_state == SPEECH_EXEC_ERROR);
    } else {
        status->state = SPEECH_EXEC_IDLE;
    }

    return 0;
}

speech_execution_state_t language_motor_get_execution_state(
    const language_motor_bridge_t* bridge)
{
    if (!bridge) {
        return SPEECH_EXEC_IDLE;
    }

    return bridge->exec_state;
}

//=============================================================================
// Motor Program Management
//=============================================================================

int language_motor_register_phoneme_program(
    language_motor_bridge_t* bridge,
    const phoneme_motor_program_t* program)
{
    if (!bridge || !program) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "language_motor_register_phoneme_program: required parameter is NULL (bridge, program)");
        return -1;
    }

    if (program->phoneme_id >= MAX_PHONEME_PROGRAMS) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_OUT_OF_RANGE, "language_motor_register_phoneme_program: capacity exceeded");
        return -1;
    }

    bridge->phoneme_programs[program->phoneme_id].program = *program;
    bridge->phoneme_programs[program->phoneme_id].is_registered = true;

    if (bridge->log_ctx) {
        NIMCP_LOG_DEBUG("Registered phoneme program: %u",
                        program->phoneme_id);
    }

    return 0;
}

int language_motor_get_phoneme_program(
    const language_motor_bridge_t* bridge,
    uint8_t phoneme_id,
    phoneme_motor_program_t* program)
{
    if (!bridge || !program) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "language_motor_get_phoneme_program: required parameter is NULL (bridge, program)");
        return -1;
    }

    if (phoneme_id >= MAX_PHONEME_PROGRAMS ||
        !bridge->phoneme_programs[phoneme_id].is_registered) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "language_motor_get_phoneme_program: required parameter is NULL (bridge, program)");
        return -1;
    }

    *program = bridge->phoneme_programs[phoneme_id].program;
    return 0;
}

bool language_motor_has_phoneme_program(
    const language_motor_bridge_t* bridge,
    uint8_t phoneme_id)
{
    if (!bridge || phoneme_id >= MAX_PHONEME_PROGRAMS) {
        return false;
    }

    return bridge->phoneme_programs[phoneme_id].is_registered;
}

//=============================================================================
// Timing Control
//=============================================================================

int language_motor_set_speech_rate(
    language_motor_bridge_t* bridge,
    float rate_multiplier)
{
    if (!bridge || rate_multiplier <= 0.0f) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "language_motor_set_speech_rate: bridge is NULL");
        return -1;
    }

    bridge->speech_rate = rate_multiplier;
    return 0;
}

float language_motor_get_speech_rate(const language_motor_bridge_t* bridge)
{
    if (!bridge) {
        return 1.0f;
    }

    return bridge->speech_rate;
}

int language_motor_apply_timing_correction(
    language_motor_bridge_t* bridge,
    float correction_ms)
{
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "bridge is NULL");

        return -1;
    }

    /* Adjust pending command times */
    for (uint32_t i = 0; i < MAX_PENDING_COMMANDS; i++) {
        if (bridge->command_queue[i].is_valid && !bridge->command_queue[i].is_sent) {
            bridge->command_queue[i].command.execution_time_ms +=
                (int64_t)correction_ms;
        }
    }

    bridge->stats.timing_adjustments++;

    return 0;
}

//=============================================================================
// Callback Registration
//=============================================================================

int language_motor_set_command_callback(
    language_motor_bridge_t* bridge,
    lm_command_callback_t callback,
    void* user_data)
{
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "bridge is NULL");

        return -1;
    }

    bridge->command_callback = callback;
    bridge->command_callback_data = user_data;
    return 0;
}

int language_motor_set_feedback_callback(
    language_motor_bridge_t* bridge,
    lm_feedback_callback_t callback,
    void* user_data)
{
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "bridge is NULL");

        return -1;
    }

    bridge->feedback_callback = callback;
    bridge->feedback_callback_data = user_data;
    return 0;
}

int language_motor_set_status_callback(
    language_motor_bridge_t* bridge,
    lm_status_callback_t callback,
    void* user_data)
{
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "bridge is NULL");

        return -1;
    }

    bridge->status_callback = callback;
    bridge->status_callback_data = user_data;
    return 0;
}

//=============================================================================
// Status and Statistics
//=============================================================================

lm_bridge_state_t language_motor_get_state(
    const language_motor_bridge_t* bridge)
{
    if (!bridge) {
        return LM_STATE_ERROR;
    }

    return bridge->state;
}

int language_motor_get_stats(
    const language_motor_bridge_t* bridge,
    language_motor_stats_t* stats)
{
    if (!bridge || !stats) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "language_motor_get_stats: required parameter is NULL (bridge, stats)");
        return -1;
    }

    *stats = bridge->stats;
    return 0;
}

void language_motor_reset_stats(language_motor_bridge_t* bridge)
{
    if (!bridge) {
        return;
    }

    memset(&bridge->stats, 0, sizeof(bridge->stats));
    bridge->stats.avg_position_accuracy = 1.0f;
    bridge->stats.avg_timing_accuracy = 1.0f;
}

int language_motor_get_config(
    const language_motor_bridge_t* bridge,
    language_motor_config_t* config)
{
    if (!bridge || !config) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "language_motor_get_config: required parameter is NULL (bridge, config)");
        return -1;
    }

    *config = bridge->config;
    return 0;
}

int language_motor_set_config(
    language_motor_bridge_t* bridge,
    const language_motor_config_t* config)
{
    if (!bridge || !config) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "language_motor_set_config: required parameter is NULL (bridge, config)");
        return -1;
    }

    bridge->config = *config;
    return 0;
}
