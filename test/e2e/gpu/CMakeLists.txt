# GPU E2E Tests CMakeLists.txt

# Macro to add GPU E2E tests
macro(add_gpu_e2e_test TEST_NAME)
    if(EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/${TEST_NAME}.cpp)
        add_executable(e2e_gpu_${TEST_NAME}
            ${TEST_NAME}.cpp
            ${CMAKE_SOURCE_DIR}/test/e2e/e2e_test_framework.cpp
        )
        target_link_libraries(e2e_gpu_${TEST_NAME}
            PRIVATE
                GTest::GTest
                GTest::Main
                nimcp
                nimcp_portia
                nimcp_middleware
                m
                pthread
        )
        target_include_directories(e2e_gpu_${TEST_NAME}
            PRIVATE
                ${CMAKE_SOURCE_DIR}/include
                ${CMAKE_SOURCE_DIR}/test/e2e
        )
        target_compile_features(e2e_gpu_${TEST_NAME} PRIVATE cxx_std_17)
        add_test(NAME e2e_gpu_${TEST_NAME}
                 COMMAND e2e_gpu_${TEST_NAME})
        set_tests_properties(e2e_gpu_${TEST_NAME} PROPERTIES
            LABELS "e2e;gpu"
            TIMEOUT 300
        )
    endif()
endmacro()

# Add GPU E2E tests
add_gpu_e2e_test(test_e2e_learning_pipeline_gpu)
add_gpu_e2e_test(test_e2e_cognitive_processing_gpu)
add_gpu_e2e_test(test_e2e_swarm_simulation_gpu)
add_gpu_e2e_test(test_e2e_brain_simulation_gpu)
add_gpu_e2e_test(test_e2e_dragonfly_hunting_gpu)
add_gpu_e2e_test(test_e2e_stress_test_gpu)

# GPU LNN Pipeline test
add_gpu_e2e_test(test_e2e_lnn_pipeline_gpu)

# GPU SNN Pipeline test
add_gpu_e2e_test(test_e2e_snn_pipeline_gpu)

# GPU Inference Pipeline test
add_gpu_e2e_test(test_e2e_inference_pipeline_gpu)

# GPU Visual Processing Pipeline test
add_gpu_e2e_test(test_e2e_visual_processing_gpu)

# GPU Inference Optimization Pipeline E2E test
add_gpu_e2e_test(test_e2e_gpu_inference_pipeline)
if(TARGET e2e_gpu_test_e2e_gpu_inference_pipeline)
    set_tests_properties(e2e_gpu_test_e2e_gpu_inference_pipeline PROPERTIES
        TIMEOUT 600
        RESOURCE_LOCK "brain_heavy"
    )
endif()

# DISABLED - require unimplemented graph GPU module
# add_gpu_e2e_test(e2e_gpu_test_brain_topology)
# add_gpu_e2e_test(e2e_gpu_test_sparse_network)

# GPU Fuzzy Controller E2E test
add_gpu_e2e_test(test_e2e_fuzzy_controller_gpu)

# GPU Financial Workflow E2E test
add_gpu_e2e_test(test_e2e_financial_workflow_gpu)

# GPU Financial Complete E2E test (comprehensive scenarios)
add_gpu_e2e_test(test_e2e_financial_complete)

# GPU Recovery Pipeline E2E test
add_gpu_e2e_test(test_e2e_gpu_recovery_pipeline)
