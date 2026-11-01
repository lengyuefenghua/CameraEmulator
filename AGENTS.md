# 仓库指令

## Commands
- Configure: `call "C:\Program Files\Microsoft Visual Studio\18\Community\VC\Auxiliary\Build\vcvarsall.bat" x64` then `cmake --preset windows-debug`
- Build: `cmake --build --preset windows-debug`
- Test: `ctest --preset windows-debug --output-on-failure`
- Qt: `G:\QT\6.8.3\msvc2022_64`; CMake/Ninja are under `G:\QT\Tools\`.

## Architecture
- `src/core` is standard-library-only L0; `src/protocol` encodes GVCP/GVSP/GenICam; `src/engine` orchestrates camera state; `src/infra/win32` owns sockets and platform services; `src/app/qt` is the Widgets GUI.
- `RegisterMap` is the source of truth for feature-backed register values; XML is generated from the feature/register catalog.
- External reference sources live in ignored `.reference/`; `docs/reference/sources.md` records pinned commits and licenses.

## Workflow
- Start new work with `using-lazy-flow`; follow active `docs/changes/*/tasks.md` one checkbox at a time.
- Behavior changes require explicit Chinese logs for network I/O, failures, state transitions, and user-visible outcomes; format: ISO time, level, module, source, message.
- Comments are Chinese and only explain non-obvious constraints or platform behavior.
- Tests default to none unless `tdd-cycle` leak/consequence gate passes; no assertions on log/UI copy.
- GUI changes require a text UI sketch; user performs visual acceptance. Never use screenshots or automated UI control.
- No commit or push without explicit user permission; commit requires manual acceptance first.
