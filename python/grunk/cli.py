# SPDX-FileCopyrightText: 2026 Jan Kleinert <jan.kleinert@dlr.de>
#
# SPDX-License-Identifier: MPL-2.0

"""The `grunk` command-line entry point (installed as a console script - see
pyproject.toml's [project.scripts]). Deliberately structured as one top-level
command with subcommands from day one, rather than a single-purpose script (e.g.
`grunk-codegen`), so future grunk CLI features have a home without introducing a
new top-level script each time - `codegen` is the first and, for now, only
subcommand."""

from __future__ import annotations

import argparse
import sys

from . import __version__
from .codegen import generate as codegen_generate


def build_parser() -> argparse.ArgumentParser:
    parser = argparse.ArgumentParser(prog="grunk", description=__doc__)
    parser.add_argument("--version", action="version", version=f"%(prog)s {__version__}")

    subparsers = parser.add_subparsers(dest="command", required=True)

    codegen_parser = subparsers.add_parser(
        "codegen",
        help="Generate a grunk plugin's registration code from a config.yml (see grunk.codegen).",
        description=codegen_generate.__doc__,
    )
    # Reuses grunk.codegen.generate's own flag definitions rather than
    # redeclaring them here - see generate.add_arguments/generate.run.
    codegen_generate.add_arguments(codegen_parser)
    codegen_parser.set_defaults(func=codegen_generate.run)

    return parser


def main() -> int:
    parser = build_parser()
    args = parser.parse_args()
    return args.func(args)


if __name__ == "__main__":
    sys.exit(main())
