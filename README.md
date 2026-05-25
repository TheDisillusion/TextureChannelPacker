# Texture Channel Packer

A small native desktop tool for packing R/G/B/A channels of texture maps —
AO, roughness, metallic, normal, height, whatever — into a single output
without firing up Photoshop or Substance Designer for a thirty-second task.

Built for game and VFX artists who want one window, one drag, one click,
one packed texture.

**[➜ Download the latest Windows build](https://github.com/TheDisillusion/TextureChannelPacker/releases/latest)**
&nbsp;·&nbsp;
[All releases](https://github.com/TheDisillusion/TextureChannelPacker/releases)
&nbsp;·&nbsp;
[Build from source](#building-from-source)

---

## What it does

Drop up to four textures into the four destination slots (R, G, B, A).
Choose which channel of each input feeds the corresponding destination.
The packed result renders live on the GPU as you change things. Hit
Export when it looks right.

Supported output formats:

| Format | Bit depth | Notes |
| --- | --- | --- |
| PNG | 8 / 16-bit | Universal. Default. |
| TGA | 8-bit | For pipelines that still want Targa. |
| DDS | 8-bit | With BC1 / BC3 / BC5 / BC7 compression, or uncompressed RGBA8. |

Built-in routing presets cover the most common cases:

- **Unreal ORM** — AO → R, Roughness → G, Metallic → B
- **Unreal MRA** — Metallic → R, Roughness → G, AO → B
- **Normal + Height** — Normal XY → RG, Height → B
- **Diffuse + Specular Alpha** — Diffuse → RGB, Specular luminance → A

## Quick start (GUI)

1. Download the latest release zip, unpack it, run `TextureChannelPacker.exe`.
2. Drag textures onto the R / G / B / A slots on the left.
3. On the right panel pick the format (PNG / TGA / DDS), and for DDS pick
   the BC variant.
4. Click **Export…**.

Useful keys when the preview is focused: `F` fit, `1` actual size,
`R G B A` isolate one channel as greyscale, wheel zooms around the cursor.

Project save/load is `Ctrl+S` / `Ctrl+O` — `.tcpproj` files store all
the slot paths and settings so you can recreate a packing session.

## Quick start (CLI)

For build scripts, asset pipelines, or anything that doesn't want a
window. The binary is called `tcp`.

```
tcp list-presets

tcp pack --preset unreal-orm \
         --slot 0=ao.png --slot 1=rough.png --slot 2=metal.png \
         -o packed.png

tcp pack --preset unreal-orm \
         --slot 0=ao.png --slot 1=rough.png --slot 2=metal.png \
         -o packed.dds --bc bc7

tcp pack project.tcpproj -o packed.png
```

`tcp --help` for the full surface. Exit codes are `0` success, `2` bad
user input, `3` I/O error, `4` runtime error — usable in build scripts.

## Building from source

Windows, Visual Studio 2022 (or newer) with the "Desktop development
with C++" workload, run everything from a **Developer Command Prompt**
so MSVC is on the path.

```pwsh
git clone --recurse-submodules https://github.com/TheDisillusion/TextureChannelPacker.git
cd TextureChannelPacker

# Bootstrap the vcpkg submodule (only the first time)
.\external\vcpkg\bootstrap-vcpkg.bat -disableMetrics
$env:VCPKG_ROOT = "$PWD\external\vcpkg"

# Configure + build
cmake --preset windows-msvc
cmake --build --preset windows-msvc-release
```

The Release executable lands at
`build\windows-msvc\bin\Release\TextureChannelPacker.exe`, the CLI at
`build\windows-msvc\bin\Release\tcp.exe`, with all Qt and OpenImageIO
DLLs and plugins deployed next to them.

The first configure downloads and builds Qt 6, OpenImageIO, OpenColorIO,
DirectXTex and friends through vcpkg — plan on roughly an hour, mostly
unattended. Subsequent configures hit the cache and finish in seconds.

Tests:

```pwsh
ctest --preset windows-msvc-release
```

There are 45 of them, covering the image pipeline, routing, resize, I/O
round-trips, presets, project files and DDS / BC encoding.

## Layout

```
core/        libtcp_core — pure C++, no Qt. Image, PackJob, exporters.
app/         tcp_app — Qt 6 desktop application.
cli/         tcp_cli — headless tcp(.exe).
tests/       Catch2 unit tests against tcp::core.
external/    vcpkg submodule.
```

The `core` library is the only thing that touches pixels. Both the GUI
and the CLI consume it; if you want to embed the pipeline into your own
engine or tool it's a single static library link.

## Tech

C++20 / CMake 3.24 / vcpkg manifest mode. Qt 6 Widgets for the UI,
QOpenGLWidget plus GLSL 3.3 for the preview. OpenImageIO for PNG / TGA /
JPG / EXR / TIFF read and write. DirectXTex for DDS and BC compression.
nlohmann/json for the project file format. Catch2 for tests. CLI11 for
the command line.

## License

MIT. See [LICENSE](LICENSE).

The Qt 6 framework is bundled under **LGPLv3** (dynamic-linked), which
adds exactly one requirement to binary redistributions: ship
[LICENSE-THIRD-PARTY.txt](LICENSE-THIRD-PARTY.txt) alongside the
executable so users have the Qt notice. That's it — Qt source isn't
required to be vendored because Qt is dynamically linked and the user
can swap it out.

Textures you produce with this tool carry no license attachment from the
tool itself. They're yours.

## Status and future

Current release: **v0.3.0**. The pipeline is complete for typical PBR
packing workflows: pack, preview, export PNG / TGA / DDS, save and
reload sessions, automate from a script.

Plausible directions if there's interest:

- HDR / float DDS (currently 8-bit only)
- macOS support (the code is portable; the preview widget would need a
  QRhi swap because Apple deprecated OpenGL)
- Batch mode with filename pattern matching
- A drag-out from the export button straight into Unreal's content
  browser
