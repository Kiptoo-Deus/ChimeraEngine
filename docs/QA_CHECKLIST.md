# QA Checklist

- Configure and build with CMake on macOS.
- Configure and build with CMake on Windows.
- Run all unit tests with `ctest`.
- Run sample license audit.
- Confirm standalone, VST3, and AU build artefacts are present.
- Validate VST3 with pluginval or an equivalent validator.
- Validate AU on macOS.
- Load the plugin in at least one DAW on each supported platform.
- Verify parameter automation.
- Verify preset save and load.
- Verify CPU use at 16 voices.
- Verify no unlicensed samples are bundled.
