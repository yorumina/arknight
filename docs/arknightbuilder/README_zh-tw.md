# ArknightBuilder

[English](README.md) | 繁體中文

`ArknightBuilder` 是 `Arknight Linux` 的命令列關卡製作工具。它可以建立與修改關卡 JSON、驗證路線/出生波次/敵人資料，也能在不啟動完整遊戲的情況下做簡單關卡模擬。

## 建置

請在專案根目錄執行：

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Debug
cmake --build build --target ArknightBuilder
```

查看說明：

```bash
./build/ArknightBuilder --help
```

## 資料目錄

工具預期專案資料放在 `data/`：

- `data/levels`：關卡 JSON 與關卡圖片
- `data/enemy`：敵人定義與動畫資料夾
- `data/operators`：幹員定義與動畫/圖片資料夾

關卡檔參數會自動映射到 `data/levels/`。

範例：

- `tutorial_1.json` 會解析成 `data/levels/tutorial_1.json`
- `Operation 1-2/stage.json` 會解析成 `data/levels/Operation 1-2/stage.json`
- 舊路徑如 `tools/ark_builder/levels/...` 仍可輸入，工具會自動轉換

## 指令

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

## 快速開始

驗證並查看現有關卡：

```bash
./build/ArknightBuilder validate tutorial_1.json
./build/ArknightBuilder show tutorial_1.json
./build/ArknightBuilder simulate tutorial_1.json --duration 60
./build/ArknightBuilder calibrate 'Operation 1-1/stage.json'
```

建立一個小型直線關卡：

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

產生的檔案會寫入 `data/levels/stage_01.json`。

## 座標映射校準

當關卡背景圖要作為實際美術地圖，而 JSON 格子只保留為不可見邏輯層時，請使用 `calibrate`。校準視窗可以選擇關卡與格子，並直接拖曳地圖上的四個黃色角點；相連角點會一起移動，按下 `Save` 後會把該格四角座標寫回關卡 JSON。

```bash
./build/ArknightBuilder calibrate 'Operation 1-1/stage.json'
```

## 地形類型

- `empty`：未使用格
- `road`：敵人路線格
- `ground`：地面幹員部署格
- `highground`：高台幹員部署格
- `unusablehighground`：不可部署的高台格
- `spawn`：敵人入口
- `goal`：敵人出口

## 路線格式

路線點使用 `x,y` 座標。若要讓敵人在某一點等待，可以在該點後面加上 `:wait`。

範例：

```bash
./build/ArknightBuilder route-set stage_01.json main 0,4 1,4 2,4:1.0 3,4
```

## 驗證

`validate` 會檢查關卡結構，讓你在遊戲載入前先發現資料問題。手動編輯關卡或使用 builder 指令後，建議都跑一次：

```bash
./build/ArknightBuilder validate Operation\ 1-2/stage.json
```
