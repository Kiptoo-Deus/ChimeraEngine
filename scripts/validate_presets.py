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
VOICE_MODES = {"poly", "mono", "legato"}
ALTERNATE_MODES = {"off", "roundrobin", "random"}
MOD_SOURCES = {"velocity", "modwheel", "aftertouch", "lfo1", "lfo2", "pitcheg", "filtereg", "ampeg"}
MOD_DESTINATIONS = {"pitch", "cutoff", "amp", "pan"}


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
    voice_mode = data.get("voiceMode", "poly")
    if not isinstance(voice_mode, str) or voice_mode not in VOICE_MODES:
        errors.append(f"{path}: voiceMode must be poly, mono, or legato")

    portamento_time = data.get("portamentoTime", 0.0)
    if not isinstance(portamento_time, (int, float)) or portamento_time < 0.0 or portamento_time > 10.0:
        errors.append(f"{path}: portamentoTime must be between 0.0 and 10.0")

    pitch_bend_range = data.get("pitchBendRange", 2)
    if not isinstance(pitch_bend_range, int) or pitch_bend_range < 0 or pitch_bend_range > 48:
        errors.append(f"{path}: pitchBendRange must be between 0 and 48")

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

        for key in ("alternates", "releaseSamples"):
            values = element.get(key, [])
            if not isinstance(values, list) or not all(isinstance(v, str) and v for v in values):
                errors.append(f"{path}: element {index} {key} must be a string array")
            else:
                for sample_path in values:
                    if not (ROOT / sample_path).is_file():
                        errors.append(f"{path}: element {index} sample not found: {sample_path}")

        alternate_mode = element.get("alternateMode", "off")
        if not isinstance(alternate_mode, str) or alternate_mode not in ALTERNATE_MODES:
            errors.append(f"{path}: element {index} alternateMode is unsupported: {alternate_mode}")

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

        for key in ("ampDecay1", "ampDecay2"):
            value = element.get(key, 0.05)
            if not isinstance(value, (int, float)) or value < 0.0 or value > 10.0:
                errors.append(f"{path}: element {index} {key} must be between 0.0 and 10.0")

        amp_sustain = element.get("ampSustain", 1.0)
        if not isinstance(amp_sustain, (int, float)) or amp_sustain < 0.0 or amp_sustain > 1.0:
            errors.append(f"{path}: element {index} ampSustain must be between 0.0 and 1.0")

        amp_release = element.get("ampRelease", 0.0)
        if not isinstance(amp_release, (int, float)) or amp_release < 0.0 or amp_release > 20.0:
            errors.append(f"{path}: element {index} ampRelease must be between 0.0 and 20.0")

        for key in ("lfo1RateHz", "lfo2RateHz"):
            value = element.get(key, 0.0)
            if not isinstance(value, (int, float)) or value < 0.0 or value > 40.0:
                errors.append(f"{path}: element {index} {key} must be between 0.0 and 40.0")

        for key in ("lfo1CutoffDepth", "lfo2AmpDepth", "lfo2PanDepth"):
            value = element.get(key, 0.0)
            if not isinstance(value, (int, float)) or value < 0.0 or value > 1.0:
                errors.append(f"{path}: element {index} {key} must be between 0.0 and 1.0")

        for key, min_depth, max_depth in (("pitchEnvelope", -4800.0, 4800.0), ("filterEnvelope", -1.0, 1.0)):
            envelope = element.get(key, {})
            if envelope and not isinstance(envelope, dict):
                errors.append(f"{path}: element {index} {key} must be an object")
                continue
            if not isinstance(envelope, dict):
                envelope = {}
            for env_key, default, low, high in (
                ("attack", 0.0, 0.0, 10.0),
                ("decay1", 0.05, 0.0, 10.0),
                ("decay2", 0.05, 0.0, 10.0),
                ("sustain", 1.0, 0.0, 1.0),
                ("release", 0.0, 0.0, 20.0),
                ("depth", 0.0, min_depth, max_depth),
            ):
                value = envelope.get(env_key, default)
                if not isinstance(value, (int, float)) or value < low or value > high:
                    errors.append(f"{path}: element {index} {key}.{env_key} must be between {low} and {high}")

        mod_slots = element.get("modSlots", [])
        if not isinstance(mod_slots, list) or len(mod_slots) > 8:
            errors.append(f"{path}: element {index} modSlots must be a list with at most 8 slots")
        else:
            for slot_index, slot in enumerate(mod_slots):
                if not isinstance(slot, dict):
                    errors.append(f"{path}: element {index} modSlot {slot_index} must be an object")
                    continue
                source = slot.get("source", "velocity")
                destination = slot.get("destination", "amp")
                depth = slot.get("depth", 0.0)
                if source not in MOD_SOURCES:
                    errors.append(f"{path}: element {index} modSlot {slot_index} source is unsupported: {source}")
                if destination not in MOD_DESTINATIONS:
                    errors.append(f"{path}: element {index} modSlot {slot_index} destination is unsupported: {destination}")
                if not isinstance(depth, (int, float)) or depth < -4.0 or depth > 4.0:
                    errors.append(f"{path}: element {index} modSlot {slot_index} depth must be between -4.0 and 4.0")

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
