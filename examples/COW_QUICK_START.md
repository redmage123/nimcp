# COW Examples Quick Start

## Which Example Should I Use?

### 🏢 Building a Multi-Tenant Inference Service?
→ **Start with**: `cow_inference_server.c`

**You need this if**:
- Serving multiple customers from one model
- Each tenant needs isolation
- Memory is limited
- Read-only inference workload

**What you'll learn**:
- Creating COW clones for tenant isolation
- Memory savings: 86% per tenant
- Verification that inference stays read-only

### 🔬 Experimenting with Training Strategies?
→ **Start with**: `cow_snapshot_learning.c`

**You need this if**:
- Trying different training approaches
- Need instant rollback capability
- Worried about catastrophic forgetting
- Want safe experimentation

**What you'll learn**:
- Instant checkpointing (<1ms)
- Comparing before/after training
- Zero-cost rollback
- Memory savings: 99%

### 🔀 A/B Testing Multiple Approaches?
→ **Start with**: `cow_ab_testing.c`

**You need this if**:
- Comparing hyperparameters
- Testing multiple algorithms
- Running grid searches
- Parallel experimentation

**What you'll learn**:
- Parallel strategy testing
- Memory efficiency with multiple branches
- Best strategy selection
- Memory savings: 64%

## Quick Build and Run

```bash
# Build all examples
cd /path/to/nimcp/build
make cow_inference_server cow_snapshot_learning cow_ab_testing

# Run each example
./examples/cow_inference_server      # ~30 seconds
./examples/cow_snapshot_learning     # ~15 seconds  
./examples/cow_ab_testing           # ~45 seconds
```

## Key API Summary

```c
// Clone for multi-tenant inference
nimcp_brain_t clone = nimcp_brain_clone_cow(base_brain);

// Snapshot before training
nimcp_brain_snapshot_t snapshot = nimcp_brain_snapshot_cow(brain);

// Rollback if needed
nimcp_brain_restore_cow(brain, snapshot);

// Get COW statistics
nimcp_brain_probe_t probe;
nimcp_brain_probe(clone, &probe);
printf("Shared: %zu MB\n", probe.cow_shared_bytes / 1024 / 1024);
```

## Expected Performance

| Operation | Time | Memory Overhead |
|-----------|------|-----------------|
| Clone creation | <10ms | 7MB |
| Snapshot creation | <1ms | 48 bytes |
| Restore | <1ms | 0 bytes |
| Inference (clone) | Same as original | 0 bytes |

## Memory Savings Examples

**10 inference tenants**:
- Traditional: 550 MB (10 × 50MB + base)
- COW: 120 MB (base + 10 × 7MB)
- **Savings: 78%**

**Snapshot during training**:
- Traditional: 100 MB (2 × 50MB)
- COW: 50 MB (base + 48 bytes)
- **Savings: 99.9%**

**3 A/B strategies**:
- Traditional: 200 MB (4 × 50MB)
- COW: 71 MB (base + 3 × 7MB)
- **Savings: 64.5%**

## Common Gotchas

❌ **Don't**: Train clone immediately after creation
- Triggers copy, losing COW benefit
- Use snapshot instead if you need to train

❌ **Don't**: Destroy base brain while clones exist
- Clones will become invalid
- Keep base alive until all clones destroyed

✅ **Do**: Use clones for read-only inference
- Maximum memory sharing
- Zero overhead

✅ **Do**: Create snapshots before risky operations
- Instant rollback capability
- Nearly zero memory cost

## Need More Details?

See `COW_EXAMPLES_README.md` for:
- Detailed architecture explanations
- Implementation details
- Best practices
- Troubleshooting guide
- Advanced patterns
