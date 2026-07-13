#!/usr/bin/env python3
from __future__ import annotations

import csv
import math
import pathlib
import wave

ROOT = pathlib.Path(__file__).resolve().parents[1]
SAMPLES = ROOT / "samples"
SYNTH = SAMPLES / "Synth"
LICENSES = SAMPLES / "LICENSES.csv"
SAMPLE_RATE = 48_000
DURATION_SECONDS = 2.0
ROOT_NOTE = 60
FREQUENCY = 261.625565


def write_24bit_wav(path: pathlib.Path, values: list[float]) -> None:
    path.parent.mkdir(parents=True, exist_ok=True)
    with wave.open(str(path), "wb") as handle:
        handle.setnchannels(1)
        handle.setsampwidth(3)
        handle.setframerate(SAMPLE_RATE)
        frames = bytearray()
        for value in values:
            scaled = max(-1.0, min(1.0, value)) * 0x7FFFFF
            integer = int(round(scaled))
            frames.extend(integer.to_bytes(3, byteorder="little", signed=True))
        handle.writeframes(bytes(frames))


def render(shape: str) -> list[float]:
    count = int(SAMPLE_RATE * DURATION_SECONDS)
    out: list[float] = []
    for index in range(count):
        phase = (index * FREQUENCY / SAMPLE_RATE) % 1.0
        if shape == "sine":
            value = math.sin(phase * math.tau)
        elif shape == "saw":
            value = phase * 2.0 - 1.0
        elif shape == "square":
            value = 1.0 if phase < 0.5 else -1.0
        elif shape == "triangle":
            value = 4.0 * abs(phase - 0.5) - 1.0
        else:
            raise ValueError(shape)

        fade = min(1.0, index / 256, (count - index - 1) / 256)
        out.append(value * 0.7 * fade)
    return out


def read_license_rows() -> list[dict[str, str]]:
    if not LICENSES.exists():
        return []
    with LICENSES.open(newline="", encoding="utf-8") as handle:
        return list(csv.DictReader(handle))


def write_license_rows(rows: list[dict[str, str]]) -> None:
    fieldnames = ["filename", "source_url", "license", "attribution_required", "attribution_text"]
    with LICENSES.open("w", newline="", encoding="utf-8") as handle:
        writer = csv.DictWriter(handle, fieldnames=fieldnames)
        writer.writeheader()
        writer.writerows(rows)


def main() -> int:
    rows = read_license_rows()
    by_name = {row["filename"]: row for row in rows}
    for shape in ["sine", "saw", "square", "triangle"]:
        filename = f"Synth/{shape}_C4_24bit.wav"
        write_24bit_wav(SAMPLES / filename, render(shape))
        by_name[filename] = {
            "filename": filename,
            "source_url": "generated://chimera-engine/scripts/generate_synth_samples.py",
            "license": "CC0",
            "attribution_required": "false",
            "attribution_text": "",
        }

    write_license_rows([by_name[name] for name in sorted(by_name)])
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
