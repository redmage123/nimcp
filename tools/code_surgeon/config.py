"""
Code Surgeon Configuration - Dependency Injection Container

WHAT: Configuration and dependency management for Code Surgeon
WHY:  Centralize settings and enable dependency injection
HOW:  Immutable dataclass with sensible defaults

PRINCIPLES:
- Dependency Injection
- Immutable Configuration
- Single Source of Truth
- Fail-Fast Validation
"""

from dataclasses import dataclass, field
from pathlib import Path
from typing import Optional
from llm_provider import LLMProvider, create_llm_provider


@dataclass(frozen=True)
class CodeSurgeonConfig:
    """
    Immutable Code Surgeon configuration

    WHAT: All settings for Code Surgeon execution
    WHY:  Single source of truth for configuration
    HOW:  Frozen dataclass with validation

    DESIGN: Builder pattern via replace() for modifications
    """

    # Project paths
    nimcp_root: Path
    test_dir: Optional[Path] = None  # Default: nimcp_root / "test"
    build_dir: Optional[Path] = None  # Default: nimcp_root / "build"
    source_dir: Optional[Path] = None  # Default: nimcp_root / "src"

    # Test execution
    max_iterations: int = 5
    timeout_sec: int = 300
    enable_parallel: bool = False
    max_workers: int = 4

    # Coverage settings
    enable_coverage: bool = True
    target_coverage: float = 100.0
    coverage_threshold: float = 50.0  # Warn if below this

    # LLM settings
    llm_provider: Optional[LLMProvider] = None  # Injected dependency
    llm_max_tokens: int = 4096

    # Debugging
    debug_mode: bool = False
    verbose: bool = False
    save_artifacts: bool = True

    def __post_init__(self):
        """
        Validate configuration

        WHAT: Check configuration values are valid
        WHY:  Fail-fast on invalid settings
        HOW:  Guard clauses with clear error messages

        RAISES: ValueError on invalid configuration
        SIDE EFFECTS: None (validation only)
        """
        # Validate nimcp_root exists
        if not self.nimcp_root.exists():
            raise ValueError(f"NIMCP root does not exist: {self.nimcp_root}")

        # Validate iteration count
        if self.max_iterations < 1:
            raise ValueError(f"max_iterations must be >= 1, got {self.max_iterations}")

        # Validate timeout
        if self.timeout_sec < 1:
            raise ValueError(f"timeout_sec must be >= 1, got {self.timeout_sec}")

        # Validate coverage targets
        if not (0.0 <= self.target_coverage <= 100.0):
            raise ValueError(f"target_coverage must be 0-100, got {self.target_coverage}")

        if not (0.0 <= self.coverage_threshold <= 100.0):
            raise ValueError(f"coverage_threshold must be 0-100, got {self.coverage_threshold}")

        # Validate worker count
        if self.max_workers < 1:
            raise ValueError(f"max_workers must be >= 1, got {self.max_workers}")

        # Initialize default paths if not provided
        # Note: Can't modify frozen dataclass in __post_init__,
        # so these must be set before construction or via replace()

    def get_test_dir(self) -> Path:
        """
        Get test directory path

        WHAT: Return configured or default test directory
        WHY:  Centralize test directory logic
        HOW:  Use configured value or default to nimcp_root/test

        RETURNS: Absolute path to test directory
        """
        if self.test_dir:
            return self.test_dir
        return self.nimcp_root / "test"

    def get_build_dir(self) -> Path:
        """
        Get build directory path

        WHAT: Return configured or default build directory
        WHY:  Centralize build directory logic
        HOW:  Use configured value or default to nimcp_root/build

        RETURNS: Absolute path to build directory
        """
        if self.build_dir:
            return self.build_dir
        return self.nimcp_root / "build"

    def get_source_dir(self) -> Path:
        """
        Get source directory path

        WHAT: Return configured or default source directory
        WHY:  Centralize source directory logic
        HOW:  Use configured value or default to nimcp_root/src

        RETURNS: Absolute path to source directory
        """
        if self.source_dir:
            return self.source_dir
        return self.nimcp_root / "src"

    def get_llm_provider(self) -> LLMProvider:
        """
        Get LLM provider

        WHAT: Return configured LLM provider or create default
        WHY:  Lazy initialization of LLM provider
        HOW:  Use configured provider or create OpenAI GPT-5 default

        RETURNS: LLMProvider instance
        SIDE EFFECTS: May create OpenAI provider
        """
        if self.llm_provider:
            return self.llm_provider

        # Default to OpenAI GPT-5 with comprehensive project context
        try:
            return create_llm_provider("openai", model="gpt-5")
        except Exception as e:
            raise RuntimeError(f"Failed to create default LLM provider: {e}")


def create_config_from_env(nimcp_root: Path, **overrides) -> CodeSurgeonConfig:
    """
    Create configuration from environment and overrides

    WHAT: Build config from defaults + env vars + explicit overrides
    WHY:  Flexible configuration from multiple sources
    HOW:  Merge defaults, env vars, and overrides (overrides win)

    PARAMETERS:
        nimcp_root: Path to NIMCP project root
        **overrides: Explicit configuration overrides

    RETURNS: CodeSurgeonConfig instance

    EXAMPLE:
        config = create_config_from_env(
            Path("/home/user/nimcp"),
            max_iterations=10,
            debug_mode=True
        )
    """
    import os

    # Start with defaults
    config_dict = {
        "nimcp_root": nimcp_root,
        "test_dir": None,
        "build_dir": None,
        "source_dir": None,
        "max_iterations": int(os.environ.get("CODE_SURGEON_MAX_ITERATIONS", "5")),
        "timeout_sec": int(os.environ.get("CODE_SURGEON_TIMEOUT", "300")),
        "enable_parallel": os.environ.get("CODE_SURGEON_PARALLEL", "false").lower() == "true",
        "max_workers": int(os.environ.get("CODE_SURGEON_MAX_WORKERS", "4")),
        "enable_coverage": os.environ.get("CODE_SURGEON_COVERAGE", "true").lower() == "true",
        "target_coverage": float(os.environ.get("CODE_SURGEON_TARGET", "100.0")),
        "coverage_threshold": float(os.environ.get("CODE_SURGEON_THRESHOLD", "50.0")),
        "llm_provider": None,  # Will be created lazily
        "llm_max_tokens": int(os.environ.get("CODE_SURGEON_MAX_TOKENS", "4096")),
        "debug_mode": os.environ.get("CODE_SURGEON_DEBUG", "false").lower() == "true",
        "verbose": os.environ.get("CODE_SURGEON_VERBOSE", "false").lower() == "true",
        "save_artifacts": os.environ.get("CODE_SURGEON_SAVE_ARTIFACTS", "true").lower() == "true",
    }

    # Apply overrides
    config_dict.update(overrides)

    return CodeSurgeonConfig(**config_dict)


def create_test_config(nimcp_root: Path, **overrides) -> CodeSurgeonConfig:
    """
    Create configuration for testing

    WHAT: Build config with mock LLM and test-friendly settings
    WHY:  Enable fast testing without API calls
    HOW:  Use mock provider and short timeouts

    PARAMETERS:
        nimcp_root: Path to NIMCP project root
        **overrides: Explicit configuration overrides

    RETURNS: CodeSurgeonConfig instance configured for testing

    EXAMPLE:
        config = create_test_config(Path("/tmp/nimcp"))
    """
    from llm_provider import MockLLMProvider

    test_defaults = {
        "nimcp_root": nimcp_root,
        "max_iterations": 1,
        "timeout_sec": 10,
        "enable_coverage": False,
        "llm_provider": MockLLMProvider(fixed_response="Mock fix response"),
        "debug_mode": True,
        "save_artifacts": False,
    }

    test_defaults.update(overrides)
    return CodeSurgeonConfig(**test_defaults)
