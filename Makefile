# MilesTag II TX - build/install automation
#
# Quick start:
#   make setup           install the Python toolchain (ufbt) + pull the Flipper SDK
#   make build           build the .fap
#   make install SD=E:   copy the .fap onto an SD card mounted at E:
#
# Run `make help` for the full target list.

PYTHON      ?= python3
SD          ?= D:
APPID       := miles_tag_ii

# On Windows, MSYS2/Git-Bash `make` runs recipes through a bash whose own
# profile scripts unconditionally re-export a fake POSIX HOME (e.g.
# /home/you) - so `export`-ing the real one from this Makefile gets
# silently clobbered before the recipe even runs (`export FOO := ...`
# above the rules does NOT survive that). Windows-native Python's
# expanduser() doesn't consult HOME at all on Windows anyway - it wants
# USERPROFILE (or HOMEDRIVE+HOMEPATH), which the same bash strips
# entirely. Without it, ufbt/scons's "~" path resolution silently fails
# and builds a broken path (a literal "~" directory created inside this
# project); GCC similarly fails to create temp files without a valid
# TMP/TEMP. Resolve the real profile path via PowerShell (queries Windows
# directly, ignoring whatever bash stripped) and inject it inline on every
# recipe invocation via $(RUN) - inline assignment on the command itself
# is what actually survives, unlike `export`.
ifeq ($(OS),Windows_NT)
WIN_USERPROFILE := $(shell powershell -NoProfile -Command "[Environment]::GetFolderPath('UserProfile')")
RUN := USERPROFILE="$(WIN_USERPROFILE)" TMP="$(WIN_USERPROFILE)\AppData\Local\Temp" TEMP="$(WIN_USERPROFILE)\AppData\Local\Temp"
FAP := $(WIN_USERPROFILE)/.ufbt/build/$(APPID).fap
else
RUN :=
FAP := $(HOME)/.ufbt/build/$(APPID).fap
endif

.PHONY: help setup build install launch test clean

help:
	@echo "MilesTag II TX - make targets"
	@echo ""
	@echo "  make setup                Install Python deps (ufbt) and pull the Flipper SDK"
	@echo "  make build                Build the .fap"
	@echo "  make install SD=<path>    Copy the .fap onto an SD card mounted at <path>"
	@echo "  make launch               Build, then launch on a Flipper connected over USB"
	@echo "  make test                 Run the host-side protocol encoder tests"
	@echo "  make clean                Remove this app's build outputs from ufbt's cache"
	@echo ""
	@echo "Note: \`ufbt build\` doesn't drop the .fap in this project - it builds into"
	@echo "~/.ufbt/build/$(APPID).fap, a per-user cache shared across every ufbt app"
	@echo "you build. \`make install\`/\`make launch\` know where to find it."
	@echo ""
	@echo "The app transmits from the Flipper's built-in IR LED - no extra hardware."
	@echo "milesTagArduino/ is a vendored copy of the protocol reference library; it"
	@echo "is deliberately excluded from the build (see sources= in application.fam)."

# ---------------------------------------------------------------- Flipper app

setup:
	$(RUN) $(PYTHON) -m pip install --upgrade -r requirements.txt
	$(RUN) $(PYTHON) -m ufbt update

build:
	$(RUN) $(PYTHON) -m ufbt build

# `make install` copies the app onto the SD card at SD=<mounted path>. On the
# Flipper the app then shows up under Apps > Infrared.
install: build
	@test -d "$(SD)/apps/Infrared" || mkdir -p "$(SD)/apps/Infrared"
	cp "$(FAP)" "$(SD)/apps/Infrared/$(APPID).fap"
	@echo "Installed to $(SD)/apps/Infrared/$(APPID).fap"

# Build + launch directly on a USB-connected Flipper via ufbt.
launch: build
	$(RUN) $(PYTHON) -m ufbt launch

# ------------------------------------------------------------------ host tests

test:
	$(RUN) $(PYTHON) tests/test_protocol.py

# ---------------------------------------------------------------- housekeeping

clean:
	$(RUN) $(PYTHON) -m ufbt clean
