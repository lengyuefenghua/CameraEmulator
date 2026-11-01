# GigE Vision Camera Emulator (Phase 0 + Phase 1)

Windows single-camera GigE Vision simulator built with C++20, Qt Widgets, Winsock and standalone Asio. It implements GVCP discovery/control/register access, HTTP GenICam XML, and continuous fixed-image GVSP streaming.

## Build

Requirements: Windows 10/11 x64, Visual Studio 2022 C++ workload, CMake/Ninja, and Qt 6.8.3 MSVC 2022 x64 at `G:/QT/6.8.3/msvc2022_64`.

```powershell
pwsh -File tools/build.ps1 configure
pwsh -File tools/build.ps1 build
pwsh -File tools/build.ps1 test
pwsh -File tools/build.ps1 xml-test
```

For a lean standalone Release package, run `pwsh -File tools/build.ps1 package`; launch `dist/camera_emulator/camera_emulator.exe`. For development, `pwsh -File tools/build.ps1 run` launches `.build/windows-debug/camera_emulator.exe` with the configured Qt runtime available. The `.build` tree is for build intermediates and developer tools, not distribution.

## First use

1. In the deployed directory, create `virtual-cameras` beside `camera_emulator.exe`. Put one schema version 1 JSON file per camera in that directory; see [`virtual-cameras/example-camera.json`](virtual-cameras/example-camera.json). The bundled example intentionally uses a non-existent placeholder and is shown as “不可启动” until its `imagePath` is changed to a real local absolute BMP file or BMP directory.
2. Set `imagePath` to an **absolute** BMP file path, or to an absolute directory containing BMP files. Relative paths are rejected. A BMP file is sent continuously; a directory is scanned non-recursively, ordered by filename, and its BMP files are looped at `frameRate`.
3. Start the emulator, use **刷新列表**, select a valid profile, then click **启动所选**. Switching profiles stops the currently running camera before starting the selected one. Invalid profiles remain visible as “不可启动”; hover the entry to see the reason. **保存** always writes `virtual-cameras/<id>.json` beside the executable and immediately refreshes/selects it; only **载入** opens a file picker for manual import.
4. Connect a GigE Vision client to the same host network and discover the camera. Configure the client's stream destination IP and UDP port before AcquisitionStart. XML Preview always shows the program-generated compatible GenICam XML; third-party vendor XML files are neither loaded nor distributed.

The loader currently supports **24-bit uncompressed Windows BMP** input; it scales each source BMP to the configured image geometry. Mono8 output is produced by luminance conversion and RGB8 output by BGR-to-RGB conversion. Supported output formats are Mono8 and RGB8.

## Network and firewall

- Permit inbound UDP **3956** (GVCP discovery/control) and the configured **HTTP XML port** (default 8080).
- Permit outbound UDP on the configured **GVSP destination port**; allow the corresponding return path on the client/host firewall.
- Bind the simulated camera to an IPv4 address assigned to the intended adapter. The service does not silently fall back to a different interface or port.
- Loopback addresses are useful for local protocol tests, but some GigE Vision clients filter them; use a real adapter address for external client validation.
- For troubleshooting, verify that the chosen service ports are unused and that Windows Firewall rules match the selected network profile.

## Compatibility and acceptance

XML is checked against the pinned GenApi 1.1 XSD and a local register/feature contract. A real Aravis client and MVS manual acceptance have not been completed in environments without those clients. Do not infer full vendor-client compatibility from schema validation or loopback tests alone.

See [`docs/reference/sources.md`](docs/reference/sources.md) for pinned references and licenses, and [`docs/changes/gige-phase1-single-camera/phase0-network-report.md`](docs/changes/gige-phase1-single-camera/phase0-network-report.md) for the Phase 0 network probe.
