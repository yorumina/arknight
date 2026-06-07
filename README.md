# Arknight Linux

English | [Traditional Chinese](README_zh-tw.md)

`Arknight Linux` is an Arknights-style 2D tower-defense prototype built with C++17 and the `PTSD` framework. It includes tile-based stages, enemy waves, operator deployment, DP/LP systems, animated operators/enemies, pause/speed controls, and JSON-driven game data.

## Requirements

- CMake 3.16+
- A C++17 compiler: GCC, Clang, or MSVC
- Git
- OpenGL-capable runtime
- FFmpeg tools are recommended for animation asset conversion and cache generation workflows

## Build

### Linux/macOS

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

### Windows

To compile on Windows, you must ensure the build environment is properly set up. Note that CMake and MSVC may crash if configured directly in folder paths containing Chinese characters or spaces (like OneDrive's `文件`). We provide helper scripts to bypass this.

1. **Install FFmpeg**: Make sure `ffmpeg` is installed and in your environment `PATH`.
2. **Download Submodules**: In PowerShell, run the download script to fetch the required `freetype` and `harfbuzz` dependencies:
   ```powershell
   powershell -ExecutionPolicy Bypass -File PTSD/lib/sdl2_ttf/external/Get-GitModules.ps1
   ```
3. **Build**: Run the Windows build batch script, which synchronizes files to a temporary `C:\ArkBuild` path, compiles there, and copies the `Arknight.exe` binary back to `.\build\Arknight.exe`:
   ```cmd
   .\build_win.bat
   ```

## Run

Linux/macOS:

```bash
./build/Arknight
```

Windows:

```powershell
.\build\Arknight.exe
```

Or run with animation preloading enabled (`ARKNIGHT_ANIMATION_PRELOAD=1`):
```cmd
.\run_preload.bat
```

## Controls

- `Left Mouse Button`: select UI buttons, drag operators from the bar, and confirm deployment direction
- `Right Mouse Button`: cancel deployment or retreat a deployed operator
- `SPACE`: start the next wave while waiting before combat
- `R`: restart the current demo
- `M`: toggle the visible map model/grid overlay
- `Z`: toggle cheat mode, which runs battle time at 10x and gives operators 10x attack damage
- `ESC`: open quit confirmation or exit
- Top-right `1X` / `2X` button: toggle game speed
- Top-right pause button: pause or resume
- Top-left `MAP` button: toggle the visible map model/grid overlay

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

Animations are decoded on demand and can use a disk cache to avoid doing expensive conversion work every time the game starts.

Useful environment variables:

- `ARKNIGHT_ANIMATION_CACHE_MB=768`: set the in-memory animation cache limit in MB
- `ARKNIGHT_ANIMATION_CACHE_MB=0`: disable the memory cache limit
- `ARKNIGHT_ANIMATION_DISK_CACHE=0`: disable the disk cache
- `ARKNIGHT_ANIMATION_DISK_CACHE_DIR=/path/to/cache`: choose a custom cache directory
- `ARKNIGHT_ANIMATION_PRELOAD=1`: preload and build all known animation cache entries for one run
- `ARKNIGHT_ENEMY_ANIMATION_PRELOAD=1`: preload enemy animation clips only
- `ARKNIGHT_GPU_ADAPTER=auto|nvidia|amd|intel`: hint the preferred GPU adapter before the window is created

Recommended workflow:

```bash
ARKNIGHT_ANIMATION_PRELOAD=1 ./build/Arknight
```

After the cache is built once, start the game normally for faster startup and lower memory use.

## ArknightBuilder

`ArknightBuilder` is a CLI helper for creating, editing, validating, and simulating stage JSON files.

Examples:

```bash
./build/ArknightBuilder validate tutorial_1.json
./build/ArknightBuilder show tutorial_1.json
./build/ArknightBuilder simulate tutorial_1.json --duration 60
./build/ArknightBuilder calibrate 'Operation 1-1/stage.json'
```

Stage file arguments are automatically mapped to `data/levels/`, so `tutorial_1.json` resolves to `data/levels/tutorial_1.json`.

Detailed builder documentation:

- [English](docs/arknightbuilder/README.md)
- [Traditional Chinese](docs/arknightbuilder/README_zh-tw.md)

## Asset Helpers

Generate flipped WebM front-facing operator assets:

```bash
./tools/generate_flipped_front.sh
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
