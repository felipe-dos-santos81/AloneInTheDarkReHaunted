#!/bin/bash
###############################################################################
# Install build dependencies for FITD (Alone In The Dark: Re-Haunted).
#
# Usage:
#   ./install_deps.sh     # or: make deps
###############################################################################
set -e

case "$(uname -s)" in
    Linux)
        if command -v apt-get >/dev/null 2>&1; then
            sudo apt-get update && sudo apt-get install -y \
                build-essential git cmake ninja-build pkg-config \
                libx11-dev libxext-dev libxrandr-dev libxinerama-dev libxcursor-dev \
                libxi-dev libgl-dev libglu1-mesa-dev libasound2-dev libpulse-dev \
                libwayland-dev libxkbcommon-dev libpipewire-0.3-dev
        elif command -v dnf >/dev/null 2>&1; then
            sudo dnf install -y gcc-c++ git cmake ninja-build pkgconfig \
                libX11-devel libXext-devel libXrandr-devel libXinerama-devel \
                libXcursor-devel libXi-devel mesa-libGL-devel mesa-libGLU-devel \
                alsa-lib-devel pulseaudio-libs-devel wayland-devel \
                libxkbcommon-devel pipewire-devel
        elif command -v pacman >/dev/null 2>&1; then
            sudo pacman -S --needed base-devel git cmake ninja pkgconf \
                libx11 libxext libxrandr libxinerama libxcursor libxi \
                mesa glu alsa-lib libpulse wayland libxkbcommon pipewire
        else
            echo "Unsupported Linux distribution - see BUILDING.md"
            exit 1
        fi
        ;;
    Darwin)
        brew install cmake ninja pkg-config
        ;;
    *)
        echo "Unsupported platform $(uname -s) - see BUILDING.md"
        exit 1
        ;;
esac
