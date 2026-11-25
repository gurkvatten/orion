# core_module

Core library for Orion. It builds the `orion_core` library and provides core APIs such as
profiles/history/bookmarks storage, settings, web extensions runtime, and utilities that
are consumed by the application.

This directory can be used as standalone CMake project and can also be included from the top-level
application build via `add_subdirectory`.

**What gets built**
- `orion_core` (shared by default, see `ORION_CORE_BUILD_SHARED`).
- `orion_core_interface` (INTERFACE target that carries include paths and compile defs).
- Codegen output for the WebKit bridge (generated at build time).

**Where to start**
- Public headers live under `core/` (for example `core/core_api.h`).

For build and installation instructions see `BUILDING.md`.
