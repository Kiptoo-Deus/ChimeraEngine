#!/usr/bin/env python3
"""Fetch approved third-party sample source archives for local curation.

Raw downloads live in sample_sources/, which is intentionally ignored by git.
Only curated, license-audited samples should be copied into samples/.
"""

from __future__ import annotations

import argparse
import csv
import dataclasses
import pathlib
import shutil
import subprocess
import sys
import urllib.request

ROOT = pathlib.Path(__file__).resolve().parents[1]
SOURCES = ROOT / "sample_sources"
DOWNLOADS = SOURCES / "downloads"
EXTRACTED = SOURCES / "extracted"
MANIFEST = SOURCES / "manifest.csv"


@dataclasses.dataclass(frozen=True)
class SourceItem:
    group: str
    name: str
    url: str
    license: str
    attribution: str
    size_bytes: int
    core: bool = False
    extract: bool = True


SOURCES_MANIFEST: tuple[SourceItem, ...] = (
    SourceItem("vsco2-ce", "VSCO 2 Community Edition", "https://github.com/sgossner/VSCO-2-CE.git", "CC0-1.0", "", 2_390_645_760, True, False),
    SourceItem("salamander", "Salamander Grand Piano 44k16", "http://freepats.zenvoid.org/Piano/SalamanderGrandPiano/SalamanderGrandPianoV3+20161209_44khz16bit.tar.xz", "CC-BY-3.0", "Salamander Grand Piano by Alexander Holm", 394_000_000, True, True),
    SourceItem("karoryfer", "Karoryfer Growlybass", "https://github.com/sfzinstruments/karoryfer.growlybass/releases/download/v1.002/Karoryfer.Growlybass.v1.002.zip", "CC0-1.0", "", 167_454_256, True),
    SourceItem("karoryfer", "Karoryfer Shinyguitar", "https://github.com/sfzinstruments/karoryfer.shinyguitar/releases/download/v1.002/Karoryfer.Shinyguitar.v1.002.zip", "CC0-1.0", "", 368_411_844, True),
    SourceItem("karoryfer", "Karoryfer Emilyguitar", "https://github.com/sfzinstruments/karoryfer.emilyguitar/releases/download/v1.001/Karoryfer.Emilyguitar.v1.001.zip", "CC0-1.0", "", 103_484_350, True),
    SourceItem("karoryfer", "Karoryfer Bear Sax", "https://github.com/sfzinstruments/karoryfer.bear-sax/releases/download/v1.004/Karoryfer.Bear_Sax.v1.004.zip", "CC0-1.0", "", 131_505_201, True),
    SourceItem("karoryfer", "Karoryfer War Tuba", "https://github.com/sfzinstruments/karoryfer.war-tuba/releases/download/v1.002/Karoryfer_War_Tuba_v1002.zip", "CC0-1.0", "", 109_135_588, True),
    SourceItem("karoryfer", "Karoryfer Big Rusty Drums", "https://github.com/sfzinstruments/karoryfer.big-rusty-drums/releases/download/v1.100/Big_Rusty_Drums_1100.zip", "CC0-1.0", "", 619_600_370, True),
    SourceItem("karoryfer", "Karoryfer Swirly Drums", "https://github.com/sfzinstruments/karoryfer.swirly-drums/releases/download/v1.104/Swirly.Drums_1104.zip", "CC0-1.0", "", 868_594_745, True),
    SourceItem("karoryfer", "Karoryfer Scarypiano", "https://github.com/sfzinstruments/karoryfer.scarypiano/releases/download/v1.002/Karoryfer.Scarypiano.v1.002.zip", "CC0-1.0", "", 359_098_429, True),
    SourceItem("karoryfer", "Karoryfer Bigcat Cello", "https://github.com/sfzinstruments/karoryfer-bigcat.cello/releases/download/v1.001/Karoryfer_Bigcat_cello.v1.001.zip", "CC0-1.0", "", 132_605_500, True),
    SourceItem("karoryfer", "Karoryfer Horse Pulse", "https://github.com/sfzinstruments/Karoryfer.HorsePulse/releases/download/v1.000/Karoryfer_Horse_Pulse_1000.zip", "CC0-1.0", "", 147_505_176),
    SourceItem("karoryfer", "The Hat With The Phat", "https://github.com/sfzinstruments/Karoryfer.TheHatWithThePhat/releases/download/v1.001/The.Hat.With.The.Phat.bank.zip", "CC0-1.0", "", 666_069_141),
    SourceItem("karoryfer", "Osiris Piano", "https://github.com/sfzinstruments/Osiris_Piano/releases/download/v0.925/Osiris_Piano_0925.zip", "CC0-1.0", "", 458_671_376),
    SourceItem("karoryfer", "DSmolken Double Bass", "https://github.com/sfzinstruments/dsmolken.double-bass/releases/download/v1.001/DSmolken.double_bass.v1.001.zip", "CC0-1.0", "", 265_232_156),
    SourceItem("karoryfer", "Karoryfer 272 Merry Orks", "https://github.com/sfzinstruments/karoryfer.272-merry-orks/releases/download/v1.001/Karoryfer.272_Merry_Orks.v1.001.zip", "CC0-1.0", "", 40_439_723),
    SourceItem("karoryfer", "Karoryfer Big Little Bass", "https://github.com/sfzinstruments/karoryfer.big-little-bass/releases/download/v1.000/Big_Little_Bass_1000.zip", "CC0-1.0", "", 263_554_264),
    SourceItem("karoryfer", "Karoryfer Black and Blue Basses", "https://github.com/sfzinstruments/karoryfer.black-and-blue-basses/releases/download/v1.002/Black_And_Blue_Basses_1002.zip", "CC0-1.0", "", 1_008_690_586),
    SourceItem("karoryfer", "Karoryfer Black and Green Guitars", "https://github.com/sfzinstruments/karoryfer.black-and-green-guitars/releases/download/v1.000/Karoryfer_Black_And_Green_Guitars_1000.zip", "CC0-1.0", "", 481_838_512),
    SourceItem("karoryfer", "Karoryfer Caveman Cosmonaut", "https://github.com/sfzinstruments/karoryfer.caveman-cosmonaut/releases/download/v1.001/Karoryfer.Caveman_Cosmonaut.v1.001.zip", "CC0-1.0", "", 97_546_345),
    SourceItem("karoryfer", "Karoryfer Cowsynth", "https://github.com/sfzinstruments/karoryfer.cowsynth/releases/download/v1.001/Karoryfer.Cowsynth.v1.001.zip", "CC0-1.0", "", 14_072_509),
    SourceItem("karoryfer", "Karoryfer Ergo EUB", "https://github.com/sfzinstruments/karoryfer.ergo/releases/download/v1.001/Karoryfer.Ergo_EUB.v1.001.zip", "CC0-1.0", "", 200_612_492),
    SourceItem("karoryfer", "Karoryfer Fashionbass", "https://github.com/sfzinstruments/karoryfer.fashionbass/releases/download/v1.001/Karoryfer.Fashionbass.v1.001.zip", "CC0-1.0", "", 316_328_061),
    SourceItem("karoryfer", "Karoryfer Frankensnare", "https://github.com/sfzinstruments/karoryfer.frankensnare/releases/download/v2.100/Frankensnare_2100.zip", "CC0-1.0", "", 365_639_598),
    SourceItem("karoryfer", "Karoryfer Gogodze Phu Vol I", "https://github.com/sfzinstruments/karoryfer.gogodze-phu-vol-i/releases/download/v1.001/Karoryfer.Gogodze_Phu_vol_I.v1.001.zip", "CC0-1.0", "", 31_249_753),
    SourceItem("karoryfer", "Karoryfer Gogodze Phu Vol II", "https://github.com/sfzinstruments/karoryfer.gogodze-phu-vol-ii/releases/download/v1.001/Karoryfer_Gogodze_Phu_vol_II.v1.001.zip", "CC0-1.0", "", 139_517_148),
    SourceItem("karoryfer", "Karoryfer Meatbass", "https://github.com/sfzinstruments/karoryfer.meatbass/releases/download/v1.001/Karoryfer.Meatbass.v1.001.zip", "CC0-1.0", "", 255_425_351),
    SourceItem("karoryfer", "Karoryfer Pastabass", "https://github.com/sfzinstruments/karoryfer.pastabass/releases/download/v1.101/Karoryfer.Pastabass.v1.101.zip", "CC0-1.0", "", 315_271_331),
    SourceItem("karoryfer", "Karoryfer Sneakybass", "https://github.com/sfzinstruments/karoryfer.sneakybass/releases/download/v1.000/Sneakybass_v1.000.zip", "CC0-1.0", "", 340_462_855),
    SourceItem("karoryfer", "Karoryfer Squidpipes", "https://github.com/sfzinstruments/karoryfer.squidpipes/releases/download/v1.001/karoryfer.squidpipes-v1.001.zip", "CC0-1.0", "", 44_187_394),
    SourceItem("karoryfer", "Karoryfer String Cyborgs", "https://github.com/sfzinstruments/karoryfer.string-cyborgs/releases/download/v1.001/Karoryfer.String_Cyborgs.v1.001.zip", "CC0-1.0", "", 63_765_812),
    SourceItem("karoryfer", "Karoryfer Swagbass", "https://github.com/sfzinstruments/karoryfer.swagbass/releases/download/v1.001/Karoryfer.Swagbass.v1.001.zip", "CC0-1.0", "", 144_579_232),
    SourceItem("karoryfer", "Karoryfer Unruly Drums", "https://github.com/sfzinstruments/karoryfer.unruly-drums/releases/download/v1.100/Unruly_Drums_1100.zip", "CC0-1.0", "", 676_343_466),
    SourceItem("karoryfer", "Karoryfer Weresax", "https://github.com/sfzinstruments/karoryfer.weresax/releases/download/v1.003/Karoryfer.Weresax.v.1.003.zip", "CC0-1.0", "", 197_565_664),
    SourceItem("karoryfer", "Virtuosity Drums", "https://github.com/sfzinstruments/virtuosity_drums/releases/download/v0.925/Virtuosity_Drums_v0.925.zip", "CC0-1.0", "", 1_227_151_376),
    SourceItem("karoryfer", "Zamenhof Stickmusterstuch Error", "https://github.com/sfzinstruments/zamenhof_stickmusterstuch_error/releases/download/v1.000/Zamenhof_Stickmusterstuch_Error.Eins_and_Zwei.v1.000.zip", "CC0-1.0", "", 79_010_316),
    SourceItem("karoryfer", "Marie Ork", "https://www.karoryfer.com/karoryfer-samples/wydawnictwa/marie-ork/zip", "NON_CC0_REVIEW_REQUIRED", "Review license before bundling", 684_363_661, False),
)


def selected_items(mode: str) -> list[SourceItem]:
    if mode == "core":
        return [item for item in SOURCES_MANIFEST if item.core]
    if mode == "all":
        return list(SOURCES_MANIFEST)
    return [item for item in SOURCES_MANIFEST if item.group == mode]


def write_manifest() -> None:
    SOURCES.mkdir(exist_ok=True)
    with MANIFEST.open("w", newline="") as handle:
        writer = csv.writer(handle)
        writer.writerow(["group", "name", "url", "license", "attribution", "size_bytes", "core"])
        for item in SOURCES_MANIFEST:
            writer.writerow([item.group, item.name, item.url, item.license, item.attribution, item.size_bytes, item.core])


def destination_for(item: SourceItem) -> pathlib.Path:
    if item.url.endswith(".git"):
        return EXTRACTED / item.group
    name = item.url.rstrip("/").split("/")[-1]
    if name == "zip":
        name = "Marie_Ork.zip"
    return DOWNLOADS / item.group / name


def download_url(item: SourceItem) -> pathlib.Path:
    destination = destination_for(item)
    destination.parent.mkdir(parents=True, exist_ok=True)
    if destination.exists() and destination.stat().st_size > 0:
        print(f"present: {destination.relative_to(ROOT)}")
        return destination
    print(f"downloading: {item.name}")
    with urllib.request.urlopen(item.url) as response, destination.open("wb") as handle:
        shutil.copyfileobj(response, handle)
    return destination


def clone_repo(item: SourceItem) -> pathlib.Path:
    destination = destination_for(item)
    if (destination / ".git").is_dir():
        print(f"present: {destination.relative_to(ROOT)}")
        return destination
    destination.parent.mkdir(parents=True, exist_ok=True)
    print(f"cloning: {item.name}")
    subprocess.run(["git", "clone", "--depth", "1", item.url, str(destination)], check=True)
    return destination


def extract_archive(path: pathlib.Path, item: SourceItem) -> pathlib.Path:
    target = EXTRACTED / item.group / path.stem.replace(".tar", "")
    if target.exists():
        print(f"extracted: {target.relative_to(ROOT)}")
        return target
    target.mkdir(parents=True, exist_ok=True)
    print(f"extracting: {path.name}")
    if path.suffix == ".zip":
        shutil.unpack_archive(str(path), str(target), "zip")
    elif path.name.endswith(".tar.xz"):
        shutil.unpack_archive(str(path), str(target), "xztar")
    else:
        print(f"skip extract, unsupported archive: {path.name}")
    return target


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--set", choices=["core", "all", "vsco2-ce", "salamander", "karoryfer"], default="core")
    parser.add_argument("--plan", action="store_true")
    parser.add_argument("--no-extract", action="store_true")
    args = parser.parse_args()

    write_manifest()
    items = selected_items(args.set)
    total = sum(item.size_bytes for item in items)
    print(f"{len(items)} items selected, estimated compressed size {total / (1024 ** 3):.2f} GiB")
    print(f"manifest: {MANIFEST.relative_to(ROOT)}")

    if args.plan:
        for item in items:
            marker = "core" if item.core else "full"
            print(f"- [{marker}] {item.group}: {item.name} ({item.size_bytes / (1024 ** 2):.1f} MiB, {item.license})")
        return 0

    for item in items:
        path = clone_repo(item) if item.url.endswith(".git") else download_url(item)
        if item.extract and not args.no_extract:
            extract_archive(path, item)
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
