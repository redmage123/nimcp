/**
 * @file nimcp_lnn_gradient.c
 * @brief Implementation of adjoint method for LNN gradient computation
 *
 * WHAT: Memory-efficient gradient computation for continuous-time LNNs
 * WHY:  Enable training without storing full trajectory
 * HOW:  Adjoint ODE integration backward in time
 *
 * @author NIMCP Development Team
 * @date 2025-12-20
 */

#include "lnn/nimcp_lnn_gradient.h"
#include "lnn/nimcp_lnn_types.h"
#include "lnn/nimcp_lnn_layer.h"
#include "lnn/nimcp_lnn_neuron.h"
#include "lnn/nimcp_lnn_network.h"
#include "lnn/nimcp_lnn_ode.h"
#include "gpu/lnn/nimcp_lnn_gpu.h"
#include "utils/memory/nimcp_memory.h"
#include "utils/thread/nimcp_thread.h"
#include "utils/thread/nimcp_thread_pool.h"
#include "middleware/training/nimcp_optimizers.h"
#include "utils/logging/nimcp_logging.h"
#include "utils/exception/nimcp_exception_macros.h"
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <float.h>
#include "utils/geometry/nimcp_differential_geometry.h"
#include "utils/fault_tolerance/nimcp_health_agent_macros.h"

NIMCP_DECLARE_HEALTH_AGENT_ATOMIC(lnn_gradient)

/*=============================================================================
 * Constants
 *===========================================================================*/

#define DEFAULT_CHECKPOINT_INTERVAL 100
#define GRADIENT_HEALTH_THRESHOLD 1e6f
#define JACOBIAN_EPSILON 1e-5f

/*=============================================================================
 * Helper Function Declarations
 *===========================================================================*/

static int allocate_gradient_storage(lnn_gradient_ctx_t* ctx, lnn_network_t* network);
static int allocate_checkpoints(lnn_gradient_ctx_t* ctx, uint32_t max_steps);
static void free_gradient_storage(lnn_gradient_ctx_t* ctx);
static void free_checkpoints(lnn_gradient_ctx_t* ctx);
static int compute_jacobian_numerical(const lnn_layer_t* layer, const nimcp_tensor_t* x, nimcp_tensor_t* jacobian);
static int accumulate_parameter_gradients(lnn_gradient_ctx_t* ctx, lnn_network_t* network, const nimcp_tensor_t* adjoint, float dt);
static bool check_tensor_health(const nimcp_tensor_t* t);
static double lnn_gradient_get_time_ms(void);


//=============================================================================
// SRP Split: Function implementations organized by responsibility
//=============================================================================
#include "nimcp_lnn_gradient_part_lifecycle.c"  // 3 functions: lifecycle
#include "nimcp_lnn_gradient_part_core.c"  // 11 functions: core
#include "nimcp_lnn_gradient_part_accessors.c"  // 3 functions: accessors
#include "nimcp_lnn_gradient_part_io.c"  // 2 functions: io
#include "nimcp_lnn_gradient_part_helpers.c"  // 7 functions: helpers
