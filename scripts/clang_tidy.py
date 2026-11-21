#!/usr/bin/env python3
#
# clang_tidy.py - run clang-tidy over the application sources
#
# Author:  Sandesh Ghimire <sandesh@soccentric.com>
# Target:  NUCLEO-F767ZI (Zephyr board nucleo_f767zi)
# SRS:     ZDNFR-004
#
# clang-tidy reuses the compile flags in build/compile_commands.json.  The
# Zephyr build targets arm-zephyr-eabi-gcc, so a few GCC-only options in that
# file are not understood by the host clang that clang-tidy runs.  They are
# removed from a copy of the database here instead of failing every file, and
# the remaining "argument unused" diagnostics are not treated as errors.
#
# Usage:
#   scripts/clang_tidy.py --build-dir build --clang-tidy clang-tidy \
#       --config .clang-tidy [--warnings-as-errors=*] src/main.c ...
#

import argparse
import json
import pathlib
import subprocess
import sys

# GCC options that the host clang used by clang-tidy does not accept.  They
# only affect GCC code generation, never the warnings clang-tidy reports.
GCC_ONLY_FLAGS = (
    "-fno-printf-return-value",
    "-fno-reorder-functions",
    "-mfp16-format=ieee",
    "-fno-defer-pop",
)


def strip_flags(entry: dict) -> dict:
    """Return a copy of a compile database entry without GCC-only flags."""
    cleaned = dict(entry)
    if "command" in cleaned:
        command = cleaned["command"]
        for flag in GCC_ONLY_FLAGS:
            command = command.replace(" " + flag, "")
        cleaned["command"] = command
    else:
        cleaned["arguments"] = [
            arg for arg in cleaned.get("arguments", [])
            if arg not in GCC_ONLY_FLAGS
        ]
    return cleaned


def write_database(build_dir: pathlib.Path) -> pathlib.Path:
    """Write a clang-tidy friendly copy of compile_commands.json."""
    source = build_dir / "compile_commands.json"
    if not source.is_file():
        sys.exit(f"clang_tidy: {source} is missing; build the firmware first")

    entries = [strip_flags(e) for e in json.loads(source.read_text("utf-8"))]
    out_dir = build_dir / "clang-tidy"
    out_dir.mkdir(parents=True, exist_ok=True)
    out = out_dir / "compile_commands.json"
    out.write_text(json.dumps(entries, indent=1) + "\n", encoding="utf-8")
    return out


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--build-dir", required=True, type=pathlib.Path)
    parser.add_argument("--clang-tidy", default="clang-tidy")
    parser.add_argument("--config", required=True, type=pathlib.Path)
    parser.add_argument("--warnings-as-errors", action="append", default=[])
    parser.add_argument("sources", nargs="+", type=pathlib.Path)
    args = parser.parse_args()

    database = write_database(args.build_dir)

    command = [
        args.clang_tidy,
        "-p", str(database.parent),
        "--config-file", str(args.config),
        "-quiet",
        # Zephyr passes picolibc specs and GCC tuning options that the host
        # clang driver does not use; do not turn those into errors.
        "--extra-arg=-Qunused-arguments",
        "--extra-arg=-Wno-error=unused-command-line-argument",
        *[f"--warnings-as-errors={value}"
          for value in args.warnings_as_errors],
        *[str(source) for source in args.sources],
    ]
    return subprocess.run(command, check=False).returncode


if __name__ == "__main__":
    sys.exit(main())
