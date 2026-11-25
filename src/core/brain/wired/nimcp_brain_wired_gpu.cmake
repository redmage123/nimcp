# Sources for wired-module group BRAIN_WIRED_GPU (owned by this group only).
#
# The group file always builds; without CUDA it compiles to an inert no-op
# (NIMCP_ENABLE_CUDA undefined) and the CUDA modules below are not built.
# The CPU fallbacks for int8/TensorRT (gpu/stubs/nimcp_gpu_stubs_int8.c,
# nimcp_gpu_stubs_tensorrt.c) are already in the main list under NOT CUDA.
list(APPEND NIMCP_CORE_SOURCES
    ${CMAKE_CURRENT_SOURCE_DIR}/../core/brain/wired/nimcp_brain_wired_gpu.c
    # GPU health monitor (probes the brain's own device; feeds the health agent)
    $<$<BOOL:${CUDAToolkit_FOUND}>:${CMAKE_CURRENT_SOURCE_DIR}/../gpu/common/nimcp_gpu_health.cu>
    # GPU memory pool + factory (small scratch pool on the existing context)
    $<$<BOOL:${CUDAToolkit_FOUND}>:${CMAKE_CURRENT_SOURCE_DIR}/../gpu/memory/nimcp_gpu_pool.cu>
    $<$<BOOL:${CUDAToolkit_FOUND}>:${CMAKE_CURRENT_SOURCE_DIR}/../gpu/memory/nimcp_gpu_pool_factory.cu>
    # Async transfer + prefetch (ride the context's existing streams)
    $<$<BOOL:${CUDAToolkit_FOUND}>:${CMAKE_CURRENT_SOURCE_DIR}/../gpu/transfer/nimcp_async_transfer.cu>
    $<$<BOOL:${CUDAToolkit_FOUND}>:${CMAKE_CURRENT_SOURCE_DIR}/../gpu/transfer/nimcp_prefetch.cu>
    # Graph topology kernels (Louvain / clustering)
    $<$<BOOL:${CUDAToolkit_FOUND}>:${CMAKE_CURRENT_SOURCE_DIR}/../gpu/topology/nimcp_topology_kernels.cu>
    # INT8 inference + TensorRT export (TensorRT branch only with NIMCP_ENABLE_TENSORRT;
    # otherwise the file's built-in stub branch reports "not available")
    $<$<BOOL:${CUDAToolkit_FOUND}>:${CMAKE_CURRENT_SOURCE_DIR}/../gpu/inference/nimcp_int8_inference.cu>
    $<$<BOOL:${CUDAToolkit_FOUND}>:${CMAKE_CURRENT_SOURCE_DIR}/../gpu/inference/nimcp_tensorrt_export.cu>
)
