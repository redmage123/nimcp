/**
 * @file nimcp_incremental_processor.c
 * @brief Incremental speech processor implementation
 *
 * @version Phase B4: Speech Enhancement
 * @date 2026-01-15
 */

#include "core/brain/regions/broca/nimcp_incremental_processor.h"
#include "utils/exception/nimcp_exception_macros.h"
#include <string.h>
#include <stdlib.h>
#include <stdio.h>

//=============================================================================
#include <stddef.h>  /* for NULL */
#include "utils/memory/nimcp_memory.h"
#include "utils/fault_tolerance/nimcp_health_agent_macros.h"
#include "utils/bridge/nimcp_bridge_boilerplate.h"
#include "mesh/nimcp_mesh_participant.h"
#include "mesh/nimcp_mesh_adapter.h"

BRIDGE_BOILERPLATE_MESH_ONLY(incremental_processor, MESH_ADAPTER_CATEGORY_COGNITIVE)


/*=============================================================================
 * INTERNAL STRUCTURES
 *===========================================================================*/

struct incremental_processor {
    incremental_config_t config;
    incremental_status_t status;
    incremental_error_t last_error;
    incremental_stats_t stats;

    /* Input buffer */
    incremental_unit_t* input_buffer;
    uint32_t input_count;
    uint32_t input_head;

    /* Output buffer */
    incremental_unit_t* output_buffer;
    uint32_t output_count;
    uint32_t output_head;

    /* Revisions */
    revision_record_t* revisions;
    uint32_t revision_count;

    uint32_t next_unit_id;
    uint64_t last_process_time_ms;
    bool input_ended;

    bio_router_t* router;
    bool bio_registered;
};

/*=============================================================================
 * LIFECYCLE FUNCTIONS
 *===========================================================================*/

incremental_config_t incremental_default_config(void) {
    incremental_config_t config;
    memset(&config, 0, sizeof(config));

    config.input_buffer_size = INCREMENTAL_DEFAULT_BUFFER_SIZE;
    config.output_buffer_size = INCREMENTAL_DEFAULT_BUFFER_SIZE;
    config.lookahead_units = INCREMENTAL_DEFAULT_LOOKAHEAD;
    config.commit_delay_ms = INCREMENTAL_DEFAULT_COMMIT_DELAY_MS;
    config.enable_revision = true;
    config.enable_bio_async = false;
    config.default_unit = UNIT_TYPE_WORD;

    return config;
}

incremental_processor_t* incremental_create(const incremental_config_t* config) {
    incremental_processor_t* processor = (incremental_processor_t*)nimcp_calloc(1, sizeof(incremental_processor_t));
    if (!processor) {

        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "incremental_create: failed to allocate processor");

        return NULL;

    }

    if (config) {
        processor->config = *config;
    } else {
        processor->config = incremental_default_config();
    }

    /* Allocate buffers */
    processor->input_buffer = (incremental_unit_t*)nimcp_calloc(
        processor->config.input_buffer_size, sizeof(incremental_unit_t));
    processor->output_buffer = (incremental_unit_t*)nimcp_calloc(
        processor->config.output_buffer_size, sizeof(incremental_unit_t));
    processor->revisions = (revision_record_t*)nimcp_calloc(
        INCREMENTAL_MAX_REVISION_DEPTH, sizeof(revision_record_t));

    if (!processor->input_buffer || !processor->output_buffer || !processor->revisions) {
        nimcp_free(processor->input_buffer);
        nimcp_free(processor->output_buffer);
        nimcp_free(processor->revisions);
        nimcp_free(processor);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "incremental_create: failed to allocate buffers");
        return NULL;
    }

    processor->status = INCREMENTAL_STATUS_IDLE;
    processor->next_unit_id = 1;

    return processor;
}

void incremental_destroy(incremental_processor_t* processor) {
    if (!processor) return;

    nimcp_free(processor->input_buffer);
    nimcp_free(processor->output_buffer);
    nimcp_free(processor->revisions);
    nimcp_free(processor);
}

bool incremental_reset(incremental_processor_t* processor) {
    if (!processor) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "processor is NULL");
        return false;
    }

    processor->input_count = 0;
    processor->input_head = 0;
    processor->output_count = 0;
    processor->output_head = 0;
    processor->revision_count = 0;
    processor->input_ended = false;

    processor->status = INCREMENTAL_STATUS_IDLE;
    processor->last_error = INCREMENTAL_ERROR_NONE;

    return true;
}

/*=============================================================================
 * INPUT FUNCTIONS
 *===========================================================================*/

uint32_t incremental_add_unit(
    incremental_processor_t* processor,
    const char* content,
    incremental_unit_type_t type,
    uint64_t timestamp_ms) {

    if (!processor) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "processor is NULL");
        return 0;
    }
    if (!content) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "content is NULL");
        return 0;
    }

    if (processor->input_count >= processor->config.input_buffer_size) {
        processor->last_error = INCREMENTAL_ERROR_BUFFER_FULL;
        return 0;
    }

    uint32_t slot = (processor->input_head + processor->input_count) % processor->config.input_buffer_size;
    incremental_unit_t* unit = &processor->input_buffer[slot];

    unit->unit_id = processor->next_unit_id++;
    unit->type = type;
    unit->status = UNIT_STATUS_PENDING;
    strncpy(unit->content, content, sizeof(unit->content) - 1);
    unit->content[sizeof(unit->content) - 1] = '\0';
    unit->arrival_time_ms = timestamp_ms;
    unit->commit_time_ms = 0;
    unit->confidence = 1.0f;

    processor->input_count++;
    processor->stats.units_received++;
    processor->status = INCREMENTAL_STATUS_BUFFERING;

    return unit->unit_id;
}

uint32_t incremental_add_phoneme(
    incremental_processor_t* processor,
    uint8_t phoneme,
    uint64_t timestamp_ms) {

    char content[8];
    snprintf(content, sizeof(content), "%c", (char)phoneme);
    return incremental_add_unit(processor, content, UNIT_TYPE_PHONEME, timestamp_ms);
}

uint32_t incremental_add_word(
    incremental_processor_t* processor,
    const char* word,
    uint64_t timestamp_ms) {

    return incremental_add_unit(processor, word, UNIT_TYPE_WORD, timestamp_ms);
}

bool incremental_end_input(incremental_processor_t* processor) {
    if (!processor) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "processor is NULL");
        return false;
    }

    processor->input_ended = true;
    return true;
}

/*=============================================================================
 * PROCESSING FUNCTIONS
 *===========================================================================*/

bool incremental_process(
    incremental_processor_t* processor,
    uint64_t current_time_ms) {

    if (!processor) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "processor is NULL");
        return false;
    }

    processor->status = INCREMENTAL_STATUS_PROCESSING;
    processor->last_process_time_ms = current_time_ms;

    /* Process pending units */
    for (uint32_t i = 0; i < processor->input_count; i++) {
        uint32_t slot = (processor->input_head + i) % processor->config.input_buffer_size;
        incremental_unit_t* unit = &processor->input_buffer[slot];

        if (unit->status == UNIT_STATUS_PENDING) {
            unit->status = UNIT_STATUS_PROCESSING;
        }

        if (unit->status == UNIT_STATUS_PROCESSING) {
            /* Simple processing - move to tentative */
            unit->status = UNIT_STATUS_TENTATIVE;
        }

        /* Check for commit */
        if (unit->status == UNIT_STATUS_TENTATIVE) {
            uint64_t age = current_time_ms - unit->arrival_time_ms;
            if (age >= processor->config.commit_delay_ms || processor->input_ended) {
                unit->status = UNIT_STATUS_COMMITTED;
                unit->commit_time_ms = current_time_ms;
                processor->stats.units_committed++;

                /* Move to output buffer */
                if (processor->output_count < processor->config.output_buffer_size) {
                    uint32_t out_slot = (processor->output_head + processor->output_count)
                                       % processor->config.output_buffer_size;
                    processor->output_buffer[out_slot] = *unit;
                    processor->output_count++;
                }

                /* Update stats */
                processor->stats.avg_commit_latency_ms =
                    (processor->stats.avg_commit_latency_ms * (processor->stats.units_committed - 1) + age)
                    / processor->stats.units_committed;
            }
        }
    }

    processor->status = INCREMENTAL_STATUS_OUTPUTTING;
    return true;
}

bool incremental_force_commit(incremental_processor_t* processor) {
    if (!processor) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "processor is NULL");
        return false;
    }

    for (uint32_t i = 0; i < processor->input_count; i++) {
        uint32_t slot = (processor->input_head + i) % processor->config.input_buffer_size;
        incremental_unit_t* unit = &processor->input_buffer[slot];

        if (unit->status == UNIT_STATUS_TENTATIVE || unit->status == UNIT_STATUS_PROCESSING) {
            unit->status = UNIT_STATUS_COMMITTED;
            unit->commit_time_ms = processor->last_process_time_ms;
            processor->stats.units_committed++;
        }
    }

    return true;
}

bool incremental_get_output(
    incremental_processor_t* processor,
    incremental_output_t* output) {

    if (!processor) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "processor is NULL");
        return false;
    }
    if (!output) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "output is NULL");
        return false;
    }

    if (processor->output_count == 0) {
        output->units = NULL;
        output->unit_count = 0;
        output->is_final = processor->input_ended;
        return true;
    }

    output->units = (incremental_unit_t*)nimcp_calloc(processor->output_count, sizeof(incremental_unit_t));
    if (!output->units) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "incremental_force_commit: output->units is NULL");
        return false;
    }

    for (uint32_t i = 0; i < processor->output_count; i++) {
        uint32_t slot = (processor->output_head + i) % processor->config.output_buffer_size;
        output->units[i] = processor->output_buffer[slot];
    }

    output->unit_count = processor->output_count;
    output->is_final = processor->input_ended && (processor->input_count == 0);
    output->timestamp_ms = processor->last_process_time_ms;

    /* Clear output buffer */
    processor->output_count = 0;

    return true;
}

void incremental_free_output(incremental_output_t* output) {
    if (!output) return;
    nimcp_free(output->units);
    output->units = NULL;
    output->unit_count = 0;
}

/*=============================================================================
 * REVISION FUNCTIONS
 *===========================================================================*/

bool incremental_revise_unit(
    incremental_processor_t* processor,
    uint32_t unit_id,
    const char* new_content) {

    if (!processor) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "processor is NULL");
        return false;
    }
    if (!new_content) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "new_content is NULL");
        return false;
    }
    if (!processor->config.enable_revision) return false;

    /* Find the unit */
    for (uint32_t i = 0; i < processor->input_count; i++) {
        uint32_t slot = (processor->input_head + i) % processor->config.input_buffer_size;
        incremental_unit_t* unit = &processor->input_buffer[slot];

        if (unit->unit_id == unit_id && unit->status != UNIT_STATUS_COMMITTED) {
            /* Record revision */
            if (processor->revision_count < INCREMENTAL_MAX_REVISION_DEPTH) {
                revision_record_t* rev = &processor->revisions[processor->revision_count++];
                rev->original_id = unit_id;
                rev->revised_id = processor->next_unit_id;
                strncpy(rev->original_content, unit->content, sizeof(rev->original_content) - 1);
                strncpy(rev->revised_content, new_content, sizeof(rev->revised_content) - 1);
                rev->revision_time_ms = processor->last_process_time_ms;
            }

            /* Update unit */
            unit->status = UNIT_STATUS_REVISED;

            /* Add new unit */
            incremental_add_unit(processor, new_content, unit->type, processor->last_process_time_ms);

            processor->stats.units_revised++;
            return true;
        }
    }

    processor->last_error = INCREMENTAL_ERROR_REVISION_FAILED;
    processor->stats.revisions_failed++;
    NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "incremental_free_output: operation failed");
    return false;
}

uint32_t incremental_get_revisions(
    const incremental_processor_t* processor,
    revision_record_t* revisions,
    uint32_t max_revisions) {

    if (!processor) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "processor is NULL");
        return 0;
    }
    if (!revisions) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "revisions is NULL");
        return 0;
    }
    if (max_revisions == 0) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "max_revisions is zero");
        return 0;
    }

    uint32_t count = (processor->revision_count < max_revisions) ?
                     processor->revision_count : max_revisions;

    memcpy(revisions, processor->revisions, count * sizeof(revision_record_t));
    return count;
}

bool incremental_can_revise(
    const incremental_processor_t* processor,
    uint32_t unit_id) {

    if (!processor || !processor->config.enable_revision) {
        return false;
    }

    for (uint32_t i = 0; i < processor->input_count; i++) {
        uint32_t slot = (processor->input_head + i) % processor->config.input_buffer_size;
        const incremental_unit_t* unit = &processor->input_buffer[slot];

        if (unit->unit_id == unit_id) {
            return unit->status != UNIT_STATUS_COMMITTED && unit->status != UNIT_STATUS_REVISED;
        }
    }

    return false;
}

/*=============================================================================
 * BUFFER MANAGEMENT
 *===========================================================================*/

uint32_t incremental_get_pending_count(const incremental_processor_t* processor) {
    if (!processor) return 0;

    uint32_t count = 0;
    for (uint32_t i = 0; i < processor->input_count; i++) {
        uint32_t slot = (processor->input_head + i) % processor->config.input_buffer_size;
        if (processor->input_buffer[slot].status == UNIT_STATUS_PENDING ||
            processor->input_buffer[slot].status == UNIT_STATUS_TENTATIVE) {
            count++;
        }
    }
    return count;
}

uint32_t incremental_get_committed_count(const incremental_processor_t* processor) {
    if (!processor) return 0;
    return (uint32_t)processor->stats.units_committed;
}

bool incremental_clear_pending(incremental_processor_t* processor) {
    if (!processor) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "processor is NULL");
        return false;
    }

    /* Remove all non-committed units */
    uint32_t new_count = 0;
    for (uint32_t i = 0; i < processor->input_count; i++) {
        uint32_t slot = (processor->input_head + i) % processor->config.input_buffer_size;
        if (processor->input_buffer[slot].status == UNIT_STATUS_COMMITTED) {
            new_count++;
        }
    }

    processor->input_count = new_count;
    return true;
}

/*=============================================================================
 * STATUS AND STATISTICS
 *===========================================================================*/

incremental_status_t incremental_get_status(const incremental_processor_t* processor) {
    if (!processor) return INCREMENTAL_STATUS_ERROR;
    return processor->status;
}

incremental_error_t incremental_get_last_error(const incremental_processor_t* processor) {
    if (!processor) return INCREMENTAL_ERROR_INTERNAL;
    return processor->last_error;
}

bool incremental_get_stats(const incremental_processor_t* processor, incremental_stats_t* stats) {
    if (!processor) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "processor is NULL");
        return false;
    }
    if (!stats) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "stats is NULL");
        return false;
    }

    *stats = processor->stats;

    /* Calculate revision rate */
    if (stats->units_received > 0) {
        stats->revision_rate = (float)stats->units_revised / stats->units_received;
    }

    return true;
}

void incremental_reset_stats(incremental_processor_t* processor) {
    if (!processor) return;
    memset(&processor->stats, 0, sizeof(incremental_stats_t));
}

bool incremental_get_config(const incremental_processor_t* processor, incremental_config_t* config) {
    if (!processor) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "processor is NULL");
        return false;
    }
    if (!config) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "config is NULL");
        return false;
    }
    *config = processor->config;
    return true;
}

/*=============================================================================
 * BIO-ASYNC INTEGRATION
 *===========================================================================*/

bool incremental_register_bio_handler(
    incremental_processor_t* processor,
    bio_router_t* router) {

    if (!processor) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "processor is NULL");
        return false;
    }
    if (!router) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "router is NULL");
        return false;
    }

    processor->router = router;
    processor->bio_registered = true;
    return true;
}
