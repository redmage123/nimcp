# Positional Encoding

**Last Updated**: 2026-03-11

## Overview

Multiple positional encoding methods for neural sequence processing. Networks lack inherent position awareness; explicit encoding is needed.

**Header**: `include/utils/encoding/nimcp_positional_encoding.h`
**Source**: `src/utils/encoding/nimcp_positional_encoding.c`

## Encoding Types

| Type | Enum | Properties |
|------|------|-----------|
| Sinusoidal | `NIMCP_POS_SINUSOIDAL` | Fixed sin/cos, no training, extrapolates well (Vaswani 2017) |
| Learned | `NIMCP_POS_LEARNED` | Trainable lookup table, task-specific |
| Rotary (RoPE) | `NIMCP_POS_ROTARY` | Rotation-based relative encoding, best for long sequences (Su 2021) |
| ALiBi | `NIMCP_POS_ALIBI` | Linear attention bias, simplest and most efficient (Press 2022) |
| Relative | `NIMCP_POS_RELATIVE` | Relative position embeddings (Shaw 2018) |

## Constants

| Constant | Value |
|----------|-------|
| `NIMCP_POS_MAX_SEQ_LENGTH` | 32768 |
| `NIMCP_POS_MAX_DIM` | 4096 |
| `NIMCP_ROPE_DEFAULT_BASE` | 10000.0 |
| `NIMCP_ALIBI_DEFAULT_HEADS` | 8 |

## Module Integration

| Module | PE Types Used |
|--------|-------------|
| Multihead Attention | RoPE, ALiBi |
| Working Memory | Sinusoidal, Relative |
| Sequence Detector | RoPE, Relative |
| Predictive Regions | Learned, Sinusoidal |
| Speech Cortex | Sinusoidal, Learned |
| Emotion-Attention | Sinusoidal, Learned |
| Circular Buffer | Sinusoidal, ALiBi |
| Swarm Signal | Sinusoidal, ALiBi |
| Language Production | Sinusoidal, RoPE |
| Population Coding | Sinusoidal |

## Configuration

Base config shared by all types:

```c
typedef struct {
    uint32_t max_seq_length;   // Maximum sequence length
    uint32_t embedding_dim;     // Dimension of embeddings
    bool cache_enabled;         // Pre-compute and cache encodings
    bool thread_safe;           // Enable thread-safe operations
} nimcp_pos_base_config_t;
```

Type-specific configs extend the base (e.g., `nimcp_pos_sinusoidal_config_t` adds `frequency_base` and `frequency_scale`).

## Bio-Async Integration

- Encoding operations emit `BIO_MSG_ENCODING_COMPUTE` messages
- Cache updates via `BIO_MSG_ENCODING_CACHE_UPDATE`
- Uses DOPAMINE channel for computation completion signals

## Error Codes (3200-3299)

`NIMCP_POS_SUCCESS`, `NIMCP_POS_ERROR_NULL_PARAM`, `NIMCP_POS_ERROR_INVALID_DIM`, `NIMCP_POS_ERROR_INVALID_POS`, `NIMCP_POS_ERROR_ALLOC_FAILED`, `NIMCP_POS_ERROR_INVALID_TYPE`, `NIMCP_POS_ERROR_NOT_INIT`, `NIMCP_POS_ERROR_CACHE_MISS`
