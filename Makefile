# Alone In The Dark: Re-Haunted — the Tatou (FITD) engine.
# Usual flow: make deps → make build-fitd → make run data=DIR.
# `make help` lists every target with its arguments.

# ── Settings ─────────────────────────────────────────────────────────────────

# The CMake project; all build output stays under it.
SRC_DIR    ?= TatouSource
BUILD_TYPE ?= Release
JOBS       ?= $(shell sysctl -n hw.ncpu 2>/dev/null || nproc 2>/dev/null || echo 4)
CMAKE       = cmake
# Texture tools prefer the project venv (make tools-deps) when it exists.
PYTHON     ?= $(if $(wildcard tools/.venv/bin/python),tools/.venv/bin/python,python3)

ifeq ($(shell uname -s),Darwin)
# Native Apple Silicon build in the "macos-arm64" preset's tree. arm64 is
# fixed: an arch-suffixed directory would collide with build/macos-x86_64.
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

# ── Target arguments (make run data=data/aitd1) ──────────────────────────────

data    ?= .
src     ?= Assets/backgrounds_hd
out     ?= $(notdir $(src)).hda
archive ?= backgrounds_hd.hda

# The texture pipeline never reuses `data`/`out`: run and hda-pack own them.
gamedata    ?= data/aitd1
textures    ?= data/textures
textures_ai ?= data/textures-ai
dest        ?= Assets/backgrounds_hd
anims       ?= $(dest)
dark        ?= mirror

TEXTURE_IMPORT = $(PYTHON) tools/textures.py import --src "$(textures_ai)" --dest "$(dest)" \
                 --originals "$(textures)" --dark "$(dark)"

# ── Built files ──────────────────────────────────────────────────────────────

# The first existing path across single- and multi-config build layouts.
built       = $(firstword $(wildcard $(addprefix $(BUILD_DIR)/,$(1))))
BINARY      = $(call built,Fitd/Tatou Fitd/Tatou.app/Contents/MacOS/Tatou Fitd/$(BUILD_TYPE)/Tatou.exe)
HDA_TOOL    = $(call built,tools/build_hda_archive tools/$(BUILD_TYPE)/build_hda_archive.exe)
UNPACK_TOOL = $(call built,tools/unpack_hda_archive tools/$(BUILD_TYPE)/unpack_hda_archive.exe)
HD_ARCHIVE  = $(SRC_DIR)/backgrounds_hd.hda

# $(call require,FILE,NAME,HINT): stop with a hint unless FILE is executable.
require = @test -x "$(1)" || { echo "error: $(2) not found - $(3)"; exit 1; }

.PHONY: help deps tools-deps configure build build-fitd build-tools run \
        test test-engine test-tools \
        export-textures check-textures import-textures hd-install hda-pack hda-unpack \
        clean distclean rebuild

help: ## List the targets
	@printf '\033[1;32mAlone In The Dark: Re-Haunted\033[0m\nUsage: make <target> [arg=value ...]\n'
	@awk 'BEGIN {FS = ":.*?## "} \
		/^##@/ {printf "\n\033[33m%s\033[0m\n", substr($$0, 5)} \
		/^[a-zA-Z0-9_.\/-]+:.*## / {printf "  \033[36m%-16s\033[0m %s\n", $$1, $$2}' $(MAKEFILE_LIST)

##@ Setup

deps: ## Install the build dependencies for this platform
	@$(SRC_DIR)/install_deps.sh

tools-deps: ## Create tools/.venv for the texture tools
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

run: build ## Play from a folder holding the game data [data=DIR]
	$(call require,$(BINARY),binary,did the build fail?)
	cd "$(data)" && "$(abspath $(BINARY))"

##@ Test

test: test-engine test-tools ## Run every test suite

test-engine: configure ## Engine unit tests (doctest: engine-free mouse modules, cursor rule)
	$(CMAKE_BUILD) --target engine_tests
	cd "$(BUILD_DIR)" && ctest -C "$(BUILD_TYPE)" --output-on-failure -R engine_tests

test-tools: ## Texture tool tests (pytest)
	$(PYTHON) -m pytest tests/tools -q

##@ HD backgrounds

export-textures: ## Export original plates, screens and animations [gamedata=DIR textures=DIR anims=DIR]
	$(PYTHON) tools/textures.py export --data "$(gamedata)" --out "$(textures)" --anims "$(anims)"

check-textures: ## Validate upscaled textures, writing nothing [textures_ai=DIR dest=DIR dark=mirror|all|none]
	$(TEXTURE_IMPORT) --dry-run

import-textures: ## Import upscaled textures into Assets/backgrounds_hd [textures_ai=DIR dest=DIR dark=mirror|all|none]
	$(TEXTURE_IMPORT)

hd-install: ## Pack Assets/backgrounds_hd and copy it into the app bundle (after build-tools)
	$(call require,$(HDA_TOOL),build_hda_archive,run 'make build-tools')
	"$(HDA_TOOL)" "$(dest)" "$(HD_ARCHIVE)"
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

##@ Clean

clean: ## Remove this configuration's build directory
	rm -rf "$(BUILD_DIR)"

distclean: ## Remove every build directory
	rm -rf "$(SRC_DIR)/build"

rebuild: clean build ## Clean, then build
