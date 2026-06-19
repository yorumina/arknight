# ArknightBuilder 手冊

[English](README.md) | 繁體中文

`ArknightBuilder` 是 `Arknight Linux` 的關卡製作用 CLI 工具。它會直接編輯遊戲 runtime 使用的 JSON 關卡檔，並提供一個小型校準 UI，用來把關卡背景圖對齊邏輯格子。

## 功能

- 建立空白關卡 JSON。
- 繪製單一格子或矩形格子區域。
- 建立敵人路線，包含等待時間與動畫方向標記。
- 設定關卡內敵人數值。
- 新增敵人波次。
- 在遊戲載入前驗證關卡 JSON。
- 模擬敵人生成與抵達目標點的時間。
- 顯示關卡摘要。
- 用視覺化工具校準 `board_art.cells` 與背景圖片。

## 建置

在專案根目錄執行：

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Debug
cmake --build build --target ArknightBuilder
```

Windows 使用者也可以透過根目錄的 `build_win.bat` 建置。

顯示 CLI 說明：

```bash
./build/ArknightBuilder --help
```

Windows：

```bat
.\build\ArknightBuilder.exe --help
```

## 路徑規則

所有關卡檔案參數都會被映射到 `data/levels/`。

範例：

- `tutorial_1.json` 會解析成 `data/levels/tutorial_1.json`
- `Operation 1-2/stage.json` 會解析成 `data/levels/Operation 1-2/stage.json`
- `Operation 1-2/stage` 會解析成 `data/levels/Operation 1-2/stage.json`
- 舊格式前綴如 `tools/ark_builder/levels/...`、`data/levels/...`、`levels/...` 也會被接受並正規化

若要讓工具讀寫不同的 data 目錄，可以設定 `ARKNIGHT_DATA_ROOT`：

```bash
ARKNIGHT_DATA_ROOT=/path/to/data ./build/ArknightBuilder show tutorial_1.json
```

## 命令總覽

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
ArknightBuilder menu-calibrate [image]
ArknightBuilder opening-calibrate [video] [--target video1|video2] [--frame-sec <sec>] [--start-sec <sec>] [--end-sec <sec>]
```

## 命令說明

### `new`

建立空白關卡，內容包含 empty tiles、空 routes、空 enemies、空 waves。

```bash
./build/ArknightBuilder new stage_01.json --name stage_01 --width 16 --height 10
```

### `paint`

把指定位置寫成指定 tile。使用 `--rect-width` 與 `--rect-height` 可以繪製矩形。

```bash
./build/ArknightBuilder paint stage_01.json 0 4 spawn
./build/ArknightBuilder paint stage_01.json 15 4 goal
./build/ArknightBuilder paint stage_01.json 1 4 road --rect-width 14 --rect-height 1
```

Tile 類型：

- `empty`：未使用格
- `road`：敵人路線格，不能部署幹員
- `ground`：地面幹員部署格
- `highground`：高台幹員部署格
- `unusablehighground`：不可部署高台格
- `spawn`：敵人入口
- `goal`：敵人出口

### `route-set`

用一串節點取代指定 route。

```bash
./build/ArknightBuilder route-set stage_01.json main 0,4:0:normal 1,4 2,4 3,4 4,4:1.0 5,4 6,4 7,4 8,4 9,4 10,4 11,4 12,4 13,4 14,4 15,4
```

路線節點格式：

```text
x,y
x,y:wait
x,y:direction
x,y:wait:direction
```

`wait` 是敵人在該點等待的秒數。`direction` 可使用 `normal`、`default`、`flip`、`flipped`。第一個路線節點必須指定 `normal` 或 `flip`；後續節點若未指定方向，就沿用前一個方向。

路線必須符合：

- 節點在關卡範圍內
- 每一步只能上下左右移動一格
- 起點必須是 `spawn`
- 終點必須是 `goal`
- 路線只能通過 `ground`、`road`、`spawn`、`goal`

### `enemy-set`

建立或更新關卡內敵人數值。

```bash
./build/ArknightBuilder enemy-set stage_01.json slug --hp 100 --speed 1.2
```

`enemy-id` 需要與 wave 使用的 ID 一致。runtime 也可以從 `data/enemy/*.json` 解析敵人 alias。

### `spawn-add`

新增一筆 wave。

```bash
./build/ArknightBuilder spawn-add stage_01.json --enemy slug --route main --count 8 --start 0 --interval 1.5
```

Wave 欄位：

- `enemy`：敵人 ID 或 runtime alias
- `route`：路線 ID
- `count`：敵人數量，必須大於 0
- `start`：第一隻敵人的生成時間，單位秒
- `interval`：每隻敵人之間的間隔秒數

### `validate`

檢查關卡 schema、tile map、路線拓樸、敵人數值與 wave 參照。

```bash
./build/ArknightBuilder validate stage_01.json
```

只要有任何問題，`validate` 就會失敗，因此可以放進腳本或 CI。

### `simulate`

根據路線長度、等待時間、敵人速度、wave start 與 interval 印出簡易敵人時間表。

```bash
./build/ArknightBuilder simulate stage_01.json --duration 60
```

標記為 `GOAL` 的敵人會在指定時間內抵達目標點；標記為 `ACTIVE` 的敵人在模擬結束時仍在地圖上。

### `show`

印出簡短關卡摘要：

```bash
./build/ArknightBuilder show stage_01.json
```

輸出包含關卡名稱、尺寸、路線數、敵人數、wave 數，以及 valid/invalid 狀態。

### `calibrate`

開啟座標校準 UI。

```bash
./build/ArknightBuilder calibrate 'Operation 1-1/stage.json'
```

當關卡有背景圖，而邏輯格需要對齊圖片上的地圖時使用。工具會把 `board_art.reference_size` 與每格的 `board_art.cells` 角點座標寫回關卡 JSON。

校準操作：

- Stage 下拉選單：選擇 `data/levels` 下的 JSON
- `Cell X` / `Cell Y`：選擇目前邏輯格
- 左鍵點地圖：設定目前選擇的角點
- 拖曳黃色控制點：移動格子的角
- 右鍵點地圖：選擇滑鼠所在格
- `Save`：寫回 JSON
- `Reload`：丟棄尚未儲存的記憶體修改並重新載入
- `ESC`：關閉校準視窗

相鄰格子的共用角點會一起移動，讓格線保持連續。

### `menu-calibrate`

開啟 loading page 選關按鈕校準 UI。

```bash
./build/ArknightBuilder menu-calibrate
```

這個工具預設開啟 `data/loadingpage/loadingpage_3.png`，讓你標定兩個選關按鈕的四角座標，並寫入 `data/loadingpage/menu_buttons.json` 給遊戲 runtime 使用。

- Button 下拉選單：選擇 Operation 1-1 或 Operation 1-2
- Corner 下拉選單：選擇要設定的角
- 左鍵點圖：設定目前選擇的角
- 拖曳黃色點：移動角點
- 右鍵點圖：選擇滑鼠所在的按鈕
- Save：儲存按鈕四角座標
- `ESC`：關閉校準視窗

### `opening-calibrate`

開啟第一段開場影片的點擊區域與有效時間標定工具。

```bash
./build/ArknightBuilder opening-calibrate
```

工具會抽出影片預覽畫面，並把兩段影片分開寫入 `data/loadingpage/menu_buttons.json`：`video_1_action` 給第一段影片切換到第二段用，`video_2_awaken` 給第二段影片的開始喚醒按鈕用。工具會依影片檔名判斷目標，也可以用 `--target video1|video2` 強制指定。

- `--target`：選擇 `video1` / `loginpage_1` 或 `video2` / `loginpage_2`
- `--frame-sec`：用來標定的預覽影格秒數
- `--start-sec`：最早接受點擊的秒數
- `--end-sec`：最晚接受點擊的秒數
- 左鍵點圖：設定目前選擇的角
- 拖曳黃色點：移動角點
- Save：儲存影片點擊區域與時間窗
- `ESC`：關閉校準視窗

## 完整建立關卡範例

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

產生的檔案會寫入 `data/levels/stage_01.json`。

## 關卡 JSON 備註

必要根欄位：

- `name`：關卡名稱
- `width`：格子寬度
- `height`：格子高度
- `tiles`：2D tile array
- `routes`：以 route ID 為 key 的 object
- `enemies`：以 enemy ID 為 key 的 object
- `waves`：wave object array

runtime 也支援可選視覺欄位，例如 `background`、`loading`、`finish`、`tile_images`、`camera`、`board_layout`、`board_art`。
