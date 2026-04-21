# ArknightBuilder

`ArknightBuilder` 是用來建立與驗證關卡的命令列工具。

## 建置

請在專案根目錄執行：

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Debug
cmake --build build --target ArknightBuilder
```

執行：

```bash
./build/ArknightBuilder --help
```

## 資料夾結構

專案資料已統一放在：

- `data/levels`
- `data/enemy`
- `data/operators`

## 指令格式

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

關卡檔路徑規則：

- 所有關卡檔參數都會自動映射到 `data/levels/`。
- 舊路徑（例如 `tools/ark_builder/levels/...`）仍可輸入，工具會自動轉換。

## 快速開始

使用現成範例關卡：

```bash
./build/ArknightBuilder validate tutorial_1.json
./build/ArknightBuilder show tutorial_1.json
./build/ArknightBuilder simulate tutorial_1.json --duration 60
```

建立新關卡：

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

## 地形類型

- `empty`：空地
- `road`：道路
- `ground`：地面
- `highground`：高台
- `spawn`：出生點
- `goal`：終點
