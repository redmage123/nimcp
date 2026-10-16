"""Streaming corpus backends. Each module defines a StreamingSource subclass
and self-registers via register_source() at import time.

To add a new backend, drop a module in this directory that:
  1. Imports streaming_ingest.StreamingSource and register_source.
  2. Defines a subclass with kind, list_for_stage, fetch_chunks, is_complete.
  3. Calls register_source(MyBackend()) at module scope.
  4. Adds itself to the import list below so it's loaded automatically.

Backends are intentionally NOT auto-discovered via os.listdir to keep the
import order deterministic and the load surface explicit. Adding a backend
is a one-line edit here.
"""

# Bundled backends (each one self-registers on import):
from . import wikipedia    # noqa: F401   — added by CE-15
from . import investopedia  # noqa: F401   — added by CE-16
from . import arxiv         # noqa: F401   — added by CE-17
from . import gutenberg_stream  # noqa: F401   — added by CE-18
from . import hf_github     # noqa: F401   — added by CE-5b

# Test stub: only loaded under explicit env flag so prod imports don't pay
# its tiny cost. Tests set NIMCP_STREAMING_TEST_STUB=1 to enable.
import os
if os.environ.get("NIMCP_STREAMING_TEST_STUB") == "1":
    from . import _test_stub  # noqa: F401
