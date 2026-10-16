/**
 * @file nimcp_gpu_stubs_tensorrt.c
 * @brief CPU fallback implementations for TensorRT export functions
 *
 * WHAT: CPU fallback stubs for all 31 TensorRT export functions
 * WHY:  Enables building and testing TensorRT export pipeline on CPU-only systems
 * HOW:  Implements CPU-based network definition, config management, and
 *       basic serialization; TensorRT engine export returns false (requires GPU)
 *
 * @author NIMCP Development Team
 * @date 2026-02-12
 */

#include "gpu/inference/nimcp_tensorrt_export.h"
#include "gpu/inference/nimcp_int8_inference.h"
#include "gpu/tensor/nimcp_tensor_gpu.h"
#include "utils/exception/nimcp_exception_macros.h"

#include <stdlib.h>
#include <stddef.h>
#include <stdbool.h>
#include <stdint.h>
#include <string.h>
#include <stdio.h>
#include <math.h>

/*=============================================================================
 * Opaque type definitions for CPU fallback
 *=============================================================================*/

/** CPU fallback exporter - stores config and network reference */
struct nimcp_trt_exporter_s {
    nimcp_trt_config_t config;
    nimcp_gpu_context_t* ctx;       /**< GPU context (unused in CPU fallback) */
};

/** CPU fallback calibrator - unused in this file but declared for completeness */
struct nimcp_trt_calibrator_s {
    nimcp_int8_calib_method_t method;
    int num_channels;
    bool per_channel;
};

/*=============================================================================
 * Internal helpers
 *=============================================================================*/

/**
 * @brief Estimate FLOPs for a single layer definition
 */
static float estimate_layer_flops(const nimcp_trt_layer_def_t* layer) {
    if (!layer) return 0.0f;

    switch (layer->type) {
        case TRT_LAYER_DENSE:
            /* 2 * in * out (multiply-add) */
            return 2.0f * (float)layer->in_features * (float)layer->out_features;

        case TRT_LAYER_CONV2D:
        case TRT_LAYER_CONV2D_BN: {
            /* 2 * Cout * Cin * kH * kW * oH * oW (approximate oH*oW as in_features) */
            float kernel_ops = 2.0f * (float)layer->out_features *
                               (float)layer->in_features *
                               (float)layer->kernel_h *
                               (float)layer->kernel_w;
            return kernel_ops;
        }

        case TRT_LAYER_ACTIVATION:
        case TRT_LAYER_SOFTMAX:
        case TRT_LAYER_LAYERNORM:
            return (float)layer->in_features;

        case TRT_LAYER_POOLING:
            return (float)layer->in_features * (float)layer->kernel_h *
                   (float)layer->kernel_w;

        case TRT_LAYER_RESIDUAL_ADD:
            return (float)layer->in_features;

        case TRT_LAYER_ATTENTION:
            /* Self-attention: ~4 * d^2 for Q,K,V,O projections */
            return 4.0f * (float)layer->in_features * (float)layer->in_features;

        default:
            return 0.0f;
    }
}

/*=============================================================================
 * Configuration API (4 functions)
 *=============================================================================*/

/* 1. nimcp_trt_default_config */
int nimcp_trt_default_config(nimcp_trt_config_t* config) {
    if (!config) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
            "nimcp_trt_default_config: config is NULL");
        return -1;
    }

    memset(config, 0, sizeof(nimcp_trt_config_t));
    config->precision = TRT_PRECISION_INT8;
    config->format = TRT_FORMAT_ENGINE;
    config->max_batch_size = TRT_MAX_BATCH_SIZE;
    config->max_workspace_size = TRT_MAX_WORKSPACE_SIZE;
    config->use_strict_types = false;
    config->use_dla = false;
    config->dla_core = 0;
    config->builder_optimization_level = 3;
    config->enable_sparse_weights = false;
    config->enable_timing_cache = true;
    config->enable_dynamic_shapes = false;
    config->min_batch_size = 1;
    config->opt_batch_size = 1;
    config->num_calibration_batches = INT8_CALIBRATION_DEFAULT_SAMPLES;
    config->calibration_cache_path = NULL;
    config->output_path = NULL;
    config->verbose = false;

    return 0;
}

/* 2. nimcp_trt_fp16_config */
int nimcp_trt_fp16_config(nimcp_trt_config_t* config) {
    if (!config) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
            "nimcp_trt_fp16_config: config is NULL");
        return -1;
    }

    /* Start from defaults then override precision */
    nimcp_trt_default_config(config);
    config->precision = TRT_PRECISION_FP16;
    config->use_strict_types = false;
    config->num_calibration_batches = 0;  /* No calibration needed for FP16 */

    return 0;
}

/* 3. nimcp_trt_int8_strict_config */
int nimcp_trt_int8_strict_config(nimcp_trt_config_t* config) {
    if (!config) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
            "nimcp_trt_int8_strict_config: config is NULL");
        return -1;
    }

    /* Start from defaults then override for strict INT8 */
    nimcp_trt_default_config(config);
    config->precision = TRT_PRECISION_INT8;
    config->use_strict_types = true;
    config->builder_optimization_level = 5;
    config->num_calibration_batches = 500;

    return 0;
}

/* 4. nimcp_trt_validate_config */
int nimcp_trt_validate_config(const nimcp_trt_config_t* config) {
    if (!config) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
            "nimcp_trt_validate_config: config is NULL");
        return -1;
    }

    if (config->precision < 0 || config->precision >= TRT_PRECISION_COUNT) {
        return -2;  /* Invalid precision */
    }

    if (config->format < 0 || config->format >= TRT_FORMAT_COUNT) {
        return -3;  /* Invalid format */
    }

    if (config->max_batch_size <= 0) {
        return -4;  /* Invalid batch size */
    }

    if (config->max_workspace_size == 0) {
        return -5;  /* Invalid workspace */
    }

    if (config->builder_optimization_level < 0 ||
        config->builder_optimization_level > 5) {
        return -6;  /* Invalid optimization level */
    }

    if (config->use_dla && (config->dla_core < 0 || config->dla_core > 1)) {
        return -7;  /* Invalid DLA core */
    }

    if (config->enable_dynamic_shapes) {
        if (config->min_batch_size <= 0 ||
            config->opt_batch_size <= 0 ||
            config->min_batch_size > config->opt_batch_size ||
            config->opt_batch_size > config->max_batch_size) {
            return -8;  /* Invalid dynamic shape batch sizes */
        }
    }

    return 0;
}

/*=============================================================================
 * Network Definition API (9 functions)
 *=============================================================================*/

/* 5. nimcp_trt_network_create */
nimcp_trt_network_def_t* nimcp_trt_network_create(
    const char* name,
    int num_layers)
{
    if (!name || num_layers <= 0) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_ARGUMENT,
            "nimcp_trt_network_create: invalid arguments (name is NULL or num_layers <= 0)");
        return NULL;
    }

    nimcp_trt_network_def_t* network = nimcp_calloc(1, sizeof(nimcp_trt_network_def_t));
    if (!network) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY,
            "nimcp_trt_network_create: allocation failed");
        return NULL;
    }

    snprintf(network->name, sizeof(network->name), "%s", name);

    network->layers = nimcp_calloc((size_t)num_layers, sizeof(nimcp_trt_layer_def_t));
    if (!network->layers) {
        nimcp_free(network);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY,
            "nimcp_trt_network_create: layers allocation failed");
        return NULL;
    }
    network->num_layers = num_layers;

    /* Pre-allocate small arrays for inputs/outputs (will grow as needed) */
    network->num_inputs = 0;
    network->input_names = NULL;
    network->input_dims = NULL;
    network->input_ranks = NULL;

    network->num_outputs = 0;
    network->output_names = NULL;

    network->calibration_data = NULL;
    network->num_calibration_samples = 0;

    return network;
}

/* 6. nimcp_trt_network_destroy */
void nimcp_trt_network_destroy(nimcp_trt_network_def_t* network) {
    if (!network) return;

    /* Free input arrays */
    if (network->input_names) {
        for (int i = 0; i < network->num_inputs; i++) {
            nimcp_free(network->input_names[i]);
        }
        nimcp_free(network->input_names);
    }
    if (network->input_dims) {
        for (int i = 0; i < network->num_inputs; i++) {
            nimcp_free(network->input_dims[i]);
        }
        nimcp_free(network->input_dims);
    }
    nimcp_free(network->input_ranks);

    /* Free output arrays */
    if (network->output_names) {
        for (int i = 0; i < network->num_outputs; i++) {
            nimcp_free(network->output_names[i]);
        }
        nimcp_free(network->output_names);
    }

    /* Free layers (weights/bias are owned externally per convention) */
    nimcp_free(network->layers);

    /* Calibration data is owned externally */

    nimcp_free(network);
}

/* 7. nimcp_trt_network_add_input */
int nimcp_trt_network_add_input(
    nimcp_trt_network_def_t* network,
    const char* name,
    const int* dims,
    int rank)
{
    if (!network || !name || !dims || rank <= 0) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
            "nimcp_trt_network_add_input: required parameter is NULL");
        return -1;
    }

    int idx = network->num_inputs;

    /* Grow input_names array */
    char** new_names = nimcp_calloc((size_t)(idx + 1), sizeof(char*));
    if (!new_names) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY,
            "nimcp_trt_network_add_input: allocation failed");
        return -1;
    }
    if (network->input_names) {
        memcpy(new_names, network->input_names, (size_t)idx * sizeof(char*));
        nimcp_free(network->input_names);
    }
    network->input_names = new_names;

    /* Allocate and copy name */
    size_t name_len = strlen(name) + 1;
    network->input_names[idx] = nimcp_malloc(name_len);
    if (!network->input_names[idx]) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY,
            "nimcp_trt_network_add_input: name allocation failed");
        return -1;
    }
    memcpy(network->input_names[idx], name, name_len);

    /* Grow input_dims array */
    int** new_dims = nimcp_calloc((size_t)(idx + 1), sizeof(int*));
    if (!new_dims) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY,
            "nimcp_trt_network_add_input: dims allocation failed");
        return -1;
    }
    if (network->input_dims) {
        memcpy(new_dims, network->input_dims, (size_t)idx * sizeof(int*));
        nimcp_free(network->input_dims);
    }
    network->input_dims = new_dims;

    /* Allocate and copy dims */
    network->input_dims[idx] = nimcp_malloc((size_t)rank * sizeof(int));
    if (!network->input_dims[idx]) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY,
            "nimcp_trt_network_add_input: dims data allocation failed");
        return -1;
    }
    memcpy(network->input_dims[idx], dims, (size_t)rank * sizeof(int));

    /* Grow input_ranks array */
    int* new_ranks = nimcp_calloc((size_t)(idx + 1), sizeof(int));
    if (!new_ranks) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY,
            "nimcp_trt_network_add_input: ranks allocation failed");
        return -1;
    }
    if (network->input_ranks) {
        memcpy(new_ranks, network->input_ranks, (size_t)idx * sizeof(int));
        nimcp_free(network->input_ranks);
    }
    network->input_ranks = new_ranks;
    network->input_ranks[idx] = rank;

    network->num_inputs = idx + 1;
    return idx;
}

/* 8. nimcp_trt_network_add_output */
int nimcp_trt_network_add_output(
    nimcp_trt_network_def_t* network,
    const char* name)
{
    if (!network || !name) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
            "nimcp_trt_network_add_output: required parameter is NULL");
        return -1;
    }

    int idx = network->num_outputs;

    /* Grow output_names array */
    char** new_names = nimcp_calloc((size_t)(idx + 1), sizeof(char*));
    if (!new_names) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY,
            "nimcp_trt_network_add_output: allocation failed");
        return -1;
    }
    if (network->output_names) {
        memcpy(new_names, network->output_names, (size_t)idx * sizeof(char*));
        nimcp_free(network->output_names);
    }
    network->output_names = new_names;

    /* Allocate and copy name */
    size_t name_len = strlen(name) + 1;
    network->output_names[idx] = nimcp_malloc(name_len);
    if (!network->output_names[idx]) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY,
            "nimcp_trt_network_add_output: name allocation failed");
        return -1;
    }
    memcpy(network->output_names[idx], name, name_len);

    network->num_outputs = idx + 1;
    return idx;
}

/* 9. nimcp_trt_network_add_dense */
int nimcp_trt_network_add_dense(
    nimcp_trt_network_def_t* network,
    int layer_idx,
    const char* name,
    int in_features,
    int out_features,
    const float* weights,
    const float* bias,
    nimcp_trt_activation_t activation,
    const nimcp_int8_quant_params_t* quant_params)
{
    if (!network || !name) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
            "nimcp_trt_network_add_dense: required parameter is NULL");
        return -1;
    }
    if (layer_idx < 0 || layer_idx >= network->num_layers) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_ARGUMENT,
            "nimcp_trt_network_add_dense: layer_idx out of range");
        return -1;
    }
    if (in_features <= 0 || out_features <= 0) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_ARGUMENT,
            "nimcp_trt_network_add_dense: in_features/out_features must be positive");
        return -1;
    }

    nimcp_trt_layer_def_t* layer = &network->layers[layer_idx];
    memset(layer, 0, sizeof(nimcp_trt_layer_def_t));

    snprintf(layer->name, sizeof(layer->name), "%s", name);
    layer->type = TRT_LAYER_DENSE;
    layer->in_features = in_features;
    layer->out_features = out_features;
    layer->kernel_h = 1;
    layer->kernel_w = 1;
    layer->stride = 1;
    layer->padding = 0;
    layer->groups = 1;
    layer->activation = activation;
    layer->weights = (void*)weights;
    layer->bias = (void*)bias;
    layer->weights_quantized = false;

    if (quant_params) {
        layer->weight_quant = *quant_params;
        layer->weights_quantized = true;
    }

    return 0;
}

/* 10. nimcp_trt_network_add_conv2d */
int nimcp_trt_network_add_conv2d(
    nimcp_trt_network_def_t* network,
    int layer_idx,
    const char* name,
    int in_channels,
    int out_channels,
    int kernel_h,
    int kernel_w,
    int stride,
    int padding,
    const float* weights,
    const float* bias,
    nimcp_trt_activation_t activation,
    const nimcp_int8_quant_params_t* quant_params)
{
    if (!network || !name) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
            "nimcp_trt_network_add_conv2d: required parameter is NULL");
        return -1;
    }
    if (layer_idx < 0 || layer_idx >= network->num_layers) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_ARGUMENT,
            "nimcp_trt_network_add_conv2d: layer_idx out of range");
        return -1;
    }
    if (in_channels <= 0 || out_channels <= 0 ||
        kernel_h <= 0 || kernel_w <= 0 || stride <= 0) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_ARGUMENT,
            "nimcp_trt_network_add_conv2d: invalid layer dimensions");
        return -1;
    }

    nimcp_trt_layer_def_t* layer = &network->layers[layer_idx];
    memset(layer, 0, sizeof(nimcp_trt_layer_def_t));

    snprintf(layer->name, sizeof(layer->name), "%s", name);
    layer->type = TRT_LAYER_CONV2D;
    layer->in_features = in_channels;
    layer->out_features = out_channels;
    layer->kernel_h = kernel_h;
    layer->kernel_w = kernel_w;
    layer->stride = stride;
    layer->padding = padding;
    layer->groups = 1;
    layer->activation = activation;
    layer->weights = (void*)weights;
    layer->bias = (void*)bias;
    layer->weights_quantized = false;

    if (quant_params) {
        layer->weight_quant = *quant_params;
        layer->weights_quantized = true;
    }

    return 0;
}

/* 11. nimcp_trt_network_add_activation */
int nimcp_trt_network_add_activation(
    nimcp_trt_network_def_t* network,
    int layer_idx,
    const char* name,
    nimcp_trt_activation_t activation)
{
    if (!network || !name) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
            "nimcp_trt_network_add_activation: required parameter is NULL");
        return -1;
    }
    if (layer_idx < 0 || layer_idx >= network->num_layers) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_ARGUMENT,
            "nimcp_trt_network_add_activation: layer_idx out of range");
        return -1;
    }

    nimcp_trt_layer_def_t* layer = &network->layers[layer_idx];
    memset(layer, 0, sizeof(nimcp_trt_layer_def_t));

    snprintf(layer->name, sizeof(layer->name), "%s", name);
    layer->type = TRT_LAYER_ACTIVATION;
    layer->activation = activation;

    /* Carry forward features from previous layer if possible */
    if (layer_idx > 0) {
        layer->in_features = network->layers[layer_idx - 1].out_features;
        layer->out_features = layer->in_features;
    }

    return 0;
}

/* 12. nimcp_trt_network_add_residual */
int nimcp_trt_network_add_residual(
    nimcp_trt_network_def_t* network,
    int layer_idx,
    const char* name,
    const char* input_a,
    const char* input_b)
{
    if (!network || !name || !input_a || !input_b) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
            "nimcp_trt_network_add_residual: required parameter is NULL");
        return -1;
    }
    if (layer_idx < 0 || layer_idx >= network->num_layers) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_ARGUMENT,
            "nimcp_trt_network_add_residual: layer_idx out of range");
        return -1;
    }

    nimcp_trt_layer_def_t* layer = &network->layers[layer_idx];
    memset(layer, 0, sizeof(nimcp_trt_layer_def_t));

    snprintf(layer->name, sizeof(layer->name), "%s", name);
    layer->type = TRT_LAYER_RESIDUAL_ADD;
    layer->activation = TRT_ACTIVATION_NONE;

    /*
     * Find features from the referenced input layers.
     * Search backwards through layers for matching names.
     */
    for (int i = 0; i < layer_idx; i++) {
        if (strcmp(network->layers[i].name, input_a) == 0) {
            layer->in_features = network->layers[i].out_features;
        }
        if (strcmp(network->layers[i].name, input_b) == 0) {
            layer->out_features = network->layers[i].out_features;
        }
    }
    /* If out_features not set, inherit from in_features (residual = same dims) */
    if (layer->out_features == 0 && layer->in_features > 0) {
        layer->out_features = layer->in_features;
    }

    return 0;
}

/* 13. nimcp_trt_network_set_calibration_data */
int nimcp_trt_network_set_calibration_data(
    nimcp_trt_network_def_t* network,
    float** data,
    int num_samples)
{
    if (!network) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
            "nimcp_trt_network_set_calibration_data: network is NULL");
        return -1;
    }
    if (!data || num_samples <= 0) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_ARGUMENT,
            "nimcp_trt_network_set_calibration_data: invalid data or num_samples");
        return -1;
    }

    /* Store reference to external calibration data */
    network->calibration_data = data;
    network->num_calibration_samples = num_samples;

    return 0;
}

/*=============================================================================
 * Exporter API (4 functions)
 *=============================================================================*/

/* 14. nimcp_trt_exporter_create */
nimcp_trt_exporter_t* nimcp_trt_exporter_create(
    nimcp_gpu_context_t* ctx,
    const nimcp_trt_config_t* config)
{
    (void)ctx;  /* Not used in CPU fallback */

    if (!config) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
            "nimcp_trt_exporter_create: config is NULL");
        return NULL;
    }

    /* Validate config before creating exporter */
    int validation = nimcp_trt_validate_config(config);
    if (validation != 0) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_ARGUMENT,
            "nimcp_trt_exporter_create: invalid config");
        return NULL;
    }

    nimcp_trt_exporter_t* exporter = nimcp_calloc(1, sizeof(nimcp_trt_exporter_t));
    if (!exporter) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY,
            "nimcp_trt_exporter_create: allocation failed");
        return NULL;
    }

    exporter->config = *config;
    exporter->ctx = ctx;

    return exporter;
}

/* 15. nimcp_trt_exporter_destroy */
void nimcp_trt_exporter_destroy(nimcp_trt_exporter_t* exporter) {
    if (!exporter) return;
    nimcp_free(exporter);
}

/* 16. nimcp_trt_export */
int nimcp_trt_export(
    nimcp_trt_exporter_t* exporter,
    const nimcp_trt_network_def_t* network,
    nimcp_trt_export_result_t* result)
{
    if (!exporter || !network || !result) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
            "nimcp_trt_export: required parameter is NULL");
        return -1;
    }

    memset(result, 0, sizeof(nimcp_trt_export_result_t));

    /*
     * CPU fallback: We cannot create a real TensorRT engine, but we can
     * generate a CPU-executable representation. For ENGINE format, we must
     * report failure. For ONNX and CALIB_CACHE, we can write data.
     */
    if (exporter->config.format == TRT_FORMAT_ENGINE) {
        result->success = false;
        snprintf(result->error_message, sizeof(result->error_message),
                 "TensorRT engine export requires GPU (CPU fallback)");

        /* Still provide performance estimates */
        float latency = 0.0f;
        float throughput = 0.0f;
        size_t memory = 0;
        nimcp_trt_estimate_performance(network, &exporter->config,
                                       &latency, &throughput, &memory);
        result->estimated_latency_ms = latency;
        result->estimated_throughput = throughput;
        result->estimated_memory_mb = memory;

        return -1;
    }

    /*
     * For ONNX/CALIB_CACHE on CPU: write a minimal serialization if
     * output_path is specified.
     */
    if (exporter->config.output_path) {
        FILE* fp = fopen(exporter->config.output_path, "wb");
        if (!fp) {
            result->success = false;
            snprintf(result->error_message, sizeof(result->error_message),
                     "Failed to open output file: %s", exporter->config.output_path);
            return -1;
        }

        /* Write a minimal header: magic, version, num_layers */
        uint32_t magic = 0x4E494D43;  /* "NIMC" */
        uint32_t version = 1;
        uint32_t num_layers = (uint32_t)network->num_layers;

        fwrite(&magic, sizeof(uint32_t), 1, fp);
        fwrite(&version, sizeof(uint32_t), 1, fp);
        fwrite(&num_layers, sizeof(uint32_t), 1, fp);

        /* Write layer definitions */
        for (int i = 0; i < network->num_layers; i++) {
            fwrite(&network->layers[i].name, sizeof(char), 256, fp);
            fwrite(&network->layers[i].type, sizeof(int), 1, fp);
            fwrite(&network->layers[i].in_features, sizeof(int), 1, fp);
            fwrite(&network->layers[i].out_features, sizeof(int), 1, fp);
            fwrite(&network->layers[i].kernel_h, sizeof(int), 1, fp);
            fwrite(&network->layers[i].kernel_w, sizeof(int), 1, fp);
            fwrite(&network->layers[i].stride, sizeof(int), 1, fp);
            fwrite(&network->layers[i].padding, sizeof(int), 1, fp);
            fwrite(&network->layers[i].activation, sizeof(int), 1, fp);
        }

        long file_size = ftell(fp);
        fclose(fp);

        result->success = true;
        result->engine_size = (size_t)file_size;
        snprintf(result->output_path, sizeof(result->output_path),
                 "%s", exporter->config.output_path);
    } else {
        /* No output path, just return success with estimates */
        result->success = true;
        result->engine_size = 0;
    }

    /* Fill performance estimates */
    nimcp_trt_estimate_performance(network, &exporter->config,
                                   &result->estimated_latency_ms,
                                   &result->estimated_throughput,
                                   &result->estimated_memory_mb);

    return 0;
}

/* 17. nimcp_trt_export_int8_model */
int nimcp_trt_export_int8_model(
    nimcp_trt_exporter_t* exporter,
    const nimcp_int8_model_t* model,
    const char** input_names,
    const int** input_dims,
    int num_inputs,
    nimcp_trt_export_result_t* result)
{
    if (!exporter || !model || !result) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
            "nimcp_trt_export_int8_model: required parameter is NULL");
        return -1;
    }

    memset(result, 0, sizeof(nimcp_trt_export_result_t));

    /*
     * Build a network definition from the INT8 model, then delegate to
     * nimcp_trt_export. On CPU fallback, we create a temporary network def.
     */
    nimcp_trt_network_def_t* network = nimcp_trt_network_create(
        model->model_name, model->num_layers);
    if (!network) {
        result->success = false;
        snprintf(result->error_message, sizeof(result->error_message),
                 "Failed to create network definition from INT8 model");
        return -1;
    }

    /* Add inputs */
    if (input_names && input_dims) {
        for (int i = 0; i < num_inputs; i++) {
            if (input_names[i] && input_dims[i]) {
                /* Assume rank 4 for typical NCHW inputs */
                nimcp_trt_network_add_input(network, input_names[i], input_dims[i], 4);
            }
        }
    }

    /* Populate layer definitions from INT8 model layers */
    for (int i = 0; i < model->num_layers && i < network->num_layers; i++) {
        nimcp_trt_layer_def_t* layer = &network->layers[i];
        snprintf(layer->name, sizeof(layer->name), "%s", model->layers[i].name);
        layer->type = TRT_LAYER_DENSE;  /* Default to dense */
        layer->activation = TRT_ACTIVATION_RELU;

        if (model->layers[i].weight) {
            /* Use dimensions from the weight tensor */
            if (model->layers[i].weight->rank >= 2) {
                layer->in_features = (int)model->layers[i].weight->dims[1];
                layer->out_features = (int)model->layers[i].weight->dims[0];
            }
            layer->weight_quant = model->layers[i].weight->params;
            layer->weights_quantized = true;
        }

        layer->input_quant = model->layers[i].input_params;
        layer->output_quant = model->layers[i].output_params;
    }

    /* Export the constructed network */
    int ret = nimcp_trt_export(exporter, network, result);

    nimcp_trt_network_destroy(network);
    return ret;
}

/*=============================================================================
 * Direct Export Functions (4 functions)
 *=============================================================================*/

/* 18. nimcp_export_tensorrt */
bool nimcp_export_tensorrt(
    nimcp_gpu_context_t* ctx,
    const nimcp_int8_model_t* model,
    const char* output_path)
{
    (void)ctx;  /* Not used in CPU fallback */

    if (!model || !output_path) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
            "nimcp_export_tensorrt: required parameter is NULL");
        return false;
    }

    /*
     * CPU fallback: TensorRT engine generation requires NVIDIA GPU.
     * Return false to indicate TensorRT export is not available.
     */
    return false;
}

/* 19. nimcp_export_onnx_quantized */
bool nimcp_export_onnx_quantized(
    const nimcp_int8_model_t* model,
    const char* output_path)
{
    if (!model || !output_path) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
            "nimcp_export_onnx_quantized: required parameter is NULL");
        return false;
    }

    /*
     * CPU fallback: Write a minimal ONNX-like serialization with layer info
     * and quantization parameters. Not a real ONNX file but preserves
     * enough information for later conversion when GPU is available.
     */
    FILE* fp = fopen(output_path, "wb");
    if (!fp) {
        return false;
    }

    /* Write header */
    uint32_t magic = 0x4F4E4E58;  /* "ONNX" marker */
    uint32_t version = 1;
    uint32_t num_layers = (uint32_t)model->num_layers;
    uint32_t name_len = (uint32_t)strlen(model->model_name);

    fwrite(&magic, sizeof(uint32_t), 1, fp);
    fwrite(&version, sizeof(uint32_t), 1, fp);
    fwrite(&name_len, sizeof(uint32_t), 1, fp);
    fwrite(model->model_name, 1, name_len, fp);
    fwrite(&num_layers, sizeof(uint32_t), 1, fp);

    /* Write each layer's quantization info */
    for (int i = 0; i < model->num_layers; i++) {
        const nimcp_int8_layer_t* layer = &model->layers[i];

        /* Layer name */
        fwrite(layer->name, sizeof(char), 256, fp);

        /* Input/output quant params (scale, zero_point, min, max) */
        fwrite(&layer->input_params.scale, sizeof(float), 1, fp);
        fwrite(&layer->input_params.zero_point, sizeof(int32_t), 1, fp);
        fwrite(&layer->input_params.min_val, sizeof(float), 1, fp);
        fwrite(&layer->input_params.max_val, sizeof(float), 1, fp);

        fwrite(&layer->output_params.scale, sizeof(float), 1, fp);
        fwrite(&layer->output_params.zero_point, sizeof(int32_t), 1, fp);
        fwrite(&layer->output_params.min_val, sizeof(float), 1, fp);
        fwrite(&layer->output_params.max_val, sizeof(float), 1, fp);

        /* Weight quant params if available */
        bool has_weight = (layer->weight != NULL);
        fwrite(&has_weight, sizeof(bool), 1, fp);
        if (has_weight) {
            fwrite(&layer->weight->params.scale, sizeof(float), 1, fp);
            fwrite(&layer->weight->params.zero_point, sizeof(int32_t), 1, fp);
            fwrite(&layer->weight->numel, sizeof(size_t), 1, fp);

            /* Write INT8 weight data */
            if (layer->weight->data && layer->weight->numel > 0) {
                fwrite(layer->weight->data, sizeof(int8_t),
                       layer->weight->numel, fp);
            }
        }
    }

    fclose(fp);
    return true;
}

/* 20. nimcp_trt_save_calibration_cache */
bool nimcp_trt_save_calibration_cache(
    const nimcp_int8_calibrator_t* calibrator,
    const char* output_path)
{
    if (!calibrator || !output_path) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
            "nimcp_trt_save_calibration_cache: required parameter is NULL");
        return false;
    }

    FILE* fp = fopen(output_path, "wb");
    if (!fp) {
        return false;
    }

    /* Write calibration cache header */
    uint32_t magic = 0x43414C42;  /* "CALB" */
    uint32_t version = 1;

    fwrite(&magic, sizeof(uint32_t), 1, fp);
    fwrite(&version, sizeof(uint32_t), 1, fp);
    fwrite(&calibrator->method, sizeof(int), 1, fp);
    fwrite(&calibrator->per_channel, sizeof(bool), 1, fp);
    fwrite(&calibrator->num_channels, sizeof(int), 1, fp);
    fwrite(&calibrator->num_samples, sizeof(int), 1, fp);
    fwrite(&calibrator->calibration_complete, sizeof(bool), 1, fp);

    /* Write running min/max values */
    int num_values = calibrator->per_channel ? calibrator->num_channels : 1;
    if (calibrator->running_min) {
        fwrite(calibrator->running_min, sizeof(float), (size_t)num_values, fp);
    }
    if (calibrator->running_max) {
        fwrite(calibrator->running_max, sizeof(float), (size_t)num_values, fp);
    }

    /* Write histogram if present */
    fwrite(&calibrator->num_bins, sizeof(int), 1, fp);
    if (calibrator->histogram && calibrator->num_bins > 0) {
        fwrite(&calibrator->hist_min, sizeof(float), 1, fp);
        fwrite(&calibrator->hist_max, sizeof(float), 1, fp);
        fwrite(&calibrator->bin_width, sizeof(float), 1, fp);
        fwrite(calibrator->histogram, sizeof(int),
               (size_t)calibrator->num_bins, fp);
    }

    fclose(fp);
    return true;
}

/* 21. nimcp_trt_load_calibration_cache */
bool nimcp_trt_load_calibration_cache(
    nimcp_int8_calibrator_t* calibrator,
    const char* input_path)
{
    if (!calibrator || !input_path) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
            "nimcp_trt_load_calibration_cache: required parameter is NULL");
        return false;
    }

    FILE* fp = fopen(input_path, "rb");
    if (!fp) {
        return false;
    }

    /* Read and verify header */
    uint32_t magic = 0;
    uint32_t version = 0;

    if (fread(&magic, sizeof(uint32_t), 1, fp) != 1 || magic != 0x43414C42) {
        fclose(fp);
        return false;
    }
    if (fread(&version, sizeof(uint32_t), 1, fp) != 1 || version != 1) {
        fclose(fp);
        return false;
    }

    /* Read calibrator metadata */
    int method_int = 0;
    if (fread(&method_int, sizeof(int), 1, fp) != 1) { fclose(fp); return false; }
    calibrator->method = (nimcp_int8_calib_method_t)method_int;

    if (fread(&calibrator->per_channel, sizeof(bool), 1, fp) != 1) { fclose(fp); return false; }
    if (fread(&calibrator->num_channels, sizeof(int), 1, fp) != 1) { fclose(fp); return false; }
    if (fread(&calibrator->num_samples, sizeof(int), 1, fp) != 1) { fclose(fp); return false; }
    if (fread(&calibrator->calibration_complete, sizeof(bool), 1, fp) != 1) { fclose(fp); return false; }

    /* Read running min/max */
    int num_values = calibrator->per_channel ? calibrator->num_channels : 1;
    if (num_values > 0) {
        if (calibrator->running_min) {
            if (fread(calibrator->running_min, sizeof(float),
                      (size_t)num_values, fp) != (size_t)num_values) {
                fclose(fp);
                return false;
            }
        }
        if (calibrator->running_max) {
            if (fread(calibrator->running_max, sizeof(float),
                      (size_t)num_values, fp) != (size_t)num_values) {
                fclose(fp);
                return false;
            }
        }
    }

    /* Read histogram if present */
    int num_bins = 0;
    if (fread(&num_bins, sizeof(int), 1, fp) != 1) { fclose(fp); return false; }

    if (num_bins > 0 && calibrator->histogram && calibrator->num_bins >= num_bins) {
        if (fread(&calibrator->hist_min, sizeof(float), 1, fp) != 1) { fclose(fp); return false; }
        if (fread(&calibrator->hist_max, sizeof(float), 1, fp) != 1) { fclose(fp); return false; }
        if (fread(&calibrator->bin_width, sizeof(float), 1, fp) != 1) { fclose(fp); return false; }
        if (fread(calibrator->histogram, sizeof(int),
                  (size_t)num_bins, fp) != (size_t)num_bins) {
            fclose(fp);
            return false;
        }
        calibrator->num_bins = num_bins;
    }

    fclose(fp);
    return true;
}

/*=============================================================================
 * Utility Functions (12 functions)
 *=============================================================================*/

/* 22. nimcp_trt_version */
const char* nimcp_trt_version(void) {
    return "cpu-fallback-1.0.0";
}

/* 23. nimcp_trt_available */
bool nimcp_trt_available(void) {
    return false;  /* TensorRT not available in CPU fallback */
}

/* 24. nimcp_trt_int8_supported */
bool nimcp_trt_int8_supported(nimcp_gpu_context_t* ctx) {
    (void)ctx;
    return false;  /* INT8 tensor cores not available on CPU */
}

/* 25. nimcp_trt_dla_available */
int nimcp_trt_dla_available(nimcp_gpu_context_t* ctx) {
    (void)ctx;
    return 0;  /* No DLA cores available on CPU */
}

/* 26. nimcp_trt_format_name */
const char* nimcp_trt_format_name(nimcp_trt_format_t format) {
    switch (format) {
        case TRT_FORMAT_ENGINE:     return "TensorRT Engine (.engine/.trt)";
        case TRT_FORMAT_ONNX:       return "ONNX with Quantization (.onnx)";
        case TRT_FORMAT_CALIB_CACHE: return "Calibration Cache (.json)";
        default:                    return "Unknown Format";
    }
}

/* 27. nimcp_trt_precision_name */
const char* nimcp_trt_precision_name(nimcp_trt_precision_t precision) {
    switch (precision) {
        case TRT_PRECISION_FP32:    return "FP32";
        case TRT_PRECISION_FP16:    return "FP16";
        case TRT_PRECISION_INT8:    return "INT8";
        case TRT_PRECISION_TF32:    return "TF32";
        default:                    return "Unknown Precision";
    }
}

/* 28. nimcp_trt_layer_type_name */
const char* nimcp_trt_layer_type_name(nimcp_trt_layer_type_t type) {
    switch (type) {
        case TRT_LAYER_DENSE:       return "Dense (FC)";
        case TRT_LAYER_CONV2D:      return "Conv2D";
        case TRT_LAYER_CONV2D_BN:   return "Conv2D+BatchNorm";
        case TRT_LAYER_ACTIVATION:  return "Activation";
        case TRT_LAYER_POOLING:     return "Pooling";
        case TRT_LAYER_SOFTMAX:     return "Softmax";
        case TRT_LAYER_RESIDUAL_ADD: return "Residual Add";
        case TRT_LAYER_LAYERNORM:   return "LayerNorm";
        case TRT_LAYER_ATTENTION:   return "Self-Attention";
        default:                    return "Unknown Layer";
    }
}

/* 29. nimcp_trt_activation_name */
const char* nimcp_trt_activation_name(nimcp_trt_activation_t activation) {
    switch (activation) {
        case TRT_ACTIVATION_RELU:    return "ReLU";
        case TRT_ACTIVATION_SIGMOID: return "Sigmoid";
        case TRT_ACTIVATION_TANH:    return "Tanh";
        case TRT_ACTIVATION_GELU:    return "GELU";
        case TRT_ACTIVATION_SILU:    return "SiLU";
        case TRT_ACTIVATION_RELU6:   return "ReLU6";
        case TRT_ACTIVATION_NONE:    return "None";
        default:                     return "Unknown Activation";
    }
}

/* 30. nimcp_trt_print_result */
void nimcp_trt_print_result(const nimcp_trt_export_result_t* result) {
    if (!result) {
        printf("[TRT Export] Result: NULL\n");
        return;
    }

    printf("=== TensorRT Export Result ===\n");
    printf("  Success:              %s\n", result->success ? "YES" : "NO");

    if (!result->success) {
        printf("  Error:                %s\n", result->error_message);
    }

    if (result->engine_size > 0) {
        printf("  Engine Size:          %zu bytes (%.2f MB)\n",
               result->engine_size,
               (float)result->engine_size / (1024.0f * 1024.0f));
    }

    if (result->output_path[0] != '\0') {
        printf("  Output Path:          %s\n", result->output_path);
    }

    printf("  Est. Latency:         %.3f ms\n", result->estimated_latency_ms);
    printf("  Est. Throughput:      %.1f samples/sec\n", result->estimated_throughput);
    printf("  Est. Memory:          %zu MB\n", result->estimated_memory_mb);
    printf("==============================\n");
}

/* 31. nimcp_trt_estimate_performance */
int nimcp_trt_estimate_performance(
    const nimcp_trt_network_def_t* network,
    const nimcp_trt_config_t* config,
    float* latency_ms,
    float* throughput,
    size_t* memory_mb)
{
    if (!network || !config) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
            "nimcp_trt_estimate_performance: required parameter is NULL");
        return -1;
    }

    /* Compute total FLOPs across all layers */
    float total_flops = 0.0f;
    size_t total_param_bytes = 0;

    for (int i = 0; i < network->num_layers; i++) {
        const nimcp_trt_layer_def_t* layer = &network->layers[i];
        total_flops += estimate_layer_flops(layer);

        /* Estimate parameter memory */
        size_t layer_params = 0;
        switch (layer->type) {
            case TRT_LAYER_DENSE:
                layer_params = (size_t)layer->in_features * (size_t)layer->out_features;
                if (layer->bias) {
                    layer_params += (size_t)layer->out_features;
                }
                break;
            case TRT_LAYER_CONV2D:
            case TRT_LAYER_CONV2D_BN:
                layer_params = (size_t)layer->out_features * (size_t)layer->in_features *
                               (size_t)layer->kernel_h * (size_t)layer->kernel_w;
                if (layer->bias) {
                    layer_params += (size_t)layer->out_features;
                }
                break;
            default:
                break;
        }

        /* Bytes per parameter depends on precision */
        size_t bytes_per_param;
        switch (config->precision) {
            case TRT_PRECISION_INT8:  bytes_per_param = 1; break;
            case TRT_PRECISION_FP16:  bytes_per_param = 2; break;
            case TRT_PRECISION_FP32:  bytes_per_param = 4; break;
            case TRT_PRECISION_TF32:  bytes_per_param = 4; break;
            default:                  bytes_per_param = 4; break;
        }
        total_param_bytes += layer_params * bytes_per_param;
    }

    /*
     * Rough performance estimates for CPU fallback:
     * - CPU baseline: ~10 GFLOPS for FP32
     * - Multiply by batch size for throughput
     */
    float cpu_gflops = 10.0f;  /* Conservative CPU GFLOPS estimate */

    /* Precision multipliers (CPU does everything in FP32, but estimate GPU perf) */
    float precision_multiplier = 1.0f;
    switch (config->precision) {
        case TRT_PRECISION_INT8:  precision_multiplier = 4.0f; break;
        case TRT_PRECISION_FP16:  precision_multiplier = 2.0f; break;
        case TRT_PRECISION_TF32:  precision_multiplier = 1.5f; break;
        default:                  precision_multiplier = 1.0f; break;
    }

    /* Estimated latency in ms for one sample */
    float est_latency = 0.0f;
    if (cpu_gflops > 0.0f) {
        est_latency = (total_flops / (cpu_gflops * 1e9f * precision_multiplier)) * 1000.0f;
    }
    /* Minimum latency floor */
    if (est_latency < 0.001f && network->num_layers > 0) {
        est_latency = 0.001f;
    }

    /* Estimated throughput in samples/sec */
    float est_throughput = 0.0f;
    if (est_latency > 0.0f) {
        est_throughput = 1000.0f / est_latency;
        /* Batch processing improves throughput */
        est_throughput *= (float)config->max_batch_size * 0.8f;  /* 80% batch efficiency */
    }

    /* Estimated memory in MB: params + workspace + activations */
    size_t est_memory = total_param_bytes / (1024 * 1024);  /* Parameter memory */
    est_memory += config->max_workspace_size / (1024 * 1024);  /* Workspace */
    /* Activation memory: rough estimate based on largest layer */
    size_t max_activation = 0;
    for (int i = 0; i < network->num_layers; i++) {
        size_t act_size = (size_t)network->layers[i].out_features *
                          (size_t)config->max_batch_size * sizeof(float);
        if (act_size > max_activation) {
            max_activation = act_size;
        }
    }
    est_memory += max_activation / (1024 * 1024);
    /* Minimum 1 MB */
    if (est_memory == 0 && network->num_layers > 0) {
        est_memory = 1;
    }

    /* Write outputs (only if pointers are non-NULL) */
    if (latency_ms) *latency_ms = est_latency;
    if (throughput) *throughput = est_throughput;
    if (memory_mb) *memory_mb = est_memory;

    return 0;
}
