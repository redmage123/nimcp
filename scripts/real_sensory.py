"""Real sensory stimuli for the developmental curriculum.

Replaces the keyword-seeded synthetic visual frames (immerse_athena's
generate_visual_frame) with real photographs when the description names an
object we have images for, and the synthesized speech channel with real
spoken-word recordings when the description contains a word we have clips
for. Everything else falls back to the synthetic generators, so coverage can
grow without changing the trainer.

Data (built by scripts/build_sensory_caches.py from the original archives):
    <data_dir>/cifar_by_label.pkl          {label: [32x32x3 uint8 bytes, ...]}  train split
    <data_dir>/cifar_heldout_by_label.pkl  same layout, test split (evaluation only)
    <data_dir>/speech_cmds_by_label.pkl    {word: [float32 16 kHz samples, ...]}
    <data_dir>/video_sequences.pkl         32x32 film shots at 2 fps (stage 0)
    <data_dir>/video_sequences_hires.pkl   128x128 JPEG film shots at 4 fps (stage 1+)
    <data_dir>/photos_hires_by_label.pkl   {label: [128x128 JPEG bytes, ...]}  (stage 1+)
    <data_dir>/photos_hires_heldout_by_label.pkl  same layout, evaluation only

Visual acuity grows with the stages (STAGE_ACUITY): 32x32 at stage 0, 64x64
at stages 1-2, 128x128 from stage 3, like an infant's sharpening vision.
Every frame the bank returns is at the current stage's resolution; sources
at another size are resized (high-resolution sources first when present).

Sources and licenses: CIFAR-10/100 (Krizhevsky, Toronto; freely redistributed
for research and widely used commercially), Google Speech Commands v0.02
(CC BY 4.0). No non-commercial-only data is used here.

Biological rationale: a child never sees the same canonical rendering twice.
Every presentation is re-augmented (flip, shift, brightness/contrast, colour
jitter) so the cortex has viewpoint/lighting variation to become invariant
to, and views_per_object lets the trainer present successive augmented views
of one object on consecutive steps (temporal continuity).
"""
from __future__ import annotations

import os
import pickle
import random
import re
from typing import Optional

import numpy as np

# ---------------------------------------------------------------------------
# Vocabulary → image label mapping.
# CIFAR-100 fine labels + CIFAR-10 labels (airplane, automobile, bird, cat,
# deer, dog, frog, horse, ship, truck). Values are candidate labels; a random
# candidate is chosen per presentation so "tree" shows all five tree classes.
# ---------------------------------------------------------------------------
SYNONYMS: dict[str, list[str]] = {
    # people
    "mother": ["woman"], "mom": ["woman"], "mommy": ["woman"], "mum": ["woman"],
    "woman": ["woman"], "lady": ["woman"], "girl": ["girl"], "daughter": ["girl"],
    "father": ["man"], "dad": ["man"], "daddy": ["man"], "man": ["man"],
    "boy": ["boy"], "son": ["boy"], "child": ["boy", "girl"], "kid": ["boy", "girl"],
    "children": ["boy", "girl"], "baby": ["baby"], "infant": ["baby"], "toddler": ["baby"],
    "people": ["man", "woman", "boy", "girl"], "person": ["man", "woman"],
    # animals
    "dog": ["dog"], "puppy": ["dog"], "pup": ["dog"], "hound": ["dog"],
    "cat": ["cat"], "kitten": ["cat"], "kitty": ["cat"],
    "bird": ["bird"], "sparrow": ["bird"], "robin": ["bird"],
    "fish": ["aquarium_fish", "trout", "flatfish"], "trout": ["trout"],
    "horse": ["horse"], "pony": ["horse"], "foal": ["horse"],
    "butterfly": ["butterfly"], "rabbit": ["rabbit"], "bunny": ["rabbit"],
    "bear": ["bear"], "dolphin": ["dolphin"], "elephant": ["elephant"],
    "bee": ["bee"], "beetle": ["beetle"], "bug": ["beetle", "cockroach"],
    "insect": ["bee", "beetle", "butterfly"], "spider": ["spider"], "snail": ["snail"],
    "worm": ["worm"], "caterpillar": ["caterpillar"], "cockroach": ["cockroach"],
    "deer": ["deer"], "frog": ["frog"], "lizard": ["lizard"], "snake": ["snake"],
    "turtle": ["turtle"], "crocodile": ["crocodile"], "dinosaur": ["dinosaur"],
    "whale": ["whale"], "shark": ["shark"], "seal": ["seal"], "otter": ["otter"],
    "crab": ["crab"], "lobster": ["lobster"], "ray": ["ray"],
    "lion": ["lion"], "tiger": ["tiger"], "leopard": ["leopard"], "wolf": ["wolf"],
    "fox": ["fox"], "raccoon": ["raccoon"], "skunk": ["skunk"], "squirrel": ["squirrel"],
    "mouse": ["mouse"], "hamster": ["hamster"], "shrew": ["shrew"], "possum": ["possum"],
    "porcupine": ["porcupine"], "beaver": ["beaver"], "kangaroo": ["kangaroo"],
    "camel": ["camel"], "cow": ["cattle"], "cattle": ["cattle"], "bull": ["cattle"],
    "monkey": ["chimpanzee"], "ape": ["chimpanzee"], "chimpanzee": ["chimpanzee"],
    "owl": ["bird"], "duck": ["bird"], "chicken": ["bird"], "eagle": ["bird"],
    # plants / nature
    "tree": ["oak_tree", "maple_tree", "pine_tree", "palm_tree", "willow_tree"],
    "oak": ["oak_tree"], "maple": ["maple_tree"], "pine": ["pine_tree"], "palm": ["palm_tree"],
    "willow": ["willow_tree"], "forest": ["forest"], "woods": ["forest"], "jungle": ["forest"],
    "flower": ["rose", "tulip", "orchid", "poppy", "sunflower"], "rose": ["rose"],
    "tulip": ["tulip"], "orchid": ["orchid"], "poppy": ["poppy"], "sunflower": ["sunflower"],
    "mushroom": ["mushroom"], "grass": ["plain"], "field": ["plain"], "meadow": ["plain"],
    "plain": ["plain"], "mountain": ["mountain"], "hill": ["mountain"],
    "cloud": ["cloud"], "sky": ["cloud"], "sea": ["sea"], "ocean": ["sea"],
    "wave": ["sea"], "beach": ["sea"], "water": ["sea"], "lake": ["sea"],
    "apple": ["apple"], "orange": ["orange"], "pear": ["pear"], "pepper": ["sweet_pepper"],
    # objects / places
    "house": ["house"], "home": ["house"], "cottage": ["house"], "castle": ["castle"],
    "building": ["skyscraper"], "skyscraper": ["skyscraper"], "tower": ["skyscraper"],
    "bridge": ["bridge"], "road": ["road"], "street": ["road"], "path": ["road"],
    "car": ["automobile"], "automobile": ["automobile"], "truck": ["truck", "pickup_truck"],
    "bus": ["bus"], "train": ["train"], "tram": ["streetcar"], "streetcar": ["streetcar"],
    "tractor": ["tractor"], "motorcycle": ["motorcycle"], "bike": ["bicycle"],
    "bicycle": ["bicycle"], "boat": ["ship"], "ship": ["ship"], "rocket": ["rocket"],
    "plane": ["airplane"], "airplane": ["airplane"], "aeroplane": ["airplane"], "tank": ["tank"],
    "mower": ["lawn_mower"], "bed": ["bed"], "chair": ["chair"], "couch": ["couch"],
    "sofa": ["couch"], "table": ["table"], "wardrobe": ["wardrobe"], "closet": ["wardrobe"],
    "lamp": ["lamp"], "light": ["lamp"], "clock": ["clock"], "telephone": ["telephone"],
    "phone": ["telephone"], "television": ["television"], "tv": ["television"],
    "keyboard": ["keyboard"], "bottle": ["bottle"], "bowl": ["bowl"], "cup": ["cup"],
    "mug": ["cup"], "plate": ["plate"], "can": ["can"],
}

_WORD_RE = re.compile(r"[a-z]+")


def _candidate_words(description: str) -> list[str]:
    """Lowercase tokens plus naive singular forms, in order of appearance."""
    out: list[str] = []
    for w in _WORD_RE.findall(description.lower()):
        out.append(w)
        if len(w) > 3 and w.endswith("ies"):
            out.append(w[:-3] + "y")
        elif len(w) > 3 and w.endswith("es"):
            out.append(w[:-2])
        if len(w) > 3 and w.endswith("s"):
            out.append(w[:-1])
    return out


def _decode_frame(raw) -> np.ndarray:
    """HxWx3 uint8 array from a stored frame: JPEG bytes or raw square RGB."""
    if isinstance(raw, (bytes, bytearray)) and raw[:2] == b"\xff\xd8":
        import io
        from PIL import Image
        return np.asarray(Image.open(io.BytesIO(raw)).convert("RGB"), dtype=np.uint8)
    arr = np.frombuffer(raw, dtype=np.uint8)
    side = int(round((arr.size // 3) ** 0.5))
    if side * side * 3 != arr.size:
        raise ValueError(f"frame of {arr.size} bytes is not a square RGB image")
    return arr.reshape(side, side, 3)


def _resize(img: np.ndarray, size: int) -> np.ndarray:
    """Square resize: area averaging down, bilinear up (PIL); numpy fallback."""
    h, w = img.shape[:2]
    if h == size and w == size:
        return img
    try:
        from PIL import Image
        resample = Image.BOX if size < min(h, w) else Image.BILINEAR
        return np.asarray(Image.fromarray(img).resize((size, size), resample), dtype=np.uint8)
    except ImportError:
        if h % size == 0 and w % size == 0:
            fy, fx = h // size, w // size
            return img.reshape(size, fy, size, fx, 3).mean(axis=(1, 3)).astype(np.uint8)
        ys = (np.arange(size) * h // size).astype(int)
        xs = (np.arange(size) * w // size).astype(int)
        return img[ys][:, xs]


def read_allowlist_levels(path: str) -> dict:
    """{identifier: maturity level} from a film allowlist.

    Line format: `identifier [level]  # comment`. A missing or unparsable
    level gets RealSensoryBank.UNTAGGED_MATURITY. Returns {} when the file
    is absent."""
    levels = {}
    if not os.path.exists(path):
        return levels
    with open(path) as fh:
        for line in fh:
            parts = line.split("#", 1)[0].split()
            if not parts or parts[0] in levels:
                continue
            try:
                lvl = int(parts[1]) if len(parts) > 1 else RealSensoryBank.UNTAGGED_MATURITY
            except ValueError:
                lvl = RealSensoryBank.UNTAGGED_MATURITY
            if lvl not in RealSensoryBank.MATURITY_LEVELS:
                lvl = RealSensoryBank.UNTAGGED_MATURITY
            levels[parts[0]] = lvl
    return levels


class RealSensoryBank:
    """Lazy-loading bank of real images and speech clips keyed by label."""

    def __init__(self, data_dir: str, seed: Optional[int] = None,
                 augment: bool = True):
        self.data_dir = data_dir
        self.augment = augment
        self.rng = random.Random(seed)
        self.nrng = np.random.RandomState(seed)
        self._images: Optional[dict[str, list[bytes]]] = None
        self._heldout: Optional[dict[str, list[bytes]]] = None
        self._speech: Optional[dict[str, list[np.ndarray]]] = None
        self._sequences: Optional[list[dict]] = None
        self._all_sequences: Optional[list[dict]] = None
        self.max_maturity = 0               # highest film maturity level played (set_stage)
        self.stage = 0
        self.resolution, self.video_fps = self.STAGE_ACUITY[0]
        self._hires_images: Optional[dict] = None
        self._hires_heldout: Optional[dict] = None
        self._queued_frames: list[bytes] = []   # frames to submit on the next presentations
        self.hits = 0
        self.misses = 0
        self.speech_hits = 0
        self.sequence_hits = 0
        self.last_label: Optional[str] = None
        self.last_hit = False

    # ---- loading -----------------------------------------------------------
    def _load_pkl(self, name: str):
        path = os.path.join(self.data_dir, name)
        if not os.path.exists(path):
            return None
        with open(path, "rb") as f:
            return pickle.load(f)

    @property
    def images(self) -> dict[str, list[bytes]]:
        if self._images is None:
            self._images = self._load_pkl("cifar_by_label.pkl") or {}
        return self._images

    @property
    def heldout(self) -> dict[str, list[bytes]]:
        if self._heldout is None:
            self._heldout = self._load_pkl("cifar_heldout_by_label.pkl") or {}
        return self._heldout

    @property
    def speech(self) -> dict[str, list[np.ndarray]]:
        if self._speech is None:
            self._speech = self._load_pkl("speech_cmds_by_label.pkl") or {}
        return self._speech

    # Films held out of training for the video probe: neutral everyday-life
    # footage, named explicitly (a hash split picked one unsuitable film).
    # If none of these are in the cache, fall back to ~1 in 20 by crc32.
    HELDOUT_FILMS = {"PrivateL1947", "TexasFar1952", "ParkCons1938"}
    HELDOUT_FILM_MOD = 20
    # Excluded from training and probes (2026-11-02): the popularity-ranked
    # Prelinger pull included adult/burlesque reels, propaganda and graphic
    # material unsuitable for a developmental curriculum. Interim list until
    # the cache is rebuilt from a curated allowlist.
    EXCLUDED_FILMS = {
        "stripper_variety_girls", "CherryKnight", "MalibuMermai",       # adult / pin-up
        "boys_beware", "Japanese1943", "Communis1952", "DontBeaS1947",  # propaganda
        "Experime1940", "AtomBomb1946", "hindenberg_explodes",          # graphic / disturbing
        "DuckandC1951", "Physical1953", "naturally_a_girl",
        "DatingDo1949",
    }

    def _is_heldout_film(self, item, present) -> bool:
        if present & self.HELDOUT_FILMS:
            return item in self.HELDOUT_FILMS
        import zlib
        return zlib.crc32(str(item).encode()) % self.HELDOUT_FILM_MOD == 0

    # Film maturity levels: the second column of scripts/video_allowlist.txt.
    # Each stage plays films up to its cap (STAGE_MAX_MATURITY). Explicit
    # sexual content has no level: it is never listed and EXCLUDED_FILMS
    # backstops the old popularity cache.
    MATURITY_LEVELS = {
        0: "everyday life: animals, nature, farms, transport, towns, play",
        1: "real-world complexity: work and industry, illness, natural hazards",
        2: "difficult history: war, propaganda, disasters (needs framing episodes)",
    }
    UNTAGGED_MATURITY = 2            # a film listed without a level unlocks last
    # Level 2 stays locked until stages 6-7 have episodes that frame such
    # films (bias, harm, context); raise those caps to 2 when they exist.
    STAGE_MAX_MATURITY = {0: 0, 1: 0, 2: 0, 3: 0, 4: 1, 5: 1, 6: 1, 7: 1}
    ALLOWLIST_PATH = os.path.join(os.path.dirname(os.path.abspath(__file__)),
                                  "video_allowlist.txt")

    # Visual acuity per stage: (frame side in pixels, video frames per second).
    # Newborn acuity is ~1/20 of adult and sharpens over the first year;
    # networks trained blurry-then-sharp learn more robust features
    # (Vogelsang et al., PNAS 2018). Stage 0 keeps the original 32x32 / 2 fps.
    STAGE_ACUITY = {0: (32, 2.0), 1: (64, 4.0), 2: (64, 4.0), 3: (128, 4.0),
                    4: (128, 4.0), 5: (128, 4.0), 6: (128, 4.0), 7: (128, 4.0)}
    BASE_RESOLUTION = 32                # CIFAR and the stage-0 video cache

    def set_stage(self, stage: int) -> int:
        """Apply a curriculum stage's film-maturity cap and visual acuity.
        Returns the maturity cap."""
        stage = int(stage)
        self.stage = stage
        cap = self.STAGE_MAX_MATURITY.get(stage, max(self.STAGE_MAX_MATURITY.values()))
        acuity = self.STAGE_ACUITY.get(stage, self.STAGE_ACUITY[max(self.STAGE_ACUITY)])
        if cap != self.max_maturity:
            self.max_maturity = cap
            self._sequences = None      # re-filter on next access
        if acuity != (self.resolution, self.video_fps):
            old_src = self._video_cache_name()
            self.resolution, self.video_fps = acuity
            if self._video_cache_name() != old_src:
                self._all_sequences = None   # switch video source
            self._sequences = None
        return cap

    # ---- acuity ------------------------------------------------------------
    @property
    def hires_images(self) -> dict:
        if self._hires_images is None:
            self._hires_images = self._load_pkl("photos_hires_by_label.pkl") or {}
        return self._hires_images

    @property
    def hires_heldout(self) -> dict:
        if self._hires_heldout is None:
            self._hires_heldout = self._load_pkl("photos_hires_heldout_by_label.pkl") or {}
        return self._hires_heldout

    def _video_cache_name(self) -> str:
        """Video source for the current acuity: the high-resolution cache once
        acuity exceeds 32x32 (when it exists), else the 32x32 cache."""
        if self.resolution > self.BASE_RESOLUTION and \
                os.path.exists(os.path.join(self.data_dir, "video_sequences_hires.pkl")):
            return "video_sequences_hires.pkl"
        return "video_sequences.pkl"

    def to_stage_frame(self, raw) -> bytes:
        """A stored frame (JPEG or raw RGB, any size) as raw RGB bytes at the
        current resolution. Raw frames already at that size pass through."""
        n = self.resolution * self.resolution * 3
        if isinstance(raw, (bytes, bytearray)) and len(raw) == n and raw[:2] != b"\xff\xd8":
            return bytes(raw)
        return _resize(_decode_frame(raw), self.resolution).tobytes()

    def _fps_view(self, seq: dict) -> dict:
        """A sequence at the current frame rate: frames dropped evenly when
        the source was recorded faster (shallow copy; frames are shared)."""
        src_fps = float(seq.get("fps", 2.0))
        step = int(round(src_fps / self.video_fps)) if self.video_fps > 0 else 1
        if step <= 1:
            return seq
        view = dict(seq)
        view["frames"] = list(seq["frames"])[::step]
        view["fps"] = src_fps / step
        return view

    def _load_all_sequences(self):
        if self._all_sequences is not None:
            return
        allseq = [q for q in (self._load_pkl(self._video_cache_name()) or [])
                  if q.get("item") not in self.EXCLUDED_FILMS]
        levels = read_allowlist_levels(self.ALLOWLIST_PATH)
        if levels:
            # The allowlist is authoritative: unlisted films never play, and
            # a film's level comes from the list, so retagging needs no rebuild.
            allseq = [q for q in allseq if q.get("item") in levels]
            for q in allseq:
                q["maturity"] = levels[q["item"]]
        else:
            for q in allseq:
                q.setdefault("maturity", self.UNTAGGED_MATURITY)
        self._all_sequences = allseq

    def _split_sequences(self):
        if self._sequences is None:
            self._load_all_sequences()
            allseq = self._all_sequences
            present = {q.get("item") for q in allseq}
            playable = [q for q in allseq if q.get("maturity", self.UNTAGGED_MATURITY) <= self.max_maturity]
            playable = [self._fps_view(q) for q in playable]
            self._sequences = [q for q in playable if not self._is_heldout_film(q.get("item"), present)]
            self._heldout_sequences = [q for q in playable if self._is_heldout_film(q.get("item"), present)]

    @property
    def sequences(self) -> list[dict]:
        """Training motion sequences (scripts/build_video_cache.py), held-out
        films excluded."""
        self._split_sequences()
        return self._sequences

    @property
    def heldout_sequences(self) -> list[dict]:
        """Sequences from held-out films (never used for training)."""
        self._split_sequences()
        return self._heldout_sequences

    def available(self) -> bool:
        return bool(self.images) or bool(self.sequences)

    # ---- motion sequences -------------------------------------------------
    def get_sequence(self) -> Optional[dict]:
        """A random sequence dict ({"frames": [bytes...], "description", ...})
        or None when no video cache is present."""
        seqs = self.sequences
        if not seqs:
            return None
        self.sequence_hits += 1
        return self.rng.choice(seqs)

    def get_film(self, n_sequences: int = 4) -> list[dict]:
        """Up to n consecutive sequences of one film (ordered by start time),
        for 'watching' a longer stretch. Empty list without a cache."""
        seqs = self.sequences
        if not seqs:
            return []
        item = self.rng.choice(seqs)["item"]
        run = sorted((s for s in seqs if s["item"] == item), key=lambda s: s.get("start_s", 0.0))
        if len(run) <= n_sequences:
            return run
        start = self.rng.randint(0, len(run) - n_sequences)
        return run[start:start + n_sequences]

    @staticmethod
    def sequence_text(seq: dict) -> str:
        """Best available ground-truth text for a sequence: the archive's
        synopsis when present, else the generic description."""
        s = (seq.get("summary") or "").strip()
        return s if len(s) > 20 else seq.get("description", "a moving scene")

    def queue_frames(self, frames: list[bytes], label: Optional[str] = None):
        """Frames to be submitted, in order, by the next presentations
        (pop_frame). Used to play a sequence across consecutive steps.
        `label` names what the frames show (e.g. the film's title); it
        becomes the visual cortex's training target for those frames."""
        self._queued_frames = list(frames)
        self._queued_label = label or "a moving scene"

    def pop_frame(self):
        """Next queued frame as (bytes, res, res, 3, label) at the current
        acuity, or None."""
        if not self._queued_frames:
            return None
        raw = self.to_stage_frame(self._queued_frames.pop(0))
        self.last_hit = True
        self.last_label = getattr(self, "_queued_label", "a moving scene")
        return raw, self.resolution, self.resolution, 3, self.last_label

    @staticmethod
    def sequence_label(seq: dict) -> str:
        """What a sequence shows, as a short phrase for the visual target."""
        t = (seq.get("title") or "").strip()
        return f"a scene from the film {t}" if t else "a moving scene"

    # ---- matching ----------------------------------------------------------
    def match_label(self, description: str) -> Optional[str]:
        """First vocabulary word in the description with images; random
        candidate label among its synonyms."""
        imgs = self.images
        if self.resolution > self.BASE_RESOLUTION and self.hires_images:
            imgs = {**imgs, **self.hires_images}
        if not imgs:
            return None
        for w in _candidate_words(description):
            cands = SYNONYMS.get(w)
            if cands is None and w in imgs:
                cands = [w]
            if not cands:
                continue
            cands = [c for c in cands if c in imgs]
            if cands:
                return self.rng.choice(cands)
        return None

    # ---- augmentation ------------------------------------------------------
    def augment_image(self, raw: bytes, w: int = 32, h: int = 32, ch: int = 3) -> bytes:
        img = np.frombuffer(raw, dtype=np.uint8).reshape(h, w, ch).astype(np.float32)
        r = self.nrng
        if r.rand() < 0.5:
            img = img[:, ::-1, :]
        # random shift up to 1/8 of the side with reflect padding (random crop)
        pad = max(1, w // 8)
        dy, dx = r.randint(-pad, pad + 1), r.randint(-pad, pad + 1)
        padded = np.pad(img, ((pad, pad), (pad, pad), (0, 0)), mode="reflect")
        img = padded[pad + dy:pad + dy + h, pad + dx:pad + dx + w, :]
        # brightness / contrast
        img = img * r.uniform(0.8, 1.2)
        mean = img.mean()
        img = (img - mean) * r.uniform(0.8, 1.2) + mean
        # per-channel colour jitter
        img = img * r.uniform(0.9, 1.1, size=(1, 1, ch))
        return np.clip(img, 0, 255).astype(np.uint8).tobytes()

    # ---- public API --------------------------------------------------------
    def get_frame(self, description: str):
        """(bytes, w, h, ch, label) for a real, freshly augmented image matching
        the description, or None (caller falls back to the synthetic frame)."""
        label = self.match_label(description)
        self.last_hit = label is not None
        self.last_label = label
        if label is None:
            self.misses += 1
            return None
        res = self.resolution
        if res > self.BASE_RESOLUTION and self.hires_images.get(label):
            raw = self.rng.choice(self.hires_images[label])     # sharp source
        else:
            raw = self.rng.choice(self.images[label])
        raw = self.to_stage_frame(raw)
        if self.augment:
            raw = self.augment_image(raw, res, res, 3)
        self.hits += 1
        return raw, res, res, 3, label

    def get_heldout_frame(self, label_or_word: str, index: Optional[int] = None):
        """Un-augmented held-out (test split) image for evaluation, or None."""
        sources = [self.heldout]
        if self.resolution > self.BASE_RESOLUTION and self.hires_heldout:
            sources.insert(0, self.hires_heldout)              # sharp source first
        for ho in sources:
            if not ho:
                continue
            label = label_or_word if label_or_word in ho else None
            if label is None:
                for c in SYNONYMS.get(label_or_word, []):
                    if c in ho:
                        label = c
                        break
            if label is None:
                continue
            clips = ho[label]
            raw = clips[index % len(clips)] if index is not None else self.rng.choice(clips)
            res = self.resolution
            return self.to_stage_frame(raw), res, res, 3, label
        return None

    def heldout_labels(self) -> list[str]:
        return sorted(self.heldout.keys())

    def get_speech(self, description: str, n_samples: int = 512):
        """(samples list, word) for a real spoken-word clip whose word appears
        in the description, or None.

        The speech cortex consumes a fixed 512-sample (32 ms at 16 kHz)
        waveform — the same shape the synthetic generator produces — so the
        clip's highest-energy 512-sample window is returned (the loudest
        part of the word, with its real spectral content). Clips are stored
        as float32 bytes so the cache is independent of the numpy version
        that built it."""
        sp = self.speech
        if not sp:
            return None
        for w in _candidate_words(description):
            if w in sp and sp[w]:
                self.speech_hits += 1
                raw = self.rng.choice(sp[w])
                clip = np.frombuffer(raw, dtype=np.float32) if isinstance(raw, (bytes, bytearray)) \
                    else np.asarray(raw, dtype=np.float32)
                if len(clip) <= n_samples:
                    out = np.zeros(n_samples, dtype=np.float32)
                    out[:len(clip)] = clip
                    return out.tolist(), w
                # highest-energy window (hop 64)
                energy = np.convolve(clip * clip, np.ones(n_samples, dtype=np.float32), mode="valid")
                start = int(np.argmax(energy[::64]) * 64)
                return clip[start:start + n_samples].tolist(), w
        return None

    def stats_str(self) -> str:
        tot = self.hits + self.misses
        rate = (self.hits / tot * 100.0) if tot else 0.0
        return (f"real-images: {self.hits}/{tot} matched ({rate:.0f}%), "
                f"{len(self.images)} labels; real-speech clips: {self.speech_hits}; "
                f"motion sequences: {self.sequence_hits} ({len(self.sequences)} available); "
                f"acuity {self.resolution}x{self.resolution} @ {self.video_fps:g} fps")
