//=============================================================================
// nimcp_creative_onnx_runtime.c - ONNX Runtime Wrapper Implementation
//=============================================================================
/**
 * @file nimcp_creative_onnx_runtime.c
 * @brief Implements ONNX Runtime integration for creative model inference
 *
 * @version 1.0.0
 * @author NIMCP Development Team
 * @date 2025-01-30
 */

#include "cognitive/creative/external/nimcp_creative_onnx_runtime.h"
#include "utils/memory/nimcp_memory.h"
#include <string.h>
#include <stdlib.h>
#include <time.h>
#include "utils/bridge/nimcp_bridge_boilerplate.h"
#include "mesh/nimcp_mesh_participant.h"
#include "mesh/nimcp_mesh_adapter.h"
#include "utils/exception/nimcp_exception_macros.h"
#include "constants/nimcp_buffer_constants.h"
#include <math.h>

BRIDGE_BOILERPLATE_MESH_ONLY(creative_onnx_runtime, MESH_ADAPTER_CATEGORY_COGNITIVE)


//=============================================================================
// Internal Types
//=============================================================================

/**
 * @brief Session internal structure
 */
struct onnx_session {
    char model_path[NIMCP_METRICS_PATH_SIZE];
    void* ort_session;             /* OrtSession* */

    /* I/O metadata */
    onnx_io_info_t* inputs;
    uint32_t num_inputs;
    onnx_io_info_t* outputs;
    uint32_t num_outputs;

    /* Statistics */
    uint64_t inferences;
    float avg_time_ms;
};

//=============================================================================
// Thread-local error message
//=============================================================================

static _Thread_local char g_last_error[NIMCP_ERROR_BUFFER_LARGE] = {0};

static void set_error(const char* msg)
{
    if (msg) {
        strncpy(g_last_error, msg, sizeof(g_last_error) - 1);
    } else {
        g_last_error[0] = '\0';
    }
}

//=============================================================================
// Configuration Defaults
//=============================================================================

void onnx_runtime_config_defaults(onnx_runtime_config_t* config)
{
    if (!config) return;

    memset(config, 0, sizeof(onnx_runtime_config_t));

    config->device = ONNX_DEVICE_CPU;
    config->device_id = 0;

    config->num_threads = 0;       /* Auto */
    config->inter_op_threads = 0;  /* Auto */

    config->gpu_memory_limit = 0;  /* Unlimited */
    config->enable_memory_arena = true;
    config->enable_memory_pattern = true;

    config->optimization_level = 3;  /* All optimizations */
    config->enable_profiling = false;

    config->log_level = 2;  /* Warning */
}

void onnx_session_config_defaults(onnx_session_config_t* config)
{
    if (!config) return;

    memset(config, 0, sizeof(onnx_session_config_t));

    config->use_runtime_defaults = true;
    config->enable_dynamic_batching = false;
    config->max_batch_size = 1;
    config->enable_graph_optimization = true;
    config->optimized_model_path = NULL;
    config->custom_op_libs = NULL;
    config->num_custom_op_libs = 0;
}

//=============================================================================
// Lifecycle
//=============================================================================

creative_onnx_runtime_t* onnx_runtime_create(const onnx_runtime_config_t* config)
{
    creative_onnx_runtime_t* runtime = nimcp_calloc(1, sizeof(creative_onnx_runtime_t));
    if (!runtime) {
        set_error("Failed to allocate runtime");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "onnx_runtime_create: runtime is NULL");
        return NULL;
    }

    if (config) {
        runtime->config = *config;
    } else {
        onnx_runtime_config_defaults(&runtime->config);
    }

    /* In production: initialize ONNX Runtime environment
     * OrtEnv* env;
     * OrtCreateEnv(ORT_LOGGING_LEVEL_WARNING, "nimcp_creative", &env);
     */

    runtime->ort_env = NULL;  /* Placeholder */
    runtime->ort_session_options = NULL;
    runtime->ort_memory_info = NULL;
    runtime->ort_allocator = NULL;

    runtime->sessions = nimcp_calloc(16, sizeof(onnx_session_t*));
    if (!runtime->sessions) return NULL;
    runtime->num_sessions = 0;
    runtime->sessions_capacity = 16;

    runtime->total_inferences = 0;
    runtime->avg_inference_time_ms = 0.0f;
    runtime->peak_memory_bytes = 0;

    return runtime;
}

void onnx_runtime_destroy(creative_onnx_runtime_t* runtime)
{
    if (!runtime) return;

    /* Unload all sessions */
    for (uint32_t i = 0; i < runtime->num_sessions; i++) {
        if (runtime->sessions[i]) {
            onnx_unload_model(runtime, runtime->sessions[i]);
        }
    }

    if (runtime->sessions) {
        nimcp_free(runtime->sessions);
    }

    /* In production: release ONNX Runtime handles
     * OrtReleaseEnv(runtime->ort_env);
     */

    nimcp_free(runtime);
    runtime = NULL;
}

bool onnx_device_available(onnx_device_t device)
{
    switch (device) {
        case ONNX_DEVICE_CPU:
            return true;  /* CPU always available */

        case ONNX_DEVICE_CUDA:
            /* In production: check CUDA availability */
            return false;  /* Placeholder — not-available is normal, not an immune event */

        case ONNX_DEVICE_TENSORRT:
            /* In production: check TensorRT availability */
            return false;

        case ONNX_DEVICE_DIRECTML:
#ifdef _WIN32
            return true;  /* Available on Windows */
#else
            return false;
#endif

        case ONNX_DEVICE_COREML:
#ifdef __APPLE__
            return true;  /* Available on macOS/iOS */
#else
            return false;
#endif

        default:
            return false;
    }
}

//=============================================================================
// Session Management
//=============================================================================

onnx_session_t* onnx_load_model(creative_onnx_runtime_t* runtime,
                                 const char* model_path,
                                 const onnx_session_config_t* config)
{
    if (!runtime || !model_path) {
        set_error("Invalid arguments");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "onnx_load_model: required parameter is NULL (runtime, model_path)");
        return NULL;
    }

    (void)config;  /* Used in production for session options */

    onnx_session_t* session = nimcp_calloc(1, sizeof(onnx_session_t));
    if (!session) {
        set_error("Failed to allocate session");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "onnx_load_model: session is NULL");
        return NULL;
    }

    strncpy(session->model_path, model_path, sizeof(session->model_path) - 1);

    /* In production: load ONNX model
     * OrtCreateSession(runtime->ort_env, model_path_w, options, &session->ort_session);
     */
    session->ort_session = NULL;

    /* Placeholder I/O metadata */
    session->num_inputs = 1;
    session->inputs = nimcp_calloc(session->num_inputs, sizeof(onnx_io_info_t));
    if (session->inputs) {
        strncpy(session->inputs[0].name, "input", sizeof(session->inputs[0].name) - 1);
        session->inputs[0].dtype = ONNX_TYPE_FLOAT32;
    }

    session->num_outputs = 1;
    session->outputs = nimcp_calloc(session->num_outputs, sizeof(onnx_io_info_t));
    if (session->outputs) {
        strncpy(session->outputs[0].name, "output", sizeof(session->outputs[0].name) - 1);
        session->outputs[0].dtype = ONNX_TYPE_FLOAT32;
    }

    session->inferences = 0;
    session->avg_time_ms = 0.0f;

    /* Add to runtime's session list */
    if (runtime->num_sessions >= runtime->sessions_capacity) {
        uint32_t new_cap = runtime->sessions_capacity * 2;
        onnx_session_t** new_sessions = nimcp_calloc(new_cap, sizeof(onnx_session_t*));
        if (!new_sessions) {
            set_error("Failed to expand sessions");
            nimcp_free(session->inputs);
            nimcp_free(session->outputs);
            nimcp_free(session);
            session = NULL;
            NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "onnx_load_model: new_sessions is NULL");
            return NULL;
        }
        memcpy(new_sessions, runtime->sessions,
               runtime->num_sessions * sizeof(onnx_session_t*));
        nimcp_free(runtime->sessions);
        runtime->sessions = new_sessions;
        runtime->sessions_capacity = new_cap;
    }

    runtime->sessions[runtime->num_sessions++] = session;

    return session;
}

onnx_session_t* onnx_load_model_from_memory(creative_onnx_runtime_t* runtime,
                                             const void* model_data,
                                             size_t model_size,
                                             const onnx_session_config_t* config)
{
    if (!runtime || !model_data || model_size == 0) {
        set_error("Invalid arguments");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "onnx_load_model_from_memory: required parameter is NULL (runtime, model_data)");
        return NULL;
    }

    /* In production: use OrtCreateSessionFromArray */
    (void)model_size;
    (void)config;

    onnx_session_t* session = nimcp_calloc(1, sizeof(onnx_session_t));
    if (!session) {
        set_error("Failed to allocate session");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "onnx_load_model_from_memory: session is NULL");
        return NULL;
    }

    strncpy(session->model_path, "<memory>", sizeof(session->model_path) - 1);
    session->ort_session = NULL;

    session->num_inputs = 1;
    session->inputs = nimcp_calloc(1, sizeof(onnx_io_info_t));
    if (!session->inputs) return NULL;
    session->num_outputs = 1;
    session->outputs = nimcp_calloc(1, sizeof(onnx_io_info_t));

    /* Bounds check before writing into session array */
    if (runtime->num_sessions >= runtime->sessions_capacity) {
        set_error("Session capacity exceeded");
        nimcp_free(session->inputs);
        nimcp_free(session->outputs);
        nimcp_free(session);
        return NULL;
    }

    runtime->sessions[runtime->num_sessions++] = session;

    return session;
}

void onnx_unload_model(creative_onnx_runtime_t* runtime, onnx_session_t* session)
{
    if (!runtime || !session) return;

    /* Remove from runtime's session list */
    for (uint32_t i = 0; i < runtime->num_sessions; i++) {
        if (runtime->sessions[i] == session) {
            /* Shift remaining sessions */
            for (uint32_t j = i; j < runtime->num_sessions - 1; j++) {
                runtime->sessions[j] = runtime->sessions[j + 1];
            }
            runtime->num_sessions--;
            break;
        }
    }

    /* Free session resources */
    if (session->inputs) {
        for (uint32_t i = 0; i < session->num_inputs; i++) {
            onnx_shape_free(&session->inputs[i].shape);
        }
        nimcp_free(session->inputs);
    }

    if (session->outputs) {
        for (uint32_t i = 0; i < session->num_outputs; i++) {
            onnx_shape_free(&session->outputs[i].shape);
        }
        nimcp_free(session->outputs);
    }

    /* In production: release ORT session
     * OrtReleaseSession(session->ort_session);
     */

    nimcp_free(session);
    session = NULL;
}

//=============================================================================
// Model Info API
//=============================================================================

uint32_t onnx_session_num_inputs(const onnx_session_t* session)
{
    return session ? session->num_inputs : 0;
}

uint32_t onnx_session_num_outputs(const onnx_session_t* session)
{
    return session ? session->num_outputs : 0;
}

int onnx_session_input_info(const onnx_session_t* session,
                            uint32_t index,
                            onnx_io_info_t* info)
{
    if (!session || !info || index >= session->num_inputs) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "onnx_session_input_info: required parameter is NULL (session, info)");
        return -1;
    }
    *info = session->inputs[index];
    return 0;
}

int onnx_session_output_info(const onnx_session_t* session,
                             uint32_t index,
                             onnx_io_info_t* info)
{
    if (!session || !info || index >= session->num_outputs) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "onnx_session_output_info: required parameter is NULL (session, info)");
        return -1;
    }
    *info = session->outputs[index];
    return 0;
}

int32_t onnx_session_input_index(const onnx_session_t* session, const char* name)
{
    if (!session || !name) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "onnx_session_input_index: required parameter is NULL (session, name)");
        return -1;
    }

    for (uint32_t i = 0; i < session->num_inputs; i++) {
        if (strcmp(session->inputs[i].name, name) == 0) {
            return (int32_t)i;
        }
    }

    return -1;  /* Not found — normal condition, not an immune event */
}

int32_t onnx_session_output_index(const onnx_session_t* session, const char* name)
{
    if (!session || !name) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "onnx_session_output_index: required parameter is NULL (session, name)");
        return -1;
    }

    for (uint32_t i = 0; i < session->num_outputs; i++) {
        if (strcmp(session->outputs[i].name, name) == 0) {
            return (int32_t)i;
        }
    }

    return -1;  /* Not found — normal condition, not an immune event */
}

//=============================================================================
// Inference API
//=============================================================================

int onnx_run(onnx_session_t* session,
             const onnx_tensor_t** inputs, uint32_t num_inputs,
             onnx_tensor_t** outputs, uint32_t num_outputs)
{
    if (!session || !inputs || !outputs) {
        set_error("Invalid arguments");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "onnx_run: required parameter is NULL (session, inputs, outputs)");
        return -1;
    }

    clock_t start = clock();

    /* In production: run ONNX inference
     * OrtRun(session->ort_session, run_options,
     *        input_names, input_tensors, num_inputs,
     *        output_names, num_outputs, output_tensors);
     */

    /* Placeholder: copy inputs to outputs with identity */
    for (uint32_t i = 0; i < num_outputs && i < num_inputs; i++) {
        if (!outputs[i]) {
            outputs[i] = nimcp_calloc(1, sizeof(onnx_tensor_t));
            if (!outputs[i]) continue;

            outputs[i]->shape = onnx_shape_create(inputs[i]->shape.dims,
                                                   inputs[i]->shape.rank);
            outputs[i]->dtype = inputs[i]->dtype;
            outputs[i]->size_bytes = inputs[i]->size_bytes;
            outputs[i]->owns_data = true;
            outputs[i]->data = nimcp_calloc(1, inputs[i]->size_bytes);
        }

        if (outputs[i]->data && inputs[i]->data) {
            memcpy(outputs[i]->data, inputs[i]->data,
                   outputs[i]->size_bytes < inputs[i]->size_bytes ?
                   outputs[i]->size_bytes : inputs[i]->size_bytes);
        }
    }

    float time_ms = (float)(clock() - start) * 1000.0f / CLOCKS_PER_SEC;

    session->inferences++;
    float n = (float)session->inferences;
    session->avg_time_ms = session->avg_time_ms * ((n-1)/(fabsf(n) > 1e-7f ? n : 1e-7f)) + time_ms / (fabsf(n) > 1e-7f ? n : 1e-7f);

    return 0;
}

int onnx_run_named(onnx_session_t* session,
                   const char** input_names,
                   const onnx_tensor_t** inputs, uint32_t num_inputs,
                   const char** output_names,
                   onnx_tensor_t** outputs, uint32_t num_outputs)
{
    /* Verify names match session I/O */
    if (!session || !input_names || !output_names) {
        set_error("Invalid arguments");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "onnx_run_named: required parameter is NULL (session, input_names, output_names)");
        return -1;
    }

    for (uint32_t i = 0; i < num_inputs; i++) {
        if (onnx_session_input_index(session, input_names[i]) < 0) {
            set_error("Unknown input name");
            NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_OUT_OF_RANGE, "onnx_run_named: validation failed");
            return -1;
        }
    }

    for (uint32_t i = 0; i < num_outputs; i++) {
        if (onnx_session_output_index(session, output_names[i]) < 0) {
            set_error("Unknown output name");
            NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_OUT_OF_RANGE, "onnx_run_named: validation failed");
            return -1;
        }
    }

    return onnx_run(session, inputs, num_inputs, outputs, num_outputs);
}

int onnx_run_async(onnx_session_t* session,
                   const onnx_tensor_t** inputs, uint32_t num_inputs,
                   onnx_completion_callback_t callback,
                   void* user_data)
{
    if (!session || !callback) {
        set_error("Invalid arguments");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "onnx_run_async: required parameter is NULL (session, callback)");
        return -1;
    }

    /* Placeholder: run synchronously and call callback */
    onnx_tensor_t** outputs = nimcp_calloc(session->num_outputs, sizeof(onnx_tensor_t*));
    if (!outputs) {
        callback(NULL, 0, -1, user_data);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "onnx_run_async: outputs is NULL");
        return -1;
    }

    int status = onnx_run(session, inputs, num_inputs,
                          outputs, session->num_outputs);

    callback(outputs, session->num_outputs, status, user_data);

    return 0;
}

//=============================================================================
// Tensor API
//=============================================================================

onnx_tensor_t* onnx_tensor_create(creative_onnx_runtime_t* runtime,
                                   const onnx_shape_t* shape,
                                   onnx_dtype_t dtype)
{
    (void)runtime;  /* Would use runtime's allocator */

    if (!shape) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "onnx_tensor_create: shape is NULL");
        return NULL;
    }

    onnx_tensor_t* tensor = nimcp_calloc(1, sizeof(onnx_tensor_t));
    if (!tensor) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "onnx_tensor_create: tensor is NULL");
        return NULL;
    }

    tensor->shape = onnx_shape_create(shape->dims, shape->rank);
    tensor->dtype = dtype;

    int64_t numel = onnx_shape_numel(shape);
    tensor->size_bytes = (size_t)numel * onnx_dtype_size(dtype);
    tensor->owns_data = true;
    tensor->data = nimcp_calloc(1, tensor->size_bytes);

    if (!tensor->data) {
        onnx_shape_free(&tensor->shape);
        nimcp_free(tensor);
        tensor = NULL;
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "onnx_tensor_create: tensor->data is NULL");
        return NULL;
    }

    return tensor;
}

onnx_tensor_t* onnx_tensor_from_data(const void* data,
                                      const onnx_shape_t* shape,
                                      onnx_dtype_t dtype)
{
    if (!data || !shape) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "onnx_tensor_from_data: required parameter is NULL (data, shape)");
        return NULL;
    }

    onnx_tensor_t* tensor = nimcp_calloc(1, sizeof(onnx_tensor_t));
    if (!tensor) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "onnx_tensor_from_data: tensor is NULL");
        return NULL;
    }

    tensor->shape = onnx_shape_create(shape->dims, shape->rank);
    tensor->dtype = dtype;

    int64_t numel = onnx_shape_numel(shape);
    tensor->size_bytes = (size_t)numel * onnx_dtype_size(dtype);
    tensor->owns_data = true;
    tensor->data = nimcp_calloc(1, tensor->size_bytes);

    if (!tensor->data) {
        onnx_shape_free(&tensor->shape);
        nimcp_free(tensor);
        tensor = NULL;
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "onnx_tensor_from_data: tensor->data is NULL");
        return NULL;
    }

    memcpy(tensor->data, data, tensor->size_bytes);

    return tensor;
}

onnx_tensor_t* onnx_tensor_wrap(void* data,
                                 const onnx_shape_t* shape,
                                 onnx_dtype_t dtype)
{
    if (!data || !shape) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "onnx_tensor_wrap: required parameter is NULL (data, shape)");
        return NULL;
    }

    onnx_tensor_t* tensor = nimcp_calloc(1, sizeof(onnx_tensor_t));
    if (!tensor) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "onnx_tensor_wrap: tensor is NULL");
        return NULL;
    }

    tensor->shape = onnx_shape_create(shape->dims, shape->rank);
    tensor->dtype = dtype;

    int64_t numel = onnx_shape_numel(shape);
    tensor->size_bytes = (size_t)numel * onnx_dtype_size(dtype);
    tensor->owns_data = false;
    tensor->data = data;

    return tensor;
}

void onnx_tensor_destroy(onnx_tensor_t* tensor)
{
    if (!tensor) return;

    if (tensor->owns_data && tensor->data) {
        nimcp_free(tensor->data);
    }

    onnx_shape_free(&tensor->shape);
    nimcp_free(tensor);
    tensor = NULL;
}

int64_t onnx_tensor_numel(const onnx_tensor_t* tensor)
{
    if (!tensor) return 0;
    return onnx_shape_numel(&tensor->shape);
}

void* onnx_tensor_data(onnx_tensor_t* tensor)
{
    return tensor ? tensor->data : NULL;
}

int onnx_tensor_copy_to(const onnx_tensor_t* tensor, void* dst, size_t dst_size)
{
    if (!tensor || !dst || dst_size == 0) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "onnx_tensor_copy_to: required parameter is NULL (tensor, dst)");
        return -1;
    }

    size_t copy_size = tensor->size_bytes < dst_size ?
                       tensor->size_bytes : dst_size;
    memcpy(dst, tensor->data, copy_size);

    return 0;
}

//=============================================================================
// Shape API
//=============================================================================

onnx_shape_t onnx_shape_create(const int64_t* dims, uint32_t rank)
{
    onnx_shape_t shape = {0};

    if (!dims || rank == 0) return shape;

    shape.rank = rank;
    shape.dims = nimcp_calloc(rank, sizeof(int64_t));
    if (shape.dims) {
        memcpy(shape.dims, dims, rank * sizeof(int64_t));
    }

    return shape;
}

void onnx_shape_free(onnx_shape_t* shape)
{
    if (!shape) return;

    if (shape->dims) {
        nimcp_free(shape->dims);
        shape->dims = NULL;
    }

    shape->rank = 0;
}

int64_t onnx_shape_numel(const onnx_shape_t* shape)
{
    if (!shape || !shape->dims || shape->rank == 0) return 0;

    int64_t numel = 1;
    for (uint32_t i = 0; i < shape->rank; i++) {
        if (shape->dims[i] < 0) {
            /* Dynamic dimension - treat as 1 */
            continue;
        }
        numel *= shape->dims[i];
    }

    return numel;
}

//=============================================================================
// Utility API
//=============================================================================

size_t onnx_dtype_size(onnx_dtype_t dtype)
{
    switch (dtype) {
        case ONNX_TYPE_FLOAT32: return 4;
        case ONNX_TYPE_FLOAT16: return 2;
        case ONNX_TYPE_INT32:   return 4;
        case ONNX_TYPE_INT64:   return 8;
        case ONNX_TYPE_UINT8:   return 1;
        case ONNX_TYPE_INT8:    return 1;
        case ONNX_TYPE_BOOL:    return 1;
        default:               return 0;
    }
}

const char* onnx_device_name(onnx_device_t device)
{
    switch (device) {
        case ONNX_DEVICE_CPU:      return "CPU";
        case ONNX_DEVICE_CUDA:     return "CUDA";
        case ONNX_DEVICE_TENSORRT: return "TensorRT";
        case ONNX_DEVICE_DIRECTML: return "DirectML";
        case ONNX_DEVICE_COREML:   return "CoreML";
        default:                   return "Unknown";
    }
}

const char* onnx_get_last_error(void)
{
    return g_last_error[0] ? g_last_error : NULL;
}
