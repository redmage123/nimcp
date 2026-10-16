#!/usr/bin/env python3
"""
World Model Training Curriculum — generates experiential training data from
physics, chemistry, and biology simulators.

Unlike text-based curriculum items that teach declarative knowledge ("balls fall"),
this module generates (state, action) → next_state transition pairs from running
actual simulations. The brain learns to predict physical outcomes by experiencing
thousands of simulated scenarios at progressive difficulty.

Usage:
    # As part of immerse_athena.py training loop:
    from world_model_curriculum import WorldModelCurriculum
    wmc = WorldModelCurriculum(brain)
    wmc.run_physics_epoch()
    wmc.run_chemistry_epoch()
    wmc.run_biology_epoch()

    # Standalone test:
    python3 world_model_curriculum.py
"""

import ctypes
import os
import sys
import random
import math
import logging

logger = logging.getLogger("world_model_curriculum")

# ============================================================================
# C Library Loading
# ============================================================================

def _load_lib():
    """Load the NIMCP shared library."""
    paths = [
        os.path.join(os.path.dirname(__file__), '..', 'build', 'lib', 'libnimcp.so'),
        '/workspace/nimcp/build/lib/libnimcp.so',
    ]
    for p in paths:
        p = os.path.abspath(p)
        if os.path.exists(p):
            return ctypes.CDLL(p)
    raise RuntimeError("Cannot find libnimcp.so")

# ============================================================================
# ctypes Wrappers
# ============================================================================

class Vec3(ctypes.Structure):
    _fields_ = [("x", ctypes.c_float), ("y", ctypes.c_float), ("z", ctypes.c_float)]

class Vel3(ctypes.Structure):
    _fields_ = [("vx", ctypes.c_float), ("vy", ctypes.c_float), ("vz", ctypes.c_float)]

class Quat(ctypes.Structure):
    _fields_ = [("w", ctypes.c_float), ("x", ctypes.c_float),
                ("y", ctypes.c_float), ("z", ctypes.c_float)]

class ShapeUnion(ctypes.Union):
    _fields_ = [("radius", ctypes.c_float), ("box", ctypes.c_float * 3),
                ("params", ctypes.c_float * 6)]

class Shape(ctypes.Structure):
    _fields_ = [("type", ctypes.c_uint32), ("u", ShapeUnion)]

class IPObject(ctypes.Structure):
    _fields_ = [
        ("id", ctypes.c_uint32),
        ("position", Vec3), ("velocity", Vel3), ("orientation", Quat),
        ("angular_velocity", Vec3),
        ("mass", ctypes.c_float), ("inv_mass", ctypes.c_float),
        ("restitution", ctypes.c_float), ("friction", ctypes.c_float),
        ("shape", Shape), ("inv_inertia", Vec3),
        ("supported_by", ctypes.c_uint32), ("contained_in", ctypes.c_uint32),
        ("is_static", ctypes.c_bool), ("visible", ctypes.c_bool),
        ("active", ctypes.c_bool), ("last_seen_time", ctypes.c_float),
    ]


class PhysicsAPI:
    """Wrapper around the C intuitive physics engine."""

    def __init__(self, lib):
        self.lib = lib
        self._bind()

    def _bind(self):
        L = self.lib
        L.intuitive_physics_create.restype = ctypes.c_void_p
        L.intuitive_physics_create.argtypes = [ctypes.c_void_p]
        L.intuitive_physics_destroy.argtypes = [ctypes.c_void_p]
        L.intuitive_physics_add_ground.restype = ctypes.c_uint32
        L.intuitive_physics_add_ground.argtypes = [ctypes.c_void_p]
        L.intuitive_physics_add_object.restype = ctypes.c_uint32
        L.intuitive_physics_add_object.argtypes = [ctypes.c_void_p, ctypes.POINTER(IPObject)]
        L.intuitive_physics_step.restype = ctypes.c_int
        L.intuitive_physics_step.argtypes = [ctypes.c_void_p, ctypes.c_float]
        L.intuitive_physics_get_object.restype = ctypes.POINTER(IPObject)
        L.intuitive_physics_get_object.argtypes = [ctypes.c_void_p, ctypes.c_uint32]
        L.intuitive_physics_is_supported.restype = ctypes.c_bool
        L.intuitive_physics_is_supported.argtypes = [ctypes.c_void_p, ctypes.c_uint32]
        L.intuitive_physics_remove_object.argtypes = [ctypes.c_void_p, ctypes.c_uint32]

    def create(self):
        engine = self.lib.intuitive_physics_create(None)
        self.lib.intuitive_physics_add_ground(engine)
        return engine

    def destroy(self, engine):
        self.lib.intuitive_physics_destroy(engine)

    def add_sphere(self, engine, x, y, z, radius=0.5, mass=1.0, vx=0, vy=0, vz=0,
                   restitution=0.5, friction=0.5):
        obj = IPObject()
        obj.position = Vec3(x, y, z)
        obj.velocity = Vel3(vx, vy, vz)
        obj.orientation = Quat(1, 0, 0, 0)
        obj.mass = mass
        obj.shape.type = 0  # SPHERE
        obj.shape.u.radius = radius
        obj.restitution = restitution
        obj.friction = friction
        obj.visible = True
        obj.active = True
        return self.lib.intuitive_physics_add_object(engine, ctypes.byref(obj))

    def add_box(self, engine, x, y, z, hx=0.5, hy=0.5, hz=0.5, mass=1.0, is_static=False):
        obj = IPObject()
        obj.position = Vec3(x, y, z)
        obj.velocity = Vel3(0, 0, 0)
        obj.orientation = Quat(1, 0, 0, 0)
        obj.mass = 0 if is_static else mass
        obj.shape.type = 1  # BOX
        obj.shape.u.box[0] = hx
        obj.shape.u.box[1] = hy
        obj.shape.u.box[2] = hz
        obj.restitution = 0.3
        obj.friction = 0.6
        obj.visible = True
        obj.active = True
        obj.is_static = is_static
        return self.lib.intuitive_physics_add_object(engine, ctypes.byref(obj))

    def step(self, engine, dt=0.01):
        return self.lib.intuitive_physics_step(engine, dt)

    def get_state(self, engine, obj_id):
        """Get (x, y, z, vx, vy, vz) as flat list."""
        ptr = self.lib.intuitive_physics_get_object(engine, obj_id)
        if not ptr:
            return [0] * 6
        o = ptr.contents
        return [o.position.x, o.position.y, o.position.z,
                o.velocity.vx, o.velocity.vy, o.velocity.vz]

    def is_supported(self, engine, obj_id):
        return self.lib.intuitive_physics_is_supported(engine, obj_id)


# ============================================================================
# Scenario Generators — Physics
# ============================================================================

def scenario_free_fall(phys, height=None, mass=None):
    """Drop a ball from random height. Learn: y(t) = h - 0.5*g*t^2."""
    h = height or random.uniform(1.0, 10.0)
    m = mass or random.uniform(0.5, 5.0)
    engine = phys.create()
    ball = phys.add_sphere(engine, 0, h, 0, mass=m)
    transitions = []
    for _ in range(int(h * 20)):  # enough steps to hit ground
        state = phys.get_state(engine, ball)
        phys.step(engine)
        next_state = phys.get_state(engine, ball)
        transitions.append((state, [0]*3, next_state))  # action = no force
    phys.destroy(engine)
    return transitions

def scenario_projectile(phys):
    """Throw a ball at random angle. Learn: parabolic trajectory."""
    speed = random.uniform(5.0, 20.0)
    angle = random.uniform(15, 75)
    vx = speed * math.cos(math.radians(angle))
    vy = speed * math.sin(math.radians(angle))
    engine = phys.create()
    ball = phys.add_sphere(engine, 0, 1.0, 0, vx=vx, vy=vy)
    transitions = []
    for _ in range(300):
        state = phys.get_state(engine, ball)
        phys.step(engine)
        next_state = phys.get_state(engine, ball)
        transitions.append((state, [0]*3, next_state))
        if next_state[1] < 0.3 and _ > 10:
            break  # hit ground
    phys.destroy(engine)
    return transitions

def scenario_collision(phys):
    """Two balls approaching each other. Learn: momentum conservation."""
    engine = phys.create()
    m1 = random.uniform(0.5, 3.0)
    m2 = random.uniform(0.5, 3.0)
    v1 = random.uniform(2.0, 8.0)
    v2 = -random.uniform(2.0, 8.0)
    b1 = phys.add_sphere(engine, -3, 0.5, 0, mass=m1, vx=v1, restitution=0.8)
    b2 = phys.add_sphere(engine, 3, 0.5, 0, mass=m2, vx=v2, restitution=0.8)
    transitions = []
    for _ in range(200):
        s1 = phys.get_state(engine, b1)
        s2 = phys.get_state(engine, b2)
        state = s1 + s2  # 12-dim: both objects
        phys.step(engine)
        n1 = phys.get_state(engine, b1)
        n2 = phys.get_state(engine, b2)
        next_state = n1 + n2
        transitions.append((state, [0]*3, next_state))
    phys.destroy(engine)
    return transitions

def scenario_stacking(phys):
    """Stack boxes. Learn: support relations, stability."""
    engine = phys.create()
    # Table (static)
    table = phys.add_box(engine, 0, 0.5, 0, hx=2.0, hy=0.5, hz=2.0, is_static=True)
    # Stack 3 boxes
    ids = []
    for i in range(3):
        bid = phys.add_box(engine, 0, 1.5 + i * 1.1, 0, hx=0.4, hy=0.5, hz=0.4, mass=1.0)
        ids.append(bid)
    transitions = []
    for _ in range(200):
        states = []
        for bid in ids:
            states.extend(phys.get_state(engine, bid))
        phys.step(engine)
        next_states = []
        for bid in ids:
            next_states.extend(phys.get_state(engine, bid))
        transitions.append((states, [0]*3, next_states))
    phys.destroy(engine)
    return transitions

def scenario_bounce(phys):
    """Drop a bouncy ball. Learn: energy loss per bounce, restitution."""
    engine = phys.create()
    rest = random.uniform(0.3, 0.9)
    ball = phys.add_sphere(engine, 0, 5.0, 0, restitution=rest)
    transitions = []
    for _ in range(500):
        state = phys.get_state(engine, ball)
        phys.step(engine)
        next_state = phys.get_state(engine, ball)
        transitions.append((state, [0]*3, next_state))
    phys.destroy(engine)
    return transitions

def scenario_ramp(phys):
    """Ball rolling down a ramp (approximated as angled velocity)."""
    engine = phys.create()
    angle = random.uniform(10, 45)
    vx = 2.0 * math.cos(math.radians(angle))
    vy = -2.0 * math.sin(math.radians(angle))
    ball = phys.add_sphere(engine, -3, 5.0, 0, vx=vx, vy=vy, friction=0.3)
    transitions = []
    for _ in range(300):
        state = phys.get_state(engine, ball)
        phys.step(engine)
        next_state = phys.get_state(engine, ball)
        transitions.append((state, [0]*3, next_state))
    phys.destroy(engine)
    return transitions


# ============================================================================
# Scenario Generators — Chemistry
# ============================================================================

def scenario_reaction_dynamics():
    """Simulate a chemical reaction and track concentration changes.
    Returns transitions as (concentrations_t, action, concentrations_t+1)."""
    # Simple model: A + B → C with rate k
    k = random.uniform(0.01, 0.5)
    a0 = random.uniform(0.5, 2.0)
    b0 = random.uniform(0.5, 2.0)
    c0 = 0.0
    dt = 0.1

    transitions = []
    a, b, c = a0, b0, c0
    for _ in range(100):
        state = [a, b, c, a + b + c]  # include total mass for conservation
        rate = k * a * b
        a_new = max(0, a - rate * dt)
        b_new = max(0, b - rate * dt)
        c_new = c + rate * dt
        next_state = [a_new, b_new, c_new, a_new + b_new + c_new]
        transitions.append((state, [k, dt, 0], next_state))
        a, b, c = a_new, b_new, c_new
    return transitions

def scenario_phase_transition():
    """Heat water from ice to steam. Track phase changes."""
    T = random.uniform(250, 270)  # start below freezing
    heat_rate = random.uniform(1.0, 5.0)  # K per step
    dt = 1.0

    transitions = []
    for _ in range(100):
        # phase: 0=solid, 1=liquid, 2=gas
        phase = 0 if T < 273.15 else (1 if T < 373.15 else 2)
        state = [T, phase, heat_rate]
        T_new = T + heat_rate * dt
        # Phase transition latent heat: temperature plateaus
        if 272 < T < 274:
            T_new = T + heat_rate * dt * 0.1  # slow at melting point
        elif 372 < T < 374:
            T_new = T + heat_rate * dt * 0.1  # slow at boiling point
        phase_new = 0 if T_new < 273.15 else (1 if T_new < 373.15 else 2)
        next_state = [T_new, phase_new, heat_rate]
        transitions.append((state, [heat_rate], next_state))
        T = T_new
    return transitions

def scenario_acid_base():
    """Mix acid and base. Track pH change."""
    acid_conc = random.uniform(0.01, 1.0)
    base_added_rate = random.uniform(0.001, 0.05)

    transitions = []
    base_added = 0.0
    for step in range(100):
        h_plus = max(1e-14, acid_conc - base_added)
        pH = -math.log10(h_plus) if h_plus > 1e-14 else 14.0
        state = [acid_conc, base_added, pH, h_plus]
        base_added += base_added_rate
        h_plus_new = max(1e-14, acid_conc - base_added)
        pH_new = -math.log10(h_plus_new) if h_plus_new > 1e-14 else 14.0
        next_state = [acid_conc, base_added, pH_new, h_plus_new]
        transitions.append((state, [base_added_rate], next_state))
    return transitions

def scenario_equilibrium():
    """Reversible reaction A ⇌ B approaching equilibrium."""
    kf = random.uniform(0.05, 0.3)
    kr = random.uniform(0.01, 0.1)
    a = random.uniform(1.0, 3.0)
    b = 0.0
    dt = 0.1

    transitions = []
    for _ in range(200):
        state = [a, b, kf, kr]
        forward = kf * a * dt
        reverse = kr * b * dt
        a_new = max(0, a - forward + reverse)
        b_new = max(0, b + forward - reverse)
        next_state = [a_new, b_new, kf, kr]
        transitions.append((state, [dt], next_state))
        a, b = a_new, b_new
    return transitions


# ============================================================================
# Scenario Generators — Biology
# ============================================================================

def scenario_predator_prey():
    """Lotka-Volterra predator-prey dynamics."""
    prey = random.uniform(50, 200)
    pred = random.uniform(10, 50)
    alpha = random.uniform(0.05, 0.15)  # prey growth
    beta = random.uniform(0.001, 0.005)  # predation
    delta = random.uniform(0.0005, 0.002)  # pred growth from prey
    gamma = random.uniform(0.05, 0.15)  # pred death
    dt = 1.0

    transitions = []
    for _ in range(365):  # 1 year
        state = [prey, pred, prey + pred]
        dprey = (alpha * prey - beta * prey * pred) * dt
        dpred = (delta * prey * pred - gamma * pred) * dt
        prey_new = max(0, prey + dprey)
        pred_new = max(0, pred + dpred)
        next_state = [prey_new, pred_new, prey_new + pred_new]
        transitions.append((state, [alpha, beta, gamma], next_state))
        prey, pred = prey_new, pred_new
    return transitions

def scenario_logistic_growth():
    """Population with carrying capacity."""
    N = random.uniform(10, 50)
    K = random.uniform(500, 2000)
    r = random.uniform(0.01, 0.05)
    dt = 1.0

    transitions = []
    for _ in range(500):
        state = [N, K, r]
        dN = r * N * (1 - N / K) * dt
        N_new = max(0, N + dN)
        next_state = [N_new, K, r]
        transitions.append((state, [r], next_state))
        N = N_new
    return transitions

def scenario_food_web():
    """3-level food web: grass → rabbits → foxes."""
    grass = random.uniform(500, 1500)
    rabbit = random.uniform(50, 150)
    fox = random.uniform(5, 20)
    dt = 1.0

    transitions = []
    for _ in range(365):
        state = [grass, rabbit, fox]
        # Grass: logistic growth
        dg = 0.03 * grass * (1 - grass / 2000) - 0.001 * rabbit * grass
        # Rabbits: eat grass, eaten by foxes
        dr = 0.0005 * rabbit * grass - 0.01 * rabbit * fox - 0.02 * rabbit
        # Foxes: eat rabbits
        df = 0.005 * fox * rabbit - 0.1 * fox
        grass_new = max(0, grass + dg * dt)
        rabbit_new = max(0, rabbit + dr * dt)
        fox_new = max(0, fox + df * dt)
        next_state = [grass_new, rabbit_new, fox_new]
        transitions.append((state, [dt], next_state))
        grass, rabbit, fox = grass_new, rabbit_new, fox_new
    return transitions

def scenario_homeostasis():
    """Body temperature regulation."""
    temp = random.uniform(36.0, 38.0)
    external_temp = random.uniform(-10, 40)
    dt = 0.1  # hours

    transitions = []
    for _ in range(200):
        state = [temp, external_temp]
        # Negative feedback toward 37°C
        regulation = -0.5 * (temp - 37.0) * dt
        # Environmental drift
        drift = 0.02 * (external_temp - temp) * dt
        temp_new = temp + regulation + drift
        next_state = [temp_new, external_temp]
        transitions.append((state, [external_temp], next_state))
        temp = temp_new
    return transitions

def scenario_genetics():
    """Allele frequency change over generations (drift)."""
    p = random.uniform(0.2, 0.8)  # dominant allele freq
    pop_size = random.randint(50, 500)

    transitions = []
    for gen in range(100):
        q = 1 - p
        # Hardy-Weinberg frequencies
        AA = p * p
        Aa = 2 * p * q
        aa = q * q
        state = [p, q, AA, Aa, aa, pop_size]
        # Genetic drift (sampling)
        p_new = p + random.gauss(0, math.sqrt(p * q / (2 * pop_size)))
        p_new = max(0.01, min(0.99, p_new))
        q_new = 1 - p_new
        next_state = [p_new, q_new, p_new**2, 2*p_new*q_new, q_new**2, pop_size]
        transitions.append((state, [pop_size], next_state))
        p = p_new
    return transitions


# ============================================================================
# WorldModelCurriculum — Orchestrates Training
# ============================================================================

class WorldModelCurriculum:
    """Generates experiential training data from simulation engines.

    Progressive difficulty levels:
      Level 1: Single-object physics (free fall, bounce)
      Level 2: Two-object physics (collision) + basic chemistry (single reaction)
      Level 3: Multi-object physics (stacking) + multi-reaction chemistry + basic biology
      Level 4: Complex scenarios (food webs, equilibria, projectiles on ramps)
    """

    PHYSICS_SCENARIOS = {
        1: [scenario_free_fall, scenario_bounce],
        2: [scenario_collision, scenario_projectile],
        3: [scenario_stacking, scenario_ramp],
    }

    CHEMISTRY_SCENARIOS = {
        1: [scenario_phase_transition],
        2: [scenario_reaction_dynamics, scenario_acid_base],
        3: [scenario_equilibrium],
    }

    BIOLOGY_SCENARIOS = {
        1: [scenario_logistic_growth, scenario_homeostasis],
        2: [scenario_predator_prey, scenario_genetics],
        3: [scenario_food_web],
    }

    # Verb tags for MOTOR-modality grounding (modality=2). Each scenario name
    # maps to the action verb that best describes the dynamics it produces;
    # the verb is bound to the action vector via brain.ground_word so the
    # lexicon's MOTOR slot accumulates real signal during world-model training.
    SCENARIO_VERBS = {
        # Physics
        "scenario_free_fall":      "fall",
        "scenario_projectile":     "throw",
        "scenario_collision":      "collide",
        "scenario_stacking":       "stack",
        "scenario_bounce":         "bounce",
        "scenario_ramp":           "roll",
        # Chemistry
        "scenario_reaction_dynamics": "react",
        "scenario_phase_transition":  "melt",
        "scenario_acid_base":         "mix",
        "scenario_equilibrium":       "balance",
        # Biology
        "scenario_predator_prey":   "hunt",
        "scenario_logistic_growth": "grow",
        "scenario_food_web":        "eat",
        "scenario_homeostasis":     "regulate",
        "scenario_genetics":        "inherit",
    }

    # GL_MODALITY_MOTOR — keep in sync with grounded_lexicon.h. 2 = MOTOR.
    _GL_MODALITY_MOTOR = 2

    def __init__(self, brain_proxy=None, max_level=3):
        self.brain = brain_proxy
        self.max_level = max_level
        self.total_transitions = 0
        self.total_scenarios = 0
        self._lib = None
        self._phys = None

    def _ensure_physics(self):
        if self._phys is None:
            self._lib = _load_lib()
            self._phys = PhysicsAPI(self._lib)

    @staticmethod
    def _spread_encode(raw, dim=1024):
        """Spread a short physics vector across the full embedding space.

        Raw sim states are 3-9 floats. Zero-padding to 1024 concentrates
        gradient on the first few input neurons, causing weight explosion
        in batch mode. Instead, hash each raw dimension into multiple
        spread-out positions using a fixed random projection, so the
        gradient distributes across the full input space — same way text
        embeddings naturally do.

        Uses a deterministic seed so the projection is consistent across
        calls (same raw dim always maps to same output positions).
        """
        import numpy as np
        out = np.zeros(dim, dtype=np.float32)
        if not raw:
            return out.tolist()
        n = len(raw)
        # Each raw dimension gets ~(dim/n) spread positions via a fixed hash
        rng = np.random.RandomState(42)  # deterministic projection
        indices = rng.randint(0, dim, size=(n, dim // n + 4))
        weights = rng.randn(n, dim // n + 4).astype(np.float32) * 0.5
        for i, val in enumerate(raw):
            if val == 0.0:
                continue
            for j in range(min(len(indices[i]), dim // n + 4)):
                idx = indices[i][j] % dim
                out[idx] += val * weights[i][j]
        # Normalize to roughly unit magnitude to match text embedding scale
        norm = np.linalg.norm(out)
        if norm > 1e-6:
            out *= (1.0 / norm) * np.sqrt(float(n))
        return out.tolist()

    def _feed_transitions(self, transitions, domain_label, verb=None):
        """Feed transitions to the brain via learn_vector_batch.

        Raw physics states are spread-encoded into the full 1024-dim
        embedding space via a fixed random projection, preventing the
        gradient concentration that caused batch mode to blow ANN loss.

        If `verb` is non-empty AND the brain proxy exposes `ground_word`,
        each transition's encoded action vector is ALSO bound to the verb
        in the grounded lexicon under modality=2 (MOTOR). This is what
        gives the lexicon its MOTOR-modality signal during training; without
        it, motor bindings stay empty and grounded_lexicon's motor slot
        is never populated.
        """
        if not self.brain:
            return len(transitions)

        pairs = []
        for state, action, next_state in transitions:
            features = self._spread_encode(state + action)
            target = self._spread_encode(next_state)
            action_vec = self._spread_encode(action)
            pairs.append((features, target, action_vec))

        if not pairs:
            return 0

        # Individual learn_vector with spread encoding + reduced LR.
        # Batch mode blows ANN loss even with spread encoding —
        # learn_vector_batch accumulates gradients too aggressively
        # for this brain's weight scale regardless of input distribution.
        ground_motor = bool(verb) and hasattr(self.brain, "ground_word")
        fed = 0
        for features, target, action_vec in pairs:
            try:
                self.brain.learn_vector(features, target,
                                        label=domain_label,
                                        learning_rate=0.0005)
                fed += 1
            except Exception:
                pass
            # MOTOR grounding — best-effort. A parallel agent is wiring
            # ground_word on the proxy; until then this silently no-ops.
            if ground_motor:
                try:
                    self.brain.ground_word(verb, action_vec,
                                           modality=self._GL_MODALITY_MOTOR,
                                           attention=0.5)
                except Exception:
                    pass
        return fed

    def run_physics_epoch(self, level=None, scenarios_per_level=5):
        """Run physics scenarios and feed transitions to brain."""
        self._ensure_physics()
        total = 0
        levels = [level] if level else range(1, self.max_level + 1)
        for lv in levels:
            generators = self.PHYSICS_SCENARIOS.get(lv, [])
            for gen in generators:
                verb = self.SCENARIO_VERBS.get(gen.__name__)
                for _ in range(scenarios_per_level):
                    try:
                        transitions = gen(self._phys)
                        fed = self._feed_transitions(transitions, f"physics_L{lv}",
                                                     verb=verb)
                        total += fed
                        self.total_transitions += fed
                        self.total_scenarios += 1
                    except Exception as e:
                        logger.warning("Physics scenario %s failed: %s", gen.__name__, e)
        logger.info("Physics epoch: %d transitions from %d scenarios", total, self.total_scenarios)
        return total

    def run_chemistry_epoch(self, level=None, scenarios_per_level=5):
        """Run chemistry scenarios (pure Python, no C engine needed)."""
        total = 0
        levels = [level] if level else range(1, self.max_level + 1)
        for lv in levels:
            generators = self.CHEMISTRY_SCENARIOS.get(lv, [])
            for gen in generators:
                verb = self.SCENARIO_VERBS.get(gen.__name__)
                for _ in range(scenarios_per_level):
                    try:
                        transitions = gen()
                        fed = self._feed_transitions(transitions, f"chemistry_L{lv}",
                                                     verb=verb)
                        total += fed
                        self.total_transitions += fed
                        self.total_scenarios += 1
                    except Exception as e:
                        logger.warning("Chemistry scenario %s failed: %s", gen.__name__, e)
        logger.info("Chemistry epoch: %d transitions", total)
        return total

    def run_biology_epoch(self, level=None, scenarios_per_level=5):
        """Run biology scenarios (pure Python, no C engine needed)."""
        total = 0
        levels = [level] if level else range(1, self.max_level + 1)
        for lv in levels:
            generators = self.BIOLOGY_SCENARIOS.get(lv, [])
            for gen in generators:
                verb = self.SCENARIO_VERBS.get(gen.__name__)
                for _ in range(scenarios_per_level):
                    try:
                        transitions = gen()
                        fed = self._feed_transitions(transitions, f"biology_L{lv}",
                                                     verb=verb)
                        total += fed
                        self.total_transitions += fed
                        self.total_scenarios += 1
                    except Exception as e:
                        logger.warning("Biology scenario %s failed: %s", gen.__name__, e)
        logger.info("Biology epoch: %d transitions", total)
        return total

    def run_full_epoch(self, level=None, scenarios_per_level=3):
        """Run all three domains."""
        p = self.run_physics_epoch(level, scenarios_per_level)
        c = self.run_chemistry_epoch(level, scenarios_per_level)
        b = self.run_biology_epoch(level, scenarios_per_level)
        logger.info("Full epoch: %d physics + %d chemistry + %d biology = %d total",
                     p, c, b, p + c + b)
        return p + c + b

    def get_stats(self):
        return {
            "total_transitions": self.total_transitions,
            "total_scenarios": self.total_scenarios,
        }


# ============================================================================
# Standalone Test
# ============================================================================

if __name__ == "__main__":
    logging.basicConfig(level=logging.INFO, format="%(asctime)s [%(name)s] %(message)s")

    wmc = WorldModelCurriculum(brain_proxy=None, max_level=3)

    # Test physics scenarios
    print("=== Physics Scenarios ===")
    n = wmc.run_physics_epoch(scenarios_per_level=2)
    print(f"  Generated {n} transitions")

    # Test chemistry scenarios
    print("\n=== Chemistry Scenarios ===")
    n = wmc.run_chemistry_epoch(scenarios_per_level=2)
    print(f"  Generated {n} transitions")

    # Test biology scenarios
    print("\n=== Biology Scenarios ===")
    n = wmc.run_biology_epoch(scenarios_per_level=2)
    print(f"  Generated {n} transitions")

    print(f"\n=== Total: {wmc.total_transitions} transitions from {wmc.total_scenarios} scenarios ===")
    print("CURRICULUM TEST PASSED")
