#!/usr/bin/env python3
import csv
import pathlib
import sys

ROOT = pathlib.Path(__file__).resolve().parents[1]
SAMPLES = ROOT / "samples"
LICENSES = SAMPLES / "LICENSES.csv"
VALID_LICENSES = {"CC0", "PUBLIC_DOMAIN", "CC-BY"}
AUDIO_SUFFIXES = {".wav", ".flac", ".aiff", ".aif"}


def main() -> int:
    if not LICENSES.exists():
        print("samples/LICENSES.csv is missing", file=sys.stderr)
        return 1

    required = {"filename", "source_url", "license", "attribution_required", "attribution_text"}
    with LICENSES.open(newline="", encoding="utf-8") as handle:
        reader = csv.DictReader(handle)
        if set(reader.fieldnames or []) != required:
            print("LICENSES.csv header is invalid", file=sys.stderr)
            return 1
        rows = list(reader)

    if not rows and not LICENSES.read_text(encoding="utf-8").strip():
        print("LICENSES.csv header is invalid", file=sys.stderr)
        return 1

    seen = {}
    for row in rows:
        filename = row.get("filename", "").strip()
        license_name = row.get("license", "").strip()
        attribution_required = row.get("attribution_required", "").strip().lower()
        attribution_text = row.get("attribution_text", "").strip()

        if license_name not in VALID_LICENSES:
            print(f"{filename}: invalid license {license_name!r}", file=sys.stderr)
            return 1
        if license_name == "CC-BY" and (attribution_required != "true" or not attribution_text):
            print(f"{filename}: CC-BY sample needs attribution text", file=sys.stderr)
            return 1
        seen[filename] = row

    failures = []
    for path in SAMPLES.rglob("*"):
        if path.is_file() and path.suffix.lower() in AUDIO_SUFFIXES:
            rel = path.relative_to(SAMPLES).as_posix()
            if rel not in seen:
                failures.append(rel)

    if failures:
        print("Audio files without license rows:", file=sys.stderr)
        for rel in failures:
            print(f"  {rel}", file=sys.stderr)
        return 1

    return 0


if __name__ == "__main__":
    raise SystemExit(main())
