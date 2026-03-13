# ArknightBuilder (PTSD 框架)

`ArknightBuilder` 是一個用於執行專題企劃階段的命令列（CLI）關卡構建測試工具。

## 建置 (Build)

請在專案根目錄（repository root）下執行以下指令：

```bash
cmake -S PTSD -B PTSD/build -DCMAKE_BUILD_TYPE=Debug
cmake --build PTSD/build --target ArknightBuilder
```

執行檔將會產生在 `PTSD/build/` 資料夾下。

您可以從專案根目錄使用以下任一方式來執行此工具：

```bash
./PTSD/build/ArknightBuilder --help
./ark-builder --help
```

> 如果您直接輸入 `ArknightBuilder` 而沒有加上 `./`，且尚未將 `PTSD/build/` 加入到系統變數 `PATH` 之中，系統將會提示 `command not found`。

## 指令列表 (Commands)

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

支援的地形類型（Tile types）：

- `empty` (空地)
- `road` (道路)
- `ground` (地面 - 即低地，可用於部署近戰幹員)
- `highground` (高臺 - 可用於部署遠程幹員)
- `spawn` (敵人出生點)
- `goal` (敵人的目標終點)

## 快速開始 (Quickstart)

您可以使用我們建立好的範例關卡來快速進行測試：

```bash
./PTSD/build/ArknightBuilder validate tools/ark_builder/levels/tutorial_1.json
./PTSD/build/ArknightBuilder show tools/ark_builder/levels/tutorial_1.json
./PTSD/build/ArknightBuilder simulate tools/ark_builder/levels/tutorial_1.json --duration 60
```

或者，您也可以按照以下步驟從零建立您的專屬關卡：

```bash
./PTSD/build/ArknightBuilder new tools/ark_builder/levels/stage_01.json --name stage_01 --width 16 --height 10
./PTSD/build/ArknightBuilder paint tools/ark_builder/levels/stage_01.json 0 4 spawn
./PTSD/build/ArknightBuilder paint tools/ark_builder/levels/stage_01.json 15 4 goal
./PTSD/build/ArknightBuilder paint tools/ark_builder/levels/stage_01.json 1 4 road --rect-width 14 --rect-height 1
./PTSD/build/ArknightBuilder route-set tools/ark_builder/levels/stage_01.json main 0,4 1,4 2,4 3,4 4,4:1.0 5,4 6,4 7,4 8,4 9,4 10,4 11,4 12,4 13,4 14,4 15,4
./PTSD/build/ArknightBuilder enemy-set tools/ark_builder/levels/stage_01.json slug --hp 100 --speed 1.2
./PTSD/build/ArknightBuilder spawn-add tools/ark_builder/levels/stage_01.json --enemy slug --route main --count 8 --start 0 --interval 1.5
./PTSD/build/ArknightBuilder validate tools/ark_builder/levels/stage_01.json
```

## 注意事項 (Scope note)

本工具的主要目標是用於快速製作與驗證**遊戲系統的原型設計**。為避免智慧財產權等相關法律問題，請盡量保持遊戲內的美術素材與故事情節為原創。
