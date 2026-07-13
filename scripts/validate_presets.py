#!/usr/bin/env python3
from __future__ import annotations

import json
import pathlib
import sys

ROOT = pathlib.Path(__file__).resolve().parents[1]
PRESETS = ROOT / "presets"
FILTER_TYPES = {
    "bypass",
    "lowPass6",
    "lowPass12",
    "lowPass24",
    "lowPassWide",
    "lowPassNarrow",
    "highPass6",
    "highPass12",
    "highPass24",
    "highPassWide",
    "bandPass12",
    "bandPass24",
    "bandPassWide",
    "bandPassNarrow",
    "notch",
    "peak",
    "lowShelf",
    "highShelf",
}


def fail(message: str) -> int:
    print(message, file=sys.stderr)
    return 1


def validate_patch(path: pathlib.Path) -> list[str]:
    errors: list[str] = []
    try:
        data = json.loads(path.read_text(encoding="utf-8"))
    except json.JSONDecodeError as exc:
        return [f"{path}: invalid JSON: {exc}"]

    if data.get("format") != "chpatch":
        errors.append(f"{path}: format must be chpatch")
    if not data.get("name"):
        errors.append(f"{path}: missing name")
    if not data.get("category"):
        errors.append(f"{path}: missing category")

    elements = data.get("elements")
    if not isinstance(elements, list) or not elements:
        errors.append(f"{path}: elements must be a non-empty list")
        return errors

    for index, element in enumerate(elements):
        if not isinstance(element, dict):
            errors.append(f"{path}: element {index} must be an object")
            continue
        sample = element.get("sample")
        if not isinstance(sample, str) or not sample:
            errors.append(f"{path}: element {index} missing sample")
        elif not (ROOT / sample).is_file():
            errors.append(f"{path}: element {index} sample not found: {sample}")

        for key, low_default, high_default in [("keyRange", 0, 127), ("velocityRange", 1, 127)]:
            value = element.get(key, [low_default, high_default])
            if not (isinstance(value, list) and len(value) == 2 and all(isinstance(v, int) for v in value)):
                errors.append(f"{path}: element {index} {key} must be two integers")
            elif value[0] > value[1]:
                errors.append(f"{path}: element {index} {key} low is greater than high")

        level = element.get("level", 1.0)
        if not isinstance(level, (int, float)) or level < 0.0 or level > 2.0:
            errors.append(f"{path}: element {index} level must be between 0.0 and 2.0")

        pan = element.get("pan", 0.0)
        if not isinstance(pan, (int, float)) or pan < -1.0 or pan > 1.0:
            errors.append(f"{path}: element {index} pan must be between -1.0 and 1.0")

        tuning_cents = element.get("tuningCents", 0.0)
        if not isinstance(tuning_cents, (int, float)) or tuning_cents < -1200.0 or tuning_cents > 1200.0:
            errors.append(f"{path}: element {index} tuningCents must be between -1200.0 and 1200.0")

        filter_type = element.get("filterType", "lowPass12")
        if filter_type not in FILTER_TYPES:
            errors.append(f"{path}: element {index} filterType is unsupported: {filter_type}")

        amp_attack = element.get("ampAttack", 0.0)
        if not isinstance(amp_attack, (int, float)) or amp_attack < 0.0 or amp_attack > 10.0:
            errors.append(f"{path}: element {index} ampAttack must be between 0.0 and 10.0")

        amp_sustain = element.get("ampSustain", 1.0)
        if not isinstance(amp_sustain, (int, float)) or amp_sustain < 0.0 or amp_sustain > 1.0:
            errors.append(f"{path}: element {index} ampSustain must be between 0.0 and 1.0")

        amp_release = element.get("ampRelease", 0.0)
        if not isinstance(amp_release, (int, float)) or amp_release < 0.0 or amp_release > 20.0:
            errors.append(f"{path}: element {index} ampRelease must be between 0.0 and 20.0")

    return errors


def main() -> int:
    patches = sorted(PRESETS.rglob("*.chpatch"))
    if not patches:
        return fail("No .chpatch presets found")

    errors: list[str] = []
    for path in patches:
        errors.extend(validate_patch(path))

    if errors:
        for error in errors:
            print(error, file=sys.stderr)
        return 1

    return 0


if __name__ == "__main__":
    raise SystemExit(main())
