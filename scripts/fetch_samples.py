#!/usr/bin/env python3
"""Controlled sample acquisition entry point.

The downloader is intentionally conservative. Each source must be implemented
with an explicit URL list or official API integration before files are admitted
to samples/.
"""

from __future__ import annotations

import dataclasses
import pathlib

ROOT = pathlib.Path(__file__).resolve().parents[1]
SAMPLES = ROOT / "samples"


@dataclasses.dataclass(frozen=True)
class Source:
    domain: str
    license_expectation: str
    notes: str


ALLOWLIST: dict[str, Source] = {
    "theremin.music.uiowa.edu": Source("theremin.music.uiowa.edu", "PUBLIC_DOMAIN", "instrument sample archive"),
    "vsco2.community": Source("vsco2.community", "CC0", "community sample library"),
    "vis.versilstudios.com": Source("vis.versilstudios.com", "CC0", "community orchestra edition"),
    "archive.org": Source("archive.org", "VERIFY_PER_ITEM", "public archive used only for approved packs"),
    "sonatina-symphonic-orchestra.com": Source("sonatina-symphonic-orchestra.com", "CC-BY", "attribution required"),
    "freesound.org": Source("freesound.org", "CC0_API_FILTER_REQUIRED", "API license must be Creative Commons 0"),
    "github.com": Source("github.com", "VERIFY_PER_FILE", "only approved open sound assets"),
}


def main() -> int:
    SAMPLES.mkdir(exist_ok=True)
    print("Sample fetcher is configured with the approved allowlist.")
    print("No downloads are performed until a source-specific verifier is implemented.")
    for name, source in sorted(ALLOWLIST.items()):
        print(f"- {name}: {source.license_expectation}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
