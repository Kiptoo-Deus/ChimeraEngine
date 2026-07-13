#!/usr/bin/env python3
from __future__ import annotations

import pathlib
import platform
import sys


def main() -> int:
    if len(sys.argv) != 2:
        print("usage: validate_build_artifacts.py <build-dir>", file=sys.stderr)
        return 2

    build_dir = pathlib.Path(sys.argv[1])
    artefacts = build_dir / "ChimeraEngine_artefacts"
    if not artefacts.exists():
        print(f"Missing artefact directory: {artefacts}", file=sys.stderr)
        return 1

    required_suffixes = [".vst3"]
    if platform.system() == "Darwin":
        required_suffixes.extend([".app", ".component"])
    elif platform.system() == "Windows":
        required_suffixes.append(".exe")

    missing: list[str] = []
    for suffix in required_suffixes:
        if not any(path.name == f"Chimera Engine{suffix}" for path in artefacts.rglob(f"*{suffix}")):
            missing.append(suffix)

    if missing:
        print("Missing build artefacts: " + ", ".join(missing), file=sys.stderr)
        return 1

    return 0


if __name__ == "__main__":
    raise SystemExit(main())
