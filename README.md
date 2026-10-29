# AudioRoleSync

<p align="center">
  <img src="resources/app.png" width="128" alt="AudioRoleSync icon">
</p>

AudioRoleSync is a lightweight native Windows tray utility that keeps the **Default Communications playback device** synchronized with the normal **Default playback device**.

Windows maintains separate default roles for general audio and communications audio. As a result, voice chat and other communications apps can remain attached to an old output device after the normal Windows output is changed. AudioRoleSync keeps those roles aligned automatically.

## Features

- Automatically synchronizes the default communications playback device with the normal default playback device.
- Runs entirely from the Windows system tray with no settings window.
- Starts with Windows by default on first launch.
- Manual **Sync now** action from the tray menu.
- Event-driven Core Audio monitoring with no continuous polling loop.
- Native Win32 C++ with no .NET, Electron, WebView, NAudio, or PowerShell runtime dependency.
- No administrator privileges required.
- No network access, telemetry, background service, or updater.
- Available as both an installer and a portable build.

## Installation

### Installer

Use the installer for the standard setup. AudioRoleSync is installed per-user under `%LOCALAPPDATA%\Programs\AudioRoleSync`, launches immediately after installation, and is configured to start when the user signs in to Windows.

### Portable

Extract the portable ZIP and run `AudioRoleSync.exe`. No installation is required.

On first launch, **Start with Windows** is enabled by default. It can be disabled from the tray menu at any time.

> Release binaries are currently unsigned. Windows SmartScreen may display a warning until signed builds are available.

## Usage

AudioRoleSync has no main window. Right-click its system tray icon to access:

- **Automatic sync** - enable or disable automatic synchronization.
- **Sync now** - immediately synchronize the playback roles.
- **Start with Windows** - enable or disable startup at Windows sign-in.
- **Exit** - close AudioRoleSync.

When automatic sync is enabled, changing the normal Windows playback device automatically updates the communications playback device to the same endpoint.

## How it works

AudioRoleSync listens for Windows Core Audio default-device changes through `IMMNotificationClient::OnDefaultDeviceChanged`.

When the default render endpoint for the normal playback role changes, the application compares it with the communications render endpoint. If they differ, it updates the communications role to use the same endpoint.

Windows does not provide a documented public API for changing the default endpoint role. AudioRoleSync therefore uses the commonly used undocumented `IPolicyConfig::SetDefaultEndpoint` interface for this single operation. This is isolated to a small part of the codebase, but compatibility with future Windows versions cannot be guaranteed.

## Runtime footprint

The application is intentionally small and event-driven. At runtime it keeps only the components required for Core Audio notifications, the hidden message window used by the tray icon, and the tray icon itself.

No timer runs continuously. A short debounce timer is created only after Windows reports a relevant audio endpoint change.

Actual memory usage varies by Windows version and loaded system components.

## Requirements

### Running

- Windows 10 or Windows 11
- x64 system

### Building

- Visual Studio 2022 or newer with the Desktop development with C++ workload
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
installer/           Inno Setup definition
resources/           Application icon and Windows resources
scripts/             Local build and packaging scripts
src/                 Native application source
```

## Limitations

- v0.0.1 synchronizes playback devices only. Microphone and recording defaults are not modified.
- The default-endpoint write operation relies on the undocumented Windows `IPolicyConfig` interface.
- Only x64 builds are currently provided.

## Privacy

AudioRoleSync does not collect or transmit data. It does not make network requests and contains no telemetry.

## Contributing

Issues and pull requests are welcome. For behavioral changes, please keep the project focused on a small native tray utility and avoid introducing runtime frameworks or unnecessary background work.

## License

Licensed under the [MIT License](LICENSE).
