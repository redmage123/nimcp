# Brain Regions Roadmap

**Last Updated**: 2026-03-11

## Implementation Status

NIMCP implements 40+ brain regions across cortical, subcortical, and specialized systems. All regions below are present as directories under `src/core/brain/regions/`.

### Cortical Regions (All Implemented)

| Region | Directory | Function |
|--------|-----------|----------|
| Broca's Area | `broca/` | Language production, syntax, phonology |
| Wernicke's Area | `wernicke/` | Speech comprehension |
| Occipital Cortex | `occipital/` | Visual processing (V1-V5), CNN feature extraction, audiovisual bridge |
| Temporal Lobe | `temporal/` | Memory, auditory processing |
| Motor Cortex | `motor/` | Movement control |
| Prefrontal Cortex | `prefrontal/` | Executive function, decision making |
| Parietal Cortex | `parietal/` | Spatial processing, FEP active inference, mental rotation |
| Insula | `insula/` | Interoception, emotion-body interface |
| Cingulate Cortex | `cingulate/` | Error detection, conflict monitoring |
| Somatosensory Cortex | `somatosensory/` | Touch, proprioception, body map |
| Orbitofrontal Cortex (OFC) | `ofc/` | Value representation, reversal learning |
| Retrosplenial Cortex | `retrosplenial/` | Spatial memory, viewpoint transformation |
| Gustatory Cortex | `gustatory/` | Taste processing |
| Olfactory Cortex | `olfactory/` | Smell processing |
| Sensory Integration | `sensory_integration/` | Cross-modal sensory binding |

### Subcortical Regions (All Implemented)

| Region | Directory | Function |
|--------|-----------|----------|
| Hippocampus | `hippocampus/` | Memory consolidation, neurogenesis simulation |
| Cerebellum | `cerebellum/` | Motor coordination, timing, error prediction |
| Hypothalamus | `hypothalamus/` | Homeostasis, drives, FEP bridges |
| Brainstem | `brainstem/` | Vital functions, arousal |
| Amygdala | `amygdala/` | Emotional processing, fear, reward |
| Thalamus | (via thalamic routing) | Attention gating, sensory relay |
| Basal Ganglia | (integrated in training) | Reward learning, habit formation, action selection |

### Neuromodulatory Centers (All Implemented)

| Region | Directory | Function |
|--------|-----------|----------|
| Locus Coeruleus | `locus_coeruleus/` | Norepinephrine, arousal, exploration-exploitation |
| VTA | `vta/` | Dopamine, reward prediction error |
| Raphe Nuclei | `raphe/` | Serotonin, mood, patience |
| Habenula | `habenula/` | Aversive learning, disappointment signaling |

### Memory Circuit (All Implemented)

| Region | Directory | Function |
|--------|-----------|----------|
| Entorhinal Cortex | `entorhinal/` | Grid cells, hippocampal gateway |
| Perirhinal Cortex | `perirhinal/` | Object recognition, familiarity |
| Parahippocampal Cortex | `parahippocampal/` | Scene/context processing |
| Mammillary Bodies | `mammillary/` | Memory consolidation relay (Papez circuit) |

### Specialized Regions (All Implemented)

| Region | Directory | Function |
|--------|-----------|----------|
| Claustrum | `claustrum/` | Cross-modal binding, salience detection |
| PAG | `pag/` | Pain modulation, fight/flight/freeze |
| Red Nucleus | `red_nucleus/` | Motor refinement |
| Reticular Formation | `reticular/` | Arousal, sleep-wake states |
| Neuropeptide System | `neuropeptide/` | Peptide signaling |
| Endocannabinoid System | `endocannabinoid/` | Retrograde signaling |
| Glymphatic System | `glymphatic/` | Waste clearance (sleep-dependent) |

## Integration Architecture

All regions integrate via:
- **Thalamic routing**: ~78 thalamic bridge files for sensory relay and attention gating
- **Bio-async messaging**: Asynchronous inter-region communication
- **SNN bridges**: 42 spiking neural network bridges connecting regions to cognitive modules
- **Cortical columns**: Minicolumns, hypercolumns, 6-layer laminar, sparse coding, K-WTA
- **Neuromodulatory bridges**: 11+ inter-module bridges for DA/5-HT/ACh/NE modulation

## Architecture Requirements for New Regions

Each region should include:
- Standard adapter interface (`nimcp_*_adapter.h/c`)
- Substrate bridge for bio-async messaging
- Training bridge for learning algorithms
- FEP bridge where applicable
- Unit tests with comprehensive coverage
- Integration with brain factory initialization
- Bridges subdirectory for region-specific cross-module connections
