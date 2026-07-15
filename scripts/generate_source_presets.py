#!/usr/bin/env python3
from __future__ import annotations

import argparse
import json
import pathlib
import re
import shutil

NOTE_BASE = {
    "C": 0,
    "C#": 1,
    "DB": 1,
    "D": 2,
    "D#": 3,
    "EB": 3,
    "E": 4,
    "F": 5,
    "F#": 6,
    "GB": 6,
    "G": 7,
    "G#": 8,
    "AB": 8,
    "A": 9,
    "A#": 10,
    "BB": 10,
    "B": 11,
}

AUDIO_SUFFIXES = {".wav", ".aif", ".aiff", ".flac"}


def midi_note_from_name(name: str) -> int | None:
    match = re.search(r"(?<![A-Za-z])([A-Ga-g](?:#|b)?)(-?\d)(?!\d)", name)
    if not match:
        return None
    note = match.group(1).upper().replace("B", "B" if len(match.group(1)) == 1 else "B")
    if len(note) == 2 and note[1] == "B":
        note = note[0] + "B"
    octave = int(match.group(2))
    if note not in NOTE_BASE:
        return None
    return max(0, min(127, (octave + 1) * 12 + NOTE_BASE[note]))


def safe_name(name: str) -> str:
    clean = re.sub(r"[^A-Za-z0-9 _-]+", " ", name).strip()
    clean = re.sub(r"\s+", " ", clean)
    return clean[:80] or "Source Instrument"


def relative(path: pathlib.Path, root: pathlib.Path) -> str:
    return path.resolve().relative_to(root.resolve()).as_posix()


def choose_key_samples(files: list[pathlib.Path], maximum: int = 8) -> list[tuple[pathlib.Path, int]]:
    keyed = []
    for path in files:
        note = midi_note_from_name(path.stem)
        if note is not None:
            keyed.append((path, note))
    keyed.sort(key=lambda item: (item[1], item[0].as_posix()))
    if not keyed:
        return []
    if len(keyed) <= maximum:
        return keyed
    indexes = [round(i * (len(keyed) - 1) / (maximum - 1)) for i in range(maximum)]
    selected = []
    seen = set()
    for index in indexes:
        item = keyed[index]
        if item[0] not in seen:
            selected.append(item)
            seen.add(item[0])
    return selected


def choose_named_or_even_samples(files: list[pathlib.Path], maximum: int = 8) -> list[tuple[pathlib.Path, int]]:
    keyed = choose_key_samples(files, maximum)
    if keyed:
        return keyed

    ordered = sorted(files, key=lambda path: path.as_posix())
    if len(ordered) > maximum:
        indexes = [round(i * (len(ordered) - 1) / (maximum - 1)) for i in range(maximum)]
        ordered = [ordered[index] for index in indexes]

    start_note = 36
    return [(path, min(127, start_note + index * 3)) for index, path in enumerate(ordered)]


def ranges_for(notes: list[int]) -> list[tuple[int, int]]:
    ranges = []
    for index, note in enumerate(notes):
        if index == 0:
            low = 0
        else:
            low = (notes[index - 1] + note) // 2 + 1
        if index == len(notes) - 1:
            high = 127
        else:
            high = (note + notes[index + 1]) // 2
        ranges.append((max(0, low), min(127, high)))
    return ranges


def make_patch(name: str, category: str, samples: list[tuple[pathlib.Path, int]], project_root: pathlib.Path, release: float) -> dict:
    notes = [note for _, note in samples]
    key_ranges = ranges_for(notes)
    elements = []
    for (path, note), (low, high) in zip(samples, key_ranges):
        elements.append(
            {
                "sample": relative(path, project_root),
                "rootKey": note,
                "keyRange": [low, high],
                "velocityRange": [1, 127],
                "level": 0.82,
                "pan": 0.0,
                "filterType": "lowPassWide",
                "ampAttack": 0.004,
                "ampDecay1": 0.04,
                "ampDecay2": 0.12,
                "ampSustain": 0.92,
                "ampRelease": release,
                "modSlots": [
                    {"source": "velocity", "destination": "amp", "depth": 0.2, "enabled": True},
                    {"source": "modwheel", "destination": "cutoff", "depth": 0.35, "enabled": True},
                    {"source": "aftertouch", "destination": "amp", "depth": 0.12, "enabled": True},
                ],
            }
        )
    return {"format": "chpatch", "version": 1, "name": name, "category": category, "elements": elements}


def write_patch(output_dir: pathlib.Path, patch: dict) -> pathlib.Path:
    output_dir.mkdir(parents=True, exist_ok=True)
    path = (output_dir / f"{safe_name(patch['name'])}.chpatch").resolve()
    path.write_text(json.dumps(patch, indent=2) + "\n", encoding="utf-8")
    return path


def source_entries(index_file: pathlib.Path) -> list[dict]:
    if not index_file.is_file():
        return []
    return json.loads(index_file.read_text(encoding="utf-8")).get("entries", [])


def audio_files(root: pathlib.Path) -> list[pathlib.Path]:
    return sorted(path for path in root.rglob("*") if path.is_file() and path.suffix.lower() in AUDIO_SUFFIXES)


def generate_salamander(project_root: pathlib.Path, output_dir: pathlib.Path, roots: list[pathlib.Path]) -> list[pathlib.Path]:
    generated = []
    for root in roots:
        files = [
            path for path in audio_files(root)
            if re.match(r"^[A-G](?:#|b)?-?\d+v\d+$", path.stem, re.IGNORECASE)
        ]
        samples = choose_key_samples(files, 8)
        if samples:
            generated.append(write_patch(output_dir, make_patch("Salamander Grand Piano", "Piano", samples, project_root, 1.2)))
    return generated


def generate_vsco(project_root: pathlib.Path, output_dir: pathlib.Path, roots: list[pathlib.Path]) -> list[pathlib.Path]:
    generated = []
    seen_names = set()

    def category_for(relative_folder: pathlib.Path) -> str:
        first = relative_folder.parts[0].lower() if relative_folder.parts else ""
        second = relative_folder.parts[1].lower() if len(relative_folder.parts) > 1 else ""
        if first == "keys" and "piano" in second:
            return "Piano"
        if first == "keys":
            return "Keys"
        if "percussion" in first or "drum" in first:
            return "Drums"
        if first in {"strings", "brass", "woodwinds"}:
            return first.title()
        return "Source"

    def release_for(relative_folder: pathlib.Path) -> float:
        text = relative_folder.as_posix().lower()
        if any(word in text for word in ("stac", "spic", "pizz", "hit", "drum", "perc", "short")):
            return 0.18
        if any(word in text for word in ("piano", "harp", "glock", "xylo", "marimba")):
            return 0.9
        if any(word in text for word in ("strings", "vib", "sus", "trem")):
            return 0.7
        return 0.45

    for root in roots:
        folders = []
        for folder in root.rglob("*"):
            if not folder.is_dir():
                continue
            direct_audio = [path for path in folder.iterdir() if path.is_file() and path.suffix.lower() in AUDIO_SUFFIXES]
            if direct_audio:
                folders.append((folder, direct_audio))

        for folder, files in sorted(folders, key=lambda item: item[0].as_posix().lower()):
            relative_folder = folder.relative_to(root)
            if "temp" in [part.lower() for part in relative_folder.parts]:
                continue

            display = " ".join(part.replace("_", " ").replace("-", " ").title() for part in relative_folder.parts)
            name = safe_name("VSCO " + display)
            if name.lower() in seen_names:
                continue

            samples = choose_named_or_even_samples(files, 8)
            if samples:
                seen_names.add(name.lower())
                generated.append(write_patch(output_dir, make_patch(name, category_for(relative_folder), samples, project_root, release_for(relative_folder))))
    return generated


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--project-root", required=True, type=pathlib.Path)
    parser.add_argument("--index", required=True, type=pathlib.Path)
    parser.add_argument("--output-dir", required=True, type=pathlib.Path)
    args = parser.parse_args()

    project_root = args.project_root.resolve()
    output_dir = args.output_dir.resolve()

    if output_dir.exists():
        shutil.rmtree(output_dir)
    output_dir.mkdir(parents=True, exist_ok=True)

    generated: list[pathlib.Path] = []
    for entry in source_entries(args.index):
        if not entry.get("extracted"):
            continue
        roots = [pathlib.Path(root) for root in entry.get("roots", []) if pathlib.Path(root).is_dir()]
        if entry.get("group") == "salamander":
            generated.extend(generate_salamander(project_root, output_dir, roots))
        elif entry.get("group") == "vsco2-ce":
            generated.extend(generate_vsco(project_root, output_dir, roots))

    print(f"Generated {len(generated)} source presets in {output_dir}")
    for path in generated:
        print(f"- {path.relative_to(project_root)}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
