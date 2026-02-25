#!/usr/bin/env python3

import pathlib
import subprocess
import sys
import time


SOURCE_SUFFIXES = {
    ".c",
    ".cc",
    ".cpp",
    ".cxx",
    ".c++",
    ".m",
    ".mm",
}

ANSI_RESET = "\033[0m"
ANSI_GREEN = "\033[32m"
ANSI_RED = "\033[31m"


def find_source_arg(args):
    for arg in args:
        if not arg or arg.startswith("-"):
            continue
        if arg.startswith("/"):
            path = pathlib.Path(arg[1:]) if arg.startswith("/Fo") else None
            if path is not None:
                continue
        suffix = pathlib.Path(arg).suffix.lower()
        if suffix in SOURCE_SUFFIXES:
            return arg
    return None


def display_name(args):
    source = find_source_arg(args)
    if source:
        return source
    for i, arg in enumerate(args):
        if arg == "-o" and i + 1 < len(args):
            return args[i + 1]
        if arg.startswith("/Fo") and len(arg) > 3:
            return arg[3:]
    return "compile"


def main():
    command = sys.argv[1:]
    if not command:
        return 0

    label = display_name(command)
    start = time.perf_counter()
    result = subprocess.run(command)
    elapsed = time.perf_counter() - start

    if result.returncode == 0:
        status = f"{ANSI_GREEN}SUCCESS{ANSI_RESET}"
    else:
        status = f"{ANSI_RED}FAILURE{ANSI_RESET}"

    print(f"[{status} | {elapsed:6.3f}s] {label}", file=sys.stderr, flush=True)

    return result.returncode


if __name__ == "__main__":
    raise SystemExit(main())
