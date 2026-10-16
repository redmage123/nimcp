# Changelog

All notable changes to the NIMCP (Neuromorphic Infant Machine Cognitive Platform) project will be documented in this file.

The format is based on [Keep a Changelog](https://keepachangelog.com/en/1.0.0/),
and this project adheres to [Semantic Versioning](https://semver.org/spec/v2.0.0.html).

## [2.6.1] - 2025-11-04

### Security
- Replaced all unsafe `strcpy()` calls with `strncpy()` + explicit null termination for buffer overflow protection
  - src/cognitive/knowledge/nimcp_knowledge.c:1675
  - src/utils/thread/nimcp_thread.c:2030
  - src/utils/json/nimcp_json.c:483
  - src/utils/json/nimcp_json.c:524
- Implemented defense-in-depth approach with explicit null byte termination after strncpy

### Fixed
- Memory corruption bug in Knowledge module that caused test segfaults
- Knowledge.LearnFromStory test now passes (previously segfaulted due to buffer overflow)

### Testing
- All 287 cognitive tests passing
- All 27 JSON tests passing
- Verified safe string handling patterns throughout codebase

## [2.6.0] - 2025-11-03

### Added
- FFT (Fast Fourier Transform) spectral analysis utilities
  - Cooley-Tukey radix-2 FFT algorithm (O(N log N) complexity)
  - Real-to-complex and complex-to-complex transforms
  - Window functions (Hann, Hamming, Blackman)
  - Power spectral density (PSD) computation
  - Brain wave band power extraction (Delta, Theta, Alpha, Beta, Gamma)
- Brain oscillation analysis module
  - Real-time neural oscillation detection
  - Cognitive state inference from brain waves
  - Phase-amplitude coupling (PAC) analysis
  - Network synchrony computation

### Testing
- 14/14 FFT tests passing
- All spectral analysis functionality validated

## [2.5.1] - 2025-11-03

### Added
- Knowledge B-tree indexing for efficient confidence-based queries (O(log n) range queries)
- `knowledge_get_by_confidence_range()` - Query knowledge items by confidence level
- `knowledge_get_all_ordered_by_confidence()` - Get all knowledge sorted by confidence
- `knowledge_add_item()` - Test helper API for direct knowledge insertion

### Fixed
- B-tree key extraction pattern using stable stored keys instead of thread-local buffers

### Testing
- 600+ tests passing
- 2/3 knowledge B-tree tests passing (1 performance issue under investigation)

## [2.5.0] - 2025-11-02

### Added
- Refactored visual and audio cortex modules to use NIMCP utility functions
- Consistent validation and logging patterns across perception modules
- Updated ~75+ validation points in visual and audio cortex

### Changed
- Replaced bare pointer checks with `nimcp_validate_pointer()` throughout perception modules
- Added descriptive `NIMCP_LOGGING_ERROR()` calls for better error tracking
- Improved error handling and diagnostics in perception layer

### Testing
- Build successful with no regressions
- All existing tests continue to pass

## Earlier Versions

See git history for changes in versions prior to 2.5.0.

---

**Note:** This changelog was started on 2025-11-04. Earlier project history is available in the git commit log.
