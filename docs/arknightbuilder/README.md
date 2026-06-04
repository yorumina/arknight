# ArknightBuilder

English | [Traditional Chinese](README_zh-tw.md)

`ArknightBuilder` is the command-line stage authoring tool for `Arknight Linux`. It creates and edits stage JSON, validates routes/spawns/enemies, and runs simple stage simulations without launching the full game.

## Build

From the repository root:

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Debug
cmake --build build --target ArknightBuilder
```

Show help:

```bash
./build/ArknightBuilder --help
```

## Data Layout

The tool expects project data under `data/`:

- `data/levels`: stage JSON and stage-specific images
- `data/enemy`: enemy definitions and animation folders
- `data/operators`: operator definitions and animation/image folders

Stage file arguments are mapped to `data/levels/` automatically.

Examples:

- `tutorial_1.json` resolves to `data/levels/tutorial_1.json`
- `Operation 1-2/stage.json` resolves to `data/levels/Operation 1-2/stage.json`
- Legacy paths such as `tools/ark_builder/levels/...` are accepted and remapped

## Commands

```bash
ArknightBuilder new <file> --name <name> --width <w> --height <h>
ArknightBuilder paint <file> <x> <y> <tile> [--rect-width <w>] [--rect-height <h>]
ArknightBuilder route-set <file> <route-id> <x,y[:wait]>...
ArknightBuilder enemy-set <file> <enemy-id> --hp <value> --speed <value>
ArknightBuilder spawn-add <file> --enemy <enemy-id> --route <route-id> --count <n> --start <sec> --interval <sec>
ArknightBuilder validate <file>
ArknightBuilder simulate <file> [--duration <sec>]
ArknightBuilder show <file>
ArknightBuilder calibrate [file]
```

## Quickstart

Validate and inspect an existing stage:

```bash
./build/ArknightBuilder validate tutorial_1.json
./build/ArknightBuilder show tutorial_1.json
./build/ArknightBuilder simulate tutorial_1.json --duration 60
./build/ArknightBuilder calibrate 'Operation 1-1/stage.json'
```

Create a small linear stage:

```bash
./build/ArknightBuilder new stage_01.json --name stage_01 --width 16 --height 10
./build/ArknightBuilder paint stage_01.json 0 4 spawn
./build/ArknightBuilder paint stage_01.json 15 4 goal
./build/ArknightBuilder paint stage_01.json 1 4 road --rect-width 14 --rect-height 1
./build/ArknightBuilder route-set stage_01.json main 0,4 1,4 2,4 3,4 4,4:1.0 5,4 6,4 7,4 8,4 9,4 10,4 11,4 12,4 13,4 14,4 15,4
./build/ArknightBuilder enemy-set stage_01.json slug --hp 100 --speed 1.2
./build/ArknightBuilder spawn-add stage_01.json --enemy slug --route main --count 8 --start 0 --interval 1.5
./build/ArknightBuilder validate stage_01.json
```

The generated file is written under `data/levels/stage_01.json`.

## Coordinate Calibration

Use `calibrate` when a stage background image should become the visual map while the JSON grid remains the invisible logic layer. The calibration window lets you choose a stage, choose a cell, and drag the four yellow cell corners directly on the map. Connected corners move together, and `Save` writes the mapped cell corners back into the stage JSON.

```bash
./build/ArknightBuilder calibrate 'Operation 1-1/stage.json'
```

## Tile Types

- `empty`: unused tile
- `road`: enemy path tile
- `ground`: ground operator deployment tile
- `highground`: highground operator deployment tile
- `unusablehighground`: blocked highground tile
- `spawn`: enemy entry tile
- `goal`: enemy exit tile

## Route Format

Route points use `x,y` coordinates. Add `:wait` after a point to make enemies pause at that point.

Example:

```bash
./build/ArknightBuilder route-set stage_01.json main 0,4 1,4 2,4:1.0 3,4
```

## Validation

`validate` checks the stage structure and reports authoring problems before the game loads the file. Run it after editing a stage by hand or after using the builder commands:

```bash
./build/ArknightBuilder validate Operation\ 1-2/stage.json
```
