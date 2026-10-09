# Contributing to AITDR

[../AGENTS.md](../AGENTS.md) holds this fork's firm rules, project map and
testing guidelines. Read it before you change code.

## Getting started

1. Fork the repository and clone your fork:

   ```bash
   git clone https://github.com/<you>/AloneInTheDarkReHaunted.git
   ```

2. Build it: [BUILDING.md](BUILDING.md).
3. Get the game data from Steam, GOG or the CD:
   [README → Game data](../README.md#game-data).
4. Learn the code modules and data flow: [ARCHITECTURE.md](ARCHITECTURE.md).

## Workflow

1. Branch from `main` (`git checkout -b feature/my-change`).
2. Keep commits small and focused.
3. Run `make test`, then build and play to check nothing broke.
4. Open a pull request against `main`.

## Coding standards

- **C++17** for the project; the engine library (`FitdLib`) compiles as C++20.
  The code must build with MSVC (VS2022+), GCC and Clang.
- **Formatting:** `TatouSource/.editorconfig` sets 4-space indents for C, C++
  and CMake files. Let your editor apply it.
- **Naming in new code:** functions `camelCase` (`loadRemasterConfig`), types
  `PascalCase` (`RemasterConfig`, `PostProcessing`), globals `g_` prefix
  (`g_gameId`, `g_controllerState`), constants and enums as the surrounding
  code does, files `camelCase.cpp` / `camelCase.h`. Code that reimplements an
  original engine function keeps the original French name (`AffObjet`,
  `GereDec`, `LoadEtage`) so it traces back to the original.
- **Headers:** `#pragma once`, or include guards in the existing
  `#ifndef _MY_HEADER_H_` style.
- **Comments** explain *why*, not *what*. New files start with the header
  block:

  ```cpp
  ///////////////////////////////////////////////////////////////////////////////
  // Alone In The Dark Re-Haunted
  // <description>
  ///////////////////////////////////////////////////////////////////////////////
  ```

- **Errors:** the code does not use exceptions. Return error codes or `bool`,
  and check resource pointers from HQR/PAK loading for `nullptr`.

## Where help is needed

- **AITD2 / AITD3:** many Life macros and features are missing. AITD2 work
  happens in
  [a separate fork](https://github.com/spacefarergames/AloneInTheDarkJackIsBackAgain/).
- **Time Gate: Knight's Chase:** very early; most engine extensions are missing.
- **Graphics:** polygon rendering, palette and depth-mask bugs.
- **Playtesting** and bug reports.

## Reporting bugs

Include:

1. The game (AITD1, AITD2, AITD3, Jack, Time Gate).
2. Where it happens (floor, room or scene).
3. Expected and actual behaviour.
4. OS, compiler and build type (Debug/Release).
5. A screenshot or short video for visual bugs.

## License

Contributions are licensed under the **GNU General Public License v2**, like
the rest of the project.
