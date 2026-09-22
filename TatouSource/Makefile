# Makefile for Alone In The Dark: Re-Haunted — FITD engine
# Targets are numbered by pipeline stage:
#   env → 1 configure → 2 build → 3 run → 4 assets → dev
SERVICE = Alone In The Dark: Re-Haunted

# Variables
UNAME_S := $(shell uname -s)

BUILD_TYPE ?= Release

ifeq ($(UNAME_S),Darwin)
# Apple Silicon: default to arm64, Ninja, and a platform-named build dir that
# matches the "macos-arm64" CMake preset so both entry points share one tree.
ARCH ?= arm64
DEPLOY_TARGET ?= 11.3
generator ?= Ninja
BUILD_DIR ?= build/macos-$(ARCH)
DARWIN_FLAGS = -DCMAKE_OSX_ARCHITECTURES="$(ARCH)" -DCMAKE_OSX_DEPLOYMENT_TARGET="$(DEPLOY_TARGET)"
else
BUILD_DIR ?= build/$(BUILD_TYPE)
DARWIN_FLAGS =
endif

JOBS ?= $(shell sysctl -n hw.ncpu 2>/dev/null || nproc 2>/dev/null || echo 4)
CMAKE = cmake

# Optional per-invocation args
CONFIGURE_FLAGS = -DCMAKE_BUILD_TYPE="$(BUILD_TYPE)" -DCMAKE_EXPORT_COMPILE_COMMANDS=ON $(DARWIN_FLAGS) $(if $(generator),-G "$(generator)")
data ?= .
src ?= ../Assets/backgrounds_hd
out ?= $(notdir $(src)).hda
archive ?= backgrounds_hd.hda

# Resolve outputs across single-config and multi-config generators.
BINARY = $(firstword $(wildcard \
	$(BUILD_DIR)/Fitd/Tatou \
	$(BUILD_DIR)/Fitd/Tatou.app/Contents/MacOS/Tatou \
	$(BUILD_DIR)/Fitd/$(BUILD_TYPE)/Tatou.exe))
HDA_TOOL = $(firstword $(wildcard \
	$(BUILD_DIR)/tools/build_hda_archive \
	$(BUILD_DIR)/tools/$(BUILD_TYPE)/build_hda_archive.exe))
UNPACK_TOOL = $(firstword $(wildcard \
	$(BUILD_DIR)/tools/unpack_hda_archive \
	$(BUILD_DIR)/tools/$(BUILD_TYPE)/unpack_hda_archive.exe))

.PHONY: help deps configure build build-fitd build-tools run \
        hda-pack hda-unpack \
        clean distclean rebuild

# ── Environment ──────────────────────────────────────────────────────────────

help: ## Print this help message
	@printf '\033[01;32m${SERVICE} — FITD engine\033[00;37m\n\n'
	@printf "\033[33mUsage:\033[0m\n  make [target] [arg=\"val\"...]\n\n\033[33mTargets:\033[0m\n"
	@grep -E '^[-a-zA-Z0-9_\.\/]+:.*?## .*$$' $(MAKEFILE_LIST) | \
		awk 'BEGIN {FS = ":.*?## "}; \
		{printf "  \033[36m%-26s\033[0m %s\n", $$1, $$2}'

deps: ## [STEP 0] Install system build dependencies (apt/dnf/pacman/brew)
	@case "$$(uname -s)" in \
		Linux) \
			if command -v apt-get >/dev/null 2>&1; then \
				sudo apt-get update && sudo apt-get install -y \
					build-essential git cmake ninja-build pkg-config \
					libx11-dev libxext-dev libxrandr-dev libxinerama-dev libxcursor-dev \
					libxi-dev libgl-dev libglu1-mesa-dev libasound2-dev libpulse-dev \
					libwayland-dev libxkbcommon-dev libpipewire-0.3-dev; \
			elif command -v dnf >/dev/null 2>&1; then \
				sudo dnf install -y gcc-c++ git cmake ninja-build pkgconfig \
					libX11-devel libXext-devel libXrandr-devel libXinerama-devel \
					libXcursor-devel libXi-devel mesa-libGL-devel mesa-libGLU-devel \
					alsa-lib-devel pulseaudio-libs-devel wayland-devel \
					libxkbcommon-devel pipewire-devel; \
			elif command -v pacman >/dev/null 2>&1; then \
				sudo pacman -S --needed base-devel git cmake ninja pkgconf \
					libx11 libxext libxrandr libxinerama libxcursor libxi \
					mesa glu alsa-lib libpulse wayland libxkbcommon pipewire; \
			else \
				echo "Unsupported Linux distribution - see BUILDING.md"; exit 1; \
			fi ;; \
		Darwin) \
			brew install cmake ninja pkg-config ;; \
		*) \
			echo "Unsupported platform $$(uname -s) - see BUILDING.md"; exit 1 ;; \
	esac

# ── Stage 1 · Configure (CMake) ───────────────────────────────────────────────

configure: ## [STEP 1] Generate build files in the build directory (usage: make configure [BUILD_TYPE=Debug] [generator=Ninja])
	$(CMAKE) -S . -B "$(BUILD_DIR)" $(CONFIGURE_FLAGS)

# ── Stage 2 · Build (game + tools) ────────────────────────────────────────────

build: configure ## [STEP 2] Build all targets (game executable + archive tools)
	$(CMAKE) --build "$(BUILD_DIR)" --config "$(BUILD_TYPE)" --parallel "$(JOBS)"

build-fitd: configure ## [STEP 2] Build only the game executable (Tatou)
	$(CMAKE) --build "$(BUILD_DIR)" --target Fitd --config "$(BUILD_TYPE)" --parallel "$(JOBS)"

build-tools: configure ## [STEP 2] Build only the HDA archive tools
	$(CMAKE) --build "$(BUILD_DIR)" --target build_hda_archive unpack_hda_archive \
		--config "$(BUILD_TYPE)" --parallel "$(JOBS)"

# ── Stage 3 · Run ─────────────────────────────────────────────────────────────

run: build ## [STEP 3] Launch the game from a game-data directory (usage: make run [data=/path/to/game/data])
	@test -x "$(BINARY)" || { echo "error: binary not found - did the build fail?"; exit 1; }
	cd "$(data)" && "$(abspath $(BINARY))"

# ── Stage 4 · Assets (HDA archives) ───────────────────────────────────────────

hda-pack: build-tools ## [STEP 4] Build an .hda archive (usage: make hda-pack [src=../Assets/backgrounds_hd] [out=backgrounds_hd.hda])
	@test -x "$(HDA_TOOL)" || { echo "error: build_hda_archive not found - run 'make build-tools'"; exit 1; }
	"$(HDA_TOOL)" "$(src)" "$(out)"

hda-unpack: build-tools ## [STEP 4] Extract an .hda archive (usage: make hda-unpack [archive=backgrounds_hd.hda] [out=dir])
	@test -x "$(UNPACK_TOOL)" || { echo "error: unpack_hda_archive not found - run 'make build-tools'"; exit 1; }
	"$(UNPACK_TOOL)" "$(archive)" $(out)

# ── Development ───────────────────────────────────────────────────────────────

clean: ## Remove the current build directory
	rm -rf "$(BUILD_DIR)"
	@echo "Cleanup complete."

distclean: ## Remove every build directory (all configurations)
	rm -rf build
	@echo "Full cleanup complete."

rebuild: clean build ## Clean then build from scratch
