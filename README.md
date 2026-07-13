# Chimera Engine

Chimera Engine is an original workstation-inspired, element-based sample instrument plugin built with JUCE and CMake.

The project targets VST3, AU on macOS, and a standalone app. The engine is organized around layered elements, multisample zones, envelopes, LFOs, filters, modulation, effects, and performance presets.

## Build

Requirements:

- CMake 3.24+
- C++20 compiler
- Python 3.10+
- Platform plugin SDK requirements handled by JUCE

Configure and build:

```sh
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --config Release
ctest --test-dir build --output-on-failure
```

## Sample Sources & Licensing

Bundled samples must be acquired only through `scripts/fetch_samples.py` and must have a valid entry in `samples/LICENSES.csv` before they are copied into the sample library.

The license audit runs in CI:

```sh
python3 scripts/audit_sample_licenses.py
```

Allowed licenses for bundled audio are CC0, public domain, and CC-BY with attribution text.

## Current Status

The instrument is not yet a complete production release.
