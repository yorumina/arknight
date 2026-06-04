# Arknight Linux

[English](README.md) | 繁體中文

`Arknight Linux` 是使用 C++17 與 `PTSD` 框架製作的明日方舟風格 2D 塔防原型。專案目前包含格子地圖、敵人波次、幹員部署、DP/LP 系統、幹員與敵人動畫、暫停/倍速控制，以及 JSON 驅動的遊戲資料。

## 環境需求

- CMake 3.16+
- 支援 C++17 的編譯器：GCC、Clang 或 MSVC
- Git
- 可用的 OpenGL 執行環境
- 建議安裝 FFmpeg，方便處理動畫素材轉換與快取建立流程

## 建置

下載專案與 submodule：

```bash
git clone --recurse-submodules <your-repo-url>
cd Arknight_Linux
```

設定並建置遊戲：

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Debug
cmake --build build --target Arknight
```

建置關卡工具：

```bash
cmake --build build --target ArknightBuilder
```

## 執行

Linux/macOS：

```bash
./build/Arknight
```

Windows：

```powershell
.\build\Arknight.exe
```

## 操作方式

- `滑鼠左鍵`：點擊 UI、從下方欄位拖曳幹員、確認部署方向
- `滑鼠右鍵`：取消部署或撤退已部署幹員
- `SPACE`：等待戰鬥開始時啟動下一波
- `R`：重新開始目前 Demo
- `M`：開啟或關閉可見的地圖模型/格線疊層
- `Z`：開啟或關閉作弊模式，戰鬥時間流速變為 10 倍，幹員攻擊傷害變為 10 倍
- `ESC`：開啟離開確認或結束遊戲
- 右上角 `1X` / `2X` 按鈕：切換遊戲速度
- 右上角暫停按鈕：暫停或繼續
- 左上角 `MAP` 按鈕：開啟或關閉可見的地圖模型/格線疊層(開發用工具)

## 資料目錄

遊戲資料位於 `data/`：

- `data/levels`：關卡 JSON 與關卡圖片
- `data/enemy`：敵人 JSON 與敵人動畫
- `data/operators`：幹員 JSON 與幹員動畫/圖片素材
- `data/levels_pic`：HUD 與關卡 UI 圖片

常見關卡路徑：

- `data/levels/test.json`
- `data/levels/tutorial_1.json`
- `data/levels/Operation 1-1/stage.json`
- `data/levels/Operation 1-2/stage.json`

## 動畫快取

動畫會依需求解碼，並可透過磁碟快取避免每次啟動遊戲都重新轉換昂貴的動畫素材。

常用環境變數：

- `ARKNIGHT_ANIMATION_CACHE_MB=768`：設定記憶體動畫快取上限，單位 MB
- `ARKNIGHT_ANIMATION_CACHE_MB=0`：取消記憶體快取上限
- `ARKNIGHT_ANIMATION_DISK_CACHE=0`：關閉磁碟快取
- `ARKNIGHT_ANIMATION_DISK_CACHE_DIR=/path/to/cache`：指定自訂快取資料夾
- `ARKNIGHT_ANIMATION_PRELOAD=1`：單次預先建立所有已知動畫快取
- `ARKNIGHT_ENEMY_ANIMATION_PRELOAD=1`：只預先建立敵人動畫快取
- `ARKNIGHT_GPU_ADAPTER=auto|nvidia|amd|intel`：在建立視窗前提示偏好的 GPU

建議流程：

```bash
ARKNIGHT_ANIMATION_PRELOAD=1 ./build/Arknight
```

快取建立完成後，日常啟動請直接正常執行遊戲，啟動速度與記憶體使用會比較穩定。

## ArknightBuilder

`ArknightBuilder` 是用來建立、修改、驗證與模擬關卡 JSON 的命令列工具。

範例：

```bash
./build/ArknightBuilder validate tutorial_1.json
./build/ArknightBuilder show tutorial_1.json
./build/ArknightBuilder simulate tutorial_1.json --duration 60
./build/ArknightBuilder calibrate 'Operation 1-1/stage.json'
```

關卡檔參數會自動映射到 `data/levels/`，所以 `tutorial_1.json` 會解析成 `data/levels/tutorial_1.json`。

完整 Builder 文件：

- [英文](docs/arknightbuilder/README.md)
- [繁體中文](docs/arknightbuilder/README_zh-tw.md)

## 素材工具

產生左右翻轉的幹員 front WebM 素材：

```bash
./tools/generate_flipped_front.sh
```

APNG 素材可用 FFmpeg 手動翻轉：

```bash
ffmpeg -hide_banner -loglevel error -y -i input.apng -vf hflip -plays 0 output.apng
```

## 專案結構

- `src/`：遊戲執行邏輯
- `include/`：公開標頭檔
- `tools/ark_builder/`：`ArknightBuilder` 原始碼
- `PTSD/`：框架與 bundled dependencies
- `docs/`：專案文件
