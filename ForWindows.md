# Windows 部署與遊戲操作手冊

本文件是 Windows 使用者的完整流程：安裝工具、建置、終端機預載動畫快取、啟動遊戲，以及遊戲內操作。

## 1. 安裝必要工具

1. 安裝 Git。
2. 安裝 CMake 3.16 以上，並把 CMake 加入 PATH。
3. 安裝 Visual Studio 2022 Build Tools，工作負載勾選 `Desktop development with C++`。
4. 安裝 FFmpeg，並確認 `ffmpeg.exe` 與 `ffprobe.exe` 可以在終端機執行。
   - 建議安裝到 `C:\Program Files\ffmpeg\bin`。
   - 或把 FFmpeg 的 `bin` 目錄加入 PATH。

## 2. 建置遊戲與預載器

在專案根目錄開 PowerShell 或 CMD：

```bat
.\build_win.bat
```

這個腳本會把專案同步到 `C:\ArkBuild` 這個英文路徑下建置。成功後會產生：

```text
build\Arknight.exe
build\ArknightPreload.exe
```

## 3. 終端機預載動畫快取

```bat
.\run_preload.bat
```

成功時會看到：

```text
Preload completed.
Run the game with: .\build\Arknight.exe
```

預載器會掃描 `data/operators` 與 `data/enemy` 下的 `.apng` / `.webm` 動畫，建立磁碟快取，讓遊戲啟動或首次使用動畫時更順。

## 4. 啟動遊戲

```bat
.\build\Arknight.exe
```

## 5. 遊戲流程

1. 進入遊戲後會先載入關卡與動畫資源。
2. 從下方部署列拖曳幹員到可部署格。
3. 放開滑鼠後選擇方向，確認部署。
4. 阻止敵人抵達目標點，防守保護目標。
5. 完成第一關後，遊戲會自動進入第二關。

## 6. 滑鼠操作

- 左鍵：點擊 UI、拖曳幹員、確認部署方向。
- 右鍵：取消部署流程，或撤退已部署的幹員。
- 點擊右上 `1X` / `2X`：切換遊戲速度。
- 點擊右上暫停按鈕：暫停或繼續。

## 7. 鍵盤操作

- `R`：重新開始目前 demo。
- `Z`：切換作弊模式，戰鬥時間加速並提高幹員攻擊力。
- `ESC`：開啟退出確認，或離開遊戲。

## 8. 部署規則

- `ground` 格可以部署地面幹員。
- `highground` 格可以部署高台幹員。
- `road`、`spawn`、`goal`、`empty` 格不能部署幹員。
- 幹員部署後會顯示方向選擇，方向會影響攻擊範圍與動畫朝向。
- 幹員撤退或死亡後會進入再部署冷卻。

## 9. 常見問題

- 找不到 `ArknightPreload.exe`：先執行 `.\build_win.bat`。
- 找不到 FFmpeg：確認 `ffmpeg.exe` 與 `ffprobe.exe` 在 PATH，或放在 `C:\Program Files\ffmpeg\bin`。
- 找不到 Visual Studio：重新安裝 Visual Studio 2022 Build Tools，並勾選 `Desktop development with C++`。
- CMake/MSVC 因路徑失敗：使用 `.\build_win.bat`，它會同步到 `C:\ArkBuild` 建置。
