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

# application.fam is the single source of truth for the app's identity - ufbt
# names the build output after `appid`. Reading it here (rather than repeating
# the name) is what lets this Makefile and .github/workflows/release.yml be
# copied to another app unchanged.
APPID       := $(shell sed -n 's/^[[:space:]]*appid="\([^"]*\)".*/\1/p' application.fam | head -1)
APPNAME     := $(shell sed -n 's/^[[:space:]]*name="\([^"]*\)".*/\1/p' application.fam | head -1)
APPVERSION  := $(shell sed -n 's/^[[:space:]]*fap_version="\([^"]*\)".*/\1/p' application.fam | head -1)

ifeq ($(strip $(APPID)),)
$(error Could not read appid from application.fam)
endif

# Are we on Windows? Do NOT rely on OS=Windows_NT alone: the same MSYS2 profile
# described below also strips OS from the environment make inherits, so under
# Git-Bash `$(OS)` is empty and the Windows branch would be skipped - producing
# exactly the broken "~" path this block exists to prevent. uname is the
# reliable signal, and on real POSIX it correctly reports Linux/Darwin (which is
# what CI needs).
UNAME_S := $(shell uname -s 2>/dev/null)
ifeq ($(OS),Windows_NT)
WINDOWS := 1
else ifneq (,$(filter MINGW% MSYS% CYGWIN%,$(UNAME_S)))
WINDOWS := 1
endif

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
ifeq ($(WINDOWS),1)
WIN_USERPROFILE := $(shell powershell -NoProfile -Command "[Environment]::GetFolderPath('UserProfile')")
RUN := USERPROFILE="$(WIN_USERPROFILE)" TMP="$(WIN_USERPROFILE)\AppData\Local\Temp" TEMP="$(WIN_USERPROFILE)\AppData\Local\Temp"
FAP := $(WIN_USERPROFILE)/.ufbt/build/$(APPID).fap
else
RUN :=
FAP := $(HOME)/.ufbt/build/$(APPID).fap
endif

.PHONY: help setup build install launch test clean print-appid print-name print-version print-fap

help:
	@echo "$(APPNAME) - make targets"
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

# ------------------------------------------------------------ tooling queries
# Used by .github/workflows/release.yml so CI never has to know the app's name.
# `make -s print-fap` prints the path ufbt built, nothing else.

print-appid:
	@echo "$(APPID)"

print-name:
	@echo "$(APPNAME)"

print-version:
	@echo "$(APPVERSION)"

print-fap:
	@echo "$(FAP)"
