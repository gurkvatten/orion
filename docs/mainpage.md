@mainpage Orion GTK — Developer Documentation

Orion GTK is a Linux desktop application that provides a GTK4/libadwaita user interface for the Orion web browser.

This repository is split into two main parts:
- **Application (UI)**: GTK/libadwaita windows, dialogs, actions, widgets, and resources.
- **Core module**: the `orion_core` library (storage, models, repositories, webextensions support, and supporting utilities).

If you are new to the codebase, the quickest entry points are:
- `app/src/main.cpp` (process entry)
- `app/src/app.cpp` (application lifecycle / activation)
- `app/src/window/` (main window and dialogs)

@section build Build & Run

This project uses **CMake presets**. The exact preset set depends on whether you build with WebKit and whether you target Flatpak.

@subsection build_native Native Linux build (without WebKit)

This is the lightest dependency setup and is useful for UI development and faster iteration.

Install dependencies (Debian/Ubuntu example):
```
sudo apt update
sudo apt install -y g++ make cmake ninja-build pkg-config \
	libgtk-4-dev libadwaita-1-dev \
	python3 python3-gi golang libsecret-1-dev
```

Configure & build:
```
cmake --preset linux-release
cmake --build --preset linux-release
```

Artifacts are installed into the local `install/` directory.

@subsection build_native_webkit Native Linux build (with WebKit)

This build includes the embedded WebKit runtime. It requires additional dependencies and Git submodules.

Checkout submodules:
```
git submodule update --init --recursive
```

Configure & build:
```
cmake --preset linux-webkit-release
cmake --build --preset linux-webkit-release
```

@subsection build_flatpak Flatpak build (with WebKit)

Install Flatpak tooling:
```
sudo apt update
sudo apt install -y flatpak gnome-software-plugin-flatpak golang-go
```

Checkout submodules:
```
git submodule update --init --recursive
```

Configure & build:
```
cmake --preset flatpak-webkit-release
cmake --build --preset flatpak-webkit-release
```

Install the Flatpak bundle and run:
```
flatpak install --user install/flatpak-webkit-release/oriongtk.flatpak
flatpak run com.kagi.Orion
```

@section architecture Architecture (High Level)

- **UI layer** lives under `app/src/`.
	- Windows and dialogs are in `app/src/window/`.
	- UI models for lists/column-views are in `app/src/data_item/`.
	- Keyboard accelerators and actions are in `app/src/accel/` and `app/src/action/`.
	- Static UI data (GResources, CSS, HTML templates, icons) is under `app/data/`.

- **Core layer** lives under `app/src/core_module/`.
	- The central API façade is `app/src/core_module/core/core_api.h` (`orion::CoreApi`).
	- Persistence is implemented in the core DB/repository layer (profiles, bookmarks, history, etc.).

The application integrates the core module with the GTK main loop and drives core-side work using a custom run loop.

@section layout Repository Layout

- `app/` — application sources, resources, and build definitions
- `docs/` — Doxygen output and documentation assets
- `scripts/` — helper scripts for building/installing/running
- `cmake/` — packaging templates and CMake helpers

@section devnotes UI Development Notes

UI dialogs are commonly authored in **Blueprint** (`.blp`) and compiled into GResources.
For a step-by-step example of adding a new dialog, see @ref md_NewDialog.

If you use GNOME Workbench for UI prototyping, see @ref md_Workbench.

@section links Further Reading

@section external Local GTK4 / libadwaita API (tagfiles)

This repo can generate *local* Doxygen HTML + tagfiles for **GTK4** and **libadwaita** from your system headers.
The main Orion documentation then uses those tagfiles so names like `GtkWidget*` / `AdwDialog*` become clickable.

Generate external docs + tagfiles:
```
./scripts/doxygen-generate-gtk-adw.sh
```

Generate Orion docs (now with external cross-links enabled):
```
doxygen Doxyfile
```

Preview in a browser (two valid options):

1) Serve the whole output directory (recommended):
```
cd docs/output
python3 -m http.server 8000
```
Open: `http://localhost:8000/html/index.html` (and externals are under `/externals/...`).

2) Serve just the HTML folder:
```
cd docs/output/html
python3 -m http.server 8000
```
In that case re-run `./scripts/doxygen-generate-gtk-adw.sh` once, which creates `docs/output/html/externals -> ../externals` so `/externals/...` works.

- Project overview: @ref md_README
- Development notes and build variants: @ref md_Development
- Creating dialogs: @ref md_NewDialog
- Workbench setup: @ref md_Workbench