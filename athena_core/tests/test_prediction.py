import torch

from athena.data.video import Clips, VideoData, gather, windows
from athena.models import Brain, ModelConfig
from athena.train import TrainConfig, train


def quiet(*_):
    pass


def moving_squares(n: int, t: int = 10, seed: int = 0) -> Clips:
    """A bright 6x6 square crossing a dark frame at a constant, per-clip
    velocity (1-2 px per frame, either direction)."""
    g = torch.Generator().manual_seed(seed)
    frames = torch.full((n, t, 3, 32, 32), 20, dtype=torch.uint8)
    for i in range(n):
        x, y = torch.randint(6, 20, (2,), generator=g).tolist()
        vx, vy = (torch.randint(1, 3, (2,), generator=g) * (torch.randint(0, 2, (2,), generator=g) * 2 - 1)).tolist()
        for j in range(t):
            cx, cy = (x + vx * j) % 26, (y + vy * j) % 26
            frames[i, j, :, cy:cy + 6, cx:cx + 6] = 230
    return Clips(frames, "squares")


def test_windows_and_gather_line_up():
    c = moving_squares(3, t=6)
    w = windows(c, 4)
    assert len(w) == 3 * 2 and w[0].tolist() == [0, 3]
    ctx, nxt = gather(c, w[:1], 4, "cpu")
    full, _ = gather(c, torch.tensor([[0, 4]]), 4, "cpu")
    assert torch.equal(ctx[0, 1:], full[0, :3]) and torch.equal(nxt[0], full[0, 3])


def test_untrained_model_predicts_no_change():
    b = Brain(ModelConfig(size="tiny", vocab_size=1, video=True)).eval()
    f = torch.randn(2, 4, 3, 32, 32)
    assert torch.equal(b.predict_next(f), f[:, -1])


def test_motion_context_switch_changes_the_prediction():
    torch.manual_seed(0)
    a = Brain(ModelConfig(size="tiny", vocab_size=1, video=True)).eval()
    torch.nn.init.normal_(a.frame_decoder.last.weight, std=0.1)
    b = Brain(ModelConfig(size="tiny", vocab_size=1, video=True, modules={"motion_context": False})).eval()
    b.load_state_dict(a.state_dict())
    f = torch.randn(2, 4, 3, 32, 32)
    assert not torch.allclose(a.predict_next(f), b.predict_next(f))


def test_learns_constant_motion():
    """Copying the last frame is wrong for a moving square; a model that
    predicts motion must beat it clearly."""
    video = VideoData(train={"squares": moving_squares(400)}, val={"squares": moving_squares(40, seed=1)},
                      test={"squares": moving_squares(40, seed=2)})
    cfg = TrainConfig(model=ModelConfig(size="tiny"), epochs=15, batch_size=64, optimizer="adamw",
                      lr=2e-3, weight_decay=0.01, device="cpu")
    m = train(cfg, [], name="squares", log=quiet, video=video)["metrics"]
    assert m["test"]["video"]["squares"]["skill"] > 0.3, m["test"]["video"]
