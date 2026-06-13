# Arknight Linux

[English](README.md) | 繁體中文

`Arknight Linux` 是一個以 C++17 與 `PTSD` framework 製作的明日方舟風格 2D 塔防原型。專案包含格子關卡、敵人波次、幹員部署、DP/LP 系統、幹員與敵人動畫、暫停與倍速、JSON 資料驅動，以及關卡製作工具 `ArknightBuilder`。

## 文件

- [Windows 部署與遊戲操作手冊](ForWindows.md)
- [ArknightBuilder 英文手冊](docs/arknightbuilder/README.md)
- [ArknightBuilder 中文手冊](docs/arknightbuilder/README_zh-tw.md)

## 需求

- CMake 3.16+
- 支援 C++17 的編譯器：GCC、Clang 或 MSVC
- Git
- 可執行 OpenGL 的環境
- FFmpeg，用於動畫轉檔與快取產生流程

## 建置

下載專案與 submodule：

```bash
git clone --recurse-submodules <your-repo-url>
cd Arknight_Linux
```

建置遊戲：

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Debug
cmake --build build --target Arknight
```

建置關卡工具：

```bash
cmake --build build --target ArknightBuilder
```

## 執行

```bash
./build/Arknight
```

## 功能狀態

目前原型規劃功能皆已完成。

- [x] 關卡載入、載入畫面、完成畫面與失敗畫面
- [x] 幹員部署、方向選擇、數值 UI、技能、再部署冷卻、HP/SP 條與攻擊範圍
- [x] 敵人波次、路線移動、路線方向翻轉、戰鬥、死亡動畫與敵人計數
- [x] ArknightBuilder 關卡建立、格子繪製、路線編輯、驗證、模擬與地圖校準
- [x] JSON 驅動的關卡、幹員、敵人、動畫與 UI 資源載入

## 資料目錄

遊戲資料位於 `data/`：

- `data/levels`：關卡 JSON 與關卡專用圖片
- `data/enemy`：敵人 JSON 與敵人動畫
- `data/operators`：幹員 JSON、幹員動畫與圖片
- `data/levels_pic`：HUD 與關卡 UI 圖片

常用關卡路徑：

- `data/levels/test.json`
- `data/levels/tutorial_1.json`
- `data/levels/Operation 1-1/stage.json`
- `data/levels/Operation 1-2/stage.json`

## 動畫快取

動畫會在需要時解碼，並可使用磁碟快取避免每次啟動都重新轉換。

常用環境變數：

- `ARKNIGHT_ANIMATION_CACHE_MB=768`：設定記憶體動畫快取上限，單位 MB
- `ARKNIGHT_ANIMATION_CACHE_MB=0`：停用記憶體快取上限
- `ARKNIGHT_ANIMATION_DISK_CACHE=0`：停用磁碟快取
- `ARKNIGHT_ANIMATION_DISK_CACHE_DIR=/path/to/cache`：指定自訂快取目錄
- `ARKNIGHT_ANIMATION_PRELOAD=1`：在單次執行中預載動畫快取
- `ARKNIGHT_ENEMY_ANIMATION_PRELOAD=1`：只預載敵人動畫
- `ARKNIGHT_GPU_ADAPTER=auto|nvidia|amd|intel`：在建立視窗前提示偏好的 GPU

## ArknightBuilder

`ArknightBuilder` 是關卡製作用 CLI 工具。它可以建立空白關卡、繪製格子、編輯路線、設定敵人與波次、驗證 JSON、模擬敵人時間軸、查看關卡摘要，並用校準工具把地圖圖片對齊邏輯格子。

快速範例：

```bash
./build/ArknightBuilder validate tutorial_1.json
./build/ArknightBuilder show tutorial_1.json
./build/ArknightBuilder simulate tutorial_1.json --duration 60
./build/ArknightBuilder calibrate 'Operation 1-1/stage.json'
```

完整命令、關卡 JSON 規則、路線格式、驗證行為、模擬輸出與校準流程請看 [ArknightBuilder 中文手冊](docs/arknightbuilder/README_zh-tw.md)。

## 資源輔助工具

產生幹員正面翻轉 WebM 資源：

```bash
./tools/generate_flipped_front.sh
```

產生敵人翻轉 APNG 動畫：

```bash
./tools/generate_flipped_enemies.sh
```

也可以手動使用 FFmpeg 翻轉 APNG：

```bash
ffmpeg -hide_banner -loglevel error -y -i input.apng -vf hflip -plays 0 output.apng
```

## 專案結構

- `src/`：遊戲 runtime 實作
- `include/`：公開 header
- `tools/ark_builder/`：`ArknightBuilder` 原始碼
- `PTSD/`：framework 與 bundled dependencies
- `docs/`：專案文件
