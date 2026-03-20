# Arknight (明日方舟核心玩法復刻專題)

此專案為復刻《明日方舟》核心玩法的遊戲專題。

## 遊戲簡介

本專題聚焦於以下核心特色：
- 網格地圖與關卡編排
- 高地位／地面位部署限制
- 敵方路徑、速度、停頓與方向控制
- 可擴充的模組化架構（以方便後續串接 UI、戰鬥與資料檔等系統）

## 開發工具

此專案內部開發了一套命令列（CLI）關卡設計測試工具：**ArknightBuilder**，它基於外部的 PTSD 框架建置，可用於快速驗證地圖、配置地形與測試敵人移動設計。

詳細的建置方式與工具使用說明，請參考：[ArknightBuilder 說明文件](tools/ark_builder/README_zh-tw.md)。

## 開發成員

- 113820028 張浤奕

> 專案開發階段與完整計畫可參考 [Proposal.md](Proposal.md) 了解時程分配細節。

## 如何執行 Demo

1. 下載專案（含 submodule）
```bash
git clone --recurse-submodules <your-repo-url>
cd Arknight
```
如果你已經 clone 過，請補抓 submodule：
```bash
git submodule update --init --recursive
```

2. 準備環境
- CMake 3.16+
- 可用 C++17 編譯器（MSVC / Clang / GCC）

3. 建置 Demo
```bash
cmake -S PTSD -B PTSD/build -DCMAKE_BUILD_TYPE=Debug
cmake --build PTSD/build --target Example
```

4. 執行 Demo
- Linux / macOS:
```bash
./PTSD/build/Example
```
- Windows:
```powershell
.\PTSD\build\Example.exe
```

## Demo 操作
- `滑鼠左鍵`：部署幹員
- `1`：選 Vanguard（只能放置於地面）
- `2`：選 Sniper（只能放置於高台）
- `SPACE`：開始關卡
- `R`：重開 Demo
- `ESC`：離開

## 關卡來源
Demo 會優先載入下列關卡：
1. `tools/ark_builder/levels/test.json`
2. `tools/ark_builder/levels/tutorial_1.json`

可用 `ArknightBuilder` 編輯與驗證關卡：
```bash
cmake --build PTSD/build --target ArknightBuilder
./PTSD/build/ArknightBuilder validate tools/ark_builder/levels/tutorial_1.json
```
