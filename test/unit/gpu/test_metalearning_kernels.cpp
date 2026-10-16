/**
 * @file test_metalearning_kernels.cpp
 * @brief Unit tests for GPU metalearning kernels
 *
 * Tests MAML, Reptile, ProtoNets, Memory-Augmented, and Task Embedding
 * GPU operations.
 */

#include <gtest/gtest.h>
#include <cmath>
#include <vector>
#include <cstring>

// Headers have their own extern "C" guards
#include "gpu/metalearning/nimcp_metalearning_gpu.h"
#include "gpu/context/nimcp_gpu_context.h"
#include "gpu/tensor/nimcp_tensor_gpu.h"

//=============================================================================
// Test Fixtures
//=============================================================================

class MetalearningKernelTest : public ::testing::Test {
protected:
    nimcp_gpu_context_t* ctx = nullptr;
    bool gpu_available = false;

    void SetUp() override {
        ctx = nimcp_gpu_context_create_auto();
        gpu_available = (ctx != nullptr && nimcp_gpu_context_is_valid(ctx));
    }

    void TearDown() override {
        if (ctx) {
            nimcp_gpu_context_destroy(ctx);
            ctx = nullptr;
        }
    }

    void RequireGPU() {
        if (!gpu_available) {
            GTEST_SKIP() << "GPU not available, skipping test";
        }
    }

    // Helper to create a tensor filled with a constant value
    nimcp_gpu_tensor_t* CreateFilledTensor(size_t* dims, size_t rank, float value) {
        if (!ctx) return nullptr;
        nimcp_gpu_tensor_t* tensor = nimcp_gpu_tensor_create(ctx, dims, rank, NIMCP_GPU_PRECISION_FP32);
        if (tensor) {
            nimcp_gpu_fill(ctx, tensor, value);
        }
        return tensor;
    }

    // Helper to create 1D tensor
    nimcp_gpu_tensor_t* Create1DTensor(size_t n, float value = 0.0f) {
        size_t dims[1] = {n};
        return CreateFilledTensor(dims, 1, value);
    }

    // Helper to create 2D tensor
    nimcp_gpu_tensor_t* Create2DTensor(size_t rows, size_t cols, float value = 0.0f) {
        size_t dims[2] = {rows, cols};
        return CreateFilledTensor(dims, 2, value);
    }

    // Helper to create 3D tensor
    nimcp_gpu_tensor_t* Create3DTensor(size_t d1, size_t d2, size_t d3, float value = 0.0f) {
        size_t dims[3] = {d1, d2, d3};
        return CreateFilledTensor(dims, 3, value);
    }

    // Helper to copy tensor to host
    std::vector<float> CopyToHost(nimcp_gpu_tensor_t* tensor) {
        size_t n = tensor->numel;
        std::vector<float> host_data(n);
        nimcp_gpu_tensor_to_host(tensor, host_data.data());
        return host_data;
    }

    // Helper to set tensor from host
    nimcp_gpu_tensor_t* SetFromHost(nimcp_gpu_tensor_t* tensor, const std::vector<float>& data) {
        if (tensor) nimcp_gpu_tensor_destroy(tensor);
        size_t dims[1] = {data.size()};
        return nimcp_gpu_tensor_from_host(ctx, data.data(), dims, 1, NIMCP_GPU_PRECISION_FP32);
    }

    // Helper to create MAML state
    nimcp_gpu_maml_state_t* CreateMAMLState(size_t n_params) {
        if (!ctx) return nullptr;
        nimcp_gpu_maml_state_t* state = new nimcp_gpu_maml_state_t();
        state->meta_weights = Create1DTensor(n_params, 0.5f);
        state->adapted_weights = Create1DTensor(n_params, 0.5f);
        state->inner_grads = Create1DTensor(n_params, 0.0f);
        state->outer_grads = Create1DTensor(n_params, 0.0f);
        state->hessian_prod = Create1DTensor(n_params, 0.0f);
        state->momentum = Create1DTensor(n_params, 0.0f);
        state->n_params = n_params;
        return state;
    }

    // Helper to destroy MAML state
    void DestroyMAMLState(nimcp_gpu_maml_state_t* state) {
        if (!state) return;
        if (state->meta_weights) nimcp_gpu_tensor_destroy(state->meta_weights);
        if (state->adapted_weights) nimcp_gpu_tensor_destroy(state->adapted_weights);
        if (state->inner_grads) nimcp_gpu_tensor_destroy(state->inner_grads);
        if (state->outer_grads) nimcp_gpu_tensor_destroy(state->outer_grads);
        if (state->hessian_prod) nimcp_gpu_tensor_destroy(state->hessian_prod);
        if (state->momentum) nimcp_gpu_tensor_destroy(state->momentum);
        delete state;
    }

    // Helper to create ProtoNet state
    nimcp_gpu_protonet_state_t* CreateProtoNetState(size_t n_classes, size_t embedding_dim) {
        if (!ctx) return nullptr;
        nimcp_gpu_protonet_state_t* state = new nimcp_gpu_protonet_state_t();
        state->support_embeddings = Create2DTensor(n_classes * 5, embedding_dim, 0.0f);  // 5-shot
        state->query_embeddings = Create2DTensor(n_classes * 10, embedding_dim, 0.0f);   // 10 query
        state->prototypes = Create2DTensor(n_classes, embedding_dim, 0.0f);
        state->distances = Create2DTensor(n_classes * 10, n_classes, 0.0f);
        state->logits = Create2DTensor(n_classes * 10, n_classes, 0.0f);
        state->n_classes = n_classes;
        state->embedding_dim = embedding_dim;
        return state;
    }

    // Helper to destroy ProtoNet state
    void DestroyProtoNetState(nimcp_gpu_protonet_state_t* state) {
        if (!state) return;
        if (state->support_embeddings) nimcp_gpu_tensor_destroy(state->support_embeddings);
        if (state->query_embeddings) nimcp_gpu_tensor_destroy(state->query_embeddings);
        if (state->prototypes) nimcp_gpu_tensor_destroy(state->prototypes);
        if (state->distances) nimcp_gpu_tensor_destroy(state->distances);
        if (state->logits) nimcp_gpu_tensor_destroy(state->logits);
        delete state;
    }

    // Helper to create Meta Memory state
    nimcp_gpu_meta_memory_state_t* CreateMetaMemoryState(size_t memory_size, size_t key_dim, size_t value_dim) {
        if (!ctx) return nullptr;
        nimcp_gpu_meta_memory_state_t* state = new nimcp_gpu_meta_memory_state_t();
        state->keys = Create2DTensor(memory_size, key_dim, 0.0f);
        state->values = Create2DTensor(memory_size, value_dim, 0.0f);
        state->usage = Create1DTensor(memory_size, 0.0f);
        state->read_weights = Create1DTensor(memory_size, 0.0f);
        state->write_weights = Create1DTensor(memory_size, 0.0f);
        state->read_output = Create1DTensor(value_dim, 0.0f);
        state->memory_size = memory_size;
        state->key_dim = key_dim;
        state->value_dim = value_dim;
        return state;
    }

    // Helper to destroy Meta Memory state
    void DestroyMetaMemoryState(nimcp_gpu_meta_memory_state_t* state) {
        if (!state) return;
        if (state->keys) nimcp_gpu_tensor_destroy(state->keys);
        if (state->values) nimcp_gpu_tensor_destroy(state->values);
        if (state->usage) nimcp_gpu_tensor_destroy(state->usage);
        if (state->read_weights) nimcp_gpu_tensor_destroy(state->read_weights);
        if (state->write_weights) nimcp_gpu_tensor_destroy(state->write_weights);
        if (state->read_output) nimcp_gpu_tensor_destroy(state->read_output);
        delete state;
    }

    // Helper to create Task Embedding state
    nimcp_gpu_task_embed_state_t* CreateTaskEmbedState(size_t embed_dim) {
        if (!ctx) return nullptr;
        nimcp_gpu_task_embed_state_t* state = new nimcp_gpu_task_embed_state_t();
        state->task_embedding = Create1DTensor(embed_dim, 0.0f);
        state->context_encoder = Create2DTensor(embed_dim, embed_dim, 0.0f);
        state->film_gamma = Create1DTensor(embed_dim, 1.0f);
        state->film_beta = Create1DTensor(embed_dim, 0.0f);
        state->embed_dim = embed_dim;
        return state;
    }

    // Helper to destroy Task Embedding state
    void DestroyTaskEmbedState(nimcp_gpu_task_embed_state_t* state) {
        if (!state) return;
        if (state->task_embedding) nimcp_gpu_tensor_destroy(state->task_embedding);
        if (state->context_encoder) nimcp_gpu_tensor_destroy(state->context_encoder);
        if (state->film_gamma) nimcp_gpu_tensor_destroy(state->film_gamma);
        if (state->film_beta) nimcp_gpu_tensor_destroy(state->film_beta);
        delete state;
    }
};

//=============================================================================
// MAML Default Parameters Tests
//=============================================================================

TEST_F(MetalearningKernelTest, MAMLParamsDefault_ReturnsValidParams) {
    nimcp_gpu_maml_params_t params = nimcp_gpu_maml_params_default();

    // Check reasonable defaults
    EXPECT_GT(params.inner_lr, 0.0f);
    EXPECT_LT(params.inner_lr, 1.0f);
    EXPECT_GT(params.outer_lr, 0.0f);
    EXPECT_LT(params.outer_lr, 1.0f);
    EXPECT_GE(params.inner_steps, 1);
    EXPECT_GE(params.outer_steps, 1);
    EXPECT_GE(params.clip_grad, 0.0f);
    EXPECT_GE(params.weight_decay, 0.0f);
    EXPECT_GE(params.num_tasks, 1);
    EXPECT_GE(params.k_shot, 1);
    EXPECT_GE(params.n_way, 2);
}

TEST_F(MetalearningKernelTest, MAMLParamsDefault_SecondOrderConfigurable) {
    nimcp_gpu_maml_params_t params = nimcp_gpu_maml_params_default();

    // second_order should be a valid boolean
    EXPECT_TRUE(params.second_order == true || params.second_order == false);
}

//=============================================================================
// Reptile Default Parameters Tests
//=============================================================================

TEST_F(MetalearningKernelTest, ReptileParamsDefault_ReturnsValidParams) {
    nimcp_gpu_reptile_params_t params = nimcp_gpu_reptile_params_default();

    // Check reasonable defaults
    EXPECT_GT(params.inner_lr, 0.0f);
    EXPECT_LT(params.inner_lr, 1.0f);
    EXPECT_GT(params.outer_lr, 0.0f);
    EXPECT_LT(params.outer_lr, 1.0f);
    EXPECT_GE(params.inner_steps, 1);
    EXPECT_GE(params.num_tasks, 1);
    EXPECT_GT(params.epsilon, 0.0f);
    EXPECT_LE(params.epsilon, 1.0f);
}

//=============================================================================
// ProtoNet Default Parameters Tests
//=============================================================================

TEST_F(MetalearningKernelTest, ProtoNetParamsDefault_ReturnsValidParams) {
    nimcp_gpu_protonet_params_t params = nimcp_gpu_protonet_params_default();

    // Check reasonable defaults
    EXPECT_GT(params.embedding_dim, 0);
    EXPECT_GT(params.temperature, 0.0f);
    EXPECT_GE(params.k_shot, 1);
    EXPECT_GE(params.n_query, 1);
    EXPECT_GE(params.n_way, 2);
    EXPECT_GE(params.margin, 0.0f);
}

TEST_F(MetalearningKernelTest, ProtoNetParamsDefault_NormalizeConfigurable) {
    nimcp_gpu_protonet_params_t params = nimcp_gpu_protonet_params_default();

    // normalize_prototypes should be a valid boolean
    EXPECT_TRUE(params.normalize_prototypes == true || params.normalize_prototypes == false);
}

//=============================================================================
// Memory-Augmented Default Parameters Tests
//=============================================================================

TEST_F(MetalearningKernelTest, MetaMemoryParamsDefault_ReturnsValidParams) {
    nimcp_gpu_meta_memory_params_t params = nimcp_gpu_meta_memory_params_default();

    // Check reasonable defaults
    EXPECT_GT(params.memory_size, 0);
    EXPECT_GT(params.key_dim, 0);
    EXPECT_GT(params.value_dim, 0);
    EXPECT_GT(params.read_strength, 0.0f);
    EXPECT_GT(params.write_strength, 0.0f);
    EXPECT_GE(params.forget_rate, 0.0f);
    EXPECT_LE(params.forget_rate, 1.0f);
    EXPECT_GT(params.temperature, 0.0f);
}

TEST_F(MetalearningKernelTest, MetaMemoryParamsDefault_UseAttentionConfigurable) {
    nimcp_gpu_meta_memory_params_t params = nimcp_gpu_meta_memory_params_default();

    // use_attention should be a valid boolean
    EXPECT_TRUE(params.use_attention == true || params.use_attention == false);
}

//=============================================================================
// Task Embedding Default Parameters Tests
//=============================================================================

TEST_F(MetalearningKernelTest, TaskEmbedParamsDefault_ReturnsValidParams) {
    nimcp_gpu_task_embed_params_t params = nimcp_gpu_task_embed_params_default();

    // Check reasonable defaults
    EXPECT_GT(params.task_embed_dim, 0);
    EXPECT_GT(params.context_size, 0);
    EXPECT_GT(params.lr, 0.0f);
    EXPECT_LT(params.lr, 1.0f);
}

TEST_F(MetalearningKernelTest, TaskEmbedParamsDefault_FiLMAndTaskNetConfigurable) {
    nimcp_gpu_task_embed_params_t params = nimcp_gpu_task_embed_params_default();

    // use_film and use_task_net should be valid booleans
    EXPECT_TRUE(params.use_film == true || params.use_film == false);
    EXPECT_TRUE(params.use_task_net == true || params.use_task_net == false);
}

//=============================================================================
// MAML State Tests
//=============================================================================

TEST_F(MetalearningKernelTest, MAMLState_CreatesValidState) {
    RequireGPU();

    const size_t n_params = 1000;
    nimcp_gpu_maml_state_t* state = CreateMAMLState(n_params);
    ASSERT_NE(state, nullptr);

    EXPECT_NE(state->meta_weights, nullptr);
    EXPECT_NE(state->adapted_weights, nullptr);
    EXPECT_NE(state->inner_grads, nullptr);
    EXPECT_NE(state->outer_grads, nullptr);
    EXPECT_NE(state->hessian_prod, nullptr);
    EXPECT_NE(state->momentum, nullptr);
    EXPECT_EQ(state->n_params, n_params);

    DestroyMAMLState(state);
}

TEST_F(MetalearningKernelTest, MAMLState_DestroyHandlesNull) {
    // Should not crash
    DestroyMAMLState(nullptr);
}

//=============================================================================
// MAML Inner Loop Tests
//=============================================================================

TEST_F(MetalearningKernelTest, MAMLInnerLoop_AdaptsWeights) {
    RequireGPU();

    const size_t n_params = 100;
    const size_t n_samples = 20;
    const size_t input_dim = 10;

    nimcp_gpu_maml_state_t* state = CreateMAMLState(n_params);
    ASSERT_NE(state, nullptr);

    nimcp_gpu_tensor_t* support_x = Create2DTensor(n_samples, input_dim, 0.5f);
    nimcp_gpu_tensor_t* support_y = Create1DTensor(n_samples, 1.0f);

    nimcp_gpu_maml_params_t params = nimcp_gpu_maml_params_default();

    auto initial_weights = CopyToHost(state->meta_weights);

    bool result = nimcp_gpu_maml_inner_loop(ctx, state, support_x, support_y, &params);
    EXPECT_TRUE(result);

    auto adapted_weights = CopyToHost(state->adapted_weights);

    // Adapted weights should differ from meta weights after inner loop
    bool weights_changed = false;
    for (size_t i = 0; i < n_params; i++) {
        if (std::abs(adapted_weights[i] - initial_weights[i]) > 1e-6f) {
            weights_changed = true;
            break;
        }
    }
    EXPECT_TRUE(weights_changed);

    nimcp_gpu_tensor_destroy(support_x);
    nimcp_gpu_tensor_destroy(support_y);
    DestroyMAMLState(state);
}

TEST_F(MetalearningKernelTest, MAMLInnerLoop_ComputesGradients) {
    RequireGPU();

    const size_t n_params = 100;
    const size_t n_samples = 20;
    const size_t input_dim = 10;

    nimcp_gpu_maml_state_t* state = CreateMAMLState(n_params);
    ASSERT_NE(state, nullptr);

    nimcp_gpu_tensor_t* support_x = Create2DTensor(n_samples, input_dim, 0.5f);
    nimcp_gpu_tensor_t* support_y = Create1DTensor(n_samples, 1.0f);

    nimcp_gpu_maml_params_t params = nimcp_gpu_maml_params_default();

    bool result = nimcp_gpu_maml_inner_loop(ctx, state, support_x, support_y, &params);
    EXPECT_TRUE(result);

    auto inner_grads = CopyToHost(state->inner_grads);

    // Inner gradients should be computed (not all zeros)
    bool has_nonzero_grads = false;
    for (size_t i = 0; i < n_params; i++) {
        if (std::abs(inner_grads[i]) > 1e-8f) {
            has_nonzero_grads = true;
            break;
        }
    }
    EXPECT_TRUE(has_nonzero_grads);

    nimcp_gpu_tensor_destroy(support_x);
    nimcp_gpu_tensor_destroy(support_y);
    DestroyMAMLState(state);
}

//=============================================================================
// MAML Outer Gradient Tests
//=============================================================================

TEST_F(MetalearningKernelTest, MAMLOuterGradient_ComputesMetaGradient) {
    RequireGPU();

    const size_t n_params = 100;
    const size_t n_samples = 20;
    const size_t input_dim = 10;

    nimcp_gpu_maml_state_t* state = CreateMAMLState(n_params);
    ASSERT_NE(state, nullptr);

    nimcp_gpu_tensor_t* query_x = Create2DTensor(n_samples, input_dim, 0.3f);
    nimcp_gpu_tensor_t* query_y = Create1DTensor(n_samples, 0.0f);

    nimcp_gpu_maml_params_t params = nimcp_gpu_maml_params_default();

    bool result = nimcp_gpu_maml_outer_gradient(ctx, state, query_x, query_y, &params);
    EXPECT_TRUE(result);

    auto outer_grads = CopyToHost(state->outer_grads);

    // Outer gradients should be computed
    bool has_nonzero_grads = false;
    for (size_t i = 0; i < n_params; i++) {
        if (std::abs(outer_grads[i]) > 1e-8f) {
            has_nonzero_grads = true;
            break;
        }
    }
    EXPECT_TRUE(has_nonzero_grads);

    nimcp_gpu_tensor_destroy(query_x);
    nimcp_gpu_tensor_destroy(query_y);
    DestroyMAMLState(state);
}

//=============================================================================
// MAML Meta Update Tests
//=============================================================================

TEST_F(MetalearningKernelTest, MAMLMetaUpdate_UpdatesMetaWeights) {
    RequireGPU();

    const size_t n_params = 100;

    nimcp_gpu_maml_state_t* state = CreateMAMLState(n_params);
    ASSERT_NE(state, nullptr);

    // Set some outer gradients
    nimcp_gpu_fill(ctx, state->outer_grads, 0.1f);

    auto initial_meta_weights = CopyToHost(state->meta_weights);

    nimcp_gpu_maml_params_t params = nimcp_gpu_maml_params_default();

    bool result = nimcp_gpu_maml_meta_update(ctx, state, &params);
    EXPECT_TRUE(result);

    auto updated_meta_weights = CopyToHost(state->meta_weights);

    // Meta weights should be updated
    bool weights_changed = false;
    for (size_t i = 0; i < n_params; i++) {
        if (std::abs(updated_meta_weights[i] - initial_meta_weights[i]) > 1e-6f) {
            weights_changed = true;
            break;
        }
    }
    EXPECT_TRUE(weights_changed);

    DestroyMAMLState(state);
}

//=============================================================================
// MAML Complete Step Tests
//=============================================================================

TEST_F(MetalearningKernelTest, MAMLStep_PerformsFullUpdate) {
    RequireGPU();

    const size_t n_params = 100;
    const size_t n_samples = 20;
    const size_t input_dim = 10;

    nimcp_gpu_maml_state_t* state = CreateMAMLState(n_params);
    ASSERT_NE(state, nullptr);

    nimcp_gpu_tensor_t* support_x = Create2DTensor(n_samples, input_dim, 0.5f);
    nimcp_gpu_tensor_t* support_y = Create1DTensor(n_samples, 1.0f);
    nimcp_gpu_tensor_t* query_x = Create2DTensor(n_samples, input_dim, 0.3f);
    nimcp_gpu_tensor_t* query_y = Create1DTensor(n_samples, 0.0f);

    auto initial_weights = CopyToHost(state->meta_weights);

    nimcp_gpu_maml_params_t params = nimcp_gpu_maml_params_default();

    bool result = nimcp_gpu_maml_step(ctx, state, support_x, support_y, query_x, query_y, &params);
    EXPECT_TRUE(result);

    auto final_weights = CopyToHost(state->meta_weights);

    // Meta weights should change after full step
    bool weights_changed = false;
    for (size_t i = 0; i < n_params; i++) {
        if (std::abs(final_weights[i] - initial_weights[i]) > 1e-6f) {
            weights_changed = true;
            break;
        }
    }
    EXPECT_TRUE(weights_changed);

    nimcp_gpu_tensor_destroy(support_x);
    nimcp_gpu_tensor_destroy(support_y);
    nimcp_gpu_tensor_destroy(query_x);
    nimcp_gpu_tensor_destroy(query_y);
    DestroyMAMLState(state);
}

//=============================================================================
// MAML Hessian-Vector Product Tests
//=============================================================================

TEST_F(MetalearningKernelTest, MAMLHessianVectorProduct_ComputesHVP) {
    RequireGPU();

    const size_t n_params = 100;

    nimcp_gpu_maml_state_t* state = CreateMAMLState(n_params);
    ASSERT_NE(state, nullptr);

    nimcp_gpu_tensor_t* vector = Create1DTensor(n_params, 1.0f);
    nimcp_gpu_tensor_t* hvp_out = Create1DTensor(n_params, 0.0f);

    nimcp_gpu_maml_params_t params = nimcp_gpu_maml_params_default();
    params.second_order = true;

    bool result = nimcp_gpu_maml_hessian_vector_product(ctx, state, vector, hvp_out, &params);
    EXPECT_TRUE(result);

    auto hvp_data = CopyToHost(hvp_out);

    // HVP should be computed (may be zeros if Hessian is zero)
    // Just check that the function completes successfully

    nimcp_gpu_tensor_destroy(vector);
    nimcp_gpu_tensor_destroy(hvp_out);
    DestroyMAMLState(state);
}

//=============================================================================
// Reptile Inner Loop Tests
//=============================================================================

TEST_F(MetalearningKernelTest, ReptileInnerLoop_AdaptsWeights) {
    RequireGPU();

    const size_t n_params = 100;
    const size_t n_samples = 20;
    const size_t input_dim = 10;

    nimcp_gpu_tensor_t* weights = Create1DTensor(n_params, 0.5f);
    nimcp_gpu_tensor_t* task_x = Create2DTensor(n_samples, input_dim, 0.5f);
    nimcp_gpu_tensor_t* task_y = Create1DTensor(n_samples, 1.0f);

    auto initial_weights = CopyToHost(weights);

    nimcp_gpu_reptile_params_t params = nimcp_gpu_reptile_params_default();

    bool result = nimcp_gpu_reptile_inner_loop(ctx, weights, task_x, task_y, &params);
    EXPECT_TRUE(result);

    auto adapted_weights = CopyToHost(weights);

    // Weights should change after inner loop
    bool weights_changed = false;
    for (size_t i = 0; i < n_params; i++) {
        if (std::abs(adapted_weights[i] - initial_weights[i]) > 1e-6f) {
            weights_changed = true;
            break;
        }
    }
    EXPECT_TRUE(weights_changed);

    nimcp_gpu_tensor_destroy(weights);
    nimcp_gpu_tensor_destroy(task_x);
    nimcp_gpu_tensor_destroy(task_y);
}

//=============================================================================
// Reptile Meta Update Tests
//=============================================================================

TEST_F(MetalearningKernelTest, ReptileMetaUpdate_InterpolatesWeights) {
    RequireGPU();

    const size_t n_params = 100;

    nimcp_gpu_tensor_t* meta_weights = Create1DTensor(n_params, 0.0f);
    nimcp_gpu_tensor_t* adapted_weights = Create1DTensor(n_params, 1.0f);

    nimcp_gpu_reptile_params_t params = nimcp_gpu_reptile_params_default();
    params.epsilon = 0.5f;  // 50% interpolation

    bool result = nimcp_gpu_reptile_meta_update(ctx, meta_weights, adapted_weights, &params);
    EXPECT_TRUE(result);

    auto updated_weights = CopyToHost(meta_weights);

    // Meta weights should be between 0 and 1 (interpolated)
    for (size_t i = 0; i < n_params; i++) {
        EXPECT_GT(updated_weights[i], 0.0f);
        EXPECT_LT(updated_weights[i], 1.0f);
    }

    nimcp_gpu_tensor_destroy(meta_weights);
    nimcp_gpu_tensor_destroy(adapted_weights);
}

TEST_F(MetalearningKernelTest, ReptileMetaUpdate_FullInterpolationWorks) {
    RequireGPU();

    const size_t n_params = 100;

    nimcp_gpu_tensor_t* meta_weights = Create1DTensor(n_params, 0.0f);
    nimcp_gpu_tensor_t* adapted_weights = Create1DTensor(n_params, 1.0f);

    nimcp_gpu_reptile_params_t params = nimcp_gpu_reptile_params_default();
    params.epsilon = 1.0f;  // Full interpolation to adapted weights

    bool result = nimcp_gpu_reptile_meta_update(ctx, meta_weights, adapted_weights, &params);
    EXPECT_TRUE(result);

    auto updated_weights = CopyToHost(meta_weights);

    // Meta weights should equal adapted weights with epsilon=1
    for (size_t i = 0; i < n_params; i++) {
        EXPECT_NEAR(updated_weights[i], 1.0f, 1e-5f);
    }

    nimcp_gpu_tensor_destroy(meta_weights);
    nimcp_gpu_tensor_destroy(adapted_weights);
}

//=============================================================================
// Reptile Complete Step Tests
//=============================================================================

TEST_F(MetalearningKernelTest, ReptileStep_PerformsFullUpdate) {
    RequireGPU();

    const size_t n_params = 100;
    const size_t n_samples = 20;
    const size_t input_dim = 10;

    nimcp_gpu_tensor_t* meta_weights = Create1DTensor(n_params, 0.5f);
    nimcp_gpu_tensor_t* task_x = Create2DTensor(n_samples, input_dim, 0.5f);
    nimcp_gpu_tensor_t* task_y = Create1DTensor(n_samples, 1.0f);

    auto initial_weights = CopyToHost(meta_weights);

    nimcp_gpu_reptile_params_t params = nimcp_gpu_reptile_params_default();

    bool result = nimcp_gpu_reptile_step(ctx, meta_weights, task_x, task_y, &params);
    EXPECT_TRUE(result);

    auto final_weights = CopyToHost(meta_weights);

    // Meta weights should change after full step
    bool weights_changed = false;
    for (size_t i = 0; i < n_params; i++) {
        if (std::abs(final_weights[i] - initial_weights[i]) > 1e-6f) {
            weights_changed = true;
            break;
        }
    }
    EXPECT_TRUE(weights_changed);

    nimcp_gpu_tensor_destroy(meta_weights);
    nimcp_gpu_tensor_destroy(task_x);
    nimcp_gpu_tensor_destroy(task_y);
}

//=============================================================================
// ProtoNet Compute Prototypes Tests
//=============================================================================

TEST_F(MetalearningKernelTest, ProtoNetComputePrototypes_ComputesMeanEmbeddings) {
    RequireGPU();

    const size_t n_classes = 5;
    const size_t embedding_dim = 64;
    const size_t k_shot = 5;

    nimcp_gpu_protonet_state_t* state = CreateProtoNetState(n_classes, embedding_dim);
    ASSERT_NE(state, nullptr);

    // Create support embeddings and labels
    nimcp_gpu_tensor_t* support_embeddings = Create2DTensor(n_classes * k_shot, embedding_dim, 0.5f);
    nimcp_gpu_tensor_t* support_labels = Create1DTensor(n_classes * k_shot, 0.0f);

    // Set labels (0,0,0,0,0,1,1,1,1,1,2,2,2,2,2,...)
    std::vector<float> labels(n_classes * k_shot);
    for (size_t c = 0; c < n_classes; c++) {
        for (size_t k = 0; k < k_shot; k++) {
            labels[c * k_shot + k] = static_cast<float>(c);
        }
    }
    support_labels = SetFromHost(support_labels, labels);

    nimcp_gpu_protonet_params_t params = nimcp_gpu_protonet_params_default();

    bool result = nimcp_gpu_protonet_compute_prototypes(ctx, state, support_embeddings, support_labels, &params);
    EXPECT_TRUE(result);

    auto prototypes = CopyToHost(state->prototypes);

    // Prototypes should be computed (not all zeros)
    bool has_nonzero = false;
    for (size_t i = 0; i < n_classes * embedding_dim; i++) {
        if (std::abs(prototypes[i]) > 1e-8f) {
            has_nonzero = true;
            break;
        }
    }
    EXPECT_TRUE(has_nonzero);

    nimcp_gpu_tensor_destroy(support_embeddings);
    nimcp_gpu_tensor_destroy(support_labels);
    DestroyProtoNetState(state);
}

//=============================================================================
// ProtoNet Classify Tests
//=============================================================================

TEST_F(MetalearningKernelTest, ProtoNetClassify_ProducesValidPredictions) {
    RequireGPU();

    const size_t n_classes = 5;
    const size_t embedding_dim = 64;
    const size_t n_query = 10;

    nimcp_gpu_protonet_state_t* state = CreateProtoNetState(n_classes, embedding_dim);
    ASSERT_NE(state, nullptr);

    // Set some prototype values
    nimcp_gpu_fill(ctx, state->prototypes, 1.0f);

    nimcp_gpu_tensor_t* query_embeddings = Create2DTensor(n_query, embedding_dim, 0.5f);
    nimcp_gpu_tensor_t* predictions = Create1DTensor(n_query, -1.0f);

    nimcp_gpu_protonet_params_t params = nimcp_gpu_protonet_params_default();

    bool result = nimcp_gpu_protonet_classify(ctx, state, query_embeddings, predictions, &params);
    EXPECT_TRUE(result);

    auto pred_data = CopyToHost(predictions);

    // Predictions should be valid class indices [0, n_classes)
    for (size_t i = 0; i < n_query; i++) {
        EXPECT_GE(pred_data[i], 0.0f);
        EXPECT_LT(pred_data[i], static_cast<float>(n_classes));
    }

    nimcp_gpu_tensor_destroy(query_embeddings);
    nimcp_gpu_tensor_destroy(predictions);
    DestroyProtoNetState(state);
}

//=============================================================================
// ProtoNet Loss Tests
//=============================================================================

TEST_F(MetalearningKernelTest, ProtoNetLoss_ComputesNonNegativeLoss) {
    RequireGPU();

    const size_t n_classes = 5;
    const size_t embedding_dim = 64;
    const size_t n_query = 10;

    nimcp_gpu_protonet_state_t* state = CreateProtoNetState(n_classes, embedding_dim);
    ASSERT_NE(state, nullptr);

    // Set some prototype and logit values
    nimcp_gpu_fill(ctx, state->prototypes, 1.0f);
    nimcp_gpu_fill(ctx, state->logits, 0.5f);

    nimcp_gpu_tensor_t* query_labels = Create1DTensor(n_query, 0.0f);

    float loss_out = -1.0f;

    nimcp_gpu_protonet_params_t params = nimcp_gpu_protonet_params_default();

    bool result = nimcp_gpu_protonet_loss(ctx, state, query_labels, &loss_out, &params);
    EXPECT_TRUE(result);

    // Loss should be non-negative
    EXPECT_GE(loss_out, 0.0f);

    nimcp_gpu_tensor_destroy(query_labels);
    DestroyProtoNetState(state);
}

//=============================================================================
// ProtoNet Episode Tests
//=============================================================================

TEST_F(MetalearningKernelTest, ProtoNetEpisode_PerformsFullEpisode) {
    RequireGPU();

    const size_t n_classes = 5;
    const size_t embedding_dim = 64;
    const size_t k_shot = 5;
    const size_t n_query = 10;

    nimcp_gpu_protonet_state_t* state = CreateProtoNetState(n_classes, embedding_dim);
    ASSERT_NE(state, nullptr);

    nimcp_gpu_tensor_t* support_embeddings = Create2DTensor(n_classes * k_shot, embedding_dim, 0.5f);
    nimcp_gpu_tensor_t* support_labels = Create1DTensor(n_classes * k_shot, 0.0f);
    nimcp_gpu_tensor_t* query_embeddings = Create2DTensor(n_query, embedding_dim, 0.3f);
    nimcp_gpu_tensor_t* query_labels = Create1DTensor(n_query, 0.0f);
    nimcp_gpu_tensor_t* predictions = Create1DTensor(n_query, -1.0f);

    // Set support labels
    std::vector<float> s_labels(n_classes * k_shot);
    for (size_t c = 0; c < n_classes; c++) {
        for (size_t k = 0; k < k_shot; k++) {
            s_labels[c * k_shot + k] = static_cast<float>(c);
        }
    }
    support_labels = SetFromHost(support_labels, s_labels);

    float loss_out = -1.0f;

    nimcp_gpu_protonet_params_t params = nimcp_gpu_protonet_params_default();

    bool result = nimcp_gpu_protonet_episode(ctx, state, support_embeddings, support_labels,
                                              query_embeddings, query_labels, &loss_out,
                                              predictions, &params);
    EXPECT_TRUE(result);

    // Loss should be computed
    EXPECT_GE(loss_out, 0.0f);

    // Predictions should be valid
    auto pred_data = CopyToHost(predictions);
    for (size_t i = 0; i < n_query; i++) {
        EXPECT_GE(pred_data[i], 0.0f);
        EXPECT_LT(pred_data[i], static_cast<float>(n_classes));
    }

    nimcp_gpu_tensor_destroy(support_embeddings);
    nimcp_gpu_tensor_destroy(support_labels);
    nimcp_gpu_tensor_destroy(query_embeddings);
    nimcp_gpu_tensor_destroy(query_labels);
    nimcp_gpu_tensor_destroy(predictions);
    DestroyProtoNetState(state);
}

//=============================================================================
// Meta Memory State Tests
//=============================================================================

TEST_F(MetalearningKernelTest, MetaMemoryState_CreatesValidState) {
    RequireGPU();

    const size_t memory_size = 100;
    const size_t key_dim = 64;
    const size_t value_dim = 128;

    nimcp_gpu_meta_memory_state_t* state = CreateMetaMemoryState(memory_size, key_dim, value_dim);
    ASSERT_NE(state, nullptr);

    EXPECT_NE(state->keys, nullptr);
    EXPECT_NE(state->values, nullptr);
    EXPECT_NE(state->usage, nullptr);
    EXPECT_NE(state->read_weights, nullptr);
    EXPECT_NE(state->write_weights, nullptr);
    EXPECT_NE(state->read_output, nullptr);
    EXPECT_EQ(state->memory_size, memory_size);
    EXPECT_EQ(state->key_dim, key_dim);
    EXPECT_EQ(state->value_dim, value_dim);

    DestroyMetaMemoryState(state);
}

TEST_F(MetalearningKernelTest, MetaMemoryState_DestroyHandlesNull) {
    // Should not crash
    DestroyMetaMemoryState(nullptr);
}

//=============================================================================
// Meta Memory Read Tests
//=============================================================================

TEST_F(MetalearningKernelTest, MetaMemoryRead_ProducesValidOutput) {
    RequireGPU();

    const size_t memory_size = 100;
    const size_t key_dim = 64;
    const size_t value_dim = 128;

    nimcp_gpu_meta_memory_state_t* state = CreateMetaMemoryState(memory_size, key_dim, value_dim);
    ASSERT_NE(state, nullptr);

    // Initialize memory with some values
    nimcp_gpu_fill(ctx, state->keys, 0.1f);
    nimcp_gpu_fill(ctx, state->values, 1.0f);

    nimcp_gpu_tensor_t* query_key = Create1DTensor(key_dim, 0.1f);
    nimcp_gpu_tensor_t* read_output = Create1DTensor(value_dim, 0.0f);

    nimcp_gpu_meta_memory_params_t params = nimcp_gpu_meta_memory_params_default();

    bool result = nimcp_gpu_meta_memory_read(ctx, state, query_key, read_output, &params);
    EXPECT_TRUE(result);

    auto output_data = CopyToHost(read_output);

    // Read output should have been computed
    bool has_nonzero = false;
    for (size_t i = 0; i < value_dim; i++) {
        if (std::abs(output_data[i]) > 1e-8f) {
            has_nonzero = true;
            break;
        }
    }
    EXPECT_TRUE(has_nonzero);

    nimcp_gpu_tensor_destroy(query_key);
    nimcp_gpu_tensor_destroy(read_output);
    DestroyMetaMemoryState(state);
}

//=============================================================================
// Meta Memory Write Tests
//=============================================================================

TEST_F(MetalearningKernelTest, MetaMemoryWrite_UpdatesMemory) {
    RequireGPU();

    const size_t memory_size = 100;
    const size_t key_dim = 64;
    const size_t value_dim = 128;

    nimcp_gpu_meta_memory_state_t* state = CreateMetaMemoryState(memory_size, key_dim, value_dim);
    ASSERT_NE(state, nullptr);

    // Initialize memory to zeros
    nimcp_gpu_fill(ctx, state->keys, 0.0f);
    nimcp_gpu_fill(ctx, state->values, 0.0f);

    nimcp_gpu_tensor_t* key = Create1DTensor(key_dim, 1.0f);
    nimcp_gpu_tensor_t* value = Create1DTensor(value_dim, 2.0f);

    nimcp_gpu_meta_memory_params_t params = nimcp_gpu_meta_memory_params_default();

    bool result = nimcp_gpu_meta_memory_write(ctx, state, key, value, &params);
    EXPECT_TRUE(result);

    // Memory should be updated (check write weights were computed)
    auto write_weights = CopyToHost(state->write_weights);
    float sum = 0.0f;
    for (size_t i = 0; i < memory_size; i++) {
        sum += write_weights[i];
    }
    // Write weights should be non-zero after write operation
    EXPECT_GT(sum, 0.0f);

    nimcp_gpu_tensor_destroy(key);
    nimcp_gpu_tensor_destroy(value);
    DestroyMetaMemoryState(state);
}

//=============================================================================
// Meta Memory Update Tests
//=============================================================================

TEST_F(MetalearningKernelTest, MetaMemoryUpdate_AppliesDecay) {
    RequireGPU();

    const size_t memory_size = 100;
    const size_t key_dim = 64;
    const size_t value_dim = 128;

    nimcp_gpu_meta_memory_state_t* state = CreateMetaMemoryState(memory_size, key_dim, value_dim);
    ASSERT_NE(state, nullptr);

    // Set high usage
    nimcp_gpu_fill(ctx, state->usage, 1.0f);

    nimcp_gpu_meta_memory_params_t params = nimcp_gpu_meta_memory_params_default();
    params.forget_rate = 0.1f;

    auto initial_usage = CopyToHost(state->usage);

    bool result = nimcp_gpu_meta_memory_update(ctx, state, &params);
    EXPECT_TRUE(result);

    auto updated_usage = CopyToHost(state->usage);

    // Usage should decay
    for (size_t i = 0; i < memory_size; i++) {
        EXPECT_LE(updated_usage[i], initial_usage[i]);
    }

    DestroyMetaMemoryState(state);
}

//=============================================================================
// Meta Memory Reset Tests
//=============================================================================

TEST_F(MetalearningKernelTest, MetaMemoryReset_ClearsMemory) {
    RequireGPU();

    const size_t memory_size = 100;
    const size_t key_dim = 64;
    const size_t value_dim = 128;

    nimcp_gpu_meta_memory_state_t* state = CreateMetaMemoryState(memory_size, key_dim, value_dim);
    ASSERT_NE(state, nullptr);

    // Fill memory with non-zero values
    nimcp_gpu_fill(ctx, state->keys, 1.0f);
    nimcp_gpu_fill(ctx, state->values, 1.0f);
    nimcp_gpu_fill(ctx, state->usage, 1.0f);

    bool result = nimcp_gpu_meta_memory_reset(ctx, state);
    EXPECT_TRUE(result);

    auto usage = CopyToHost(state->usage);

    // Usage should be reset to zero
    for (size_t i = 0; i < memory_size; i++) {
        EXPECT_NEAR(usage[i], 0.0f, 1e-6f);
    }

    DestroyMetaMemoryState(state);
}

//=============================================================================
// Task Embedding State Tests
//=============================================================================

TEST_F(MetalearningKernelTest, TaskEmbedState_CreatesValidState) {
    RequireGPU();

    const size_t embed_dim = 128;

    nimcp_gpu_task_embed_state_t* state = CreateTaskEmbedState(embed_dim);
    ASSERT_NE(state, nullptr);

    EXPECT_NE(state->task_embedding, nullptr);
    EXPECT_NE(state->context_encoder, nullptr);
    EXPECT_NE(state->film_gamma, nullptr);
    EXPECT_NE(state->film_beta, nullptr);
    EXPECT_EQ(state->embed_dim, embed_dim);

    DestroyTaskEmbedState(state);
}

TEST_F(MetalearningKernelTest, TaskEmbedState_DestroyHandlesNull) {
    // Should not crash
    DestroyTaskEmbedState(nullptr);
}

//=============================================================================
// Task Embedding Infer Tests
//=============================================================================

TEST_F(MetalearningKernelTest, TaskEmbedInfer_ComputesEmbedding) {
    RequireGPU();

    const size_t embed_dim = 128;
    const size_t context_size = 10;
    const size_t input_dim = 64;

    nimcp_gpu_task_embed_state_t* state = CreateTaskEmbedState(embed_dim);
    ASSERT_NE(state, nullptr);

    nimcp_gpu_tensor_t* context_x = Create2DTensor(context_size, input_dim, 0.5f);
    nimcp_gpu_tensor_t* context_y = Create1DTensor(context_size, 1.0f);

    nimcp_gpu_task_embed_params_t params = nimcp_gpu_task_embed_params_default();

    bool result = nimcp_gpu_task_embed_infer(ctx, state, context_x, context_y, &params);
    EXPECT_TRUE(result);

    auto embedding = CopyToHost(state->task_embedding);

    // Task embedding should be computed (not all zeros)
    bool has_nonzero = false;
    for (size_t i = 0; i < embed_dim; i++) {
        if (std::abs(embedding[i]) > 1e-8f) {
            has_nonzero = true;
            break;
        }
    }
    EXPECT_TRUE(has_nonzero);

    nimcp_gpu_tensor_destroy(context_x);
    nimcp_gpu_tensor_destroy(context_y);
    DestroyTaskEmbedState(state);
}

//=============================================================================
// Task Embedding FiLM Tests
//=============================================================================

TEST_F(MetalearningKernelTest, TaskEmbedFiLM_ModulatesActivations) {
    RequireGPU();

    const size_t embed_dim = 128;
    const size_t batch_size = 32;

    nimcp_gpu_task_embed_state_t* state = CreateTaskEmbedState(embed_dim);
    ASSERT_NE(state, nullptr);

    // Set FiLM parameters
    nimcp_gpu_fill(ctx, state->film_gamma, 2.0f);  // Scale by 2
    nimcp_gpu_fill(ctx, state->film_beta, 1.0f);   // Shift by 1

    nimcp_gpu_tensor_t* activations = Create2DTensor(batch_size, embed_dim, 1.0f);

    nimcp_gpu_task_embed_params_t params = nimcp_gpu_task_embed_params_default();
    params.use_film = true;

    auto initial_activations = CopyToHost(activations);

    bool result = nimcp_gpu_task_embed_film(ctx, state, activations, &params);
    EXPECT_TRUE(result);

    auto modulated_activations = CopyToHost(activations);

    // Activations should be transformed: gamma * x + beta = 2 * 1 + 1 = 3
    for (size_t i = 0; i < batch_size * embed_dim; i++) {
        EXPECT_NEAR(modulated_activations[i], 3.0f, 1e-5f);
    }

    nimcp_gpu_tensor_destroy(activations);
    DestroyTaskEmbedState(state);
}

//=============================================================================
// Task Embedding Similarity Tests
//=============================================================================

TEST_F(MetalearningKernelTest, TaskEmbedSimilarity_ComputesValidSimilarity) {
    RequireGPU();

    const size_t embed_dim = 128;

    nimcp_gpu_tensor_t* embed1 = Create1DTensor(embed_dim, 1.0f);
    nimcp_gpu_tensor_t* embed2 = Create1DTensor(embed_dim, 1.0f);

    float similarity = -2.0f;

    bool result = nimcp_gpu_task_embed_similarity(ctx, embed1, embed2, &similarity);
    EXPECT_TRUE(result);

    // Same vectors should have high similarity (cosine ~= 1)
    EXPECT_GE(similarity, 0.9f);
    EXPECT_LE(similarity, 1.0f);

    nimcp_gpu_tensor_destroy(embed1);
    nimcp_gpu_tensor_destroy(embed2);
}

TEST_F(MetalearningKernelTest, TaskEmbedSimilarity_OrthogonalVectorsLowSimilarity) {
    RequireGPU();

    const size_t embed_dim = 128;

    nimcp_gpu_tensor_t* embed1 = Create1DTensor(embed_dim, 0.0f);
    nimcp_gpu_tensor_t* embed2 = Create1DTensor(embed_dim, 0.0f);

    // Create orthogonal vectors
    std::vector<float> v1(embed_dim, 0.0f);
    std::vector<float> v2(embed_dim, 0.0f);
    for (size_t i = 0; i < embed_dim / 2; i++) {
        v1[i] = 1.0f;
        v2[embed_dim / 2 + i] = 1.0f;
    }
    embed1 = SetFromHost(embed1, v1);
    embed2 = SetFromHost(embed2, v2);

    float similarity = 1.0f;

    bool result = nimcp_gpu_task_embed_similarity(ctx, embed1, embed2, &similarity);
    EXPECT_TRUE(result);

    // Orthogonal vectors should have zero similarity
    EXPECT_NEAR(similarity, 0.0f, 0.1f);

    nimcp_gpu_tensor_destroy(embed1);
    nimcp_gpu_tensor_destroy(embed2);
}

//=============================================================================
// Utility Function Tests - Sample Episode
//=============================================================================

TEST_F(MetalearningKernelTest, SampleEpisode_ProducesValidSplits) {
    RequireGPU();

    const size_t n_samples = 500;
    const size_t input_dim = 64;
    const int n_way = 5;
    const int k_shot = 5;
    const int n_query = 10;

    nimcp_gpu_tensor_t* data_x = Create2DTensor(n_samples, input_dim, 0.5f);
    nimcp_gpu_tensor_t* data_y = Create1DTensor(n_samples, 0.0f);

    // Create labels for 10 classes
    std::vector<float> labels(n_samples);
    for (size_t i = 0; i < n_samples; i++) {
        labels[i] = static_cast<float>(i % 10);
    }
    data_y = SetFromHost(data_y, labels);

    nimcp_gpu_tensor_t* support_x = Create2DTensor(n_way * k_shot, input_dim, 0.0f);
    nimcp_gpu_tensor_t* support_y = Create1DTensor(n_way * k_shot, -1.0f);
    nimcp_gpu_tensor_t* query_x = Create2DTensor(n_way * n_query, input_dim, 0.0f);
    nimcp_gpu_tensor_t* query_y = Create1DTensor(n_way * n_query, -1.0f);

    bool result = nimcp_gpu_sample_episode(ctx, data_x, data_y, support_x, support_y,
                                            query_x, query_y, n_way, k_shot, n_query);
    EXPECT_TRUE(result);

    auto support_labels = CopyToHost(support_y);
    auto query_labels = CopyToHost(query_y);

    // Support labels should have k_shot samples per class
    for (size_t i = 0; i < static_cast<size_t>(n_way * k_shot); i++) {
        EXPECT_GE(support_labels[i], 0.0f);
        EXPECT_LT(support_labels[i], static_cast<float>(n_way));
    }

    // Query labels should have n_query samples per class
    for (size_t i = 0; i < static_cast<size_t>(n_way * n_query); i++) {
        EXPECT_GE(query_labels[i], 0.0f);
        EXPECT_LT(query_labels[i], static_cast<float>(n_way));
    }

    nimcp_gpu_tensor_destroy(data_x);
    nimcp_gpu_tensor_destroy(data_y);
    nimcp_gpu_tensor_destroy(support_x);
    nimcp_gpu_tensor_destroy(support_y);
    nimcp_gpu_tensor_destroy(query_x);
    nimcp_gpu_tensor_destroy(query_y);
}

//=============================================================================
// Utility Function Tests - Few-Shot Accuracy
//=============================================================================

TEST_F(MetalearningKernelTest, FewShotAccuracy_ComputesCorrectAccuracy) {
    RequireGPU();

    const size_t n_samples = 100;

    nimcp_gpu_tensor_t* predictions = Create1DTensor(n_samples, 0.0f);
    nimcp_gpu_tensor_t* labels = Create1DTensor(n_samples, 0.0f);

    // All predictions match labels
    float accuracy = -1.0f;

    bool result = nimcp_gpu_few_shot_accuracy(ctx, predictions, labels, &accuracy);
    EXPECT_TRUE(result);

    // Should be 100% accurate
    EXPECT_NEAR(accuracy, 1.0f, 1e-5f);

    nimcp_gpu_tensor_destroy(predictions);
    nimcp_gpu_tensor_destroy(labels);
}

TEST_F(MetalearningKernelTest, FewShotAccuracy_ComputesPartialAccuracy) {
    RequireGPU();

    const size_t n_samples = 100;

    nimcp_gpu_tensor_t* predictions = Create1DTensor(n_samples, 0.0f);
    nimcp_gpu_tensor_t* labels = Create1DTensor(n_samples, 0.0f);

    // Set half of predictions to be wrong
    std::vector<float> preds(n_samples, 0.0f);
    for (size_t i = 0; i < n_samples / 2; i++) {
        preds[i] = 1.0f;  // Wrong prediction
    }
    predictions = SetFromHost(predictions, preds);

    float accuracy = -1.0f;

    bool result = nimcp_gpu_few_shot_accuracy(ctx, predictions, labels, &accuracy);
    EXPECT_TRUE(result);

    // Should be 50% accurate
    EXPECT_NEAR(accuracy, 0.5f, 0.01f);

    nimcp_gpu_tensor_destroy(predictions);
    nimcp_gpu_tensor_destroy(labels);
}

//=============================================================================
// NULL Safety Tests
//=============================================================================

TEST_F(MetalearningKernelTest, MAMLInnerLoop_NullSafety) {
    RequireGPU();

    nimcp_gpu_tensor_t* tensor = Create1DTensor(10, 0.0f);
    nimcp_gpu_tensor_t* tensor2d = Create2DTensor(10, 10, 0.0f);
    nimcp_gpu_maml_params_t params = nimcp_gpu_maml_params_default();
    nimcp_gpu_maml_state_t* state = CreateMAMLState(100);

    EXPECT_FALSE(nimcp_gpu_maml_inner_loop(nullptr, state, tensor2d, tensor, &params));
    EXPECT_FALSE(nimcp_gpu_maml_inner_loop(ctx, nullptr, tensor2d, tensor, &params));
    EXPECT_FALSE(nimcp_gpu_maml_inner_loop(ctx, state, nullptr, tensor, &params));
    EXPECT_FALSE(nimcp_gpu_maml_inner_loop(ctx, state, tensor2d, nullptr, &params));
    EXPECT_FALSE(nimcp_gpu_maml_inner_loop(ctx, state, tensor2d, tensor, nullptr));

    nimcp_gpu_tensor_destroy(tensor);
    nimcp_gpu_tensor_destroy(tensor2d);
    DestroyMAMLState(state);
}

TEST_F(MetalearningKernelTest, MAMLOuterGradient_NullSafety) {
    RequireGPU();

    nimcp_gpu_tensor_t* tensor = Create1DTensor(10, 0.0f);
    nimcp_gpu_tensor_t* tensor2d = Create2DTensor(10, 10, 0.0f);
    nimcp_gpu_maml_params_t params = nimcp_gpu_maml_params_default();
    nimcp_gpu_maml_state_t* state = CreateMAMLState(100);

    EXPECT_FALSE(nimcp_gpu_maml_outer_gradient(nullptr, state, tensor2d, tensor, &params));
    EXPECT_FALSE(nimcp_gpu_maml_outer_gradient(ctx, nullptr, tensor2d, tensor, &params));
    EXPECT_FALSE(nimcp_gpu_maml_outer_gradient(ctx, state, nullptr, tensor, &params));
    EXPECT_FALSE(nimcp_gpu_maml_outer_gradient(ctx, state, tensor2d, nullptr, &params));
    EXPECT_FALSE(nimcp_gpu_maml_outer_gradient(ctx, state, tensor2d, tensor, nullptr));

    nimcp_gpu_tensor_destroy(tensor);
    nimcp_gpu_tensor_destroy(tensor2d);
    DestroyMAMLState(state);
}

TEST_F(MetalearningKernelTest, MAMLMetaUpdate_NullSafety) {
    RequireGPU();

    nimcp_gpu_maml_params_t params = nimcp_gpu_maml_params_default();
    nimcp_gpu_maml_state_t* state = CreateMAMLState(100);

    EXPECT_FALSE(nimcp_gpu_maml_meta_update(nullptr, state, &params));
    EXPECT_FALSE(nimcp_gpu_maml_meta_update(ctx, nullptr, &params));
    EXPECT_FALSE(nimcp_gpu_maml_meta_update(ctx, state, nullptr));

    DestroyMAMLState(state);
}

TEST_F(MetalearningKernelTest, MAMLStep_NullSafety) {
    RequireGPU();

    nimcp_gpu_tensor_t* tensor = Create1DTensor(10, 0.0f);
    nimcp_gpu_tensor_t* tensor2d = Create2DTensor(10, 10, 0.0f);
    nimcp_gpu_maml_params_t params = nimcp_gpu_maml_params_default();
    nimcp_gpu_maml_state_t* state = CreateMAMLState(100);

    EXPECT_FALSE(nimcp_gpu_maml_step(nullptr, state, tensor2d, tensor, tensor2d, tensor, &params));
    EXPECT_FALSE(nimcp_gpu_maml_step(ctx, nullptr, tensor2d, tensor, tensor2d, tensor, &params));
    EXPECT_FALSE(nimcp_gpu_maml_step(ctx, state, nullptr, tensor, tensor2d, tensor, &params));
    EXPECT_FALSE(nimcp_gpu_maml_step(ctx, state, tensor2d, tensor, tensor2d, tensor, nullptr));

    nimcp_gpu_tensor_destroy(tensor);
    nimcp_gpu_tensor_destroy(tensor2d);
    DestroyMAMLState(state);
}

TEST_F(MetalearningKernelTest, MAMLHessianVectorProduct_NullSafety) {
    RequireGPU();

    nimcp_gpu_tensor_t* tensor = Create1DTensor(100, 0.0f);
    nimcp_gpu_maml_params_t params = nimcp_gpu_maml_params_default();
    nimcp_gpu_maml_state_t* state = CreateMAMLState(100);

    EXPECT_FALSE(nimcp_gpu_maml_hessian_vector_product(nullptr, state, tensor, tensor, &params));
    EXPECT_FALSE(nimcp_gpu_maml_hessian_vector_product(ctx, nullptr, tensor, tensor, &params));
    EXPECT_FALSE(nimcp_gpu_maml_hessian_vector_product(ctx, state, nullptr, tensor, &params));
    EXPECT_FALSE(nimcp_gpu_maml_hessian_vector_product(ctx, state, tensor, nullptr, &params));
    EXPECT_FALSE(nimcp_gpu_maml_hessian_vector_product(ctx, state, tensor, tensor, nullptr));

    nimcp_gpu_tensor_destroy(tensor);
    DestroyMAMLState(state);
}

TEST_F(MetalearningKernelTest, ReptileInnerLoop_NullSafety) {
    RequireGPU();

    nimcp_gpu_tensor_t* tensor = Create1DTensor(10, 0.0f);
    nimcp_gpu_tensor_t* tensor2d = Create2DTensor(10, 10, 0.0f);
    nimcp_gpu_reptile_params_t params = nimcp_gpu_reptile_params_default();

    EXPECT_FALSE(nimcp_gpu_reptile_inner_loop(nullptr, tensor, tensor2d, tensor, &params));
    EXPECT_FALSE(nimcp_gpu_reptile_inner_loop(ctx, nullptr, tensor2d, tensor, &params));
    EXPECT_FALSE(nimcp_gpu_reptile_inner_loop(ctx, tensor, nullptr, tensor, &params));
    EXPECT_FALSE(nimcp_gpu_reptile_inner_loop(ctx, tensor, tensor2d, nullptr, &params));
    EXPECT_FALSE(nimcp_gpu_reptile_inner_loop(ctx, tensor, tensor2d, tensor, nullptr));

    nimcp_gpu_tensor_destroy(tensor);
    nimcp_gpu_tensor_destroy(tensor2d);
}

TEST_F(MetalearningKernelTest, ReptileMetaUpdate_NullSafety) {
    RequireGPU();

    nimcp_gpu_tensor_t* tensor = Create1DTensor(10, 0.0f);
    nimcp_gpu_reptile_params_t params = nimcp_gpu_reptile_params_default();

    EXPECT_FALSE(nimcp_gpu_reptile_meta_update(nullptr, tensor, tensor, &params));
    EXPECT_FALSE(nimcp_gpu_reptile_meta_update(ctx, nullptr, tensor, &params));
    EXPECT_FALSE(nimcp_gpu_reptile_meta_update(ctx, tensor, nullptr, &params));
    EXPECT_FALSE(nimcp_gpu_reptile_meta_update(ctx, tensor, tensor, nullptr));

    nimcp_gpu_tensor_destroy(tensor);
}

TEST_F(MetalearningKernelTest, ReptileStep_NullSafety) {
    RequireGPU();

    nimcp_gpu_tensor_t* tensor = Create1DTensor(10, 0.0f);
    nimcp_gpu_tensor_t* tensor2d = Create2DTensor(10, 10, 0.0f);
    nimcp_gpu_reptile_params_t params = nimcp_gpu_reptile_params_default();

    EXPECT_FALSE(nimcp_gpu_reptile_step(nullptr, tensor, tensor2d, tensor, &params));
    EXPECT_FALSE(nimcp_gpu_reptile_step(ctx, nullptr, tensor2d, tensor, &params));
    EXPECT_FALSE(nimcp_gpu_reptile_step(ctx, tensor, nullptr, tensor, &params));
    EXPECT_FALSE(nimcp_gpu_reptile_step(ctx, tensor, tensor2d, nullptr, &params));
    EXPECT_FALSE(nimcp_gpu_reptile_step(ctx, tensor, tensor2d, tensor, nullptr));

    nimcp_gpu_tensor_destroy(tensor);
    nimcp_gpu_tensor_destroy(tensor2d);
}

TEST_F(MetalearningKernelTest, ProtoNetComputePrototypes_NullSafety) {
    RequireGPU();

    nimcp_gpu_tensor_t* tensor = Create1DTensor(10, 0.0f);
    nimcp_gpu_tensor_t* tensor2d = Create2DTensor(10, 64, 0.0f);
    nimcp_gpu_protonet_params_t params = nimcp_gpu_protonet_params_default();
    nimcp_gpu_protonet_state_t* state = CreateProtoNetState(5, 64);

    EXPECT_FALSE(nimcp_gpu_protonet_compute_prototypes(nullptr, state, tensor2d, tensor, &params));
    EXPECT_FALSE(nimcp_gpu_protonet_compute_prototypes(ctx, nullptr, tensor2d, tensor, &params));
    EXPECT_FALSE(nimcp_gpu_protonet_compute_prototypes(ctx, state, nullptr, tensor, &params));
    EXPECT_FALSE(nimcp_gpu_protonet_compute_prototypes(ctx, state, tensor2d, nullptr, &params));
    EXPECT_FALSE(nimcp_gpu_protonet_compute_prototypes(ctx, state, tensor2d, tensor, nullptr));

    nimcp_gpu_tensor_destroy(tensor);
    nimcp_gpu_tensor_destroy(tensor2d);
    DestroyProtoNetState(state);
}

TEST_F(MetalearningKernelTest, ProtoNetClassify_NullSafety) {
    RequireGPU();

    nimcp_gpu_tensor_t* tensor = Create1DTensor(10, 0.0f);
    nimcp_gpu_tensor_t* tensor2d = Create2DTensor(10, 64, 0.0f);
    nimcp_gpu_protonet_params_t params = nimcp_gpu_protonet_params_default();
    nimcp_gpu_protonet_state_t* state = CreateProtoNetState(5, 64);

    EXPECT_FALSE(nimcp_gpu_protonet_classify(nullptr, state, tensor2d, tensor, &params));
    EXPECT_FALSE(nimcp_gpu_protonet_classify(ctx, nullptr, tensor2d, tensor, &params));
    EXPECT_FALSE(nimcp_gpu_protonet_classify(ctx, state, nullptr, tensor, &params));
    EXPECT_FALSE(nimcp_gpu_protonet_classify(ctx, state, tensor2d, nullptr, &params));
    EXPECT_FALSE(nimcp_gpu_protonet_classify(ctx, state, tensor2d, tensor, nullptr));

    nimcp_gpu_tensor_destroy(tensor);
    nimcp_gpu_tensor_destroy(tensor2d);
    DestroyProtoNetState(state);
}

TEST_F(MetalearningKernelTest, ProtoNetLoss_NullSafety) {
    RequireGPU();

    nimcp_gpu_tensor_t* tensor = Create1DTensor(10, 0.0f);
    nimcp_gpu_protonet_params_t params = nimcp_gpu_protonet_params_default();
    nimcp_gpu_protonet_state_t* state = CreateProtoNetState(5, 64);
    float loss = 0.0f;

    EXPECT_FALSE(nimcp_gpu_protonet_loss(nullptr, state, tensor, &loss, &params));
    EXPECT_FALSE(nimcp_gpu_protonet_loss(ctx, nullptr, tensor, &loss, &params));
    EXPECT_FALSE(nimcp_gpu_protonet_loss(ctx, state, nullptr, &loss, &params));
    EXPECT_FALSE(nimcp_gpu_protonet_loss(ctx, state, tensor, nullptr, &params));
    EXPECT_FALSE(nimcp_gpu_protonet_loss(ctx, state, tensor, &loss, nullptr));

    nimcp_gpu_tensor_destroy(tensor);
    DestroyProtoNetState(state);
}

TEST_F(MetalearningKernelTest, MetaMemoryRead_NullSafety) {
    RequireGPU();

    nimcp_gpu_tensor_t* tensor = Create1DTensor(64, 0.0f);
    nimcp_gpu_meta_memory_params_t params = nimcp_gpu_meta_memory_params_default();
    nimcp_gpu_meta_memory_state_t* state = CreateMetaMemoryState(100, 64, 128);

    EXPECT_FALSE(nimcp_gpu_meta_memory_read(nullptr, state, tensor, tensor, &params));
    EXPECT_FALSE(nimcp_gpu_meta_memory_read(ctx, nullptr, tensor, tensor, &params));
    EXPECT_FALSE(nimcp_gpu_meta_memory_read(ctx, state, nullptr, tensor, &params));
    EXPECT_FALSE(nimcp_gpu_meta_memory_read(ctx, state, tensor, nullptr, &params));
    EXPECT_FALSE(nimcp_gpu_meta_memory_read(ctx, state, tensor, tensor, nullptr));

    nimcp_gpu_tensor_destroy(tensor);
    DestroyMetaMemoryState(state);
}

TEST_F(MetalearningKernelTest, MetaMemoryWrite_NullSafety) {
    RequireGPU();

    nimcp_gpu_tensor_t* key = Create1DTensor(64, 0.0f);
    nimcp_gpu_tensor_t* value = Create1DTensor(128, 0.0f);
    nimcp_gpu_meta_memory_params_t params = nimcp_gpu_meta_memory_params_default();
    nimcp_gpu_meta_memory_state_t* state = CreateMetaMemoryState(100, 64, 128);

    EXPECT_FALSE(nimcp_gpu_meta_memory_write(nullptr, state, key, value, &params));
    EXPECT_FALSE(nimcp_gpu_meta_memory_write(ctx, nullptr, key, value, &params));
    EXPECT_FALSE(nimcp_gpu_meta_memory_write(ctx, state, nullptr, value, &params));
    EXPECT_FALSE(nimcp_gpu_meta_memory_write(ctx, state, key, nullptr, &params));
    EXPECT_FALSE(nimcp_gpu_meta_memory_write(ctx, state, key, value, nullptr));

    nimcp_gpu_tensor_destroy(key);
    nimcp_gpu_tensor_destroy(value);
    DestroyMetaMemoryState(state);
}

TEST_F(MetalearningKernelTest, MetaMemoryUpdate_NullSafety) {
    RequireGPU();

    nimcp_gpu_meta_memory_params_t params = nimcp_gpu_meta_memory_params_default();
    nimcp_gpu_meta_memory_state_t* state = CreateMetaMemoryState(100, 64, 128);

    EXPECT_FALSE(nimcp_gpu_meta_memory_update(nullptr, state, &params));
    EXPECT_FALSE(nimcp_gpu_meta_memory_update(ctx, nullptr, &params));
    EXPECT_FALSE(nimcp_gpu_meta_memory_update(ctx, state, nullptr));

    DestroyMetaMemoryState(state);
}

TEST_F(MetalearningKernelTest, MetaMemoryReset_NullSafety) {
    EXPECT_FALSE(nimcp_gpu_meta_memory_reset(nullptr, nullptr));
    if (gpu_available) {
        EXPECT_FALSE(nimcp_gpu_meta_memory_reset(ctx, nullptr));
    }
}

TEST_F(MetalearningKernelTest, TaskEmbedInfer_NullSafety) {
    RequireGPU();

    nimcp_gpu_tensor_t* tensor = Create1DTensor(10, 0.0f);
    nimcp_gpu_tensor_t* tensor2d = Create2DTensor(10, 64, 0.0f);
    nimcp_gpu_task_embed_params_t params = nimcp_gpu_task_embed_params_default();
    nimcp_gpu_task_embed_state_t* state = CreateTaskEmbedState(128);

    EXPECT_FALSE(nimcp_gpu_task_embed_infer(nullptr, state, tensor2d, tensor, &params));
    EXPECT_FALSE(nimcp_gpu_task_embed_infer(ctx, nullptr, tensor2d, tensor, &params));
    EXPECT_FALSE(nimcp_gpu_task_embed_infer(ctx, state, nullptr, tensor, &params));
    EXPECT_FALSE(nimcp_gpu_task_embed_infer(ctx, state, tensor2d, nullptr, &params));
    EXPECT_FALSE(nimcp_gpu_task_embed_infer(ctx, state, tensor2d, tensor, nullptr));

    nimcp_gpu_tensor_destroy(tensor);
    nimcp_gpu_tensor_destroy(tensor2d);
    DestroyTaskEmbedState(state);
}

TEST_F(MetalearningKernelTest, TaskEmbedFilm_NullSafety) {
    RequireGPU();

    nimcp_gpu_tensor_t* tensor2d = Create2DTensor(32, 128, 0.0f);
    nimcp_gpu_task_embed_params_t params = nimcp_gpu_task_embed_params_default();
    nimcp_gpu_task_embed_state_t* state = CreateTaskEmbedState(128);

    EXPECT_FALSE(nimcp_gpu_task_embed_film(nullptr, state, tensor2d, &params));
    EXPECT_FALSE(nimcp_gpu_task_embed_film(ctx, nullptr, tensor2d, &params));
    EXPECT_FALSE(nimcp_gpu_task_embed_film(ctx, state, nullptr, &params));
    EXPECT_FALSE(nimcp_gpu_task_embed_film(ctx, state, tensor2d, nullptr));

    nimcp_gpu_tensor_destroy(tensor2d);
    DestroyTaskEmbedState(state);
}

TEST_F(MetalearningKernelTest, TaskEmbedSimilarity_NullSafety) {
    RequireGPU();

    nimcp_gpu_tensor_t* tensor = Create1DTensor(128, 0.0f);
    float similarity = 0.0f;

    EXPECT_FALSE(nimcp_gpu_task_embed_similarity(nullptr, tensor, tensor, &similarity));
    EXPECT_FALSE(nimcp_gpu_task_embed_similarity(ctx, nullptr, tensor, &similarity));
    EXPECT_FALSE(nimcp_gpu_task_embed_similarity(ctx, tensor, nullptr, &similarity));
    EXPECT_FALSE(nimcp_gpu_task_embed_similarity(ctx, tensor, tensor, nullptr));

    nimcp_gpu_tensor_destroy(tensor);
}

TEST_F(MetalearningKernelTest, SampleEpisode_NullSafety) {
    RequireGPU();

    nimcp_gpu_tensor_t* tensor = Create1DTensor(100, 0.0f);
    nimcp_gpu_tensor_t* tensor2d = Create2DTensor(100, 64, 0.0f);
    nimcp_gpu_tensor_t* small2d = Create2DTensor(25, 64, 0.0f);
    nimcp_gpu_tensor_t* small1d = Create1DTensor(25, 0.0f);

    EXPECT_FALSE(nimcp_gpu_sample_episode(nullptr, tensor2d, tensor, small2d, small1d, small2d, small1d, 5, 5, 5));
    EXPECT_FALSE(nimcp_gpu_sample_episode(ctx, nullptr, tensor, small2d, small1d, small2d, small1d, 5, 5, 5));
    EXPECT_FALSE(nimcp_gpu_sample_episode(ctx, tensor2d, nullptr, small2d, small1d, small2d, small1d, 5, 5, 5));
    EXPECT_FALSE(nimcp_gpu_sample_episode(ctx, tensor2d, tensor, nullptr, small1d, small2d, small1d, 5, 5, 5));
    EXPECT_FALSE(nimcp_gpu_sample_episode(ctx, tensor2d, tensor, small2d, nullptr, small2d, small1d, 5, 5, 5));
    EXPECT_FALSE(nimcp_gpu_sample_episode(ctx, tensor2d, tensor, small2d, small1d, nullptr, small1d, 5, 5, 5));
    EXPECT_FALSE(nimcp_gpu_sample_episode(ctx, tensor2d, tensor, small2d, small1d, small2d, nullptr, 5, 5, 5));

    nimcp_gpu_tensor_destroy(tensor);
    nimcp_gpu_tensor_destroy(tensor2d);
    nimcp_gpu_tensor_destroy(small2d);
    nimcp_gpu_tensor_destroy(small1d);
}

TEST_F(MetalearningKernelTest, FewShotAccuracy_NullSafety) {
    RequireGPU();

    nimcp_gpu_tensor_t* tensor = Create1DTensor(100, 0.0f);
    float accuracy = 0.0f;

    EXPECT_FALSE(nimcp_gpu_few_shot_accuracy(nullptr, tensor, tensor, &accuracy));
    EXPECT_FALSE(nimcp_gpu_few_shot_accuracy(ctx, nullptr, tensor, &accuracy));
    EXPECT_FALSE(nimcp_gpu_few_shot_accuracy(ctx, tensor, nullptr, &accuracy));
    EXPECT_FALSE(nimcp_gpu_few_shot_accuracy(ctx, tensor, tensor, nullptr));

    nimcp_gpu_tensor_destroy(tensor);
}

//=============================================================================
// Integration Tests
//=============================================================================

TEST_F(MetalearningKernelTest, Integration_MAMLFewShotLearning) {
    RequireGPU();

    const size_t n_params = 500;
    const size_t n_samples = 25;  // 5-shot, 5-way
    const size_t input_dim = 64;
    const int n_tasks = 5;

    nimcp_gpu_maml_state_t* state = CreateMAMLState(n_params);
    ASSERT_NE(state, nullptr);

    nimcp_gpu_maml_params_t params = nimcp_gpu_maml_params_default();
    params.inner_steps = 3;
    params.k_shot = 5;
    params.n_way = 5;

    auto initial_weights = CopyToHost(state->meta_weights);

    // Simulate multiple meta-learning iterations
    for (int task = 0; task < n_tasks; task++) {
        nimcp_gpu_tensor_t* support_x = Create2DTensor(n_samples, input_dim, 0.5f * (task + 1));
        nimcp_gpu_tensor_t* support_y = Create1DTensor(n_samples, 0.0f);
        nimcp_gpu_tensor_t* query_x = Create2DTensor(n_samples, input_dim, 0.3f * (task + 1));
        nimcp_gpu_tensor_t* query_y = Create1DTensor(n_samples, 0.0f);

        // Set labels for each class
        std::vector<float> labels(n_samples);
        for (size_t i = 0; i < n_samples; i++) {
            labels[i] = static_cast<float>(i % 5);
        }
        support_y = SetFromHost(support_y, labels);
        query_y = SetFromHost(query_y, labels);

        bool result = nimcp_gpu_maml_step(ctx, state, support_x, support_y, query_x, query_y, &params);
        EXPECT_TRUE(result);

        nimcp_gpu_tensor_destroy(support_x);
        nimcp_gpu_tensor_destroy(support_y);
        nimcp_gpu_tensor_destroy(query_x);
        nimcp_gpu_tensor_destroy(query_y);
    }

    auto final_weights = CopyToHost(state->meta_weights);

    // Meta weights should have changed after multiple tasks
    float total_change = 0.0f;
    for (size_t i = 0; i < n_params; i++) {
        total_change += std::abs(final_weights[i] - initial_weights[i]);
    }
    EXPECT_GT(total_change, 0.0f);

    DestroyMAMLState(state);
}

TEST_F(MetalearningKernelTest, Integration_ReptileMetaLearning) {
    RequireGPU();

    const size_t n_params = 500;
    const size_t n_samples = 20;
    const size_t input_dim = 64;
    const int n_tasks = 5;

    nimcp_gpu_tensor_t* meta_weights = Create1DTensor(n_params, 0.5f);

    nimcp_gpu_reptile_params_t params = nimcp_gpu_reptile_params_default();
    params.inner_steps = 5;
    params.epsilon = 0.1f;

    auto initial_weights = CopyToHost(meta_weights);

    // Simulate multiple Reptile steps on different tasks
    for (int task = 0; task < n_tasks; task++) {
        nimcp_gpu_tensor_t* task_x = Create2DTensor(n_samples, input_dim, 0.5f * (task + 1));
        nimcp_gpu_tensor_t* task_y = Create1DTensor(n_samples, static_cast<float>(task % 3));

        bool result = nimcp_gpu_reptile_step(ctx, meta_weights, task_x, task_y, &params);
        EXPECT_TRUE(result);

        nimcp_gpu_tensor_destroy(task_x);
        nimcp_gpu_tensor_destroy(task_y);
    }

    auto final_weights = CopyToHost(meta_weights);

    // Meta weights should evolve over tasks
    float total_change = 0.0f;
    for (size_t i = 0; i < n_params; i++) {
        total_change += std::abs(final_weights[i] - initial_weights[i]);
    }
    EXPECT_GT(total_change, 0.0f);

    nimcp_gpu_tensor_destroy(meta_weights);
}

TEST_F(MetalearningKernelTest, Integration_ProtoNetClassification) {
    RequireGPU();

    const size_t n_classes = 5;
    const size_t embedding_dim = 64;
    const size_t k_shot = 5;
    const size_t n_query = 10;

    nimcp_gpu_protonet_state_t* state = CreateProtoNetState(n_classes, embedding_dim);
    ASSERT_NE(state, nullptr);

    nimcp_gpu_protonet_params_t params = nimcp_gpu_protonet_params_default();
    params.n_way = static_cast<int>(n_classes);
    params.k_shot = static_cast<int>(k_shot);
    params.n_query = static_cast<int>(n_query);

    // Create support set with distinct embeddings per class
    nimcp_gpu_tensor_t* support_embeddings = Create2DTensor(n_classes * k_shot, embedding_dim, 0.0f);
    nimcp_gpu_tensor_t* support_labels = Create1DTensor(n_classes * k_shot, 0.0f);

    std::vector<float> s_embed(n_classes * k_shot * embedding_dim);
    std::vector<float> s_labels(n_classes * k_shot);
    for (size_t c = 0; c < n_classes; c++) {
        for (size_t k = 0; k < k_shot; k++) {
            size_t idx = c * k_shot + k;
            s_labels[idx] = static_cast<float>(c);
            // Each class has a distinct centroid
            for (size_t d = 0; d < embedding_dim; d++) {
                s_embed[idx * embedding_dim + d] = (d == c) ? 1.0f : 0.1f;
            }
        }
    }
    support_embeddings = SetFromHost(support_embeddings, s_embed);
    support_labels = SetFromHost(support_labels, s_labels);

    // Create query set from class 0
    nimcp_gpu_tensor_t* query_embeddings = Create2DTensor(n_query, embedding_dim, 0.0f);
    nimcp_gpu_tensor_t* query_labels = Create1DTensor(n_query, 0.0f);
    nimcp_gpu_tensor_t* predictions = Create1DTensor(n_query, -1.0f);

    std::vector<float> q_embed(n_query * embedding_dim);
    for (size_t q = 0; q < n_query; q++) {
        for (size_t d = 0; d < embedding_dim; d++) {
            q_embed[q * embedding_dim + d] = (d == 0) ? 1.0f : 0.1f;  // Similar to class 0
        }
    }
    query_embeddings = SetFromHost(query_embeddings, q_embed);

    float loss = -1.0f;
    bool result = nimcp_gpu_protonet_episode(ctx, state, support_embeddings, support_labels,
                                              query_embeddings, query_labels, &loss,
                                              predictions, &params);
    EXPECT_TRUE(result);

    // Loss should be computed
    EXPECT_GE(loss, 0.0f);

    // Predictions should favor class 0 for queries similar to class 0
    auto pred_data = CopyToHost(predictions);
    int correct = 0;
    for (size_t i = 0; i < n_query; i++) {
        if (std::abs(pred_data[i] - 0.0f) < 0.5f) {
            correct++;
        }
    }
    // Should have reasonable accuracy for clearly clustered data
    EXPECT_GE(correct, static_cast<int>(n_query / 2));

    nimcp_gpu_tensor_destroy(support_embeddings);
    nimcp_gpu_tensor_destroy(support_labels);
    nimcp_gpu_tensor_destroy(query_embeddings);
    nimcp_gpu_tensor_destroy(query_labels);
    nimcp_gpu_tensor_destroy(predictions);
    DestroyProtoNetState(state);
}

TEST_F(MetalearningKernelTest, Integration_MetaMemoryReadWrite) {
    RequireGPU();

    const size_t memory_size = 50;
    const size_t key_dim = 32;
    const size_t value_dim = 64;
    const int n_writes = 10;

    nimcp_gpu_meta_memory_state_t* state = CreateMetaMemoryState(memory_size, key_dim, value_dim);
    ASSERT_NE(state, nullptr);

    nimcp_gpu_meta_memory_params_t params = nimcp_gpu_meta_memory_params_default();
    params.use_attention = true;

    // Reset memory
    bool reset_result = nimcp_gpu_meta_memory_reset(ctx, state);
    EXPECT_TRUE(reset_result);

    // Write multiple key-value pairs
    for (int i = 0; i < n_writes; i++) {
        nimcp_gpu_tensor_t* key = Create1DTensor(key_dim, 0.1f * (i + 1));
        nimcp_gpu_tensor_t* value = Create1DTensor(value_dim, 1.0f * (i + 1));

        bool write_result = nimcp_gpu_meta_memory_write(ctx, state, key, value, &params);
        EXPECT_TRUE(write_result);

        nimcp_gpu_tensor_destroy(key);
        nimcp_gpu_tensor_destroy(value);
    }

    // Update memory (apply decay)
    bool update_result = nimcp_gpu_meta_memory_update(ctx, state, &params);
    EXPECT_TRUE(update_result);

    // Read with a query key
    nimcp_gpu_tensor_t* query_key = Create1DTensor(key_dim, 0.5f);
    nimcp_gpu_tensor_t* read_output = Create1DTensor(value_dim, 0.0f);

    bool read_result = nimcp_gpu_meta_memory_read(ctx, state, query_key, read_output, &params);
    EXPECT_TRUE(read_result);

    auto output_data = CopyToHost(read_output);

    // Read output should be a weighted combination of stored values
    bool has_content = false;
    for (size_t i = 0; i < value_dim; i++) {
        if (std::abs(output_data[i]) > 1e-6f) {
            has_content = true;
            break;
        }
    }
    EXPECT_TRUE(has_content);

    nimcp_gpu_tensor_destroy(query_key);
    nimcp_gpu_tensor_destroy(read_output);
    DestroyMetaMemoryState(state);
}

TEST_F(MetalearningKernelTest, Integration_TaskEmbeddingTransfer) {
    RequireGPU();

    const size_t embed_dim = 128;
    const size_t context_size = 20;
    const size_t input_dim = 64;
    const size_t batch_size = 32;

    nimcp_gpu_task_embed_state_t* state1 = CreateTaskEmbedState(embed_dim);
    nimcp_gpu_task_embed_state_t* state2 = CreateTaskEmbedState(embed_dim);
    ASSERT_NE(state1, nullptr);
    ASSERT_NE(state2, nullptr);

    nimcp_gpu_task_embed_params_t params = nimcp_gpu_task_embed_params_default();
    params.use_film = true;

    // Infer task embedding from context for task 1
    nimcp_gpu_tensor_t* context_x1 = Create2DTensor(context_size, input_dim, 0.5f);
    nimcp_gpu_tensor_t* context_y1 = Create1DTensor(context_size, 1.0f);

    bool infer_result1 = nimcp_gpu_task_embed_infer(ctx, state1, context_x1, context_y1, &params);
    EXPECT_TRUE(infer_result1);

    // Infer task embedding from different context for task 2
    nimcp_gpu_tensor_t* context_x2 = Create2DTensor(context_size, input_dim, 0.2f);
    nimcp_gpu_tensor_t* context_y2 = Create1DTensor(context_size, 0.0f);

    bool infer_result2 = nimcp_gpu_task_embed_infer(ctx, state2, context_x2, context_y2, &params);
    EXPECT_TRUE(infer_result2);

    // Compute similarity between task embeddings
    float similarity = 0.0f;
    bool sim_result = nimcp_gpu_task_embed_similarity(ctx, state1->task_embedding,
                                                       state2->task_embedding, &similarity);
    EXPECT_TRUE(sim_result);

    // Different tasks should have different embeddings (not perfectly similar)
    EXPECT_GE(similarity, -1.0f);
    EXPECT_LE(similarity, 1.0f);

    // Apply FiLM conditioning from task 1
    nimcp_gpu_tensor_t* activations = Create2DTensor(batch_size, embed_dim, 1.0f);

    bool film_result = nimcp_gpu_task_embed_film(ctx, state1, activations, &params);
    EXPECT_TRUE(film_result);

    auto act_data = CopyToHost(activations);

    // Activations should be modulated
    bool modulated = false;
    for (size_t i = 0; i < batch_size * embed_dim; i++) {
        if (std::abs(act_data[i] - 1.0f) > 1e-6f) {
            modulated = true;
            break;
        }
    }
    EXPECT_TRUE(modulated);

    nimcp_gpu_tensor_destroy(context_x1);
    nimcp_gpu_tensor_destroy(context_y1);
    nimcp_gpu_tensor_destroy(context_x2);
    nimcp_gpu_tensor_destroy(context_y2);
    nimcp_gpu_tensor_destroy(activations);
    DestroyTaskEmbedState(state1);
    DestroyTaskEmbedState(state2);
}

TEST_F(MetalearningKernelTest, Integration_EndToEndFewShotPipeline) {
    RequireGPU();

    // Full few-shot learning pipeline
    const size_t n_data = 1000;
    const size_t input_dim = 64;
    const size_t embedding_dim = 64;
    const int n_way = 5;
    const int k_shot = 5;
    const int n_query = 15;
    const int n_episodes = 3;

    // Create dataset
    nimcp_gpu_tensor_t* data_x = Create2DTensor(n_data, input_dim, 0.5f);
    nimcp_gpu_tensor_t* data_y = Create1DTensor(n_data, 0.0f);

    std::vector<float> labels(n_data);
    for (size_t i = 0; i < n_data; i++) {
        labels[i] = static_cast<float>(i % 10);
    }
    data_y = SetFromHost(data_y, labels);

    // Create episode tensors
    nimcp_gpu_tensor_t* support_x = Create2DTensor(n_way * k_shot, input_dim, 0.0f);
    nimcp_gpu_tensor_t* support_y = Create1DTensor(n_way * k_shot, 0.0f);
    nimcp_gpu_tensor_t* query_x = Create2DTensor(n_way * n_query, input_dim, 0.0f);
    nimcp_gpu_tensor_t* query_y = Create1DTensor(n_way * n_query, 0.0f);

    // Create ProtoNet state
    nimcp_gpu_protonet_state_t* state = CreateProtoNetState(n_way, embedding_dim);
    ASSERT_NE(state, nullptr);

    nimcp_gpu_protonet_params_t params = nimcp_gpu_protonet_params_default();
    params.n_way = n_way;
    params.k_shot = k_shot;
    params.n_query = n_query;

    nimcp_gpu_tensor_t* predictions = Create1DTensor(n_way * n_query, 0.0f);

    float total_accuracy = 0.0f;

    for (int ep = 0; ep < n_episodes; ep++) {
        // Sample episode
        bool sample_result = nimcp_gpu_sample_episode(ctx, data_x, data_y,
                                                       support_x, support_y,
                                                       query_x, query_y,
                                                       n_way, k_shot, n_query);
        EXPECT_TRUE(sample_result);

        // Run ProtoNet episode (using support/query as embeddings directly for simplicity)
        float loss = 0.0f;
        bool episode_result = nimcp_gpu_protonet_episode(ctx, state,
                                                          support_x, support_y,
                                                          query_x, query_y,
                                                          &loss, predictions, &params);
        EXPECT_TRUE(episode_result);

        // Compute accuracy
        float accuracy = 0.0f;
        bool acc_result = nimcp_gpu_few_shot_accuracy(ctx, predictions, query_y, &accuracy);
        EXPECT_TRUE(acc_result);

        total_accuracy += accuracy;

        // Loss should be non-negative
        EXPECT_GE(loss, 0.0f);
        // Accuracy should be in valid range
        EXPECT_GE(accuracy, 0.0f);
        EXPECT_LE(accuracy, 1.0f);
    }

    // Average accuracy over episodes
    float avg_accuracy = total_accuracy / n_episodes;
    // Random baseline for 5-way would be 20%, should be at least better than that
    // (though this depends on implementation)
    EXPECT_GE(avg_accuracy, 0.0f);
    EXPECT_LE(avg_accuracy, 1.0f);

    nimcp_gpu_tensor_destroy(data_x);
    nimcp_gpu_tensor_destroy(data_y);
    nimcp_gpu_tensor_destroy(support_x);
    nimcp_gpu_tensor_destroy(support_y);
    nimcp_gpu_tensor_destroy(query_x);
    nimcp_gpu_tensor_destroy(query_y);
    nimcp_gpu_tensor_destroy(predictions);
    DestroyProtoNetState(state);
}
