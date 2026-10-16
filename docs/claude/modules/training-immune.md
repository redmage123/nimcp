# Training-Immune Integration

Bidirectional integration between brain immune system and training pipeline.

## Types
- `training_immune_system_t` vs `brain_immune_system_t` -- different types, do not confuse
- Enable via `train_config.enable_immune_integration = true`

## Learning Rate Modulation (Fever Model)

| Inflammation | LR Factor |
|--------------|-----------|
| NONE | 1.00 |
| LOCAL | 0.95 |
| REGIONAL | 0.80 |
| SYSTEMIC | 0.50 |
| STORM | 0.10 |

## Instability Detection

| Condition | Severity |
|-----------|----------|
| NaN/Inf | 10 |
| Loss Explosion | 8 |
| Gradient Explosion | 6 |
| Gradient Vanishing | 4 |
| Loss Plateau | 3 |

## Integration Points
- Gradient health monitoring: NaN/Inf detection in loss and gradients
- Immune response to training instability (LR reduction, rollback)
- Integration with LNN training context

## Files
- Header: `include/middleware/immune/nimcp_training_immune.h`
- Implementation: `src/middleware/immune/nimcp_training_immune.c`

## Test Coverage: 30+ tests
