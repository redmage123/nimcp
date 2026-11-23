"""E-prop learner: its update is the true gradient where theory says it must
be, it memorises (overfit test), random feedback still learns, and the
learning signal is not a statue."""
import torch

from athena.bio.eprop import EpropConfig, EpropSNN


def _patterns(n_classes=4, n_in=30, per_class=5, seed=0):
    g = torch.Generator().manual_seed(seed)
    protos = (torch.rand(n_classes, n_in, generator=g) > 0.6).float()
    y = torch.arange(n_classes).repeat_interleave(per_class)
    X = (protos[y] * 0.9 + 0.1 * torch.rand(len(y), n_in, generator=g)).clamp(0, 1)
    return X, y


class _Spike(torch.autograd.Function):
    """Heaviside forward, e-prop's pseudo-derivative backward."""
    @staticmethod
    def forward(ctx, v, th, gamma):
        ctx.save_for_backward(v)
        ctx.th, ctx.gamma = th, gamma
        return (v > th).to(v.dtype)

    @staticmethod
    def backward(ctx, g):
        (v,) = ctx.saved_tensors
        th = ctx.th
        psi = ctx.gamma * torch.clamp(1.0 - (v - th).abs() / th, min=0.0) / th
        return g * psi, None, None


def test_symmetric_eprop_equals_the_gradient_without_recurrence():
    """With no recurrent weights and B = W_out^T, e-prop is exact (Bellec et
    al. 2020): its update equals d/dW of sum_t CE(pi(t), target), computed by
    autograd through the same dynamics (reset path not differentiated, as in
    e-prop's LIF eligibility)."""
    torch.set_default_dtype(torch.float64)
    try:
        cfg = EpropConfig(n_rec=12, steps=15, read_steps=5, recurrent=False, feedback="symmetric",
                          reg=0.0, threshold=0.3, seed=3)
        net = EpropSNN(8, 3, cfg)
        g = torch.Generator().manual_seed(1)
        spikes = (torch.rand(cfg.steps, 4, 8, generator=g) < 0.4).to(torch.float64)
        target = torch.nn.functional.one_hot(torch.tensor([0, 1, 2, 1]), 3).to(torch.float64)
        _, (d_in, _, d_out, d_b), _ = net.run(spikes, target)

        W_in = net.W_in.clone().requires_grad_(True)
        W_out = net.W_out.clone().requires_grad_(True)
        b = net.b_out.clone().requires_grad_(True)
        v = torch.zeros(4, 12)
        z = torch.zeros(4, 12)
        y = torch.zeros(4, 3)
        loss = 0.0
        for t in range(cfg.steps):
            v = net.a * v + spikes[t] @ W_in - z.detach() * cfg.threshold
            z = _Spike.apply(v, cfg.threshold, cfg.gamma)
            y = net.k * y + z @ W_out + b
            loss = loss - (target * torch.log_softmax(y, 1)).sum()
        loss.backward()
        assert torch.allclose(d_in, W_in.grad, atol=1e-9), (d_in - W_in.grad).abs().max()
        assert torch.allclose(d_out, W_out.grad, atol=1e-9)
        assert torch.allclose(d_b, b.grad, atol=1e-9)
        assert W_in.grad.abs().sum() > 0          # the check is not vacuous
    finally:
        torch.set_default_dtype(torch.float32)


def test_random_eprop_memorises():
    X, y = _patterns()
    net = EpropSNN(X.shape[1], 4, EpropConfig(n_rec=64, seed=0))
    net.fit(X, y, epochs=40, batch=4)
    assert (net.predict(X, repeats=3) == y).float().mean() >= 0.95


def test_learning_signal_trains_the_hidden_weights():
    """With feedback B = 0 and no regulariser the input weights never move
    (only the readout learns); with feedback they do."""
    X, y = _patterns()
    for B_scale, moves in ((0.0, False), (1.0, True)):
        net = EpropSNN(X.shape[1], 4, EpropConfig(n_rec=32, reg=0.0, seed=1))
        net.B *= B_scale
        w0 = net.W_in.clone()
        net.fit(X, y, epochs=3, batch=4)
        assert bool((net.W_in - w0).abs().sum() > 0) == moves


def test_no_autograd_in_the_learner():
    X, y = _patterns()
    net = EpropSNN(X.shape[1], 4, EpropConfig(n_rec=16, seed=2))
    net.fit(X, y, epochs=1, batch=4)
    for w in (net.W_in, net.W_rec, net.W_out, net.b_out):
        assert not w.requires_grad and w.grad is None
