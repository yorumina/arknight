# Arknight Linux

English | [Traditional Chinese](README_zh-tw.md)

`Arknight Linux` is an Arknights-style 2D tower-defense prototype built with C++17 and the `PTSD` framework. It includes tile-based stages, enemy waves, operator deployment, DP/LP systems, animated operators/enemies, pause/speed controls, JSON-driven game data, and a command-line stage builder.

## Documentation

- [Windows deployment and gameplay guide](ForWindows.md)
- [ArknightBuilder manual](docs/arknightbuilder/README.md)
- [ArknightBuilder Traditional Chinese manual](docs/arknightbuilder/README_zh-tw.md)

## Requirements

- CMake 3.16+
- A C++17 compiler: GCC, Clang, or MSVC
- Git
- OpenGL-capable runtime
- FFmpeg tools for animation conversion and cache generation workflows

## Build

Clone with submodules:

```bash
git clone --recurse-submodules <your-repo-url>
cd Arknight_Linux
```

Configure and build the game:

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Debug
cmake --build build --target Arknight
```

Build the stage builder CLI:

```bash
cmake --build build --target ArknightBuilder
```

## Run

```bash
./build/Arknight
```

## Feature Status

All planned prototype features are complete.

- [x] Stage loading, loading screens, finish screens, and failure overlay
- [x] Operator deployment, direction selection, stats UI, skills, redeploy cooldowns, HP/SP bars, and attack ranges
- [x] Enemy waves, route movement, route direction flipping, combat, death animations, and enemy counters
- [x] ArknightBuilder stage creation, painting, route editing, validation, simulation, and calibration
- [x] JSON-driven stage, operator, enemy, animation, and UI asset loading

## Data Layout

Game data lives under `data/`:

- `data/levels`: stage JSON files and stage-specific images
- `data/enemy`: enemy JSON files and enemy animation clips
- `data/operators`: operator JSON files and operator animation/image assets
- `data/levels_pic`: HUD and level UI images

Common stage paths:

- `data/levels/test.json`
- `data/levels/tutorial_1.json`
- `data/levels/Operation 1-1/stage.json`
- `data/levels/Operation 1-2/stage.json`

## Animation Cache

Animations are decoded on demand and can use a disk cache to avoid expensive conversion work every time the game starts.

Useful environment variables:

- `ARKNIGHT_ANIMATION_CACHE_MB=768`: set the in-memory animation cache limit in MB
- `ARKNIGHT_ANIMATION_CACHE_MB=0`: disable the memory cache limit
- `ARKNIGHT_ANIMATION_DISK_CACHE=0`: disable the disk cache
- `ARKNIGHT_ANIMATION_DISK_CACHE_DIR=/path/to/cache`: choose a custom cache directory
- `ARKNIGHT_ANIMATION_PRELOAD=1`: preload animation cache entries for one run
- `ARKNIGHT_ENEMY_ANIMATION_PRELOAD=1`: preload enemy animation clips only
- `ARKNIGHT_GPU_ADAPTER=auto|nvidia|amd|intel`: hint the preferred GPU adapter before the window is created

## ArknightBuilder

`ArknightBuilder` is a CLI tool for stage authoring. It can create blank stages, paint tiles, edit routes, configure enemies and waves, validate JSON, simulate enemy timing, inspect a stage summary, and calibrate map art against the logic grid.

Quick examples:

```bash
./build/ArknightBuilder validate tutorial_1.json
./build/ArknightBuilder show tutorial_1.json
./build/ArknightBuilder simulate tutorial_1.json --duration 60
./build/ArknightBuilder calibrate 'Operation 1-1/stage.json'
```

Read the full [ArknightBuilder manual](docs/arknightbuilder/README.md) for command syntax, stage JSON rules, route format, validation behavior, simulation output, and calibration workflow.

## Asset Helpers

Generate flipped WebM front-facing operator assets:

```bash
./tools/generate_flipped_front.sh
```

Generate flipped APNG enemy animation assets:

```bash
./tools/generate_flipped_enemies.sh
```

APNG assets can be flipped manually with FFmpeg:

```bash
ffmpeg -hide_banner -loglevel error -y -i input.apng -vf hflip -plays 0 output.apng
```

## Project Layout

- `src/`: game runtime implementation
- `include/`: public headers
- `tools/ark_builder/`: `ArknightBuilder` source
- `PTSD/`: framework and bundled dependencies
- `docs/`: project documentation
