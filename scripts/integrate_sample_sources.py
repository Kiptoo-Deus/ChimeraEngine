#!/usr/bin/env python3
from __future__ import annotations

import argparse
import csv
import json
import pathlib
import sys
from datetime import datetime, timezone

AUDIO_SUFFIXES = {".wav", ".aif", ".aiff", ".flac"}
MAPPING_SUFFIXES = {".sfz"}
ARCHIVE_SUFFIXES = {".zip", ".xz", ".tar"}
APPROVED_LICENSES = {"CC0-1.0", "CC-BY-3.0"}


def archive_name_from_url(url: str) -> str:
    name = url.rstrip("/").split("/")[-1]
    return "Marie_Ork.zip" if name == "zip" else name


def is_audio(path: pathlib.Path) -> bool:
    return path.suffix.lower() in AUDIO_SUFFIXES


def is_mapping(path: pathlib.Path) -> bool:
    return path.suffix.lower() in MAPPING_SUFFIXES


def find_archive(source_root: pathlib.Path, group: str, url: str) -> pathlib.Path | None:
    name = archive_name_from_url(url)
    candidates = [
        source_root / "downloads" / group / name,
        source_root / "downloads" / name,
    ]
    for candidate in candidates:
        if candidate.is_file():
            return candidate
    return None


def extracted_roots(source_root: pathlib.Path, group: str) -> list[pathlib.Path]:
    extracted = source_root / "extracted"
    if group == "vsco2-ce":
        roots = [extracted / "vsco2-ce"]
    elif group == "salamander":
        roots = list((extracted / "salamander").glob("*"))
    else:
        roots = list((extracted / group).glob("*"))
    return [root for root in roots if root.is_dir()]


def count_files(roots: list[pathlib.Path]) -> tuple[int, int]:
    audio = 0
    mappings = 0
    for root in roots:
        for path in root.rglob("*"):
            if not path.is_file():
                continue
            audio += int(is_audio(path))
            mappings += int(is_mapping(path))
    return audio, mappings


def read_manifest(source_root: pathlib.Path) -> list[dict[str, str]]:
    manifest = source_root / "manifest.csv"
    if not manifest.is_file():
        return []
    with manifest.open(newline="", encoding="utf-8") as handle:
        return list(csv.DictReader(handle))


def build_index(source_root: pathlib.Path) -> dict[str, object]:
    rows = read_manifest(source_root)
    source_available = source_root.is_dir() and bool(rows)
    entries: list[dict[str, object]] = []
    failures: list[str] = []

    for row in rows:
        group = row.get("group", "").strip()
        name = row.get("name", "").strip()
        url = row.get("url", "").strip()
        license_name = row.get("license", "").strip()
        core = row.get("core", "").strip().lower() == "true"
        expected_size = int(row.get("size_bytes", "0") or 0)
        roots = extracted_roots(source_root, group)
        audio_count, mapping_count = count_files(roots)
        archive = find_archive(source_root, group, url)
        archive_size = archive.stat().st_size if archive is not None else 0
        extracted = audio_count > 0 or mapping_count > 0
        downloaded = archive is not None or extracted or (group == "vsco2-ce" and roots)

        if core and license_name not in APPROVED_LICENSES:
            failures.append(f"{name}: core source has unapproved license {license_name}")
        if core and not downloaded:
            failures.append(f"{name}: core source is missing")
        if core and archive is not None and expected_size > 0 and archive_size < expected_size * 0.88:
            failures.append(f"{name}: archive looks incomplete ({archive_size} < {expected_size})")

        entries.append(
            {
                "group": group,
                "name": name,
                "url": url,
                "license": license_name,
                "attribution": row.get("attribution", "").strip(),
                "core": core,
                "downloaded": downloaded,
                "extracted": extracted,
                "archive": str(archive) if archive is not None else "",
                "archiveSizeBytes": archive_size,
                "expectedSizeBytes": expected_size,
                "roots": [str(root) for root in roots],
                "audioFiles": audio_count,
                "mappingFiles": mapping_count,
            }
        )

    return {
        "generatedAt": datetime.now(timezone.utc).isoformat(),
        "sourceRoot": str(source_root),
        "available": source_available,
        "entryCount": len(entries),
        "coreEntryCount": sum(1 for entry in entries if entry["core"]),
        "downloadedEntryCount": sum(1 for entry in entries if entry["downloaded"]),
        "extractedEntryCount": sum(1 for entry in entries if entry["extracted"]),
        "audioFileCount": sum(int(entry["audioFiles"]) for entry in entries),
        "mappingFileCount": sum(int(entry["mappingFiles"]) for entry in entries),
        "failures": failures,
        "entries": entries,
    }


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--source-root", required=True, type=pathlib.Path)
    parser.add_argument("--output", required=True, type=pathlib.Path)
    parser.add_argument("--strict", action="store_true")
    args = parser.parse_args()

    index = build_index(args.source_root)
    args.output.parent.mkdir(parents=True, exist_ok=True)
    args.output.write_text(json.dumps(index, indent=2) + "\n", encoding="utf-8")

    manifest_copy = args.output.with_name("manifest.csv")
    manifest = args.source_root / "manifest.csv"
    if manifest.is_file():
        manifest_copy.write_text(manifest.read_text(encoding="utf-8"), encoding="utf-8")
    else:
        manifest_copy.write_text("group,name,url,license,attribution,size_bytes,core\n", encoding="utf-8")

    print(
        "Sample source index: "
        f"{index['downloadedEntryCount']}/{index['entryCount']} downloaded, "
        f"{index['audioFileCount']} audio files, {index['mappingFileCount']} mappings"
    )
    for failure in index["failures"]:
        print(f"sample source failure: {failure}", file=sys.stderr)

    return 1 if args.strict and index["failures"] else 0


if __name__ == "__main__":
    raise SystemExit(main())
