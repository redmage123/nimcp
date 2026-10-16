"""CE-7: Visual art corpus — procedural composition generator.

Pure-Python curriculum module. The training loop calls `run_visual_drip`
at the same cadence as the canonical / math / streaming drips. For each
call we:

  1. Pick a stage-appropriate composition name (deterministic but rotated).
  2. Generate a deterministic geometric image for that name on-the-fly via
     numpy rasterizers (no PIL/cv2; no images persisted to disk).
  3. Generate a verbal description of the composition (color words, shape
     words, layout words).
  4. Pipe the image into the brain's visual cortex if the visual-path RPC
     `visual_cortex_process` is available; otherwise fall back to
     `train_language(description, description)` as a textual stand-in.
  5. Probe the brain with `produce_text(intent_from_description)` and score
     the response against expected keywords:
            (2 * recall + specificity) / 3
     — same family of scoring as CE-1 storytelling.
  6. Feedback per composition:
        - composite >= 0.45 → brain.learn_language(produced_text)
        - composite <  0.45 → brain.train_language(description, description)
                              (re-anchor on the description so the brain
                               learns the target shape).

Visual-path choice: the brain daemon exposes
`visual_cortex_process(pixels, width, height, channels)` (see
`scripts/brain_client.py:513`). If that method is present on the brain
handle we use it; otherwise we fall back to the textual `train_language`
path so this module remains usable in test fixtures + headless brains.

CRITICAL: deployment has tight disk + storage constraints. We NEVER
persist generated images. Every frame is built in-memory, handed to the
brain, and discarded.

Public API:
    ART_COMPOSITIONS = {1: [...], 2: [...], 3: [...]}
    generate_image_array(composition_name, *, height=64, width=64, channels=3) -> np.ndarray
    generate_composition_description(composition_name, stage) -> str
    score_visual_response(brain_response_text, expected_keywords) -> dict
    run_visual_drip(brain, stage, *, composer=None, num_compositions=2,
                    composition_index=None, log_every=10) -> list[dict] | None
    VISUAL_PASS_THRESHOLD = 0.45

Stage 1 is NOT a no-op — early Athena needs sensory exposure to colour
and shape primitives. Stage 1 uses the simplest 4-composition palette.
"""
from __future__ import annotations

import re
from typing import Iterable

import numpy as np


# ---------------------------------------------------------------------------
# Stage palettes
# ---------------------------------------------------------------------------

ART_COMPOSITIONS: dict[int, list[str]] = {
    1: [
        "single_red_circle",
        "single_blue_square",
        "horizontal_split_yellow_green",
        "checkerboard_2x2",
    ],
    2: [
        "three_circles_triangle",
        "concentric_rings",
        "diagonal_stripes_warm",
        "house_shape",
        "tree_silhouette",
    ],
    3: [
        "kandinsky_geometric",
        "mondrian_grid",
        "color_field_gradient",
        "pointillist_mosaic",
        "fragmented_symmetry",
    ],
}


VISUAL_PASS_THRESHOLD = 0.45


# ---------------------------------------------------------------------------
# Color palette helpers (named tuples avoided — plain RGB triples are fine)
# ---------------------------------------------------------------------------

_RED    = (220,  30,  30)
_BLUE   = ( 30,  60, 220)
_YELLOW = (235, 215,  60)
_GREEN  = ( 40, 170,  70)
_ORANGE = (235, 140,  40)
_PURPLE = (140,  70, 180)
_BLACK  = (  0,   0,   0)
_WHITE  = (255, 255, 255)
_BROWN  = (110,  70,  40)
_DARKGREEN = ( 30, 110,  50)


def _blank(height: int, width: int, channels: int,
           fill: tuple[int, int, int] = _WHITE) -> np.ndarray:
    """Return an (H, W, C) uint8 canvas filled with `fill`."""
    img = np.zeros((height, width, channels), dtype=np.uint8)
    if channels == 3:
        img[..., 0] = fill[0]
        img[..., 1] = fill[1]
        img[..., 2] = fill[2]
    else:
        # Grayscale fallback: use luminance of fill.
        lum = int(0.2126 * fill[0] + 0.7152 * fill[1] + 0.0722 * fill[2])
        img[..., 0] = lum
        for c in range(1, channels):
            img[..., c] = lum
    return img


def _draw_disk(img: np.ndarray, cy: int, cx: int, r: int,
               color: tuple[int, int, int]) -> None:
    """In-place rasterized disk: (y - cy)^2 + (x - cx)^2 <= r^2."""
    h, w = img.shape[:2]
    yy, xx = np.ogrid[:h, :w]
    mask = (yy - cy) ** 2 + (xx - cx) ** 2 <= r * r
    for c in range(min(3, img.shape[2])):
        img[..., c] = np.where(mask, color[c], img[..., c])


def _draw_ring(img: np.ndarray, cy: int, cx: int,
               r_outer: int, r_inner: int,
               color: tuple[int, int, int]) -> None:
    """In-place annulus."""
    h, w = img.shape[:2]
    yy, xx = np.ogrid[:h, :w]
    d2 = (yy - cy) ** 2 + (xx - cx) ** 2
    mask = (d2 <= r_outer * r_outer) & (d2 >= r_inner * r_inner)
    for c in range(min(3, img.shape[2])):
        img[..., c] = np.where(mask, color[c], img[..., c])


def _draw_rect(img: np.ndarray, y0: int, x0: int, y1: int, x1: int,
               color: tuple[int, int, int]) -> None:
    """In-place axis-aligned filled rectangle. Coords clamped to bounds."""
    h, w = img.shape[:2]
    y0 = max(0, min(h, int(y0)))
    y1 = max(0, min(h, int(y1)))
    x0 = max(0, min(w, int(x0)))
    x1 = max(0, min(w, int(x1)))
    if y1 <= y0 or x1 <= x0:
        return
    for c in range(min(3, img.shape[2])):
        img[y0:y1, x0:x1, c] = color[c]


def _draw_triangle(img: np.ndarray,
                   p1: tuple[int, int],
                   p2: tuple[int, int],
                   p3: tuple[int, int],
                   color: tuple[int, int, int]) -> None:
    """In-place filled triangle via three half-plane tests.

    Points are (y, x). We compute the signed area of (p1, p2, p3); a pixel
    is inside iff all three sub-triangle signed areas have the same sign as
    the parent.
    """
    h, w = img.shape[:2]
    yy, xx = np.mgrid[:h, :w]

    def sgn(ay, ax, by, bx, cy, cx):
        return (bx - ax) * (cy - ay) - (by - ay) * (cx - ax)

    d1 = sgn(p1[0], p1[1], p2[0], p2[1], yy, xx)
    d2 = sgn(p2[0], p2[1], p3[0], p3[1], yy, xx)
    d3 = sgn(p3[0], p3[1], p1[0], p1[1], yy, xx)

    has_neg = (d1 < 0) | (d2 < 0) | (d3 < 0)
    has_pos = (d1 > 0) | (d2 > 0) | (d3 > 0)
    mask = ~(has_neg & has_pos)
    for c in range(min(3, img.shape[2])):
        img[..., c] = np.where(mask, color[c], img[..., c])


def _draw_line(img: np.ndarray, y0: int, x0: int, y1: int, x1: int,
               color: tuple[int, int, int], thickness: int = 1) -> None:
    """In-place thick line via point-to-segment distance."""
    h, w = img.shape[:2]
    yy, xx = np.ogrid[:h, :w]
    dy = float(y1 - y0)
    dx = float(x1 - x0)
    seg_len2 = dy * dy + dx * dx
    if seg_len2 <= 0:
        return
    # Param t = clamp( ((p - a) . (b - a)) / |b - a|^2, 0, 1 )
    t = ((yy - y0) * dy + (xx - x0) * dx) / seg_len2
    t = np.clip(t, 0.0, 1.0)
    py = y0 + t * dy
    px = x0 + t * dx
    dist2 = (yy - py) ** 2 + (xx - px) ** 2
    mask = dist2 <= float(thickness) ** 2
    for c in range(min(3, img.shape[2])):
        img[..., c] = np.where(mask, color[c], img[..., c])


def _seed_for(name: str) -> int:
    """Deterministic 32-bit seed derived from the composition name."""
    return hash(name) & 0xFFFFFFFF


# ---------------------------------------------------------------------------
# Per-composition rasterizers
# ---------------------------------------------------------------------------

def _compose_single_red_circle(h, w, c) -> np.ndarray:
    img = _blank(h, w, c, _WHITE)
    _draw_disk(img, h // 2, w // 2, min(h, w) // 3, _RED)
    return img


def _compose_single_blue_square(h, w, c) -> np.ndarray:
    img = _blank(h, w, c, _WHITE)
    side = min(h, w) // 2
    cy, cx = h // 2, w // 2
    _draw_rect(img, cy - side // 2, cx - side // 2,
                    cy + side // 2, cx + side // 2, _BLUE)
    return img


def _compose_horizontal_split_yellow_green(h, w, c) -> np.ndarray:
    img = _blank(h, w, c, _WHITE)
    _draw_rect(img, 0, 0, h // 2, w, _YELLOW)
    _draw_rect(img, h // 2, 0, h, w, _GREEN)
    return img


def _compose_checkerboard_2x2(h, w, c) -> np.ndarray:
    img = _blank(h, w, c, _WHITE)
    half_h, half_w = h // 2, w // 2
    # Top-left + bottom-right black; top-right + bottom-left white.
    _draw_rect(img, 0, 0, half_h, half_w, _BLACK)
    _draw_rect(img, 0, half_w, half_h, w, _WHITE)
    _draw_rect(img, half_h, 0, h, half_w, _WHITE)
    _draw_rect(img, half_h, half_w, h, w, _BLACK)
    return img


def _compose_three_circles_triangle(h, w, c) -> np.ndarray:
    img = _blank(h, w, c, _WHITE)
    r = min(h, w) // 8
    # Three circles arranged in a triangle (apex up).
    _draw_disk(img, h // 4, w // 2, r, _RED)
    _draw_disk(img, 3 * h // 4, w // 4, r, _BLUE)
    _draw_disk(img, 3 * h // 4, 3 * w // 4, r, _GREEN)
    return img


def _compose_concentric_rings(h, w, c) -> np.ndarray:
    img = _blank(h, w, c, _WHITE)
    cy, cx = h // 2, w // 2
    r_max = min(h, w) // 2
    palette = [_RED, _ORANGE, _YELLOW, _GREEN, _BLUE, _PURPLE]
    n = 5
    for i in range(n):
        r_outer = int(r_max * (n - i) / n)
        r_inner = int(r_max * (n - i - 1) / n)
        if r_outer <= r_inner:
            r_inner = max(0, r_outer - 1)
        _draw_ring(img, cy, cx, r_outer, r_inner, palette[i % len(palette)])
    return img


def _compose_diagonal_stripes_warm(h, w, c) -> np.ndarray:
    img = _blank(h, w, c, _WHITE)
    yy, xx = np.mgrid[:h, :w]
    # Stripes by (y + x) // stripe_w
    stripe_w = max(2, min(h, w) // 8)
    band = ((yy + xx) // stripe_w) % 3
    palette = [_RED, _ORANGE, _YELLOW]
    for i, color in enumerate(palette):
        mask = band == i
        for ch in range(min(3, img.shape[2])):
            img[..., ch] = np.where(mask, color[ch], img[..., ch])
    return img


def _compose_house_shape(h, w, c) -> np.ndarray:
    img = _blank(h, w, c, (200, 220, 235))  # sky-blue background
    # Ground.
    _draw_rect(img, 4 * h // 5, 0, h, w, _DARKGREEN)
    # House body (brown rect).
    body_y0, body_y1 = h // 2, 4 * h // 5
    body_x0, body_x1 = w // 4, 3 * w // 4
    _draw_rect(img, body_y0, body_x0, body_y1, body_x1, _BROWN)
    # Roof (red triangle).
    apex = (h // 4, w // 2)
    left = (body_y0, body_x0)
    right = (body_y0, body_x1)
    _draw_triangle(img, apex, left, right, _RED)
    # Door (small rect).
    door_x0 = w // 2 - w // 16
    door_x1 = w // 2 + w // 16
    door_y0 = body_y0 + (body_y1 - body_y0) // 3
    _draw_rect(img, door_y0, door_x0, body_y1, door_x1, _BLACK)
    return img


def _compose_tree_silhouette(h, w, c) -> np.ndarray:
    img = _blank(h, w, c, (220, 230, 240))
    # Ground.
    _draw_rect(img, 4 * h // 5, 0, h, w, _DARKGREEN)
    # Trunk.
    trunk_w = max(2, w // 16)
    cx = w // 2
    _draw_rect(img, h // 2, cx - trunk_w // 2, 4 * h // 5, cx + trunk_w // 2, _BROWN)
    # Foliage (large green disk).
    _draw_disk(img, h // 3, cx, min(h, w) // 4, _DARKGREEN)
    return img


def _compose_kandinsky_geometric(h, w, c) -> np.ndarray:
    img = _blank(h, w, c, _WHITE)
    rng = np.random.default_rng(_seed_for("kandinsky_geometric"))
    palette = [_RED, _BLUE, _YELLOW, _GREEN, _ORANGE, _PURPLE, _BLACK]
    # A handful of overlapping geometric shapes.
    n_shapes = 8
    for _ in range(n_shapes):
        kind = rng.integers(0, 3)
        col = palette[int(rng.integers(0, len(palette)))]
        if kind == 0:
            cy = int(rng.integers(h // 8, 7 * h // 8))
            cx = int(rng.integers(w // 8, 7 * w // 8))
            r = int(rng.integers(max(2, h // 16), max(3, h // 6)))
            _draw_disk(img, cy, cx, r, col)
        elif kind == 1:
            y0 = int(rng.integers(0, 3 * h // 4))
            x0 = int(rng.integers(0, 3 * w // 4))
            y1 = y0 + int(rng.integers(max(2, h // 16), max(3, h // 4)))
            x1 = x0 + int(rng.integers(max(2, w // 16), max(3, w // 4)))
            _draw_rect(img, y0, x0, y1, x1, col)
        else:
            y0 = int(rng.integers(0, h))
            x0 = int(rng.integers(0, w))
            y1 = int(rng.integers(0, h))
            x1 = int(rng.integers(0, w))
            _draw_line(img, y0, x0, y1, x1, col, thickness=max(1, h // 32))
    return img


def _compose_mondrian_grid(h, w, c) -> np.ndarray:
    img = _blank(h, w, c, _WHITE)
    # A few orthogonal black bars defining grid cells.
    bar = max(2, min(h, w) // 32)
    # Vertical bars at ~1/3 and ~2/3.
    _draw_rect(img, 0, w // 3 - bar // 2, h, w // 3 + bar // 2, _BLACK)
    _draw_rect(img, 0, 2 * w // 3 - bar // 2, h, 2 * w // 3 + bar // 2, _BLACK)
    # Horizontal bars at ~1/3 and ~2/3.
    _draw_rect(img, h // 3 - bar // 2, 0, h // 3 + bar // 2, w, _BLACK)
    _draw_rect(img, 2 * h // 3 - bar // 2, 0, 2 * h // 3 + bar // 2, w, _BLACK)
    # Three primary-colour cells (deterministic placement).
    # Top-left cell yellow.
    _draw_rect(img, 0, 0, h // 3 - bar // 2, w // 3 - bar // 2, _YELLOW)
    # Middle-right cell red.
    _draw_rect(img, h // 3 + bar // 2, 2 * w // 3 + bar // 2,
                    2 * h // 3 - bar // 2, w, _RED)
    # Bottom-left cell blue.
    _draw_rect(img, 2 * h // 3 + bar // 2, 0, h, w // 3 - bar // 2, _BLUE)
    return img


def _compose_color_field_gradient(h, w, c) -> np.ndarray:
    img = np.zeros((h, w, c), dtype=np.uint8)
    # Vertical gradient red → purple top-to-bottom; horizontal gradient
    # adds blue.
    yy, xx = np.mgrid[:h, :w].astype(np.float32)
    ty = yy / max(1, h - 1)
    tx = xx / max(1, w - 1)
    if c >= 3:
        img[..., 0] = np.clip(220 - 180 * ty, 0, 255).astype(np.uint8)
        img[..., 1] = np.clip(60 + 80 * (1 - tx), 0, 255).astype(np.uint8)
        img[..., 2] = np.clip(60 + 180 * tx, 0, 255).astype(np.uint8)
    return img


def _compose_pointillist_mosaic(h, w, c) -> np.ndarray:
    img = _blank(h, w, c, _WHITE)
    rng = np.random.default_rng(_seed_for("pointillist_mosaic"))
    palette = [_RED, _BLUE, _YELLOW, _GREEN, _ORANGE, _PURPLE]
    n_dots = max(40, (h * w) // 64)
    r = max(1, min(h, w) // 32)
    for _ in range(n_dots):
        cy = int(rng.integers(0, h))
        cx = int(rng.integers(0, w))
        col = palette[int(rng.integers(0, len(palette)))]
        _draw_disk(img, cy, cx, r, col)
    return img


def _compose_fragmented_symmetry(h, w, c) -> np.ndarray:
    img = _blank(h, w, c, _BLACK)
    rng = np.random.default_rng(_seed_for("fragmented_symmetry"))
    palette = [_RED, _BLUE, _YELLOW, _GREEN, _ORANGE, _PURPLE, _WHITE]
    # Generate shapes in the top-left quadrant; mirror to other quadrants
    # for 4-fold symmetry.
    n_shapes = 5
    for _ in range(n_shapes):
        ky = int(rng.integers(0, h // 2))
        kx = int(rng.integers(0, w // 2))
        kr = int(rng.integers(max(2, h // 16), max(3, h // 8)))
        col = palette[int(rng.integers(0, len(palette)))]
        # Mirror across both axes.
        for (cy, cx) in (
            (ky, kx),
            (ky, w - 1 - kx),
            (h - 1 - ky, kx),
            (h - 1 - ky, w - 1 - kx),
        ):
            _draw_disk(img, cy, cx, kr, col)
    return img


_COMPOSERS = {
    "single_red_circle":             _compose_single_red_circle,
    "single_blue_square":            _compose_single_blue_square,
    "horizontal_split_yellow_green": _compose_horizontal_split_yellow_green,
    "checkerboard_2x2":              _compose_checkerboard_2x2,
    "three_circles_triangle":        _compose_three_circles_triangle,
    "concentric_rings":              _compose_concentric_rings,
    "diagonal_stripes_warm":         _compose_diagonal_stripes_warm,
    "house_shape":                   _compose_house_shape,
    "tree_silhouette":               _compose_tree_silhouette,
    "kandinsky_geometric":           _compose_kandinsky_geometric,
    "mondrian_grid":                 _compose_mondrian_grid,
    "color_field_gradient":          _compose_color_field_gradient,
    "pointillist_mosaic":            _compose_pointillist_mosaic,
    "fragmented_symmetry":           _compose_fragmented_symmetry,
}


# ---------------------------------------------------------------------------
# Public: generate_image_array + generate_composition_description
# ---------------------------------------------------------------------------

def generate_image_array(composition_name: str, *,
                         height: int = 64, width: int = 64,
                         channels: int = 3) -> np.ndarray:
    """Procedurally render `composition_name` to an (H, W, C) uint8 array.

    Pure numpy. Deterministic — the same composition_name always produces
    identical pixels. No file I/O. Raises ValueError on unknown names.
    """
    fn = _COMPOSERS.get(composition_name)
    if fn is None:
        raise ValueError(f"unknown composition name: {composition_name!r}")
    if height <= 0 or width <= 0 or channels <= 0:
        raise ValueError("height/width/channels must all be positive")
    img = fn(int(height), int(width), int(channels))
    # Defensive: clamp + cast to make sure we're returning a uint8 in [0, 255].
    if img.dtype != np.uint8:
        img = np.clip(img, 0, 255).astype(np.uint8)
    return img


# ---------------------------------------------------------------------------
# Composition descriptions (verbal stand-in + scoring keywords)
# ---------------------------------------------------------------------------

# Each entry: (verbal description sentence, list of expected keywords).
# The description is what we hand to brain.train_language as the textual
# fallback; the keywords drive the response scoring (recall + specificity).
_DESCRIPTIONS: dict[str, tuple[str, list[str]]] = {
    "single_red_circle": (
        "A single red circle on a white background.",
        ["red", "circle", "white", "background"],
    ),
    "single_blue_square": (
        "A blue square centered on a white background.",
        ["blue", "square", "center", "white"],
    ),
    "horizontal_split_yellow_green": (
        "The image is split horizontally with yellow on top and green below.",
        ["yellow", "green", "above", "below", "split"],
    ),
    "checkerboard_2x2": (
        "A two by two checkerboard of black and white squares.",
        ["black", "white", "checkerboard", "square", "pattern"],
    ),
    "three_circles_triangle": (
        "Three circles arranged in a triangle: red on top, blue and green below.",
        ["three", "circle", "triangle", "red", "blue", "green"],
    ),
    "concentric_rings": (
        "Concentric colored rings expanding outward from the center.",
        ["concentric", "ring", "circle", "center", "rainbow"],
    ),
    "diagonal_stripes_warm": (
        "Diagonal stripes in warm colors of red orange and yellow.",
        ["diagonal", "stripe", "warm", "red", "orange", "yellow"],
    ),
    "house_shape": (
        "A simple house with a red triangular roof and a brown body on green ground under a blue sky.",
        ["house", "roof", "triangle", "brown", "red", "green", "blue", "sky"],
    ),
    "tree_silhouette": (
        "A tree silhouette with a brown trunk and a green leafy crown above the ground.",
        ["tree", "trunk", "brown", "green", "leaf", "above"],
    ),
    "kandinsky_geometric": (
        "An abstract Kandinsky-style composition of overlapping circles rectangles and lines in many colors.",
        ["abstract", "geometric", "circle", "rectangle", "line", "color"],
    ),
    "mondrian_grid": (
        "A Mondrian-style grid of black lines dividing white red yellow and blue rectangles.",
        ["mondrian", "grid", "black", "white", "red", "yellow", "blue", "rectangle"],
    ),
    "color_field_gradient": (
        "A smooth color field gradient transitioning from red through purple into blue.",
        ["gradient", "color", "smooth", "red", "blue", "purple"],
    ),
    "pointillist_mosaic": (
        "A pointillist mosaic of many small colored dots scattered across the canvas.",
        ["pointillist", "mosaic", "dot", "small", "scatter", "color"],
    ),
    "fragmented_symmetry": (
        "A fragmented symmetric pattern with mirrored colored shapes on a black background.",
        ["symmetric", "mirror", "fragment", "pattern", "black", "color"],
    ),
}


def generate_composition_description(composition_name: str, stage: int) -> str:
    """Return a one-sentence verbal description of `composition_name`.

    `stage` is accepted for API symmetry with the storytelling drip and
    for future stage-conditioned phrasing; current implementation returns
    the same description string regardless of stage. Raises ValueError on
    unknown names.
    """
    entry = _DESCRIPTIONS.get(composition_name)
    if entry is None:
        raise ValueError(f"unknown composition name: {composition_name!r}")
    desc, _kw = entry
    return desc


def _expected_keywords_for(composition_name: str) -> list[str]:
    entry = _DESCRIPTIONS.get(composition_name)
    if entry is None:
        return []
    return list(entry[1])


# ---------------------------------------------------------------------------
# Scoring
# ---------------------------------------------------------------------------

_TOK_RE = re.compile(r"[A-Za-z']+")


def _tokenize(text: str) -> list[str]:
    if not text:
        return []
    return [m.group(0).lower() for m in _TOK_RE.finditer(text)]


def score_visual_response(brain_response_text: str,
                          expected_keywords: Iterable[str]) -> dict:
    """Score a brain text response against expected visual keywords.

    Returns a dict with:
      - recall:      fraction of expected keywords present in the response
      - specificity: fraction of response tokens that are expected
                     keywords (penalises blob/mode-collapse output that
                     happens to contain a keyword among many off-topic ones)
      - composite:   (2 * recall + specificity) / 3
      - tokens:      number of tokens in the response

    Empty response → all components 0. Empty expected_keywords → 0
    (we can't meaningfully score against nothing).
    """
    keywords = {k.lower() for k in expected_keywords if isinstance(k, str)}
    tokens = _tokenize(brain_response_text)
    if not keywords or not tokens:
        return {
            "recall": 0.0,
            "specificity": 0.0,
            "composite": 0.0,
            "tokens": len(tokens),
        }
    token_set = set(tokens)
    hits = sum(1 for k in keywords if k in token_set)
    recall = hits / float(len(keywords))
    # Specificity: how much of the response is on-topic. Use token COUNT
    # (not set) so that "red red red" doesn't get inflated specificity.
    on_topic_count = sum(1 for t in tokens if t in keywords)
    specificity = on_topic_count / float(len(tokens))
    composite = (2.0 * recall + specificity) / 3.0
    return {
        "recall": float(recall),
        "specificity": float(specificity),
        "composite": float(composite),
        "tokens": len(tokens),
    }


# ---------------------------------------------------------------------------
# Drip driver
# ---------------------------------------------------------------------------

# Per-stage rotation counters (in-process; not checkpointed).
_ROTATION_COUNTER: dict[int, int] = {1: 0, 2: 0, 3: 0}


def _bump_rotation(stage: int) -> int:
    cur = _ROTATION_COUNTER.get(int(stage), 0)
    _ROTATION_COUNTER[int(stage)] = cur + 1
    return cur


def _intent_from_text(text: str, composer) -> list[float] | None:
    """Build the intent vector for produce_text from a description.

    Same pattern as storytelling: prefer the supplied Composer; fall back
    to a 1024-d zero vector. Returns a plain Python list (or None on
    catastrophic failure)."""
    intent = None
    if composer is not None:
        try:
            intent = composer.compose(text=text, modality="text")
        except Exception:
            intent = None
    if intent is None:
        try:
            intent = np.zeros(1024, dtype=np.float32)
        except Exception:
            return None
    try:
        return intent.tolist() if hasattr(intent, "tolist") else list(intent)
    except Exception:
        return None


def _push_visual(brain, image: np.ndarray, description: str) -> str:
    """Pipe `image` into the brain's visual cortex if the RPC is available;
    otherwise fall back to train_language(description, description).

    Returns the path actually used: "visual" or "language".
    """
    visual_fn = getattr(brain, "visual_cortex_process", None)
    if callable(visual_fn):
        h, w = image.shape[:2]
        c = image.shape[2] if image.ndim >= 3 else 1
        # Flatten to a list of pixel values; the daemon RPC expects a flat
        # list + dimensions.
        try:
            visual_fn(image.flatten().tolist(), int(w), int(h), int(c))
            return "visual"
        except TypeError:
            # Some bindings prefer keyword args.
            try:
                visual_fn(pixels=image.flatten().tolist(),
                          width=int(w), height=int(h), channels=int(c))
                return "visual"
            except Exception:
                pass
        except Exception:
            pass
    # Fallback path.
    try:
        try:
            brain.train_language(description, description)
        except TypeError:
            brain.train_language(text=description, target_text=description)
    except Exception:
        pass
    return "language"


def run_visual_drip(brain, stage: int, *,
                    composer=None,
                    num_compositions: int = 2,
                    composition_index: int | None = None,
                    log_every: int = 10,
                    ) -> list[dict] | None:
    """Run a visual-art drip of `num_compositions` compositions.

    Returns a list of per-composition score dicts (one per composition),
    or None if the stage has no compositions. Stage 1 IS active — early
    Athena needs the simplest 4-composition palette.

    For each composition we:
      1. Generate the image array (procedural, no disk I/O).
      2. Generate the verbal description.
      3. Push to visual cortex (or fall back to train_language).
      4. Probe with produce_text.
      5. Score the response.
      6. Apply the feedback signal:
            composite >= VISUAL_PASS_THRESHOLD → learn_language(produced)
            composite <  VISUAL_PASS_THRESHOLD → train_language(desc, desc)
      7. try/except around brain calls — log + continue.

    `composition_index` overrides the rotation counter (for tests +
    deterministic replays).
    """
    palette = ART_COMPOSITIONS.get(int(stage))
    if not palette:
        return None

    n = max(1, int(num_compositions))
    results: list[dict] = []

    # Decide rotation: explicit index → use as-is, do not bump counter.
    if composition_index is not None:
        start = int(composition_index)
        bump = False
    else:
        start = _bump_rotation(int(stage))
        bump = True

    for k in range(n):
        idx = (start + k) % len(palette)
        composition_name = palette[idx]

        # 1. Image.
        try:
            image = generate_image_array(composition_name)
        except Exception as e:
            print(f"  [Visual:s{stage}] generate_image_array({composition_name}) "
                  f"failed: {e}")
            continue

        # 2. Description.
        try:
            description = generate_composition_description(composition_name, int(stage))
        except Exception as e:
            print(f"  [Visual:s{stage}] description({composition_name}) failed: {e}")
            continue

        # 3. Push into the brain (visual RPC if available; else language).
        try:
            path_used = _push_visual(brain, image, description)
        except Exception as e:
            print(f"  [Visual:s{stage}] visual push failed: {e}")
            path_used = "none"

        # 4. Probe.
        intent = _intent_from_text(description, composer)
        if intent is None:
            continue

        try:
            response = brain.produce_text(intent)
        except Exception as e:
            print(f"  [Visual:s{stage}] produce_text failed: {e}")
            continue

        produced = (response or {}).get("text", "") if isinstance(response, dict) else ""
        confidence = (response or {}).get("confidence", 0.0) \
            if isinstance(response, dict) else 0.0

        # 5. Score.
        keywords = _expected_keywords_for(composition_name)
        score = score_visual_response(produced, keywords)

        # 6. Feedback.
        try:
            if score["composite"] >= VISUAL_PASS_THRESHOLD and produced.strip():
                brain.learn_language(produced)
            else:
                try:
                    brain.train_language(description, description)
                except TypeError:
                    brain.train_language(text=description, target_text=description)
        except Exception as e:
            print(f"  [Visual:s{stage}] feedback signal failed: {e}")

        if int(start + k) % max(1, int(log_every)) == 0:
            print(f"  [Visual:s{stage}] {composition_name} "
                  f"composite={score['composite']:.2f} "
                  f"recall={score['recall']:.2f} "
                  f"spec={score['specificity']:.2f} "
                  f"tok={score['tokens']} conf={confidence:.2f} "
                  f"path={path_used}")

        results.append({
            "stage": int(stage),
            "rotation": int(start + k),
            "composition": composition_name,
            "description": description,
            "produced": produced,
            "confidence": float(confidence),
            "path": path_used,
            **score,
        })

    return results
