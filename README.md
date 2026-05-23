# MakuTweaker++

Native **C++ / Dear ImGui** port of [MakuTweaker](https://github.com/MarkAdderly/MakuTweaker) — a Windows system tweaker with 13 feature sections and 26 UI languages.

## Requirements

- Windows 10 1607+ (build 14393+)
- Visual Studio 2022 with **Desktop development with C++** and Windows SDK
- CMake 3.20+

Administrator rights are recommended (UAC manifest: `highestAvailable`).

## Build

### Release (recommended)

```powershell
.\scripts\build-release.ps1
```

Or with [CMake presets](https://cmake.org/cmake/help/latest/manual/cmake-presets.7.html):

```powershell
cmake --workflow --preset release
# older CMake:
cmake --preset windows-x64
cmake --build --preset release
```

Output: `build/Release/MakuTweaker++.exe` with `loc/` and `assets/` copied beside the binary.

### Debug

```powershell
cmake --preset windows-x64
cmake --build --preset debug
```

Output: `build/Debug/MakuTweaker++.exe`

### Visual Studio (Open Folder)

1. Open the repo folder in VS 2022.
2. Select configuration **x64-Debug** or **x64-Release** in the toolbar.
3. **Project → Delete Cache and Reconfigure** if CMake reports a cache/path error.

If configure fails after moving the repo:

```powershell
.\scripts\clean-cmake.ps1
```

Then in VS: **Project → Delete Cache and Reconfigure**.

### Manual (without presets)

```powershell
cmake -B build -G "Visual Studio 17 2022" -A x64
cmake --build build --config Release
```

## Project layout

```
├── CMakeLists.txt
├── app.manifest
├── loc/              # JSON localization (26 languages)
├── assets/           # Icons and images
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

## Technical stack

- **UI:** Dear ImGui (Win32 + Direct3D 11)
- **Config:** JSON in `%AppData%\MakuTweakerPlusPlus\settings.json`
- **i18n:** `loc/{lang}.json` (same format as the WPF app)
- **System:** Win32 Registry API, WMI, `CreateProcess` for `powercfg`, `dism`, PowerShell, etc.

## License

See [LICENSE](LICENSE). Copyright Mark Adderly.
