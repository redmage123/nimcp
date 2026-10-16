# NIMCP Test-to-Source Mapping
Generated: Sat Nov 15 03:59:47 AM CET 2025

## Test Files and Their Targets

### test/e2e/test_visual_cortex_e2e.cpp
```
#include "include/perception/nimcp_visual_cortex.h"
#include "utils/memory/nimcp_memory.h"
```

### test/integration/cognitive/consolidation/test_systems_consolidation_integration.cpp
```
#include "core/brain/nimcp_brain.h"
#include "cognitive/memory/nimcp_systems_consolidation.h"
#include "cognitive/memory/nimcp_engram.h"
#include "utils/platform/nimcp_platform_time.h"
```

### test/integration/cognitive/emotions/test_emotional_system_integration.cpp
```
#include "core/brain/nimcp_brain.h"
#include "cognitive/nimcp_emotional_system.h"
#include "cognitive/nimcp_emotional_tagging.h"
```

### test/integration/cognitive/global_workspace/test_global_workspace_integration.cpp
```
#include "cognitive/global_workspace/nimcp_global_workspace.h"
#include "cognitive/nimcp_working_memory.h"
#include "cognitive/nimcp_executive.h"
#include "utils/memory/nimcp_memory.h"
#include "utils/time/nimcp_time.h"
```

### test/integration/cognitive/grief/test_grief_and_loss_integration.cpp
```
#include "cognitive/nimcp_grief_and_loss.h"
#include "plasticity/neuromodulators/nimcp_neuromodulators.h"
// #include "cognitive/nimcp_wellbeing.h"  // TODO: Add when wellbeing system is available
```

### test/integration/cognitive/joy/test_joy_euphoria_integration.cpp
```
#include "cognitive/nimcp_joy_euphoria.h"
#include "plasticity/neuromodulators/nimcp_neuromodulators.h"
```

### test/integration/cognitive/logic/test_logic_integration.cpp
```
#include "core/brain/nimcp_brain.h"
#include "core/neuron_types/nimcp_neural_logic.h"
#include "cognitive/nimcp_symbolic_logic.h"
#include "cognitive/knowledge/nimcp_knowledge.h"
```

### test/integration/cognitive/memory/test_engram_integration.cpp
```
#include "cognitive/memory/nimcp_engram.h"
#include "cognitive/nimcp_sleep_wake.h"
#include "cognitive/memory/nimcp_systems_consolidation.h"
#include "plasticity/neuromodulators/nimcp_neuromodulators.h"
#include "core/nimcp_types.h"
```

### test/integration/cognitive/memory/test_semantic_memory_integration.cpp
```
#include "core/brain/nimcp_brain.h"
```

### test/integration/core/brain_regions/test_visual_cortex_integration.cpp
```
#include "include/perception/nimcp_visual_cortex.h"
#include "utils/memory/nimcp_memory.h"
// #include "include/nimcp_curiosity.h"
// #include "include/nimcp_attention.h"
// #include "include/nimcp_knowledge.h"
```

### test/integration/core/brain/test_brain_integration.cpp
```
#include "core/brain/nimcp_brain.h"
#include "networking/distributed/nimcp_distributed_cognition.h"
#include "plasticity/neuromodulators/nimcp_neuromodulators.h"
#include "utils/memory/nimcp_memory.h"
#include "utils/time/nimcp_time.h"
```

### test/integration/core/brain/test_brain_integration_rk4.cpp
```
#include "core/brain/nimcp_brain.h"
#include "core/neuron_models/nimcp_neuron_model.h"
#include "utils/memory/nimcp_memory.h"
#include "utils/time/nimcp_time.h"
```

### test/integration/core/brain/test_comprehensive_brain_integration.cpp
```
#include "core/brain/nimcp_brain.h"
#include "core/neuralnet/nimcp_neuralnet.h"
#include "core/neuron_types/nimcp_neuron_types.h"
#include "core/brain_regions/nimcp_brain_regions.h"
#include "cognitive/ethics/nimcp_ethics.h"
```

### test/integration/core/integration/test_integration_cognitive.cpp
```
#include "test_helpers.h"
#include "core/brain/nimcp_brain.h"
#include "cognitive/consolidation/nimcp_consolidation.h"
#include "cognitive/curiosity/nimcp_curiosity.h"
#include "cognitive/ethics/nimcp_ethics.h"
```

### test/integration/core/integration/test_integration_e2e.cpp
```
#include "test_helpers.h"
#include "plasticity/adaptive/nimcp_adaptive.h"
#include "networking/events/nimcp_events.h"
#include "core/neuralnet/nimcp_neuralnet.h"
#include "networking/protocol/nimcp_protocol.h"
```

### test/integration/core/integration/test_integration_networking.cpp
```
#include "test_helpers.h"
#include "core/brain/nimcp_brain.h"
#include "core/neuralnet/nimcp_neuralnet.h"
#include "networking/p2p/nimcp_p2pnode.h"
#include "networking/protocol/nimcp_protocol.h"
```

### test/integration/core/integration/test_love_loyalty_friendship_integration.cpp
```
#include "cognitive/nimcp_love_loyalty_friendship.h"
#include "cognitive/nimcp_emotional_tagging.h"
```

### test/integration/core/integration/test_pink_noise_integration.cpp
```
#include "plasticity/noise/nimcp_pink_noise.h"
```

### test/integration/core/integration/test_vesicle_packaging_integration.cpp
```
#include "plasticity/neuromodulators/nimcp_vesicle_packaging.h"
#include "utils/memory/nimcp_memory.h"
```

### test/integration/core/integration/test_wm_transfer_integration.cpp
```
#include "core/brain/nimcp_brain.h"
#include "cognitive/memory/nimcp_wm_transfer.h"
#include "cognitive/memory/nimcp_engram.h"
#include "utils/platform/nimcp_platform_time.h"
```

### test/integration/core/topology/topology_integration_tests.cpp
```
#include "core/topology/nimcp_fractal_topology.h"
#include "core/neuralnet/nimcp_neuralnet.h"
```

### test/integration/glial/test_glial_integration.cpp
```
#include "glial/integration/nimcp_glial_integration.h"
#include "core/neuralnet/nimcp_neuralnet.h"
#include "glial/astrocytes/nimcp_astrocytes.h"
#include "glial/oligodendrocytes/nimcp_oligodendrocytes.h"
#include "glial/microglia/nimcp_microglia.h"
```

### test/integration/gpu/test_gpu_execution_mode_integration.cpp
```
#include "core/brain/nimcp_brain.h"
#include "gpu/nimcp_execution_mode.h"
```

### test/integration/gpu/test_gpu_neuron_integration.cpp
```
#include "core/brain/nimcp_brain.h"
#include "gpu/nimcp_gpu_neuron.h"
```

### test/integration/gpu/test_gpu_spike_event_integration.cpp
```
#include "core/brain/nimcp_brain.h"
#include "gpu/nimcp_spike_event.h"
```

### test/integration/information/test_quantum_shannon_integration.cpp
```
#include "utils/quantum/nimcp_quantum_shannon.h"
#include "core/brain/nimcp_brain.h"
#include "plasticity/adaptive/nimcp_adaptive.h"
```

### test/integration/information/test_shannon_integration.cpp
```
#include "information/nimcp_shannon.h"
#include "core/brain/nimcp_brain.h"
```

### test/integration/io/test_metabolic_pathways_integration.cpp
```
