# Arknight Demo

This repository contains an Arknight-style gameplay prototype built on top of the `PTSD` framework.

## Requirements

- CMake 3.16+
- A C++17 compiler (MSVC / Clang / GCC)

## Build and Run

1. Clone with submodules:
```bash
git clone --recurse-submodules <your-repo-url>
cd Arknight_Linux
```

2. Configure and build:
```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Debug
cmake --build build --target Arknight
```

3. Run:
- Linux/macOS
```bash
./build/Arknight
```
- Windows
```powershell
.\build\Arknight.exe
```

## Controls

- `Left Mouse Button`: deploy operator
- `1`: select Vanguard (ground only)
- `2`: select Sniper (highground only)
- `SPACE`: start wave
- `R`: restart demo
- `ESC`: exit

## Data Layout

Gameplay data is now stored under:

- `data/levels`
- `data/enemy`
- `data/operators`

The demo tries stage files such as:

1. `data/levels/test.json`
2. `data/levels/tutorial_1.json`

## ArknightBuilder

Build the builder:
```bash
cmake --build build --target ArknightBuilder
```

Example:
```bash
./build/ArknightBuilder validate tutorial_1.json
```

`ArknightBuilder` now auto-maps stage file arguments to `data/levels/`.
Detailed builder documentation:

- English: `docs/arknightbuilder/README.md`
- Traditional Chinese: `docs/arknightbuilder/README_zh-tw.md`
