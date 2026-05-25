# Texture Channel Packer

A focused desktop tool for game-dev and VFX artists: drag textures into R/G/B/A slots, route source channels, preview the packed result live, and export to PNG / TGA. DDS/BC compression and Unreal export presets land in later phases.

> **Status:** v0.1.0 — first usable release. Drag four textures, pick channel sources, preview live, export PNG/TGA. Project save/load (JSON), keyboard shortcuts, dark theme, toast notifications. CLI mode and DDS/BC compression land in Phase 5+.

---

## Goals

- Native-feeling Windows desktop tool, fast cold-start, GPU-driven live preview.
- Hard separation between a pure-C++ processing core and the Qt UI.
- Architecture that survives the addition of batch mode, presets, CLI mode, and Unreal export without rewrites.

## Tech stack

| Layer            | Choice                                             |
| ---------------- | -------------------------------------------------- |
| Language         | C++20                                              |
| UI               | Qt 6 Widgets (dynamic-linked, LGPLv3 compliant)    |
| Image I/O        | OpenImageIO 2.5+ (PNG, TGA, JPG, EXR, TIFF)        |
| GPU preview      | `QOpenGLWidget` + GLSL 3.3 core                    |
| Build            | CMake 3.24+, vcpkg manifest mode                   |
| Tests            | Catch2 v3 (core), Qt Test (UI logic)               |
| CI               | GitHub Actions, Windows runner                     |

## Building

### Prerequisites

- **Visual Studio 2022 / 18 Community** with the "Desktop development with C++" workload (provides MSVC, CMake, Ninja).
- **vcpkg** — see [Setting up vcpkg](#setting-up-vcpkg) below.

### Setting up vcpkg

vcpkg is consumed as a git submodule in `external/vcpkg/`. After cloning:

```pwsh
git submodule update --init --recursive
.\external\vcpkg\bootstrap-vcpkg.bat -disableMetrics
```

Set `VCPKG_ROOT` for the current shell (or persistently for the user):

```pwsh
$env:VCPKG_ROOT = "$PWD\external\vcpkg"
```

### Configure & build

```pwsh
cmake --preset windows-msvc-debug
cmake --build --preset windows-msvc-debug
ctest --preset windows-msvc-debug --output-on-failure
```

Release builds:

```pwsh
cmake --preset windows-msvc-release
cmake --build --preset windows-msvc-release
```

## License

This project is licensed under the [MIT License](LICENSE).

It uses Qt 6 under the LGPLv3 (dynamic-linked) — see [LICENSE-THIRD-PARTY.txt](LICENSE-THIRD-PARTY.txt) for the full third-party attribution and links to the Qt source. OpenImageIO and Catch2 are used under their respective permissive licenses.

You can use, modify, and redistribute this software (including commercially and for client work) under the terms of the MIT License. Binary redistributions must include the third-party attribution file so end users have the LGPL notice for Qt.
