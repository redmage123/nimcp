// nimcp_rcog_engine_part_helpers.c - helpers functions
// Part of nimcp_rcog_engine.c (SRP #include-based split)
// DO NOT compile separately - #included from nimcp_rcog_engine.c

static uint64_t engine_now_ms(void) {
    return nimcp_time_monotonic_ms();
}


static rcog_active_request_t* find_request_by_id(rcog_engine_t* engine, uint64_t request_id) {
    for (uint32_t i = 0; i < RCOG_ENGINE_MAX_CONCURRENT_GOALS; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && RCOG_ENGINE_MAX_CONCURRENT_GOALS > 256) {
            rcog_engine_heartbeat("rcog_engine_loop",
                             (float)(i + 1) / (float)RCOG_ENGINE_MAX_CONCURRENT_GOALS);
        }

        if (engine->requests[i].active &&
            engine->requests[i].handle &&
            engine->requests[i].handle->request_id == request_id) {
            return &engine->requests[i];
        }
    }
    return NULL;
}


static rcog_active_request_t* allocate_request_slot(rcog_engine_t* engine) {
    for (uint32_t i = 0; i < RCOG_ENGINE_MAX_CONCURRENT_GOALS; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && RCOG_ENGINE_MAX_CONCURRENT_GOALS > 256) {
            rcog_engine_heartbeat("rcog_engine_loop",
                             (float)(i + 1) / (float)RCOG_ENGINE_MAX_CONCURRENT_GOALS);
        }

        if (!engine->requests[i].active) {
            memset(&engine->requests[i], 0, sizeof(rcog_active_request_t));
            engine->requests[i].active = true;
            engine->active_count++;
            return &engine->requests[i];
        }
    }
    return NULL;
}


static void release_request_slot(rcog_engine_t* engine, rcog_active_request_t* req) {
    if (req && req->active) {
        req->active = false;
        if (engine->active_count > 0) {
            engine->active_count--;
        }
    }
}
