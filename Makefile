# Alone In The Dark: Re-Haunted — the Tatou (FITD) engine.
# Usual flow: make deps (Linux only), then make run (game data in data/aitd1).
# `make help` lists every target and its arguments.

# ── Settings ─────────────────────────────────────────────────────────────────

# The CMake project; all build output goes under it.
SRC_DIR    ?= TatouSource
BUILD_TYPE ?= Release
JOBS       ?= $(shell sysctl -n hw.ncpu 2>/dev/null || nproc 2>/dev/null || echo 4)
CMAKE       = cmake
# The Python tools use tools/.venv (make tools-deps) when it exists.
PYTHON     ?= $(if $(wildcard tools/.venv/bin/python),tools/.venv/bin/python,python3)

ifeq ($(shell uname -s),Darwin)
# Native Apple Silicon build, in the "macos-arm64" preset's tree. arm64 is
# fixed: an arch-suffixed tree would collide with build/macos-x86_64.
DEPLOY_TARGET   ?= 11.3
generator       ?= Ninja
BUILD_DIR       ?= $(SRC_DIR)/build/macos-arm64
PLATFORM_FLAGS   = -DCMAKE_OSX_ARCHITECTURES="arm64" -DCMAKE_OSX_DEPLOYMENT_TARGET="$(DEPLOY_TARGET)"
BUNDLE_RESOURCES = $(BUILD_DIR)/Fitd/Tatou.app/Contents/Resources
else
BUILD_DIR       ?= $(SRC_DIR)/build/$(BUILD_TYPE)
endif

CONFIGURE_FLAGS = -DCMAKE_BUILD_TYPE="$(BUILD_TYPE)" -DCMAKE_EXPORT_COMPILE_COMMANDS=ON \
                  $(PLATFORM_FLAGS) $(if $(generator),-G "$(generator)")
CMAKE_BUILD     = $(CMAKE) --build "$(BUILD_DIR)" --config "$(BUILD_TYPE)" --parallel "$(JOBS)"

# ── Target arguments (make run data=DIR) ─────────────────────────────────────

# Game data and HD backgrounds (run, hd-install, hda-pack, hda-unpack).
gamedata    ?= data/aitd1
data        ?= $(gamedata)
backgrounds ?= Assets/backgrounds_hd
src         ?= $(backgrounds)
out         ?= $(notdir $(src)).hda
archive     ?= backgrounds_hd.hda

# HD character models (export-models ... models-install).
models          ?= data/models
models_ai       ?= data/models-ai
models_identity ?= data/models-identity
models_blender  ?= data/models-blender
models_hd       ?= Assets/models_hd
atlases         ?= Assets/atlases
BLENDER         ?= /Applications/Blender.app/Contents/MacOS/Blender
bodies          ?=
report          ?=

MODEL_IMPORT   = $(PYTHON) tools/models.py import --data "$(gamedata)" --models "$(models)" \
                 --src "$(models_ai)" --dest "$(models_hd)"$(if $(bodies), --bodies "$(bodies)")$(if $(report), --report "$(report)")
BLENDER_MODELS = $(PYTHON) tools/models.py blender --models "$(models)" --out "$(models_ai)" --work "$(models_blender)" \
                 --blender "$(BLENDER)"$(if $(bodies), --bodies "$(bodies)")

# ── Built files ──────────────────────────────────────────────────────────────

# The first path that exists, for single- and multi-config build layouts.
built       = $(firstword $(wildcard $(addprefix $(BUILD_DIR)/,$(1))))
BINARY      = $(call built,Fitd/Tatou Fitd/Tatou.app/Contents/MacOS/Tatou Fitd/$(BUILD_TYPE)/Tatou.exe)
HDA_TOOL    = $(call built,tools/build_hda_archive tools/$(BUILD_TYPE)/build_hda_archive.exe)
UNPACK_TOOL = $(call built,tools/unpack_hda_archive tools/$(BUILD_TYPE)/unpack_hda_archive.exe)
HD_ARCHIVE  = $(SRC_DIR)/backgrounds_hd.hda
# Where the game reads its files: the app bundle on macOS, else the data folder.
GAME_DIR    = $(or $(BUNDLE_RESOURCES),$(data))

# $(call require,FILE,NAME,HINT): stop with a hint unless FILE is executable.
require = @test -x "$(1)" || { echo "error: $(2) not found - $(3)"; exit 1; }

.PHONY: help deps tools-deps configure build build-fitd build-tools run \
        test test-engine test-tools \
        hd-install hda-pack hda-unpack \
        export-models identity-models blender-models check-models import-models models-install \
        lang-extract lang-pack \
        clean distclean rebuild

help: ## List the targets
	@printf '\033[1;32mAlone In The Dark: Re-Haunted\033[0m\nUsage: make <target> [arg=value ...]\n'
	@awk 'BEGIN {FS = ":.*?## "} \
		/^##@/ {printf "\n\033[33m%s\033[0m\n", substr($$0, 5)} \
		/^[a-zA-Z0-9_.\/-]+:.*## / {printf "  \033[36m%-16s\033[0m %s\n", $$1, $$2}' $(MAKEFILE_LIST)

##@ Setup

deps: ## Install the build dependencies for this platform
	@$(SRC_DIR)/install_deps.sh

tools-deps: ## Create tools/.venv for the Python tools
	python3 -m venv tools/.venv
	tools/.venv/bin/pip install -q -r tools/requirements-dev.txt

##@ Build and run

configure: ## Generate the CMake build [BUILD_TYPE=Debug] [generator=Ninja]
	$(CMAKE) -S "$(SRC_DIR)" -B "$(BUILD_DIR)" $(CONFIGURE_FLAGS)

build: configure ## Build the game and the archive tools
	$(CMAKE_BUILD)

build-fitd: configure ## Build the game only
	$(CMAKE_BUILD) --target Fitd

build-tools: configure ## Build the .hda archive tools only
	$(CMAKE_BUILD) --target build_hda_archive unpack_hda_archive

run: build-fitd ## Build the game and play from data/aitd1 [data=DIR]
	$(call require,$(BINARY),binary,did the build fail?)
	cd "$(data)" && "$(abspath $(BINARY))"

##@ Test

test: test-engine test-tools ## Run every test suite

test-engine: configure ## Engine unit tests (doctest)
	$(CMAKE_BUILD) --target engine_tests
	cd "$(BUILD_DIR)" && ctest -C "$(BUILD_TYPE)" --output-on-failure -R engine_tests

test-tools: ## Python tool tests (pytest)
	$(PYTHON) -m pytest tests/tools -q

##@ HD backgrounds

hd-install: build-tools ## Pack Assets/backgrounds_hd and copy it into the app bundle [backgrounds=DIR]
	$(call require,$(HDA_TOOL),build_hda_archive,run 'make build-tools')
	"$(HDA_TOOL)" "$(backgrounds)" "$(HD_ARCHIVE)"
	@if [ -n "$(BUNDLE_RESOURCES)" ] && [ -d "$(BUNDLE_RESOURCES)" ]; then \
		cp "$(HD_ARCHIVE)" "$(BUNDLE_RESOURCES)/" && echo "installed $(HD_ARCHIVE) -> $(BUNDLE_RESOURCES)/"; \
	else \
		echo "note: no built app bundle found; the next 'make build' copies $(HD_ARCHIVE) into it"; \
	fi

hda-pack: build-tools ## Pack a folder into an .hda archive [src=DIR out=FILE]
	$(call require,$(HDA_TOOL),build_hda_archive,run 'make build-tools')
	"$(HDA_TOOL)" "$(src)" "$(out)"

hda-unpack: build-tools ## Extract an .hda archive [archive=FILE out=DIR]
	$(call require,$(UNPACK_TOOL),unpack_hda_archive,run 'make build-tools')
	"$(UNPACK_TOOL)" "$(archive)" "$(out)"

##@ HD character models

export-models: ## Export the original bodies into data/models [gamedata=DIR models=DIR bodies=KEY,...]
	$(PYTHON) tools/models.py export --data "$(gamedata)" --out "$(models)"$(if $(bodies), --bodies "$(bodies)")

identity-models: ## Turn the exported originals into deliveries, the compare-mode reference [models=DIR models_identity=DIR bodies=KEY,...]
	$(PYTHON) tools/models.py identity --models "$(models)" --out "$(models_identity)" --bodies "$(bodies)"

blender-models: ## Refine the bodies in Blender into data/models-ai [models=DIR models_ai=DIR bodies=KEY,... BLENDER=PATH]
	$(BLENDER_MODELS)

check-models: ## Check the deliveries without importing [models_ai=DIR bodies=KEY,... report=DIR]
	$(MODEL_IMPORT) --dry-run

import-models: ## Check and pack the deliveries into Assets/models_hd [models_ai=DIR models_hd=DIR bodies=KEY,...]
	$(MODEL_IMPORT)

# Each build copies Assets/models_hd next to the game. A models_hd/ where the game
# reads its files (GAME_DIR) wins over that copy; this target puts other models,
# and the atlases, there.
models-install: ## Copy the models and atlases where the game reads them [models_hd=DIR atlases=DIR data=DIR]
	@test -d "$(GAME_DIR)" || { echo "error: no $(GAME_DIR) - $(if $(BUNDLE_RESOURCES),run 'make build-fitd' first,see the README's game data steps)"; exit 1; }
	@$(CMAKE) "-DDESTINATION=$(GAME_DIR)" "-DMODELS=$(abspath $(models_hd))" "-DATLASES=$(abspath $(atlases))" \
		-P $(SRC_DIR)/cmake/copy_hd_models.cmake

##@ Translation

lang-extract: ## Write the English and French text as UTF-8 files into data/lang [gamedata=DIR]
	$(PYTHON) tools/lang.py extract --data "$(gamedata)"

lang-pack: ## Check Assets/lang/pt-BR and pack it into the game [gamedata=DIR]
	$(PYTHON) tools/lang.py pack --data "$(gamedata)"

##@ Clean

clean: ## Remove this configuration's build directory
	rm -rf "$(BUILD_DIR)"

distclean: ## Remove every build directory
	rm -rf "$(SRC_DIR)/build"

rebuild: clean build ## Clean, then build
