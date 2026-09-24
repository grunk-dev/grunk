# SPDX-FileCopyrightText: 2026 Jan Kleinert <jan.kleinert@dlr.de>
#
# SPDX-License-Identifier: MPL-2.0

"""grunk's own top-level Python package - re-exports the compiled ._bindings
extension's public API (grunk.state, grunk.Feature, ...) when it's importable.

Importing this package (and hence, transitively, grunk.codegen and the `grunk`
CLI's own codegen subcommand - see cli.py) must not itself require ._bindings to
have been built: grunk.codegen is pure Python, needed by any plugin depending on
grunk purely for its `grunk codegen` CLI (see e.g. grunk-occt's/grunk-adolc's own
pyproject.toml comment on their own grunk pin), and building the compiled
extension is a materially heavier, more fragile dependency (a full C++ toolchain,
nanobind, parametric, ...) that such a consumer never needed at all - found via
code review, after this exact chain (grunk.cli -> grunk.__init__ ->
grunk._bindings) turned out to make every use of grunk.codegen unnecessarily
depend on the compiled extension having been built successfully.
"""

import importlib.metadata

try:
    from ._bindings import __doc__, __version__  # noqa: A004 - re-exporting the extension's own docstring/version is deliberate
    from ._bindings import __default_state__
    from ._bindings import plugin
    from ._bindings import *  # noqa: F403 - the extension's own public API, re-exported verbatim
    _bindings_import_error: ImportError | None = None
except ImportError as _exc:
    # Fall back to the installed distribution's own version metadata (no
    # compiled-extension dependency at all) - __doc__ stays this module's own
    # docstring above (only overwritten by the try block above on success).
    __version__ = importlib.metadata.version(__name__)
    _bindings_import_error = _exc


def __getattr__(name: str):
    """PEP 562 module-level attribute lookup - only ever reached for a name the
    `from ._bindings import *` above didn't already bind (i.e. only when
    ._bindings failed to import at all, per the try/except above). Turns what
    would otherwise be a confusing bare AttributeError (or, without this
    module's own try/except, an ImportError at the top of this file that broke
    even pure-Python grunk.codegen) into a clear explanation of what actually
    needs the compiled extension and how to get it."""
    if _bindings_import_error is not None:
        raise ImportError(
            f"grunk.{name} needs the compiled grunk._bindings extension, which failed to "
            f"import ({_bindings_import_error}). Install grunk's Python bindings properly "
            f"(pip install . / pixi run install_python) to use it - not needed at all for "
            f"grunk.codegen or the `grunk codegen` CLI."
        ) from _bindings_import_error
    raise AttributeError(f"module {__name__!r} has no attribute {name!r}")
