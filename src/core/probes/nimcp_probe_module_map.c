/**
 * @file nimcp_probe_module_map.c
 * @brief Maps bio_module_id_t → void* pointer in brain_struct
 *
 * Switch-table implementation: compile-time constants → jump table.
 * Returns NULL for modules that aren't initialized.
 */

#include "core/probes/nimcp_brain_probes.h"
#include "core/brain/nimcp_brain_internal.h"
#include "async/nimcp_bio_messages.h"

void* probe_resolve_module(brain_t brain, uint16_t module_id) {
    if (!brain) return NULL;

    switch (module_id) {
        /* Core brain — return the brain struct itself */
        case 0x0100: return brain;  /* BIO_MODULE_BRAIN */

        /* Cognitive modules */
        case 0x0210: return brain->multihead_attention;     /* ATTENTION */
        case 0x0220: return brain->curiosity;               /* CURIOSITY */
        case 0x0230: return brain->engram_system;           /* ENGRAM */
        case 0x0240: return brain->semantic_memory;         /* SEMANTIC_MEMORY */
        case 0x0250: return brain->working_memory;          /* WORKING_MEMORY */
        case 0x0260: return brain->executive;               /* EXECUTIVE */
        case 0x0270: return brain->theory_of_mind;          /* THEORY_OF_MIND */
        case 0x0280: return brain->mirror_neurons;          /* MIRROR_NEURONS */
        case 0x0290: return brain->global_workspace;        /* GLOBAL_WORKSPACE */

        /* Emotion & wellbeing */
        case 0x0300: return brain->emotional_learning;      /* EMOTION */
        case 0x0310: return brain->introspection;           /* INTROSPECTION */

        /* Glial & astrocytes */
        case 0x0350: return brain->glial;                   /* GLIAL */

        /* Plasticity */
        case 0x0400: return brain->plasticity_coordinator;  /* PLASTICITY */
        case 0x0410: return brain->event_driven_plasticity; /* EDP */
        case 0x0420: return brain->structural_plasticity;   /* STRUCTURAL */
        case 0x0430: return brain->neuromodulator_system;   /* NEUROMOD */

        /* Networks */
        case 0x0500: return brain->network;                 /* ADAPTIVE */
        case 0x0510: return brain->snn_network;             /* SNN */
        case 0x0520: return brain->lnn_network;             /* LNN */
        case 0x0530: return brain->cnn_trainer;             /* CNN */

        /* Training */
        case 0x0600: return brain->unified_training;        /* UTM */

        /* Language */
        case 0x0700: return brain->grounded_lang;           /* LANGUAGE */
        case 0x0710: return brain->language_layer;          /* LANGUAGE_LAYER */
        case 0x0720: return brain->native_language;         /* NATIVE_LANGUAGE */

        /* Reasoning */
        case 0x0800: return brain->reasoning_engine;        /* REASONING */
        case 0x0810: return brain->symbolic_logic;          /* SYMBOLIC_LOGIC */

        /* Memory systems */
        case 0x0900: return brain->systems_consolidation;   /* CONSOLIDATION */
        case 0x0910: return brain->sleep_system;            /* SLEEP */
        case 0x0920: return brain->multiscale_memory;       /* MULTISCALE_MEM */
        case 0x0930: return brain->episodic_replay;         /* EPISODIC_REPLAY */

        /* Prediction & world model */
        case 0x0A00: return brain->predictive_network;      /* PREDICTIVE */
        case 0x0A10: return brain->pred_hierarchy;          /* PRED_HIERARCHY */
        case 0x0A20: return brain->jepa_predictor;          /* JEPA */
        case 0x0A30: return brain->world_model_trainer;     /* WORLD_MODEL */
        case 0x0A40: return brain->world_prior;             /* WORLD_PRIOR */
        case 0x0A50: return brain->fep_orchestrator;        /* FEP */

        /* Creative & imagination */
        case 0x0B00: return brain->imagination;             /* IMAGINATION */
        case 0x0B10: return brain->creative_orchestrator;   /* CREATIVE */

        /* Ethics & safety */
        case 0x0C00: return brain->ethics;                  /* ETHICS */
        case 0x0C10: return brain->lgss;                    /* LGSS */

        /* Edge & robotics */
        case 0x0D00: return brain->sensor_hub;              /* SENSOR_HUB */
        case 0x0D10: return brain->safety_watchdog;         /* WATCHDOG */

        /* Cortex CNNs */
        case 0x0E00: return brain->cortex_cnns[0];          /* VISUAL_CORTEX */
        case 0x0E10: return brain->cortex_cnns[1];          /* AUDIO_CORTEX */
        case 0x0E20: return brain->cortex_cnns[2];          /* SPEECH_CORTEX */
        case 0x0E30: return brain->cortex_cnns[3];          /* SOMATO_CORTEX */

        default: return NULL;
    }
}

const char* probe_module_name(uint16_t module_id) {
    switch (module_id) {
        case 0x0100: return "brain";
        case 0x0210: return "attention";
        case 0x0220: return "curiosity";
        case 0x0230: return "engram";
        case 0x0240: return "semantic_mem";
        case 0x0250: return "working_mem";
        case 0x0260: return "executive";
        case 0x0270: return "tom";
        case 0x0280: return "mirror";
        case 0x0290: return "gw";
        case 0x0300: return "emotion";
        case 0x0310: return "introspection";
        case 0x0350: return "glial";
        case 0x0400: return "plasticity";
        case 0x0410: return "edp";
        case 0x0420: return "structural";
        case 0x0430: return "neuromod";
        case 0x0500: return "adaptive";
        case 0x0510: return "snn";
        case 0x0520: return "lnn";
        case 0x0530: return "cnn";
        case 0x0600: return "utm";
        case 0x0700: return "language";
        case 0x0710: return "lang_layer";
        case 0x0720: return "native_lang";
        case 0x0800: return "reasoning";
        case 0x0810: return "symbolic";
        case 0x0900: return "consolidation";
        case 0x0910: return "sleep";
        case 0x0920: return "multiscale";
        case 0x0930: return "replay";
        case 0x0A00: return "predictive";
        case 0x0A10: return "pred_hier";
        case 0x0A20: return "jepa";
        case 0x0A30: return "world_model";
        case 0x0A40: return "world_prior";
        case 0x0A50: return "fep";
        case 0x0B00: return "imagination";
        case 0x0B10: return "creative";
        case 0x0C00: return "ethics";
        case 0x0C10: return "lgss";
        case 0x0D00: return "sensor_hub";
        case 0x0D10: return "watchdog";
        case 0x0E00: return "visual_cortex";
        case 0x0E10: return "audio_cortex";
        case 0x0E20: return "speech_cortex";
        case 0x0E30: return "somato_cortex";
        default: return NULL;
    }
}
