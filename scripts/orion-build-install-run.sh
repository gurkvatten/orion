#!/bin/bash

cmake --preset flatpak-webkit-release && \
    cmake --build --preset flatpak-webkit-release && \
    flatpak install --user install/flatpak-webkit-release/oriongtk.flatpak -y && \
    flatpak run com.kagi.Orion