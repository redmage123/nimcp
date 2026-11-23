"""Diagonal-recurrence LM: its local traces are the exact gradient (vs
autograd through time), random feedback still learns context, and the
local learner uses no autograd."""
import math

import torch

from athena.bio.diag_lm import DiagLM, DiagLMConfig, DiagLMTorch


def test_local_traces_equal_backprop_through_time():
    cfg = DiagLMConfig(n=12, feedback="symmetric", ring=64, seed=4)
    net = DiagLM(7, cfg, dtype=torch.float64)
    g = torch.Generator().manual_seed(0)
    S = torch.randint(0, 7, (3, 20), generator=g)
    net.W_in += 0.5                                  # keep most rates positive
    st = net.init_state(3, learn=True)
    d = net.zero_deltas()
    for t in range(S.shape[1] - 1):
        net.step(st, S[:, t], S[:, t + 1], d)

    m = DiagLMTorch(net)
    logits, _ = m(S[:, :-1])
    loss = torch.nn.functional.cross_entropy(logits.reshape(-1, 7), S[:, 1:].reshape(-1),
                                             reduction="sum")
    loss.backward()
    for k in DiagLM.PARAMS:
        got, want = d[k], getattr(m, k).grad
        assert torch.allclose(got, want, atol=1e-9), (k, (got - want).abs().max())
        assert want.abs().sum() > 0, k                # not vacuous


def _order_task(net, reps=300, passes=6):
    ids = torch.tensor([3, 0, 1, 4, 0, 2] * reps)
    for _ in range(passes):
        net.train_pass(ids, batch=4)
    S = DiagLM.streams(ids, 4)
    st = net.init_state(4, learn=False)
    hit = n = 0
    for t in range(S.shape[1] - 1):
        logp = net.step(st, S[:, t])
        if t > 12 and bool((S[:, t] == 0).all()):     # the ambiguous position
            hit += int((logp.argmax(1) == S[:, t + 1]).sum())
            n += 4
    return hit / n


def test_random_feedback_learns_context():
    """After 0 comes 1 or 2 depending on the word before: only memory of the
    previous word can tell, so this needs the recurrence to learn."""
    assert _order_task(DiagLM(5, DiagLMConfig(n=32, seed=1))) >= 0.9


def test_no_autograd_in_the_local_learner():
    net = DiagLM(5, DiagLMConfig(n=16, seed=2))
    nll = net.train_pass(torch.tensor([0, 1, 2, 3, 4] * 40), batch=4)
    assert math.isfinite(nll)
    for k in DiagLM.PARAMS:
        w = getattr(net, k)
        assert not w.requires_grad and w.grad is None


def test_learned_feedback_aligns_with_the_forward_weights():
    """Kolen-Pollack: B starts random and comes to point along W_out^T."""
    net = DiagLM(6, DiagLMConfig(n=24, feedback="kp", kp_decay=0.02, seed=5))
    cos = lambda: float(torch.nn.functional.cosine_similarity(net.B.flatten(), net.W_out.T.flatten(), dim=0))
    before = cos()
    for _ in range(4):
        net.train_pass(torch.tensor([0, 1, 2, 3, 4, 5, 3, 1] * 60), batch=4)
    assert abs(before) < 0.3 and cos() > 0.9, (before, cos())


def test_learned_feedback_learns_context():
    assert _order_task(DiagLM(5, DiagLMConfig(n=32, feedback="kp", seed=1))) >= 0.9
