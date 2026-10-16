/**
 * @file nimcp_swarm_kernels.cu
 * @brief GPU Swarm Intelligence CUDA Kernels Implementation
 *
 * WHAT: CUDA kernels for parallel swarm algorithm computation
 * WHY:  Swarm algorithms are embarrassingly parallel - each agent processes
 *       independently, enabling massive GPU acceleration
 * HOW:  Custom kernels for flocking, consensus, pheromone, quorum, task allocation
 *
 * @version 1.0
 * @author NIMCP Development Team
 * @date 2025
 */

#ifdef NIMCP_ENABLE_CUDA

// Include CUDA headers FIRST (before any extern "C" blocks from our headers)
#include <cuda_runtime.h>
#include <math.h>
#include <float.h>
#include <stdlib.h>
#include <string.h>

// Now include our headers (which have extern "C" blocks)
#include "gpu/swarm/nimcp_swarm_gpu.h"
#include "utils/logging/nimcp_logging.h"
#include "utils/exception/nimcp_exception_macros.h"
#include "gpu/common/nimcp_cuda_utils.h"
#include "gpu/recovery/nimcp_gpu_recovery.h"
#include "utils/memory/nimcp_memory.h"

#define LOG_MODULE "SWARM_GPU"

#define BLOCK_SIZE 256
#define GRID_SIZE(n) (((n) + BLOCK_SIZE - 1) / BLOCK_SIZE)

//=============================================================================
// Device Helper Functions
//=============================================================================

__device__ inline float device_length3(float x, float y, float z)
{
    return sqrtf(x * x + y * y + z * z);
}

__device__ inline float device_clamp(float val, float min_val, float max_val)
{
    return fmaxf(min_val, fminf(max_val, val));
}

__device__ inline void device_normalize3(float* x, float* y, float* z)
{
    float len = device_length3(*x, *y, *z);
    if (len > 1e-6f) {
        float inv_len = 1.0f / len;
        *x *= inv_len;
        *y *= inv_len;
        *z *= inv_len;
    }
}

//=============================================================================
// Default Parameter Functions
//=============================================================================

extern "C" void nimcp_flocking_gpu_default_params(nimcp_flocking_gpu_params_t* params)
{
    if (!params) return;

    params->separation_weight = 1.5f;
    params->alignment_weight = 1.0f;
    params->cohesion_weight = 1.0f;

    params->separation_radius = 2.0f;
    params->alignment_radius = 5.0f;
    params->cohesion_radius = 5.0f;

    params->max_speed = 5.0f;
    params->max_force = 0.5f;

    params->obstacle_weight = 2.0f;
    params->goal_weight = 0.5f;
    params->boundary_weight = 1.0f;

    params->dt = 0.016f;  // ~60 FPS
}

extern "C" void nimcp_consensus_gpu_default_params(nimcp_consensus_gpu_params_t* params)
{
    if (!params) return;

    params->learning_rate = 0.1f;
    params->min_confidence = 0.01f;
    params->byzantine_threshold = 0.333f;  // 1/3
    params->max_iterations = 100;
}

extern "C" void nimcp_pheromone_gpu_default_params(nimcp_pheromone_gpu_params_t* params)
{
    if (!params) return;

    for (int i = 0; i < SWARM_GPU_MAX_PHEROMONE_TYPES; i++) {
        params->decay_rates[i] = 0.01f;  // 1% decay per step
    }
    params->diffusion_rate = 0.1f;
    params->evaporation_rate = 0.005f;
    params->max_concentration = 100.0f;
    params->deposit_amount = 1.0f;
}

extern "C" void nimcp_quorum_gpu_default_params(nimcp_quorum_gpu_params_t* params)
{
    if (!params) return;

    params->base_threshold = 0.5f;
    params->decay_rate = 0.1f;
    params->amplification = 1.5f;
    params->inhibition = 0.5f;
    params->commitment_low = 0.2f;
    params->commitment_high = 0.8f;
}

extern "C" void nimcp_task_alloc_gpu_default_params(nimcp_task_alloc_gpu_params_t* params)
{
    if (!params) return;

    params->bid_increment = 0.1f;
    params->epsilon = 0.01f;
    params->max_rounds = 100;
}

extern "C" void nimcp_collision_gpu_default_params(nimcp_collision_gpu_params_t* params)
{
    if (!params) return;

    params->collision_radius = 1.0f;
    params->grid_cell_size = 2.0f;
    params->use_variable_radius = false;
}

//=============================================================================
// Flocking Kernels
//=============================================================================

/**
 * @brief Compute cell ID for spatial hashing
 */
__device__ inline uint32_t device_compute_cell_id(
    float x, float y, float z,
    float cell_size,
    uint32_t grid_x, uint32_t grid_y, uint32_t grid_z,
    float origin_x, float origin_y, float origin_z)
{
    int cx = (int)floorf((x - origin_x) / cell_size);
    int cy = (int)floorf((y - origin_y) / cell_size);
    int cz = (int)floorf((z - origin_z) / cell_size);

    cx = max(0, min((int)grid_x - 1, cx));
    cy = max(0, min((int)grid_y - 1, cy));
    cz = max(0, min((int)grid_z - 1, cz));

    return (uint32_t)(cz * grid_y * grid_x + cy * grid_x + cx);
}

/**
 * @brief Compute flocking forces (separation, alignment, cohesion) in single pass
 */
__global__ void kernel_flocking_forces(
    const float4* positions,      // [N]
    const float4* velocities,     // [N]
    float4* forces,               // [N] output
    const uint32_t* neighbor_counts,  // [N]
    const uint32_t* neighbor_indices, // [N x max_neighbors]
    size_t n_agents,
    size_t max_neighbors,
    float sep_weight, float sep_radius,
    float align_weight, float align_radius,
    float cohesion_weight, float cohesion_radius,
    float max_force)
{
    size_t idx = blockIdx.x * blockDim.x + threadIdx.x;
    if (idx >= n_agents) return;

    float4 pos_i = positions[idx];
    float4 vel_i = velocities[idx];

    // Accumulate forces
    float sep_x = 0.0f, sep_y = 0.0f, sep_z = 0.0f;
    float align_x = 0.0f, align_y = 0.0f, align_z = 0.0f;
    float coh_x = 0.0f, coh_y = 0.0f, coh_z = 0.0f;

    int sep_count = 0;
    int align_count = 0;
    int coh_count = 0;

    uint32_t n_neighbors = neighbor_counts[idx];
    const uint32_t* my_neighbors = neighbor_indices + idx * max_neighbors;

    for (uint32_t n = 0; n < n_neighbors; n++) {
        uint32_t j = my_neighbors[n];
        if (j == idx) continue;

        float4 pos_j = positions[j];
        float4 vel_j = velocities[j];

        float dx = pos_i.x - pos_j.x;
        float dy = pos_i.y - pos_j.y;
        float dz = pos_i.z - pos_j.z;
        float dist = device_length3(dx, dy, dz);

        if (dist < 1e-6f) continue;

        // Separation: steer away from nearby agents
        if (dist < sep_radius) {
            float inv_dist = 1.0f / dist;
            sep_x += dx * inv_dist / dist;  // Inverse square
            sep_y += dy * inv_dist / dist;
            sep_z += dz * inv_dist / dist;
            sep_count++;
        }

        // Alignment: match velocity of neighbors
        if (dist < align_radius) {
            align_x += vel_j.x;
            align_y += vel_j.y;
            align_z += vel_j.z;
            align_count++;
        }

        // Cohesion: move toward center of mass
        if (dist < cohesion_radius) {
            coh_x += pos_j.x;
            coh_y += pos_j.y;
            coh_z += pos_j.z;
            coh_count++;
        }
    }

    // Finalize forces
    float fx = 0.0f, fy = 0.0f, fz = 0.0f;

    // Separation force
    if (sep_count > 0) {
        sep_x /= sep_count;
        sep_y /= sep_count;
        sep_z /= sep_count;
        device_normalize3(&sep_x, &sep_y, &sep_z);
        fx += sep_x * sep_weight;
        fy += sep_y * sep_weight;
        fz += sep_z * sep_weight;
    }

    // Alignment force: steer toward average heading
    if (align_count > 0) {
        align_x /= align_count;
        align_y /= align_count;
        align_z /= align_count;
        // Steering = desired - current
        float steer_x = align_x - vel_i.x;
        float steer_y = align_y - vel_i.y;
        float steer_z = align_z - vel_i.z;
        device_normalize3(&steer_x, &steer_y, &steer_z);
        fx += steer_x * align_weight;
        fy += steer_y * align_weight;
        fz += steer_z * align_weight;
    }

    // Cohesion force: steer toward center of mass
    if (coh_count > 0) {
        coh_x = coh_x / coh_count - pos_i.x;
        coh_y = coh_y / coh_count - pos_i.y;
        coh_z = coh_z / coh_count - pos_i.z;
        device_normalize3(&coh_x, &coh_y, &coh_z);
        fx += coh_x * cohesion_weight;
        fy += coh_y * cohesion_weight;
        fz += coh_z * cohesion_weight;
    }

    // Limit force
    float force_mag = device_length3(fx, fy, fz);
    if (force_mag > max_force) {
        float scale = max_force / force_mag;
        fx *= scale;
        fy *= scale;
        fz *= scale;
    }

    forces[idx] = make_float4(fx, fy, fz, 0.0f);
}

/**
 * @brief Update positions and velocities based on forces
 */
__global__ void kernel_flocking_update(
    float4* positions,    // [N] in/out
    float4* velocities,   // [N] in/out
    const float4* forces, // [N]
    size_t n_agents,
    float dt,
    float max_speed)
{
    size_t idx = blockIdx.x * blockDim.x + threadIdx.x;
    if (idx >= n_agents) return;

    float4 pos = positions[idx];
    float4 vel = velocities[idx];
    float4 force = forces[idx];

    // Update velocity: v = v + a * dt
    vel.x += force.x * dt;
    vel.y += force.y * dt;
    vel.z += force.z * dt;

    // Limit speed
    float speed = device_length3(vel.x, vel.y, vel.z);
    if (speed > max_speed) {
        float scale = max_speed / speed;
        vel.x *= scale;
        vel.y *= scale;
        vel.z *= scale;
    }

    // Update position: p = p + v * dt
    pos.x += vel.x * dt;
    pos.y += vel.y * dt;
    pos.z += vel.z * dt;

    positions[idx] = pos;
    velocities[idx] = vel;
}

/**
 * @brief Compute particle cell IDs for spatial hashing
 */
__global__ void kernel_compute_cell_ids(
    const float4* positions,
    uint32_t* particle_cells,
    size_t n_agents,
    float cell_size,
    uint32_t grid_x, uint32_t grid_y, uint32_t grid_z,
    float origin_x, float origin_y, float origin_z)
{
    size_t idx = blockIdx.x * blockDim.x + threadIdx.x;
    if (idx >= n_agents) return;

    float4 pos = positions[idx];
    particle_cells[idx] = device_compute_cell_id(
        pos.x, pos.y, pos.z,
        cell_size, grid_x, grid_y, grid_z,
        origin_x, origin_y, origin_z);
}

/**
 * @brief Find neighbors using brute force (for small N)
 */
__global__ void kernel_find_neighbors_brute(
    const float4* positions,
    uint32_t* neighbor_counts,
    uint32_t* neighbor_indices,
    size_t n_agents,
    size_t max_neighbors,
    float radius)
{
    size_t idx = blockIdx.x * blockDim.x + threadIdx.x;
    if (idx >= n_agents) return;

    float4 pos_i = positions[idx];
    float r2 = radius * radius;

    uint32_t count = 0;
    uint32_t* my_neighbors = neighbor_indices + idx * max_neighbors;

    for (size_t j = 0; j < n_agents && count < max_neighbors; j++) {
        if (j == idx) continue;

        float4 pos_j = positions[j];
        float dx = pos_i.x - pos_j.x;
        float dy = pos_i.y - pos_j.y;
        float dz = pos_i.z - pos_j.z;
        float d2 = dx * dx + dy * dy + dz * dz;

        if (d2 < r2) {
            my_neighbors[count++] = (uint32_t)j;
        }
    }

    neighbor_counts[idx] = count;
}

//=============================================================================
// Consensus Kernels
//=============================================================================

/**
 * @brief Parallel averaging step for consensus
 */
__global__ void kernel_consensus_averaging(
    const float* beliefs,        // [N x belief_dim]
    const float* weights,        // [N x N] adjacency weights
    const float* confidences,    // [N]
    float* new_beliefs,          // [N x belief_dim] output
    size_t n_agents,
    size_t belief_dim,
    float learning_rate)
{
    size_t idx = blockIdx.x * blockDim.x + threadIdx.x;
    if (idx >= n_agents) return;

    float conf_i = confidences[idx];

    for (size_t d = 0; d < belief_dim; d++) {
        float sum_weighted = 0.0f;
        float sum_weights = 0.0f;

        for (size_t j = 0; j < n_agents; j++) {
            float w = weights[idx * n_agents + j];
            if (w > 0.0f) {
                float conf_j = confidences[j];
                float belief_j = beliefs[j * belief_dim + d];
                sum_weighted += w * conf_j * belief_j;
                sum_weights += w * conf_j;
            }
        }

        float belief_i = beliefs[idx * belief_dim + d];
        float neighbor_avg = (sum_weights > 0.0f) ? sum_weighted / sum_weights : belief_i;

        // Update: blend current belief with neighbor average
        new_beliefs[idx * belief_dim + d] = belief_i + learning_rate * (neighbor_avg - belief_i);
    }
}

/**
 * @brief Belief propagation message passing
 */
__global__ void kernel_consensus_belief_propagation(
    const float* beliefs,
    const float* weights,
    float* new_beliefs,
    size_t n_agents,
    size_t belief_dim)
{
    size_t idx = blockIdx.x * blockDim.x + threadIdx.x;
    if (idx >= n_agents) return;

    for (size_t d = 0; d < belief_dim; d++) {
        float product = 1.0f;
        int neighbor_count = 0;

        for (size_t j = 0; j < n_agents; j++) {
            if (j == idx) continue;
            float w = weights[idx * n_agents + j];
            if (w > 0.0f) {
                float msg = beliefs[j * belief_dim + d];
                product *= fmaxf(msg, 0.001f);  // Avoid log(0)
                neighbor_count++;
            }
        }

        // Normalize using geometric mean
        if (neighbor_count > 0) {
            float geo_mean = powf(product, 1.0f / neighbor_count);
            new_beliefs[idx * belief_dim + d] = (beliefs[idx * belief_dim + d] + geo_mean) * 0.5f;
        } else {
            new_beliefs[idx * belief_dim + d] = beliefs[idx * belief_dim + d];
        }
    }
}

/**
 * @brief Opinion dynamics update (bounded confidence model)
 */
__global__ void kernel_consensus_opinion_dynamics(
    const float* beliefs,
    const float* weights,
    float* new_beliefs,
    size_t n_agents,
    size_t belief_dim,
    float confidence_threshold)
{
    size_t idx = blockIdx.x * blockDim.x + threadIdx.x;
    if (idx >= n_agents) return;

    for (size_t d = 0; d < belief_dim; d++) {
        float belief_i = beliefs[idx * belief_dim + d];
        float sum = belief_i;
        int count = 1;

        for (size_t j = 0; j < n_agents; j++) {
            if (j == idx) continue;
            float w = weights[idx * n_agents + j];
            if (w > 0.0f) {
                float belief_j = beliefs[j * belief_dim + d];
                // Only interact if opinions are close enough
                if (fabsf(belief_i - belief_j) < confidence_threshold) {
                    sum += belief_j;
                    count++;
                }
            }
        }

        new_beliefs[idx * belief_dim + d] = sum / count;
    }
}

/**
 * @brief Compute belief variance for convergence check
 */
__global__ void kernel_consensus_variance(
    const float* beliefs,
    float* variances,       // [belief_dim] output per-dimension variance
    size_t n_agents,
    size_t belief_dim)
{
    extern __shared__ float sdata[];

    size_t d = blockIdx.x;  // One block per belief dimension
    if (d >= belief_dim) return;

    size_t tid = threadIdx.x;

    // First pass: compute mean
    float sum = 0.0f;
    for (size_t i = tid; i < n_agents; i += blockDim.x) {
        sum += beliefs[i * belief_dim + d];
    }
    sdata[tid] = sum;
    __syncthreads();

    // Reduce to get sum
    for (size_t s = blockDim.x / 2; s > 0; s >>= 1) {
        if (tid < s) {
            sdata[tid] += sdata[tid + s];
        }
        __syncthreads();
    }
    float mean = sdata[0] / n_agents;
    __syncthreads();

    // Second pass: compute variance
    float var_sum = 0.0f;
    for (size_t i = tid; i < n_agents; i += blockDim.x) {
        float diff = beliefs[i * belief_dim + d] - mean;
        var_sum += diff * diff;
    }
    sdata[tid] = var_sum;
    __syncthreads();

    for (size_t s = blockDim.x / 2; s > 0; s >>= 1) {
        if (tid < s) {
            sdata[tid] += sdata[tid + s];
        }
        __syncthreads();
    }

    if (tid == 0) {
        variances[d] = sdata[0] / n_agents;
    }
}

//=============================================================================
// Pheromone Kernels
//=============================================================================

/**
 * @brief Apply pheromone diffusion (3D Laplacian)
 */
__global__ void kernel_pheromone_diffusion(
    const float* concentration,   // [Z x Y x X x n_types]
    float* new_concentration,     // [Z x Y x X x n_types] output
    uint32_t grid_x, uint32_t grid_y, uint32_t grid_z,
    uint32_t n_types,
    float diffusion_rate,
    float dt)
{
    size_t idx = blockIdx.x * blockDim.x + threadIdx.x;
    size_t total_cells = (size_t)grid_x * grid_y * grid_z;

    if (idx >= total_cells) return;

    // Convert linear index to 3D coordinates
    uint32_t x = idx % grid_x;
    uint32_t y = (idx / grid_x) % grid_y;
    uint32_t z = idx / (grid_x * grid_y);

    size_t stride = (size_t)grid_x * grid_y * grid_z;

    for (uint32_t t = 0; t < n_types; t++) {
        float center = concentration[idx * n_types + t];

        // Sum of neighbors (6-connected)
        float laplacian = -6.0f * center;

        if (x > 0) laplacian += concentration[(idx - 1) * n_types + t];
        else laplacian += center;  // Neumann BC

        if (x < grid_x - 1) laplacian += concentration[(idx + 1) * n_types + t];
        else laplacian += center;

        if (y > 0) laplacian += concentration[(idx - grid_x) * n_types + t];
        else laplacian += center;

        if (y < grid_y - 1) laplacian += concentration[(idx + grid_x) * n_types + t];
        else laplacian += center;

        size_t z_stride = (size_t)grid_x * grid_y;
        if (z > 0) laplacian += concentration[(idx - z_stride) * n_types + t];
        else laplacian += center;

        if (z < grid_z - 1) laplacian += concentration[(idx + z_stride) * n_types + t];
        else laplacian += center;

        // Update: c_new = c + D * dt * laplacian
        new_concentration[idx * n_types + t] = center + diffusion_rate * dt * laplacian;
    }
}

/**
 * @brief Apply pheromone decay
 */
__global__ void kernel_pheromone_decay(
    float* concentration,
    size_t total_elements,
    const float* decay_rates,  // [n_types]
    uint32_t n_types,
    float dt)
{
    size_t idx = blockIdx.x * blockDim.x + threadIdx.x;
    size_t total_cells = total_elements / n_types;

    if (idx >= total_cells) return;

    for (uint32_t t = 0; t < n_types; t++) {
        float c = concentration[idx * n_types + t];
        float decay = expf(-decay_rates[t] * dt);
        concentration[idx * n_types + t] = c * decay;
    }
}

/**
 * @brief Deposit pheromone at agent positions
 */
__global__ void kernel_pheromone_deposit(
    float* concentration,
    const float* positions,     // [N x 3]
    const uint32_t* types,      // [N]
    const float* amounts,       // [N]
    size_t n_deposits,
    uint32_t grid_x, uint32_t grid_y, uint32_t grid_z,
    uint32_t n_types,
    float voxel_size,
    float origin_x, float origin_y, float origin_z,
    float max_concentration)
{
    size_t idx = blockIdx.x * blockDim.x + threadIdx.x;
    if (idx >= n_deposits) return;

    float px = positions[idx * 3 + 0];
    float py = positions[idx * 3 + 1];
    float pz = positions[idx * 3 + 2];

    int cx = (int)floorf((px - origin_x) / voxel_size);
    int cy = (int)floorf((py - origin_y) / voxel_size);
    int cz = (int)floorf((pz - origin_z) / voxel_size);

    if (cx < 0 || cx >= (int)grid_x ||
        cy < 0 || cy >= (int)grid_y ||
        cz < 0 || cz >= (int)grid_z) return;

    size_t cell_idx = (size_t)cz * grid_y * grid_x + cy * grid_x + cx;
    uint32_t type = types[idx];
    if (type >= n_types) return;

    float amount = amounts[idx];

    // Atomic add to avoid race conditions
    float* target = &concentration[cell_idx * n_types + type];
    float old_val, new_val;
    do {
        old_val = *target;
        new_val = fminf(old_val + amount, max_concentration);
    } while (atomicCAS((unsigned int*)target, __float_as_uint(old_val), __float_as_uint(new_val)) != __float_as_uint(old_val));
}

/**
 * @brief Sample pheromone concentration at positions
 */
__global__ void kernel_pheromone_sample(
    const float* concentration,
    const float* positions,    // [N x 3]
    float* output,             // [N]
    size_t n_samples,
    uint32_t type,
    uint32_t grid_x, uint32_t grid_y, uint32_t grid_z,
    uint32_t n_types,
    float voxel_size,
    float origin_x, float origin_y, float origin_z)
{
    size_t idx = blockIdx.x * blockDim.x + threadIdx.x;
    if (idx >= n_samples) return;

    float px = positions[idx * 3 + 0];
    float py = positions[idx * 3 + 1];
    float pz = positions[idx * 3 + 2];

    // Trilinear interpolation
    float fx = (px - origin_x) / voxel_size - 0.5f;
    float fy = (py - origin_y) / voxel_size - 0.5f;
    float fz = (pz - origin_z) / voxel_size - 0.5f;

    int x0 = max(0, min((int)grid_x - 1, (int)floorf(fx)));
    int y0 = max(0, min((int)grid_y - 1, (int)floorf(fy)));
    int z0 = max(0, min((int)grid_z - 1, (int)floorf(fz)));
    int x1 = min((int)grid_x - 1, x0 + 1);
    int y1 = min((int)grid_y - 1, y0 + 1);
    int z1 = min((int)grid_z - 1, z0 + 1);

    float wx = fx - floorf(fx);
    float wy = fy - floorf(fy);
    float wz = fz - floorf(fz);

    // Sample 8 corners
    #define SAMPLE(X, Y, Z) concentration[((size_t)(Z) * grid_y * grid_x + (Y) * grid_x + (X)) * n_types + type]

    float c000 = SAMPLE(x0, y0, z0);
    float c100 = SAMPLE(x1, y0, z0);
    float c010 = SAMPLE(x0, y1, z0);
    float c110 = SAMPLE(x1, y1, z0);
    float c001 = SAMPLE(x0, y0, z1);
    float c101 = SAMPLE(x1, y0, z1);
    float c011 = SAMPLE(x0, y1, z1);
    float c111 = SAMPLE(x1, y1, z1);

    #undef SAMPLE

    // Trilinear interpolation
    float c00 = c000 * (1 - wx) + c100 * wx;
    float c01 = c001 * (1 - wx) + c101 * wx;
    float c10 = c010 * (1 - wx) + c110 * wx;
    float c11 = c011 * (1 - wx) + c111 * wx;

    float c0 = c00 * (1 - wy) + c10 * wy;
    float c1 = c01 * (1 - wy) + c11 * wy;

    output[idx] = c0 * (1 - wz) + c1 * wz;
}

/**
 * @brief Compute pheromone gradient at positions
 */
__global__ void kernel_pheromone_gradient(
    const float* concentration,
    const float* positions,    // [N x 3]
    float* gradients,          // [N x 3]
    size_t n_samples,
    uint32_t type,
    uint32_t grid_x, uint32_t grid_y, uint32_t grid_z,
    uint32_t n_types,
    float voxel_size,
    float origin_x, float origin_y, float origin_z)
{
    size_t idx = blockIdx.x * blockDim.x + threadIdx.x;
    if (idx >= n_samples) return;

    float px = positions[idx * 3 + 0];
    float py = positions[idx * 3 + 1];
    float pz = positions[idx * 3 + 2];

    int cx = (int)floorf((px - origin_x) / voxel_size);
    int cy = (int)floorf((py - origin_y) / voxel_size);
    int cz = (int)floorf((pz - origin_z) / voxel_size);

    cx = max(1, min((int)grid_x - 2, cx));
    cy = max(1, min((int)grid_y - 2, cy));
    cz = max(1, min((int)grid_z - 2, cz));

    #define SAMPLE(X, Y, Z) concentration[((size_t)(Z) * grid_y * grid_x + (Y) * grid_x + (X)) * n_types + type]

    // Central differences
    float inv_2h = 0.5f / voxel_size;
    gradients[idx * 3 + 0] = (SAMPLE(cx+1, cy, cz) - SAMPLE(cx-1, cy, cz)) * inv_2h;
    gradients[idx * 3 + 1] = (SAMPLE(cx, cy+1, cz) - SAMPLE(cx, cy-1, cz)) * inv_2h;
    gradients[idx * 3 + 2] = (SAMPLE(cx, cy, cz+1) - SAMPLE(cx, cy, cz-1)) * inv_2h;

    #undef SAMPLE
}

//=============================================================================
// Quorum Sensing Kernels
//=============================================================================

/**
 * @brief Compute total signal concentration from all agents
 */
__global__ void kernel_quorum_compute_concentration(
    const float* agent_signals,         // [N x n_types]
    float* signal_concentrations,       // [n_types]
    size_t n_agents,
    size_t n_signal_types)
{
    extern __shared__ float sdata[];

    size_t type = blockIdx.x;
    if (type >= n_signal_types) return;

    size_t tid = threadIdx.x;

    // Sum agent contributions
    float sum = 0.0f;
    for (size_t i = tid; i < n_agents; i += blockDim.x) {
        sum += agent_signals[i * n_signal_types + type];
    }
    sdata[tid] = sum;
    __syncthreads();

    // Parallel reduction
    for (size_t s = blockDim.x / 2; s > 0; s >>= 1) {
        if (tid < s) {
            sdata[tid] += sdata[tid + s];
        }
        __syncthreads();
    }

    if (tid == 0) {
        signal_concentrations[type] = sdata[0];
    }
}

/**
 * @brief Check if signals exceed threshold
 */
__global__ void kernel_quorum_check_thresholds(
    const float* signal_concentrations,  // [n_types]
    uint32_t* threshold_reached,         // [n_types] output (0 or 1)
    size_t n_signal_types,
    float base_threshold,
    size_t n_agents)
{
    size_t idx = blockIdx.x * blockDim.x + threadIdx.x;
    if (idx >= n_signal_types) return;

    // Threshold scales with population
    float threshold = base_threshold * n_agents;
    threshold_reached[idx] = (signal_concentrations[idx] >= threshold) ? 1 : 0;
}

/**
 * @brief Update agent commitments based on signal levels
 */
__global__ void kernel_quorum_update_commitments(
    float* agent_commitments,            // [N] in/out
    float* agent_strengths,              // [N] in/out
    const float* signal_concentrations,  // [n_types]
    size_t n_agents,
    size_t n_signal_types,
    float commitment_low,
    float commitment_high,
    float amplification,
    float inhibition)
{
    size_t idx = blockIdx.x * blockDim.x + threadIdx.x;
    if (idx >= n_agents) return;

    float current_strength = agent_strengths[idx];
    float current_commitment = agent_commitments[idx];

    // Find dominant signal
    float max_signal = 0.0f;
    int max_type = -1;
    for (size_t t = 0; t < n_signal_types; t++) {
        if (signal_concentrations[t] > max_signal) {
            max_signal = signal_concentrations[t];
            max_type = (int)t;
        }
    }

    // Update commitment strength based on signal
    if (max_type >= 0) {
        // Positive feedback from dominant signal
        float new_strength = current_strength + amplification * max_signal / (1.0f + max_signal);

        // Cross-inhibition from other signals
        for (size_t t = 0; t < n_signal_types; t++) {
            if (t != (size_t)max_type) {
                new_strength -= inhibition * signal_concentrations[t] / (1.0f + signal_concentrations[t]);
            }
        }

        new_strength = device_clamp(new_strength, 0.0f, 1.0f);
        agent_strengths[idx] = new_strength;

        // State transitions based on strength
        if (new_strength < commitment_low && current_commitment >= 0.0f) {
            agent_commitments[idx] = -1.0f;  // Uncommitted
        } else if (new_strength > commitment_high && current_commitment < 1.0f) {
            agent_commitments[idx] = (float)max_type;  // Committed to dominant type
        }
    }
}

/**
 * @brief Decay agent signals
 */
__global__ void kernel_quorum_decay_signals(
    float* agent_signals,    // [N x n_types]
    size_t n_agents,
    size_t n_signal_types,
    float decay_rate,
    float dt)
{
    size_t idx = blockIdx.x * blockDim.x + threadIdx.x;
    if (idx >= n_agents * n_signal_types) return;

    float decay = expf(-decay_rate * dt);
    agent_signals[idx] *= decay;
}

//=============================================================================
// Task Allocation Kernels
//=============================================================================

/**
 * @brief Compute capability match between agents and tasks
 */
__global__ void kernel_task_compute_matches(
    const float* agent_capabilities,    // [N x n_cap]
    const float* task_requirements,     // [M x n_cap]
    float* bids,                        // [N x M] output
    size_t n_agents,
    size_t n_tasks,
    size_t n_capability_types)
{
    size_t idx = blockIdx.x * blockDim.x + threadIdx.x;
    if (idx >= n_agents * n_tasks) return;

    size_t agent = idx / n_tasks;
    size_t task = idx % n_tasks;

    // Compute capability match as negative distance (higher = better match)
    float sum_sq = 0.0f;
    bool can_do = true;

    for (size_t c = 0; c < n_capability_types; c++) {
        float cap = agent_capabilities[agent * n_capability_types + c];
        float req = task_requirements[task * n_capability_types + c];

        // Check if agent can meet requirement
        if (cap < req) {
            can_do = false;
        }

        float diff = cap - req;
        sum_sq += diff * diff;
    }

    // Bid = capability match (negative if can't do)
    bids[idx] = can_do ? (1.0f / (1.0f + sqrtf(sum_sq))) : -1.0f;
}

/**
 * @brief Parallel auction round - agents bid on tasks
 */
__global__ void kernel_task_auction_round(
    const float* bids,           // [N x M]
    const float* prices,         // [M]
    int32_t* assignments,        // [N] (task ID or -1)
    float* best_bids,            // [M] output
    int32_t* best_agents,        // [M] output
    size_t n_agents,
    size_t n_tasks,
    float epsilon)
{
    size_t agent = blockIdx.x * blockDim.x + threadIdx.x;
    if (agent >= n_agents) return;

    // Each agent finds best task
    float best_value = -FLT_MAX;
    int32_t best_task = -1;
    float second_best = -FLT_MAX;

    for (size_t t = 0; t < n_tasks; t++) {
        float bid = bids[agent * n_tasks + t];
        float value = bid - prices[t];

        if (bid < 0.0f) continue;  // Can't do this task

        if (value > best_value) {
            second_best = best_value;
            best_value = value;
            best_task = (int32_t)t;
        } else if (value > second_best) {
            second_best = value;
        }
    }

    if (best_task >= 0) {
        // Compute bid increment
        float bid_inc = best_value - second_best + epsilon;
        float my_bid = bids[agent * n_tasks + best_task] + bid_inc;

        // Clamp negative bids to zero - atomicMax on float via int cast
        // requires non-negative values for correct ordering
        if (my_bid < 0.0f) my_bid = 0.0f;

        // Atomic max to compete for task
        atomicMax((int*)&best_bids[best_task], __float_as_int(my_bid));
    }

    assignments[agent] = best_task;
}

/**
 * @brief Update task prices based on winning bids
 */
__global__ void kernel_task_update_prices(
    float* prices,          // [M] in/out
    const float* best_bids, // [M]
    size_t n_tasks)
{
    size_t idx = blockIdx.x * blockDim.x + threadIdx.x;
    if (idx >= n_tasks) return;

    if (best_bids[idx] > 0.0f) {
        prices[idx] = best_bids[idx];
    }
}

/**
 * @brief Finalize assignments - resolve conflicts
 */
__global__ void kernel_task_finalize(
    int32_t* assignments,       // [N] in/out
    const float* bids,          // [N x M]
    const int32_t* best_agents, // [M]
    size_t n_agents,
    size_t n_tasks)
{
    size_t agent = blockIdx.x * blockDim.x + threadIdx.x;
    if (agent >= n_agents) return;

    int32_t my_task = assignments[agent];
    if (my_task >= 0 && my_task < (int32_t)n_tasks) {
        // Check if we're the winning agent for this task
        if (best_agents[my_task] != (int32_t)agent) {
            assignments[agent] = -1;  // Lost the bid
        }
    }
}

//=============================================================================
// Collision Detection Kernels
//=============================================================================

/**
 * @brief Detect collisions using brute force (for small N)
 */
__global__ void kernel_collision_detect_brute(
    const float4* positions,
    const float* radii,           // [N] or NULL for uniform
    float* collision_flags,       // [N] output (0.0 or 1.0)
    uint32_t* collision_pairs,    // [max_pairs x 2] output
    uint32_t* pair_count,         // Atomic counter
    size_t n_agents,
    size_t max_pairs,
    float default_radius,
    bool use_variable_radius)
{
    size_t idx = blockIdx.x * blockDim.x + threadIdx.x;
    if (idx >= n_agents) return;

    float4 pos_i = positions[idx];
    float r_i = use_variable_radius ? radii[idx] : default_radius;

    bool collided = false;

    // Check against ALL other agents (not just j > idx) to properly set our flag
    for (size_t j = 0; j < n_agents; j++) {
        if (j == idx) continue;

        float4 pos_j = positions[j];
        float r_j = use_variable_radius ? radii[j] : default_radius;

        float dx = pos_i.x - pos_j.x;
        float dy = pos_i.y - pos_j.y;
        float dz = pos_i.z - pos_j.z;
        float dist_sq = dx * dx + dy * dy + dz * dz;

        float min_dist = r_i + r_j;
        if (dist_sq < min_dist * min_dist) {
            collided = true;

            // Only record pair once (when idx < j) to avoid duplicates
            if (idx < j) {
                uint32_t pair_idx = atomicAdd(pair_count, 1);
                if (pair_idx < max_pairs) {
                    collision_pairs[pair_idx * 2 + 0] = (uint32_t)idx;
                    collision_pairs[pair_idx * 2 + 1] = (uint32_t)j;
                }
            }
        }
    }

    collision_flags[idx] = collided ? 1.0f : 0.0f;
}

/**
 * @brief Compute pairwise distances
 */
__global__ void kernel_pairwise_distances(
    const float4* positions,
    float* distances,       // [N x N]
    size_t n_agents)
{
    size_t idx = blockIdx.x * blockDim.x + threadIdx.x;
    if (idx >= n_agents * n_agents) return;

    size_t i = idx / n_agents;
    size_t j = idx % n_agents;

    if (i == j) {
        distances[idx] = 0.0f;
        return;
    }

    float4 pos_i = positions[i];
    float4 pos_j = positions[j];

    float dx = pos_i.x - pos_j.x;
    float dy = pos_i.y - pos_j.y;
    float dz = pos_i.z - pos_j.z;

    distances[idx] = sqrtf(dx * dx + dy * dy + dz * dz);
}

//=============================================================================
// Flocking API Implementation
//=============================================================================

extern "C" nimcp_flocking_gpu_state_t* nimcp_flocking_gpu_create(
    nimcp_gpu_context_t* ctx,
    size_t n_agents,
    size_t max_neighbors,
    const nimcp_flocking_gpu_params_t* params)
{
    if (!ctx || n_agents == 0) return NULL;
    if (n_agents > SWARM_GPU_MAX_AGENTS) {
        LOG_ERROR("n_agents (%zu) exceeds maximum (%d)", n_agents, SWARM_GPU_MAX_AGENTS);
        return NULL;
    }

    nimcp_flocking_gpu_state_t* state = (nimcp_flocking_gpu_state_t*)nimcp_calloc(1, sizeof(nimcp_flocking_gpu_state_t));
    if (!state) return NULL;

    state->n_agents = n_agents;
    state->max_neighbors = max_neighbors > 0 ? max_neighbors : SWARM_GPU_MAX_NEIGHBORS;

    if (params) {
        state->params = *params;
    } else {
        nimcp_flocking_gpu_default_params(&state->params);
    }

    // Create tensors
    size_t dims_agents[2] = {n_agents, 4};
    size_t dims_neighbors[2] = {n_agents, state->max_neighbors};
    size_t dims_counts[1] = {n_agents};

    state->positions = nimcp_gpu_tensor_create(ctx, dims_agents, 2, NIMCP_GPU_PRECISION_FP32);
    state->velocities = nimcp_gpu_tensor_create(ctx, dims_agents, 2, NIMCP_GPU_PRECISION_FP32);
    state->accelerations = nimcp_gpu_tensor_create(ctx, dims_agents, 2, NIMCP_GPU_PRECISION_FP32);
    state->forces = nimcp_gpu_tensor_create(ctx, dims_agents, 2, NIMCP_GPU_PRECISION_FP32);
    state->neighbor_counts = nimcp_gpu_tensor_create(ctx, dims_counts, 1, NIMCP_GPU_PRECISION_UINT32);
    state->neighbor_indices = nimcp_gpu_tensor_create(ctx, dims_neighbors, 2, NIMCP_GPU_PRECISION_UINT32);

    if (!state->positions || !state->velocities || !state->accelerations ||
        !state->forces || !state->neighbor_counts || !state->neighbor_indices) {
        LOG_ERROR("Failed to allocate GPU tensors for flocking state");
        nimcp_flocking_gpu_destroy(state);
        return NULL;
    }

    // Initialize to zero
    nimcp_gpu_zeros(ctx, state->positions);
    nimcp_gpu_zeros(ctx, state->velocities);
    nimcp_gpu_zeros(ctx, state->accelerations);
    nimcp_gpu_zeros(ctx, state->forces);
    nimcp_gpu_zeros(ctx, state->neighbor_counts);
    nimcp_gpu_zeros(ctx, state->neighbor_indices);

    LOG_INFO("Created flocking GPU state: %zu agents, %zu max neighbors",
             n_agents, state->max_neighbors);

    return state;
}

extern "C" void nimcp_flocking_gpu_destroy(nimcp_flocking_gpu_state_t* state)
{
    if (!state) return;

    if (state->positions) nimcp_gpu_tensor_destroy(state->positions);
    if (state->velocities) nimcp_gpu_tensor_destroy(state->velocities);
    if (state->accelerations) nimcp_gpu_tensor_destroy(state->accelerations);
    if (state->forces) nimcp_gpu_tensor_destroy(state->forces);
    if (state->neighbor_counts) nimcp_gpu_tensor_destroy(state->neighbor_counts);
    if (state->neighbor_indices) nimcp_gpu_tensor_destroy(state->neighbor_indices);

    nimcp_free(state);
    LOG_DEBUG("Destroyed flocking GPU state");
}

extern "C" bool nimcp_gpu_flocking_compute_forces(
    nimcp_gpu_context_t* ctx,
    nimcp_flocking_gpu_state_t* state)
{
    // Initialize GPU recovery if not already done
    if (!nimcp_gpu_recovery_is_initialized()) {
        nimcp_gpu_recovery_init(NULL);
    }

    if (!ctx || !state) return false;

    kernel_flocking_forces<<<GRID_SIZE(state->n_agents), BLOCK_SIZE>>>(
        (const float4*)state->positions->data,
        (const float4*)state->velocities->data,
        (float4*)state->forces->data,
        (const uint32_t*)state->neighbor_counts->data,
        (const uint32_t*)state->neighbor_indices->data,
        state->n_agents,
        state->max_neighbors,
        state->params.separation_weight, state->params.separation_radius,
        state->params.alignment_weight, state->params.alignment_radius,
        state->params.cohesion_weight, state->params.cohesion_radius,
        state->params.max_force);

    NIMCP_CUDA_RECOVER_LAST(GPU_ERROR_KERNEL_LAUNCH);
    return true;
}

extern "C" bool nimcp_gpu_flocking_update(
    nimcp_gpu_context_t* ctx,
    nimcp_flocking_gpu_state_t* state,
    float dt)
{
    // Initialize GPU recovery if not already done
    if (!nimcp_gpu_recovery_is_initialized()) {
        nimcp_gpu_recovery_init(NULL);
    }

    if (!ctx || !state) return false;

    float timestep = (dt > 0.0f) ? dt : state->params.dt;

    kernel_flocking_update<<<GRID_SIZE(state->n_agents), BLOCK_SIZE>>>(
        (float4*)state->positions->data,
        (float4*)state->velocities->data,
        (const float4*)state->forces->data,
        state->n_agents,
        timestep,
        state->params.max_speed);

    NIMCP_CUDA_RECOVER_LAST(GPU_ERROR_KERNEL_LAUNCH);
    return true;
}

extern "C" bool nimcp_gpu_flocking_separation(
    nimcp_gpu_context_t* ctx,
    nimcp_flocking_gpu_state_t* state)
{
    // Use compute_forces with only separation enabled
    return nimcp_gpu_flocking_compute_forces(ctx, state);
}

extern "C" bool nimcp_gpu_flocking_alignment(
    nimcp_gpu_context_t* ctx,
    nimcp_flocking_gpu_state_t* state)
{
    return nimcp_gpu_flocking_compute_forces(ctx, state);
}

extern "C" bool nimcp_gpu_flocking_cohesion(
    nimcp_gpu_context_t* ctx,
    nimcp_flocking_gpu_state_t* state)
{
    return nimcp_gpu_flocking_compute_forces(ctx, state);
}

extern "C" bool nimcp_gpu_flocking_find_neighbors(
    nimcp_gpu_context_t* ctx,
    nimcp_flocking_gpu_state_t* state,
    float radius)
{
    // Initialize GPU recovery if not already done
    if (!nimcp_gpu_recovery_is_initialized()) {
        nimcp_gpu_recovery_init(NULL);
    }

    if (!ctx || !state) return false;

    kernel_find_neighbors_brute<<<GRID_SIZE(state->n_agents), BLOCK_SIZE>>>(
        (const float4*)state->positions->data,
        (uint32_t*)state->neighbor_counts->data,
        (uint32_t*)state->neighbor_indices->data,
        state->n_agents,
        state->max_neighbors,
        radius);

    NIMCP_CUDA_RECOVER_LAST(GPU_ERROR_KERNEL_LAUNCH);
    return true;
}

//=============================================================================
// Spatial Hash API Implementation
//=============================================================================

extern "C" nimcp_spatial_hash_t* nimcp_spatial_hash_create(
    nimcp_gpu_context_t* ctx,
    float cell_size,
    uint32_t grid_dim_x,
    uint32_t grid_dim_y,
    uint32_t grid_dim_z,
    size_t max_particles)
{
    if (!ctx || cell_size <= 0.0f) return NULL;

    nimcp_spatial_hash_t* hash = (nimcp_spatial_hash_t*)nimcp_calloc(1, sizeof(nimcp_spatial_hash_t));
    if (!hash) return NULL;

    hash->cell_size = cell_size;
    hash->grid_dim_x = grid_dim_x;
    hash->grid_dim_y = grid_dim_y;
    hash->grid_dim_z = grid_dim_z;
    hash->origin_x = 0.0f;
    hash->origin_y = 0.0f;
    hash->origin_z = 0.0f;

    size_t n_cells = (size_t)grid_dim_x * grid_dim_y * grid_dim_z;
    size_t dims_cells[1] = {n_cells};
    size_t dims_particles[1] = {max_particles};

    hash->cell_start = nimcp_gpu_tensor_create(ctx, dims_cells, 1, NIMCP_GPU_PRECISION_UINT32);
    hash->cell_end = nimcp_gpu_tensor_create(ctx, dims_cells, 1, NIMCP_GPU_PRECISION_UINT32);
    hash->particle_cells = nimcp_gpu_tensor_create(ctx, dims_particles, 1, NIMCP_GPU_PRECISION_UINT32);
    hash->sorted_indices = nimcp_gpu_tensor_create(ctx, dims_particles, 1, NIMCP_GPU_PRECISION_UINT32);

    if (!hash->cell_start || !hash->cell_end || !hash->particle_cells || !hash->sorted_indices) {
        nimcp_spatial_hash_destroy(hash);
        return NULL;
    }

    return hash;
}

extern "C" void nimcp_spatial_hash_destroy(nimcp_spatial_hash_t* hash)
{
    if (!hash) return;

    if (hash->cell_start) nimcp_gpu_tensor_destroy(hash->cell_start);
    if (hash->cell_end) nimcp_gpu_tensor_destroy(hash->cell_end);
    if (hash->particle_cells) nimcp_gpu_tensor_destroy(hash->particle_cells);
    if (hash->sorted_indices) nimcp_gpu_tensor_destroy(hash->sorted_indices);

    nimcp_free(hash);
}

extern "C" bool nimcp_spatial_hash_clear(
    nimcp_gpu_context_t* ctx,
    nimcp_spatial_hash_t* hash)
{
    // Initialize GPU recovery if not already done
    if (!nimcp_gpu_recovery_is_initialized()) {
        nimcp_gpu_recovery_init(NULL);
    }

    if (!ctx || !hash) return false;

    nimcp_gpu_zeros(ctx, hash->cell_start);
    nimcp_gpu_zeros(ctx, hash->cell_end);

    return true;
}

extern "C" bool nimcp_gpu_spatial_hash_build(
    nimcp_gpu_context_t* ctx,
    nimcp_spatial_hash_t* hash,
    const nimcp_gpu_tensor_t* positions,
    size_t n_agents)
{
    // Initialize GPU recovery if not already done
    if (!nimcp_gpu_recovery_is_initialized()) {
        nimcp_gpu_recovery_init(NULL);
    }

    if (!ctx || !hash || !positions) return false;

    kernel_compute_cell_ids<<<GRID_SIZE(n_agents), BLOCK_SIZE>>>(
        (const float4*)positions->data,
        (uint32_t*)hash->particle_cells->data,
        n_agents,
        hash->cell_size,
        hash->grid_dim_x, hash->grid_dim_y, hash->grid_dim_z,
        hash->origin_x, hash->origin_y, hash->origin_z);

    NIMCP_CUDA_RECOVER_LAST(GPU_ERROR_KERNEL_LAUNCH);
    return true;
}

//=============================================================================
// Consensus API Implementation
//=============================================================================

extern "C" nimcp_consensus_gpu_state_t* nimcp_consensus_gpu_create(
    nimcp_gpu_context_t* ctx,
    size_t n_agents,
    size_t belief_dim,
    const nimcp_consensus_gpu_params_t* params)
{
    if (!ctx || n_agents == 0 || belief_dim == 0) return NULL;

    nimcp_consensus_gpu_state_t* state = (nimcp_consensus_gpu_state_t*)nimcp_calloc(1, sizeof(nimcp_consensus_gpu_state_t));
    if (!state) return NULL;

    state->n_agents = n_agents;
    state->belief_dim = belief_dim;

    if (params) {
        state->params = *params;
    } else {
        nimcp_consensus_gpu_default_params(&state->params);
    }

    size_t dims_beliefs[2] = {n_agents, belief_dim};
    size_t dims_conf[1] = {n_agents};
    size_t dims_weights[2] = {n_agents, n_agents};

    state->beliefs = nimcp_gpu_tensor_create(ctx, dims_beliefs, 2, NIMCP_GPU_PRECISION_FP32);
    state->confidences = nimcp_gpu_tensor_create(ctx, dims_conf, 1, NIMCP_GPU_PRECISION_FP32);
    state->weights = nimcp_gpu_tensor_create(ctx, dims_weights, 2, NIMCP_GPU_PRECISION_FP32);
    state->new_beliefs = nimcp_gpu_tensor_create(ctx, dims_beliefs, 2, NIMCP_GPU_PRECISION_FP32);

    if (!state->beliefs || !state->confidences || !state->weights || !state->new_beliefs) {
        nimcp_consensus_gpu_destroy(state);
        return NULL;
    }

    // Initialize
    nimcp_gpu_zeros(ctx, state->beliefs);
    nimcp_gpu_ones(ctx, state->confidences);
    nimcp_gpu_zeros(ctx, state->weights);

    LOG_INFO("Created consensus GPU state: %zu agents, %zu belief dimensions",
             n_agents, belief_dim);

    return state;
}

extern "C" void nimcp_consensus_gpu_destroy(nimcp_consensus_gpu_state_t* state)
{
    if (!state) return;

    if (state->beliefs) nimcp_gpu_tensor_destroy(state->beliefs);
    if (state->confidences) nimcp_gpu_tensor_destroy(state->confidences);
    if (state->weights) nimcp_gpu_tensor_destroy(state->weights);
    if (state->new_beliefs) nimcp_gpu_tensor_destroy(state->new_beliefs);

    nimcp_free(state);
}

extern "C" bool nimcp_gpu_consensus_averaging(
    nimcp_gpu_context_t* ctx,
    nimcp_consensus_gpu_state_t* state)
{
    // Initialize GPU recovery if not already done
    if (!nimcp_gpu_recovery_is_initialized()) {
        nimcp_gpu_recovery_init(NULL);
    }

    if (!ctx || !state) return false;

    kernel_consensus_averaging<<<GRID_SIZE(state->n_agents), BLOCK_SIZE>>>(
        (const float*)state->beliefs->data,
        (const float*)state->weights->data,
        (const float*)state->confidences->data,
        (float*)state->new_beliefs->data,
        state->n_agents,
        state->belief_dim,
        state->params.learning_rate);

    // Swap buffers
    nimcp_gpu_tensor_t* tmp = state->beliefs;
    state->beliefs = state->new_beliefs;
    state->new_beliefs = tmp;

    NIMCP_CUDA_RECOVER_LAST(GPU_ERROR_KERNEL_LAUNCH);
    return true;
}

extern "C" bool nimcp_gpu_consensus_belief_propagation(
    nimcp_gpu_context_t* ctx,
    nimcp_consensus_gpu_state_t* state)
{
    // Initialize GPU recovery if not already done
    if (!nimcp_gpu_recovery_is_initialized()) {
        nimcp_gpu_recovery_init(NULL);
    }

    if (!ctx || !state) return false;

    kernel_consensus_belief_propagation<<<GRID_SIZE(state->n_agents), BLOCK_SIZE>>>(
        (const float*)state->beliefs->data,
        (const float*)state->weights->data,
        (float*)state->new_beliefs->data,
        state->n_agents,
        state->belief_dim);

    nimcp_gpu_tensor_t* tmp = state->beliefs;
    state->beliefs = state->new_beliefs;
    state->new_beliefs = tmp;

    NIMCP_CUDA_RECOVER_LAST(GPU_ERROR_KERNEL_LAUNCH);
    return true;
}

extern "C" bool nimcp_gpu_consensus_opinion_dynamics(
    nimcp_gpu_context_t* ctx,
    nimcp_consensus_gpu_state_t* state)
{
    // Initialize GPU recovery if not already done
    if (!nimcp_gpu_recovery_is_initialized()) {
        nimcp_gpu_recovery_init(NULL);
    }

    if (!ctx || !state) return false;

    float confidence_threshold = 0.5f;  // Bounded confidence model threshold

    kernel_consensus_opinion_dynamics<<<GRID_SIZE(state->n_agents), BLOCK_SIZE>>>(
        (const float*)state->beliefs->data,
        (const float*)state->weights->data,
        (float*)state->new_beliefs->data,
        state->n_agents,
        state->belief_dim,
        confidence_threshold);

    nimcp_gpu_tensor_t* tmp = state->beliefs;
    state->beliefs = state->new_beliefs;
    state->new_beliefs = tmp;

    NIMCP_CUDA_RECOVER_LAST(GPU_ERROR_KERNEL_LAUNCH);
    return true;
}

extern "C" bool nimcp_gpu_consensus_check_convergence(
    nimcp_gpu_context_t* ctx,
    const nimcp_consensus_gpu_state_t* state,
    bool* converged,
    float* variance)
{
    // Initialize GPU recovery if not already done
    if (!nimcp_gpu_recovery_is_initialized()) {
        nimcp_gpu_recovery_init(NULL);
    }

    if (!ctx || !state || !converged || !variance) return false;

    // Allocate device memory for variances
    float* d_variances = NULL;
    float* h_variances = NULL;
    bool success = false;

    if (cudaMalloc(&d_variances, state->belief_dim * sizeof(float)) != cudaSuccess) {
        LOG_ERROR("Failed to allocate device variances for convergence check");
        goto cleanup_convergence;
    }

    {
        size_t shared_size = BLOCK_SIZE * sizeof(float);
        kernel_consensus_variance<<<state->belief_dim, BLOCK_SIZE, shared_size>>>(
            (const float*)state->beliefs->data,
            d_variances,
            state->n_agents,
            state->belief_dim);
    }

    // Copy back and compute total variance
    h_variances = (float*)nimcp_malloc(state->belief_dim * sizeof(float));
    if (!h_variances) {
        LOG_ERROR("Failed to allocate host variances");
        goto cleanup_convergence;
    }

    if (cudaMemcpy(h_variances, d_variances, state->belief_dim * sizeof(float),
                   cudaMemcpyDeviceToHost) != cudaSuccess) {
        LOG_ERROR("Failed to copy variances from device");
        goto cleanup_convergence;
    }

    {
        float total_var = 0.0f;
        for (size_t d = 0; d < state->belief_dim; d++) {
            total_var += h_variances[d];
        }
        total_var /= state->belief_dim;

        *variance = total_var;
        *converged = (total_var < state->params.min_confidence);
    }

    success = true;

cleanup_convergence:
    nimcp_free(h_variances);
    cudaFree(d_variances);

    return success;
}

//=============================================================================
// Pheromone API Implementation
//=============================================================================

extern "C" nimcp_pheromone_gpu_state_t* nimcp_pheromone_gpu_create(
    nimcp_gpu_context_t* ctx,
    uint32_t grid_x,
    uint32_t grid_y,
    uint32_t grid_z,
    uint32_t n_types,
    float voxel_size,
    const nimcp_pheromone_gpu_params_t* params)
{
    if (!ctx || grid_x == 0 || grid_y == 0 || n_types == 0) return NULL;
    if (n_types > SWARM_GPU_MAX_PHEROMONE_TYPES) {
        LOG_ERROR("n_types exceeds maximum");
        return NULL;
    }

    nimcp_pheromone_gpu_state_t* state = (nimcp_pheromone_gpu_state_t*)nimcp_calloc(1, sizeof(nimcp_pheromone_gpu_state_t));
    if (!state) return NULL;

    state->grid_x = grid_x;
    state->grid_y = grid_y;
    state->grid_z = grid_z > 0 ? grid_z : 1;
    state->n_types = n_types;
    state->voxel_size = voxel_size;
    state->origin_x = state->origin_y = state->origin_z = 0.0f;

    if (params) {
        state->params = *params;
    } else {
        nimcp_pheromone_gpu_default_params(&state->params);
    }

    size_t total_cells = (size_t)grid_x * grid_y * state->grid_z * n_types;
    size_t dims[1] = {total_cells};

    state->concentration = nimcp_gpu_tensor_create(ctx, dims, 1, NIMCP_GPU_PRECISION_FP32);
    state->temp_buffer = nimcp_gpu_tensor_create(ctx, dims, 1, NIMCP_GPU_PRECISION_FP32);

    if (!state->concentration || !state->temp_buffer) {
        nimcp_pheromone_gpu_destroy(state);
        return NULL;
    }

    nimcp_gpu_zeros(ctx, state->concentration);

    LOG_INFO("Created pheromone GPU state: %ux%ux%u grid, %u types",
             grid_x, grid_y, state->grid_z, n_types);

    return state;
}

extern "C" void nimcp_pheromone_gpu_destroy(nimcp_pheromone_gpu_state_t* state)
{
    if (!state) return;

    if (state->concentration) nimcp_gpu_tensor_destroy(state->concentration);
    if (state->temp_buffer) nimcp_gpu_tensor_destroy(state->temp_buffer);
    if (state->gradient) nimcp_gpu_tensor_destroy(state->gradient);

    nimcp_free(state);
}

extern "C" bool nimcp_gpu_pheromone_diffusion(
    nimcp_gpu_context_t* ctx,
    nimcp_pheromone_gpu_state_t* state,
    float dt)
{
    // Initialize GPU recovery if not already done
    if (!nimcp_gpu_recovery_is_initialized()) {
        nimcp_gpu_recovery_init(NULL);
    }

    if (!ctx || !state) return false;

    size_t total_cells = (size_t)state->grid_x * state->grid_y * state->grid_z;

    kernel_pheromone_diffusion<<<GRID_SIZE(total_cells), BLOCK_SIZE>>>(
        (const float*)state->concentration->data,
        (float*)state->temp_buffer->data,
        state->grid_x, state->grid_y, state->grid_z,
        state->n_types,
        state->params.diffusion_rate,
        dt);

    // Swap buffers
    nimcp_gpu_tensor_t* tmp = state->concentration;
    state->concentration = state->temp_buffer;
    state->temp_buffer = tmp;

    NIMCP_CUDA_RECOVER_LAST(GPU_ERROR_KERNEL_LAUNCH);
    return true;
}

extern "C" bool nimcp_gpu_pheromone_decay(
    nimcp_gpu_context_t* ctx,
    nimcp_pheromone_gpu_state_t* state,
    float dt)
{
    // Initialize GPU recovery if not already done
    if (!nimcp_gpu_recovery_is_initialized()) {
        nimcp_gpu_recovery_init(NULL);
    }

    if (!ctx || !state) return false;

    // Upload decay rates
    float* d_decay_rates = NULL;
    if (cudaMalloc(&d_decay_rates, SWARM_GPU_MAX_PHEROMONE_TYPES * sizeof(float)) != cudaSuccess) {
        LOG_ERROR("Failed to allocate decay rates on device");
        return false;
    }
    if (cudaMemcpy(d_decay_rates, state->params.decay_rates,
                   SWARM_GPU_MAX_PHEROMONE_TYPES * sizeof(float), cudaMemcpyHostToDevice) != cudaSuccess) {
        LOG_ERROR("Failed to copy decay rates to device");
        cudaFree(d_decay_rates);
        return false;
    }

    {
        size_t total_cells = (size_t)state->grid_x * state->grid_y * state->grid_z;

        kernel_pheromone_decay<<<GRID_SIZE(total_cells), BLOCK_SIZE>>>(
            (float*)state->concentration->data,
            state->concentration->numel,
            d_decay_rates,
            state->n_types,
            dt);
    }

    cudaFree(d_decay_rates);
    NIMCP_CUDA_RECOVER_LAST(GPU_ERROR_KERNEL_LAUNCH);
    return true;
}

extern "C" bool nimcp_gpu_pheromone_deposit(
    nimcp_gpu_context_t* ctx,
    nimcp_pheromone_gpu_state_t* state,
    const nimcp_gpu_tensor_t* positions,
    const nimcp_gpu_tensor_t* types,
    const nimcp_gpu_tensor_t* amounts,
    size_t n_deposits)
{
    // Initialize GPU recovery if not already done
    if (!nimcp_gpu_recovery_is_initialized()) {
        nimcp_gpu_recovery_init(NULL);
    }

    if (!ctx || !state || !positions || !types || !amounts) return false;

    kernel_pheromone_deposit<<<GRID_SIZE(n_deposits), BLOCK_SIZE>>>(
        (float*)state->concentration->data,
        (const float*)positions->data,
        (const uint32_t*)types->data,
        (const float*)amounts->data,
        n_deposits,
        state->grid_x, state->grid_y, state->grid_z,
        state->n_types,
        state->voxel_size,
        state->origin_x, state->origin_y, state->origin_z,
        state->params.max_concentration);

    NIMCP_CUDA_RECOVER_LAST(GPU_ERROR_KERNEL_LAUNCH);
    return true;
}

extern "C" bool nimcp_gpu_pheromone_sample(
    nimcp_gpu_context_t* ctx,
    const nimcp_pheromone_gpu_state_t* state,
    const nimcp_gpu_tensor_t* positions,
    uint32_t type,
    nimcp_gpu_tensor_t* output)
{
    // Initialize GPU recovery if not already done
    if (!nimcp_gpu_recovery_is_initialized()) {
        nimcp_gpu_recovery_init(NULL);
    }

    if (!ctx || !state || !positions || !output) return false;

    size_t n_samples = positions->dims[0];  // First dimension is number of samples

    kernel_pheromone_sample<<<GRID_SIZE(n_samples), BLOCK_SIZE>>>(
        (const float*)state->concentration->data,
        (const float*)positions->data,
        (float*)output->data,
        n_samples,
        type,
        state->grid_x, state->grid_y, state->grid_z,
        state->n_types,
        state->voxel_size,
        state->origin_x, state->origin_y, state->origin_z);

    NIMCP_CUDA_RECOVER_LAST(GPU_ERROR_KERNEL_LAUNCH);
    return true;
}

extern "C" bool nimcp_gpu_pheromone_gradient(
    nimcp_gpu_context_t* ctx,
    const nimcp_pheromone_gpu_state_t* state,
    const nimcp_gpu_tensor_t* positions,
    uint32_t type,
    nimcp_gpu_tensor_t* gradients)
{
    // Initialize GPU recovery if not already done
    if (!nimcp_gpu_recovery_is_initialized()) {
        nimcp_gpu_recovery_init(NULL);
    }

    if (!ctx || !state || !positions || !gradients) return false;

    size_t n_samples = positions->dims[0];  // First dimension is number of samples

    kernel_pheromone_gradient<<<GRID_SIZE(n_samples), BLOCK_SIZE>>>(
        (const float*)state->concentration->data,
        (const float*)positions->data,
        (float*)gradients->data,
        n_samples,
        type,
        state->grid_x, state->grid_y, state->grid_z,
        state->n_types,
        state->voxel_size,
        state->origin_x, state->origin_y, state->origin_z);

    NIMCP_CUDA_RECOVER_LAST(GPU_ERROR_KERNEL_LAUNCH);
    return true;
}

//=============================================================================
// Quorum Sensing API Implementation
//=============================================================================

extern "C" nimcp_quorum_gpu_state_t* nimcp_quorum_gpu_create(
    nimcp_gpu_context_t* ctx,
    size_t n_agents,
    size_t n_signal_types,
    const nimcp_quorum_gpu_params_t* params)
{
    if (!ctx || n_agents == 0 || n_signal_types == 0) return NULL;
    if (n_signal_types > SWARM_GPU_MAX_SIGNAL_TYPES) {
        LOG_ERROR("n_signal_types exceeds maximum");
        return NULL;
    }

    nimcp_quorum_gpu_state_t* state = (nimcp_quorum_gpu_state_t*)nimcp_calloc(1, sizeof(nimcp_quorum_gpu_state_t));
    if (!state) return NULL;

    state->n_agents = n_agents;
    state->n_signal_types = n_signal_types;

    if (params) {
        state->params = *params;
    } else {
        nimcp_quorum_gpu_default_params(&state->params);
    }

    size_t dims_conc[1] = {n_signal_types};
    size_t dims_signals[2] = {n_agents, n_signal_types};
    size_t dims_agents[1] = {n_agents};

    state->signal_concentrations = nimcp_gpu_tensor_create(ctx, dims_conc, 1, NIMCP_GPU_PRECISION_FP32);
    state->agent_signals = nimcp_gpu_tensor_create(ctx, dims_signals, 2, NIMCP_GPU_PRECISION_FP32);
    state->agent_commitments = nimcp_gpu_tensor_create(ctx, dims_agents, 1, NIMCP_GPU_PRECISION_FP32);
    state->agent_strengths = nimcp_gpu_tensor_create(ctx, dims_agents, 1, NIMCP_GPU_PRECISION_FP32);
    state->threshold_reached = nimcp_gpu_tensor_create(ctx, dims_conc, 1, NIMCP_GPU_PRECISION_UINT32);

    if (!state->signal_concentrations || !state->agent_signals ||
        !state->agent_commitments || !state->agent_strengths || !state->threshold_reached) {
        nimcp_quorum_gpu_destroy(state);
        return NULL;
    }

    nimcp_gpu_zeros(ctx, state->signal_concentrations);
    nimcp_gpu_zeros(ctx, state->agent_signals);
    nimcp_gpu_zeros(ctx, state->agent_strengths);
    nimcp_gpu_zeros(ctx, state->threshold_reached);

    // Initialize commitments to -1 (uncommitted)
    float neg_one = -1.0f;
    size_t numel = state->agent_commitments->numel;
    float* h_data = (float*)nimcp_malloc(numel * sizeof(float));
    for (size_t i = 0; i < numel; i++) h_data[i] = neg_one;
    cudaMemcpy(state->agent_commitments->data, h_data, numel * sizeof(float), cudaMemcpyHostToDevice);
    nimcp_free(h_data);

    LOG_INFO("Created quorum GPU state: %zu agents, %zu signal types",
             n_agents, n_signal_types);

    return state;
}

extern "C" void nimcp_quorum_gpu_destroy(nimcp_quorum_gpu_state_t* state)
{
    if (!state) return;

    if (state->signal_concentrations) nimcp_gpu_tensor_destroy(state->signal_concentrations);
    if (state->agent_signals) nimcp_gpu_tensor_destroy(state->agent_signals);
    if (state->agent_commitments) nimcp_gpu_tensor_destroy(state->agent_commitments);
    if (state->agent_strengths) nimcp_gpu_tensor_destroy(state->agent_strengths);
    if (state->threshold_reached) nimcp_gpu_tensor_destroy(state->threshold_reached);

    nimcp_free(state);
}

extern "C" bool nimcp_gpu_quorum_compute_concentration(
    nimcp_gpu_context_t* ctx,
    nimcp_quorum_gpu_state_t* state)
{
    // Initialize GPU recovery if not already done
    if (!nimcp_gpu_recovery_is_initialized()) {
        nimcp_gpu_recovery_init(NULL);
    }

    if (!ctx || !state) return false;

    size_t shared_size = BLOCK_SIZE * sizeof(float);
    kernel_quorum_compute_concentration<<<state->n_signal_types, BLOCK_SIZE, shared_size>>>(
        (const float*)state->agent_signals->data,
        (float*)state->signal_concentrations->data,
        state->n_agents,
        state->n_signal_types);

    NIMCP_CUDA_RECOVER_LAST(GPU_ERROR_KERNEL_LAUNCH);
    return true;
}

extern "C" bool nimcp_gpu_quorum_check_thresholds(
    nimcp_gpu_context_t* ctx,
    nimcp_quorum_gpu_state_t* state)
{
    // Initialize GPU recovery if not already done
    if (!nimcp_gpu_recovery_is_initialized()) {
        nimcp_gpu_recovery_init(NULL);
    }

    if (!ctx || !state) return false;

    kernel_quorum_check_thresholds<<<GRID_SIZE(state->n_signal_types), BLOCK_SIZE>>>(
        (const float*)state->signal_concentrations->data,
        (uint32_t*)state->threshold_reached->data,
        state->n_signal_types,
        state->params.base_threshold,
        state->n_agents);

    NIMCP_CUDA_RECOVER_LAST(GPU_ERROR_KERNEL_LAUNCH);
    return true;
}

extern "C" bool nimcp_gpu_quorum_update_commitments(
    nimcp_gpu_context_t* ctx,
    nimcp_quorum_gpu_state_t* state)
{
    // Initialize GPU recovery if not already done
    if (!nimcp_gpu_recovery_is_initialized()) {
        nimcp_gpu_recovery_init(NULL);
    }

    if (!ctx || !state) return false;

    kernel_quorum_update_commitments<<<GRID_SIZE(state->n_agents), BLOCK_SIZE>>>(
        (float*)state->agent_commitments->data,
        (float*)state->agent_strengths->data,
        (const float*)state->signal_concentrations->data,
        state->n_agents,
        state->n_signal_types,
        state->params.commitment_low,
        state->params.commitment_high,
        state->params.amplification,
        state->params.inhibition);

    NIMCP_CUDA_RECOVER_LAST(GPU_ERROR_KERNEL_LAUNCH);
    return true;
}

extern "C" bool nimcp_gpu_quorum_decay_signals(
    nimcp_gpu_context_t* ctx,
    nimcp_quorum_gpu_state_t* state,
    float dt)
{
    // Initialize GPU recovery if not already done
    if (!nimcp_gpu_recovery_is_initialized()) {
        nimcp_gpu_recovery_init(NULL);
    }

    if (!ctx || !state) return false;

    size_t total = state->n_agents * state->n_signal_types;

    kernel_quorum_decay_signals<<<GRID_SIZE(total), BLOCK_SIZE>>>(
        (float*)state->agent_signals->data,
        state->n_agents,
        state->n_signal_types,
        state->params.decay_rate,
        dt);

    NIMCP_CUDA_RECOVER_LAST(GPU_ERROR_KERNEL_LAUNCH);
    return true;
}

//=============================================================================
// Task Allocation API Implementation
//=============================================================================

extern "C" nimcp_task_alloc_gpu_state_t* nimcp_task_alloc_gpu_create(
    nimcp_gpu_context_t* ctx,
    size_t n_agents,
    size_t n_tasks,
    size_t n_capability_types,
    const nimcp_task_alloc_gpu_params_t* params)
{
    if (!ctx || n_agents == 0 || n_tasks == 0 || n_capability_types == 0) return NULL;

    nimcp_task_alloc_gpu_state_t* state = (nimcp_task_alloc_gpu_state_t*)nimcp_calloc(1, sizeof(nimcp_task_alloc_gpu_state_t));
    if (!state) return NULL;

    state->n_agents = n_agents;
    state->n_tasks = n_tasks;
    state->n_capability_types = n_capability_types;

    if (params) {
        state->params = *params;
    } else {
        nimcp_task_alloc_gpu_default_params(&state->params);
    }

    size_t dims_caps[2] = {n_agents, n_capability_types};
    size_t dims_reqs[2] = {n_tasks, n_capability_types};
    size_t dims_bids[2] = {n_agents, n_tasks};
    size_t dims_prices[1] = {n_tasks};
    size_t dims_assign[1] = {n_agents};

    state->agent_capabilities = nimcp_gpu_tensor_create(ctx, dims_caps, 2, NIMCP_GPU_PRECISION_FP32);
    state->task_requirements = nimcp_gpu_tensor_create(ctx, dims_reqs, 2, NIMCP_GPU_PRECISION_FP32);
    state->bids = nimcp_gpu_tensor_create(ctx, dims_bids, 2, NIMCP_GPU_PRECISION_FP32);
    state->prices = nimcp_gpu_tensor_create(ctx, dims_prices, 1, NIMCP_GPU_PRECISION_FP32);
    state->assignments = nimcp_gpu_tensor_create(ctx, dims_assign, 1, NIMCP_GPU_PRECISION_INT32);
    state->best_bids = nimcp_gpu_tensor_create(ctx, dims_prices, 1, NIMCP_GPU_PRECISION_FP32);
    state->best_agents = nimcp_gpu_tensor_create(ctx, dims_prices, 1, NIMCP_GPU_PRECISION_INT32);

    if (!state->agent_capabilities || !state->task_requirements || !state->bids ||
        !state->prices || !state->assignments || !state->best_bids || !state->best_agents) {
        nimcp_task_alloc_gpu_destroy(state);
        return NULL;
    }

    nimcp_gpu_zeros(ctx, state->prices);
    nimcp_gpu_zeros(ctx, state->bids);

    // Initialize assignments to -1
    int32_t* h_assign = (int32_t*)nimcp_malloc(n_agents * sizeof(int32_t));
    for (size_t i = 0; i < n_agents; i++) h_assign[i] = -1;
    cudaMemcpy(state->assignments->data, h_assign, n_agents * sizeof(int32_t), cudaMemcpyHostToDevice);
    nimcp_free(h_assign);

    LOG_INFO("Created task allocation GPU state: %zu agents, %zu tasks, %zu capabilities",
             n_agents, n_tasks, n_capability_types);

    return state;
}

extern "C" void nimcp_task_alloc_gpu_destroy(nimcp_task_alloc_gpu_state_t* state)
{
    if (!state) return;

    if (state->agent_capabilities) nimcp_gpu_tensor_destroy(state->agent_capabilities);
    if (state->task_requirements) nimcp_gpu_tensor_destroy(state->task_requirements);
    if (state->bids) nimcp_gpu_tensor_destroy(state->bids);
    if (state->prices) nimcp_gpu_tensor_destroy(state->prices);
    if (state->assignments) nimcp_gpu_tensor_destroy(state->assignments);
    if (state->best_bids) nimcp_gpu_tensor_destroy(state->best_bids);
    if (state->best_agents) nimcp_gpu_tensor_destroy(state->best_agents);

    nimcp_free(state);
}

extern "C" bool nimcp_gpu_task_compute_matches(
    nimcp_gpu_context_t* ctx,
    nimcp_task_alloc_gpu_state_t* state)
{
    // Initialize GPU recovery if not already done
    if (!nimcp_gpu_recovery_is_initialized()) {
        nimcp_gpu_recovery_init(NULL);
    }

    if (!ctx || !state) return false;

    size_t total = state->n_agents * state->n_tasks;

    kernel_task_compute_matches<<<GRID_SIZE(total), BLOCK_SIZE>>>(
        (const float*)state->agent_capabilities->data,
        (const float*)state->task_requirements->data,
        (float*)state->bids->data,
        state->n_agents,
        state->n_tasks,
        state->n_capability_types);

    NIMCP_CUDA_RECOVER_LAST(GPU_ERROR_KERNEL_LAUNCH);
    return true;
}

extern "C" bool nimcp_gpu_task_auction_round(
    nimcp_gpu_context_t* ctx,
    nimcp_task_alloc_gpu_state_t* state)
{
    // Initialize GPU recovery if not already done
    if (!nimcp_gpu_recovery_is_initialized()) {
        nimcp_gpu_recovery_init(NULL);
    }

    if (!ctx || !state) return false;

    // Clear best bids
    nimcp_gpu_zeros(ctx, state->best_bids);

    kernel_task_auction_round<<<GRID_SIZE(state->n_agents), BLOCK_SIZE>>>(
        (const float*)state->bids->data,
        (const float*)state->prices->data,
        (int32_t*)state->assignments->data,
        (float*)state->best_bids->data,
        (int32_t*)state->best_agents->data,
        state->n_agents,
        state->n_tasks,
        state->params.epsilon);

    NIMCP_CUDA_RECOVER_LAST(GPU_ERROR_KERNEL_LAUNCH);
    return true;
}

extern "C" bool nimcp_gpu_task_update_prices(
    nimcp_gpu_context_t* ctx,
    nimcp_task_alloc_gpu_state_t* state)
{
    // Initialize GPU recovery if not already done
    if (!nimcp_gpu_recovery_is_initialized()) {
        nimcp_gpu_recovery_init(NULL);
    }

    if (!ctx || !state) return false;

    kernel_task_update_prices<<<GRID_SIZE(state->n_tasks), BLOCK_SIZE>>>(
        (float*)state->prices->data,
        (const float*)state->best_bids->data,
        state->n_tasks);

    NIMCP_CUDA_RECOVER_LAST(GPU_ERROR_KERNEL_LAUNCH);
    return true;
}

extern "C" bool nimcp_gpu_task_finalize_assignments(
    nimcp_gpu_context_t* ctx,
    nimcp_task_alloc_gpu_state_t* state)
{
    // Initialize GPU recovery if not already done
    if (!nimcp_gpu_recovery_is_initialized()) {
        nimcp_gpu_recovery_init(NULL);
    }

    if (!ctx || !state) return false;

    kernel_task_finalize<<<GRID_SIZE(state->n_agents), BLOCK_SIZE>>>(
        (int32_t*)state->assignments->data,
        (const float*)state->bids->data,
        (const int32_t*)state->best_agents->data,
        state->n_agents,
        state->n_tasks);

    NIMCP_CUDA_RECOVER_LAST(GPU_ERROR_KERNEL_LAUNCH);
    return true;
}

//=============================================================================
// Collision Detection API Implementation
//=============================================================================

extern "C" nimcp_collision_gpu_state_t* nimcp_collision_gpu_create(
    nimcp_gpu_context_t* ctx,
    size_t n_agents,
    size_t max_pairs,
    const nimcp_collision_gpu_params_t* params)
{
    if (!ctx || n_agents == 0) return NULL;

    nimcp_collision_gpu_state_t* state = (nimcp_collision_gpu_state_t*)nimcp_calloc(1, sizeof(nimcp_collision_gpu_state_t));
    if (!state) return NULL;

    state->n_agents = n_agents;
    state->max_pairs = max_pairs > 0 ? max_pairs : n_agents * 10;

    if (params) {
        state->params = *params;
    } else {
        nimcp_collision_gpu_default_params(&state->params);
    }

    size_t dims_pos[2] = {n_agents, 4};
    size_t dims_radii[1] = {n_agents};
    size_t dims_flags[1] = {n_agents};
    size_t dims_pairs[2] = {state->max_pairs, 2};
    size_t dims_count[1] = {1};

    state->positions = nimcp_gpu_tensor_create(ctx, dims_pos, 2, NIMCP_GPU_PRECISION_FP32);
    state->radii = nimcp_gpu_tensor_create(ctx, dims_radii, 1, NIMCP_GPU_PRECISION_FP32);
    state->collision_flags = nimcp_gpu_tensor_create(ctx, dims_flags, 1, NIMCP_GPU_PRECISION_UINT32);
    state->collision_pairs = nimcp_gpu_tensor_create(ctx, dims_pairs, 2, NIMCP_GPU_PRECISION_UINT32);
    state->pair_count = nimcp_gpu_tensor_create(ctx, dims_count, 1, NIMCP_GPU_PRECISION_UINT32);

    if (!state->positions || !state->collision_flags ||
        !state->collision_pairs || !state->pair_count) {
        nimcp_collision_gpu_destroy(state);
        return NULL;
    }

    nimcp_gpu_zeros(ctx, state->collision_flags);
    nimcp_gpu_zeros(ctx, state->pair_count);

    // Initialize radii to default
    float default_radius = state->params.collision_radius;
    float* h_radii = (float*)nimcp_malloc(n_agents * sizeof(float));
    for (size_t i = 0; i < n_agents; i++) h_radii[i] = default_radius;
    cudaMemcpy(state->radii->data, h_radii, n_agents * sizeof(float), cudaMemcpyHostToDevice);
    nimcp_free(h_radii);

    LOG_INFO("Created collision GPU state: %zu agents, %zu max pairs",
             n_agents, state->max_pairs);

    return state;
}

extern "C" void nimcp_collision_gpu_destroy(nimcp_collision_gpu_state_t* state)
{
    if (!state) return;

    if (state->positions) nimcp_gpu_tensor_destroy(state->positions);
    if (state->radii) nimcp_gpu_tensor_destroy(state->radii);
    if (state->collision_flags) nimcp_gpu_tensor_destroy(state->collision_flags);
    if (state->collision_pairs) nimcp_gpu_tensor_destroy(state->collision_pairs);
    if (state->pair_count) nimcp_gpu_tensor_destroy(state->pair_count);
    if (state->spatial_hash) nimcp_spatial_hash_destroy(state->spatial_hash);

    nimcp_free(state);
}

extern "C" bool nimcp_gpu_collision_detect(
    nimcp_gpu_context_t* ctx,
    nimcp_collision_gpu_state_t* state)
{
    // Initialize GPU recovery if not already done
    if (!nimcp_gpu_recovery_is_initialized()) {
        nimcp_gpu_recovery_init(NULL);
    }

    if (!ctx || !state) return false;

    // Reset pair count
    nimcp_gpu_zeros(ctx, state->pair_count);
    nimcp_gpu_zeros(ctx, state->collision_flags);

    kernel_collision_detect_brute<<<GRID_SIZE(state->n_agents), BLOCK_SIZE>>>(
        (const float4*)state->positions->data,
        state->params.use_variable_radius ? (const float*)state->radii->data : NULL,
        (float*)state->collision_flags->data,
        (uint32_t*)state->collision_pairs->data,
        (uint32_t*)state->pair_count->data,
        state->n_agents,
        state->max_pairs,
        state->params.collision_radius,
        state->params.use_variable_radius);

    NIMCP_CUDA_RECOVER_LAST(GPU_ERROR_KERNEL_LAUNCH);
    return true;
}

extern "C" bool nimcp_gpu_pairwise_distances(
    nimcp_gpu_context_t* ctx,
    const nimcp_gpu_tensor_t* positions,
    nimcp_gpu_tensor_t* distances,
    size_t n_agents)
{
    // Initialize GPU recovery if not already done
    if (!nimcp_gpu_recovery_is_initialized()) {
        nimcp_gpu_recovery_init(NULL);
    }

    if (!ctx || !positions || !distances) return false;

    size_t total = n_agents * n_agents;

    kernel_pairwise_distances<<<GRID_SIZE(total), BLOCK_SIZE>>>(
        (const float4*)positions->data,
        (float*)distances->data,
        n_agents);

    NIMCP_CUDA_RECOVER_LAST(GPU_ERROR_KERNEL_LAUNCH);
    return true;
}

extern "C" bool nimcp_gpu_collision_get_pairs(
    nimcp_gpu_context_t* ctx,
    const nimcp_collision_gpu_state_t* state,
    uint32_t* pairs_out,
    size_t max_pairs,
    size_t* count_out)
{
    // Initialize GPU recovery if not already done
    if (!nimcp_gpu_recovery_is_initialized()) {
        nimcp_gpu_recovery_init(NULL);
    }

    if (!ctx || !state || !pairs_out || !count_out) return false;

    uint32_t h_count;
    NIMCP_CUDA_RECOVER(cudaMemcpy(&h_count, state->pair_count->data, sizeof(uint32_t), cudaMemcpyDeviceToHost), GPU_ERROR_CUDA_RUNTIME);

    size_t copy_count = (h_count < max_pairs) ? h_count : max_pairs;
    *count_out = copy_count;

    if (copy_count > 0) {
        NIMCP_CUDA_RECOVER(cudaMemcpy(pairs_out, state->collision_pairs->data,
                              copy_count * 2 * sizeof(uint32_t), cudaMemcpyDeviceToHost), GPU_ERROR_CUDA_RUNTIME);
    }

    return true;
}

#else // NIMCP_ENABLE_CUDA

// Stub implementations when CUDA is not enabled
#include "gpu/swarm/nimcp_swarm_gpu.h"
#include "utils/logging/nimcp_logging.h"
#include <stdlib.h>

#define LOG_MODULE "SWARM_GPU"

void nimcp_flocking_gpu_default_params(nimcp_flocking_gpu_params_t* params) {
    if (params) memset(params, 0, sizeof(*params));
}

void nimcp_consensus_gpu_default_params(nimcp_consensus_gpu_params_t* params) {
    if (params) memset(params, 0, sizeof(*params));
}

void nimcp_pheromone_gpu_default_params(nimcp_pheromone_gpu_params_t* params) {
    if (params) memset(params, 0, sizeof(*params));
}

void nimcp_quorum_gpu_default_params(nimcp_quorum_gpu_params_t* params) {
    if (params) memset(params, 0, sizeof(*params));
}

void nimcp_task_alloc_gpu_default_params(nimcp_task_alloc_gpu_params_t* params) {
    if (params) memset(params, 0, sizeof(*params));
}

void nimcp_collision_gpu_default_params(nimcp_collision_gpu_params_t* params) {
    if (params) memset(params, 0, sizeof(*params));
}

nimcp_flocking_gpu_state_t* nimcp_flocking_gpu_create(
    nimcp_gpu_context_t* ctx, size_t n_agents, size_t max_neighbors,
    const nimcp_flocking_gpu_params_t* params) {
    LOG_ERROR("CUDA not enabled"); return NULL;
}

void nimcp_flocking_gpu_destroy(nimcp_flocking_gpu_state_t* state) {}
bool nimcp_gpu_flocking_compute_forces(nimcp_gpu_context_t* ctx, nimcp_flocking_gpu_state_t* state) { return false; }
bool nimcp_gpu_flocking_update(nimcp_gpu_context_t* ctx, nimcp_flocking_gpu_state_t* state, float dt) { return false; }
bool nimcp_gpu_flocking_separation(nimcp_gpu_context_t* ctx, nimcp_flocking_gpu_state_t* state) { return false; }
bool nimcp_gpu_flocking_alignment(nimcp_gpu_context_t* ctx, nimcp_flocking_gpu_state_t* state) { return false; }
bool nimcp_gpu_flocking_cohesion(nimcp_gpu_context_t* ctx, nimcp_flocking_gpu_state_t* state) { return false; }
bool nimcp_gpu_flocking_find_neighbors(nimcp_gpu_context_t* ctx, nimcp_flocking_gpu_state_t* state, float radius) { return false; }

nimcp_spatial_hash_t* nimcp_spatial_hash_create(nimcp_gpu_context_t* ctx, float cell_size,
    uint32_t grid_dim_x, uint32_t grid_dim_y, uint32_t grid_dim_z, size_t max_particles) { return NULL; }
void nimcp_spatial_hash_destroy(nimcp_spatial_hash_t* hash) {}
bool nimcp_spatial_hash_clear(nimcp_gpu_context_t* ctx, nimcp_spatial_hash_t* hash) { return false; }
bool nimcp_gpu_spatial_hash_build(nimcp_gpu_context_t* ctx, nimcp_spatial_hash_t* hash,
    const nimcp_gpu_tensor_t* positions, size_t n_agents) { return false; }

nimcp_consensus_gpu_state_t* nimcp_consensus_gpu_create(nimcp_gpu_context_t* ctx, size_t n_agents,
    size_t belief_dim, const nimcp_consensus_gpu_params_t* params) { return NULL; }
void nimcp_consensus_gpu_destroy(nimcp_consensus_gpu_state_t* state) {}
bool nimcp_gpu_consensus_averaging(nimcp_gpu_context_t* ctx, nimcp_consensus_gpu_state_t* state) { return false; }
bool nimcp_gpu_consensus_belief_propagation(nimcp_gpu_context_t* ctx, nimcp_consensus_gpu_state_t* state) { return false; }
bool nimcp_gpu_consensus_opinion_dynamics(nimcp_gpu_context_t* ctx, nimcp_consensus_gpu_state_t* state) { return false; }
bool nimcp_gpu_consensus_check_convergence(nimcp_gpu_context_t* ctx, const nimcp_consensus_gpu_state_t* state,
    bool* converged, float* variance) { return false; }

nimcp_pheromone_gpu_state_t* nimcp_pheromone_gpu_create(nimcp_gpu_context_t* ctx, uint32_t grid_x,
    uint32_t grid_y, uint32_t grid_z, uint32_t n_types, float voxel_size,
    const nimcp_pheromone_gpu_params_t* params) { return NULL; }
void nimcp_pheromone_gpu_destroy(nimcp_pheromone_gpu_state_t* state) {}
bool nimcp_gpu_pheromone_diffusion(nimcp_gpu_context_t* ctx, nimcp_pheromone_gpu_state_t* state, float dt) { return false; }
bool nimcp_gpu_pheromone_decay(nimcp_gpu_context_t* ctx, nimcp_pheromone_gpu_state_t* state, float dt) { return false; }
bool nimcp_gpu_pheromone_deposit(nimcp_gpu_context_t* ctx, nimcp_pheromone_gpu_state_t* state,
    const nimcp_gpu_tensor_t* positions, const nimcp_gpu_tensor_t* types,
    const nimcp_gpu_tensor_t* amounts, size_t n_deposits) { return false; }
bool nimcp_gpu_pheromone_sample(nimcp_gpu_context_t* ctx, const nimcp_pheromone_gpu_state_t* state,
    const nimcp_gpu_tensor_t* positions, uint32_t type, nimcp_gpu_tensor_t* output) { return false; }
bool nimcp_gpu_pheromone_gradient(nimcp_gpu_context_t* ctx, const nimcp_pheromone_gpu_state_t* state,
    const nimcp_gpu_tensor_t* positions, uint32_t type, nimcp_gpu_tensor_t* gradients) { return false; }

nimcp_quorum_gpu_state_t* nimcp_quorum_gpu_create(nimcp_gpu_context_t* ctx, size_t n_agents,
    size_t n_signal_types, const nimcp_quorum_gpu_params_t* params) { return NULL; }
void nimcp_quorum_gpu_destroy(nimcp_quorum_gpu_state_t* state) {}
bool nimcp_gpu_quorum_compute_concentration(nimcp_gpu_context_t* ctx, nimcp_quorum_gpu_state_t* state) { return false; }
bool nimcp_gpu_quorum_check_thresholds(nimcp_gpu_context_t* ctx, nimcp_quorum_gpu_state_t* state) { return false; }
bool nimcp_gpu_quorum_update_commitments(nimcp_gpu_context_t* ctx, nimcp_quorum_gpu_state_t* state) { return false; }
bool nimcp_gpu_quorum_decay_signals(nimcp_gpu_context_t* ctx, nimcp_quorum_gpu_state_t* state, float dt) { return false; }

nimcp_task_alloc_gpu_state_t* nimcp_task_alloc_gpu_create(nimcp_gpu_context_t* ctx, size_t n_agents,
    size_t n_tasks, size_t n_capability_types, const nimcp_task_alloc_gpu_params_t* params) { return NULL; }
void nimcp_task_alloc_gpu_destroy(nimcp_task_alloc_gpu_state_t* state) {}
bool nimcp_gpu_task_compute_matches(nimcp_gpu_context_t* ctx, nimcp_task_alloc_gpu_state_t* state) { return false; }
bool nimcp_gpu_task_auction_round(nimcp_gpu_context_t* ctx, nimcp_task_alloc_gpu_state_t* state) { return false; }
bool nimcp_gpu_task_update_prices(nimcp_gpu_context_t* ctx, nimcp_task_alloc_gpu_state_t* state) { return false; }
bool nimcp_gpu_task_finalize_assignments(nimcp_gpu_context_t* ctx, nimcp_task_alloc_gpu_state_t* state) { return false; }

nimcp_collision_gpu_state_t* nimcp_collision_gpu_create(nimcp_gpu_context_t* ctx, size_t n_agents,
    size_t max_pairs, const nimcp_collision_gpu_params_t* params) { return NULL; }
void nimcp_collision_gpu_destroy(nimcp_collision_gpu_state_t* state) {}
bool nimcp_gpu_collision_detect(nimcp_gpu_context_t* ctx, nimcp_collision_gpu_state_t* state) { return false; }
bool nimcp_gpu_pairwise_distances(nimcp_gpu_context_t* ctx, const nimcp_gpu_tensor_t* positions,
    nimcp_gpu_tensor_t* distances, size_t n_agents) { return false; }
bool nimcp_gpu_collision_get_pairs(nimcp_gpu_context_t* ctx, const nimcp_collision_gpu_state_t* state,
    uint32_t* pairs_out, size_t max_pairs, size_t* count_out) { return false; }

#endif // NIMCP_ENABLE_CUDA
