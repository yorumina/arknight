# ArknightBuilder Manual

English | [Traditional Chinese](README_zh-tw.md)

`ArknightBuilder` is the stage authoring CLI for `Arknight Linux`. It edits the JSON files used by the game runtime and provides a small calibration UI for aligning stage artwork with the logical tile grid.

## What It Does

- Create blank stage JSON files.
- Paint one tile or rectangular tile regions.
- Define enemy routes with wait points and sprite direction markers.
- Configure stage-local enemy stats.
- Add wave spawn plans.
- Validate stage JSON before the game loads it.
- Simulate enemy spawn and goal timing.
- Print a compact stage summary.
- Calibrate `board_art.cells` visually against a background image.

## Build

From the repository root:

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Debug
cmake --build build --target ArknightBuilder
```

Windows users can build it through the root `build_win.bat`.

Show CLI help:

```bash
./build/ArknightBuilder --help
```

On Windows:

```bat
.\build\ArknightBuilder.exe --help
```

## Path Rules

All stage file arguments are mapped into `data/levels/`.

Examples:

- `tutorial_1.json` resolves to `data/levels/tutorial_1.json`
- `Operation 1-2/stage.json` resolves to `data/levels/Operation 1-2/stage.json`
- `Operation 1-2/stage` resolves to `data/levels/Operation 1-2/stage.json`
- Old prefixes such as `tools/ark_builder/levels/...`, `data/levels/...`, and `levels/...` are accepted and normalized.

Set `ARKNIGHT_DATA_ROOT` when you want the builder to read and write a different data directory:

```bash
ARKNIGHT_DATA_ROOT=/path/to/data ./build/ArknightBuilder show tutorial_1.json
```

## Commands

```bash
ArknightBuilder new <file> --name <name> --width <w> --height <h>
ArknightBuilder paint <file> <x> <y> <tile> [--rect-width <w>] [--rect-height <h>]
ArknightBuilder route-set <file> <route-id> <x,y[:wait[:direction]]>...
ArknightBuilder enemy-set <file> <enemy-id> --hp <value> --speed <value>
ArknightBuilder spawn-add <file> --enemy <enemy-id> --route <route-id> --count <n> --start <sec> --interval <sec>
ArknightBuilder validate <file>
ArknightBuilder simulate <file> [--duration <sec>]
ArknightBuilder show <file>
ArknightBuilder calibrate [file]
```

## Command Details

### `new`

Creates a blank stage with empty tiles, no routes, no enemies, and no waves.

```bash
./build/ArknightBuilder new stage_01.json --name stage_01 --width 16 --height 10
```

### `paint`

Writes a tile type at one position. Use `--rect-width` and `--rect-height` to paint a rectangle.

```bash
./build/ArknightBuilder paint stage_01.json 0 4 spawn
./build/ArknightBuilder paint stage_01.json 15 4 goal
./build/ArknightBuilder paint stage_01.json 1 4 road --rect-width 14 --rect-height 1
```

Tile types:

- `empty`: unused tile
- `road`: enemy path tile; operators cannot deploy here
- `ground`: ground operator deployment tile
- `highground`: highground operator deployment tile
- `unusablehighground`: blocked highground tile
- `spawn`: enemy entry tile
- `goal`: enemy exit tile

### `route-set`

Replaces a route with a list of route nodes.

```bash
./build/ArknightBuilder route-set stage_01.json main 0,4:0:normal 1,4 2,4 3,4 4,4:1.0 5,4 6,4 7,4 8,4 9,4 10,4 11,4 12,4 13,4 14,4 15,4
```

Route node format:

```text
x,y
x,y:wait
x,y:direction
x,y:wait:direction
```

`wait` is seconds spent at that node. `direction` can be `normal`, `default`, `flip`, or `flipped`. The first route node must specify `normal` or `flip`; later nodes inherit the previous direction unless a new direction is specified.

Routes must:

- stay inside the stage bounds
- move orthogonally one tile at a time
- start on a `spawn` tile
- end on a `goal` tile
- pass only through `ground`, `road`, `spawn`, or `goal`

### `enemy-set`

Creates or updates stage-local enemy stats.

```bash
./build/ArknightBuilder enemy-set stage_01.json slug --hp 100 --speed 1.2
```

The `enemy-id` should match the ID used by waves. The runtime can also resolve aliases from `data/enemy/*.json`.

### `spawn-add`

Appends a wave entry.

```bash
./build/ArknightBuilder spawn-add stage_01.json --enemy slug --route main --count 8 --start 0 --interval 1.5
```

Wave fields:

- `enemy`: enemy ID or runtime alias
- `route`: route ID
- `count`: number of units, must be greater than 0
- `start`: first spawn time in seconds
- `interval`: seconds between spawns

### `validate`

Checks the stage schema, tile map, route topology, enemy stats, and wave references.

```bash
./build/ArknightBuilder validate stage_01.json
```

Validation fails when any issue is found, which makes it suitable for scripts and CI.

### `simulate`

Prints a simple enemy timing table using route length, waits, speed, wave start, and interval.

```bash
./build/ArknightBuilder simulate stage_01.json --duration 60
```

Rows marked `GOAL` reach the goal within the requested duration. Rows marked `ACTIVE` are still on the map at the end of the simulation window.

### `show`

Prints a compact summary:

```bash
./build/ArknightBuilder show stage_01.json
```

Output includes stage name, size, route count, enemy count, wave count, and valid/invalid status.

### `calibrate`

Opens the coordinate calibration UI.

```bash
./build/ArknightBuilder calibrate 'Operation 1-1/stage.json'
```

Use calibration when a stage has a background image and the logical grid needs to line up with the art. The tool writes `board_art.reference_size` and per-cell `board_art.cells` corner coordinates back into the stage JSON.

Calibration controls:

- Stage dropdown: choose a JSON file under `data/levels`
- `Cell X` / `Cell Y`: choose the active logic cell
- Left-click map: set the selected corner
- Drag a yellow handle: move a cell corner
- Right-click map: select the hovered cell
- `Save`: write changes to JSON
- `Reload`: discard unsaved in-memory changes and reload the file
- `ESC`: close the calibration window

Connected corners move together, so shared vertices between neighboring cells stay aligned.

## Full Stage Creation Example

```bash
./build/ArknightBuilder new stage_01.json --name stage_01 --width 16 --height 10
./build/ArknightBuilder paint stage_01.json 0 4 spawn
./build/ArknightBuilder paint stage_01.json 15 4 goal
./build/ArknightBuilder paint stage_01.json 1 4 road --rect-width 14 --rect-height 1
./build/ArknightBuilder route-set stage_01.json main 0,4:0:normal 1,4 2,4 3,4 4,4:1.0 5,4 6,4 7,4 8,4 9,4 10,4 11,4 12,4 13,4 14,4 15,4
./build/ArknightBuilder enemy-set stage_01.json slug --hp 100 --speed 1.2
./build/ArknightBuilder spawn-add stage_01.json --enemy slug --route main --count 8 --start 0 --interval 1.5
./build/ArknightBuilder validate stage_01.json
./build/ArknightBuilder show stage_01.json
./build/ArknightBuilder simulate stage_01.json --duration 30
```

The generated file is written to `data/levels/stage_01.json`.

## Stage JSON Notes

Required root fields:

- `name`: stage display name
- `width`: grid width
- `height`: grid height
- `tiles`: 2D tile array
- `routes`: object keyed by route ID
- `enemies`: object keyed by enemy ID
- `waves`: array of wave objects

Optional visual fields used by the runtime include `background`, `loading`, `finish`, `tile_images`, `camera`, `board_layout`, and `board_art`.
