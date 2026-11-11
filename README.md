<div align="center">
  <img src="resources/app.png" width="132" alt="AudioRoleSync icon">

# AudioRoleSync

**Keep Windows' default communications playback and microphone devices in sync with your normal defaults.**

<p>
  <a href="https://github.com/Riyoway/AudioRoleSync/releases/latest"><img alt="Release" src="https://img.shields.io/github/v/release/Riyoway/AudioRoleSync?display_name=tag&style=flat-square"></a>
  <img alt="Windows" src="https://img.shields.io/badge/Windows-10%20%7C%2011-0078D4?style=flat-square&logo=windows11&logoColor=white">
  <img alt="Architecture" src="https://img.shields.io/badge/arch-x64-555?style=flat-square">
  <a href="LICENSE"><img alt="License" src="https://img.shields.io/github/license/Riyoway/AudioRoleSync?style=flat-square"></a>
</p>

<p>
  <a href="https://github.com/Riyoway/AudioRoleSync/releases/latest"><strong>Download</strong></a>
  ·
  <a href="docs/demo.mp4"><strong>Watch demo</strong></a>
  ·
  <a href="#how-it-works"><strong>How it works</strong></a>
</p>
</div>

---

AudioRoleSync is a tiny native Windows tray utility that keeps the **Default Communications playback and recording devices** synchronized with the normal **Default playback and recording devices**.

Windows maintains separate default roles for general audio and communications audio. Because of that, apps that use the communications role can remain attached to an old speaker or microphone after the normal Windows default is changed. AudioRoleSync keeps both sets of roles aligned automatically, with independent playback and microphone switches.

## Demo

GitHub does not render repository MP4 files inline inside README pages, so the demo opens as a normal video when clicked.

[![Watch demo](https://img.shields.io/badge/%E2%96%B6%20Watch%20the%20demo-2387F2?style=for-the-badge)](docs/demo.mp4)

## Features

- **Automatic synchronization** of communications playback and microphone defaults with their respective normal Windows defaults.
- **Independent playback and microphone switches** in the tray menu (both enabled by default).
- **System tray only** with no settings window or visible background UI.
- **Starts with Windows by default** on first launch.
- **Manual Sync now** action from the tray context menu.
- **Event-driven Core Audio monitoring** with no continuous polling loop.
- **Native Win32 C++** with no .NET, Electron, WebView, NAudio, or PowerShell runtime dependency.
- **No administrator privileges required.**
- **No network access, telemetry, background service, or updater.**
- Available as both an **installer** and a **portable build**.

## Installation

Download the latest build from [Releases](https://github.com/Riyoway/AudioRoleSync/releases/latest).

### Installer

Use the installer for the standard setup. AudioRoleSync is installed per-user under `%LOCALAPPDATA%\Programs\AudioRoleSync`, launches immediately after installation, and is configured to start when the user signs in to Windows.

### Portable

Extract the portable ZIP and run `AudioRoleSync.exe`. No installation is required.

On first launch, **Start with Windows** is enabled by default. It can be disabled from the tray menu at any time.

> [!NOTE]
> Release binaries are currently unsigned. Windows SmartScreen may display a warning until signed builds are available.

## Usage

AudioRoleSync has no main window. Right-click its system tray icon to access:

| Action | Description |
| --- | --- |
| **Automatic sync** | Enable or disable automatic synchronization for the selected device types. |
| **Sync playback** | Include playback/output devices in synchronization. Enabled by default. |
| **Sync microphone** | Include recording/input devices in synchronization. Enabled by default. |
| **Sync now** | Immediately synchronize the enabled device types, even if Automatic sync is off. |
| **Start with Windows** | Enable or disable startup at Windows sign-in. |
| **Exit** | Close AudioRoleSync. |

With automatic sync enabled, the flow is simply:

```text
Windows default playback or microphone changes
                       ↓
           AudioRoleSync detects it
                       ↓
     Default Communications device
     for that type is set to match
```

## How it works

AudioRoleSync listens for Windows Core Audio default-device changes through `IMMNotificationClient::OnDefaultDeviceChanged`.

When the default render (playback) or capture (microphone) endpoint for the normal multimedia role changes, the application compares it with the corresponding communications endpoint. If they differ, it updates only that data flow's communications role to use the same endpoint. Playback and microphone synchronization can be switched on or off separately.

The application changes **default device assignments only**; it does not record microphone audio, change microphone levels, or turn a microphone on or off. Settings are saved per user under `HKCU\Software\AudioRoleSync`.

Windows does not provide a documented public API for changing the default endpoint role. AudioRoleSync therefore uses the commonly used undocumented `IPolicyConfig::SetDefaultEndpoint` interface for this single operation. This is isolated to a small part of the codebase, but compatibility with future Windows versions cannot be guaranteed.

## Runtime footprint

AudioRoleSync is intentionally small and event-driven. At runtime it keeps only the components required for Core Audio notifications, the hidden message window used by the tray icon, and the tray icon itself.

No timer runs continuously. A short debounce timer is created only after Windows reports a relevant audio endpoint change.

Actual memory usage varies by Windows version and loaded system components.

## Requirements

### Running

- Windows 10 or Windows 11
- x64 system

### Building

- Visual Studio 2022 or newer with the **Desktop development with C++** workload
- CMake 3.24 or newer
- Inno Setup 6 only when building the installer

## Building from source

Build the application:

```powershell
./scripts/build.ps1
```

Output:

```text
dist/win-x64/AudioRoleSync.exe
```

Build both release packages:

```powershell
./scripts/build-release.ps1
```

Outputs for v0.0.1:

```text
dist/AudioRoleSync-v0.0.1-win-x64-portable.zip
dist/AudioRoleSync-v0.0.1-win-x64-setup.exe
```

The Release configuration statically links the MSVC runtime and enables link-time optimization, dead-code elimination, and identical COMDAT folding.

## Project structure

```text
.github/workflows/   GitHub Actions build and release workflows
docs/                Demo video and documentation assets
installer/           Inno Setup definition
resources/           Application icon and Windows resources
scripts/             Local build and packaging scripts
src/                 Native application source
```

## Limitations

- The original v0.0.1 release synchronized playback only. Microphone support is available after updating to a build containing this change.
- The default-endpoint write operation relies on the undocumented Windows `IPolicyConfig` interface.
- Only x64 builds are currently provided.

## Privacy

AudioRoleSync does not collect or transmit data. It does not make network requests and contains no telemetry.

## Contributing

Issues and pull requests are welcome. For behavioral changes, please keep the project focused on a small native tray utility and avoid introducing runtime frameworks or unnecessary background work.

## License

Licensed under the [MIT License](LICENSE).
