/**
 * nimcp_brain_init_cognitive_training.c — Initialize cognitive training subsystems
 *
 * WHAT: Creates JEPA predictor, predictive hierarchy, and self-heal engine
 * WHY:  Required for C5/C6 inference-time training pipeline
 * HOW:  Creates each subsystem with default configs, sets enabled flags
 *
 * @version 1.0.0
 */

#include "core/brain/nimcp_brain_internal.h"
#include "cognitive/jepa/nimcp_jepa_predictor.h"
#include "cognitive/predictive/nimcp_predictive_hierarchy.h"
#include "cognitive/immune/nimcp_self_heal.h"
#include "utils/logging/nimcp_logging.h"
#include "utils/memory/nimcp_memory.h"

#define LOG_MODULE "brain_init_cognitive_training"

bool nimcp_brain_factory_init_cognitive_training_subsystem(brain_t brain) {
    if (!brain) return false;

    /* 1. JEPA Predictor — latent space prediction for imagination */
    if (!brain->jepa_predictor) {
        jepa_predictor_config_t jepa_cfg;
        jepa_predictor_default_config(&jepa_cfg);
        jepa_cfg.input_dim = 256;
        jepa_cfg.output_dim = 256;
        jepa_cfg.num_layers = 2;
        jepa_cfg.hidden_dim = 128;
        jepa_cfg.learning_rate = 0.001f;
        jepa_cfg.enable_fep = true;

        brain->jepa_predictor = (struct jepa_predictor*)jepa_predictor_create(&jepa_cfg);
        if (brain->jepa_predictor) {
            brain->jepa_predictor_enabled = true;
            LOG_INFO(LOG_MODULE, "JEPA predictor initialized (256-dim, 2 layers)");
        } else {
            LOG_WARN(LOG_MODULE, "JEPA predictor creation failed (non-fatal)");
        }
    }

    /* 2. Predictive Hierarchy — hierarchical temporal prediction */
    if (!brain->pred_hierarchy) {
        pred_level_config_t levels[3] = {
            { .dim = 256, .gen_hidden_dim = 128, .gen_type = 0,
              .initial_precision = 1.0f, .precision_lr = 0.01f, .learnable_precision = true },
            { .dim = 128, .gen_hidden_dim = 64, .gen_type = 0,
              .initial_precision = 1.0f, .precision_lr = 0.01f, .learnable_precision = true },
            { .dim = 64, .gen_hidden_dim = 32, .gen_type = 0,
              .initial_precision = 1.0f, .precision_lr = 0.01f, .learnable_precision = true }
        };
        pred_hier_config_t hier_cfg = {
            .num_levels = 3,
            .level_configs = levels,
            .update_mode = 0,
            .state_update_rate = 0.1f,
            .weight_lr = 0.001f,
            .precision_lr = 0.01f,
            .enable_learning = true,
            .enable_lateral = false,
            .enable_fep = true,
            .complexity_weight = 0.01f,
            .gpu_mode = 0,
            .enable_bio_async = false
        };

        brain->pred_hierarchy = (struct predictive_hierarchy*)pred_hier_create(&hier_cfg);
        if (brain->pred_hierarchy) {
            brain->pred_hierarchy_enabled = true;
            LOG_INFO(LOG_MODULE, "Predictive hierarchy initialized (3 levels: 256->128->64)");
        } else {
            LOG_WARN(LOG_MODULE, "Predictive hierarchy creation failed (non-fatal)");
        }
    }

    /* 3. Self-Heal Engine — learn from training/inference outcomes */
    if (!brain->self_heal_engine) {
        brain->self_heal_engine = (void*)self_heal_create(NULL);
        if (brain->self_heal_engine) {
            brain->self_heal_enabled = true;
            LOG_INFO(LOG_MODULE, "Self-heal engine initialized");
        } else {
            LOG_WARN(LOG_MODULE, "Self-heal engine creation failed (non-fatal)");
        }
    }

    /* Set default cognitive training interval if not already set */
    if (brain->cognitive_train_interval == 0) {
        brain->cognitive_train_interval = 5;
    }

    return true;  /* Non-fatal if individual subsystems fail */
}
