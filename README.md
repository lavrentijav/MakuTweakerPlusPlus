# MakuTweaker++

Native **C++ / Dear ImGui** port of [MakuTweaker](https://github.com/MarkAdderly/MakuTweaker) — a Windows system tweaker with 15 feature sections and 26 UI languages.

The original WPF (.NET 8) sources are kept locally under `legacy/` (gitignored) for reference during porting.

## Requirements

- Windows 10 1607+ (build 14393+)
- Visual Studio 2022 or 2026 with **Desktop development with C++** and Windows SDK
- CMake 3.20+

Administrator rights are recommended (UAC manifest: `highestAvailable`).

## Build

```powershell
cmake -B build -G "Visual Studio 17 2022" -A x64
```

```powershell
cmake --build build --config Release
```

Output: `build/Release/MakuTweaker++.exe` with `loc/`, `assets/`, and `previewimg/` copied beside the binary.

### Single-file build (embedded payload)

`loc/`, `assets/`, and `previewimg/` are packed into the executable. On first run they are extracted to `%AppData%\MakuTweaker\runtime\` (cached by payload hash).

```powershell
cmake -B build-onefile -G "Visual Studio 17 2022" -A x64 -DMAKU_ONEFILE=ON
```

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
│   ├── cli/          # Headless command-line interface
│   ├── core/         # Tweak registry, registry/WMI access, settings, jobs
│   ├── platform/     # Win32, D3D11, tray, updates
│   └── ui/           # ImGui pages
└── legacy/           # Original WPF project (not in git)
```

## The tweak registry

Every system tweak lives in one table, `src/core/TweakRegistry.cpp`. Each entry
carries its id, page, localization key, Windows build range, whether it needs
elevation, and its probe/apply functions.

Both surfaces read that table: `ui/TweakPage.cpp` renders a page's entries as
Fluent toggle rows and action buttons, and `cli/Cli.cpp` exposes the same
entries as `tweak get/set/run`. Adding a tweak in one place makes it appear in
the GUI *and* the CLI with no extra wiring.

Probing runs on a worker thread — several probes shell out to `bcdedit`, which
would otherwise stall a frame.

## Features (ported)

| Section | Page tag | Notes |
|---------|----------|-------|
| Explorer & Desktop | `exp` | 11 toggles, drive-letter hiding, delegate-folder fix |
| Windows Update | `wu` | Block/restore, driver exclusion, reserved storage, pause, cache reset, feature-update pin (1607–26H2) |
| System & Recovery | `sys` | Telemetry, UAC, hibernation, SmartScreen, Bing, sticky keys, core isolation, chkdsk, BitLocker + 7 cleanup actions |
| Personalization | `per` | Theme, transparency, verbose boot, End Task, classic context menu, menu delay, compact title bars, boot logo/animation, taskbar widgets, search ads, clipboard history, 8 accent presets |
| Remove UWP Apps | `uwp` | Full installed-package list, search, curated bloatware selection |
| Quick Windows Setup | `quick` | 26-entry checklist applied in one batch |
| Advanced | `adv` | VBS, TTL, indexing, legacy boot menu, advanced boot options, pagefile, Edge removal, site blocking |
| Windows Components | `compon` | DirectPlay, .NET 3.5, Photo Viewer, gpedit, Hyper-V, WinSxS, PowerShell policy, Game DVR |
| Windows Activation | `act` | See *Not ported* below |
| Performance | `perf` | Sleep/display timeouts, Ultimate Performance plan, max processor state |
| Shutdown Timer | `sat` | |
| Process Management | `pmgr` | Processes and services, grouping, exclusions, MakuYan |
| PC Information | `pci` | Hardware inventory + CPU benchmark |
| Windows Information | `wininfo` | |
| Monitoring | `mon` | Live CPU/RAM/disk graphs, optional background service |
| Settings / About | `settings` | Language, theme, Win+R aliases, presets, analytics |

### Not ported

- **Windows activation** — the original's activation page is not open source, so
  the page shows an explanatory message instead. This is a deliberate omission,
  not a gap.

## Command-line interface

`MakuTweaker++.exe` opens the GUI with no arguments and runs headless with a
subcommand, attaching to the calling console.

Because that binary is linked `/SUBSYSTEM:WINDOWS` (so the GUI never flashes a
console), shells do not wait for it. The build therefore also produces a tiny
console front end, **`MakuTweaker++.com`**, which starts the real executable,
waits, and returns its exit code. Windows prefers `.com` over `.exe` for a bare
name, so typing `MakuTweaker++ <command>` picks it up automatically and piping,
redirection and `%ERRORLEVEL%` all behave normally. Keep the two files next to
each other.

```powershell
MakuTweaker++.exe help
```

```powershell
MakuTweaker++.exe tweak list --page per
```

```powershell
MakuTweaker++.exe tweak set sys.telemetry-off on
```

```powershell
MakuTweaker++.exe preset apply quick
```

```powershell
MakuTweaker++.exe bench --multi --json
```

Command groups: `tweak`, `preset`, `wu`, `cpu`, `bench`, `sysinfo`, `proc`,
`svc`, `uwp`, `shutdown`, `hosts`, `apps`, `analytics`, `settings`, `version`.
Most support `--json` for scripting. Exit codes are `0` success, `1` failure,
`2` usage error. Admin-only commands fail with a clear message rather than
silently doing nothing.

The legacy GUI switches (`/u`, `/p`, `/s`, `/mgr`, `/pc`, `/win`, `/mon`) still
open the window on that page.

## Analytics

Nothing is transmitted until the user answers the first-run question. The
prompt states exactly what would be shared:

- the processor model and its built-in benchmark score,
- which tabs of the program are opened most often.

Declining leaves the channel closed permanently. Tab counts are still recorded
locally so *Settings → About* can show what would have been shared, and after a
benchmark run the PC Information page offers a one-shot **“Share this result
with Mark Adderly”** button for people who want to contribute a score without
enabling analytics.

`MakuTweaker++.exe analytics status|on|off|reset` controls the same setting.

## Technical stack

- **UI:** Dear ImGui (Win32 + Direct3D 11), Fluent/WinUI palette and metrics
- **Config:** JSON in `%AppData%\MakuTweakerPlusPlus\settings.json`
- **i18n:** `loc/{lang}.json` (same format as the WPF app)
- **System:** Win32 Registry API, WMI, PowrProf for power schemes, `CreateProcess` for `dism`, `bcdedit`, PowerShell

### Idle cost

The frame loop is paced rather than free-running: 60 Hz while interacting or
showing live graphs, 30 Hz when focused and quiet, 6 Hz in the background, and
no rendering at all while minimized. Waiting happens on the message queue
(`MsgWaitForMultipleObjectsEx`), so input still wakes the UI immediately.
Power-scheme state is read through PowrProf instead of parsing `powercfg`
output, which removes several process spawns per page visit and works
regardless of Windows display language.

## License

See [LICENSE](LICENSE). Copyright Mark Adderly.
