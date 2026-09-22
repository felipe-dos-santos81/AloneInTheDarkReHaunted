# Makefile for Alone In The Dark: Re-Haunted — FITD engine.
# Stages: deps → configure → build → run → hda-pack / hda-unpack.
# Run `make help` for every target and its arguments.

# ── Variables ────────────────────────────────────────────────────────────────

UNAME_S := $(shell uname -s)

# The CMake project lives here; all build output stays under it.
SRC_DIR ?= TatouSource

BUILD_TYPE ?= Release
JOBS ?= $(shell sysctl -n hw.ncpu 2>/dev/null || nproc 2>/dev/null || echo 4)
CMAKE = cmake

ifeq ($(UNAME_S),Darwin)
# Native Apple Silicon build, sharing the "macos-arm64" CMake preset's tree.
# arm64 is fixed: an arch-suffixed dir would collide with build/macos-x86_64.
DEPLOY_TARGET ?= 11.3
generator ?= Ninja
BUILD_DIR ?= $(SRC_DIR)/build/macos-arm64
DARWIN_FLAGS = -DCMAKE_OSX_ARCHITECTURES="arm64" \
               -DCMAKE_OSX_DEPLOYMENT_TARGET="$(DEPLOY_TARGET)"
else
BUILD_DIR ?= $(SRC_DIR)/build/$(BUILD_TYPE)
DARWIN_FLAGS =
endif

# Per-invocation overrides, e.g. make build BUILD_TYPE=Debug generator=Ninja
CONFIGURE_FLAGS = -DCMAKE_BUILD_TYPE="$(BUILD_TYPE)" \
                  -DCMAKE_EXPORT_COMPILE_COMMANDS=ON \
                  $(DARWIN_FLAGS) \
                  $(if $(generator),-G "$(generator)")

data ?= .
src ?= Assets/backgrounds_hd
out ?= $(notdir $(src)).hda
archive ?= backgrounds_hd.hda

# Texture tools (tools/textures.py). Prefer the project venv when present.
PYTHON ?= $(if $(wildcard tools/.venv/bin/python),tools/.venv/bin/python,python3)

# Texture pipeline paths (never reuse `data`/`out`: run and hda-pack own them).
gamedata ?= data/aitd1
textures ?= data/textures
textures_ai ?= data/textures-ai
dest ?= Assets/backgrounds_hd
anims ?= $(dest)
dark ?= mirror
HD_ARCHIVE = $(SRC_DIR)/backgrounds_hd.hda
ifeq ($(UNAME_S),Darwin)
BUNDLE_RESOURCES = $(BUILD_DIR)/Fitd/Tatou.app/Contents/Resources
else
BUNDLE_RESOURCES =
endif

# First existing path across the single-config and multi-config output layouts.
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
        hda-pack hda-unpack clean distclean rebuild \
        tools-deps test-tools \
        export-textures check-textures import-textures hd-install

# ── Environment ──────────────────────────────────────────────────────────────

help: ## Print this help message
	@printf '\033[1;32mAlone In The Dark: Re-Haunted — FITD engine\033[0m\n\n'
	@printf '\033[33mUsage:\033[0m make [target] [arg=value ...]\n\n\033[33mTargets:\033[0m\n'
	@awk 'BEGIN {FS = ":.*?## "} /^[a-zA-Z0-9_.\/-]+:.*## / \
		{printf "  \033[36m%-26s\033[0m %s\n", $$1, $$2}' $(MAKEFILE_LIST)

deps: ## [STEP 0] Install build dependencies for this platform
	@$(SRC_DIR)/install_deps.sh

# ── Stage 1 · Configure (CMake) ───────────────────────────────────────────────

configure: ## [STEP 1] Generate build files in the build directory (usage: make configure [BUILD_TYPE=Debug] [generator=Ninja])
	$(CMAKE) -S "$(SRC_DIR)" -B "$(BUILD_DIR)" $(CONFIGURE_FLAGS)

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

hda-pack: build-tools ## [STEP 4] Build an .hda archive (usage: make hda-pack [src=Assets/backgrounds_hd] [out=backgrounds_hd.hda])
	@test -x "$(HDA_TOOL)" || { echo "error: build_hda_archive not found - run 'make build-tools'"; exit 1; }
	"$(HDA_TOOL)" "$(src)" "$(out)"

hda-unpack: build-tools ## [STEP 4] Extract an .hda archive (usage: make hda-unpack [archive=backgrounds_hd.hda] [out=dir])
	@test -x "$(UNPACK_TOOL)" || { echo "error: unpack_hda_archive not found - run 'make build-tools'"; exit 1; }
	"$(UNPACK_TOOL)" "$(archive)" "$(out)"

# ── Stage 5 · Textures (export originals / import upscales) ──────────────────

tools-deps: ## [STEP 5] Create tools/.venv with the texture tool dependencies
	python3 -m venv tools/.venv
	tools/.venv/bin/pip install -q -r tools/requirements-dev.txt

test-tools: ## Run the texture tool test-suite
	$(PYTHON) -m pytest tests/tools -q

export-textures: ## [STEP 5] Export original plates, screens and animation jobs (usage: make export-textures [gamedata=DIR] [textures=DIR] [anims=DIR])
	$(PYTHON) tools/textures.py export --data "$(gamedata)" --out "$(textures)" --anims "$(anims)"

check-textures: ## [STEP 5] Validate upscaled textures without writing (usage: make check-textures [textures_ai=DIR] [dest=DIR] [dark=mirror|all|none])
	$(PYTHON) tools/textures.py import --src "$(textures_ai)" --dest "$(dest)" --originals "$(textures)" --dark "$(dark)" --dry-run

import-textures: ## [STEP 5] Import upscaled textures into Assets/backgrounds_hd (usage: make import-textures [textures_ai=DIR] [dest=DIR] [dark=mirror|all|none])
	$(PYTHON) tools/textures.py import --src "$(textures_ai)" --dest "$(dest)" --originals "$(textures)" --dark "$(dark)"

hd-install: ## [STEP 5] Pack Assets/backgrounds_hd into backgrounds_hd.hda and copy it into the app bundle (run 'make build-tools' first)
	@test -x "$(HDA_TOOL)" || { echo "error: build_hda_archive not found - run 'make build-tools'"; exit 1; }
	"$(HDA_TOOL)" "$(dest)" "$(HD_ARCHIVE)"
	@if [ -n "$(BUNDLE_RESOURCES)" ] && [ -d "$(BUNDLE_RESOURCES)" ]; then \
		cp "$(HD_ARCHIVE)" "$(BUNDLE_RESOURCES)/" && echo "installed $(HD_ARCHIVE) -> $(BUNDLE_RESOURCES)/"; \
	else \
		echo "note: no built app bundle found; the next 'make build' copies $(HD_ARCHIVE) into it"; \
	fi

# ── Development ───────────────────────────────────────────────────────────────

clean: ## Remove the current build directory
	rm -rf "$(BUILD_DIR)"
	@echo "Cleanup complete."

distclean: ## Remove every build directory (all configurations)
	rm -rf "$(SRC_DIR)/build"
	@echo "Full cleanup complete."

rebuild: clean build ## Clean then build from scratch
