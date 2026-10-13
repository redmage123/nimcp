# Core library sources - New hierarchical structure
set(NIMCP_CORE_SOURCES
    # Public API facade (unified interface for all bindings)
    ${CMAKE_CURRENT_SOURCE_DIR}/../api/nimcp.c

    # Core brain components
    ${CMAKE_CURRENT_SOURCE_DIR}/../core/brain/nimcp_brain.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../core/neuralnet/nimcp_neuralnet.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../core/neuralnet/nimcp_neuralnet_simd.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../core/neuron_types/nimcp_neuron_types.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../core/brain_regions/nimcp_brain_regions.c

    # Cognitive functions
    ${CMAKE_CURRENT_SOURCE_DIR}/../cognitive/ethics/nimcp_ethics.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../cognitive/knowledge/nimcp_knowledge.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../cognitive/curiosity/nimcp_curiosity.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../cognitive/introspection/nimcp_introspection.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../cognitive/wellbeing/nimcp_wellbeing.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../cognitive/consolidation/nimcp_consolidation.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../cognitive/salience/nimcp_salience.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../cognitive/logic/nimcp_symbolic_logic.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../cognitive/checkpointing/nimcp_checkpointing.c
    ${CMAKE_CURRENT_SOURCE_DIR}/cognitive/nimcp_hierarchical.c

    # Plasticity mechanisms
    ${CMAKE_CURRENT_SOURCE_DIR}/../plasticity/adaptive/nimcp_adaptive.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../plasticity/adaptive/nimcp_weights.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../plasticity/bcm/nimcp_bcm.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../plasticity/neuromodulators/nimcp_neuromodulators.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../plasticity/attention/nimcp_attention.c

    # Glial cells
    ${CMAKE_CURRENT_SOURCE_DIR}/../glial/astrocytes/nimcp_astrocytes.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../glial/oligodendrocytes/nimcp_oligodendrocytes.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../glial/microglia/nimcp_microglia.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../glial/integration/nimcp_glial_integration.c

    # Networking
    ${CMAKE_CURRENT_SOURCE_DIR}/../networking/p2p/nimcp_p2pnode.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../networking/protocol/nimcp_protocol.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../networking/events/nimcp_events.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../networking/replication/nimcp_replication.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../networking/distributed/nimcp_distributed_cognition.c

    # I/O
    ${CMAKE_CURRENT_SOURCE_DIR}/../io/dataio/nimcp_dataio.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../io/stream/nimcp_stream.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../io/serialization/nimcp_serialization.c

    # Security
    ${CMAKE_CURRENT_SOURCE_DIR}/../security/nimcp_security.c

    # Utils - Memory & Thread
    ${CMAKE_CURRENT_SOURCE_DIR}/../utils/memory/nimcp_memory.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../utils/thread/nimcp_thread.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../utils/thread/nimcp_thread_pool.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../utils/async/nimcp_async.c

    # Utils - Logging
    ${CMAKE_CURRENT_SOURCE_DIR}/../utils/logging/nimcp_logging.c

    # Utils - Containers
    ${CMAKE_CURRENT_SOURCE_DIR}/../utils/containers/nimcp_vector.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../utils/containers/nimcp_hash_table.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../utils/containers/nimcp_btree.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../utils/containers/nimcp_queue.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../utils/containers/nimcp_graph.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../utils/containers/nimcp_min_heap.c

    # Utils - Other
    ${CMAKE_CURRENT_SOURCE_DIR}/../utils/time/nimcp_time.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../utils/validation/nimcp_validate.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../utils/queue_manager/nimcp_queue_manager.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../utils/json/nimcp_json.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../utils/config/nimcp_config.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../utils/metrics/nimcp_metrics.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../utils/cache/nimcp_cache.c

    # Utils - Spectral Analysis
    ${CMAKE_CURRENT_SOURCE_DIR}/../utils/spectral/nimcp_fft.c

    # Brain Oscillations
    ${CMAKE_CURRENT_SOURCE_DIR}/../core/brain_oscillations/nimcp_brain_oscillations.c

    # Perception
    ${CMAKE_CURRENT_SOURCE_DIR}/perception/nimcp_visual_cortex.c
    ${CMAKE_CURRENT_SOURCE_DIR}/perception/nimcp_audio_cortex.c

    # Python bindings
    ${CMAKE_SOURCE_DIR}/src/python/nimcp_module.c
    ${CMAKE_SOURCE_DIR}/src/python/nimcp_types.c
    ${CMAKE_SOURCE_DIR}/src/python/nimcp_metrics_py.c
)

# Core library
add_library(nimcp SHARED
    ${NIMCP_CORE_SOURCES}
)

# Update include directories to point to new structure
target_include_directories(nimcp
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_SOURCE_DIR}/src>
        $<INSTALL_INTERFACE:include/nimcp>
    PRIVATE
        ${NIMCP_LIB_DIR}
        ${Python3_INCLUDE_DIRS}
)

target_link_libraries(nimcp
    PUBLIC
        Python3::Python
        lz4
        jansson
)

# SIMD compiler flags (v2.7.0)
if(CMAKE_SYSTEM_PROCESSOR MATCHES "(x86)|(X86)|(amd64)|(AMD64)")
    # Enable AVX2 by default, AVX-512 if available
    include(CheckCXXCompilerFlag)
    if(CMAKE_COMPILER_IS_GNUCXX OR CMAKE_CXX_COMPILER_ID MATCHES "Clang")
        target_compile_options(nimcp PRIVATE -mavx2 -mfma)
        # Optionally enable AVX-512 (commented out by default as not all CPUs support it)
        # target_compile_options(nimcp PRIVATE -mavx512f -mavx512bw)
    elseif(MSVC)
        target_compile_options(nimcp PRIVATE /arch:AVX2)
    endif()
endif()

# Add testing API support
target_compile_definitions(nimcp PRIVATE NIMCP_TESTING)

# Enable Position Independent Code and set library properties
set_target_properties(nimcp PROPERTIES
    POSITION_INDEPENDENT_CODE ON
    ENABLE_EXPORTS ON
    VERSION ${PROJECT_VERSION}
    SOVERSION 2
    OUTPUT_NAME nimcp
    LIBRARY_OUTPUT_DIRECTORY ${CMAKE_SOURCE_DIR}/bin
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_SOURCE_DIR}/bin
)

# Installation rules
install(TARGETS nimcp
    EXPORT nimcp-targets
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
    ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
    INCLUDES DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}
)
