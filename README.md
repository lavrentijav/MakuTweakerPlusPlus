# MakuTweaker++

Native **C++ / Dear ImGui** port of [MakuTweaker](https://github.com/MarkAdderly/MakuTweaker) — a Windows system tweaker with 13 feature sections and 26 UI languages.

The original WPF (.NET 8) sources are kept locally under `legacy/` (gitignored) for reference during porting.

## Requirements

- Windows 10 1607+ (build 14393+)
- Visual Studio 2022 with **Desktop development with C++** and Windows SDK
- CMake 3.20+

Administrator rights are recommended (UAC manifest: `highestAvailable`).

## Build

```powershell
cmake -B build -G "Visual Studio 17 2022" -A x64
cmake --build build --config Release
```

Output: `build/Release/MakuTweaker.exe` with `loc/`, `assets/`, and `previewimg/` copied beside the binary.

### Single-file build (embedded payload)

`loc/`, `assets/`, and `previewimg/` are packed into the executable. On first run they are extracted to `%AppData%\MakuTweaker\runtime\` (cached by payload hash).

```powershell
cmake -B build-onefile -G "Visual Studio 17 2022" -A x64 -DMAKU_ONEFILE=ON
cmake --build build-onefile --config Release
```

Artifact: `build-onefile/Release/MakuTweaker.exe` only (no sidecar folders required).

## Project layout

```
├── CMakeLists.txt
├── app.manifest
├── loc/              # JSON localization (26 languages)
├── assets/           # Icons and images
├── previewimg/       # README / store screenshots per language
├── src/
│   ├── main.cpp
│   ├── app/          # Application shell
│   ├── core/         # Registry, WMI, settings, jobs
│   ├── platform/     # Win32, D3D11, tray, updates
│   └── ui/           # ImGui pages
└── legacy/           # Original WPF project (not in git)
```

## Features (ported)

| Section | Tag |
|---------|-----|
| Explorer & Desktop | exp |
| Windows Update | wu |
| System & Recovery | sys |
| Personalization | per |
| Remove UWP Apps | uwp |
| Quick Windows Setup | quick |
| Advanced | adv |
| Windows Components | compon |
| Windows Activation | act (placeholder text only) |
| Performance | perf |
| Shutdown Timer | sat |
| Process Management | pmgr |
| PC Information | pci |
| Settings / About | settings |
| Windows Information | wininfo |

## Technical stack

- **UI:** Dear ImGui (Win32 + Direct3D 11)
- **Config:** JSON in `%AppData%\MakuTweaker\settings.json`
- **i18n:** `loc/{lang}.json` (same format as the WPF app)
- **System:** Win32 Registry API, WMI, `CreateProcess` for `powercfg`, `dism`, PowerShell, etc.

## License

See [LICENSE](LICENSE). Copyright Mark Adderly.
