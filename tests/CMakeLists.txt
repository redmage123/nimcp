# ============================================================================
# Standalone-C unit tests for the SNN-language stack
# ============================================================================
# WHAT: Wires the 21 .c smoke tests under tests/unit/ into ctest so they run
#       under `ctest -R "test_lang_|test_bigram|test_snn_lang|test_bulk_lex"`.
# WHY:  Until now these tests existed as ad-hoc files compiled by hand from
#       the doc-comment header. Each one was self-contained (printf + exit,
#       no GTest dep) which made them fast and portable, but invisible to CI.
#       Wiring them in lets every CMake build verify the SNN-language bridge
#       contracts in one shot.
# HOW:  Each test is a single .c file that links against libnimcp + libm +
#       pthread. We define a helper `add_nimcp_standalone_test()` that
#       creates the executable, sets RPATH so it can find libnimcp.so.2 at
#       runtime, registers it with `add_test()`, and tags it under the
#       "lang_smoke" ctest label.
#
#       The tests are NOT linked against GTest — they use printf+exit-code
#       reporting (exit 0 == pass). ctest reads the exit code so we get
#       the same PASS/FAIL signal as a GTest binary.
#
#       The 21 .c files are listed by name (not glob) so a missing file
#       fails configuration with a clear "test source not found" error
#       rather than silently disappearing.
# ============================================================================

set(NIMCP_STANDALONE_LANG_TESTS
    # ============================================================
    # Bridge transport + non-learning paths (Slice A-compatible)
    # ============================================================
    test_lang_bridge_spike_routing
    test_bigram_spectrum
    test_lang_bridge_config_persistence
    test_lang_phantom_wiring_audit
    test_lang_anaphora
    test_lang_negation_discourse_polysemy
    test_lang_bridge_get_config
    # ============================================================
    # Lexicon / cognitive subsystems independent of bridge STDP
    # ============================================================
    test_bulk_lexicon
    test_lang_multiturn_persistence
    test_lang_lgss_gates
    test_lang_immune_inspection
    test_lang_spectrum_tick
    test_lang_da_modulation
    test_lang_reconsolidation
    test_lang_topic_shift
    test_lang_speech_acts
    test_lang_sentence_segmentation
    test_lang_tom_wiring
    test_lang_config_persist
    test_lang_consumer_events
    test_lang_subword_oov
    test_lang_coref
    test_lang_cascade_self_train_gated
    test_lang_cascade_orchestrator
    test_lang_parser_coverage
    test_snn_lang_v5_beam_rerank
    test_lang_cascade_counters
    test_broca_lexicon_concurrency
    test_lang_self_train_persistence
    test_lang_respond_via_cascade
    test_lang_phonological_loop
    test_lang_cerebellar_correction
    test_gl_fno_audio_guard
    test_snn_fno_recording
    test_lang_thalamic_gates
    test_lang_distributional_anticollapse
    test_lang_bigram_rerank
    test_gl_phrase_persistence
    test_lang_pos_slot_filling
    test_lang_wm_content_intent
    test_lang_stepe_cognitive_intent
    test_lang_surface_polish
    test_lang_function_words
    test_lang_f4_fluency
    test_lang_autoregressive_produce
    test_lang_pronominalization
    test_lang_discourse_seed
    test_lang_clause_frame
    test_lang_produce_distributional_weight
    test_lang_produce_frequency_penalty
    test_lang_inflection_guards
    test_lang_snn_decode
    test_lang_givenness_definite
    test_lang_t3_conjunction
    test_lang_tf_diff
    test_lang_tf_gating
    test_lang_tf_trigram_apply
    test_lang_tf_distrib_apply
    test_lang_tf_stdp_apply
    test_lang_tf_integration_50turn
    test_lang_tf_off_no_op
    # ============================================================
    # Option-1 rebuild — new slice surface tests
    # ============================================================
    test_snn_bridge_plumbing_only   # Slice A: bridge is transport-only
    test_concept_registry_binding   # Slice B: cross-modal registry
    test_concept_registry_live_binding  # Walkthrough-2: gl→registry live wiring
    test_lang_stage_scaffolding     # Slice E: stage constraints
    test_snn_negative_da            # Slice F: SNN STDP under negative DA
    test_snn_generate_step          # Produce-time SNN step: inject→step→decode
    test_snn_lang_learn_discriminate # SNN learns + generates + discriminates concept→word
    test_snn_lang_targeted_projection # scalable targeted concept→word topology (128 syn, not 1M)
    test_snn_lang_generate_and_decode # produce-path bridge call: connect_network + generate_and_decode
    test_snn_csr_reopen               # runtime synaptogenesis core: reopen finalized CSR + re-add
    test_lang_networks_into_content # ANN/LNN/HNN/CNN → content_intent (default-OFF)
    # ============================================================
    # REMOVED in walkthrough-1 (Option-1 rebuild) — these asserted
    # bridge STDP / weight-matrix / produce-from-bindings behavior
    # that Slice A intentionally deleted. The bridge is now transport
    # only; the equivalent learning lives in the SNN's three-factor
    # STDP (test_snn_negative_da) and Slice B's concept_registry
    # (test_concept_registry_binding):
    #
    #   test_lang_bridge_cosine_norm     — produce() returns -1 (no bindings)
    #   test_lang_bridge_sampling        — produce() returns -1
    #   test_lang_bridge_glove_blend     — produce() returns -1
    #   test_lang_bridge_autoregressive  — produce() returns -1
    #   test_lang_bridge_next_token      — learn_next_token_* removed
    #   test_lang_bridge_riemannian      — strengthen_binding_riemannian removed
    #   test_lang_bridge_hyperbolic_glove — produce() returns -1
    #   test_lang_bridge_quantum_sampling — produce() returns -1
    #   test_lang_bridge_quantum_shannon — produce() returns -1
    #   test_lang_bridge_beam_eos_repeat — produce() returns -1
    #   test_lang_bridge_latency         — produce() returns -1
    #   test_lang_bridge_rng_seed        — seed drives produce sampling
    #   test_snn_lang_bridge_stdp        — bridge STDP removed wholesale
    #   test_lang_trigram_learning       — bridge trigram counters frozen
    #   test_lang_length_control         — produce() returns -1
    #   test_lang_stream_produce         — produce() returns -1
    #   test_lang_decode_bench           — decode returns zero candidates
    #   test_lang_cascade_self_train     — echo_correct + binding strengthen gone
    #   test_lang_lateral_inhibition     — decode_with_lateral_inhibition stub
)

# Helper: register a single standalone-C test.
#
#   target_name : ctest target name (also the executable name)
#   source_file : absolute path to the .c file
#
# Each test:
#   - links nimcp + m + pthread (the same triplet the doc-comments specify)
#   - sets BUILD_RPATH and INSTALL_RPATH to ${CMAKE_BINARY_DIR}/lib so the
#     executable can locate libnimcp.so.2 at run time without LD_LIBRARY_PATH.
#     This matches the user-facing build pattern documented in the test file
#     headers ("-Wl,-rpath,/home/bbrelin/nimcp/build/lib").
#   - uses include path ${CMAKE_SOURCE_DIR}/include so headers like
#     "snn/bridges/nimcp_snn_language_bridge.h" resolve from the worktree.
#   - registers with add_test() under the "lang_smoke" label.
function(add_nimcp_standalone_test target_name source_file)
    if(NOT EXISTS "${source_file}")
        message(WARNING "Standalone test source not found: ${source_file}")
        return()
    endif()

    add_executable(${target_name} "${source_file}")

    # The .c tests poke at private headers under include/.
    target_include_directories(${target_name} PRIVATE
        "${CMAKE_SOURCE_DIR}/include"
        "${CMAKE_SOURCE_DIR}/src"
    )

    # Link against the in-tree libnimcp + libm + libpthread.
    # Threads::Threads is set up by the top-level CMakeLists; if not, fall
    # back to plain "pthread" so this file is self-contained.
    if(TARGET Threads::Threads)
        target_link_libraries(${target_name} PRIVATE nimcp m Threads::Threads)
    else()
        target_link_libraries(${target_name} PRIVATE nimcp m pthread)
    endif()

    # RPATH so the test binary can dlopen libnimcp.so.2 at runtime without
    # the caller exporting LD_LIBRARY_PATH. Two settings — BUILD_RPATH for
    # in-place ctest runs, INSTALL_RPATH for installed-test layouts.
    set_target_properties(${target_name} PROPERTIES
        BUILD_WITH_INSTALL_RPATH FALSE
        BUILD_RPATH   "${CMAKE_BINARY_DIR}/lib"
        INSTALL_RPATH "${CMAKE_BINARY_DIR}/lib"
    )

    # Make sure the lib is built before the test (some generators get this
    # implicitly via target_link_libraries; explicit add_dependencies is a
    # belt-and-suspenders for older Make/Ninja generators).
    add_dependencies(${target_name} nimcp)

    # Belt-and-suspenders header tracking for the SNN language bridge.
    #
    # WHAT: hardcoded OBJECT_DEPENDS on the bridge header, in addition to
    #       the implicit `-MD -MF *.c.o.d` depfile path CMake already
    #       wires up via target_link_libraries(${target} PRIVATE nimcp).
    #
    # WHY:  the previous EOS commit (revert 65337cdbc) shipped from a
    #       worktree where the test .c.o files were stale w.r.t. the bridge
    #       header — they had been compiled against the pre-EOS struct
    #       layout while the library was rebuilt with the larger layout.
    #       The implicit depfile path SHOULD have caught this — and on a
    #       clean tree it does — but if anything (filesystem time skew,
    #       partial build, hand-edited *.c.o, interrupted `make -j`)
    #       leaves the compiler_depend.make in a half-populated state,
    #       this explicit OBJECT_DEPENDS forces a recompile whenever the
    #       bridge header changes mtime regardless of compiler-depfile
    #       contents. Pairs with the _Static_assert size sentinels at
    #       the bottom of the header to turn ABI drift into a compile
    #       error instead of a runtime stack-smash.
    #
    # HOW:  set_property(... PROPERTY OBJECT_DEPENDS ...) attaches a
    #       Make-level dependency on the listed files to every object
    #       generated by this target. Make re-runs the compile any time
    #       one of these files is newer than the .c.o.
    set_property(
        TARGET ${target_name}
        APPEND PROPERTY OBJECT_DEPENDS
            "${CMAKE_SOURCE_DIR}/include/snn/bridges/nimcp_snn_language_bridge.h"
    )

    # Register with ctest. Run from the binary dir so any temp files the
    # test writes (test_bulk_lexicon writes a /tmp .bin) land in a known
    # place. Tag with a label so `ctest -L lang_smoke` selects only these.
    add_test(NAME ${target_name} COMMAND ${target_name})
    set_tests_properties(${target_name} PROPERTIES
        LABELS "lang_smoke"
        # 60s is plenty — every standalone test is a few-call smoke,
        # not a stress run.
        TIMEOUT 60
    )
endfunction()

# Walk the list and register each one. Skip any source file that is missing
# from the worktree (e.g. test_bulk_lexicon.c if the loader hasn't been
# checked in on this branch — per the task spec, that one is allowed to be
# absent).
foreach(_t IN LISTS NIMCP_STANDALONE_LANG_TESTS)
    set(_src "${CMAKE_CURRENT_SOURCE_DIR}/unit/${_t}.c")
    if(EXISTS "${_src}")
        add_nimcp_standalone_test(${_t} "${_src}")
    else()
        message(STATUS "Standalone test source not found, skipping: ${_src}")
    endif()
endforeach()
