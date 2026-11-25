# orion-linux development

## Build setup with WebKit (native)
Install more dependencies:
```
sudo apt update
sudo apt install -y g++ make cmake ninja-build pkg-config libgtk-4-dev libadwaita-1-dev python3 python3-gi golang git ruby gperf unifdef gettext gobject-introspection gi-docgen libgtk-4-dev libadwaita-1-dev libcairo2-dev libgcrypt20-dev libtasn1-6-dev libxml2-dev libsqlite3-dev libsoup-3.0-dev libxslt1-dev libsecret-1-dev libdrm-dev libgbm-dev libwoff-dev libmanette-0.2-dev flite1-dev libenchant-2-dev libjxl-dev libhyphen-dev libavif-dev libsystemd-dev libseccomp-dev libgstreamer1.0-dev libgstreamer-plugins-base1.0-dev libgstreamer-plugins-bad1.0-dev libmanette-0.2-dev libjpeg-dev
```

Checkout submodules:
```
git submodule update --init --recursive
```

Configure and build with:
```
cmake --preset linux-webkit-release
```

To build Linux version of the application, choose number of CPU cores wisely (`taskset` `-c` parameter),
a safe choice is cca 3-4 GiB of available RAM per core, for example:
```
taskset -c 0-15 cmake --build --preset linux-webkit-release
```

## Build setup (native Linux build without WebKit)

Tested on:
- Ubuntu 24.10 with libgtk-4-dev v4.16, libadwaita-1-dev v1.6. Note that some features are not available in libadwaita v1.6 and older, some dialogs may not work correctly in this case.
- Kali Linux (in WSL) with libgtk-4-dev v4.17, libadwaita-1-dev v1.7.

On Ubuntu 22.04 LTS and Ubuntu 24.04 LTS, both libgtk and libadwaita package versions are older.

Install dependencies:
```
sudo apt update
sudo apt install -y g++ make cmake ninja-build pkg-config libgtk-4-dev libadwaita-1-dev python3 python3-gi golang libsecret-1-dev libbz2-dev libwebp-dev
```

After git clone (ie., clean checkout of git repository), run:
```
cd orion-linux
```

To configure and build Linux version of the application, run:
```
cmake --preset linux-release
taskset -c 0-15 cmake --build --preset linux-release
```

All required files will be installed in 'install' directory.

### Ubuntu: AppArmor
When attempting to debug native build, you may get failures with dbus proxy.
```
sudo sysctl -w kernel.apparmor_restrict_unprivileged_unconfined=0
sudo sysctl -w kernel.apparmor_restrict_unprivileged_userns=0
```

## Development setup (Windows OS with WSL)
Deprecated – not guaranteed to work anymore.

Tested on Windows 11 Pro 24H2 using WSL.

If WSL has not been installed yet, then run:
```
wsl --install
```
Else if WSL has been installed, then run:
```
wsl --update
wsl --shutdown
```

Set default WSL version:
```
wsl --set-default-version 2
```

### kali-linux
Deprecated – not guaranteed to work anymore.

Note that this is a rolling distribution – this may cause issues with the setup in the future and may make reproducibility difficult. This distribution contains libgtk library version 4.17 (or greater in the future) which is sufficiently up-to-date.

Install kali-linux (using WSL version 2):
```
wsl --install --distribution kali-linux
wsl --set-default kali-linux
wsl ~
```
In case of graphics issues, install vGPU drivers (may be needed to work properly) – choose one of Intel, AMD, NVIDIA:
https://learn.microsoft.com/en-us/windows/wsl/tutorials/gui-apps#install-support-for-linux-gui-apps

### Setup in WSL session
Deprecated – not guaranteed to work anymore.

Run:
```
sudo apt update
sudo apt install g++ gdb make ninja-build rsync zip cmake libgtk-4-dev libadwaita-1-dev python3 python3-gi -y
```
