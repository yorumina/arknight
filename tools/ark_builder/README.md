# ArknightBuilder (PTSD Framework)

`ArknightBuilder` is a CLI stage-construction tool for Proposal execution.
The tool is outside `PTSD/`, but built with the PTSD framework via CMake (`add_subdirectory(PTSD)`).

## Build

From repository root:

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Debug
cmake --build build --target ArknightBuilder
```

The executable is generated under `build/`.

Run it from repo root with one of these forms:

```bash
./build/ArknightBuilder --help
./ark-builder --help
```

If you type `ArknightBuilder` without `./` and without adding `build/` to `PATH`, you will get `command not found`.

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
```

Tile types:

- `empty`
- `road`
- `ground`
- `highground`
- `spawn`
- `goal`

## Quickstart

Use the sample stage:

```bash
ArknightBuilder validate tools/ark_builder/levels/tutorial_1.json
ArknightBuilder show tools/ark_builder/levels/tutorial_1.json
ArknightBuilder simulate tools/ark_builder/levels/tutorial_1.json --duration 60
```

Or create your own stage:

```bash
ArknightBuilder new tools/ark_builder/levels/stage_01.json --name stage_01 --width 16 --height 10
ArknightBuilder paint tools/ark_builder/levels/stage_01.json 0 4 spawn
ArknightBuilder paint tools/ark_builder/levels/stage_01.json 15 4 goal
ArknightBuilder paint tools/ark_builder/levels/stage_01.json 1 4 road --rect-width 14 --rect-height 1
ArknightBuilder route-set tools/ark_builder/levels/stage_01.json main 0,4 1,4 2,4 3,4 4,4:1.0 5,4 6,4 7,4 8,4 9,4 10,4 11,4 12,4 13,4 14,4 15,4
ArknightBuilder enemy-set tools/ark_builder/levels/stage_01.json slug --hp 100 --speed 1.2
ArknightBuilder spawn-add tools/ark_builder/levels/stage_01.json --enemy slug --route main --count 8 --start 0 --interval 1.5
ArknightBuilder validate tools/ark_builder/levels/stage_01.json
```

## Scope note

This tool targets gameplay-system prototyping. Keep assets and storyline original to avoid IP/legal issues.

