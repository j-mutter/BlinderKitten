# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Overview

BlinderKitten is a lighting software built on top of the JUCE framework and OrganicUI. It's a C++ application for controlling stage lighting with support for DMX, Art-Net, sACN, and MIDI protocols.

## Build System

This project uses the JUCE framework with Projucer project files. The main project file is `BlinderKitten.jucer`.

### Building the Project

**Prerequisites:**
 - Ensure all git submodules are recursively initialized and up to date: `git submodule update --init --recursive`

**macOS:**
- Open `Builds/MacOSX_CI/BlinderKitten.xcodeproj` in Xcode
- Build from Xcode or use xcodebuild command line

**Linux:**
```bash
cd Builds/LinuxMakefile
make
```

**Windows (Visual Studio):**
- Open `Builds/VisualStudio2022/BlinderKitten.sln` (or appropriate version)
- Build from Visual Studio

**Raspberry Pi:**
```bash
cd Builds/Raspberry  # or Builds/Raspberry64 for 64-bit
make
```

### Installation Dependencies (Linux)
Run the provided script to install system dependencies:
```bash
./install_linux_deps.sh
```

## Architecture

### Core Components

- **BKEngine** (`Source/BKEngine.h/cpp`) - Main engine class, extends OrganicUI's Engine
- **Brain** (`Source/Brain.h/cpp`) - Central processing unit handling lighting calculations and DMX output
- **MainComponent** - Primary UI component and application entry point

### Key Subsystems

**Lighting System:**
- **ChannelFamily/ChannelType** - Define DMX channel types (intensity, color, position, etc.)
- **FixtureType/Fixture** - Light fixture definitions and instances
- **Programmer** - Real-time lighting control interface
- **Groups/Presets** - Reusable lighting configurations

**Show Control:**
- **Cuelist/Cue** - Sequential lighting sequences
- **Effect** - Dynamic parameter automation with curves
- **Carousel** - Looping effects across multiple fixtures
- **Command** - Lighting commands with timing and selection

**Interfaces:**
- **DMX** (`Source/Common/DMX/`) - DMX512, Art-Net, sACN output
- **MIDI** (`Source/Common/MIDI/`) - MIDI control surface support
- **Serial** (`Source/Common/Serial/`) - Serial device communication, used for DMX devices connected via USB

### File Structure

- `Source/` - All C++ source code
  - `Common/` - Shared utilities (DMX, MIDI, Serial, etc.)
  - `Definitions/` - Core lighting objects and managers
  - `UI/` - User interface components
- `Builds/` - Platform-specific build configurations
- `External/` - Third-party dependencies
- `JUCE/` - Custom build of the JUCE framework, required by OrganicUI (don't edit)
- `JuceLibraryCode/` - Generated JUCE framework code (don't edit)
- `Ressources/` - Application resources and assets

## Development Notes

- The project extends OrganicUI framework - refer to OrganicUI patterns for UI development
- Uses JUCE's module system - each major component has its own Manager class
- DMX universe handling is centralized in `Brain.cpp`
- All lighting objects inherit from OrganicUI's ControllableContainer
- File format is `.olga` for projects (custom JSON-based format)

## External Dependencies

- JUCE Framework (included as submodule)
- OrganicUI (included as submodule)
- libusb (for USB-DMX interfaces)
- Various platform-specific libraries for serial/network communication